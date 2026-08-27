# Feature Roadmap

This is a living roadmap connecting [ARCHITECTURE.md](../ARCHITECTURE.md) (how the engine is
built) to [GDD.md](GDD.md) (what the game is). It orders the work from the current engine
scaffold (SDL3 window, RmlUi, empty ECS, `HelloWorldLayer` — nothing else) through to a
playable game, milestone by milestone.

Per [CLAUDE.md](../CLAUDE.md)'s division-of-labor: Claude builds the systems and tooling in
each epic below; the user authors the real game content through them. Claude does not author
game content on its own initiative beyond minimal test fixtures.

**Every epic must be considered through three lenses:**

1. **Gameplay systems required** — the engine/runtime code (`Core`/`App`).
2. **Game UI required** — what the player sees and interacts with.
3. **Which data should be authorable through data and/or the editor** — rather than
   hard-coded, per `CLAUDE.md`'s data-driven-where-it-pays-off principle.

Each epic below states these as **Engine** / **UI** / **Editor** bullets respectively. A
bullet may legitimately say "none yet" when a system has no content-authoring or
player-facing surface at that point in the sequence — that absence is stated explicitly, never
silently omitted.

**Ordering logic:** the data-driven foundation and editor scaffold (M1–M2) unblock everything
else — no later system should be hand-authored in C++. World representation and dungeon
generation (M3–M4) precede entities (M5), since entities spawn into generated areas. Entities
and stats precede the turn/combat systems that consume them (M6–M7). Itemization and the Mag
(M8–M9) follow combat, since drop tables and feeding need combat/exploration to hook into. The
hub, difficulty tiers, and progression/permadeath (M10–M11) close the loop last, since they
integrate everything before them.

**Sibling reference:** `UnnamedRoguelike` (same Core/App template, one milestone ahead) already
has a working standalone `Editor/` project — a separate executable linking `Core`, composed of
per-content-type `Layer`s (`BiomeEditorLayer`, `DungeonEditorLayer`, `EntityEditorLayer`,
`FeatureEditorLayer`, `WorldMapEditorLayer`) on a shared RML field-widget library
(`FieldWidgets`, `RmlClickListener`, `ColorPickerPopup`, `TexturePickerPopup`). This roadmap
adopts that same pattern rather than inventing a new one. Its `DungeonEditorLayer` is *not* the
model for this project's dungeon generation, though — that sibling generates dungeons via a
biome-layer procgen pipeline, not hand-authored pieces. This project's dungeon generation
(M4) is deliberately different: a hand-authored library of room/corridor **pieces** (tile-grid
chunks with typed exit sockets on their borders) procedurally stitched together at
mission-generation time by matching compatible sockets — closer to a classic roguelike
vault/connector generator than a layered-noise pipeline. The closest existing sibling analog is
`FeatureEditorLayer`'s List/Edit/paint-grid shell (a palette-driven tile-grid canvas with a
per-cell inspector), which M4 adapts for painting full room/corridor pieces instead of small
entity-stamp footprints.

**Working through this roadmap:** before starting any bullet, Claude must first prompt the user
with which bullet is about to be worked on and let them give an upfront brief — scope,
constraints, specifics they want honored or avoided — before Claude writes an implementation
plan for it. Don't skip straight from picking a bullet to planning it.

## Fast path to a playable dungeon

The milestones above are grouped by system, but building them in that literal order delays
first playability more than necessary. This section resequences the *same* bullets — nothing
below is new scope, and nothing from M1–M11 is cut — into a **Phase A** critical path that
reaches a playable dungeon checkpoint as early as possible, and a **Phase B** that fills in the
rest of the roadmap afterward. Phase A targets one concrete, playable slice: **generate a
Forest-only dungeon, walk it on the turn scheduler, and fight Forest enemies with real
Hunter/Ranger weapons via basic melee/ranged resolution** — no Photon Arts/Techniques, no drop
tables/Section ID, no Mag, no hub. Only Forest needs to be authored end-to-end to hit this
checkpoint; the other three areas and the fixed unlock order are authored in Phase B once the
pipeline is proven on one area.

Three deviations from strict document order, each because the dependency is real, not stylistic:

- **M8.1 (item & equipment schema) moves ahead of the rest of M7/M8.** M7.1's own bullet already
  says weapon-type fields (range shape, ATP/ATA split) are "added to the item schema/editor
  (M8.1)" — melee/ranged resolution can't be authored or tested without weapons existing first.
