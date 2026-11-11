# N-Body Simulation Intro
An **N-body simulation** predicts how a collection of particles moves relative to each other while
experiencing gravity. In this project, each body is a
point-like object in a 2D world with a mass $m$, position vector
$\mathbf{r}$, and velocity vector $\dot{\mathbf{r}}$.

$$
\mathbf{r} = \begin{bmatrix} x \\ y \end{bmatrix},
\qquad
\dot{\mathbf{r}} = \begin{bmatrix} \dot{x} \\ \dot{y} \end{bmatrix}
\qquad
\ddot{\mathbf{r}} = \begin{bmatrix} \ddot{x} \\ \ddot{y} \end{bmatrix}
$$

## 1. Newton's law of gravity

For two bodies $i$ and $j$, Newton's law says that the attractive force has
magnitude

$$
F_{ij} = G\frac{m_i m_j}{r_{ij}^{2}},
$$
where $G$ is the gravitational constant and $r_{ij}$ is the distance
between the bodies. The force points from body $i$ toward body $j$.

The simulation uses Newton's second law, $\mathbf{F}=m\ddot{\mathbf{r}}$, to turn
force into acceleration. The acceleration of body $i$ caused by body $j$
is

$$
\ddot{\mathbf{r}}_{i\leftarrow j}
  = Gm_j\frac{\mathbf{r}_j-\mathbf{r}_i}{r_{ij}^{3}}.
$$

Notice that $m_i$ is not in this final expression: a more massive body feels
more force, but it also has proportionally more inertia.

## 2. Adding every gravitational influence

Body $i$ is pulled by *all* the other bodies, not just its closest neighbor.
Because gravitational accelerations add as vectors, the complete acceleration
is the sum of the contributions:

$$
\ddot{\mathbf{r}}_i
  = \sum_{\substack{j=1\\j\ne i}}^{N}
    Gm_j\frac{\mathbf{r}_j-\mathbf{r}_i}{r_{ij}^{3}}.
$$

At each simulation step, the program:

1. Looks at every pair of bodies.
2. Finds their separation and the direction between them.
3. Adds that pair's acceleration to both bodies in opposite directions.
4. Uses the resulting $\ddot{\mathbf{r}}_i$ to update position and velocity.

Updating both bodies with equal-and-opposite pair contributions is Newton's
third law in action. It also helps preserve the system's total momentum.
Computationally, checking all pairs costs approximately $N^2$ work, which is
why large simulations become more expensive quickly.

Very small separations can make the ideal $1/r^2$ force extremely large. This
simulator uses a small softening value in the denominator,
$r^2+\varepsilon^2$, to keep close encounters numerically stable:

$$
\ddot{\mathbf{r}}_{i\leftarrow j}
  = Gm_j\frac{\mathbf{r}_j-\mathbf{r}_i}
  {(r_{ij}^{2}+\varepsilon^2)r_{ij}}.
$$

## 3. Numerical integration

The equations describe continuous motion, but a computer advances the system
in finite time steps of size $\Delta t$. An **integrator** approximates what
happens during each step.

### Euler step

Euler is the simplest approximation. It evaluates acceleration once, at the
current positions, and advances using

$$
\mathbf{r}(t+\Delta t)
  \approx \mathbf{r}(t)+\dot{\mathbf{r}}(t)\Delta t,
\qquad
\dot{\mathbf{r}}(t+\Delta t)
  \approx \dot{\mathbf{r}}(t)+\ddot{\mathbf{r}}(t)\Delta t.
$$

It is fast because it needs one gravity calculation per step. However, the
approximation error accumulates, so an orbit may slowly spiral, gain energy, or
become unstable when $\Delta t$ is too large.

### Classical fourth-order Runge--Kutta (RK4)

RK4 makes four trial evaluations within one time step. It samples the motion
at the beginning, two intermediate estimates, and the end, then combines them
with weights:

$$
\mathbf{y}_{n+1}
 = \mathbf{y}_n
 + \frac{\Delta t}{6}
   \left(\mathbf{k}_1+2\mathbf{k}_2+2\mathbf{k}_3+\mathbf{k}_4\right),
\qquad
\mathbf{y}=
\begin{bmatrix}\mathbf{r}\\\dot{\mathbf{r}}\end{bmatrix}.
$$

Here, each $\mathbf{k}$ is a derivative estimate containing both velocity
and acceleration. RK4 costs about four gravity evaluations per step, but it
usually follows curved orbits much more accurately than Euler for the same
time step.

For the coupled state $\mathbf{y}=[\mathbf{r};\dot{\mathbf{r}}]$, define
$\mathbf{f}(\mathbf{y})=[\dot{\mathbf{r}};\ddot{\mathbf{r}}(\mathbf{r})]$. The four
derivative estimates are

$$
\begin{aligned}
\mathbf{k}_1 &= \mathbf{f}(\mathbf{y}_n),\\
\mathbf{k}_2 &= \mathbf{f}\left(\mathbf{y}_n
             + \frac{\Delta t}{2}\mathbf{k}_1\right),\\
\mathbf{k}_3 &= \mathbf{f}\left(\mathbf{y}_n
             + \frac{\Delta t}{2}\mathbf{k}_2\right),\\
\mathbf{k}_4 &= \mathbf{f}\left(\mathbf{y}_n
             + \Delta t\,\mathbf{k}_3\right).
\end{aligned}
$$

In other words, RK4 estimates the acceleration at the current position, at
two halfway trial positions, and at a final trial position. Those estimates
are then combined using the weighted average above. The trial states are
temporary: only the final weighted result becomes the next position and
velocity.

## Takeaway

The simulation repeatedly performs one cycle:

$$
\text{positions}
\;\longrightarrow\;
\text{pairwise gravity}
\;\longrightarrow\;
\text{total accelerations}
\;\longrightarrow\;
\text{Euler or RK4 update}.
$$

The resulting paths are not pre-drawn ellipses. They emerge from applying
Newton's laws to every body, one small time step at a time.
