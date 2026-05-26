# Physics-Driven Orbits Design

Date: 2026-05-26
Status: Approved concept, pending implementation plan

## Goal

Make OrbitGuard feel closer to real orbital motion while keeping it playable for the current project. The target experience is not full aerospace simulation; it is a game-friendly physics model where satellites, debris, collision warnings, and avoidance burns follow understandable physical rules.

The normal visual speed target is:

- PlayerSat should take about 30 seconds to complete a half orbit.
- A full orbit at the default MEO launch radius should take about 60 seconds.
- Lower orbits may move faster and higher orbits slower, but the game should stay readable.

## Current Problem

Objects currently move along fixed circular paths by incrementing `angleRad` with `angularSpeed`. Collision debris can be scripted to move directly toward PlayerSat. This is easy to control, but it does not feel physical:

- Orbit speed is visually too fast.
- Prediction windows of 8-12 seconds are not meaningful because objects move too much in that time.
- Avoidance changes orbit parameters directly instead of applying a physical maneuver.
- Debris interception feels scripted rather than orbital.

## Chosen Approach

Use a simplified two-body physics model with game-tuned scale.

Earth remains the central gravity source. Each physics-driven object stores position and velocity. Every frame, the simulation applies central gravity and integrates motion. The gravity constant is tuned so the default PlayerSat orbit completes one full orbit in about 60 seconds.

The model should preserve the current gameplay language:

- LEO / MEO / GEO labels remain.
- Launch settings still choose radius, inclination, speed bias, and initial angle.
- Risk levels remain visible.
- Pressing `A` still performs avoidance.
- Collision debris still appears as a distinct object.

## Object Model

`OrbitObject` should gain physics fields:

- `Vector3 velocity`
- `float collisionRadius`
- `bool physicsDriven`

Existing fields such as `orbitRadius`, `inclinationDeg`, `initialAngleDeg`, and `angleRad` can remain as UI and launch parameters. For physics-driven objects, position and velocity become the source of truth during motion. Orbit radius and angle can be refreshed from position for display and orbit classification.

## Motion Model

The update loop should use a simple stable integrator, preferably semi-implicit Euler for the first implementation:

```text
acceleration = -earthMu * position / |position|^3
velocity += acceleration * physicsDelta
position += velocity * physicsDelta
```

`earthMu` should be game-tuned, not real-world SI units. It should produce about a 60-second full orbit at the default MEO radius.

The existing `timeScale` control remains, but the default game speed should feel readable. Higher time scales may speed up prediction and observation, but the baseline should be the 30-second half-orbit target.

## Launch Behavior

Launching PlayerSat creates position from the current launch settings, then derives a tangent velocity:

```text
speed = sqrt(earthMu / radius) * speedBias
velocity = tangentDirection * speed
```

The existing `Angular Speed` UI field should become a speed bias. It may keep the same visual slot at first, but the label should eventually be updated to avoid implying direct angular motion.

## Debris Behavior

Impact debris should spawn near PlayerSat with an initial position and velocity that naturally approaches PlayerSat under the same physics model. It should not be moved by directly stepping toward PlayerSat except as a temporary fallback if the first implementation cannot reliably create encounters.

If PlayerSat avoids the debris:

- The debris continues away under physics.
- Once it is safely distant or the warning resolves, it is removed.

If debris hits PlayerSat:

- PlayerSat is destroyed.
- The impact debris is removed too.
- The explosion visual remains at the impact position.

## Avoidance Behavior

Avoidance should apply a small delta-v impulse to PlayerSat instead of directly changing orbit radius or inclination.

The first implementation can keep the existing `AvoidancePlan` shape, but the plan should represent:

- A proposed delta-v vector.
- Predicted closest approach before the burn.
- Predicted closest approach after the burn.

Pressing `A` starts a short burn animation and applies the delta-v. The visual can still highlight old and new orbit paths, but those paths should be treated as approximations derived from predicted positions.

## Risk Prediction

Because the target speed is slower, prediction becomes useful over longer simulation windows.

First implementation:

- Predict future closest approach for about 60-120 seconds of simulation time.
- Use a fixed small time step for prediction.
- Compare PlayerSat against debris threats.
- Trigger warning when predicted closest approach is below the high or medium risk thresholds.

The prediction should not mutate the real object list.

## UI and Controls

Keep the current controls and panels as much as possible.

Recommended text updates:

- Replace or supplement `Angular Speed` with `Speed Bias`.
- Show predicted closest approach time when available.
- Keep `A` as the avoidance key.
- Keep the top-left back arrow.

The user should not need to understand orbital mechanics. The UI should communicate practical choices: launch, monitor, avoid, report.

## Testing

Add or update smoke tests for:

- Default MEO PlayerSat completes about half an orbit in 30 seconds.
- Physics updates preserve a stable orbit radius within a reasonable tolerance.
- Launch creates nonzero velocity tangent to the orbit.
- Avoidance changes PlayerSat velocity rather than directly teleporting orbit parameters.
- Prediction detects a future close approach over a 60-120 second window.
- Impact debris is removed after either successful avoidance or collision.
- Existing menu/back-button and mission tests remain passing.

## Non-Goals

This phase will not add:

- Moon or solar gravity.
- Atmospheric drag.
- Earth oblateness / J2 perturbation.
- Real-world units or exact orbital periods.
- Full Lambert targeting or precise aerospace-grade conjunction analysis.

Those can be future upgrades after the simplified physical model feels good.
