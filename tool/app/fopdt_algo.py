import math


def calculate_model_curve(
    k, tau, theta, t_start, t_end, step_time, y0, delta_pwm, dt=0.1
):
    """
    Generates time and temperature arrays for the FOPDT model curve.
    """
    times = []
    temps = []

    t = t_start
    while t <= t_end:
        times.append(t)
        if t < step_time + theta:
            temps.append(y0)
        else:
            # y(t) = y0 + K * delta_u * (1 - e^(-(t - step_time - theta) / tau))
            val = y0 + k * delta_pwm * (
                1.0 - math.exp(-(t - step_time - theta) / tau)
            )
            temps.append(val)

        t += dt

    return times, temps


def cost_function(params, times, temps, step_time, y0, delta_pwm):
    """
    Cost function for optimization: Sum of Squared Errors (SSE).
    params: [k, tau, theta]
    """
    k, tau, theta = params

    # Constraints
    if tau <= 1e-6 or theta < 0:
        return 1e9  # High penalty

    total_error = 0.0

    for i in range(len(times)):
        t = times[i]
        actual_temp = temps[i]

        if t < step_time + theta:
            model_temp = y0
        else:
            model_temp = y0 + k * delta_pwm * (
                1.0 - math.exp(-(t - step_time - theta) / tau)
            )

        err = actual_temp - model_temp
        total_error += err * err

    return total_error


def nelder_mead_solver(func, x0, args=(), max_iter=200):
    """
    Simple implementation of Nelder-Mead optimization to avoid scipy dependency.
    """
    alpha = 1.0  # Reflection
    gamma = 2.0  # Expansion
    rho = 0.5  # Contraction
    sigma = 0.5  # Shrink

    n = len(x0)
    simplex = []

    # 1. Initialize Simplex
    # P0
    p0 = list(x0)
    simplex.append((p0, func(p0, *args)))

    # P1..Pn
    for i in range(n):
        p = list(x0)
        if abs(p[i]) > 1e-9:
            p[i] *= 1.05
        else:
            p[i] = 0.00025
        simplex.append((p, func(p, *args)))

    for _ in range(max_iter):
        # 2. Sort
        simplex.sort(key=lambda x: x[1])
        best = simplex[0]
        worst = simplex[-1]
        second_worst = simplex[-2]

        # Check convergence
        if abs(worst[1] - best[1]) < 1e-6:
            break

        # 3. Centroid
        centroid = [0.0] * n
        for i in range(n):  # All except worst
            for j in range(n):
                centroid[j] += simplex[i][0][j]
        for j in range(n):
            centroid[j] /= n

        # 4. Reflection
        reflected = [0.0] * n
        for j in range(n):
            reflected[j] = centroid[j] + alpha * (centroid[j] - worst[0][j])
        reflected_cost = func(reflected, *args)

        if best[1] <= reflected_cost < second_worst[1]:
            simplex[-1] = (reflected, reflected_cost)
            continue

        # 5. Expansion
        if reflected_cost < best[1]:
            expanded = [0.0] * n
            for j in range(n):
                expanded[j] = centroid[j] + gamma * (
                    reflected[j] - centroid[j]
                )
            expanded_cost = func(expanded, *args)

            if expanded_cost < reflected_cost:
                simplex[-1] = (expanded, expanded_cost)
            else:
                simplex[-1] = (reflected, reflected_cost)
            continue

        # 6. Contraction
        is_outside = reflected_cost < worst[1]
        contracted = [0.0] * n

        if is_outside:
            for j in range(n):
                contracted[j] = centroid[j] + rho * (
                    reflected[j] - centroid[j]
                )
            contracted_cost = func(contracted, *args)
            if contracted_cost <= reflected_cost:
                simplex[-1] = (contracted, contracted_cost)
                continue
        else:
            for j in range(n):
                contracted[j] = centroid[j] + rho * (worst[0][j] - centroid[j])
            contracted_cost = func(contracted, *args)
            if contracted_cost < worst[1]:
                simplex[-1] = (contracted, contracted_cost)
                continue

        # 7. Shrink
        new_simplex = [best]
        for i in range(1, len(simplex)):
            p = simplex[i][0]
            new_p = [0.0] * n
            for j in range(n):
                new_p[j] = best[0][j] + sigma * (p[j] - best[0][j])
            new_simplex.append((new_p, func(new_p, *args)))
        simplex = new_simplex

    return simplex[0][0]  # Return best params


def linear_interpolation(x1, y1, x2, y2, y_target):
    if abs(y2 - y1) < 1e-9:
        return x1
    return x1 + (y_target - y1) * (x2 - x1) / (y2 - y1)


