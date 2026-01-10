# Introduction

This document derives the First-Order Plus Dead Time (FOPDT) model from first
principles, explains how to identify system parameters \(k\), \(\tau\), and
\(\theta\) from step-response data, and presents IMC-based formulas for
practical PID controller tuning.

## FOPDT Model Derivation

FOPDT (First-Order Plus Dead Time) is a widely used method for modeling dynamic
systems and serves as a fundamental basis for controller design.

---

## Mathematical Derivation of the FOPDT Model

In the FOPDT model, we assume that the rate of change of the system output is
proportional to the difference between the desired value (setpoint) and the
current output:

**Equation (1)**

$$
\frac{dy(t)}{dt} \propto \bigl(u(t) - y(t)\bigr).
$$

where:

- \( $y(t)$ \): system output (e.g., temperature)
- \( $u(t)$ \): system input (e.g., fan duty or heating power)

To make this relationship explicit, we introduce two parameters:

- \( $k$ \): steady-state gain (input-to-output amplification)
- \( $\tau$ \): time constant (response speed)

Thus, the first-order differential equation becomes:

**Equation (2)**

$$
\frac{dy(t)}{dt} = \frac{1}{\tau}\bigl(k u(t) - y(t)\bigr), \quad \tau > 0, \quad k \in \mathbb{R}.
$$

Multiplying both sides by \(\tau\):

**Equation (3)**

$$
\tau\frac{dy(t)}{dt} = k u(t) - y(t).
$$

---

## Laplace Domain Representation

Taking the Laplace transform of Equation (3), and **assuming zero initial
conditions** $y(0)=0$:

$$
\mathcal{L}\left[\frac{dy(t)}{dt}\right] = sY(s), \qquad
\mathcal{L}\left[y(t)\right] = Y(s), \qquad
\mathcal{L}\left[u(t)\right] = U(s).
$$

Hence,

**Equation (4)**

$$
\tau sY(s) = k U(s) - Y(s).
$$

Rearranging:

**Equation (5)**

$$
(\tau s + 1)Y(s) = k U(s).
$$

The system is **linear and time-invariant (LTI)**.  
Thus, the transfer function is:

**Equation (6)**

$$
G_0(s) = \frac{Y(s)}{U(s)} = \frac{k}{\tau s + 1}.
$$

---

## Including Time Delay

To model a real process with **dead time** \(\theta\),  
the input can be written as:

**Equation (7)**

$$
u_d(t) = u(t - \theta),
$$

where \(\theta\) represents the delay between the input change and the observed
system response.

To ensure the delay has no effect before \(t = \theta\), we use the Heaviside
step function:

**Equation (8)**

$$
u_d(t) = u(t - \theta)H(t - \theta),
$$

where

$$
H(t)=
\begin{cases}
0, & t < 0,\\
1, & t \ge 0.
\end{cases}
$$

Taking the Laplace transform:

**Equation (9)**

$$
U_d(s) = e^{-\theta s}U(s).
$$

Substituting into Equation (3):

**Equation (10)**

$$
(\tau s + 1)Y(s) = k e^{-\theta s} U(s).
$$

Therefore, the **FOPDT transfer function** is:

**Equation (11)**

$$
G(s) = \frac{Y(s)}{U(s)} = \frac{k e^{-\theta s}}{\tau s + 1}.
$$

---

## Normalization of the Step Response

The steady-state change of output is:

**Equation (12)**

$$
\Delta y_\infty = y(\infty) - y(0^+) = k\Delta u,
$$

Assuming deviation form ($y(0^+)=0$):

$$
y(\infty) = k\Delta u.
$$

Define the normalized output as:

**Equation (13)**

$$
f(t) = \frac{y(t) - y(0^+)}{\Delta y_\infty}.
$$

---

## Step Response in the Laplace Domain

For a step input $u(t) = \Delta u\,H(t)$:

**Equation (14)**

$$
U(s) = \frac{\Delta u}{s}.
$$

Substituting into $Y(s) = G(s)U(s)$:

**Equation (15)**

$$
\begin{aligned}
Y(s) &= \frac{k e^{-\theta s}}{\tau s + 1}\cdot\frac{\Delta u}{s} = \frac{k\Delta u e^{-\theta s}}{s(\tau s + 1)}.
\end{aligned}
$$

Dividing both sides by $k\Delta u$ gives:

**Equation (16)**

$$
F(s) = \frac{Y(s)}{k\Delta u} = \frac{e^{-\theta s}}{s(\tau s + 1)}.
$$

Here $F(s) = \mathcal{L}\{f(t)\}$, and the exponential term $e^{-\theta s}$
represents the time delay in the frequency domain.

---

## Removing Delay for Simplification

When \(\theta = 0\), we obtain the base form:

**Equation (17)**

$$
F(s) = \frac{1}{s(\tau s + 1)}.
$$

Applying **partial-fraction decomposition** and dividing by \(\tau\):

**Equation (18)**

$$
F(s) = \frac{1}{s(\tau s + 1)} = \frac{1}{s} - \frac{\tau}{\tau s + 1}.
$$

Taking the inverse Laplace transform:

**Equation (19)**

$$
f(t) = 1 - e^{-t/\tau}, \quad t \ge 0.
$$

---

## Including the Time Delay

Reintroducing the delay term $e^{-\theta s}$ from Equation (16):

