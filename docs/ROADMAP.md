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

---

## M1 — Data-Driven Foundation

**Status:** Not started

- **1.1 JSON + serialization integration:** Engine: add `rapidjson` (config authoring) and
  `cereal` (binary run-state persistence) to `vcpkg.json`; a small `psr::Json` load/validate
  helper in `Core`. Editor/UI: none (infrastructure).
- **1.2 Prefab / reflection registry:** Engine: extend `Registry`
  (`Core/Source/Engine/ECS/Registry.h`) with a JSON-loaded prefab registry + `entt::meta`
  component reflection, per the fuller shape `ARCHITECTURE.md` already points at in
  UnnamedRoguelike's `Registry`. Editor/UI: none yet — this is what every later editor
  introspects against.
- **1.3 Content file layout & hot-reload:** Engine: `App/Assets/Data/<category>/*.json`
  convention + file-watch reload in dev builds. Editor/UI: none.

## M2 — Standalone Editor Scaffold

**Status:** Not started

- **2.1 Editor executable:** Engine: none (consumes `Core` as-is). Editor: new `Editor/`
  project + `Build-Editor.lua` linking `Core`, `EditorMenuLayer` as the landing screen listing
  sub-editors (mirrors UnnamedRoguelike's `Editor/Source/main.cpp` + `EditorMenuLayer`). UI:
  `editor_menu.rml`/`.rcss` shell.
- **2.2 Reusable field-widget library:** Editor: port/adapt `FieldWidgets`, `RmlClickListener`,
  `ColorPickerPopup`, `TexturePickerPopup` from UnnamedRoguelike's `Editor/Source/UI` as the
  shared base every later sub-editor builds on. UI: shared `.rcss` widget styles.

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