- **A gameplay layer is inserted before M6/M7, not in the original numbered list.** Nothing
  before M10.1 (persistent hub) gives the player a way to actually enter a mission. Originally
  scoped as a throwaway launcher (CLAUDE.md's test-fixture exception) to be replaced outright by
  M10.1/M10.3 — **revised by the user's explicit choice**: `GameplayLayer` is the permanent
  gameplay entry point instead, starting minimal (spawn the player in a fixed test dungeon, no
  class-picker/hub yet) and grown in place. M10.1/M10.3 *extend* it (real mission-select/hub UI,
  real character creation) rather than replacing it — see its own write-up after M6 below.
- **`cereal` is split out of M1.1 and added in M11.2 instead.** `cereal` is for binary run-state
  persistence, whose only consumer is M11.2 (run persistence & permadeath); adding it 30 steps
  before it's used has no payoff. M1.1 in Phase A means rapidjson only.

### Phase A — playable dungeon (walk + fight, Forest only)

1. M1.1 — JSON integration, **rapidjson only** (see `cereal` note above)
2. M1.2 — Prefab / reflection registry
3. M1.3 — Content file layout & hot-reload
4. M2.1 — Editor executable
5. M2.2 — Reusable field-widget library
6. M3.1 — Grid & tile rendering
7. M3.2 — Area/biome data schema + area editor (author **Forest only**)
8. M4.1 — Piece data format & socket schema
9. M4.2 — Piece editor (author Forest room/corridor pieces)
10. M4.3 — Socket-matching stitcher
11. M4.4 — Generation preview & tuning tool
12. M5.1 — Core stat components (ATP/ATA/MST/DFP/EVP/LCK, race tagging)
13. M5.2 — Entity/enemy editor (author a handful of Forest enemies)
14. M8.1 — Item & equipment schema + item editor *(pulled forward — see above)*, author
    starter Hunter/Ranger weapons
15. M6.1 — Energy-based turn scheduler
16. M6.2 — Grid movement & input
17. M7.1 — Melee/ranged resolution (now unblocked by M8.1)
18. *(new)* `GameplayLayer` — the permanent gameplay entry point: generates a dungeon into a live
    `Grid`, spawns the player, wires turn-based input. Extended in place by M10.1/M10.3, not
    replaced

**Checkpoint:** playable dungeon reached. Force is melee-fallback-only until Phase B lands
Techniques (7.2) — matches the GDD's own fallback framing for Force without Techniques.

### Phase B — remaining depth (completes M1–M11, nothing cut)

19. M7.2 — Photon Arts (PP) & Techniques (TP) — makes Force fully playable
20. M7.3 — Status effects
21. M8.2 — Drop tables & Section ID
22. M8.3 — Photon crystals / stat materials
23. M5.3 — Rare enemies & tier reskins
24. M9.1 — Mag entity & feeding
25. M4.5 — Fixed area unlock order, plus authoring Caves/Mines/Ruins through the now-proven
    M3.2/M4.2/M5.2 editors (content authoring, not new engine work)
26. M10.1 — Persistent hub (extends the Phase A `GameplayLayer` with real mission-select/hub UI)
27. M10.2 — Difficulty tiers
28. M10.3 — Character creation (extends `GameplayLayer` with a real class-picker)
29. M11.1 — XP & leveling
30. M11.2 — Run persistence & permadeath (`cereal` added here)

---

## M1 — Data-Driven Foundation

**Status:** 1.1/1.2/1.3 done

- **1.1 JSON + serialization integration:** Engine: add `rapidjson` (config authoring) and
  `cereal` (binary run-state persistence) to `vcpkg.json`; a small `psr::Json` load/validate
  helper in `Core`. Editor/UI: none (infrastructure). **Done:** `rapidjson` added to
  `vcpkg.json` (Phase A scope — `cereal` stays deferred to M11.2, see the Phase-A note above);
  `psr::ReadJsonFile`/`WriteJsonFile` (`Core/Source/Engine/Persistence/JsonFile.h`) and
  `psr::LoadJsonDirectory` (`JsonDirectoryLoader.h`) for batch-loading a content directory,
  both ported from `UnnamedRoguelike`'s proven pattern; Catch2 coverage in
  `Core-Test/Source/JsonFileTests.cpp` / `JsonDirectoryLoaderTests.cpp`.
- **1.2 Prefab / reflection registry:** Engine: extend `Registry`
  (`Core/Source/Engine/ECS/Registry.h`) with a JSON-loaded prefab registry + `entt::meta`
  component reflection, per the fuller shape `ARCHITECTURE.md` already points at in
  UnnamedRoguelike's `Registry`. Editor/UI: none yet — this is what every later editor
  introspects against. **Done:** ported from `UnnamedRoguelike`'s proven pattern —
  `entt::meta` component reflection/schema (`ComponentSchemaRegistrar`, `ComponentMeta`,
  `TypeReflection`), a JSON-loaded prefab pipeline (`IEntityLoader`, `JsonEntityLoader`,
  `EntitySchemaEmitter`/`JsonSchemaBuilders` JSON-Schema validation), read-path introspection
  (`EntityDescriber`, `Registry::DescribeEntity`), and per-entity pub/sub
  (`EventHandlerComponent`, `Entity` handle, `Registry::BindComponentEvents`/`OnConstruct`/
  `OnDestroy`) — all under `Core/Source/Engine/ECS/`. Deliberately **not** ported: anything
  `cereal`-backed (chunk snapshot save/load, `ChunkLoadSession`) — `cereal` itself stays
  deferred to M11.2 per the Phase-A note above; `Registry::SetWorldContext`/`GetWorld` and
  `Entity::GetWorld` — no `World` type exists yet (M3); `FieldKind::TilePosition` and its
  schema/loader/describer branches — `TilePosition` is a `World`-specific type that doesn't
  exist yet either. All three slot in later alongside the milestones that actually need them.
  Catch2 coverage in `Core-Test/Source/RegistryTests.cpp` / `RegistryPrefabTests.cpp` /
  `EventHandlerComponentTests.cpp` / `JsonEntityLoaderTests.cpp` / `EntitySchemaEmitterTests.cpp`.
- **1.3 Content file layout & hot-reload:** Engine: `App/Assets/Data/<category>/*.json`
  convention + file-watch reload in dev builds. Editor/UI: none. **Done:** unlike M1.1/M1.2, no
  proven pattern existed to port here — `UnnamedRoguelike` turned out to have no actual
  hot-reload (content loads once at startup; editors only re-scan their own directory right
  after they save), so this was built new, per the user's explicit choice to diverge rather than
  match that narrower pattern. `psr::ContentWatcher`
  (`Core/Source/Engine/Persistence/ContentWatcher.h`) polls `std::filesystem::last_write_time`
  across a directory (recursive, `filename_suffix`-filtered like `LoadJsonDirectory`) and reports
  added/removed/modified files via `Poll()`; `Registry::RegisterPrefabs` was made idempotent
  (clears `prefab_registry` + the id map before repopulating) so a caller can safely re-run it on
  a detected change. No `App`-side wiring yet (no content schema needs the `Data/` directory
  before M3.2/M5.2) — this lands as reusable `Core` infrastructure + tests only, same as M1.1/M1.2.
  Catch2 coverage in `Core-Test/Source/ContentWatcherTests.cpp` and a new reload-replaces-not-leaks
  case in `RegistryPrefabTests.cpp`.

## M2 — Standalone Editor Scaffold

**Status:** 2.1/2.2 done

- **2.1 Editor executable:** Engine: none (consumes `Core` as-is). Editor: new `Editor/`
  project + `Build-Editor.lua` linking `Core`, `EditorMenuLayer` as the landing screen listing
  sub-editors (mirrors UnnamedRoguelike's `Editor/Source/main.cpp` + `EditorMenuLayer`). UI:
  `editor_menu.rml`/`.rcss` shell. **Done:** `Editor/Build-Editor.lua` (wired into the root
  `Build.lua` under a `Tools` group) mirrors `App/Build-App.lua`'s shape, with a two-stage
  postbuild asset copy (App's `Assets` first, then `Editor/Assets/RML` overlaid on top) so the
  editor reuses App's fonts without duplicating them. `Editor/Source/main.cpp` +
  `EditorFilepaths` mirror `App`'s entry point and `ApplicationFilepaths` pattern.
  `EditorMenuLayer` is a keyboard-only (Up/Down/Enter/Space) landing screen with a single "Exit"
  row for now — deliberately not pre-populated with placeholder rows for the sub-editors
  (Area/Piece/Entity/Item) that don't exist until M3.2/M4.2/M5.2/M8.1; each of those appends its
  own row + `TransitionTo<>()` case when its layer is built. Mouse-click wiring
  (`RmlClickListener`) is deferred to M2.2, per that milestone's own scope. Unlike the sibling
  project (which has no shared theme file — every `.rcss` hardcodes its own copy-pasted palette),
  this introduces `Editor/Assets/RML/theme.rcss`: a single cohesive dark palette (one cyan/blue
  accent instead of the sibling's competing gold/orange/blue) via shared classes (`.panel`,
  `.btn`, `.menu-row`, text-role helpers) that `editor_menu.rml` and future sub-editor documents
  `<link>` and build on, rather than repeating the sibling's per-file hardcoding.
- **2.2 Reusable field-widget library:** Editor: port/adapt `FieldWidgets`, `RmlClickListener`,
  `ColorPickerPopup`, `TexturePickerPopup` from UnnamedRoguelike's `Editor/Source/UI` as the
  shared base every later sub-editor builds on. UI: shared `.rcss` widget styles. **Done:**
  ported under `Editor/Source/UI/` — `RmlClickListener`/`RmlEventListener` (self-detaching
  `Rml::EventListener` adapters for "click" and arbitrary events), `FieldWidgets`
  (`BuildIntField`/`BuildFloatField`/`BuildStringField`/`BuildBoolField`/`BuildNameIdField`/
  `BuildVec2Field`/`BuildEnumField`/`BuildColorField`/`BuildTextureField`/`WireCollapseToggle`,
  each returning owner-held `Listeners` for the row's lifetime), `FieldPickers` (decouples
  color/texture fields from the popups that back their "Pick.../Choose..." buttons), and both
  popups (`ColorPickerPopup`: HSV square + hue strip built from discrete `background-color`
  strips rather than `linear-gradient` decorators, since this project's SDL `RenderInterface`
  has no `CompileShader` support, same constraint as the sibling; `TexturePickerPopup`: rescans
  `EditorFilepaths::TexturesPath` — new — on every `Open()`). Deliberately **not** ported:
  `BuildTilePositionField` — `TilePosition` is a `World`-specific type that doesn't exist yet
  (M3), same deferral M1.2 already made for the same reason elsewhere in the ECS; it slots in
  once M3 lands. Cleanup beyond a straight port, per the user's brief: the sibling toolkit's
  field-widget/popup-chrome CSS was hand-duplicated per editor screen with its own gold/orange
  palette; this port instead centralizes those rules into `Editor/Assets/RML/field_widgets.rcss`
  plus `color_picker.rcss`/`texture_picker.rcss`, all `<link>`ing `theme.rcss` and building on
  its existing cyan accent and `.btn`/`.panel` classes rather than redeclaring a second palette.
  Verified with a throwaway smoke-test layer (per this file's own verification guidance) that
  exercised every builder plus both popups end-to-end in a live build — removed once confirmed
  working, not kept. Catch2 coverage doesn't apply here (RmlUi widget code needs a live
  `Rml::Context`, not practically unit-testable), matching this file's own note that Editor
  UI work is verified manually.

## M3 — Tile/Grid World Representation

**Status:** 3.1 done

- **3.1 Grid & tile rendering:** Engine: `TileMap`/`Grid` component, camera, SDL-renderer tile
  blitting, ECS `Position`/`Transform`. Editor/UI: none yet (no content format to edit until
  3.2). **Done:** ported from `UnnamedRoguelike`'s proven pattern, stripped of everything
  specific to its chunked/streamed/multi-height `World` (`Chunk`, `ChunkPosition`,
  `LocalTilePosition`, and the height-band tint/shade/scale/parallax system all dropped
  entirely) — this project's world is a single fixed-size, non-chunked grid per dungeon area.
  `Application::Initialize` now creates a GPU-backed `SDL_Renderer` (`SDL_CreateGPUDevice` +
  `SDL_CreateGPURenderer`, Vulkan/SPIR-V) instead of a plain one, per the user's explicit choice
  to port the sibling's full custom SDL_GPU tile pipeline rather than fall back to plain
  `SDL_Renderer` blits. New in `Core/Source/Engine/Render/`: `Camera`/`Viewport`/
  `TileVertexMath` (ported onto `psr::Vec2`, no `TilePosition` split needed without chunking;
  `parallax_factor` dropped, height-only), `GpuResource`/`ShaderCompiler`/`TileVertex`/
  `TileGpuPipeline` (the SPIR-V pipeline + two-tone palette-swap shading, ported verbatim —
  none of it was chunk-coupled in the source), `TextureAtlasMath`/`TextureAtlasPacker`/
  `TextureAtlas` (recursive `.png` packer keyed by filename-stem hash, ported verbatim), and
  `RenderableTile`/`IRenderableLookup`/`TileRenderer` (the one reworked file: iterates a
  `Grid`'s cells intersected with the viewport via `Rect::Intersect` instead of chunk lookups;
  `RenderableTile` drops `texture_id_string`/`Describe()`, both JSON-round-trip concerns
  deferred to M3.2). New `Core/Source/Engine/World/Grid.h` (fixed `width x height` flat
  `entt::entity` array, `entt::null` for empty cells — not an ECS component, a plain owned
  class) and `Core/Source/Engine/Math/Vec2f.h` (float mirror of `Vec2`, no `Lerp()` — unused
  until M6 tweening). New `Core/Source/Engine/ECS/Position.h` (wraps `Vec2 tile`,
  `entt::meta`-reflectable via the same `Register(ComponentSchemaRegistrar&)` pattern as
  `PrefabIdComponent`) — deliberately **not** paired with a `Transform` component:
  `IRenderableLookup::GetRenderOffset` is the existing seam for a future sub-tile offset (an
  M6 tween concern), so a second component with nothing to write into it would be speculative.
  App-side (`App/Source/`): `Components/RenderableComponent.h` (App's mirror of `RenderableTile`,
  reflectable), `Render/RegistryRenderableLookup` (the `IRenderableLookup` impl over `Registry`,
  simplified vs. the sibling — no fog-of-war/dimming, since no visibility system exists here
  yet), and `Components/RegisterComponents.{h,cpp}` — the first real instance of the
  `entt::meta` registration aggregator `Registry.h`'s and `Build-App.lua`'s comments had long
  anticipated but that never existed until now; it also finally registers `PrefabIdComponent`,
  which had been sitting unregistered since M1.2. `vcpkg.json` gained `sdl3[vulkan]` and a
  host-only `glslang[tools]` (the offline GLSL→SPIR-V compiler); new
  `Scripts/Compile-Shaders.ps1` plus `App/Assets/Shaders/TileSprite.{vert,frag}.glsl` (+
  committed `.spv`). Verified with a throwaway 40x30 checkerboard smoke-test layer (per this
  file's own verification guidance, matching the pattern M1.1/M2.2 already used) that exercised
  the full pipeline end-to-end in a live build (screenshot-confirmed rendering, camera
  pan/zoom logic covered separately by unit tests) — removed once confirmed working, not kept.
  Catch2 coverage in `Core-Test/Source/CameraTests.cpp`/`ViewportTests.cpp`/
  `TileVertexMathTests.cpp`; the GPU pipeline/atlas/`TileRenderer` itself aren't practically
  unit-testable (need a live GPU-backed `SDL_Renderer`), same reasoning this file already gives
  for Editor UI being manually verified instead.
- **3.2 Area/biome data schema:** Engine: JSON schema for area theme (tile palette, race,
  hazard type) — feeds dungeon generation in M4. Editor: **Area editor layer** — tile palette
  assignment, race/hazard config, live preview (mirrors `BiomeEditorLayer` + its texture/color
  pickers). UI: none (content authoring only, not player-facing).

## M4 — Dungeon Piece Library & Generation

**Status:** 4.1/4.2/4.3/4.4 done, 4.5 not started

- **4.1 Piece data format & socket schema:** Engine: `DungeonPiece` schema — a sparse set of
  cells (arbitrary/non-rectangular footprint; membership, not a fixed W×H array, defines the
  shape), each cell a stack of stamped entity **prefabs** (mirrors `FeatureCell::prefabs` in
  UnnamedRoguelike), plus metadata (room category — corridor/room/vault/boss arena/entrance/
  exit — area tag). Editor/UI: none yet (needs 4.2's authoring UI). **Done:** pieces are
  composed of entities, not embedded tile/render data, per the user's explicit brief — a
  cell's visual comes entirely from its stamped prefabs' own `RenderableComponent`. A "socket"
  is just a stamped prefab carrying the new `SocketComponent` (`Core/Source/Engine/ECS/
  SocketComponent.h`: `tags`, `fallback_prefab_id`) — its border edge is a per-placement
  override (`PieceCellPrefab::edge`, `Core/Source/Engine/Dungeon/DungeonPiece.h`), not stored on
  the prefab, since the same socket prefab can face any direction depending on where it's
  stamped. Follows the `Biome`/`Feature` bespoke-schema pattern (not the ECS
  `ComponentSchemaRegistrar` pipeline, since `DungeonPiece` is a content asset, not a
  component): `PieceSchema`/`PieceSchemaEmitter`/`PieceLibraryFile`/`PieceLibrary`
  (`Core/Source/Engine/Dungeon/`), content at `App/Assets/Data/Pieces/*.json`. No `_string`
  companion fields for NameId references (`prefab_id`, piece/dungeon refs) — resolved via the
  existing `NameIdRegistry` instead, this project's own (better) convention vs. the sibling's
  per-struct retention. Fixed a pre-existing gap in the process: the Editor previously ran off a
  postbuild-copied `Assets/` folder, so any editor-saved content would have lived only in the
  gitignored `Binaries/` output; `EditorFilepaths::DataPath` now points at App's *source*
  `Assets/Data` via a `PSR_APP_ASSETS_DIR` compile-time define (mirrors UnnamedRoguelike's own
  `RL_APP_ASSETS_DIR` fix), and the Editor now compiles `App/Source/Components/` and
  `App/Source/Render/` directly (App is a `ConsoleApp`, can't be linked) so content editors can
  enumerate/preview real entity prefabs. Catch2 coverage in
  `Core-Test/Source/DungeonPieceSchemaTests.cpp`. Follow-up fix (alongside 4.4's Prefab Editor
  work below): `Grid` (M3.1) had never actually caught up to "each cell a stack of stamped entity
  prefabs" above — it only ever held one `entt::entity` per cell. `Grid` (`Core/Source/Engine/
  World/Grid.h`) now stores `std::vector<std::vector<entt::entity>>`, preserving stamp/insertion
  order per cell; `TileRenderer.cpp` was updated to iterate `GetEntities` (switched `sort` to
  `stable_sort` so same-layer stamps keep their authored order). Catch2 coverage in
  `Core-Test/Source/GridTests.cpp`.
- **4.2 Piece editor:** Engine: none new. Editor: **Piece editor layer**. **Done:**
  `Editor/Source/Layers/PieceEditorLayer` — List/Edit shell consistent with the other content
  editors (mirrors `FeatureEditorLayer`'s shell almost directly, closer than expected once
  cells became pure entity-stamp lists like `FeatureCell`): a single palette-driven paint tool
  (no separate tile/socket tools — tiles are prefabs too) stamps the selected prefab into cells,
  which is what defines the footprint shape (irregular/non-rectangular falls out for free); a
  socket-carrying brush auto-computes its default border edge from which neighbour cell is
  unpainted at paint time, editable after in the per-cell inspector per the "few constraints"
  brief. UI: none (authoring only).
- **4.3 Socket-matching stitcher:** Engine: generator that places pieces from the library and
  connects them by matching compatible open sockets edge-to-edge, filtered by the piece's area
  tag. Editor/UI: none new — this is what 4.4 previews. **Done:** `Dungeon` schema
  (`Core/Source/Engine/Dungeon/Dungeon.h` — a **saved content asset**, not just generation
  params: which pieces are eligible plus a **per-dungeon weight/max-occurrence per piece
  reference** since the same piece can be reused across dungeons differently, a room-count
  range, a loopback-count range, and a list of lock-and-key configs) and
  `DungeonStitcher::GenerateDungeon` (`Core/Source/Engine/Dungeon/DungeonStitcher.{h,cpp}`):
  Phase 1 grows a connected tree from a single Entrance to a single Exit (connectivity
  guaranteed by construction, not a separate check); Phase 2 adds loopback connections between
  still-open, geometrically-adjacent, tag-compatible sockets for multiple paths; Phase 3
  resolves every socket left unconnected as a dead end (swapped for its `fallback_prefab_id` or
  left open); Phase 4 places `dungeon.locks` as solvable lock/key gates on **bridge** edges of
  the entrance-to-exit path only (a loopback edge can never gate anything, since an alternate
  path already exists), processed outward from Entrance so a later key is never blocked by an
  earlier lock, each verified solvable via BFS before being recorded. Deliberately an
  **abstract, verified-solvable annotation** on the layout, not a spawned in-world lock/key
  entity — items (M8) and interaction (M6/M7) don't exist yet for that wiring. A `SocketLookup`
  callback boundary keeps the stitcher itself free of any ECS/Registry dependency, so it's
  fully unit-testable against synthetic fixture pieces with no live engine state. Catch2
  coverage in `Core-Test/Source/DungeonSchemaTests.cpp` /
  `Core-Test/Source/DungeonStitcherTests.cpp` (connectivity, no cell/socket overlap,
  `max_occurrences`/`weight` respected, room/loopback ranges honoured, every lock's key
  reachable without crossing it or an earlier lock, seed-reproducible).
- **4.4 Generation preview & tuning:** Editor: **generation preview tool** — seed/param tuning
  with live regenerate-and-preview of a full stitched layout. **Done:** folded into a
  **Dungeon editor** (`Editor/Source/Layers/DungeonEditorLayer`) rather than a separate layer,
  per the user's brief — the described params (piece-pool filter, target room count, loop
  tolerance) are exactly the `Dungeon` schema's own fields, so the Dungeon definition's List/
  Edit screen (repeatable piece-ref rows with weight/max-occurrence, room/loopback-range
  fields, repeatable lock-config rows) embeds a "Generate"/"Reroll seed" toolbar and live
  preview canvas directly. The preview computes the generated layout's world-cell bounding box
  (unlike `PieceEditorLayer`'s fixed edit canvas — a whole dungeon isn't bounded the same way a
  single piece is) and renders every placed piece's stamped prefabs, plus a debug overlay
  (locked connections outlined, key rooms tinted, dead-end sockets tinted) so the loopback/
  dead-end/lock-key structure is inspectable while tuning. Shares a name with (but is unrelated
  to) UnnamedRoguelike's own `DungeonEditorLayer`, which serves its different layered-noise
  biome pipeline. UI: none yet (mission-entry UI lands in M10). Along the way, fixed a bug
  affecting **both** M4.2/M4.4: `theme.rcss`'s opaque `body` background was painting over every
  SDL/GPU-drawn grid line and sprite, since `Application::Run` renders RmlUi's document *after*
  each `Layer::OnRender` — `content_editor.rcss` (the new shared List/Edit shell stylesheet
  both editors `<link>`, factored out to avoid the per-screen copy-paste UnnamedRoguelike's own
  editor layers each keep) now makes `body` transparent. Verified live in the running Editor
  with throwaway fixture pieces/entities (per this file's own verification precedent) — removed
  once confirmed working, not kept. Follow-up: the Dungeon/Piece editors' preview panes were
  refactored out into shared, content-agnostic chrome — `PreviewCanvas`
  (`Editor/Source/UI/PreviewCanvas.{h,cpp}`: pan/zoom/auto-fit camera over arbitrary world bounds)
  and `PreviewWindowChrome` (`Editor/Source/UI/PreviewWindowChrome.{h,cpp}`: the bordered/resizable
  `#preview-window` DOM + zoom toolbar) — both now shared with the new Prefab Editor (see M5
  status note below) rather than each editor hand-rolling its own grid-panel layout math.
  `ComponentSchema` also gained an `authorable` bool (`Core/Source/Engine/ECS/ComponentSchema.h`)
  gating whether a component may appear in entity JSON / an editor's "add component" picker at
  all — `Position` and `PrefabIdComponent` are registered `authorable=false` (engine-derived-only,
  never hand-authored), threaded through `EntitySchemaEmitter`'s JSON-schema emission so authoring
  either's key is now a hard validation failure.
- **4.5 Fixed area unlock order:** Engine: Forest→Caves→Mines→Ruins gating hook, consumed by
  the hub in M10. Editor: ordering field on the M3.2 area editor. UI: none yet.

## M5 — Entity & Stat Framework

**Status:** 5.1/5.2 done. A generic **Prefab Editor** already exists ahead of schedule
(`Editor/Source/Layers/PrefabEditorLayer`) — browse/create/edit/delete the entity-prefab JSON
files under `App/Assets/Data/Entities/`, rendering one Inspector-style card per currently-registered
*authorable* component (see the M4.4 follow-up note above for the `authorable` flag and shared
`PreviewCanvas`/`PreviewWindowChrome` it builds on). 5.1's stat/race sections below were added as
additional hand-wired cards on this same layer, per its own class doc comment, not a rewrite.

- **5.1 Core stat components:** Engine: ATP/ATA/MST/DFP/EVP/LCK components, four-race tagging
  (Native/A.Beast/Machine/Dark) on enemy prefabs. Editor: wired into the Prefab Editor now (pulled
  forward from 5.2, per the user's brief) rather than deferred to a dedicated entity/enemy editor
  pass. **Done:** `StatsComponent` (`Core/Source/Engine/ECS/StatsComponent.h`) is a single struct
  of six `int` fields (`atp`/`ata`/`mst`/`dfp`/`evp`/`lck`, all defaulting to `0` — no balance
  numbers are authored here per CLAUDE.md's division of labor). `RaceComponent`
  (`Core/Source/Engine/ECS/RaceComponent.h`) holds a single `race_id` field — deliberately a
  `NameId` (a string hashed via `entt::hashed_string`, resolved through `NameIdRegistry`), not a
  compile-time `enum class`, per the user's explicit brief: new races can be added or removed
  purely as authored data, no engine recompile, the same convention already used for
  `texture_id`/`fallback_prefab_id`. Both register through the normal
  `ComponentSchemaRegistrar` pipeline (authorable, `FieldKind::Integer` ×6 and `FieldKind::NameId`
  respectively) with zero new schema plumbing — the registrar's existing `is_integral_v`/
  `is_same_v<..., std::uint32_t>` branches already cover both shapes. Registered alongside the
  other App components in `App/Source/Components/RegisterComponents.cpp`. `PrefabEditorLayer`
  gained "Stats" and "Race" Inspector cards (`kComponentKinds`, `ReadStatsBody`/`WriteStatsBody`,
  `RefreshEditForm`'s field wiring) using the existing `BuildIntField`/`BuildNameIdField` widgets —
  no new widget types. Verified live in the running Editor (added both cards to the `test` prefab,
  edited values, saved, confirmed the JSON round-trip, e.g. `"race": { "race_id": "native" }`) —
  the test prefab itself was reverted afterward, per this file's own throwaway-fixture convention.
  Catch2 coverage in `Core-Test/Source/StatsRaceComponentTests.cpp` (schema shape/authorable, plus
  a `JsonEntityLoader` round-trip asserting `race_id` hashes and registers its label correctly).
- **5.2 Entity/enemy editor:** Engine: enemy prefab schema (stats, race, sprite ref, spawn
  weight). Editor: **Entity editor layer** — stat field forms, race picker, sprite picker
  (mirrors `EntityEditorLayer` + `EntityFieldForms`). UI: none (authoring only). **Done:** no new
  code — per the user's explicit brief, stayed folded into the existing Prefab Editor rather than
  building a separate `EntityEditorLayer`/`EntityFieldForms` (same call as 5.1), and that layer
  already had every field this bullet asks for before 5.2 was even reached: stat field forms +
  race picker landed with 5.1, and the renderable card's texture picker/UV/color fields (sprite
  ref) predate 5.1 as part of the Prefab Editor's original base. **Spawn weight was deliberately
  deferred**, per the user's explicit brief — no spawn/population system exists yet to consume it
  (M4's dungeon generation only hand-stamps specific prefabs into piece cells; there's no
  random-encounter mechanic to weight against), so adding the field now would mean guessing at a
  shape for data nothing reads. Revisit alongside whatever milestone actually introduces enemy
  population/spawning. Content authoring (the actual Forest enemies) is the user's own work
  through this editor, per `CLAUDE.md`'s division of labor — not done by Claude.
- **5.3 Rare enemies & tier reskins:** Engine: rare-variant flag (alt palette + stat multiplier
  + guaranteed drop hook), tier-based roster substitution (e.g. Garanz→Baranz on Ultimate, not
  just stat scaling). Editor: rare-variant toggle + tier-substitution mapping on the entity
  editor. UI: none yet — rare/boss visual callouts land with combat UI in M7.

## M6 — Turn-Based Scheduler & Movement

**Status:** 6.1/6.2 done (engine/systems only — see the UI deferral note below)

- **6.1 Energy-based turn scheduler:** Engine: turn queue keyed on energy cost, variable
  per-action costs (exact numbers explicitly deferred per GDD). Editor: none (numeric
  balancing pass, not structural). UI: turn-order indicator. **Done:** ported from
  `UnnamedRoguelike`'s proven pattern — `TurnQueue` (`Core/Source/Engine/Turns/TurnQueue.h/.cpp`)
  is a dependency-light energy scheduler (generic over `entt::entity`, no Registry/World
  coupling) keyed on an absolute "time to next reach `action_threshold`" per actor, cached for
  O(log n) `NextActor`/`Enqueue`/`Remove`/`Requeue`, ties broken by insertion order. Alongside it,
  a generic Action framework ported the same way: `IAction`/`ActionResult`/`ActionExecutor`
  (`Core/Source/Engine/Actions/`) — `ResolveAction` follows an `ActionResult::fallback` chain,
  applying only the final cost (the seam M7 combat's bump-to-attack will reuse). App-side,
  `TurnCoordinator` (`App/Source/Systems/TurnCoordinator.{h,cpp}`) drives the loop: `EnergyComponent`
  (`App/Source/Components/EnergyComponent.h`, `authorable=false`) is the persisted scheduling
  energy, with `TurnQueue` membership following its construct/destroy lifecycle via
  `Registry::OnConstruct`/`OnDestroy`. Deliberately simplified vs. the source: no AI controller
  seam is wired to real AI yet (`TurnCoordinator::SetNpcDecision` is the documented hook a future
  AI session plugs into — every non-player actor Waits by default this round) and no
  `QueuedActionsComponent` mid-turn draining exists yet (nothing in this round's scope produces a
  multi-step fallback chain). Catch2 coverage in `Core-Test/Source/TurnQueueTests.cpp` /
  `ActionExecutorTests.cpp`; App-side coverage (new `App-Test` project, see below) in
  `TurnCoordinatorTests.cpp`.
- **6.2 Grid movement & input:** Engine: tile-to-tile movement resolution, keyboard/mouse turn
  command mapping. Editor: none. UI: movement/action cursor, valid-move tile highlight — **deferred
  this round, per the user's explicit choice** (no turn-order indicator or move-cursor/highlight
  yet; revisit once a real mission/dungeon flow exists to display them in). **Done:** `MoveAction`/
  `WaitAction` (`App/Source/Actions/`) implement `IAction` against this project's flat, non-chunked
  `Grid` (no z-step/chunk-boundary/fall-through-pit complexity from the source, since none of that
  applies here) — an out-of-bounds or `BlocksMovementComponent`-blocked target tile is a free
  no-op (no bump-to-attack fallback yet, since M7 combat doesn't exist). Costs are flat
  (`kMoveCost`/`kWaitCost` = 100 = the default action threshold, no per-entity speed multiplier) per
  the GDD's explicit deferral of exact per-action costs to a future balancing pass. Unlike the
  source (which only tweens attacks), **movement itself animates** per the user's explicit choice:
  `Position` still snaps instantly (so turn logic never waits on animation), but `MoveAction`
  emplaces a `TweenComponent` (`App/Source/Components/TweenComponent.h`, deliberately **not**
  meta-registered — transient render-only state) that `TweenSystem`
  (`App/Source/Systems/TweenSystem.{h,cpp}`) eases toward `{0,0}` every frame, consumed by
  `RegistryRenderableLookup::GetRenderOffset` (the seam M3.1 had already reserved for this).
  Input: `ActionMap<TKey>`/`InputBuffer<TKey>` (`Core/Source/Engine/Input/`, DAS-style held-key
  repeat) are generic, engine-agnostic ports; `App/Source/Content/KeyBindings.{h,cpp}` binds the
  default 4-directional arrow keys + Space-to-wait (diagonals deferred — nothing in the GDD commits
  to 8-directional movement). No gameplay `Layer` wired any of this to the live SDL event loop
  this round, per the user's explicit choice and the roadmap's own Phase-A sequencing (`GameplayLayer`,
  which does that wiring, comes after M7.1 — see its own write-up below). Verified via unit tests
  only this round (no throwaway smoke-test layer, per the user's explicit choice) — a new **`App-Test`** project
  (`App-Test/Build-App-Test.lua`, registered in `Build.lua`) was added since none existed before,
  mirroring `Core-Test`'s Catch2 setup and reusing the same "compile App's pure-logic subfolders
  directly" trick `Editor/Build-Editor.lua` already established (App is a `ConsoleApp`, can't be
  linked). Catch2 coverage in `App-Test/Source/MoveActionTests.cpp` / `WaitActionTests.cpp` /
  `TweenSystemTests.cpp`; `Core-Test/Source/ActionMapTests.cpp` / `InputBufferTests.cpp` /
  `EasingTests.cpp` cover the new Core-level primitives.

## Gameplay Layer (Phase A item 18)

**Status:** initial landing done (player spawn + movement in a fixed test dungeon)

The permanent gameplay entry point — see the Phase-A deviation note above for why this isn't a
throwaway launcher. **Done:** two new pieces close the loop from "generated dungeon layout" to
"player moving around on screen with wall collision," neither of which existed before:
`Core/Source/Engine/Dungeon/DungeonInstantiator.h/.cpp` (`ComputeDungeonBounds`/
`InstantiateDungeon`) bridges a `DungeonStitcher`-produced `DungeonLayout` into a live `Grid` of
entities — stamping every placed piece's cells via `Registry::CreateEntity(prefab_id)`, swapping a
dead-end socket's own prefab for its `fallback_prefab_id` (mirrors the Dungeon Editor preview's own
dead-end rendering), and translating the layout's possibly-negative world coordinates into the
`Grid`'s zero-based space. `App/Source/Layers/GameplayLayer.h/.cpp` is the first real consumer of
`TileRenderer`/`Camera`/`TextureAtlas`/`TileGpuPipeline` together (M3.1 built them, nothing used
them as a set until now): on attach it loads content, generates a dungeon (currently a hardcoded
`test_dungeon` id — no mission-select exists yet, see M10.1 above), instantiates it, spawns the
player at the entrance from a new `App/Assets/Data/Entities/player.json` prefab (appearance lives
in data like every other entity, not hand-built in the layer — no character creation yet, see
M10.3 above, to pick anything other than this one default), and constructs `TurnCoordinator`
*before* the player's `EnergyComponent` is emplaced (queue membership is driven by that
construction order). `OnEvent` finally wires `TurnCoordinator::PressKey`/`ReleaseKey` to the live
SDL event loop, the connection M6.2 left dangling. Also fixed in passing:
`App/Assets/Data/Entities/basic_socket.json` had an empty `SocketComponent.tags` list, so
`DungeonStitcher`'s tag-intersection rule could never match any two sockets and generation always
failed past the Entrance — given a real tag (`["door"]`); and a new throwaway
`App/Assets/Data/Entities/wall.json` (`blocks_movement` + `geo_wall`) is available for wall cells
to be authored into pieces via the Piece Editor (not yet placed into any piece — nothing currently
blocks movement in the test dungeon). Not yet done: mission-select (hardcoded dungeon id),
character creation (no `EquipmentComponent`/stats populated on spawn), any HUD, enemy spawning.

Brought up to UnnamedRoguelike's `WorldLayer` quality bar (the sibling project's reference
implementation, per the user's explicit direction) after the initial landing: `OnAttach()`
originally wrapped every content-load/generation step in its own `try/catch` that logged and
silently returned, leaving a black screen on failure — `WorldLayer` deliberately does none of this
(a missing/malformed content file is a build-input bug, not a runtime condition a player can hit,
so it's allowed to crash loudly with a real exception message instead of being hidden). Matched
that convention: those `try/catch` blocks are gone, `GenerateDungeon`/`LoadPieceLibrary`/
`LoadDungeonLibrary`/`JsonEntityLoader::Load` now throw straight through `OnAttach()`, and the two
remaining non-exception failure checks (no `test_dungeon` definition found, generated dungeon has
no cells) were converted from log-and-return to `throw std::runtime_error`. `App/Source/main.cpp`
gets the one addition `WorldLayer`'s own reference doesn't even have: a top-level `try/catch`
around `PushLayer`/`Run()` that logs the exception and exits(1) cleanly instead of an OS crash
dialog — verified by deliberately hiding `test_dungeon.json` from the build output and confirming
`App.exe` logs `"GameplayLayer: no 'test_dungeon' dungeon definition found"` and exits 1, then
restoring it and confirming normal startup again.

Catch2 coverage in `Core-Test/Source/DungeonInstantiatorTests.cpp`; `GameplayLayer` itself is
GPU-rendering/live-input code in the same category M3.1 already documented as impractical to
unit-test — verified instead by running `App.exe` directly and confirming on screen (dungeon
renders, player sprite appears at the entrance, arrow keys move it turn-by-turn with the camera
tracking) plus the deliberate-failure check above.

## M7 — Combat System

**Status:** 7.1 done

- **7.1 Melee/ranged resolution:** Engine: Hunter melee (adjacent/cone/line shapes, ATP-vs-ATA
  tradeoff), Ranger ranged (range/spread/hits-per-turn), four-race damage bonus from 5.1.
  Editor: weapon-type fields (range shape, ATP/ATA split) added to the item schema/editor
  (M8.1). UI: HP/action bars, target/range-preview overlay, combat log. **Done:** a new
  `HealthComponent` (`Core/Source/Engine/ECS/HealthComponent.h` -- `current_hp`/`max_hp`, same
  shape as `StatsComponent`) fills the gap M8.1 left open: nothing could be damaged or killed
  before this, since no HP concept existed anywhere. `Core/Source/Engine/Combat/CombatMath.h/
  .cpp` holds the pure formula: `ComputeHitChance` (ATA-vs-EVP ratio, clamped to [0.05, 0.95] so
  a hit is never guaranteed or impossible), `ComputeDamage` (ATP minus half DFP, a small
  \[0.9, 1.1\] random variance band, floored at 1), and `ApplyRaceBonus` (the 5.1 four-race %
  bonus). Per the user's explicit brief, this is shaped after PSO's known ATA/EVP and ATP/DFP
  mechanics, not a claimed bit-exact reproduction of PSO's original (undocumented) constants --
  real per-entity numbers stay authored `StatsComponent` data, same deferral every other
  milestone here already makes; `grind_level`'s stat contribution is left unconsumed for the
  same reason (no single documented universal per-grind formula to port faithfully).
  `App/Source/Combat/EffectiveStats.h/.cpp` (`ComputeEffectiveStats`) sums an actor's base
  `StatsComponent` with its equipped weapon/armor entities' `StatsComponent` bonus (read via
  `EquipmentComponent`) plus its weapon's prefix/suffix affix flat bonuses -- finally giving
  M8.1's Affix library a real consumer. `App/Source/Combat/Hostility.h` (`IsHostile`) is a
  placeholder player-vs-everyone-else rule, mirroring `TurnCoordinator`'s own existing
  simplification, until a real faction system exists. `App/Source/Actions/AttackAction.h/.cpp`
  is the new `IAction`: resolves target tiles from the wielded weapon's `WeaponRangeShape`
  (`SingleTarget` the one adjacent tile; `Line` pierces every hostile target up to `range`,
  stopping only at a wall -- a `BlocksMovementComponent` occupant with no `HealthComponent`;
  `Cone3` the forward tile plus its two perpendicular neighbours; `Surrounding` all four
  cardinal-adjacent tiles), rolls `hits_per_turn` hit/damage checks per hostile occupant found,
  and destroys (`Registry::DestroyEntity`) anything reduced to 0 HP -- which automatically drops
  it from `TurnQueue` via `TurnCoordinator`'s existing `OnDestroy<EnergyComponent>` listener, no
  new death-handling wiring needed. `MoveAction` (M6.2) is extended, not replaced: bumping into a
  hostile `HealthComponent`-carrying occupant now returns an `AttackAction` via
  `ActionResult::fallback` instead of a bare no-op -- the exact seam M6.1's `ResolveAction`
  reserved for this; a non-attackable/non-hostile blocker still no-ops as before. `MoveAction`'s
  (and `CreateDefaultKeyBindings`'s) constructor grew an `AffixLibrary&`/`std::mt19937&` pair to
  thread through to that fallback. Editor: a "Health" Inspector card was added to
  `PrefabEditorLayer` (mirrors the existing "Stats" card, two `BuildIntField`s), per CLAUDE.md's
  "every feature needs a UI/editor answer" -- HP is authored content like any other stat. UI:
  HP/action bars, target/range-preview overlay, and combat log are **deliberately deferred this
  round**, per the user's explicit choice matching M6.2's own precedent -- no gameplay `Layer`
  existed yet to host them in; revisit now that `GameplayLayer` (below) exists. Likewise
  out of scope: PP/TP costs and Photon Arts/Techniques (7.2), status effects (7.3), and
  `GameplayLayer` itself (a separate Phase A item) -- nothing here wires `AttackAction`/
  `MoveAction` into a live input loop or spawns real entities, so verification stayed unit-test-
  only (no throwaway smoke-test layer), same reasoning M6.2 already gives for logic that's fully
  unit-testable without a live gameplay layer. Catch2 coverage in
  `Core-Test/Source/HealthComponentTests.cpp` / `CombatMathTests.cpp` and
  `App-Test/Source/AttackActionTests.cpp` (weapon-less/target-less/non-hostile no-ops, lethal
  resolution and destroy, `hits_per_turn` multiplicity, race-bonus application) plus new
  bump-to-attack cases in `App-Test/Source/MoveActionTests.cpp`.
- **7.2 Photon Arts (PP) & Techniques (TP):** Engine: separate PP (Hunter/Ranger) and TP
  (Force) pools; Photon Art as a chosen PP-cost attack option (not a hidden proc, per GDD's
  turn-based adaptation); Technique spell system with elemental damage + status, tiered by use.
  Editor: **Photon Art / Technique editor** — cost, effect family, tier-scaling fields. UI:
  PP/TP bars, Photon Art/Technique selection menu, status icons.
- **7.3 Status effects:** Engine: Freeze/Poison/Shock/Confuse framework (duration, tick, cure).
  Editor: status-effect fields on the 7.2 editor. UI: status icon + duration on HUD and over
  affected entities.

## M8 — Itemization & Economy

**Status:** 8.1/8.2 done (8.1 pulled forward, see the Phase-A note above)

- **8.1 Item & equipment schema:** Engine: weapon/armor/material components + inventory/equip
  slots. Editor: **Item editor layer** — weapon stats, race-bonus %, equip-slot config. UI:
  inventory grid, equipment slot panel. **Done:** weapons/armor/mods are entity prefabs, per the
  user's explicit brief — composed of components under `Core/Source/Engine/ECS/`, not embedded
  value structs. `WeaponComponent` (`range_shape`: `SingleTarget`/`Cone3`/`Surrounding`/`Line`,
  reconciling the brief's four range cases with the GDD's own melee "single adjacent tile/cone"
  and ranged "range/spread/line/hits-per-turn" vocabulary; `range`, `hits_per_turn`,
  `grind_level` for the monogrinder effect; `prefix_affix_id`/`suffix_affix_id` NameId refs;
  `race_bonuses` — a `std::vector<{race_id, bonus_percent}>` rather than one fixed-race field,
  per the user's explicit choice, matching `RaceComponent`'s own "no fixed enum, purely
  data-driven" design intent). `ArmorComponent` (`slot`: `Head`/`Torso`/`Hands`/`Legs`;
  `mod_slot_count`, 0-4, a flat cap for every armor piece per the user's explicit brief —
  deliberately more generous than PSO's variable 0-4). `ModComponent` (empty tag, same shape as
  `BlocksMovementComponent`). `RarityComponent` (`stars`, a single shared component reused
  verbatim on weapon/armor/mod prefabs, per the brief's "all three need it" — one component, not
  three). All four reuse `StatsComponent` (reattached to mean "stat bonus granted when equipped,"
  same struct, different role) for their stat contribution. New
  `Core/Source/Engine/Items/` directory: a bespoke, non-ECS **Affix library**
  (`Affix`/`AffixSchema`/`AffixSchemaEmitter`/`AffixLibrary`/`AffixLibraryFile`, content at
  `App/Assets/Data/Affixes/*.json`) for the prefix/suffix definitions weapons reference by
  NameId — follows the `DungeonPiece`/`Dungeon` file-family pattern (a reusable named content
  type, not an entity) rather than the `ComponentSchemaRegistrar` pipeline; its effect payload
  (one `AffixStat` + a flat `amount`) is deliberately minimal, matching `DungeonLockConfig`'s
  "just needs to round-trip as data for now" precedent, since nothing consumes an affix's effect
  yet. Per the user's explicit brief, **affixes are weapon-only this round** — armor doesn't get
  prefix/suffix fields (trivial to add later with the identical mechanism if wanted). New
  `App/Source/Components/EquipmentComponent.h` (weapon + Head/Torso/Hands/Legs, each a live
  `entt::entity`) gives M7.1 something to read "what's equipped" from — deliberately **not**
  meta-registered (`entt::entity` has no `FieldKind` mapping) and never authored in a prefab
  JSON, following `TweenComponent`'s existing precedent for engine-internal runtime-only state
  rather than `PrefabIdComponent`'s `authorable=false`-but-still-registered pattern; it ships
  unpopulated this round (nothing yet sets it — populating it for the player is `GameplayLayer`'s
  job, not yet done as of its own initial landing below). Authored entity-prefab JSON stays a **template**: fields
  like `grind_level`/`race_bonuses`/affix refs are base/default values, not rolled instances —
  the actual random-roll-at-drop logic is M8.2's job (drop tables, not yet started), and the
  monogrinder's consumable-use flow and mod-plugged-into-armor-slot runtime state are likewise
  deferred for lack of any consumer yet (no item-use system, no mod-effect system, no inventory
  UI) — only the static `mod_slot_count` template field ships. Editor: `PrefabEditorLayer`
  (`Editor/Source/Layers/PrefabEditorLayer.{h,cpp}`) gained four new Inspector cards (Weapon,
  Armor, Mod, Rarity) alongside its existing four, using only existing `FieldWidgets` primitives
  (`BuildEnumField` for `range_shape`/`slot`, `BuildIdEnumField` for the prefix/suffix affix
  pickers sourced from the Affix library, `BuildRowList` for the repeatable race-bonus rows, and
  a `BuildEnumField` over `"0".."4"` — not `BuildIntField` — for `mod_slot_count`, since nothing
  in `ComponentSchema`/`EntitySchemaEmitter` enforces an integer field's range generically; the
  editor dropdown is the pragmatic enforcement point instead of adding that machinery for one
  field). A new standalone `Editor/Source/Layers/AffixEditorLayer.{h,cpp}` (wired into
  `EditorMenuLayer` as a new "Affixes" row) follows the `PieceEditorLayer`/`DungeonEditorLayer`
  List/Edit template for the bespoke Affix library — much shorter than either, since Affix has
  no nested arrays and needs no preview canvas. **Deviates from this bullet's literal "Item
  editor layer" wording**: coverage is split across the extended Prefabs row and the new
  standalone Affixes row rather than one dedicated layer — the same kind of deviation M5.2 already
  made (folding entity-editor scope into the Prefab Editor instead of a dedicated
  `EntityEditorLayer`). UI: inventory grid / equipment slot panel **deliberately deferred** — no
  gameplay UI layer exists yet to host them, same reasoning every other milestone's UI bullets
  wait on a real screen to display in. Verified live in the running Editor: the Affix Editor's
  full List→New→fill→Save→List cycle was exercised end-to-end and the saved JSON inspected on
  disk to confirm the round-trip (name/kind/stat/amount all correct), then reverted per this
  file's own throwaway-fixture convention; the Prefab Editor's card system was confirmed still
  loading/rendering correctly against the existing `test` prefab. Catch2 coverage in
  `Core-Test/Source/ItemComponentTests.cpp` (schema shape/authorable flags for all four new
  components, plus full `JsonEntityLoader` round-trips for a weapon entity — including
  `race_bonuses` and a non-default `range_shape` — an armor entity, and a mod entity) and
  `Core-Test/Source/AffixSchemaTests.cpp` (schema reflection, save/load round-trip, and the
  unknown-stat-name / schema-version-mismatch error paths), mirroring
  `StatsRaceComponentTests.cpp`/`DungeonSchemaTests.cpp`'s existing structure. Content authoring
  (real starter Hunter/Ranger weapons, per this milestone's own Phase-A scope) is the user's own
  work through these editors, per `CLAUDE.md`'s division of labor — not done by Claude.
- **8.2 Drop tables & Section ID:** Engine: per-enemy common+rare tables, Section-ID weighting
  (10 IDs), boss guaranteed tables, Meseta currency. Editor: drop-table editor (weighted entry
  list per enemy/boss, Section ID weight matrix). UI: loot-drop toast, Meseta HUD counter.
  **Done (worked autonomously overnight; not built/run — no Windows/MSVC/vcpkg toolchain was
  available in that session, so this needs a build + live-Editor/App smoke-test before merging,
  unlike every earlier "Done" note in this file which was verified live):** a new
  `Core/Source/Engine/Items/` file family (`DropEntry`/`DropTable`/`DropTableError`/
  `DropTableSchema`/`DropTableSchemaEmitter`/`DropTableLibrary`/`DropTableLibraryFile`) follows
  the `PhotonArt`/`Affix` bespoke-content-type pattern exactly — one `DropTable` per file under
  `App/Assets/Data/DropTables/*.json`, referenced from an enemy/boss prefab via the new
  `DropTableComponent::drop_table_id` NameId (mirrors `RaceComponent::race_id`). `SectionId`
  (`Core/Source/Engine/ECS/SectionId.h`) is the ten PSO ids plus a `None` sentinel (value 0, this
  codebase's usual "0 = none" convention) for "not chosen yet" — character creation (M10.3) isn't
  built yet to make a real choice, so the new `SectionIdComponent` is authored as a template
  default (`None`) the same way `WeaponComponent::grind_level` is, and `GameplayLayer` only
  `GetOrEmplace`s it onto the player so an authored default on `player.json` isn't clobbered.
  `DropEntry` (shared by `DropTable::common_entries`/`::rare_entries`, one struct not two, mirrors
  `PhotonArtTier`'s reuse across tiers) carries `item_prefab_id`/`weight` plus a single
  `favored_section_id` field rather than a per-entry list of per-section weight overrides — a
  deliberate scope simplification (avoids a two-level nested repeatable array in the editor UI,
  a materially bigger and riskier UI-engineering task to get right without the ability to run it)
  that still captures the real GDD mechanic: `None` is unbiased and always eligible, a matching
  section gets a weight bonus (`Engine/Items/DropResolution.cpp`'s `kSectionIdMatchBonus`, a
  placeholder multiplier per this doc's own "deferred to a future balancing pass" precedent), and
  a non-matching non-`None` entry is excluded outright — PSO itself section-locks some rare/board
  items the same way, not just uniformly biasing every entry's weight. `DropResolution.h/.cpp`
  (`ResolveDrop`) is the pure, `Core-Test`-covered roll: an independent meseta roll when
  `meseta_max > 0`; `boss_guaranteed_rare` or a `rare_chance_percent` roll picks the rare table,
  falling back to common if the rare list has no entry eligible for the roller's Section ID rather
  than dropping nothing. New `MesetaComponent` (not authorable — a running total starting at 0,
  mirrors `EnergyComponent`'s "engine-managed" convention) and `App/Source/Systems/
  LootDropSystem` apply a roll on death: **deliberately not wired via `AttackAction`/
  `PhotonArtAction`/`TechniqueAction`** (the "Photon Arts & Techniques" work the user flagged as
  already in progress in a separate session, and — as of this pass — still visibly mid-flight
  itself: no `photon_art_editor.rml`/`technique_editor.rml`/`TechniqueEditorLayer` exist yet, and
  `EditorMenuLayer` doesn't route to `PhotonArtEditorLayer` either) — instead `LootDropSystem`
  hooks entt's *entity-level* destroy signal (`registry.on_destroy<entt::entity>()`, via
  `Registry::OnDestroy<entt::entity, ...>`, not a per-component one) so it fires before any of the
  dying entity's component pools are cleared, keeping `Position`/`DropTableComponent` safely
  readable regardless of destroy-internal pool ordering — the same reasoning that already has
  `AttackAction` et al. capture a tile before calling `DestroyEntity` rather than reading it back
  afterward. This keeps the whole feature decoupled from — and additive to — whatever the other
  session lands, at the cost of not routing loot through "who actually landed the killing blow"
  (moot for now: single-player, per `docs/GDD.md`'s own scope decision, so Meseta always credits
  whichever entity carries both `MesetaComponent` and `PlayerControlledComponent`). A rolled item
  entry is spawned as a real `GroundItemComponent`-tagged entity in the `Grid` at the death tile —
  visibly on the ground, matching PSO's "walk over a dropped item" — but not pickup-able yet: no
  inventory/pickup system exists (M8.1's own deferral), the same "engine lands, interaction UI
  deferred" precedent M7.1 already set for HP bars/combat log. Editor: `PrefabEditorLayer` gained
  "Loot" (`DropTableComponent`, a `BuildIdEnumField` picker sourced from a new `m_drop_tables`
  library member, mirrors the weapon affix pickers) and "Section ID" (`SectionIdComponent`, a
  plain `BuildEnumField`) Inspector cards, following the exact same
  read-body/write-body/`kComponentKinds`-entry/`RefreshEditForm`-wiring recipe every other card
  already uses. A new standalone `Editor/Source/Layers/DropTableEditorLayer` (wired into
  `EditorMenuLayer` as a new "Drop Tables" row) follows the `PhotonArtEditorLayer`/
  `DungeonEditorLayer` List/Edit template — two repeatable entry lists (common/rare), each row an
  item picker (scans `App/Assets/Data/Entities` for options, tolerant of a missing/empty
  directory) + weight + favored-Section-ID dropdown, using `DungeonEditorLayer`'s deferred-
  `m_pending_action`-drained-in-`OnRender` reorder pattern rather than `PhotonArtEditorLayer`'s
  own synchronous one (which appears to violate `WireDragReorder`'s own "never rebuild
  synchronously" doc comment — an existing issue, not touched here). UI: loot-drop toast / Meseta
  HUD counter **deliberately deferred**, matching every earlier milestone's identical UI-bullet
  deferral reasoning — no HUD exists in `GameplayLayer` yet to host them in. `App/Assets/Data/
  DropTables/` ships with only a `.gitkeep` (no sample content authored — real drop tables are the
  user's own work through the new editor, per `CLAUDE.md`'s division of labor), same reasoning
  `PhotonArts`/`Techniques`/`Affixes` shipping without seed content already established; unlike
  those two libraries, `GameplayLayer::OnAttach` unconditionally loading `DropTablesPath` won't
  throw on a bare checkout, since the directory itself (just no `.json` inside it) is enough to
  satisfy `LoadJsonDirectory`. Catch2 coverage in `Core-Test/Source/DropTableSchemaTests.cpp`
  (schema shape, save/load round-trip, unknown-enum-name and schema-version-mismatch error paths,
  mirroring `AffixSchemaTests.cpp`) and `Core-Test/Source/DropResolutionTests.cpp` (meseta range,
  rare-chance boundary via `rare_chance_percent=0`/`boss_guaranteed_rare`, Section-ID gating/
  fallback, empty-table no-drop) plus `App-Test/Source/LootDropSystemTests.cpp` (Meseta credited
  on a matching death, no-ops for a missing/unknown `DropTableComponent` and for a non-`HealthComponent`
  destroy). **Not verified live** (Editor round-trip, in-game kill-credits-Meseta/spawns-loot) for
  the reason stated at the top of this note — flagged explicitly rather than silently claimed, per
  this project's own verification convention.
- **8.3 Photon crystals / stat materials:** Engine: consumable permanent-stat-boost items
  (Power/Mind/HP Material, etc.). Editor: material-effect fields on the item editor. UI:
  use-item confirmation + stat-gain feedback.

## M9 — Mag Companion

**Status:** Not started

- **9.1 Mag entity & feeding:** Engine: Mag ECS entity, in-field feeding (consumable → Mag),
  evolution/stat-boost accumulation. Editor: Mag species/evolution editor (feed-response
  table, evolution thresholds). UI: Mag status panel, feed-prompt when holding a feedable
  item.

## M10 — Hub, Missions & Difficulty

**Status:** Not started

- **10.1 Persistent hub:** Engine: non-procedural hub scene, shop buy/sell, storage,
  mission-select gated by per-character unlocks. Editor: none new (consumes M4/M5.3 data). UI:
  mission-select/shop/storage/character-sheet screens.
- **10.2 Difficulty tiers:** Engine: Normal→Hard→Very Hard→Ultimate data (stat/population/drop
  deltas + M5.3 roster substitutions), per-character clear-gating. Editor: difficulty-tier
  editor (per-area deltas, substitution table). UI: tier selector, clear/unlock indicators.
- **10.3 Character creation:** Engine: class lock (Hunter/Ranger/Force) + Section ID chosen at
  creation, persisted for the run. Editor: none (player-facing flow, not content data). UI:
  character-creation screen.

## M11 — Progression & Permadeath

**Status:** Not started

- **11.1 XP & leveling:** Engine: combat XP, per-class stat growth curves. Editor:
  growth-curve editor. UI: level-up notification + stat-delta display.
- **11.2 Run persistence & permadeath:** Engine: `cereal`-backed save of in-run state between
  missions; permadeath wipes on death; full reset on new character (no meta-progression, per
  GDD). Editor: none. UI: death/run-summary screen, new-character flow.
