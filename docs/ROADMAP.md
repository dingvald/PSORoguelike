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
- **A minimal debug mission launcher is inserted before M6/M7, not in the original numbered
  list.** Nothing before M10.1 (persistent hub) gives the player a way to actually enter a
  mission. Rather than pull the full hub forward, Phase A adds a throwaway launcher layer
  (pick a class, drop into a generated Forest mission) under CLAUDE.md's test-fixture exception
  — scaffolding to exercise the system, not content. It's replaced by the real M10.1/M10.3 UI in
  Phase B, not kept.
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
18. *(new)* Minimal debug mission launcher — throwaway scaffold to pick a class and drop into a
    generated Forest mission; replaced by M10.1/M10.3 in Phase B

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
26. M10.1 — Persistent hub (replaces the Phase A debug launcher)
27. M10.2 — Difficulty tiers
28. M10.3 — Character creation (replaces the Phase A debug class-picker)
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

**Status:** Not started

- **3.1 Grid & tile rendering:** Engine: `TileMap`/`Grid` component, camera, SDL-renderer tile
  blitting, ECS `Position`/`Transform`. Editor/UI: none yet (no content format to edit until
  3.2).
- **3.2 Area/biome data schema:** Engine: JSON schema for area theme (tile palette, race,
  hazard type) — feeds dungeon generation in M4. Editor: **Area editor layer** — tile palette
  assignment, race/hazard config, live preview (mirrors `BiomeEditorLayer` + its texture/color
  pickers). UI: none (content authoring only, not player-facing).

## M4 — Dungeon Piece Library & Generation

**Status:** Not started

- **4.1 Piece data format & socket schema:** Engine: `DungeonPiece` schema — a tile-grid chunk
  (dimensions, tiles drawn from the owning area's M3.2 palette), border **sockets** (cell
  position + edge direction + a socket type/tag, e.g. `"north-door"`, so the stitcher only
  connects type-compatible sockets), and metadata (room category — corridor/room/vault/boss
  arena — area/race tag, placement weight). Editor/UI: none yet (needs 4.2's authoring UI).
- **4.2 Piece editor:** Engine: none new. Editor: **Piece editor layer** — List/Edit shell
  consistent with the other content editors (M3.2 area editor, M5.2 entity editor); Edit mode
  is a palette-driven tile-paint canvas (adapts `FeatureEditorLayer`'s grid-paint-plus-inspector
  pattern to a full room footprint) plus a socket-placement tool for tagging border cells. UI:
  none (authoring only).
- **4.3 Socket-matching stitcher:** Engine: mission generator that places pieces from the
  library and connects them by matching compatible open sockets edge-to-edge (placement/
  backtracking algorithm), filtered by the piece's area/race tag and the fixed area unlock
  order (4.5). Editor/UI: none new — this is what 4.4 previews.
- **4.4 Generation preview & tuning:** Engine: none new (consumes 4.3). Editor: **generation
  preview tool** — seed/param tuning (piece-pool filters, target room count, dead-end/loop
  tolerance) with live regenerate-and-preview of a full stitched layout (mirrors
  `DungeonEditorLayer`'s always-live preview-canvas pattern). This is where a piece library
  gets validated as actually producing coherent dungeons. UI: none yet (mission-entry UI lands
  in M10).
- **4.5 Fixed area unlock order:** Engine: Forest→Caves→Mines→Ruins gating hook, consumed by
  the hub in M10. Editor: ordering field on the M3.2 area editor. UI: none yet.

## M5 — Entity & Stat Framework

**Status:** Not started

- **5.1 Core stat components:** Engine: ATP/ATA/MST/DFP/EVP/LCK components, four-race tagging
  (Native/A.Beast/Machine/Dark) on enemy prefabs. Editor/UI: none yet (needs 5.2's authoring
  UI).
- **5.2 Entity/enemy editor:** Engine: enemy prefab schema (stats, race, sprite ref, spawn
  weight). Editor: **Entity editor layer** — stat field forms, race picker, sprite picker
  (mirrors `EntityEditorLayer` + `EntityFieldForms`). UI: none (authoring only).
- **5.3 Rare enemies & tier reskins:** Engine: rare-variant flag (alt palette + stat multiplier
  + guaranteed drop hook), tier-based roster substitution (e.g. Garanz→Baranz on Ultimate, not
  just stat scaling). Editor: rare-variant toggle + tier-substitution mapping on the entity
  editor. UI: none yet — rare/boss visual callouts land with combat UI in M7.

## M6 — Turn-Based Scheduler & Movement

**Status:** Not started

- **6.1 Energy-based turn scheduler:** Engine: turn queue keyed on energy cost, variable
  per-action costs (exact numbers explicitly deferred per GDD). Editor: none (numeric
  balancing pass, not structural). UI: turn-order indicator.
- **6.2 Grid movement & input:** Engine: tile-to-tile movement resolution, keyboard/mouse turn
  command mapping. Editor: none. UI: movement/action cursor, valid-move tile highlight.

## M7 — Combat System

**Status:** Not started

- **7.1 Melee/ranged resolution:** Engine: Hunter melee (adjacent/cone/line shapes, ATP-vs-ATA
  tradeoff), Ranger ranged (range/spread/hits-per-turn), four-race damage bonus from 5.1.
  Editor: weapon-type fields (range shape, ATP/ATA split) added to the item schema/editor
  (M8.1). UI: HP/action bars, target/range-preview overlay, combat log.
- **7.2 Photon Arts (PP) & Techniques (TP):** Engine: separate PP (Hunter/Ranger) and TP
  (Force) pools; Photon Art as a chosen PP-cost attack option (not a hidden proc, per GDD's
  turn-based adaptation); Technique spell system with elemental damage + status, tiered by use.
  Editor: **Photon Art / Technique editor** — cost, effect family, tier-scaling fields. UI:
  PP/TP bars, Photon Art/Technique selection menu, status icons.
- **7.3 Status effects:** Engine: Freeze/Poison/Shock/Confuse framework (duration, tick, cure).
  Editor: status-effect fields on the 7.2 editor. UI: status icon + duration on HUD and over
  affected entities.

## M8 — Itemization & Economy

**Status:** Not started

- **8.1 Item & equipment schema:** Engine: weapon/armor/material components + inventory/equip
  slots. Editor: **Item editor layer** — weapon stats, race-bonus %, equip-slot config. UI:
  inventory grid, equipment slot panel.
- **8.2 Drop tables & Section ID:** Engine: per-enemy common+rare tables, Section-ID weighting
  (10 IDs), boss guaranteed tables, Meseta currency. Editor: drop-table editor (weighted entry
  list per enemy/boss, Section ID weight matrix). UI: loot-drop toast, Meseta HUD counter.
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
