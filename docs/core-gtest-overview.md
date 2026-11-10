# N-BodySim Core Unit Test Overview

This document is the implementation plan for unit tests covering the
SFML-free `nbody_core` library. The tests will use GoogleTest and link only
against `nbody_core`, so they can run without opening a window or requiring
the application layer.

## Test build and conventions

- Add a GoogleTest dependency to the test CMake configuration and create a
  `nbody_core_tests` executable.
- Register the executable with CTest.
- Keep tests deterministic: use fixed seeds, explicit initial states, and
  `EXPECT_NEAR`/`EXPECT_DOUBLE_EQ` according to the numerical contract.
- Use small, hand-computable systems for force and integrator tests.
- Use invariant checks for conservation properties rather than exact
  trajectories where floating-point integration error is expected.
- Add reusable test helpers for comparing `Vec2`, bodies, total mass,
  momentum, and approximate floating-point values.
- Avoid testing private members directly. Verify behavior through the public
  API and observable world state.

## `Vec2`

Header-only vector value type in `include/nbody/core/Vec2.hpp`.

| Test | Description |
|---|---|
| `Vec2Test.DefaultConstructionIsZero` | Verify a default vector has `x == 0` and `y == 0`. |
| `Vec2Test.AdditionAndSubtractionAreComponentWise` | Check `+` and `-` for positive, negative, and mixed components. |
| `Vec2Test.ScalarMultiplicationWorksInBothOrders` | Verify `vector * scalar` and `scalar * vector` produce identical components, including a negative and zero scalar. |
| `Vec2Test.ScalarDivisionScalesComponents` | Verify both components are divided by a nonzero scalar. |
| `Vec2Test.UnaryMinusNegatesBothComponents` | Check component-wise negation. |
| `Vec2Test.CompoundOperatorsMutateAndReturnReference` | Verify `+=`, `-=`, and `*=` update in place and return the same object for chaining. |
| `Vec2Test.DotAndLengthFunctionsAreCorrect` | Check dot product, squared length, and Euclidean length for a 3-4-5 vector and the zero vector. |
| `Vec2Test.ConstexprOperatorsCanBeEvaluatedAtCompileTime` | Compile-time check representative arithmetic using `static_assert`, preserving the intended `constexpr` contract. |

## `Body` and `Body::Appearance`

Value types in `include/nbody/core/Body.hpp`.

| Test | Description |
|---|---|
| `BodyTest.DefaultConstructionUsesDocumentedDefaults` | Verify default id, mass, radius, position, velocity, and all appearance fields. |
| `BodyTest.FiveArgumentConstructorInitializesPhysicsState` | Verify the legacy five-argument constructor copies all physics fields and supplies default appearance. |
| `BodyTest.SixArgumentConstructorPreservesAppearance` | Verify color, glow, ring, and bands are copied exactly. |
| `BodyTest.AppearanceIsIndependentOfPhysicsFields` | Confirm appearance values can be changed without changing mass, radius, position, or velocity. |

## `GravityField`

Implementation in `src/core/GravityField.cpp`.

| Test | Description |
|---|---|
| `GravityFieldTest.EmptyInputProducesEmptyAcceleration` | Verify an empty body list clears/resizes the output to zero. |
| `GravityFieldTest.SingleBodyHasZeroAcceleration` | Verify no self-force is applied. |
| `GravityFieldTest.TwoEqualBodiesAccelerateTowardEachOther` | Use a one-dimensional pair to check direction and the documented softened magnitude. |
| `GravityFieldTest.MassScalingAffectsAccelerationCorrectly` | Verify each body's acceleration depends on the other body's mass and that unequal masses satisfy equal-and-opposite force. |
| `GravityFieldTest.SofteningChangesOnlyTheExpectedMagnitude` | Compare softened and unsoftened fields and verify the denominator uses `d^2 + epsilon^2`. |
| `GravityFieldTest.CoincidentBodiesDoNotProduceNaNOrInf` | Verify pairs closer than the zero-distance threshold are skipped and produce finite zero acceleration. |
| `GravityFieldTest.OutputIsFullyRewrittenAndResized` | Pre-fill output with stale values, then verify every entry is replaced and the vector size matches the body count for both growth and shrinkage. |
| `GravityFieldTest.TotalMomentumDerivativeIsConserved` | For a multi-body system, verify the mass-weighted acceleration sum is approximately zero. |
| `GravityFieldTest.InputBodiesAreNotModified` | Snapshot the source bodies and verify force evaluation does not change them. |

## `IIntegrator`

Abstract interface in `include/nbody/core/Integrator.hpp`.

| Test | Description |
|---|---|
| `IntegratorContractTest.ConcreteImplementationsExposeStableNames` | Verify Euler reports `"euler"` and RK4 reports `"rk4"` through `IIntegrator`. |
| `IntegratorContractTest.StepPreservesBodyCountAndOrder` | Run each concrete implementation and verify ids/order remain unchanged; body creation and removal belong to `PhysicsWorld`. |