def identify_two_point(times, temps, step_time, y0, final_temp, delta_pwm):
    """
    Identifies FOPDT parameters using the Two-Point method (63.2% and 28.3%).
    """
    temp_change = final_temp - y0

    if abs(delta_pwm) < 1e-6 or abs(temp_change) < 1e-6:
        return {"k": 0.0, "tau": 0.0, "theta": 0.0}

    k = temp_change / delta_pwm

    temp_28 = y0 + 0.283 * temp_change
    temp_63 = y0 + 0.632 * temp_change

    time_28 = -1.0
    time_63 = -1.0

    for i in range(1, len(times)):
        if times[i] < step_time:
            continue

        t1, y1 = times[i - 1], temps[i - 1]
        t2, y2 = times[i], temps[i]

        # Find 28.3% time
        if time_28 < 0:
            if (temp_change > 0 and y1 < temp_28 and y2 >= temp_28) or (
                temp_change < 0 and y1 > temp_28 and y2 <= temp_28
            ):
                time_28 = linear_interpolation(t1, y1, t2, y2, temp_28)

        # Find 63.2% time
        if time_63 < 0:
            if (temp_change > 0 and y1 < temp_63 and y2 >= temp_63) or (
                temp_change < 0 and y1 > temp_63 and y2 <= temp_63
            ):
                time_63 = linear_interpolation(t1, y1, t2, y2, temp_63)
                break  # Found both

    if time_28 < 0 or time_63 < 0:
        return {"k": k, "tau": 0.0, "theta": 0.0}

    tau = 1.5 * (time_63 - time_28)
    theta = time_63 - step_time - tau

    if theta < 0:
        theta = 0

    return {"k": k, "tau": tau, "theta": theta}


def identify_nelder_mead(
    times, temps, step_time, y0, delta_pwm, initial_guess
):
    """
    Identifies FOPDT parameters using Nelder-Mead optimization.
    """
    # Initial guess: [k, tau, theta]
    x0 = [initial_guess["k"], initial_guess["tau"], initial_guess["theta"]]

    best_params = nelder_mead_solver(
        cost_function, x0, args=(times, temps, step_time, y0, delta_pwm)
    )

    return {
        "k": best_params[0],
        "tau": best_params[1],
        "theta": best_params[2],
    }


def identify_lsm(times, temps, step_time, y0, final_temp, delta_pwm):
    """
    Identifies FOPDT parameters using Least Squares Method linearization.
    Equation: ln(1 - (y(t)-y0)/(K*du)) = -(t - step_time - theta)/tau
    """
    # 1. Determine K first
    temp_change = final_temp - y0
    if abs(delta_pwm) < 1e-9:
        return {"k": 0, "tau": 0, "theta": 0}
    k = temp_change / delta_pwm

    # 2. Prepare data for Linear Regression
    # Y' = ln(1 - y_norm)
    # X' = t
    # Slope = -1/tau
    # Intercept = (step_time + theta)/tau

    x_data = []
    y_data_lin = []

    start_idx = 0
    for i, t in enumerate(times):
        if t > step_time:
            start_idx = i
            break

    for i in range(start_idx, len(times)):
        t = times[i]
        y = temps[i]

        # Normalized response: (y - y0) / (K * du)
        # Should range from 0 to 1
        val = (y - y0) / temp_change

        # Threshold 10% to 90% (Match C++)
        if val <= 0.1:
            continue
        if val >= 0.9:
            continue

        try:
            # Match C++ Logic:
            # Linearization: ln(1 - yNorm) = - (t - theta) / tau
            # Rewrite as: t - theta = tau * (-ln(1 - yNorm))
            # t = tau * (-ln(1 - yNorm)) + theta + step_time (if t is absolute)
            # Actually C++ uses relative time:
            # y_reg = t - step_time
            # x_reg = -ln(1 - yNorm)
            # y_reg = slope * x_reg + intercept
            # slope = tau
            # intercept = theta

            y_norm_inv = 1.0 - val
            if y_norm_inv < 1e-9:
                continue

            lx = -math.log(y_norm_inv)
            ly = t - step_time

            x_data.append(lx)
            y_data_lin.append(ly)
        except:
            continue

    if len(x_data) < 2:
        return {"k": k, "tau": 0, "theta": 0}

    # 3. Solve Linear Regression: y = mx + c
    n = len(x_data)
    sum_x = sum(x_data)
    sum_y = sum(y_data_lin)
    sum_xy = sum(x * y for x, y in zip(x_data, y_data_lin))
    sum_x2 = sum(x * x for x in x_data)

    denom = n * sum_x2 - sum_x * sum_x
    if abs(denom) < 1e-9:
        return {"k": k, "tau": 0, "theta": 0}

    slope = (n * sum_xy - sum_x * sum_y) / denom
    intercept = (sum_y - slope * sum_x) / n

    # 4. Extract Parameters (Match C++)
    # Slope = Tau
    # Intercept = Theta

    tau = slope
    theta = intercept

    if theta < 0:
        theta = 0
    if tau < 0:
        tau = 0

    return {"k": k, "tau": tau, "theta": theta}
