# phosphor-pid-autotune

An OpenBMC-side service that **runs fan experiments**, identifies a **FOPDT** process
model from step data, and computes **PID coefficients** via **IMC** tuning.
It prefers configuration from **EntityManager** and falls back to a local **JSON**
file when EM data is not present.

- DBus service (gate): `xyz.openbmc_project.PIDAutotune`
- DBus object: `/xyz/openbmc_project/PIDAutotune`
- DBus interface: `xyz.openbmc_project.Object.Enable`
- Property to trigger a run: **`Enabled`** (`false` → idle, `true` → run autotune once)

---

A compact, GitHub-friendly derivation of **FOPDT**  

$$
G(s)=\frac{K e^{-L s}}{T s + 1}
$$

and **IMC PID** rules is in **[`docs/fopdt.md`](docs/fopdt.md)**.

## Repository layout

```
phosphor-pid-autotune
├── buildjson
│   ├── buildjson.cpp                # Parse JSON config → in-memory Config with defaults and validation
│   ├── buildjson.hpp                # Config structs and JSON loader interfaces
│   └── json.hpp                     # Local include wrapper for nlohmann::json
│
├── configs
│   ├── autotune.json                # Default runtime config (uses accuracyC/qStepC from sensorinfo)
│   └── sensorinfo.json              # Central table: maps sensor type → q/accuracy/bits/tconv
│
├── core
│   ├── logging.cpp                  # Logging helpers implementation (stderr / phosphor-logging bridge)
│   ├── logging.hpp                  # Minimal logging macros and APIs
│   ├── numeric.cpp                  # Numeric utilities implementation
│   ├── numeric.hpp                  # Decimal truncation, clamping, duty↔percent conversions
│   ├── steady_state.cpp             # Steady-state detection using slope and RMSE thresholds
│   ├── steady_state.hpp             # Definitions and regression logic
│   ├── sysfs_io.cpp                 # Sysfs I/O for temperature/tachometer read and PWM (0–255) write
│   ├── sysfs_io.hpp                 # File operations and fan batch helpers
│   ├── time_utils.cpp               # Monotonic clock and sleep helpers
│   ├── time_utils.hpp               # nowMono(), sleepForSec(), and basic timers
│   ├── units.cpp                    # Unit conversion and scaling helpers implementation
│   └── units.hpp                    # Percent/duty scaling and tolerance utilities
│
├── dbus
│   ├── constants.hpp                # Well-known D-Bus names and paths (service, interface, mapper)
│   ├── dbusconfiguration.cpp        # Reads EntityManager Exposes → builds Config (supports q/accuracy)
│   └── dbusconfiguration.hpp        # Declaration of loadConfigFromEntityManager()
│
├── docs
│   ├── fopdt.md                     # Mathematical derivation of the FOPDT model
│   └── steady_state.md              # Steady-state detection derivation and implementation mapping
│
├── examples
│   ├── autotune.example.json        # Example for configs/autotune.json (with inline comments)
│   ├── entitymanager.example.json   # Example EntityManager Exposes (with inline comments)
│   └── sensorinfo.example.json      # Example sensor info table (with inline comments)
│
├── experiment
│   ├── base_duty.cpp                # Base duty search (requires steady + near setpoint within errBand)
│   ├── base_duty.hpp                # Config structures, results, and API
│   ├── step_trigger.cpp             # Step test (pre: steady+near SP, post: steady only)
│   └── step_trigger.hpp             # Step experiment data types and API
│
├── PID_tuning_methods
│   ├── imc.cpp                      # IMC tuning: λ-list → PID gain map; writes output log
│   └── imc.hpp                      # IMC API definitions and PID gain structures
│
├── process_models
│   ├── fopdt.cpp                    # Identify K/T/L from step response; normalize duty (0–100%)
│   └── fopdt.hpp                    # FOPDT parameter structure and identification API
│
├── .clang-format                    # Clang-format style configuration
├── main.cpp                         # Main orchestrator: D-Bus Enable handler, autotune pipeline, log writer
├── meson.build                      # Meson build script (dependencies, sources, install paths)
├── phosphor-pid-autotune.bb         # Yocto recipe: build/install binary, unit, fallback JSON
├── phosphor-pid-autotune.service    # systemd unit; After=EntityManager; runs autotune daemon
└── README.md                        # Quickstart guide, config schema, logs/paths, and troubleshooting

```

---

## Build (Meson)

> OpenBMC SDKs ship required deps: `sdbusplus`, `systemd` (libsystemd), `boost` (Asio), `nlohmann-json`.

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

### Yocto (OpenBMC image)
Place `phosphor-pid-autotune.bb` under your layer, e.g.  
`meta-yourlayer/recipes-phosphor/fans/phosphor-pid-autotune.bb`, then:

