import math


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
