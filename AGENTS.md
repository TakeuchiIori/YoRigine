# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Build System

This project uses **Premake5 + Visual Studio 2022** (MSVC v143, C++20, x64).

```
# Step 1: Generate the Visual Studio solution
premake.bat          # runs: premake5 vs2022 → produces YoRigine.sln

# Step 2: Build via MSBuild
msbuild YoRigine.sln /p:Platform=x64,Configuration=Debug
msbuild YoRigine.sln /p:Platform=x64,Configuration=Release
```

CI runs both configurations on push (`.github/workflows/Debug.yml` / `Release.yml`). No automated tests exist.

### Debug vs Release differences
- `USE_IMGUI` is defined only in **Debug** — all ImGui/editor code (`YEngine/Core/Editor/*`, `VfxMeshEditor`, `JsonManager` UI, etc.) is `#ifdef USE_IMGUI` guarded.
- **YGame** builds as a **DLL** in Debug (`GAME_BUILD_DLL` / `GAME_IMPORT_DLL`) and as a **static lib** in Release.
- Resources are only copied to the output directory in Release builds (Debug runs from the repo root via `debugdir`).

---

## Project Structure

| Project | Kind | Role |
|---|---|---|
| `YMath` | StaticLib | Custom math (Vector2/3/4, Matrix4x4, Quaternion, AABB, OBB, Sphere, `Ray`/`RaycastHit`, Easing, Intersection) |
| `YEngine` | StaticLib | Engine core (DirectX 12, scene system, rendering, collision, navigation, audio, VFX, etc.) |
| `YGame` | DLL (Debug) / StaticLib (Release) | Game logic, scenes, player, enemies, weapons, UI. Entry class `MyGame` lives in `YGame/Core/MyGame.{h,cpp}` |
| `YMain` | EXE | Entry point (`YMain/Main.cpp`) — instantiates `MyGame` and calls `Run()` |
| `ImGui` | StaticLib (external vcxproj) | Dear ImGui + extensions (ImGuizmo, GraphEditor, …) |
| `DirectXTex` | StaticLib (external vcxproj) | Texture loading |
| `YResources` | None | Resource files only (no compilation) |

### Include roots (from `premake5.lua`)

Engine: `YEngine`, `YEngine/Core`, `YEngine/Core/DirectX`, `YEngine/Generators`, `YEngine/Graphics`, `YEngine/Utilities`, `YEngine/Model`, `YMath`, `Externals/{nlohmann,DirectXTex,imgui,assimp/include}`.

Game: `YGame`, `YGame/Core`, `YGame/Scenes`, `YGame/GameObjects`, `YGame/UI`.

`premake5.lua` also lists `YEngine/Systems` and `YGame/SystemsApp` as include roots, but **neither directory exists** — they are harmless dead entries. Engine subsystems actually live under `YEngine/Utilities/Systems/…` and are reached via the `YEngine/Utilities` root (e.g. `#include "Systems/Camera/Camera.h"`).

---

## Engine Architecture

### Boot sequence
`WinMain` → `MyGame::Initialize()` → `Framework::Initialize()` → `SceneManager::Initialize()` → first `BaseScene`.

`Framework` (`YEngine/Core/Framework/Framework.h`) owns all engine singletons and drives the `Initialize / Update / Draw / Finalize` loop via `Run()`. `MyGame` (`YGame/Core/MyGame.h`) inherits `Framework` and wires up `SceneManager`, `OffScreen`, and the `SceneFactory`.

### Scene system (`YEngine/Core/SceneSystems/`)
- `SceneManager` (singleton) holds the active `BaseScene` and manages fade transitions.
- `BaseScene` interface: `Initialize / Update / Draw / DrawNonOffscreen / DrawShadow / GetViewProjection`.
- `GameScene` (`YGame/Scenes/MainScenes/Game/`) delegates to a `SubSceneManager` which switches between `FieldScene` and `BattleScene` via callbacks and transition data structs (`SceneDataStructures.h`).
- New scenes register in `YGame/Scenes/CoreScenes/SceneFactory.h`; add the name to `Editor::sceneNames_` if editor navigation is needed.

