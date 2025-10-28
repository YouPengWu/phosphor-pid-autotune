# phosphor-pid-autotune

An OpenBMC-side service that **runs fan experiments**, identifies a **FOPDT** process
model from step data, and computes **PID coefficients** via **IMC** tuning.
It prefers configuration from **EntityManager** and falls back to a local **JSON**
file when EM data is not present.

- DBus service (gate): `xyz.openbmc_project.PIDAutotune`.
- DBus object: `/xyz/openbmc_project/PIDAutotune`.
- DBus interface: `xyz.openbmc_project.Object.Enable`.
- Property to trigger a run: **`Enabled`** (`false` → idle, `true` → run autotune once).

---

A compact, GitHub-friendly derivation of **FOPDT**

$$
G(s)=\frac{K e^{-L s}}{T s + 1}
$$

and **IMC PID** rules is in **[`docs/fopdt.md`](docs/fopdt.md)**.  
The steady-state detector math is in **[`docs/steady_state.md`](docs/steady_state.md)**.

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
│   ├── dbus_io.cpp                  # DBus I/O: read temperatures/fans and write PWM targets
│   ├── dbus_io.hpp                  # DBus paths, interfaces, and typed read/write helpers
│   ├── sensorinfo.cpp               # Load/cache sensor info DB; lookup by sensorType
│   ├── sensorinfo.hpp               # TempSensorInfo struct and lookup APIs
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
├── main.cpp                         # Main orchestrator: DBus Enable handler, autotune pipeline, log writer
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

### Preferred: EntityManager (Exposes).

Provide objects of these types (keys mirror `autotune.json`):

- `xyz.openbmc_project.Configuration.PIDAutotuneBasic`.
- `xyz.openbmc_project.Configuration.PIDAutotuneSensor`.
- `xyz.openbmc_project.Configuration.PIDAutotuneExperiment`.
- `xyz.openbmc_project.Configuration.PIDAutotuneProcessModel`.
- `xyz.openbmc_project.Configuration.PIDAutotuneTuningMethod`.

See **`examples/entitymanager.example.json`**.

### Fallback: Local JSON.

If no Exposes are found, the service loads **`configs/autotune.json`**.  
See **`examples/autotune.example.json`** for a complete template.

**Key fields (subset):**

- `basic settings`:
  - `pollInterval` *(s)*, `truncatedecimals`, `maxiterations`.
  - `steadyslope`, `steadyrmse`, `steadywindow`, `steadysetpointband` (optional).
  - `sensorinfopath` (optional override for `configs/sensorinfo.json`).
- **New-style sensors**:
  - `tempsensors`: one object with `Name`, `input`, `setpoint`, `type:"temp"`, `sensortype` (e.g., `"tmp75"`). Optional overrides: `qstepc`, `accuracyc`.
  - `fansensors`: N objects with `Name`, `input`, `minduty`, `maxduty`.
- `experiment`:
  - `baseduty`: `stepoutsidetol`, `stepinsidetol`, `basedutylog`, `enable`, `priority` *(note: `tol` removed; error band comes from sensor accuracy/quantization)*.
  - `steptrigger`: `stepduty`, `stepdutylog`, `enable`, `priority`.
- `process models`:
  - `fopdt`: `fopdtlog`, `lambdafactor` (array, e.g. `[0.5,1.0,1.5,2.0]`), `priority`.
- `PID tuning methods`:
  - `imc`: `imcpidlog`, `enable`.

### DBus I/O bindings

- **Temperature read**: `/xyz/openbmc_project/sensors/temperature/<input>`,  
  interface `xyz.openbmc_project.Sensor.Value`, property `Value` (double, °C).
- **Fan speed read (optional)**: `/xyz/openbmc_project/sensors/fan_tach/<input>`,  
  interface `xyz.openbmc_project.Sensor.Value`, property `Value` (double, RPM).
- **Fan PWM write**: `/xyz/openbmc_project/control/fanpwm/<input>`,  
  interface `xyz.openbmc_project.Control.FanPwm`, property `Target` (uint64, 0–255).  
  *(When reading PWM as percentage: `/xyz/openbmc_project/sensors/fan_pwm/<input>`, `Value` in 0–100.)*

