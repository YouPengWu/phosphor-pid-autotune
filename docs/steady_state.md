# Steady-State Detection

This document derives the steady-state detection method used in **phosphor-pid-autotune**, based on a
statistical linear regression model with slope and RMSE thresholds.  
It accounts for sensor quantization, accuracy, and sample timing to provide a robust and automatic
steady-state decision.

---

## Linear Regression Model

Assume a temperature sensor provides samples at fixed intervals \( \Delta t \):

**Equation (1)**

$$
(t_i, y_i), \quad i = 1, 2, \dots, N
$$

We model the temperature trend over the last \( N \) samples with a straight line:

**Equation (2)**

$$
y_i \approx a + b\,t_i
$$

where:

- \(a\): intercept — mean temperature level  
- \(b\): slope — average temperature rate (°C/s)

---

## Least-Squares Derivation

We find \(a, b\) by minimizing the sum of squared residuals:

**Equation (3)**

$$
S(a,b) = \sum_{i=1}^{N} (y_i - a - b\,t_i)^2
$$

Taking partial derivatives:

**Equation (4)**

$$
\frac{\partial S}{\partial a} = -2\sum(y_i - a - b\,t_i) = 0,
\quad
\frac{\partial S}{\partial b} = -2\sum t_i(y_i - a - b\,t_i) = 0
$$

Define sample means:

**Equation (5)**

$$
\bar{t} = \frac{1}{N}\sum t_i, \quad
\bar{y} = \frac{1}{N}\sum y_i
$$

Then the least-squares solution is:

**Equation (6)**

$$
b = \frac{\sum (t_i - \bar{t})(y_i - \bar{y})}
         {\sum (t_i - \bar{t})^2},
\qquad
a = \bar{y} - b\,\bar{t}
$$

---

## Residual and RMSE

The residuals are defined as:

**Equation (7)**

$$
\varepsilon_i = y_i - (a + b\,t_i)
$$

The root-mean-square error (RMSE) is:

**Equation (8)**

$$
\mathrm{RMSE} = \sqrt{\frac{1}{N}\sum_{i=1}^{N} \varepsilon_i^2}
$$

---

## Quantization-Aware Limits

For a digital sensor with quantization step \(q\) (°C/LSB),  
the theoretical RMS quantization error is:

**Equation (9)**

$$
\sigma_q = \frac{q}{\sqrt{12}}
$$

Since samples are spaced by \(\Delta t\), the smallest measurable slope is:

**Equation (10)**

$$
b_{\min} = \frac{\sigma_q}{\Delta t}
$$

These represent the **physical limits** of detectability.

---

## Steady-State Conditions

Let user-defined thresholds be \(b_{\text{user}}\) and \(e_{\text{user}}\) for slope and RMSE respectively.  
The effective thresholds are limited by sensor quantization floors:

**Equation (11)**

$$
b_{\text{eff}} = \max(b_{\text{user}}, b_{\min}),
\qquad
e_{\text{eff}} = \max(e_{\text{user}}, \sigma_q)
$$

A window is declared **steady** when both conditions hold:

**Equation (12)**

$$
\boxed{
|b| \le b_{\text{eff}}
\quad \text{and} \quad
\mathrm{RMSE} \le e_{\text{eff}}
}
$$

---

## Setpoint Proximity Band

In some phases (e.g. base duty search), the mean temperature must also
be close to the target setpoint \(y_{\mathrm{sp}}\):

**Equation (13)**

$$
|\bar{y} - y_{\mathrm{sp}}| \le \mathrm{errBand}
$$

where the band is derived from sensor accuracy and quantization:

**Equation (14)**

$$
\mathrm{errBand} = \max(A_c, \frac{q}{\sqrt{12}})
$$

with:

- \(A_c\): sensor accuracy (°C)  
- \(q\): quantization step (°C/LSB)

---

## Application Rules

### 1. BaseDuty Phase

The system is considered converged if:

1. Steady-state holds (Equation 12), **and**  
2. Mean temperature is within the setpoint band (Equation 13).

When iterating duty values:

- If deviation \(> \mathrm{errBand}\): take a larger step.  
- If deviation \(\le \mathrm{errBand}\): take a smaller step.

---

### 2. StepTrigger Phase

- **Pre-step condition:** must be both steady and near setpoint.  
- **Post-step condition:** must be steady only (temperature expected to differ from setpoint).

---

## Parameter Summary

| Parameter | Meaning | Unit / Typical |
|------------|----------|----------------|
| \(N\) | regression window size | 20–30 samples |
| \(\Delta t\) | polling interval | 1 s |
| \(q\) | quantization step | 0.0625 °C (TMP75) |
| \(A_c\) | sensor accuracy | 0.5 °C |
| \(b_{\text{user}}\) | user slope threshold | 0.02–0.05 °C/s |
| \(e_{\text{user}}\) | user RMSE threshold | 0.05–0.1 °C |
| \(b_{\min}\) | quantization slope floor | \(q / (\sqrt{12}\,\Delta t)\) |
| \(\sigma_q\) | quantization RMSE floor | \(q / \sqrt{12}\) |
| \(\mathrm{errBand}\) | tolerance band | max(\(A_c, \sigma_q\)) |

---

## Implementation Mapping

| Code Field | Description | Equation |
|-------------|--------------|-----------|
| `basic.pollIntervalSec` | sampling interval \(\Delta t\) | Eq. (1) |
| `basic.steadyWindow` | window size \(N\) | Eq. (2) |
| `basic.steadyslope` | user slope threshold | Eq. (11) |
| `basic.steadyrmse` | user RMSE threshold | Eq. (11) |
| `temp.qStepC` | quantization step \(q\) | Eq. (9) |
| `temp.accuracyC` | sensor accuracy \(A_c\) | Eq. (14) |

---

## Intuitive Summary

- **Slope criterion** detects whether temperature drift has ceased.  
- **RMSE criterion** ensures fluctuations are within quantization and sensor limits.  
- **Quantization floors** prevent false instability when signal noise is below measurable levels.  
- **Setpoint proximity** adds an extra logical gate for phases requiring equilibrium at target.

---

## References

- Linear least-squares regression (standard form).  
- Quantization noise model: uniform error variance \(q^2/12\).  
- Temperature sensor datasheets (e.g., TMP75, LM75) for \(q\) and \(A_c\).  