### Game objects (`YEngine/Generators/Object3D/BaseObject.h`)
All in-game entities inherit `BaseObject`, which provides:
- `WorldTransform wt_` — SRT transform with parent hierarchy.
- Collision slots: `obbCollider_`, `aabbCollider_`, `sphereCollider_` (shared_ptr).
- `JsonManager` fields for JSON-backed parameter persistence.
- Virtual collision callbacks: `OnEnterCollision / OnCollision / OnExitCollision / OnDirectionCollision`.

Concrete game objects live in `YGame/GameObjects/`:
- `Player/` — `Player`, `PlayerMovement`, `PlayerCombat`, `PlayerCombo`, `PlayerCamera` …
- `Enemy/` — `FieldEnemy`, `BattleEnemy`, plus per-enemy managers.
- `Weapon/` — `PlayerSword` (owns a `TrailMeshEmitter` for the swing trail), `PlayerShield`.
- `Ground/` — visual-only floor (currently has **no collider**, see Gotchas).
- `SkyBox/` — sky dome driven by the `CubeMap` generator.

### Collision system (`YEngine/Utilities/Collision/`)
- Three collider shapes: `SphereCollider`, `AABBCollider`, `OBBCollider` — all extend `BaseCollider`. Constructed via `ColliderFactory::Create<T>(owner, &wt, camera, typeId)`.
- `CollisionManager` (singleton) runs narrow-phase per-frame and fires enter/stay/exit callbacks. Filtering is by integer type ID defined in `CollisionTypeIdDef.h`.
- **Raycast API**: `CollisionManager::Raycast(ray, maxDistance, &hit, ignoreTypeIDs)` and `RaycastMasked(ray, maxDistance, layerMask, &hit)`. Used by camera resolution and available for ground/aim probes.
- Area collision (trigger zones): `CircleArea`, `PolygonArea` managed by `AreaManager`.
- Special type `kNavObstacle` is read by `NavGrid::Bake()` to mark impassable cells. New type IDs go in `CollisionTypeIdDef.h`.

### Navigation (`YEngine/Utilities/Systems/Navigation/`)
- `NavGrid`: **XZ-plane** grid baked from `kNavObstacle` AABB colliders. Call `Initialize()` then `Bake(objectManager)` after scene load.
- `NavPathfinder`: A* over `NavGrid`.
- `VisionSystem`: line-of-sight via `NavGrid::HasLineOfSight()`.

### Terrain (`YEngine/Utilities/Terrain/`)
Terrain/heightmap utilities used to back ground-style geometry. Independent from the `Ground` GameObject — the latter is currently visual-only.

### Camera system (`YEngine/Utilities/Systems/Camera/`)
- `CameraDirector` drives a `VirtualCamera` state machine. Camera states (`DefaultCameraState`, `BattleStartCameraState`, `CinematicCameraState`, `ParryCameraState`, …) are switched at runtime.
- `FollowCamera` is the in-game follow camera used by `Player`.
- `CameraCollisionResolver` casts a ray from the player pivot toward the camera and pulls the camera in when it would clip a wall/`StaticWall`. Combines `CollisionManager::Raycast` with area-wall sampling.

### Rendering pipeline (`YEngine/Graphics/`)
- `PipelineManager/YPipelineManager` (singleton) — creates and caches all PSOs by name (`"Sprite"`, `"Object"`, `"Particle"`, `"ShadowMap"`, etc.). Look up root-parameter indices by name to avoid magic numbers.
- `OffScreen` / `PostEffectChain` / `PostEffectManager` (`YEngine/Generators/OffScreen/`) — off-screen render target + post-effect passes (blur, outline, radial blur, tone mapping, dissolve, chromatic aberration).
- `Buffers`, `Drawer`, `LightManager`, `WorldTransform`, `ComputeShaderManager` — supporting graphics layers.
- `SrvManager`, `RtvManager`, `DsvManager` (`YEngine/Core/DirectX/`) — descriptor heap management.
- `CubeMap` (`YEngine/Generators/CubeMap/`) — cubemap resource feeding `SkyBox` and IBL.
- `PipCameraSystem` (`YEngine/Generators/PipCamera/`) — picture-in-picture / secondary-view render targets.

### VFX systems

