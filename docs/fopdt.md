# Introduction

This document derives the First-Order Plus Dead Time (FOPDT) model from first principles, explains
how to identify system parameters \(K\), \(T\), and \(L\) from step-response data, and presents
IMC-based formulas for practical PID controller tuning.

## FOPDT Model Derivation

FOPDT (First-Order Plus Dead Time) is a widely used method for modeling dynamic systems and serves
as a fundamental basis for controller design.

---

## Mathematical Derivation of the FOPDT Model

In the FOPDT model, we assume that the rate of change of the system output is proportional to the
difference between the desired value (setpoint) and the current output:

**Equation (1)**

$$
\frac{dy(t)}{dt} \propto \bigl(u(t) - y(t)\bigr).
$$

where:

- \( $y(t)$ \): system output (e.g., temperature)
- \( $u(t)$ \): system input (e.g., fan duty or heating power)

To make this relationship explicit, we introduce two parameters:

- \( $K$ \): steady-state gain (input-to-output amplification)
- \( $T$ \): time constant (response speed)

Thus, the first-order differential equation becomes:

**Equation (2)**

$$
\frac{dy(t)}{dt} = \frac{1}{T}\bigl(Ku(t) - y(t)\bigr), \quad T > 0, \quad K \in \mathbb{R}.
$$

Multiplying both sides by \(T\):

**Equation (3)**

$$
T\frac{dy(t)}{dt} = K u(t) - y(t).
$$

---

## Laplace Domain Representation

Taking the Laplace transform of Equation (3), and **assuming zero initial conditions** $y(0)=0$:

$$
\mathcal{L}\left[\frac{dy(t)}{dt}\right] = sY(s), \qquad
\mathcal{L}\left[y(t)\right] = Y(s), \qquad
\mathcal{L}\left[u(t)\right] = U(s).
$$


Hence,

**Equation (4)**

$$
TsY(s) = K U(s) - Y(s).
$$

Rearranging:

**Equation (5)**

$$
(Ts + 1)Y(s) = K U(s).
$$

The system is **linear and time-invariant (LTI)**.  
Thus, the transfer function is:

**Equation (6)**

$$
G_0(s) = \frac{Y(s)}{U(s)} = \frac{K}{Ts + 1}.
$$

---

## Including Time Delay

To model a real process with **dead time** \(L\),  
the input can be written as:

**Equation (7)**

$$
u_d(t) = u(t - L),
$$

where \(L\) represents the delay between the input change and the observed system response.

To ensure the delay has no effect before \(t = L\), we use the Heaviside step function:

**Equation (8)**

$$
u_d(t) = u(t - L)H(t - L),
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
U_d(s) = e^{-Ls}U(s).
$$

Substituting into Equation (3):

**Equation (10)**

$$
(Ts + 1)Y(s) = K e^{-Ls} U(s).
$$

Therefore, the **FOPDT transfer function** is:

**Equation (11)**

$$
G(s) = \frac{Y(s)}{U(s)} = \frac{K e^{-Ls}}{Ts + 1}.
$$

---

## Normalization of the Step Response

The steady-state change of output is:

**Equation (12)**

$$
\Delta y_\infty = y(\infty) - y(0^+) = K\Delta u,
$$

Assuming deviation form ($y(0^+)=0$):

$$
y(\infty) = K\Delta u.
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
Y(s) &= \frac{K e^{-Ls}}{Ts + 1}\cdot\frac{\Delta u}{s} = \frac{K\Delta u e^{-Ls}}{s(Ts + 1)}.
\end{aligned}
$$


Dividing both sides by $K\Delta u$ gives:

**Equation (16)**

$$
F(s) = \frac{Y(s)}{K\Delta u} = \frac{e^{-Ls}}{s(Ts + 1)}.
$$

Here $F(s) = \mathcal{L}\{f(t)\}$,
and the exponential term $e^{-Ls}$ represents the time delay in the frequency domain.


---

## Removing Delay for Simplification

When \(L = 0\), we obtain the base form:

**Equation (17)**

$$
F(s) = \frac{1}{s(Ts + 1)}.
$$

Applying **partial-fraction decomposition** and dividing by \(T\):

**Equation (18)**

$$
F(s) = \frac{1}{s(Ts + 1)} = \frac{1}{s} - \frac{T}{Ts + 1}.
$$

Taking the inverse Laplace transform:

**Equation (19)**