**Equation (20)**

$$
f(t)=
\begin{cases}
0, & t < \theta,\\
1 - e^{-(t - \theta)/\tau}, & t \ge \theta.
\end{cases}
$$

This is the **normalized FOPDT step response**, with $f(0^+)=0$ and
$f(\infty)=1$.

---

## Determination of Parameters \(\theta\) and \(\tau\)

From **Equation (20)**, for any $p\in(0,1)$ with $p=f(t_p)$,

$$
t_p = \theta - \tau\ln(1 - p).
$$

**Equation (21)**

$$
t_p = \theta - \tau \ln(1 - p).
$$

Selecting $p_1 = 0.283$ and $p_2 = 0.632$:

**Equation (22)**

$$
\begin{cases}
t_{28.3} = \theta - \tau\ln(0.717),\\
t_{63.2} = \theta - \tau\ln(0.368).
\end{cases}
$$

Subtracting to eliminate \(\theta\):

**Equation (23)**

$$
t_{63.2} - t_{28.3}
= \tau\bigl[\ln(0.717) - \ln(0.368)\bigr]
= \tau\ln\left(\frac{0.717}{0.368}\right).
$$

Hence:

**Equation (24)**

$$
\tau = \frac{t_{63.2} - t_{28.3}}{\ln(0.717/0.368)},
$$

using:

$$
\ln\left(\frac{0.717}{0.368}\right)\approx 0.667.
$$

**Equation (25)**

$$
\boxed{\tau \approx 1.49\bigl(t_{63.2}-t_{28.3}\bigr).}
$$

Then \(\theta\) can be obtained by substitution:

**Equation (26)**

$$
\theta = t_{28.3} + \tau\ln(0.717) = t_{63.2} + \tau\ln(0.368).
$$

Approximation:

$$
\boxed{\theta \approx t_{28.3} - 0.333\tau}.
$$

---

## Determination of \(k\)

The system gain \(k\) is determined by the steady-state ratio between the output
and input changes:

**Equation (27)**

$$
k = \frac{\Delta y_\infty}{\Delta u} = \frac{y(\infty) - y(0^+)}{\Delta u}.
$$

#### Example

- Input (fan duty) changes from 40% to 55%:  
  $\Delta u = 15\%$.
- Output (temperature) changes from $25.0^\circ\mathrm{C}$ to
  $31.5^\circ\mathrm{C}$:  
  $\Delta y_\infty = 6.5^\circ\mathrm{C}$.

**Equation (28)**

$$
k=\frac{6.5}{15}\approx 0.433\frac{^{\circ}C}{\text{Duty}}.
$$

---

## FOPDT Summary

Thus, the complete FOPDT model is:

$$
\boxed{ G(s) = \frac{k e^{-\theta s}}{\tau s + 1} }.
$$

- \(k\): system gain — steady-state sensitivity
- \(\tau\): time constant — response speed
- \(\theta\): dead time — response delay

and serves as the fundamental representation for modeling and controller tuning.

---

## Application to Controller Design (IMC Method)

Based on the identified FOPDT model:

$$
G(s) = \frac{k e^{-\theta s}}{\tau s + 1}.
$$

the IMC (Internal Model Control) method provides a systematic way to calculate
PID parameters that balance speed and robustness.

---

## Application to Controller Design (IMC Method)

The Internal Model Control (IMC) tuning approach derives PID parameters directly
from the process model:

$$
G(s)=\frac{k e^{-\theta s}}{\tau s + 1}.
$$

A tuning constant $\epsilon$ controls the trade-off between response speed and
robustness: smaller $\epsilon \rightarrow$ faster response, larger
$\epsilon \rightarrow$ greater stability.

---

### IMC–PID Tuning Rules (Rivera 1986, Table II)

The software calculates **two** sets of controller parameters for each
$\epsilon$:

1.  **PID** (Row 1): Applicable for a wide range of ratios.
2.  **Improved PI** (Row 3): Recommended when $\epsilon / \theta > 1.7$.

#### 1. PID Controller (Row 1)

$$
\boxed{%
\begin{aligned}
K_c &= \frac{2\tau + \theta}{k(2\epsilon + \theta)} \\
\tau_I &= \tau + \frac{\theta}{2} \\
\tau_D &= \frac{\tau \theta}{2\tau + \theta}
\end{aligned}}
$$

Converted to standard parallel form gains ($K_p, K_i, K_d$):

$$
K_p = K_c, \quad K_i = \frac{K_c}{\tau_I}, \quad K_d = K_c \tau_D
$$

#### 2. Improved PI Controller (Row 3)

$$
\boxed{%
\begin{aligned}
K_c &= \frac{2\tau + \theta}{2k\epsilon} \\
\tau_I &= \tau + \frac{\theta}{2} \\
\tau_D &= 0
\end{aligned}}
$$

Converted to standard parallel form gains ($K_p, K_i, K_d$):

$$
K_p = K_c, \quad K_i = \frac{K_c}{\tau_I}, \quad K_d = 0
$$

---

### Selection Criteria

Rivera (1986) recommends:

- If **$\epsilon / \theta > 1.7$**, use **Improved PI** (simpler, sufficient
  performance).
- Otherwise, use **PID**.

The software logs both outputs along with the ratio $\epsilon / \theta$,
allowing the user to select the appropriate controller.