Two unrelated families: **particles** (point/sprite swarms) and **procedural meshes** (continuous strips/volumes). Pick by shape, not by name.

#### Particles

The legacy `ParticleManager` has been removed (see `YGame/Core/MyGame.cpp:2`). The current setup is:

| System | Path | Use for |
|---|---|---|
| `YParticle` (CPU) | `YEngine/Generators/Particle/YParticle*` | All CPU-side particle work. Module-based (`Particle/Modules/Spawn/…`, `Update/…`), JSON-defined systems. |
| GPU compute particles | `YEngine/Generators/GPUParticle/` | High count, GPU-resident (`GPUEmitter`, `GpuEmitManager`, HLSL `UpdateParticle.CS.hlsl`). |

Key facade types:
- **`YParticleManager`** (singleton) — owns `YParticleSystem`s. Initialised in `MyGame::Initialize` (`MyGame.cpp:41`). Load systems from JSON via `LoadSystemsFromFile("Resources/Json/YParticleSystems/*.json")` or bundles via `LoadEffectBundle("Resources/Json/YEffects/*.json")`.
- **`YEmitterGroupManager`** (singleton) — owns `YEmitterGroup`s. Load via `LoadGroupFromFile("Resources/Json/YEmitterGroups/*.json")`.
- **`EffectHandle`** — gameplay-facing ergonomic wrapper. Use this from game code, not the raw emitters:
  ```cpp
  EffectHandle hit = EffectHandle::PlayOneShot("HitSpark", hitPos);
  slashHandle_ = EffectHandle::Play("SlashTrail", swordPos, /*loop*/true);
  slashHandle_.SetPosition(swordTipPos); slashHandle_.Stop();
  EffectHandle::EmitAll({"Explosion","Smoke","Debris"}, blastPos, 30);
  ```
- **`GpuEmitManager`** (singleton) — initialised in `MyGame.cpp:62`. Owns GPU emitters configured from `Resources/Json/GpuEmitters/`.

#### VFX procedural meshes (`YEngine/Generators/Vfx/VfxMesh/`)

For shapes particles cannot express (continuous adjacency, ribbons, beams). **No manager singleton** — owners (e.g. `PlayerSword`) hold their emitter directly. Common base: `ProceduralMeshBase` (shared `ProceduralMeshVertex` = position / texcoord / color / age).

- **`TrailMesh` + `TrailMeshEmitter`** — Catmull-Rom smoothed ribbon used by `PlayerSword` for swing trails. Supports Flat/Arc/Fan shapes, dual-edge points (`tip`, `root`), per-vertex age fade. Rich shader CB (`MeshTrailParamsCB`: rim color, fresnel, sparkle, color waves, UV scroll). Asset: `Resources/Vfx/*.json` via `VfxEffectAsset`.
- **`LightVolumeMesh`** — volumetric light beam geometry.
- **`VfxMeshEditor`** — ImGui editor for tuning trails/volumes (Debug only).

Rule of thumb: **point/spark/smoke → `EffectHandle` (YParticle); high count/screen-fill → GPU particles; continuous strip/ribbon/beam → `VfxMesh`**. Do not try to express ribbons as particles — they need adjacency neither particle system tracks.

### ImGui editor (`YEngine/Core/Editor/`, Debug-only)
`Editor` (singleton) hosts a dockspace layout with a game viewport and registered `GameUI` panels. Objects and scenes register panels via `Editor::RegisterGameUI(name, drawFunc, sceneName)`. The `ModelManipulator` (`YEngine/Generators/ModelManipulator/`) provides scene-object drag-drop, prefab creation/loading, and JSON scene serialisation.

### JSON parameter system (`YEngine/Utilities/Loaders/Json/JsonManager.h`)
`JsonManager` registers C++ variables by name, persists them to `Resources/Json/…`, and exposes them in the ImGui editor. Pattern:
```cpp
jsonManager_ = std::make_unique<YoRigine::JsonManager>("FileName", "Resources/Json/Category/");
jsonManager_->Register("speed", &speed_);   // auto-load on register
jsonManager_->Save();                        // save on demand
```

