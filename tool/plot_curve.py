import argparse
import matplotlib.pyplot as plt
import sys
import os
import math


def parse_fopdt_file(fopdt_path):
    params_632 = None
    params_lsm = None
    current_section = None

    try:
        with open(fopdt_path, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                if "632 Method" in line:
                    current_section = "632"
                    params_632 = {}
                elif "LSM Method" in line:
                    current_section = "lsm"
                    params_lsm = {}
                elif "Optimization Method" in line:
                    current_section = "opt"
                    params_opt = {}

                if "=" in line:
                    key, val = line.split("=")
                    key = key.strip()
                    try:
                        f_val = float(val.strip())
                        if current_section == "632" and params_632 is not None:
                            params_632[key] = f_val
                        elif (
                            current_section == "lsm" and params_lsm is not None
                        ):
                            params_lsm[key] = f_val
                        elif (
                            current_section == "opt" and params_opt is not None
                        ):
                            params_opt[key] = f_val
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Error parsing FOPDT file: {e}")
        return None, None, None

    return params_632, params_lsm, params_opt


def get_model_curve(p, t_start, t_end, step_time, y0, delta_pwm, dt=0.1):
    if not p:
        return [], []
    k = p["k"]
    tau = p["tau"]
    theta = p["theta"]

    m_times = []
    m_temps = []
    t_curr = t_start

    # Calculate delta_duty once
    delta_duty = delta_pwm * 100.0 / 255.0

    while t_curr <= t_end:
        m_times.append(t_curr)
        eff_t = t_curr - (step_time + theta)

        if eff_t < 0:
            val = y0
        else:
            val = y0 + k * delta_duty * (1 - math.exp(-eff_t / tau))

        m_temps.append(val)
        t_curr += dt

    return m_times, m_temps


def main():
    parser = argparse.ArgumentParser(
        description="Plot temperature curve from autotune data."
    )
    parser.add_argument(
        "filename",
        help="Path to the plot data file (e.g., plot_CPU0_TEMP.txt)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.5,
        help="Polling interval in seconds",
    )
    parser.add_argument(
        "--windowsize",
        type=int,
        default=30,
        help="Window size for baseline calculation (default: 30)",
    )
    parser.add_argument("--output", help="Output image filename")

    args = parser.parse_args()

    if not os.path.exists(args.filename):
        print(f"Error: File {args.filename} not found.")
        sys.exit(1)

    # 1. Read Data
    iterations = []
    pwms = []
    temps = []
    first_col_is_time = False

    try:
        with open(args.filename, "r") as f:
            header = f.readline().strip()
            if header.lower().startswith("time"):
                first_col_is_time = True

            for line in f:
                line = line.strip()
                if not line:
                    continue

                # Detect delimiter
                if "," in line:
                    parts = line.split(",")
                else:
                    parts = line.split()

                if len(parts) >= 3:
                    try:
                        # Handle case where first column might be index 'n' or time
                        # Based on file check: n,time,temp,pwm...
                        # So time is index 1, pwm is index 3, temp is index 2
                        # But legacy files might be: time pwm temp

                        # Let's try to be smart.
                        # If header existed and said "n,time,temp,pwm", we could map it.
                        # But header parsing is outside this loop.

                        # Heuristic:
                        # If 3 columns: likely Time, PWM, Temp (legacy)
                        # If >3 columns and comma separated: likely n, Time, Temp, PWM, ... (new format)

                        if "," in line and len(parts) >= 4:
                            # New format: n, time, temp, pwm
                            # But wait, looking at file content: n,time,temp,pwm
                            val = float(parts[1])  # time
                            temp = float(parts[2])  # temp
                            pwm = int(
                                float(parts[3])
                            )  # pwm (might be float in string)

                            # If we are parsing this format, the value IS time.
                            first_col_is_time = True

                        elif len(parts) == 3:
                            # Old format: val, pwm, temp
                            val = float(parts[0])
                            pwm = int(float(parts[1]))
                            temp = float(parts[2])
                        else:
                            # Fallback or skipping
                            continue

                        iterations.append(val)
                        pwms.append(pwm)
                        temps.append(temp)
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

    if not iterations:
        print("No valid data found.")
        sys.exit(1)

    if first_col_is_time:
        times = iterations
    else:
        times = [n * args.interval for n in iterations]

    # 2. Detect Step Trigger
    step_idx = -1
    delta_pwm = 0
    if len(pwms) > 1:
        initial_pwm = pwms[0]
        for i in range(1, len(pwms)):
            if pwms[i] != initial_pwm:
                step_idx = i
                delta_pwm = pwms[i] - pwms[i - 1]
                break

    if step_idx == -1:
        print("Warning: No PWM step detected. Plotting all data.")
        step_time = times[0]
        start_idx = 0
        y0 = temps[0]
    else:
        step_time = times[step_idx]
        start_idx = step_idx

        # Calculate y0 (average of windowsize points before step)
        avg_window = args.windowsize
        if step_idx >= avg_window:
            y0 = sum(temps[step_idx - avg_window : step_idx]) / avg_window
        elif step_idx > 0:
            y0 = sum(temps[:step_idx]) / step_idx
        else:
            y0 = temps[0]

    # 3. Read Parameters
    params_632 = None
    params_lsm = None
    params_opt = None

    # Construct FOPDT filename:
    # plot_XYZ.txt -> fopdt_XYZ.txt
    # step_trigger_XYZ.txt -> fopdt_XYZ.txt
    dirname = os.path.dirname(args.filename)
    basename = os.path.basename(args.filename)

    if basename.startswith("plot_"):
        fopdt_basename = basename.replace("plot_", "fopdt_")
    elif basename.startswith("step_trigger_"):
        fopdt_basename = basename.replace("step_trigger_", "fopdt_")
    else:
        fopdt_basename = "fopdt_" + basename

    fopdt_path = os.path.join(dirname, fopdt_basename)

    if os.path.exists(fopdt_path):
        print(f"Reading parameters from {fopdt_path}...")
        params_632, params_lsm, params_opt = parse_fopdt_file(fopdt_path)
    else:
        print(f"Warning: FOPDT file {fopdt_path} not found.")

    # 4. Filter Data for Plotting (Start from Step Trigger)
    plot_times = times[start_idx:]
    plot_temps = temps[start_idx:]

    if not plot_times:
        print("Error: No data to plot after step trigger.")
        sys.exit(1)

    t_start = plot_times[0]
    t_end = plot_times[-1]

    # 5. Plot
    plt.figure(figsize=(10, 6))

    # Real Data (Grey Dots)
    plt.scatter(
        plot_times,
        plot_temps,
        color="grey",
        s=15,
        alpha=0.6,
        label="Actual Data",
        zorder=2,
    )

    # 632 Method (Blue Dashed)
    if params_632:
        mt, mv = get_model_curve(
            params_632, t_start, t_end, step_time, y0, delta_pwm
        )
        label_str = f"TwoPoint (k={params_632['k']:.3f}, tau={params_632['tau']:.1f}, theta={params_632['theta']:.1f})"
        plt.plot(
            mt,
            mv,
            color="blue",
            linestyle="--",
            linewidth=2,
            label=label_str,
            zorder=3,
        )

    # LSM Method (Red Dashed - wait, user previous preference was Red Solid for LSM in batch, but legacy was different. User said "same as run_and_plot_batch".
    # In run_and_plot_batch: TwoPoint=Blue Dashed, LSM=Red Solid, Opt=Green Solid.
    # I will follow run_and_plot_batch style which I established in previous turns and user liked.)
    # Actually wait, let's look at the file content I'm replacing.
    # Original file had LSM as Red Solid. TwoPoint as Blue Dashed.

    # LSM Method (Green Solid)
    if params_lsm:
        mt, mv = get_model_curve(
            params_lsm, t_start, t_end, step_time, y0, delta_pwm
        )
        label_str = f"LSM (k={params_lsm['k']:.3f}, tau={params_lsm['tau']:.1f}, theta={params_lsm['theta']:.1f})"
        plt.plot(
            mt,
            mv,
            color="green",
            linestyle="-",
            linewidth=2,
            label=label_str,
            zorder=4,
        )

    # Optimization Method (Red Solid - Most Accurate)
    if params_opt:
        mt, mv = get_model_curve(
            params_opt, t_start, t_end, step_time, y0, delta_pwm
        )
        label_str = f"Nelder-Mead (k={params_opt['k']:.3f}, tau={params_opt['tau']:.1f}, theta={params_opt['theta']:.1f})"
        plt.plot(
            mt,
            mv,
            color="red",
            linestyle="-",
            linewidth=2,
            label=label_str,
            zorder=5,
        )

    plt.xlabel("Time(seconds)")
    plt.ylabel("Temperature (C)")

    # Title using sensor name
    sensor_name = basename.replace("plot_", "").replace(".txt", "")
    plt.title(f"{sensor_name} Step Response Analysis")

    plt.grid(True, linestyle="--", alpha=0.6)
    plt.legend()

    # Save Output
    if args.output:
        output_file = args.output
    else:
        output_file = os.path.join(
            dirname, os.path.splitext(basename)[0] + ".png"
        )

    try:
        plt.savefig(output_file)
        print(f"Plot saved to {output_file}")
    except Exception as e:
        print(f"Error saving plot: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