---

## Runtime flow

1. **Load configuration.**  
   - Try **EntityManager**; if nothing found → use **`configs/autotune.json`**.
2. **Wait on DBus “Enable” gate.**  
   - Service: `xyz.openbmc_project.PIDAutotune`.  
   - Object: `/xyz/openbmc_project/PIDAutotune`.  
   - Interface: `xyz.openbmc_project.Object.Enable`.  
   - Property: `Enabled` (`false` by default).
3. **When `Enabled=true`.**  
   - `systemctl stop phosphor-pid-control`.  
   - Run **experiments** (enabled, **highest priority first**):
     - **`base_duty`**: search base PWM (0–255).  
       Requirements: **steady** *and* **near setpoint** (band from sensor accuracy/quantization).  
       If no steady within `maxiterations`, logs and uses **best-so-far** (closest to setpoint).
     - **`step_trigger`**:  
       **Pre-step:** steady + near setpoint → apply step (±`stepduty`).  
       **Post-step:** steady-only (temperature expected to differ from setpoint).  
       Records triplets `(tick, temp, pwm)`.
   - Process model: **FOPDT** identification from the step trace.  
   - Tuning: **IMC** for all `lambdafactor` values.  
   - Persist logs/results → set `Enabled=false` → `systemctl start phosphor-pid-control`.

---

## Steady-state rules (exact behavior)

We use a **linear regression** model on the last $N$ samples (window), with **slope** and **RMSE** thresholds,
automatically floored by sensor quantization and accuracy (see `docs/steady_state.md`).

Let samples be $(t_i, y_i)$ and the fit $y_i \approx a + b t_i$. Residuals $\varepsilon_i = y_i - (a + b t_i)$.

- **Slope gate:** $\lvert b \rvert \le b_{\mathrm{eff}}$, where $b_{\mathrm{eff}} = \max\{b_{\text{user}},\, q/(\sqrt{12}\,\Delta t)\}$.  
- **RMSE gate:** $\mathrm{RMSE} \le e_{\mathrm{eff}}$, where $e_{\mathrm{eff}} = \max\{e_{\text{user}},\, q/\sqrt{12}\}$.  
- **Setpoint band (when required):** $\lvert \bar{y} - y_{\mathrm{sp}} \rvert \le \mathrm{errBand}$ with $\mathrm{errBand} = \max\{A_c,\, q/\sqrt{12}\}$.

Application:

- **BaseDuty:** must satisfy **(slope gate) ∧ (RMSE gate) ∧ (setpoint band)**.  
- **StepTrigger:** **pre-step** uses all three; **post-step** uses only **(slope gate) ∧ (RMSE gate)**.

All thresholds can be tuned via `basic settings`:
`steadyslope`, `steadyrmse`, `steadywindow`, and optionally `steadysetpointband`.  
Sensor floors come from `sensortype` via `sensorinfo.json` (or per-sensor overrides `qstepc`, `accuracyc`).

---

## How to trigger a run (DBus)

**Enable autotune.**
```bash
busctl set-property xyz.openbmc_project.PIDAutotune \
  /xyz/openbmc_project/PIDAutotune \
  xyz.openbmc_project.Object.Enable Enabled b true
```

**Check state.**
```bash
busctl get-property xyz.openbmc_project.PIDAutotune \
  /xyz/openbmc_project/PIDAutotune \
  xyz.openbmc_project.Object.Enable Enabled
```

**Logs (typical).**
- `basedutylog`: `/var/lib/autotune/log/base_duty_log.txt`.  
- `stepdutylog`: `/var/lib/autotune/log/step_trigger_log.txt`.  
- `fopdtlog`: `/var/lib/autotune/log/fopdt_log.txt`.  
- `imcpidlog`: `/var/lib/autotune/log/imc_pid_log.txt`.

> Paths are configurable and will be created if missing.

---

## Notes

- Uses **DBus I/O** for all sensor reads and PWM writes; no hwmon file paths in the runtime path.  
- Falls back gracefully to a local JSON when EntityManager is not configured.  
- The step experiment normalizes output to duty **percent** (0–100%) inside process modeling.
