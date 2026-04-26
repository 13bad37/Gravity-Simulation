# GravitySim Theory Notes

I wrote these notes alognside the project. The idea was to keep the reasoning clear enough that the code and physics stay connected. It also acted as my notes as I did the research on the math to complete this project.

## First implementation

The very first version used game style units instead of real SI units. Positions were basically just screen space values, and `G`, masses, and velocities were tuned so the sim felt good and stayed readable.

I started off with the game stle implementation since that was more what I was going for at the start but then changed my mind shortly after. It was useful for getting something working, but I decided it was only going to be temporary. Since this, the project is more grounded in real world physics. 


## Core gravity theory

Newtonian gravity starts from:

$$
F = \frac{G m_1 m_2}{r^2}
$$

Since $F = ma$, the acceleration on body $A$ caused by body $B$ becomes:

$$
a = \frac{G m_B}{r^2}
$$

and it points toward body `B`.

In vector form, that becomes:

$$
\mathbf{a} = G m_B \frac{\boldsymbol{\delta}}{\lVert \boldsymbol{\delta} \rVert^3}
$$

where:

- $\boldsymbol{\delta} = \mathrm{pos}_B - \mathrm{pos}_A$

The sim uses a fixed timestep so the physics step doesn't change just because rendering speed changes.

Ealier bulds used a semi-implicit Euler:

- $v = v + a\,dt$
- $x = x + v\,dt$

That was simple, but wasn't ideal for long term oribatal stability.

## Moving to real units

The current simulation stores physics in physically meaningful units:

- position in metres
- velocity in metres per second
- mass in kilograms
- timestep in seconds

This was an important step because once the simulatiuon started using real scales, things like orbital speed, espace behvaiour and diagnositc values start to actually mean something physically instead of just looking plausible. 

Rendering is still its own problem. Orbital distances are too big to draw literally, so the renderer uses a camera transform and an exaggerated visual radius when needed, which at this stage is a display compromise.

## Integrator upgrade

The simulation now uses velocity Verlet instead of Euler style stepping.

The position update is:

$$
x(t + dt) = x(t) + v(t)\,dt + \frac{1}{2} a(t)\,dt^2
$$

Then acceleration is recomputed at the new position, and velocity is updated with the average of the old and new accelerations:

$$
v(t + dt) = v(t) + \frac{1}{2}\left(a_{\text{old}} + a_{\text{new}}\right)dt
$$

This is also important because orbital systems are pretty sensitive to numerical drift. Velocity Verlet is still simple enough to follow, but it works much better over longer runs.

## Integrator comparison mode

Once I already had diagnostics and drift tracking in place, it stopped making much sense to just keep one integrator hardcoded and assume that was enough. I wanted to actually compare them properly in the same scenes and under the same conditions.

The simulation now lets me switch between:

- semi implicit Euler
- velocity Verlet
- RK4

Semi implicit Euler is the simplest of the three:

$$
\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{a}_n \, dt
$$

$$
\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_{n+1} \, dt
$$

It's cheap and easy to follow, but it usually drifts more and tends to be the weakest option for long orbital runs.

Velocity Verlet is still the default because it's a really good middle ground. It's simple enough to understand, behaves much better for orbital systems, and generally keeps the long term motion healthier than Euler without getting too heavy.

RK4 is different. It takes four slope samples across the timestep and combines them:

$$
\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{dt}{6}\left(\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4\right)
$$

where the full state is:

$$
\mathbf{y} = (\mathbf{x}, \mathbf{v})
$$

RK4 is usually very accurate per step, especially over short to medium runs, but it isn't automatically the "best" choice just because it is higher order. For orbital problems, long term structure matters too, not just local accuracy.

That is basically why the comparison mode exists. It is there so I can run the same setup, switch integrators with `I`, and actually look at the drift values instead of arguing from gut feeling.

## Softening

The simulation uses gravitational softening so forces don't blow up when two bodies get really close.

It's a practical numerical tool not a physics law. It makes the integration more stable during close encounters and keeps the force from becoming really large at small separations.

## Spawning bodies

The spawn system keeps a small `SpawnState` that remembers:

- where the drag started
- where the mouse currently is
- what mass the next body should have

Letting go of the mouse turns that drag vector into velocity:

$$
\mathrm{velocity} = (\mathrm{release} - \mathrm{start}) \cdot \mathrm{scale}
$$