These are contract tests shared by both concrete integrators rather than
tests of an abstract object that cannot be instantiated.

## `EulerIntegrator`

Implementation in `src/core/EulerIntegrator.cpp`.

| Test | Description |
|---|---|
| `EulerIntegratorTest.AdvancesPositionUsingOldVelocity` | Use a body with known velocity and no gravity to verify `position += velocity * dt` uses the pre-step velocity. |
| `EulerIntegratorTest.AdvancesVelocityUsingCurrentAcceleration` | Use a two-body force with a hand-computable acceleration and verify `velocity += acceleration * dt`. |
| `EulerIntegratorTest.ZeroDtLeavesStateUnchanged` | Verify a zero-duration call does not change position or velocity (the world separately enforces positive configured dt). |
| `EulerIntegratorTest.HandlesEmptyAndChangingBodyLists` | Exercise empty input and a body-count change across calls to verify scratch storage does not leak stale acceleration values. |
| `EulerIntegratorTest.DoesNotChangeMassRadiusIdOrAppearance` | Verify integration changes only position and velocity. |

## `Rk4Integrator`

Implementation in `src/core/Rk4Integrator.cpp`.

| Test | Description |
|---|---|
| `Rk4IntegratorTest.ZeroGravityMatchesConstantVelocitySolution` | With `g == 0`, verify the exact constant-velocity position update and unchanged velocity. |
| `Rk4IntegratorTest.MatchesKnownSingleStepReference` | Compare a small two-body step against independently calculated RK4 reference values using a suitable tolerance. |
| `Rk4IntegratorTest.ImprovesAccuracyForSmoothTwoBodyMotion` | Compare one larger step against a high-resolution reference and verify RK4 has lower position error than Euler for the same initial state. |
| `Rk4IntegratorTest.ReusesCorrectScratchSizeAfterBodyCountChanges` | Run steps before and after a body list resize to cover merge-driven changes in the world. |
| `Rk4IntegratorTest.PreservesBodyMetadata` | Verify id, mass, radius, and appearance remain unchanged while position and velocity advance. |
| `Rk4IntegratorTest.DoesNotChangeBodyCountOrOrder` | Verify the integrator itself never creates, removes, or reorders bodies. |

## `PhysicsWorld` and `WorldConfig`

Implementation in `src/core/PhysicsWorld.cpp`.

| Test | Description |
|---|---|
| `PhysicsWorldTest.StartsWithConfiguredSettingsAndEmptyBodies` | Verify the configured timestep, gravity object, zero simulation time/steps/merges, and empty initial body list. |
| `PhysicsWorldTest.StepDelegatesToIntegratorAndAdvancesClock` | Use a small deterministic world to verify one step advances bodies, increments `totalSteps`, and adds exactly the configured dt to simulation time. |
| `PhysicsWorldTest.ResetReplacesBodiesAndClearsStepCounters` | Verify reset replaces the state and resets time, step count, and merge count. |
| `PhysicsWorldTest.ResetAssignsFreshIdsToZeroIdBodies` | Verify zero ids receive unique monotonic ids and ids are not reused across resets. |
| `PhysicsWorldTest.SetIntegratorUsesNewImplementationOnNextStep` | Swap from Euler to RK4 and verify the new integrator is exposed and used by the next step. |
| `PhysicsWorldTest.SetGravityAppliesOnTheNextStep` | Change `g` and softening, then verify subsequent acceleration reflects the new field. |
| `PhysicsWorldTest.SetTimeStepUpdatesConfiguredStep` | Verify a valid positive finite timestep is stored and used for clock advancement. |
| `PhysicsWorldTest.OverlappingBodiesMergeAfterIntegration` | Verify an overlap causes one body to remain, with combined mass, center-of-mass position, momentum-weighted velocity, area-preserving radius rule, and one merge count. |
| `PhysicsWorldTest.LargerMassSurvivesCollision` | Verify the more massive body keeps its id and appearance after a merge. |
| `PhysicsWorldTest.EqualMassCollisionUsesLowerIdAsSurvivor` | Verify the documented deterministic tie-breaker. |
| `PhysicsWorldTest.CollisionConservesMassAndMomentum` | Verify total mass and total linear momentum before and after one or more merges. |
| `PhysicsWorldTest.RepeatedOverlapsAreResolvedUntilStable` | Construct a chain/cluster of overlapping bodies and verify all necessary merges occur in one `step()` and no overlap remains. |
| `PhysicsWorldTest.TouchingBodiesDoNotMerge` | Verify the strict `<` collision condition: bodies exactly at the sum of radii remain separate. |
| `PhysicsWorldTest.AppearanceOfSurvivorIsRetained` | Verify collision resolution does not accidentally copy the loser's cosmetic appearance onto the survivor. |