```bash
bitbake obmc-phosphor-image
```

---

## Configure

### Preferred: EntityManager (Exposes)
Provide objects of these types (keys mirror `autotune.json`):

- `xyz.openbmc_project.Configuration.PIDAutotuneBasic`
- `xyz.openbmc_project.Configuration.PIDAutotuneSensor`
- `xyz.openbmc_project.Configuration.PIDAutotuneExperiment`
- `xyz.openbmc_project.Configuration.PIDAutotuneProcessModel`
- `xyz.openbmc_project.Configuration.PIDAutotuneTuningMethod`

See **`examples/entitymanager.example.json`**.

### Fallback: Local JSON
If no Exposes are found, the service loads **`configs/autotune.json`**.  
See **`examples/autotune.example.json`** for a complete template.

**Key fields (subset):**
- `basic settings.pollInterval` *(sec)*, `stablecount`, `truncatedecimals`, `maxiterations`
- `sensors`: one **temp** (setpoint + input path), N **fan** (hwmon pwm/tach + min/max duty)
- `experiment`:
  - `baseduty`: `tol`, `stepoutsidetol`, `stepinsidetol`, `basedutylog`, `enable`, `priority`
  - `steptrigger`: `stepduty`, `stepdutylog`, `enable`, `priority`
- `process models`:
  - `fopdt`: `fopdtlog`, `lambdafactor` (array, e.g. `[0.5,1.0,1.5,2.0]`), `priority`
- `PID tuning methods`:
  - `imc`: `imcpidlog`, `enable`

---

## Runtime flow

1. **Load configuration**  
   - Try **EntityManager**; if nothing found → use **`configs/autotune.json`**.
2. **Wait on DBus “Enable” gate**  
   - Service: `xyz.openbmc_project.PIDAutotune`  
   - Object: `/xyz/openbmc_project/PIDAutotune`  
   - Interface: `xyz.openbmc_project.Object.Enable`  
   - Property: `Enabled` (`false` by default)
3. **When `Enabled=true`**  
   - `systemctl stop phosphor-pid-control`
   - Run **experiments** (enabled, **highest priority first**):
     - **`base_duty`**: search base PWM (0–255).  
       *Steady-state rule:* **equal-to-setpoint** for ≥ `stablecount` samples.  
       Inside/Outside tolerance logic uses both **increment and decrement**; always clamps 0–255.  
       If no steady within `maxiterations`, it logs and uses **best-so-far** (closest to setpoint).
     - **`step_trigger`**:  
       **Pre-step:** wait steady at setpoint (Rule 1) → **apply step** (±`stepduty`) →  
       **Post-step:** wait steady by **equal-to-previous** (Rule 2); record `(time,temp,pwm)`.
   - Run **process model** (highest priority only):  
     - **`fopdt`**: identify **K, T, L** on duty **percent (0–100)**, converted from 0–255.
   - Run **PID tuning methods** (enabled):  
     - **`imc`**: compute PID for **each** `lambdafactor` and print a **λ → (Kp,Ki,Kd)** table; save to `imcpidlog`.
   - Persist logs/results → set `Enabled=false` → `systemctl start phosphor-pid-control`.

---

## How to trigger a run (DBus)

**Enable autotune**
```bash
busctl set-property xyz.openbmc_project.PIDAutotune \
  /xyz/openbmc_project/PIDAutotune \
  xyz.openbmc_project.Object.Enable Enabled b true
```

**Check state**
```bash
busctl get-property xyz.openbmc_project.PIDAutotune \
  /xyz/openbmc_project/PIDAutotune \
  xyz.openbmc_project.Object.Enable Enabled
```

**Logs (typical)**
- `basedutylog`: `/var/lib/autotune/log/base_duty_log.txt`
- `stepdutylog`: `/var/lib/autotune/log/step_trigger_log.txt`
- `fopdtlog`: `/var/lib/autotune/log/fopdt_log.txt`
- `imcpidlog`: `/var/lib/autotune/log/imc_pid_log.txt`

> Paths are configurable and will be created if missing.

---

## Steady-state rules (exact behavior)

- **Rule (1): equal-to-setpoint**  
  Compare the **truncated** temperature to **truncated setpoint**.  
  If equal for **≥ `stablecount`** consecutive samples → steady.
- **Rule (2): equal-to-previous**  
  Compare the **truncated** temperature to the **previous truncated** reading.  
  If equal for **≥ `stablecount`** consecutive samples → steady.

> `base_duty` uses **Rule (1)** only.  
> `step_trigger` uses **Rule (1) before stepping** and **Rule (2) after stepping**.