$$
f(t) = 1 - e^{-t/T}, \quad t \ge 0.
$$

---

## Including the Time Delay

Reintroducing the delay term $e^{-Ls}$ from Equation (16):

**Equation (20)**

$$
f(t)=
\begin{cases}
0, & t < L,\\
1 - e^{-(t - L)/T}, & t \ge L.
\end{cases}
$$

This is the **normalized FOPDT step response**, with $f(0^+)=0$ and $f(\infty)=1$.


---

## Determination of Parameters \(L\) and \(T\)

From **Equation (20)**, for any $p\in(0,1)$ with $p=f(t_p)$,

$$
t_p = L - T\ln(1 - p).
$$

**Equation (21)**

$$
t_p = L - T \ln(1 - p).
$$

Selecting $p_1 = 0.283$ and $p_2 = 0.632$:


**Equation (22)**

$$
\begin{cases}
t_{28.3} = L - T\ln(0.717),\\
t_{63.2} = L - T\ln(0.368).
\end{cases}
$$

Subtracting to eliminate \(L\):

**Equation (23)**

$$
t_{63.2} - t_{28.3}
= T\bigl[\ln(0.717) - \ln(0.368)\bigr]
= T\ln\left(\frac{0.717}{0.368}\right).
$$

Hence:

**Equation (24)**

$$
T = \frac{t_{63.2} - t_{28.3}}{\ln(0.717/0.368)},
$$

using:

$$
\ln\left(\frac{0.717}{0.368}\right)\approx 0.667.
$$

**Equation (25)**

$$
\boxed{T \approx 1.49\bigl(t_{63.2}-t_{28.3}\bigr).}
$$

Then \(L\) can be obtained by substitution:

**Equation (26)**

$$
L = t_{28.3} + T\ln(0.717) = t_{63.2} + T\ln(0.368).
$$

Approximation:

$$
\boxed{L \approx t_{28.3} - 0.333T}.
$$



---

## Determination of \(K\)

The system gain \(K\) is determined by the steady-state ratio between the output and input changes:

**Equation (27)**

$$
K = \frac{\Delta y_\infty}{\Delta u} = \frac{y(\infty) - y(0^+)}{\Delta u}.
$$

#### Example

- Input (fan duty) changes from 40% to 55%:  
  $\Delta u = 15\%$.
- Output (temperature) changes from $25.0^\circ\mathrm{C}$ to $31.5^\circ\mathrm{C}$:  
  $\Delta y_\infty = 6.5^\circ\mathrm{C}$.

**Equation (28)**

$$
K=\frac{6.5}{15}\approx 0.433\frac{^{\circ}C}{\text{Duty}}.
$$


---

## FOPDT Summary

Thus, the complete FOPDT model is:

$$
\boxed{ G(s) = \frac{K e^{-Ls}}{T s + 1} }.
$$

- \(K\): system gain — steady-state sensitivity
- \(T\): time constant — response speed
- \(L\): dead time — response delay

and serves as the fundamental representation for modeling and controller tuning.

---

## Application to Controller Design (IMC Method)

Based on the identified FOPDT model:

$$
G(s) = \frac{K e^{-Ls}}{T s + 1}.
$$

the IMC (Internal Model Control) method provides a systematic way to calculate PID parameters that
balance speed and robustness.

---

## Application to Controller Design (IMC Method)

The Internal Model Control (IMC) tuning approach derives PID parameters directly from the process model:

$$
G(s)=\frac{K e^{-L s}}{T s + 1}.
$$

A tuning constant $\lambda$ controls the trade-off between response speed and robustness:
smaller $\lambda \rightarrow$ faster response, larger $\lambda \rightarrow$ greater stability.



---

### IMC–PID Tuning Rules (Fully Expanded Form)

The IMC-based PID coefficients are given by:

**Equation (29)**

$$
\boxed{%
\begin{aligned}
K_p &= \frac{T}{K(\lambda + L)}, \\
K_i &= \frac{1}{K(\lambda + L)\left(T + \frac{L}{2}\right)}, \\
K_d &= \frac{T^{2} L}{K(\lambda + L)(2T + L)}.
\end{aligned}}
$$


where:

- \($K_p$, $K_i$, $K_d$\): proportional, integral, and derivative gains
- \($K$\): process gain
- \($T$\): time constant
- \($L$\): dead time
- \($\lambda$\): IMC filter parameter, 0.5~2

---