Invalid constructor pointers and invalid timestep arguments are guarded by
`assert` in the current implementation. If death tests are enabled, add
debug-build death tests; otherwise leave those checks to the existing
assert-enabled build configuration rather than weakening the production
contract.

## Preset and factory functions

Functions in `include/nbody/core/Presets.hpp` and
`src/core/Presets.cpp`.

### Seed folding

| Test | Description |
|---|---|
| `PresetSeedTest.SmallNonnegativeSeedsPassThrough` | Verify seeds in `[0, 2^32)` map to the same `uint32_t` value. |
| `PresetSeedTest.OutOfRangeSeedsFoldDeterministically` | Verify negative and `>= 2^32` seeds produce repeatable folded values and do not rely on time or global RNG state. |

### `twoBody`

| Test | Description |
|---|---|
| `TwoBodyPresetTest.BuildsDefaultCircularPair` | Verify two ids, equal masses, mirrored positions/velocities, `G == 1`, zero softening, and the documented circular speed. |
| `TwoBodyPresetTest.ScalesWithSeparation` | Verify changing separation scales positions and circular speed according to the documented formula. |
| `TwoBodyPresetTest.HyperbolicOptionIncreasesSpeed` | Verify the hyperbolic variant uses 1.6 times the circular speed while preserving geometry. |
| `TwoBodyPresetTest.IgnoresSeed` | Verify identical output for different seeds because this scene is deterministic. |

### `solarSystem`

| Test | Description |
|---|---|
| `SolarSystemPresetTest.BuildsSunAndRequestedPlanetPrefix` | Verify body count, ids, and inside-out planet ordering for representative planet counts. |
| `SolarSystemPresetTest.ClampsPlanetCount` | Verify negative counts clamp to zero and counts above eight clamp to eight. |
| `SolarSystemPresetTest.PlutoRequiresAllEightPlanets` | Verify Pluto is added only when the clamped planet count is eight and the option is enabled. |
| `SolarSystemPresetTest.PlanetsHaveCircularTangentialVelocities` | Verify each planet's position radius and velocity magnitude/direction match the circular-orbit formulas. |
| `SolarSystemPresetTest.SetsExpectedAppearanceMetadata` | Verify sun glow and representative planet colors, bands, and rings. |
| `SolarSystemPresetTest.IgnoresSeed` | Verify the deterministic scene is unchanged for different seeds. |

### `randomCloud`

| Test | Description |
|---|---|
| `RandomCloudPresetTest.ClampsCountToSupportedRange` | Verify counts below two and above 2000 are clamped. |
| `RandomCloudPresetTest.IsDeterministicForSameSeed` | Verify all generated body fields are repeatable for the same signed seed. |
| `RandomCloudPresetTest.DifferentSeedsProduceDifferentClouds` | Verify representative generated state changes for different seeds. |
| `RandomCloudPresetTest.IsPrefixStable` | Verify the first N bodies of a larger cloud exactly match a smaller cloud made with the same seed. |
| `RandomCloudPresetTest.AssignIdsAndValidMassesAndRadii` | Verify ids are sequential, masses are within the documented range, and radii follow the mass rule. |
| `RandomCloudPresetTest.UsesExpectedWorldConfig` | Verify gravity and softening match the random-cloud contract. |

### `figureEight`

| Test | Description |
|---|---|
| `FigureEightPresetTest.BuildsThreeBodyReferenceState` | Verify the three ids, equal masses, mirrored positions, center-of-mass position, and center-of-mass velocity. |
| `FigureEightPresetTest.ScaleUsesKeplerianPositionAndVelocityScaling` | Verify positions scale by `k` and velocities by `1/sqrt(k)` for representative scales. |
| `FigureEightPresetTest.UnitScalePreservesReferenceConstants` | Verify scale 1.0 reproduces the documented reference values. |
| `FigureEightPresetTest.IgnoresSeed` | Verify identical output for different seeds. |

### Dispatch and integrator factories

| Test | Description |
|---|---|
| `PresetFactoryTest.CreatePresetDispatchesAllPresetNames` | Verify each enum selects the corresponding default setup and produces the expected body count/configuration. |
| `PresetFactoryTest.MakeIntegratorCreatesRequestedConcreteType` | Verify Euler and RK4 factory results are non-null, have the correct `name()`, and can execute a step through `IIntegrator`. |

## Suggested implementation order

1. Add the GoogleTest target and shared assertion helpers.
2. Implement `Vec2` and `Body` tests.
3. Implement `GravityField` tests.
4. Implement the shared integrator contract, Euler, and RK4 tests.
5. Implement `PhysicsWorld` lifecycle and collision tests.
6. Implement preset and factory tests.
7. Run the focused test executable, then the complete CTest suite, and adjust
   numerical tolerances only from measured floating-point error rather than
   relaxing assertions broadly.
