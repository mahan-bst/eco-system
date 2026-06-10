# EcoSim — a neuro-evolution sandbox

A predator / prey / food ecosystem in C++ and **SFML 3**. Nothing is scripted
and there are no behaviour rules: every creature is driven by its own small
**neural network**, inherited from its parent with mutation. Brains that find
food (or catch prey) reproduce; brains that don't, starve. Natural selection
does the programming.

## The rules

| | Prey (teal circles) | Predators (red triangles) |
|---|---|---|
| Eats | food (green dots), automatically on contact | prey, automatically on contact |
| Body | identical for every individual (speed, vision, size are species constants) | same |
| Brain | its own neural net — the **only** thing that is inherited and mutated | same |
| Death | energy reaches 0 (prey can also be eaten) | energy reaches 0 |
| Reproduction | solo, at ≥ 80 % of energy capacity; the child gets 40 % of the parent's energy and a mutated copy of its brain | same |

Energy drains constantly (`base + moveCost · throttle²`), so both standing
still forever and sprinting blindly are losing strategies — the network has
to learn *when* to move and *where*.

If a species drops below a small floor, a few "immigrants" appear (mutated
descendants of survivors, or fresh random brains after a true extinction),
so the experiment never dead-ends.

## The brain

A fixed-topology feed-forward network, small enough to read on screen but
big enough to produce real behaviour:

```
10 inputs  →  8 hidden (tanh)  →  2 outputs
```

All senses are **egocentric** (relative to the creature's own heading), so a
brain works identically anywhere on the map:

| # | Input | Meaning |
|---|---|---|
| 0–2 | target signal / sin / cos | nearest thing it eats (food for prey, prey for predators) |
| 3–5 | other signal / sin / cos | prey: nearest predator · predator: nearest rival |
| 6 | energy | own energy / capacity |
| 7 | wall | proximity of the nearest wall |
| 8–9 | centre sin / cos | direction of the world centre |
| | **Outputs** | **turn** (−1..1, tanh) and **throttle** (0..1, sigmoid) |

Mutation adds gaussian noise to every weight, plus a 2 % chance per weight of
a complete rewire. There is no crossover and no back-propagation — pure
selection.

## The brain viewer

**Left-click any creature.** A live diagram of its network appears: node
colours show the current activations (teal = positive, orange = negative)
and connection brightness shows the signal flowing through each synapse
*this tick* — you can literally watch a predator's "prey" inputs light up
and propagate into a hard turn. The selected creature is marked in the world
with a white ring and its vision circle. Click empty space to deselect.

## Controls

| Key | Action |
|---|---|
| `Space` | pause / resume |
| `+` / `-` (or `↑` / `↓`) | time speed ×1, ×2, ×4, ×8, ×16, ×32 |
| `.` | single step while paused |
| `V` | show vision rings for everyone |
| `T` | tuning sliders — drag to change parameters while it runs |
| `S` | save the world to a `.eco` file (a file dialog asks where) |
| `O` | load a previously saved world |
| `R` | restart the simulation |
| left click | select a creature / open its brain |
| right click | drop a burst of food at the cursor |

The tuning panel exposes food spawn rate, food energy, mutation strength,
prey/predator metabolism multipliers and the predator meal-gain multiplier —
all applied instantly.

## Save / load

`S` snapshots the *entire* world into a binary `.eco` file: simulation time,
every food item, every prey and predator (position, heading, energy, age, id)
including their full brain weights, plus the current slider values. `O` loads
one back. Very handy for presentations: evolve a competent population at ×32
beforehand, save it, and open the trained world live instead of waiting.
Loading validates the file and leaves the running world untouched if the file
is corrupt or from an incompatible build (the brain topology is checked).

## The charts

The right panel records the whole run (the sample buffer compacts itself, so
even hour-long runs fit):

1. **Population** — prey / predator / food counts. Expect noisy randomness
   early, then Lotka–Volterra-style oscillations once brains get competent.
2. **Prey / Predator — average energy & lifespan** — the cleanest signal that
   evolution is working: average age climbs as brains learn to survive.

Good talking points for a presentation: at t = 0 everyone spins in circles
(random brains); within a few minutes prey visibly beeline to food; later,
prey start veering away when a predator closes in.

## Building

All knobs live in [src/Config.hpp](src/Config.hpp); the network topology is
in [src/Brain.hpp](src/Brain.hpp). The CMake project expects a static SFML 3
at `D:/libs/SFML-3.1.0` (edit `SFML_DIR` in [CMakeLists.txt](CMakeLists.txt)
if yours lives elsewhere):

```
cmake -B build
cmake --build build
```

UI text uses a system font (Segoe UI / Arial / Consolas on Windows); if none
is found the simulation still runs, just without labels.
