def calculate_pid(k, tau, theta, ratio, method="standard"):
    """
    Calculates PID coefficients based on FOPDT parameters using IMC tuning.

    Args:
        k (float): Process gain.
        tau (float): Time constant.
        theta (float): Dead time.
        ratio (float): Epsilon/Theta ratio.
        method (str): "standard" (PID) or "pi" (Improved PI).

    Returns:
        dict: {'kp': float, 'ki': float, 'kd': float} (kd is 0 for PI)
    """

    # Stability safeguard
    effective_theta = theta
    if effective_theta < 0.1:
        effective_theta = 0.1

    epsilon = effective_theta * ratio

    kp = 0.0
    ki = 0.0
    kd = 0.0

    if method == "standard":
        # IMC PID
        numerator = 2.0 * tau + effective_theta
        denominator = k * (2.0 * epsilon + effective_theta)

        if abs(denominator) > 1e-9:
            kc = numerator / denominator
            tau_i = tau + effective_theta / 2.0
            tau_d = (tau * effective_theta) / (2.0 * tau + effective_theta)

            kp = kc
            ki = kc / tau_i if abs(tau_i) > 1e-9 else 0.0
            kd = kc * tau_d

    elif method == "pi":
        # IMC Improved PI
        denom_pi = 2.0 * k * epsilon
        if abs(denom_pi) > 1e-9:
            kc_pi = (2.0 * tau + effective_theta) / denom_pi
            tau_i_pi = tau + effective_theta / 2.0

            kp = kc_pi
            ki = kc_pi / tau_i_pi if abs(tau_i_pi) > 1e-9 else 0.0
            kd = 0.0

    return {"kp": kp, "ki": ki, "kd": kd}