The scale converts the on screen drag into a launch velocity.

Spawned body radius comes from mass with a density assumption. If density is treated as roughly constant, mass grows with volume and volume grows with `r^3`, so radius follows a cube root relationship.

## Preset scenes

A preset scene is just a set of initial conditions for the same gravity law. The physics engine does not change. What changes is where bodies start, how massive they are, and what initial velocities they get.

### Starter scene

The starter scene uses the usual near circular orbit approximation:

$$
v = \sqrt{\frac{GM}{r}}
$$

It gives a good first approximation for a body orbiting a dominant central mass.

### Three body scene

This scene is useful because the general three body problem doesn't have a simple closed form solution. Small changes in the starting state can produce very different results, which makes it a good test of both the physics and the numerics.

### Binary stars

For two equal stars with mass `M` separated by distance `d`, each one orbits the barycentre at radius `d / 2`.

Matching gravity to centripetal motion gives:

$$
\frac{G M^2}{d^2} = \frac{M v^2}{d/2}
$$

so:

$$
v = \sqrt{\frac{GM}{2d}}
$$

### Zero total momentum

For the presets, it's useful to remove unwanted drift by making total momentum zero:

$$
v_i' = v_i - \frac{P_{\text{total}}}{M_{\text{total}}}
$$

This keeps the centre of mass from sliding across the screen for no reason.

## Collision system

The collision system currently models a perfectly inelastic collision, so two bodies merge instead of bouncing. This is an okay firs model since large scale impacts iompacts lose energy to heat, deformation and internal motion.

A collision is detected when the distance between two body centres is less than or equal to their combined physical radii:

$$
\mathrm{distance} \le r_a + r_b
$$

This uses the real physics radius, not the inflated visual radius used for rendering. That is why two bodies can sometimes look like they overlap on screen even though they haven't physically collided yet.

When two bodies merge:

- mass is conserved: $M = m_1 + m_2$
- the new position is the centre of mass:
  $$
  \mathbf{r}_{\text{new}} = \frac{m_1 \mathbf{r}_1 + m_2 \mathbf{r}_2}{m_1 + m_2}
  $$
- the new velocity conserves linear momentum:
  $$
  \mathbf{v}_{\text{new}} = \frac{m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2}{m_1 + m_2}
  $$

This is why velocities aren't just averaged. A heavier body contributes more momentum, so it has more influence on the merged body's final motion.

The merged radius is based on mass, density, and volume instead of just adding radii. Each body has volume:

$$
V = \frac{m}{\rho}
$$

Volumes add during the merge, then the new radius comes from the sphere volume relation:

$$
r = \sqrt[3]{\frac{3M}{4\pi\rho}}
$$

which keeps size tied to physical traits rather than a visual shortcut.

The system also tracks angular momentum. In a 2D simulation, angular momentum only exists as a z component:

$$
L_z = m(xv_y - yv_x)
$$

When bodies merge off centre, some orbital angular momentum becomes spin on the merged body. I haven't implemented any rendering for spin yet, but keeping track of it makes the angular momentum diagnositc more useful.

## Drift system

The drift system was something I added during debugging, but I decided to keep it since if I was asking myself "does the orbit look right?" other people might have the same question. It basically asks how much the simulation has numerically 'drifted' away from the chosen reference state. 

The reference quantities are the conserved ones you care about in an isolated gravity system:

- total energy: $E = K + U$
- total linear momentum: $P = \sum (mv)$
- total angular momentum: $L_z = \sum m(xv_y - yv_x) + \mathrm{spin}$

When the baseline is created, the simulation stores `E0`, `P0`, and `L0`. Each frame, it compares the current state to that baseline:

- energy drift: $d_E = (E - E_0) / \mathrm{scale}_E$
- momentum drift: $d_P = \lVert P - P_0 \rVert / \mathrm{scale}_P$
- angular momentum drift: $d_L = (L - L_0) / \mathrm{scale}_L$

This is useful because a good integrator should keep these values pretty stable over time. If they drift badly, that usually means theres some timestep error, integration error, or a bug in the force or collision logic.

One important caveat is that after a spawn or a merge, the physical system itself has changed. At that point the drift is not purely measuring numerical error anymore, which is why the simulation also lets the baseline be reset manually.