### Player architecture (`YGame/GameObjects/Player/`)
`Player` owns separate `PlayerMovement` and `PlayerCombat` components, each with its own state machine (`StateMachine.h`). Combat states: Idle, Attacking, Dodging, Guarding, Hit, Stunned, Dead. Movement states: Idle, Moving. `PlayerCamera` and the `Combo`/`AttackCameraComponent` subfolders drive attack-time camera effects.

### Enemy architecture (`YGame/GameObjects/Enemy/`)
- `FieldEnemy`: patrols/searches/chases on the field map (Patrol/Alert/Search/Chase states). Uses `NavGrid` + `VisionSystem`.
- `BattleEnemy`: full combat AI (Idle/Approach/Attack variants/Damage/Downed/Recovery/Dead).

---

## Key Conventions

- **Naming**: classes PascalCase, member variables trailing underscore (`speed_`); engine-internal classes live in `YoRigine::` namespace.
- **Includes**: engine headers are included by short relative path from one of the include roots (e.g. `#include "Systems/Camera/Camera.h"` resolves via `YEngine/Utilities`).
- **Magic-number PSO indices**: look up by name through `YPipelineManager` rather than hard-coding root-parameter indices.

### Adding new things

| Adding… | Steps |
|---|---|
| **GameObject** | Inherit `BaseObject`, implement `Initialize(Camera*)`, `InitCollision()`, `InitJson()`, `Update()`, `Draw()`. Place under `YGame/GameObjects/<Category>/`. |
| **Scene** | Inherit `BaseScene`, register in `YGame/Scenes/CoreScenes/SceneFactory.h`, add to `Editor::sceneNames_` if needed. |
| **Collision type** | Add ID in `YEngine/Utilities/Collision/Core/CollisionTypeIdDef.h` + update `CollisionTypeIdToString` and `LayerFromTypeId`. |
| **Collider on an object** | Use `ColliderFactory::Create<OBBCollider>(this, &wt_, camera_, typeId)` inside `InitCollision()`. Tune via `JsonManager`. |
| **PSO** | Register a new entry in `YPipelineManager`; look it up by string name from drawers. |
| **Particle effect** | Author JSON under `Resources/Json/YParticleSystems/` (+ optional `YEmitterGroups/`), load via `YParticleManager::LoadSystemsFromFile`, trigger from gameplay with `EffectHandle::Play / PlayOneShot / Burst`. GPU-resident → `GpuEmitManager` + JSON in `Resources/Json/GpuEmitters/`. Ribbon/trail → `TrailMeshEmitter` owned directly (not particles). |

---

## Gotchas

- **`Ground` has no collider.** It is currently a pure render object — there is no engine-side ground collision and `Player` Y is effectively fixed at 0. Any slope/jump work needs a collider added to `Ground` (or a dedicated ground type) plus a downward Raycast probe in `PlayerMovement::ApplyMovement`.
- **`NavGrid` is XZ-only.** It bakes from `kNavObstacle` AABBs in a flat plane. Multi-floor or sloped terrain breaks `FieldEnemy` pathing — keep enemy nav zones flat, or extend `NavGrid` to carry per-cell height before introducing height variation in playable areas.
- **`USE_IMGUI` is Debug-only.** Anything inside `#ifdef USE_IMGUI` (the entire `Editor`, `VfxMeshEditor`, JSON inspector UIs, gizmos, debug draws) is absent in Release. Do not put gameplay-essential logic inside those guards.
- **YGame is a DLL in Debug, static in Release.** Anything crossing the YGame boundary needs the `GAME_API` macro (`YGame/GameAPI.h`); pure-template/header-only utilities are safer kept engine-side.
- **Mesh Shader support is detected but unused.** `DeviceManager::SupportsMeshShaders()` exists, but no PSO uses MS. Pipeline additions can rely on the detection flag for fallback paths.
- **Stale `ParticleManager` forward decl.** `YEngine/Core/YoRigineContext.h:11,39` still references `YoRigine::ParticleManager*`, but the class itself was deleted during the YParticle migration (see `MyGame.cpp:2`). Ignore that pointer; do not try to revive it.
- **`CameraCollisionResolver` ignores certain type IDs.** When adding new world geometry, decide whether the camera should treat it as a clipper or pass through, and update its ignore list accordingly.
