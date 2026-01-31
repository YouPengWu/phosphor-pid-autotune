import os
import sys


def parse_data_file(filename, interval, windowsize):
    iterations = []
    pwms = []
    temps = []
    first_col_is_time = False

    try:
        with open(filename, "r") as f:
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
                        if "," in line and len(parts) >= 4:
                            val = float(parts[1])  # time
                            temp = float(parts[2])  # temp
                            pwm = int(float(parts[3]))  # pwm
                            first_col_is_time = True
                        elif len(parts) == 3:
                            val = float(parts[0])
                            pwm = int(float(parts[1]))
                            temp = float(parts[2])
                        else:
                            continue

                        iterations.append(val)
                        pwms.append(pwm)
                        temps.append(temp)
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Error reading file: {e}")
        return None, None, None, None, None, None, None

    if not iterations:
        print("No valid data found.")
        return None, None, None, None, None, None, None

    if first_col_is_time:
        times = iterations
    else:
        times = [n * interval for n in iterations]

    # Detect Step Trigger
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
        if step_idx >= windowsize:
            y0 = sum(temps[step_idx - windowsize : step_idx]) / windowsize
        elif step_idx > 0:
            y0 = sum(temps[:step_idx]) / step_idx
        else:
            y0 = temps[0]

        # Calculate y_final (average of last windowsize points)
        if len(temps) >= windowsize:
            y_final = sum(temps[-windowsize:]) / windowsize
        else:
            y_final = temps[-1]

    return times, temps, pwms, step_time, y0, delta_pwm, start_idx, y_final
