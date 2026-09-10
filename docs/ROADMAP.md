# Feature Roadmap

This is a living roadmap connecting [ARCHITECTURE.md](../ARCHITECTURE.md) (how the engine is
built) to [GDD.md](GDD.md) (what the game is). It ordered the work from the original engine
scaffold (SDL3 window, RmlUi, empty ECS, `HelloWorldLayer` — nothing else) through to a
playable game, milestone by milestone, and now carries the ship-readiness work past that point
too. For where the project actually stands today, see "Feature overview" below rather than this
paragraph.

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

**Scope note (ship-readiness pass):** M1-M11 were written as a plan for building the *game
systems*. They do not describe everything a shippable build needs, and taking them as a
complete definition of done would leave the project with no title screen, no audio, no save
file, and no way to quit without losing a character. A feature-overview and gap analysis was run
across the whole codebase to close that hole; its findings are the "Feature overview" section
immediately below, expansions to several existing bullets (each marked **Ship-readiness gap
detail**), and seven new milestones, M12-M18, sequenced as "Phase C" inside the fast-path section.
Content and balance are excluded from that analysis by design — it covers only mechanisms,
tooling, and release engineering.

## Feature overview — what exists today

A snapshot of shipped capability, written so the ship-readiness analysis (M12–M18 below) has
something concrete to be measured against. Every line here is implemented and either
unit-tested or manually verified per its own milestone entry — nothing aspirational appears in
this list, and anything partial says so.

**Engine (`Core`)**

- **Application & loop:** SDL3 resizable window, GPU-backed `SDL_Renderer` (Vulkan/SPIR-V),
  RmlUi context, layer stack with deferred push/replace/remove, two independent event passes
  (raw SDL native + semantic `Event`), deferred-delivery `MessageBus`.
- **ECS:** `entt`-backed `Registry` with `entt::meta` component reflection, JSON-Schema-validated
  prefab loading, per-entity pub/sub (`EventHandlerComponent`), read-path introspection
  (`EntityDescriber`), an `authorable` flag gating what may be hand-authored.
- **Content pipeline:** `ReadJsonFile`/`WriteJsonFile`, `LoadJsonDirectory`, `ContentWatcher`
  (mtime-polled hot-reload, built but not yet wired into `App`), `NameIdRegistry` string interning.
- **World & generation:** fixed-size multi-occupant `Grid`; a `DungeonPiece` library with authored
  sockets and spawn waves; `DungeonStitcher` (entrance-to-exit tree growth, loopbacks, dead-end
  fallback, BFS-verified-solvable lock/key placement); `DungeonInstantiator`; `RoomMap` and
  `RoomVisibilityTracker`.
- **Rendering:** custom SDL_GPU tile pipeline with two-tone palette-swap shading, runtime texture
  atlas packer, camera/viewport with clamped zoom, the `IRenderableLookup` decorator seam,
  sprite-strip animation (`AnimationClock`), floating text, generic visual effects.
- **Turns & actions:** energy-keyed `TurnQueue`, `IAction`/`ActionExecutor` with fallback chains,
  `ActionMap`/`InputBuffer` with DAS-style held-key repeat.
- **Combat primitives:** `HealthComponent` with `HealthSystem` as sole HP writer, `DeathSystem`,
  damage/heal event chains with veto and modify hooks, targeting modes, and the
  status-effect/element content types.

**Game (`App`)**

- **Turn loop & states:** `TurnCoordinator` plus a pushdown `GameStateMachine` with Exploring,
  TargetSelection, Animation, CharacterScreen, TechniquesScreen, MissionSelect, Shop, Storage,
  and GameOver states.
- **Combat:** hit chance (ATA vs EVP, clamped), damage (ATP vs DFP with a variance band),
  four-race bonuses, four weapon range shapes, hits-per-turn, Photon Arts, Techniques with
  tiers/elements/projectiles/aim preview, five status effects with stacking and per-turn ticks,
  on-hit VFX, miss flashes, floating damage numbers.
- **Items:** weapon/armor/mod/rarity/consumable components, an affix library, inventory and
  equipment with equip/unequip, ground pickup and drop, Meseta as a real ground pickup,
  inline weighted drop tables, Technique Disks that teach Techniques, a ten-slot hotbar carrying
  both items and abilities.
- **Progression:** XP on kill, a class-agnostic growth curve, multi-level-up in one kill with a
  diffed stat-gain line in the log.
- **Hub loop:** a hub scene instantiated from one authored piece, walk-up interactables
  (shopkeeper/storage terminal/teleprompter), buy and sell, uncapped storage, mission select,
  mission completion recorded into `RunProgress`, an abandon-mission key, and death returning to
  the hub with inventory/equipment/Meseta/level intact. Mission Select gates on a fixed
  Forest→Caves→Mines→Ruins-style area unlock order once an `Area` names a predecessor.
- **Areas:** an `Area` library (name, dominant race, hazard type, tile palette, unlock
  predecessor) giving a `Dungeon`'s bare `area_tag` real content.
- **HUD:** HP/TP bars, level and name, status-effect chips, hotbar, target panel, Meseta counter,
  a scrolling combat log with inline color/bold/italic markup and scroll-position opacity fade,
  an interaction prompt, and six full-screen modal overlays.
- **AI & population:** piece-authored spawn waves gated on previous-wave clear, one
  `ChaseAndAttack` behavior with a detection range, per-room fog of war, tab targeting.

**Editor**

- Standalone executable, shared dark theme, and a reusable field-widget library
  (int/float/string/bool/NameId/Vec2/enum/color/texture/id-enum/row-list) plus color and texture
  picker popups and a shared pan/zoom preview canvas.
- Eight content editors: Areas, Prefabs (per-component Inspector cards), Pieces (paint grid,
  sockets, spawns), Dungeons (piece pool, live generation preview, lock/key debug overlay),
  Affixes, Photon Arts, Techniques, and Shop Stock.

**Testing:** 30 `Core-Test` files and 49 `App-Test` files of Catch2 coverage over the pure-logic
layer. Rendering, live input, and RmlUi widget code are verified by running the app instead, per
the convention this file already documents.

### What that adds up to, and what it is missing

The vertical slice is genuinely complete: a player can leave a hub, generate a dungeon, explore it
under fog of war, fight enemies that chase and hit back, cast Techniques with real projectiles and
status effects, loot and equip and consume, level up, return to the hub, and buy, sell, store, and
launch the next mission. Very little of the *simulation* is missing.

What is missing is almost entirely the layer *around* that simulation — the parts a player
touches before, between, and after the turn loop, and the parts a build needs to leave a
developer's machine. Specifically, and each expanded into its own milestone below:

| Gap | Severity | Milestone |
|---|---|---|
| No audio subsystem of any kind | Blocker | M12 |
| No title screen, pause, settings, or quit confirmation | Blocker | M14 |
| No save/load; a closed window loses the character | Blocker | M11.2 |
| Escape quits to desktop instantly from gameplay | Blocker | M14 |
| No key rebinding, no gamepad, minimal mouse support | High | M15 |
| No packaging, no CI, Windows-only, three known test failures | Blocker | M18 |
| No item tooltips, comparison, stacking, or sorting | High | M16 |
| No minimap, no turn-order indicator, no help screen | High | M16 |
| Uncapped frame rate (no vsync or frame limiter) | High | M18 |
| One AI behavior; no boss encounter framework | High | M17 |
| Entities are always 1x1; no multi-tile entities or bosses | High | M5.4 / M17.3 |
| ~~An area is a single dungeon; no multi-level areas (Forest 1/2, Caves 1-3) or teleporters~~ — resolved (Mission Select still lists every dungeon individually, see 4.6's own note) | Medium | M4.6 |
| ~~Area/biome schema (M3.2) never built, so no per-area theming~~ — resolved | High | M3 |
| No character creation, no classes, no difficulty tiers | High | M10 |
| Mag companion not started | Medium | M9 |
| No localization seam; strings hardcoded in C++ and RML | Medium | M16 |

Severity here means "distance from shippable," not implementation difficulty. A blocker is
something a player would hit in the first two minutes and correctly call broken.

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
### Phase C — ship readiness (M12–M18, everything but content and balance)

Phase A reached a playable dungeon and Phase B fills in the remaining designed depth. Neither
produces a *shippable build*: both are scoped entirely to simulation and content tooling, and a
player never sees a title screen, hears a sound, rebinds a key, or reloads a character in either
one. Phase C is that missing layer, and it is deliberately kept separate rather than sprinkled
through M1–M11 — mixing "the game needs a pause menu" into a combat milestone is how it stays
un-owned until the week before release.

Ordering logic within Phase C: the session shell (M14) comes first because everything else needs
somewhere to live — an options screen has no home until there is a pause menu, and save/load has
no trigger until there is a "quit to title." Persistence (M11.2) follows immediately, since it is
the one remaining blocker that silently destroys player progress. Audio (M12) is third because it
is the largest single missing subsystem and its editor surface (sound reference fields) wants to
land before content authoring gets far. Input and options (M15) then have both a screen to live in
and a settings file to persist into. UI polish (M16) and encounter depth (M17) are the two
broad-front passes, and release engineering (M18) is last only in the sense that its final gates
run last — its CI and cross-platform work should start early and run continuously.

31. M14 — Front end & session flow (title, pause, quit, transitions, credits)
32. M11.2 — Run persistence & permadeath (`cereal` added here; see its heavily expanded entry)
33. M12 — Audio (engine, content type, editor field, and a cue for every existing feedback moment)
34. M15 — Input, options & accessibility (rebinding, gamepad, mouse, settings persistence)
35. M16 — UI polish & information architecture (tooltips, stacking, minimap, help, localization seam)
36. M17 — Encounter & simulation depth (boss framework, AI behaviors, factions, rare enemies)
37. M18 — Release engineering & distribution (CI, packaging, crash handling, performance, licensing)

**Phase C does not include content or balance.** Authoring the four areas, their enemies, weapons,
drop weights, growth curve numbers, prices, and difficulty deltas remains the user's work through
the editors, per `CLAUDE.md`'s division of labor. Phase C is only about making a build that a
stranger could install, launch, understand, play, configure, quit, and come back to.

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

**Status:** 3.1 done, 3.2 implemented **pending your own Windows build + manual verification** —
same caveat M10.1 already carries: this session ran in a Linux sandbox with no MSVC/vcpkg
toolchain, so the new `Area`/editor code compiles by careful inspection against this codebase's
own established patterns, not an actual build.

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
  pickers). UI: none (content authoring only, not player-facing). **Done:** built as its own
  bespoke content type (`App/Source/Areas/`: `Area`/`AreaSchema`/`AreaSchemaEmitter`/
  `AreaLibrary`/`AreaLibraryFile`, content at `App/Assets/Data/Areas/*.json`) rather than fields
  folded onto `Dungeon`, per the user's explicit brief — follows the `Affix`/`Dungeon` five-file
  family exactly, letting multiple Dungeons share one Area definition. Lives in **App, not
  Core** — `Dungeon`/`DungeonPiece` (Core) stay theme-agnostic, carrying `area_tag` as a bare
  filter string with no meaning of its own (see `DungeonStitcher`'s plain equality checks); `Area`
  is what gives that string real content, the same relationship `RaceComponent` (also App-side)
  already has with Core's generic NameId race fields. `Area::tag` — not its file-derived
  `id_string` — is the field that actually matches `Dungeon::area_tag`/`DungeonPiece::area_tag`,
  since those compare as a plain string, not a `NameIdRegistry`-resolved reference like every
  other cross-content link in this project; `tag` defaults to `id_string` when left unauthored,
  same "name defaults to id" convention `Affix`/`Dungeon` already use for their own `name` field.
  Fields: `name`, `tag`, `race_id` (NameId, open-ended like `RaceComponent`'s own races), `hazard`
  (a fixed `HazardType` enum — **None/Poison/Electric/Fire/Dark**, per the user's explicit choice
  over an open NameId, since hazards are meant to be a short curated list like `Element`/
  `SectionId`; nothing consumes a hazard yet, so it only needs to round-trip as data for now, same
  precedent `DungeonLockConfig`/`Affix::amount` already set), a three-field tile palette
  (`floor_texture_id`/`wall_texture_id`/`accent_texture_id`, each a plain NameId rather than a
  nested object, for the same "no consumer yet" reason — this project's rendering is
  prefab-driven per M3.1's own design note, not biome/tileset-driven, so there is nothing to wire
  these into besides round-tripping), and `unlock_predecessor_tag` (a plain area-tag string, empty
  = unlocked from the start — M4.5 below is its consumer). Editor: new
  `Editor/Source/Layers/AreaEditorLayer` (+ `area_editor.rml`), per the user's explicit choice of
  a dedicated layer over folding into `DungeonEditorLayer` — follows `AffixEditorLayer`'s List/
  Edit shell exactly (no nested arrays, no preview canvas), with the three texture-palette fields
  and `race_id` as plain `BuildNameIdField` text-entry rather than `PrefabEditorLayer`'s
  swatch+picker-popup `BuildTextureField`, deliberately avoiding that popup's extra plumbing for
  fields nothing consumes yet; wired into `EditorMenuLayer` as a new "Areas" row (now first in the
  list, ahead of Pieces, since every later piece/dungeon-authoring step wants an Area to tag
  against first). UI: none (content authoring only, not player-facing, matching the bullet's own
  UI lens). A committed `App/Assets/Data/Areas/.gitkeep` keeps the directory present on a fresh
  checkout — `LoadJsonDirectory` throws if the directory itself is missing (unlike an empty one),
  and `GameplayLayer` now loads this library for real every run (unlike `m_affixes`/`m_photon_arts`
  /`m_status_effects`, which stay pending their own content directories not existing yet — a
  pre-existing gap this pass didn't touch since it's outside this bullet's scope, but is worth
  flagging: those three `LoadXLibrary` calls in `GameplayLayer::SpawnNewCharacter` will throw on a
  truly fresh checkout with no local editor-created directory, since `Affixes`/`PhotonArts`/
  `StatusEffects` are never `git add`ed as empty dirs the way `Areas` now is). No area content
  (real Forest/Caves/Mines/Ruins definitions) authored — that's the user's own work through the
  new editor, per `CLAUDE.md`'s division of labor; `IsDungeonUnlocked` (M4.5 below) falls back to
  "unconditionally unlocked" for any `area_tag` with no matching `Area` yet, so today's existing
  `test_dungeon.json` (whose `area_tag` is empty) is unaffected. Catch2 coverage in
  `App-Test/Source/AreaSchemaTests.cpp` (schema shape/field kinds, `SaveArea`/`LoadAreaLibrary`
  round-trip including `FindByTag`, the `tag`-defaults-to-`id_string` fallback, and the
  unknown-hazard/schema-version-mismatch error paths).

  **Ship-readiness gap detail — resolved.** Was the most depended-on unstarted bullet in this
  file (M4.5's unlock order, M10.2's per-area difficulty deltas, M12.4's per-area music, M13.4's
  area title card, and M17.5's per-area weighted spawn tables all name it as a prerequisite); M4.5
  is picked up immediately below in the same pass. M10.2/M12.4/M13.4/M17.5 still consume nothing
  from `Area` yet — that's each milestone's own future work.

## M4 — Dungeon Piece Library & Generation

**Status:** 4.1/4.2/4.3/4.4 done, 4.5 implemented **pending your own Windows build + manual
verification** (same caveat as M3.2 above -- landed in the same unverified pass), 4.6 not started

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
  `Core-Test/Source/GridTests.cpp`. **Follow-up redesign:** a socket is no longer a stamped
  entity prefab carrying `SocketComponent` — that component and its `basic_socket` prefab are
  gone. Sockets are now data authored directly on `DungeonPiece` (`PieceSocket`:
  `cell_offset`, `edge`, `tags`, `connects_to_tags`, `fallback_prefab_id`,
  `Core/Source/Engine/Dungeon/DungeonPiece.h`), since a socket has no visual of its own and
  doesn't belong in a cell's stamped-prefab list. `PieceCellPrefab` correspondingly lost its
  per-stamp `edge` override (nothing reads it once sockets moved off it). Matching also
  changed from a single symmetric `tags`-to-`tags` intersection to a one-way filter checked
  both directions: socket A connects to B iff `A.connects_to_tags ∩ B.tags` or
  `B.connects_to_tags ∩ A.tags` is non-empty (`tags` is what a socket *is*, `connects_to_tags`
  what it *accepts*).
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
  entity — items (M8) and interaction (M6/M7) don't exist yet for that wiring. The stitcher
  reads socket data straight off each piece's own `DungeonPiece::sockets` (see 4.1's follow-up
  redesign) and has no ECS/Registry dependency of its own, so it's fully unit-testable against
  synthetic fixture pieces with no live engine state. Catch2
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
  the hub in M10. Editor: ordering field on the M3.2 area editor. UI: none yet. **Done:** rather
  than a numeric ordering field, `Area` (M3.2 above) carries an explicit
  `unlock_predecessor_tag` string (empty = unlocked from the start) — a predecessor reference
  reads its own gating directly, without every area needing to agree on a shared index space, and
  matches `Dungeon`/`DungeonPiece`'s existing "area is just a plain tag" convention rather than
  introducing a second kind of area reference. `Missions::IsDungeonUnlocked`'s body (previously
  unconditionally `true`, per its own doc comment naming this bullet as the fill-in) now resolves
  `dungeon.area_tag` to an `Area` via the new `AreaLibrary::FindByTag`, and — if that area has a
  non-empty `unlock_predecessor_tag` — requires at least one dungeon tagged with that predecessor
  area to already be in `RunProgress::completed_dungeon_ids`. **Deviates from the function's own
  prior doc comment** ("M4.5 is expected to extend this function's body, not signature"): that
  turned out to be unworkable once `Area` existed as real content rather than a hypothetical —
  resolving a predecessor area's completion needs both the `Area` definition itself and every
  other `Dungeon` tagged with that predecessor's area, neither of which `IsDungeonUnlocked`'s old
  two-parameter signature could reach. It now takes `DungeonLibrary`/`AreaLibrary` too; both new
  call sites (`MissionSelectSnapshot::BuildMissionSelectMessage`, already threaded through
  `MissionSelectState`, and `GameplayLayer::OnMissionSelected`) already held both libraries as
  process-lifetime members, so this was a pure wiring change, no new loading. An area with no
  matching `Area` entry (including today's placeholder `test_dungeon.json`, whose `area_tag` is
  empty) stays unconditionally unlocked, preserving this function's pre-M4.5 fallback exactly. No
  ordering field on the editor as such (see M3.2's own note) — the predecessor-tag field doubles
  as it. UI: none yet, unchanged from this bullet's own scope — a locked Mission Select row's
  visual treatment is M16's job. M10.2 is expected to add a tier parameter alongside this same
  check once difficulty tiers exist. Catch2 coverage in `App-Test/Source/RunProgressTests.cpp`
  (unconditionally unlocked with no matching `Area`; unconditionally unlocked for an `Area` with
  no predecessor; locked until a predecessor-area dungeon is completed, unlocked after) and
  updated `MissionSelectSnapshotTests.cpp` for the new `AreaLibrary` parameter.
- **4.6 Multi-level dungeons & teleporters.** Engine + editor **done**; Mission Select still lists
  every `Dungeon` individually (see below). Previously a `Dungeon` asset generated one stitched
  layout, Entrance to Exit, and reaching the Exit auto-triggered mission completion on room entry.
  `Area` (M3.2) now carries `dungeon_id_strings`, an ordered list of `Dungeon::id_string`s making
  up that area's sequence (Forest 1, Forest 2; Caves 1-3; ...) — a plain string list, same
  "plain tag, not a hashed ref" convention `unlock_predecessor_tag` already set. A new
  `TeleporterComponent` (`App/Source/Components/TeleporterComponent.h`, `destination`:
  `return_to_hub` or `advance_level`) is stamped into the Entrance/Exit pieces' cells as ordinary
  entity prefabs (`dungeon.entrance_teleporter`/`dungeon.exit_teleporter`) — no new
  `PieceCategory` needed, since Entrance/Exit already exist and the teleporter is just another
  stamped prefab like `hub.teleporter`. Standing on one and pressing Space (mirrors the hub's
  `InteractableComponent` UX, dispatched via `Missions/TeleporterInteraction.h`'s
  `FindTeleporterAt`) calls `GameplayLayer::OnTeleporterActivated` directly instead of opening a
  modal screen: `return_to_hub` bails out with no completion credit (same as the `H` abandon key);
  `advance_level` credits the current dungeon completed, publishes `MissionCompletedMessage`, and
  transitions to whatever `Missions::AreaProgression.h`'s `NextDungeonInArea` resolves (the next
  dungeon in the sequence, or the hub if this was the last one) — reusing
  `TransitionToWorld`/`DungeonInstantiator`'s existing world-swap plumbing exactly as planned. This
  fully replaces the old room-category auto-trigger (`GameplayLayer::OnMissionExitReached`,
  `m_room_categories`, `m_mission_exit_handled` are gone). Editor: the Area editor's new "Dungeon
  Sequence" card (a reorderable row list, `AreaEditorLayer::RefreshDungeonSequenceRows`, mirroring
  `DungeonEditorLayer`'s own piece-ref/lock lists) authors the ordered list; the teleporter
  prefabs themselves need no new Piece-editor code, since cell-prefab stamping was already
  generic. UI: a "Press SPACE to return to the Hub"/"...proceed to the next level" HUD prompt
  (`TeleporterPromptMessage`, dungeon-scene sibling of `HubInteractionPromptMessage`) — the
  "Forest 1/2" level-indicator/transition-card UI from the original note is still open, deferred
  to M13.4/whichever milestone lands a HUD area-progress readout. **Known gap:** Mission Select
  (`MissionSelectSnapshot`/`IsDungeonUnlocked`) still gates and lists every `Dungeon` individually,
  not one row per `Area` — once an author gives an `Area` more than one `dungeon_id_strings`
  entry, every level in the sequence stays directly selectable from Mission Select rather than
  only the first. Reworking Mission Select to show one row per `Area` (selecting its first level)
  and gating later levels on sequence position instead of `unlock_predecessor_tag` is follow-up
  work, not yet scheduled to a milestone.

## M5 — Entity & Stat Framework

**Status:** 5.1/5.2 done, 5.3/5.4 not started. A generic **Prefab Editor** already exists ahead of schedule
(`Editor/Source/Layers/PrefabEditorLayer`) — browse/create/edit/delete the entity-prefab JSON
files under `App/Assets/Data/Entities/`, rendering one Inspector-style card per currently-registered
*authorable* component (see the M4.4 follow-up note above for the `authorable` flag and shared
`PreviewCanvas`/`PreviewWindowChrome` it builds on). 5.1's stat/race sections below were added as
additional hand-wired cards on this same layer, per its own class doc comment, not a rewrite.

- **5.1 Core stat components:** Engine: ATP/ATA/MST/DFP/EVP/LCK components, four-race tagging
  (Native/A.Beast/Machine/Dark) on enemy prefabs. Editor: wired into the Prefab Editor now (pulled
  forward from 5.2, per the user's brief) rather than deferred to a dedicated entity/enemy editor
  pass. **Done:** `StatsComponent` (`Core/Source/Components/StatsComponent.h`) is a single struct
  of six `int` fields (`atp`/`ata`/`mst`/`dfp`/`evp`/`lck`, all defaulting to `0` — no balance
  numbers are authored here per CLAUDE.md's division of labor). `RaceComponent`
  (`Core/Source/Components/RaceComponent.h`) holds a single `race_id` field — deliberately a
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
  Catch2 coverage in `App-Test/Source/StatsRaceComponentTests.cpp` (schema shape/authorable, plus
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

  **Ship-readiness gap detail:** restated as a mechanism (not content) in M17.4 below. The
  rare-variant roll needs a home in the spawn path (`DungeonInstantiator` / `SpawnWaveSystem`),
  and the alternate palette is nearly free given `RenderableComponent`'s existing two-color
  palette-swap shading. The "rare/boss visual callout" this bullet defers to M7 was never picked
  up there; it is tracked in M17.4's UI lens now, and wants an audio cue alongside it (M12.3) -
  a rare spawn the player does not notice defeats the entire mechanic.
- **5.4 Multi-tile entities & footprints.** Not started. `Grid` (M3.1, extended in M4.1's
  follow-up) holds a stack of entities per cell, but every entity that gets stamped or spawned
  into one — the eventual boss included — is assumed to occupy exactly one cell; there is no
  concept of a single logical entity spanning more than one tile. Bosses (M17.3) and other large
  enemies need a footprint of multiple cells (a 2x2 or 3x3 body, or an irregular shape) that still
  behaves as one entity: one `HealthComponent`, one turn-queue slot, one set of stats, but
  movement, collision, targeting, and rendering that account for every occupied cell, not just an
  anchor tile. Engine: a `FootprintComponent` (an offset list from the entity's anchor `Position`)
  that `Grid` placement/removal, `MoveAction`'s collision check, targeting-mode queries, and
  `RenderableTile`/`TileRenderer` all read instead of assuming 1x1 — placement needs to stamp the
  anchor entity into every covered cell so a lookup from any of them resolves to the same entity,
  and move/destroy needs to clear and restamp the whole footprint atomically rather than one cell
  at a time. Editor: a footprint field (width/height, or an explicit offset list for irregular
  shapes) on the Prefab Editor, with the preview canvas and `PreviewWindowChrome` rendering the
  full footprint instead of a single cell. UI: none new — existing HUD/targeting UI already keys
  off a resolved entity, not a tile count. This is a prerequisite for M17.3's boss framework
  (a boss confined to one tile reads as a reskinned regular enemy, not a boss) and is scoped as
  its own bullet rather than folded into that milestone, since ordinary large enemies benefit from
  it too.

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

### M6 follow-up: `ActorComponent` replaces `EnergyComponent`, action costs scale by speed

**Status:** done, landed uncommitted alongside a session boundary and captured here retroactively,
same lag the M7/M8 follow-up sections already had. 6.1's own bullet named "variable per-action
costs" as in scope but never delivered a mechanism to vary them by — every action charged its flat
base cost regardless of the acting entity. `EnergyComponent` (the lone `{energy}` field
`TurnQueue` membership keyed off) is gone, folded into a renamed, extended
`App/Source/Components/ActorComponent.h`: `ap` (the same persisted scheduling value, renamed from
`energy`) plus two new stats, `movement_speed`/`act_speed` (both default `100` = unmodified).
`TurnCoordinator` and every other call site that referenced `EnergyComponent` was mechanically
repointed at `ActorComponent`/`ap` — no behavior change there. New
`App/Source/Combat/ActionCost.h/.cpp` (`EffectiveMoveCost`/`EffectiveActCost`) scales a base cost
by `100 * base_cost / speed` (missing `ActorComponent` == baseline 100, same "safe to call for any
entity" contract `ComputeEffectiveStats` already sets), floored at 1; `MoveAction`/`AttackAction`
now charge `EffectiveMoveCost(actor, kMoveCost)`/`EffectiveActCost(actor, kAttackCost)` instead of
the flat constants directly, and `PhotonArtAction`/`TechniqueAction`/`UseItemAction`/`DropAction`
were updated to the same seam for their own fixed costs. `ap` is deliberately **not** exposed
through `PrefabEditorLayer`'s new "Actor" Inspector card (only `movement_speed`/`act_speed` are
authorable) — it's pure runtime scheduling state, never meant to be hand-set on a prefab, matching
the old `EnergyComponent`'s same never-authored precedent. Catch2 coverage: new
`App-Test/Source/ActionCostTests.cpp` and `ActorComponentTests.cpp` (schema shape/authorable
fields), extended `AttackActionTests.cpp`/`MoveActionTests.cpp`/`TurnCoordinatorTests.cpp` for the
renamed component and speed-scaled costs.

## Gameplay Layer (Phase A item 18)

**Status:** initial landing done (player spawn + movement in a fixed test dungeon)

The permanent gameplay entry point — see the Phase-A deviation note above for why this isn't a
throwaway launcher. **Done:** two new pieces close the loop from "generated dungeon layout" to
"player moving around on screen with wall collision," neither of which existed before:
`Core/Source/Engine/Dungeon/DungeonInstantiator.h/.cpp` (`ComputeDungeonBounds`/
`InstantiateDungeon`) bridges a `DungeonStitcher`-produced `DungeonLayout` into a live `Grid` of
entities — stamping every placed piece's cells via `Registry::CreateEntity(prefab_id)`, additionally
stamping a dead-end socket's own `fallback_prefab_id` (mirrors the Dungeon Editor preview's own
dead-end rendering; see 4.1's follow-up redesign — a socket carries no prefab of its own to swap,
so its fallback stamps in addition to the cell's ordinary prefabs rather than replacing one), and
translating the layout's possibly-negative world coordinates into the
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

### Gameplay Layer follow-ups: spawn waves, enemy AI, fog of war, game over/restart

**Status:** done, landed across two commits (`8c77b44` "Add piece-authored spawn waves with
runtime wave-clear gating", `0147975` "Add enemy AI, fog of war, and game-over/restart flow") that
weren't captured in this file when they merged. Filed here rather than under a numbered milestone
because none of the four cleanly is one — this is the remaining work needed to actually reach the
Phase A "fight Forest enemies" checkpoint (the initial `GameplayLayer` landing above still had no
enemy spawning) plus playability polish (fog of war, game over) beyond what any single bullet
named.

- **Spawn waves** (`8c77b44`): closes the "Spawns ... for spawn waves" item in
  `issues_and_bugs.md`, modeled on the socket redesign. `DungeonPiece` gains `PieceSpawn`
  (`cell_offset`/`prefab_id`/`wave`, `Core/Source/Engine/Dungeon/DungeonPiece.h`), authored/
  validated through the same schema/JSON pipeline as `PieceSocket`, with a mirrored "Add Spawn" UI
  in `Editor/Source/Layers/PieceEditorLayer.cpp`. Unlike sockets, waves are runtime-gated, not just
  data: `DungeonInstantiator::InstantiateDungeon` groups a placed piece's spawns by wave number,
  stamps the lowest wave immediately (tagged with a new runtime-only `SpawnWaveComponent{group_id,
  wave}`), and hands later waves to a new `SpawnWaveSystem`
  (`Core/Source/Engine/Dungeon/SpawnWaveSystem.{h,cpp}`) as `PendingSpawnWave` data. `SpawnWaveSystem`
  spawns each subsequent wave once every entity from the previous wave dies — deliberately keyed off
  `SpawnWaveComponent`'s own `entt::on_destroy` signal rather than `DeathEvent`/`HealthSystem`/
  `DeathSystem` (the same pattern `TurnCoordinator` already uses for `EnergyComponent`), so the
  mechanic works regardless of why an entity died. An all-invalid-prefab wave is a documented stall
  edge case, covered by tests. Catch2 coverage: new `SpawnWaveSystemTests.cpp`, extended
  `DungeonInstantiatorTests.cpp`/`DungeonPieceSchemaTests.cpp`.
- **Enemy AI** (`0147975`): replaces M6.1's "every non-player actor Waits by default" stub. New
  `AiComponent` (`App/Source/Components/AiComponent.h`) currently has one behavior,
  `ChaseAndAttack`, plus a `detection_range` (default 8). `EnemyAiSystem::Decide`
  (`App/Source/Systems/EnemyAiSystem.{h,cpp}`) finds the nearest `PlayerControlledComponent` entity
  within Manhattan `detection_range`, snaps the delta to a cardinal direction, and issues a
  `MoveAction` (trying the perpendicular axis if the primary one is blocked, else Waiting) — no
  separate attack path, since `MoveAction`'s existing bump-into-hostile fallback (M7.1) already
  converts a step into an occupied hostile tile into an `AttackAction`. `TurnCoordinator::
  SetNpcDecision` is wired in `GameplayLayer::OnAttach` to `EnemyAiSystem::Decide`.
  `TurnCoordinator` also gained `m_live_player_count` (tracked via `PlayerControlledComponent`
  construct/destroy), so losing the last player returns a new `TurnStep::PlayerDefeated` instead of
  spinning forever requeuing NPCs. Catch2 coverage: new `EnemyAiSystemTests.cpp`, extended
  `TurnCoordinatorTests.cpp`.
- **Fog of war**: `RoomMap` (`Core/Source/Engine/Dungeon/RoomMap.{h,cpp}`) is a flat per-tile array
  mapping each stamped tile to its placed-piece index, built alongside the `Grid` inside
  `InstantiateDungeon`; `DungeonInstantiation` also now carries `room_adjacency`. `RoomVisibilityTracker`
  (pure logic, no Grid/Registry) tracks `Hidden`/`Explored`/`Visible` per room, updated each turn from
  the player's current room via `RoomMap::GetRoom`, extending visibility to adjacent rooms too.
  `FogOfWarRenderableLookup` (`App/Source/Render/FogOfWarRenderableLookup.{h,cpp}`) decorates an
  inner `IRenderableLookup` — the same seam M3.1 reserved and `RegistryRenderableLookup` already
  implements — hiding entities in never-visited rooms entirely, darkening `Explored`-room tiles
  (35% factor) and additionally hiding any `AiComponent`/`PlayerControlledComponent` occupant there,
  and passing `Visible` rooms through unchanged. Kept as a separate decorator (not folded into
  `RegistryRenderableLookup`) so Editor previews stay unaffected. Catch2 coverage: new
  `RoomVisibilityTrackerTests.cpp`, `FogOfWarRenderableLookupTests.cpp`.
- **Game over / restart flow**: new `GameOverState` (`App/Source/States/GameOverState.{h,cpp}`,
  new `GameStateId::GameOver`) is pushed by `ExploringState` on `TurnStep::PlayerDefeated`,
  publishes `PlayerDefeatedMessage` once on entry, and on first keypress publishes
  `RestartRequestedMessage` (latched to avoid double-publish). `GameplayLayer::OnRestartRequested`
  rebuilds the registry/dungeon from scratch (`LoadNewGame()`), pops `GameOverState` off the state
  stack to uncover the same `ExploringState`, and publishes `GameRestartedMessage`. `HudLayer`
  subscribes to both — showing a "You Died" overlay and clearing it/the log on restart.
- **`InnateWeaponComponent`** (`App/Source/Components/InnateWeaponComponent.{h,cpp}`) is a baked-in
  weapon reference (`weapon_prefab_id`) for entities with no interactive equip flow: `GameplayLayer`'s
  spawn hooks read it once to create a weapon entity and set `EquipmentComponent::weapon`, used
  identically for the player's starting weapon and for enemy spawns (its `DeathEvent` handler
  destroys the equipped weapon entity on death, since it's not lootable). This is also the first
  content-authored enemy beyond throwaway test fixtures: `App/Assets/Data/Entities/enemies/
  booma.json` (`ai`/`innate_weapon`/`health`/`stats`/`race`/`renderable`), with
  `booma_claws.json`/`saber.json` as its and the player's weapons. Catch2 coverage: new
  `InnateWeaponComponentTests.cpp`.

### Gameplay Layer follow-ups: tab targeting, camera zoom, Frame armor, breakable boxes, HUD event-log overhaul

**Status:** done, landed uncommitted alongside a session boundary and captured here retroactively —
the same lag every other follow-ups section in this file already had. Five unrelated small
additions bundled here because none is large enough for its own milestone bullet.

- **Tab targeting** (`b89c63a`): Tab cycles the player's persistent target through every hostile
  `HealthComponent` entity, nearest-first by Manhattan distance (a fresh scan each press, so
  death/movement never leaves a stale ordering); Escape clears it. `TabTargetComponent` holds the
  lock on the player; `TabTargetSystem` owns a single reusable world marker
  (`ui.tab_target_marker`, drawn underneath the target's own sprite) repositioned via `Grid`
  remove/add each frame as the target moves, and self-clears if the target dies. Both keys are
  intercepted directly in `GameplayLayer::OnEvent` alongside the existing non-turn-costing
  Character/Techniques-screen key handling, not routed through `ActionMap`.
  `TargetSelectionState` now starts its interactive cursor at (`TargetSquare`) or facing
  (`Directional`) the locked target instead of always defaulting to the caster's own tile/facing,
  so attacks/Techniques/Photon Arts aim at it immediately. `HudLayer` gains a bottom-left target
  panel (name, race, HP as a numberless bar) driven by a new `TargetStateMessage`, published every
  frame alongside the existing floating-text state. `DisplayName`'s player-or-prefab-label
  resolution was lifted out of `CombatLogBridge` into a shared `Combat/DisplayName.h` so both call
  sites share one implementation. Confirmed with the user: room-visibility already restricts
  cycling to visible enemies, no change needed there.
- **Numpad camera zoom** (`5c2b130`): `Camera` already had a clamped `SetZoom`/`GetZoom` and
  `TileRenderer::Draw` already accepted a zoom factor, but `GameplayLayer` never called either —
  `OnRender` hardcoded `zoom=1.0f`. Numpad +/- now adjust `Camera`'s zoom in 0.5 steps (handled
  unconditionally in `OnEvent`, a view control rather than a turn action); the render call uses
  `Camera::GetZoom()`, and floating text's tile-to-pixel conversion scales its tile step by zoom too
  so it stays aligned with the world. `ZoomLimits.h`'s range is now `[1x, 4x]`.
- **Frame armor** (`de0e4d5`): the equip/unequip and damage-calc plumbing for armor already existed
  and worked (M8.1's `ArmorComponent`/`EquipmentComponent` slot routing, already folded into every
  damage/hit-chance formula) — this adds the missing piece, an actual armor prefab (Frame, torso,
  DFP 10, matching PSO GameCube's base Frame) obtainable from `box_metal`, plus mod-slot *display*
  as bullet points under its Character-screen equipment row (always "(empty)" for now, since
  inserting a mod into a slot has no defined mechanic yet).
- **Breakable boxes** (`6ea8d98`): `box_wood` (15 HP, common Monomate/small-Meseta drops) and
  `box_metal` (35 HP, rarer Technique-Disk/bigger-Meseta drops) are pure content, no engine changes
  needed — `HealthComponent` + `BlocksMovementComponent` already make `MoveAction`'s existing
  bump-into-hostile-with-HP fallback redirect an attack into them, and
  `HealthSystem`/`DeathSystem`/`LootDropSystem` already handle HP-to-zero, destruction, and
  drop-table rolling generically for any `HealthComponent` entity. Stamped into several existing
  room pieces as static furniture (not the enemy-spawn path, so neither joins the turn queue or
  gets AI).
- **HUD event-log overhaul** (`2e6df5b`): fixed `AppendLogLine` reading `GetScrollHeight()` one
  frame stale (RmlUi only re-lays-out in `Context::Update()`, which runs after `HudLayer`'s own
  `OnUpdate`) — previously invisible until the log outgrew its fixed-height box, at which point the
  newest line was perpetually clipped; the scroll-to-bottom (and now opacity) recompute is deferred
  to the top of the next `OnUpdate` instead. Adds a small `[c=#RRGGBB]`/`[b]`/`[i]` inline markup
  syntax (`LogMarkup.h`) so combat-log lines can carry color/bold/italic — wired into
  `CombatLogBridge`/`ExperienceSystem`/`HudLayer::OnLootDrop` reusing `hud.rcss`'s existing accent
  palette (this is what M11.1's diffed level-up stat-gain line and the loot-drop/Technique-learned
  lines above render through) — prefixes every entry with a "> " marker, and fades each visible
  line's opacity by its position in the log's scroll viewport (newest = 1.0, oldest visible = 0.25)
  via a new `RmlScrollListener` that recomputes live as the log is scrolled.

## M7 — Combat System

**Status:** 7.1/7.2/7.3 done

- **7.1 Melee/ranged resolution:** Engine: Hunter melee (adjacent/cone/line shapes, ATP-vs-ATA
  tradeoff), Ranger ranged (range/spread/hits-per-turn), four-race damage bonus from 5.1.
  Editor: weapon-type fields (range shape, ATP/ATA split) added to the item schema/editor
  (M8.1). UI: HP/action bars, target/range-preview overlay, combat log. **Done:** a new
  `HealthComponent` (`Core/Source/Engine/ECS/HealthComponent.h` -- `current_hp`/`max_hp`, same
  shape as `StatsComponent`) fills the gap M8.1 left open: nothing could be damaged or killed
  before this, since no HP concept existed anywhere. `Core/Source/Combat/CombatMath.h/
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
  PP/TP bars, Photon Art/Technique selection menu, status icons. **Done:** the brief evolved twice
  mid-implementation — first to require a real interactive target-select flow (directional/
  target-square/self-target) rather than deferred targeting UI, then to require that flow match
  `UnnamedRoguelike`'s own targeting architecture exactly, confirmed by directly reading that
  sibling's implementation rather than assumed. `Core/Source/Engine/Combat/TargetingMode.h`
  (`Directional`/`TargetSquare`/`SelfTarget`) and `EffectFamily.h` (`Damage`/`Drain`/`Status`) are
  new shared enums; `PhotonArt.h`/`Technique.h` (+ `Schema`/`SchemaEmitter`/`Library`/
  `LibraryFile`/`Error`, one five-file family each) are standalone bespoke content types mirroring
  `Affix`'s pattern exactly, not `ComponentSchemaRegistrar` components, each with an authored
  `tiers` array (`{tier, power_multiplier}`) resolved at `tiers[0]` only — the per-actor
  usage-counter M11.1 would need to advance tiers doesn't exist yet, same deferral shape as M5.2's
  spawn-weight. `Technique` deliberately carries no `drain_percent` (unlike `PhotonArt`): a
  `Drain`-family Technique type-checks (the enum is shared) but resolves identically to `Damage`
  for lack of an amount to size a restore by. `WeaponComponent` gains `photon_art_ids`/
  `technique_ids` (`std::vector<std::uint32_t>` NameId refs into the two new libraries) — a Saber
  grants Photon Arts, a Wand grants Techniques, by content convention, not engine enforcement.
  `PPComponent`/`TPComponent` (`Core/Source/Engine/ECS/`) are new, same shape as
  `HealthComponent`, no regen mechanic this round. `CombatMath::ComputeTechniqueDamage` mirrors
  `ComputeDamage`'s formula keyed on `mst` (`StatsComponent`'s technique-power field, unused until
  now). `App/Source/Combat/TargetResolution.h/.cpp` lifts `AttackAction`'s own tile-geometry
  helper out of its anonymous namespace (plus a new `SnapToCardinalDirection`) so
  `PhotonArtAction`/`TechniqueAction` (`App/Source/Actions/`, new) can share it against their own
  `range_shape`/`range` fields — both read their target from a new non-authorable
  `SelectedTargetComponent` (`{Vec2 tile}`, mirrors the sibling's own component of the same name)
  written at confirm time rather than taking one via constructor, keeping them stateless like
  every other `IAction`; a `{0,0}` offset (self-target) skips the hit roll entirely. Unlike
  `AttackAction`'s free-swing-into-empty-air convention, the turn cost is always charged once a
  cast executes — the player explicitly chose this target through an interactive flow, so there's
  no accidental miss to refund.
  <br><br>
  The targeting flow itself ports `UnnamedRoguelike`'s architecture: `TurnCoordinator` now
  implements a small `ITargetRequestSink` (`RequestTargeting`/`TakePendingTargetRequest`/
  `SetPendingAction`), and `Step()` gained `TurnStep::TargetingRequested`. One documented
  deviation from the sibling: since which Photon Art/Technique is in play depends on the equipped
  weapon and a placeholder slot number (resolved dynamically, not fixed at `ActionMap` bind time
  the way the sibling's one hardcoded ranged attack was), `GameplayLayer` calls
  `RequestTargeting` directly from its own key handler instead of through an `ActionMap`-bound
  `SelectTargetAction` — so `Step()` checks the pending request at the very top of its loop rather
  than only right after resolving an action. Per the user's explicit choice, this milestone also
  ports the sibling's generic pushdown state-machine framework (not a lighter local-flag
  equivalent), anticipating reuse for Inventory/Equipment/menu states around M8.3/M10:
  `App/Source/States/GameState.h`/`GameStateMachine.h/.cpp` (renamed from the sibling's
  `IGameState` — per CLAUDE.md, the `I` prefix is reserved for all-pure-virtual interfaces, and
  `OnEnter`/`OnExit`/`HandleEvent` have default bodies, the same reasoning that already keeps this
  project's own `Layer` base class unprefixed), `ExploringState` (wraps the previous direct
  `TurnCoordinator::Step()` call), and `TargetSelectionState` — the modal cursor, spawned as a
  real data-driven ECS entity (`App/Assets/Data/Entities/ui/target_select_cursor.json`, a bare
  `RenderableComponent`) drawn by the ordinary tile-render pass, not a bespoke overlay. All three
  `TargetingMode` values share one mechanism (spawn/move/render/confirm/cancel identical) varying
  only the reachable-tile predicate and the arrow-key step function: `SelfTarget` fixes the cursor
  at the origin; `Directional` jumps it straight to whichever cardinal neighbor is pressed;
  `TargetSquare` moves it incrementally within a Chebyshev `range` gate. Out-of-range is signaled
  by recoloring the cursor sprite grey in place, mirroring the sibling's own `Greyed()` helper.
  `GameplayLayer` now owns a `GameStateMachine` (pushed `ExploringState` at startup) instead of
  calling `TurnCoordinator::Step()` directly, and gained a placeholder `TryBeginCast` (number keys
  1–4 for Photon Art slots, 5–8 for Technique slots — throwaway test wiring per CLAUDE.md's
  fixture exception, real selection-menu UI is still out of scope) that resolves the equipped
  weapon's Nth granted id, checks PP/TP affordability, and calls `RequestTargeting`. Editor:
  `PhotonArtEditorLayer`/`TechniqueEditorLayer` (+ `.rml`) are new List/Edit shells mirroring
  `AffixEditorLayer`, each with a repeatable `tiers` row list; `EditorMenuLayer` gained two rows;
  `PrefabEditorLayer`'s Weapon card gained two repeatable `BuildIdEnumField` row lists
  (`photon_art_ids`/`technique_ids`) sourced from the two new libraries, loaded in `OnAttach`
  alongside the existing Affix library. UI: PP/TP bars, a real Photon Art/Technique selection
  *menu* (today's number-key slots are a stand-in), and status icons are **deliberately deferred
  this round**, per the brief — `status_effect_id` ships unconsumed pending M7.3. Catch2 coverage
  in `App-Test/Source/PhotonArtSchemaTests.cpp`/`TechniqueSchemaTests.cpp` (schema reflection +
  round-trip + malformed-content/version-mismatch errors), new `ComputeTechniqueDamage` cases in
  `CombatMathTests.cpp`, an extended `WeaponComponent` round-trip in `ItemComponentTests.cpp`; and
  `App-Test/Source/PhotonArtActionTests.cpp`/`TechniqueActionTests.cpp` (no-weapon/ungranted-id/
  insufficient-pool no-ops, self-target skips the hit roll, lethal resolution, tier multiplier,
  race bonus), `TargetSelectionStateTests.cpp` (reachable-tile predicate and cursor movement per
  mode, grid-edge clamping, confirm/cancel, and a full round trip into
  `TurnCoordinator::SetPendingAction`), `GameStateMachineTests.cpp` (push/pop/replace/dispatch in
  isolation from any concrete state), and new `RequestTargeting`/`TakePendingTargetRequest`/
  `SetPendingAction` cases in `TurnCoordinatorTests.cpp`.
- **7.3 Status effects:** Engine: Freeze/Poison/Shock/Confuse framework (duration, tick, cure).
  Editor: status-effect fields on the 7.2 editor. UI: status icon + duration on HUD and over
  affected entities. **Done:** `Burn` was added alongside the four ROADMAP-named types per the
  user's explicit request, and a genuine elemental-damage layer (`Element`: Fire/Ice/Lightning/
  Light/Dark) was built alongside the status framework, superseding `Technique::element_id`'s
  earlier free-form-NameId shape (the GDD's "declines to commit to a fixed roster" framing is now
  moot -- the user committed to exactly five). `StatusEffect`/`StatusEffectType`/`Element`
  (+ `Schema`/`SchemaEmitter`/`Library`/`LibraryFile`/`Error`, the usual five-file family) are new
  `Core/Source/Engine/Combat/` bespoke content types mirroring `Affix`'s pattern exactly, content at
  `App/Assets/Data/StatusEffects/` (unpopulated, same pre-existing gap as `PhotonArts/`/
  `Techniques/`/`Affixes/` -- no directory exists on disk yet, so `GameplayLayer::OnAttach`'s
  `LoadStatusEffectLibrary` call throws today exactly as its Photon-Art/Technique siblings already
  do). `StatusEffectComponent` (`Core/Source/Engine/ECS/`, not meta-registered -- runtime-only
  accumulated state, mirrors `TweenComponent`'s precedent) holds `{status_effect_id, stacks,
  remaining_duration}` stacks, added on demand via `ApplyStatusEffect`
  (`StatusEffectApplication.h`) -- re-applying the same effect increments its stack count and
  refreshes remaining_duration to the fresh application's value, per the user's explicit stacking
  answer. `TickStatusEffects` runs once per turn (see below): Poison/Burn deal `magnitude * stacks`
  self-inflicted damage (via the same `BeforeDamageEvent`/`AfterDamageEvent` dispatch
  `TechniqueAction`'s self-target branch already uses) and every stack's `remaining_duration`
  decrements, expiring at 0 (the "cure" this bullet calls for is natural expiry only -- no Cure
  item/spell content is authored this pass, per CLAUDE.md's content-authoring boundary).
  Freeze/Shock/Confuse are presence-based, not magnitude-scaled, per the user's explicit answers:
  Freeze pre-empts `TurnCoordinator::Step()`'s action selection entirely, substituting a real,
  energy-costing forced `WaitAction` (never a zero-cost one -- that would stall the turn queue on
  the same frozen actor); Shock cancels `BeforeAttackEvent`/`BeforePhotonArtCastEvent`/
  `BeforeTechniqueCastEvent` outright via a new `cancelled` field on each (mirroring
  `BeforeMoveEvent::cancelled`'s existing veto pattern) -- attack-type actions no-op for zero cost,
  movement still works; Confuse redirects `BeforeMoveEvent::offset` to a random cardinal direction,
  which required actually consuming that field in `MoveAction::Perform` for the first time (it
  existed but was dead -- `m_offset` was read instead). New `Core/Source/Engine/Actions/TurnEvent.h`
  (`AfterTurnEvent`) is dispatched by `TurnCoordinator::Step()` once per resolved turn (including a
  forced Wait), driving `TickStatusEffects` via `StatusEffectComponent`'s own subscribed handler --
  a lethal tick can now destroy the acting entity mid-`Step()`, so the post-`ResolveAction`
  energy/requeue block gained an `actor.IsValid()` guard (this exact hazard already existed latently
  for a self-lethal `EffectFamily::Damage` self-target cast; DoT is what makes it routinely
  reachable). Elemental damage: `WeaponComponent` gained `element`/`status_effect_id`/
  `status_chance_percent` (a weapon's own flavor, inherited by both its plain attacks and its
  granted Photon Arts -- "channeled through" the weapon, per the user's explicit
  "extend to weapons/Photon Arts" answer); `Technique` kept its own spell-authored `element` plus a
  new `status_chance_percent`. `App/Source/Combat/StatusEffectHooks.h`'s
  `MaybeApplyElementalStatus` rolls that chance on a landed, non-lethal hit in all three actions.
  `EffectFamily::Status` is now actually implemented in `TechniqueAction`/`PhotonArtAction`'s
  directional target loops (previously a pre-M7.3 gap: they dealt damage regardless of
  `effect_family`) -- landing the hit-chance roll guarantees the ailment with no damage roll;
  self-target Status stays a no-op (out of scope, no buff-shaped use case exists yet). Editor: new
  `StatusEffectEditorLayer` (+ `.rml`) mirrors `AffixEditorLayer`'s List/Edit shell exactly;
  `EditorMenuLayer` gained a row; `TechniqueEditorLayer`'s `element_id` NameId field became an enum
  dropdown, plus a new `status_chance_percent` field; `PrefabEditorLayer`'s Weapon card gained
  `element`/`status_effect_id`/`status_chance_percent` rows (`status_effect_id` a real
  `BuildIdEnumField` picker sourced from a newly-loaded `StatusEffectLibrary`, same treatment as
  `prefix_affix_id`). UI: `StatusEffectsMessage` + `CombatLogBridge::PublishStatusEffects` (new
  `AfterStatusEffectsChangedEvent` subscription) feed `HudLayer` a colored icon+stack+duration chip
  row per active ailment; "over affected entities" reuses M7.2's own target-select-cursor precedent
  (a real data-driven ECS entity, `App/Assets/Data/Entities/ui/status_effect_marker.json`) instead
  of touching `TileRenderer` -- a new `StatusEffectWorldMarkers` (mirrors `CombatLogBridge`'s
  single-tracked-entity scope, "no enemies spawn yet") spawns/repositions one tinted marker per
  distinct active type, riding the same per-turn `AfterStatusEffectsChangedEvent` cadence so a
  marker never lags an entity's own movement by more than its last turn. Catch2 coverage in
  `App-Test/Source/StatusEffectSchemaTests.cpp`/`StatusEffectApplicationTests.cpp` (stacking,
  DoT/decrement/expiry, a lethal tick destroying the entity without crashing) and extended
  `TechniqueSchemaTests.cpp`/`ItemComponentTests.cpp` for the new fields; `App-Test`'s
  `TurnCoordinatorTests.cpp` (Freeze forces a real-cost Wait pre-empting even a confirmed pending
  action, `AfterTurnEvent` fires once per turn, survives a lethal tick), `Attack`/`PhotonArt`/
  `TechniqueActionTests.cpp` (Shock's zero-cost cancel, elemental status on a guaranteed-chance
  hit, `EffectFamily::Status` dealing no damage), and `MoveActionTests.cpp` (a deterministic
  redirect-consumption regression guard plus a statistical Confuse trial, since
  `StatusEffectComponent`'s Confuse handler owns its own unseedable RNG).

### M7 follow-ups: Technique Disks replace weapon-granted Techniques, a real selection screen, technique projectiles, and generic on-hit VFX

**Status:** done, landed uncommitted alongside a session boundary and captured here retroactively —
the same lag the M6/M8 follow-up sections already had. Closes two of 7.2's own explicitly-deferred
bullets ("real Photon Art/Technique selection menu," "status icons" stays open) plus adds a
mechanic 7.2 never scoped at all (Techniques as something *learned*, not innately granted).

- **Technique Disks & learning:** per the user's explicit direction, Techniques are no longer
  weapon-granted — `WeaponComponent::technique_ids` is gone (Photon Arts are unaffected, still
  weapon-granted). New `App/Source/Components/KnownTechniquesComponent.h`
  (`{known: vector<KnownTechniqueEntry{technique_id, tier}>}`, deliberately not meta-registered —
  runtime-only player state, same precedent `EquipmentComponent`/`InventoryComponent` already set)
  tracks what a Force has actually learned. `Items/TechniqueLearning.h`'s `LearnTechnique` teaches
  or raises a known Technique's tier (never downgrades a re-consumed lower-tier disk) via
  `GetOrEmplace`, called by `UseItemAction` on a new `ConsumableEffect::TeachTechnique`
  (`ConsumableComponent` gained a `technique_id` field alongside its existing `effect`/`amount`).
  Content: `App/Assets/Data/Entities/TechniqueDisks/{foie,barta,zonde,resta}_disk.json`, one
  consumable-item prefab per existing Technique, plus drop-table entries on `box_metal` (rarer than
  Monomate/Monofluid). `TechniqueAction` now resolves its tier/affordability check against
  `KnownTechniquesComponent` instead of the old weapon-granted lookup — casting an unlearned
  Technique is a free no-op, same shape as an unaffordable-PP/TP cast already was.
- **Real Techniques/Photon Arts selection screen:** replaces 7.2's placeholder number-key slots
  (1–4/5–8) entirely. New `App/Source/States/TechniquesScreenState` (pushed/popped on `T`, mirrors
  `CharacterScreenState`'s suspension mechanism exactly, minus `RequestClose()` — every action on
  this screen, hotbar assignment included, is free/instant, so there's never a mid-modal turn to
  wait out) publishes a new `TechniquesScreenMessage` (`Items/TechniquesScreenSnapshot.h`'s
  `BuildTechniquesScreenMessage`: every learned Technique from `KnownTechniquesComponent` plus the
  equipped weapon's granted Photon Arts, fully-resolved display names/TP costs/icon paths, same
  "fully resolved, id is the key" contract `CharacterScreenMessage` already set for its own rows).
  `HudLayer` renders it as a `#techniques-screen` overlay (same `hud.rml` overlay-div convention the
  Character screen and game-over screen both already use) with keyboard row navigation; selecting a
  row opens an "assign to hotbar slot 0–9" prompt publishing a new
  `TechniquesScreenSlotAssignedMessage`, routed by `GameplayLayer` to a new
  `Items/Hotbar.h::AssignAbilityToHotbarSlot` (validates server-side — the id must actually be
  known/granted, not trusted from the message — same free/instant contract `AssignItemToHotbarSlot`
  already has). `GameplayLayer::TryActivateSlot`'s number-key Photon-Art/Technique slot stub from
  7.2 is gone, replaced by resolving whatever id each hotbar slot was actually assigned.
- **Technique projectiles:** `Technique` gained `projectile_speed`/`projectile_prefab_id`/
  `projectile_pierces` fields (`projectile_speed == 0`, the default, keeps a Technique's existing
  instant-resolve behavior — `zonde` is untouched). When set, `TechniqueAction` spawns a real
  `ProjectileComponent` + `ActorComponent` entity (a genuine `TurnQueue` participant, not a fake
  tween) instead of resolving damage inline; its per-hop cost is derived from `projectile_speed`
  against the same `ActorComponent`/`ActionCost` scheduler every other actor uses, so it acts far
  more often than a normal entity with zero scheduler changes needed. New
  `ProjectileAdvanceAction` moves it one tile per turn (instant `Position`/`Grid` update plus a
  cosmetic glide `TweenComponent`, same idiom `MoveAction` already uses) and, once its path is
  exhausted, rolls the hit fresh against whatever occupies the impact tile(s) — dispatched as the
  original caster so damage attribution/combat log/lifesteal are unaffected. A new
  `BuildProjectilePath` (shared with `TargetSelectionState`'s aim-preview, see below) decides
  whether it stops at the first creature/wall or pierces to full range, per the authored
  `projectile_pierces` flag.
- **Generic on-hit VFX:** every weapon/Photon-Art/Technique hit (instant or projectile-resolved)
  can now spawn a placeholder VFX at the impact tile. New `OnHitEffectComponent`
  (`effect_prefab_id`/`duration`, authored on weapon prefabs) threads through
  `BeforeAttackEvent`/`BeforePhotonArtCastEvent` (via `EquipmentComponent`'s existing event-fill
  seam) and `IncomingDamageEvent`/`AfterDamageEvent` into a new `OnHitEffectSystem`, which spawns it
  via the existing `VisualEffectSystem` — one mechanism covering all three attack paths. Techniques
  carry the same two values directly (`hit_effect_prefab_id`/`hit_effect_duration`, since they
  aren't ECS entities). Placeholder content: `vfx/generic_hit.json`,
  `TechniqueDisks/{foie,barta}_projectile.json`.
- **Target-select cursor & projectile aim preview:** the cursor prefab now renders `rectangle.png`
  instead of reusing `floor.png`. While aiming a projectile Technique, `TargetSelectionState` spawns
  travel-preview and area-preview tile entities as the cursor moves, computed via the same
  `BuildProjectilePath` the real cast uses, so the preview always matches what the cast will
  actually do (wall-stopping/pierce behavior included); melee Photon Arts and instant-cast
  Techniques are unaffected. Confirmed with the user: tab-targeting's existing room-visibility
  filter already restricts cycling to visible enemies, no change needed there.
- Editor: `PrefabEditorLayer`'s Weapon card lost its `technique_ids` row list (Photon Arts only
  now); `TechniqueEditorLayer` gained the three new projectile fields and
  `hit_effect_prefab_id`/`hit_effect_duration`; `PrefabEditorLayer`'s Renderable-adjacent cards
  gained an `OnHitEffectComponent` card. UI: PP/TP bars and status icons predate this entry (see
  M7.2/M7.3 above); a real selection *menu* was this entry's own deferred item, now done — status
  icons over affected entities remain the only still-open UI item from 7.2/7.3. Content authoring
  (real disk/projectile/VFX prefab tuning beyond the placeholder values above) is the user's own
  work, per `CLAUDE.md`'s division of labor.

## M8 — Itemization & Economy

**Status:** 8.1/8.2 done (8.1 pulled forward, see the Phase-A note above). Note: the Phase A "fight
Forest enemies" checkpoint itself wasn't actually reachable until enemy spawning/AI landed — see
"Gameplay Layer follow-ups" above, filed there rather than under M8 since it's checkpoint
completion work, not itemization. 8.1's previously-deferred inventory/equipment UI is now done too
— see "M8 follow-ups" immediately below. Recovery consumables (Monomate/Monofluid) and the
use-item action are also done, as an addition alongside 8.3 rather than 8.3 itself — see the
addition filed right after 8.3 below. 8.2's drop-table/Section-ID shape was since simplified and
Meseta turned into a real pickup, and the Character screen gained a full context-menu/keyboard-nav
overhaul — see the second "M8 follow-ups" section, right after the first.

### M8 follow-ups: item pickup, inventory, and the Character screen

**Status:** done, landed across two commits (`89eab6c` "Add item pickup, drop, and inventory
systems", `0ccb11d` "Add Character screen: Inventory + Equipment lists with equip/unequip") that
weren't captured in this file when they merged — the same lag "Gameplay Layer follow-ups" above
already had once. Filed here rather than folded into 8.1's own bullet because it's UI/interaction
work fulfilling 8.1's previously-deferred bullet, not a redo of the schema itself.

- **Item pickup/drop** (`89eab6c`): new `ItemComponent` (`Core/Source/Engine/ECS/ItemComponent.h`,
  empty tag) marks a ground-entity prefab as pickupable — every weapon/armor/mod prefab
  (`saber.json`, `booma_claws.json`) gained an `"item": {}` component block. New
  `InventoryComponent` (`App/Source/Components/InventoryComponent.h`, `{items:
  vector<entt::entity>, capacity=20}`) — deliberately **not** meta-registered
  (`vector<entt::entity>` has no `FieldKind` mapping), same "runtime-only, never hand-authored"
  precedent `EquipmentComponent` already set in 8.1, hardcoded-emplaced on the player in
  `GameplayLayer::LoadNewGame`. New turn-costing `IAction`s in `App/Source/Actions/`:
  `PickupAction` (100 cost, scans the actor's tile for `ItemComponent` occupants up to capacity,
  moves them off the `Grid` into `InventoryComponent`) and `DropAction` (100 cost, constructed
  per-invocation with a runtime-chosen inventory index — the item to drop is a UI choice, not a
  key binding) reversing it. Both dispatch a new `AfterItemPickupEvent`/`AfterItemDropEvent`
  (`Core/Source/Engine/Items/ItemPickupEvent.h`/`ItemDropEvent.h`, just `{item_prefab_id}`) at the
  actor; `CombatLogBridge` gained `OnItemPickup`/`OnItemDrop` subscribers logging a line via the
  existing event-log path (same reuse-over-new-widget call M8.2 already made for loot toasts).
- **Character screen** (`0ccb11d`): closes 8.1's deferred "inventory grid / equipment slot panel"
  UI bullet. New `CharacterScreenState` (`App/Source/States/CharacterScreenState.{h,cpp}`) pushed
  on `C`, popped on `C`/Escape — the same suspension mechanism `TargetSelectionState` already uses
  (only the state-stack top changes, turn loop pauses). No separate `.rml` document: folded into
  the existing `hud.rml`/`hud.rcss` as a `#character-screen` overlay div, same convention as the
  game-over overlay. New `App/Source/Items/Equip.h/.cpp` — free functions `EquipItem(Entity actor,
  int inventory_index)`/`UnequipSlot(Entity actor, EquipmentSlot slot)`, deliberately **not**
  `IAction`s (nothing else can act while the modal is open, so no turn cost). `HudLayer::
  OnCharacterScreenState` renders both lists (fully-resolved display strings from a new
  `CharacterScreenSnapshot.h/.cpp`, decorating weapon names with prefix/element/suffix/grind),
  attaches a click listener per row publishing `InventoryItemActivatedMessage`/
  `EquipmentSlotActivatedMessage`; `GameplayLayer` handles both (guarded on the Character screen
  being the active state), calling `EquipItem`/`UnequipSlot` directly and republishing the screen
  state on success. `EquipItem` currently no-ops silently for non-weapon/non-armor inventory
  entries (nothing routes a click on those yet) — relevant background for the consumable-item
  addition below.

### M8 follow-ups: drop-table simplification, Meseta pickups, and the Character-screen context menu

**Status:** done, landed uncommitted alongside a session boundary and captured here retroactively —
the same lag the two follow-up sections above already had. Four related changes, bundled because
they landed together and the last two depend on the first two: dropped loot needing to actually be
pickupable/usable is what motivated giving the Character screen a real per-item action menu instead
of Equip being the only thing an inventory click could do.

- **Drop-table simplification:** the standalone `DropTable`/`DropTableSchema`/`DropTableLibrary`/
  `DropTableLibraryFile` five-file family (`App/Source/Items/`), `DropTableEditorLayer` (+
  `drop_table_editor.rml`), the "Drop Tables" editor menu row, and both `ApplicationFilepaths`/
  `EditorFilepaths::DropTablesPath` are deleted outright, along with Section-ID weighting and the
  common/rare pool split 8.2 originally built. `DropTableComponent`
  (`App/Source/Components/DropTableComponent.h`) is now authored directly and inline on each
  enemy/boss prefab instead of referencing a separate library asset by id: a flat weighted pool of
  `no_drop_weight`, `meseta_weight` (with `meseta_min`/`meseta_max`), and `entries` (a
  `std::vector<LootEntry{item_prefab_id, weight}>`) — exactly one outcome resolved per kill by the
  rewritten `DropTableRoller::Roll` (`App/Source/Items/DropTableRoller.cpp`), no Section-ID
  multiplier or rare-roll gate anymore. `booma.json` was re-authored to the new shape as the one
  existing content reference. Editor: `PrefabEditorLayer` lost its `m_drop_tables` library
  dependency and its single `BuildIdEnumField` drop-table picker, gaining `RefreshDropEntryRows` (a
  `BuildRowList` of item-picker + weight rows, same shape 8.2's `common_entries`/`rare_entries` rows
  already used) so a table's `entries` are authored inline on the Drop Table card instead of on a
  separately-edited asset. UI: no change — Meseta still gets the HUD counter 8.2 already built (see
  below for it becoming pickup-driven rather than instant-credit).
- **Meseta as a real ground pickup:** previously `LootDropSystem` credited `CurrencyComponent`
  directly on a Meseta roll — no pickup step, unlike every other drop. New
  `CurrencyPickupComponent` (`App/Source/Components/CurrencyPickupComponent.h`, single `amount`
  field) + `App/Assets/Data/Entities/meseta.json` (an `ItemComponent`-tagged ground prefab carrying
  it) give Meseta the same ground-entity shape every other drop already has.
  `LootDropSystem::OnDamage` now spawns the `meseta` prefab (its `CurrencyPickupComponent::amount`
  overwritten with the roll) instead of touching `CurrencyComponent`; `PickupAction::Perform` gained
  a first pass over the tile's occupants that special-cases a `CurrencyPickupComponent` hit —
  credits `CurrencyComponent`, publishes `MesetaChangedMessage`, and destroys the pickup entity
  immediately, never entering `InventoryComponent`/counting against its capacity (Meseta was never
  meant to occupy an inventory slot). `PickupAction`/`CreateDefaultKeyBindings` both gained a
  `MessageBus&` parameter to thread through for that publish.
- **Character-screen context menu overhaul:** the previous Character screen only supported one
  action per row (click an inventory item to Equip it, click an equipment slot to Unequip). Now
  every row opens a real context menu instead: Inventory items offer `Equip`/`Use`/`Drop`/`Assign to
  Hotbar` (filtered — `Use` only appears on a `ConsumableComponent` item, `Equip` only on a
  weapon/armor item, both driven by two new `CharacterScreenMessage::ItemEntry` fields,
  `equip_slot`/`is_consumable`, populated in `CharacterScreenSnapshot.cpp` from the already-existing
  `ResolveEquipSlot` — pulled out of `Equip.cpp`'s anonymous namespace into a shared declaration in
  `Equip.h` so both call sites use one switch, not two); Equipment slots offer `Remove` (routes to
  the same `UnequipSlot` as before) plus a same-item shortcut to jump focus onto its matching
  Inventory row (`HudLayer::JumpToMatchingInventoryItem`) rather than a redundant second Equip path.
  `InventoryItemActivatedMessage` gained an `InventoryItemAction` enum (`Equip`/`Use`/`Drop`);
  `GameplayLayer::OnInventoryItemActivated` still resolves `Equip` instantly (free, no turn cost,
  matching `EquipItem`'s existing contract), but `Use`/`Drop` now construct a real `UseItemAction`/
  `DropAction` and submit it via `TurnCoordinator::SetPendingAction` — an actual energy-costing turn
  — closing the Character screen first via a new `CharacterScreenState::RequestClose()` so the
  submitted action can actually resolve once `ExploringState` is back on top of the state stack.
  `HotbarSlotAssignedMessage` (new) carries the `Assign to Hotbar` flow: `HudLayer` shows an
  "awaiting a 0-9 keypress" sub-state after that menu choice, then publishes the message once one is
  pressed; `GameplayLayer::OnHotbarSlotAssigned` routes it to a new pure function,
  `AssignItemToHotbarSlot` (`App/Source/Items/Hotbar.h/.cpp` — a `ConsumableComponent` item only,
  since that's the only kind an Item-type `HotbarSlot` can resolve; see `HotbarComponent.h`'s
  updated doc comment), which rewrites `HotbarComponent` and republishes `HotbarStateMessage`.
  `HudLayer` also gained real keyboard navigation for the whole screen — panel focus (Stats/
  Equipment/Inventory, via a new `CharacterScreenPanel` enum), row focus within a panel, and
  highlight-driven menu selection — replacing what was previously mouse-click-only interaction;
  `CharacterScreenMessage::StatsSummary` (new) gives the Stats panel real numbers for the first
  time: HP/TP straight from `HealthComponent`/`TPComponent`, plus ATP/ATA/MST/DFP/EVP/LCK computed
  through the existing `ComputeEffectiveStats` (M7.1) — the same numbers combat itself uses, base
  stats plus equipped-item/affix bonuses. `BuildCharacterScreenMessage` had to take a non-const
  `Registry&` (previously `const`) since `ComputeEffectiveStats` needs an `Entity`, whose
  constructor requires one.
- Editor/UI answers per CLAUDE.md's own framing: the drop-table simplification's editor surface
  moved (not disappeared — see above); the Character-screen work is entirely UI, no new editor
  surface needed since nothing here is authorable content. Content authoring (the actual
  `entries`/`no_drop_weight`/`meseta_weight` values on any given enemy beyond `booma.json`'s
  existing reference numbers) remains the user's own work through the Prefab Editor, per
  `CLAUDE.md`'s division of labor. Catch2 coverage: rewritten `DropTableRollerTests.cpp` (flat-pool
  resolution, no-drop/meseta/item outcomes, the rounding-edge fallback, seed-reproducibility) and
  `LootComponentsSchemaTests.cpp`/`LootDropSystemTests.cpp` (new `CurrencyPickupComponent` schema
  shape, Meseta spawning as a real ground entity with the rolled amount, no more direct
  `CurrencyComponent` credit at drop time) replacing the deleted `DropTableSchemaTests.cpp`; extended
  `PickupActionTests.cpp` (currency-pickup credit-and-destroy path bypassing `InventoryComponent`
  capacity, ordinary items unaffected); new `HotbarTests.cpp` (`AssignItemToHotbarSlot`'s
  bounds/component-guard no-ops and success path) and `CharacterScreenSnapshotTests.cpp`
  (`ItemEntry::equip_slot`/`is_consumable` population, `StatsSummary` round-trip including the
  `ComputeEffectiveStats` figures).

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
  `App-Test/Source/AffixSchemaTests.cpp` (schema reflection, save/load round-trip, and the
  unknown-stat-name / schema-version-mismatch error paths), mirroring
  `StatsRaceComponentTests.cpp`/`DungeonSchemaTests.cpp`'s existing structure. Content authoring
  (real starter Hunter/Ranger weapons, per this milestone's own Phase-A scope) is the user's own
  work through these editors, per `CLAUDE.md`'s division of labor — not done by Claude.
- **8.2 Drop tables & Section ID:** Engine: per-enemy common+rare tables, Section-ID weighting
  (10 IDs), boss guaranteed tables, Meseta currency. Editor: drop-table editor (weighted entry
  list per enemy/boss, Section ID weight matrix). UI: loot-drop toast, Meseta HUD counter.
  **Superseded in part:** the standalone `DropTable` library/editor and Section-ID weighting
  described below were later deleted and replaced by a simpler inline-authored shape — see "M8
  follow-ups: drop-table simplification, Meseta pickups, and the Character-screen context menu"
  right after the follow-ups section below. Left as-written for the historical record of what 8.2
  originally built. **Done:** Section ID is a real `enum class SectionId` (`App/Source/Items/SectionId.h`, 10 values
  + `EnumNames`) rather than an open NameId — same fixed-small-roster precedent M7.3's `Element`
  already set, unlike `RaceComponent`'s deliberately open-ended NameId. `DropTable` is a new bespoke
  content type following the `Affix`/`Dungeon` five-file family exactly (`App/Source/Items/
  DropTable.h` + `Schema`/`SchemaEmitter`/`Library`/`LibraryFile`, content at
  `App/Assets/Data/DropTables/*.json`): `DropTableEntry{item_prefab_id, weight,
  section_id_weights}` (a `std::array<float, kSectionIdCount>`, authored as a sparse JSON object
  keyed by section-id name — unlisted IDs default to `1.0`, so most entries never write all 10), and
  `DropTable{common_entries, rare_entries, guaranteed_item_ids, rare_roll_chance_percent,
  meseta_min, meseta_max}`. New pure-function `DropTableRoller::Roll` (`App/Source/Items/
  DropTableRoller.{h,cpp}`, same "randomness passed in, no Registry" shape as
  `StatusEffectHooks::MaybeApplyElementalStatus`) always includes every guaranteed entry, gates
  rare-vs-common via `rare_roll_chance_percent`, weighted-picks **one** entry from the chosen pool
  (weight × that entry's Section-ID multiplier), and rolls Meseta independently — matching PSO's
  "one item slot per table tier" drop shape, not a full weighted-bag-of-many-items simulation. New
  `App/Source/Systems/LootDropSystem.{h,cpp}` is `Subscribe()`d only on the player (unlike
  `CombatLogBridge`'s subscribe-every-actor pattern) since `AfterDamageEvent` is dispatched at the
  attacker (see M7.1's `DamageEvent.h`) and loot should only drop for a player-landed killing blow:
  on a `target_defeated` hit it reads the target's new `DropTableComponent` (a NameId ref, same
  shape as `RaceComponent`) and the player's new `SectionIdComponent`, rolls via `DropTableRoller`,
  spawns each item as an ordinary ground entity at the target's tile (`Registry::CreateEntity` +
  `Grid::AddEntity`, the exact call-site pattern `SpawnWaveSystem` already established — no
  pickup/inventory mechanic exists yet, so loot just sits there, consistent with
  `EquipmentComponent`'s still-unpopulated M8.1 deferral), and credits a new `CurrencyComponent`
  (`{int meseta}`) on the player directly. `SectionIdComponent`/`CurrencyComponent` are both
  hardcoded-emplaced on the player in `GameplayLayer::LoadNewGame` (defaulting to `Viridia`/`0`)
  rather than authored in `player.json`, mirroring `HealthComponent`'s own "no character creation
  yet" precedent there. Editor: new `DropTableEditorLayer` (+ `.rml`) follows
  `AffixEditorLayer`/`PhotonArtEditorLayer`'s List/Edit shell, with `guaranteed_item_ids`/
  `common_entries`/`rare_entries` as `BuildRowList` repeatable rows (item picked via
  `BuildIdEnumField` sourced from every authored entity prefab — no dedicated "item library" exists,
  per M8.1's own note that weapons/armor/mods are just prefabs). **Deviates from the bullet's literal
  "Section ID weight matrix" wording**: no 2D grid/matrix widget exists anywhere in `FieldWidgets`
  (confirmed before building this), so each entry's Section-ID overrides live in a collapsed-by-
  default sub-panel (reusing `WireCollapseToggle`'s existing per-item collapse mechanism verbatim)
  instead of new matrix-input plumbing — the common case (no favoritism) never needs opening it.
  `EditorMenuLayer` gained a "Drop Tables" row; `PrefabEditorLayer` gained a "Drop Table" Inspector
  card (single `BuildIdEnumField` sourced from `DropTableLibrary`, same shape as the Weapon card's
  affix pickers). UI: per the roadmap's own two-surface wording, a loot drop feeds the **existing**
  scrolling event log (`CombatLogEntryMessage` path) rather than new toast/fade-timer widget
  machinery that doesn't exist anywhere else in this codebase — a pragmatic reuse-over-new-code call;
  Meseta gets a real persistent counter element in `hud.rml` (`HudLayer::OnMesetaChanged`, same
  pattern as the HP/TP bars). Catch2 coverage in `App-Test/Source/DropTableSchemaTests.cpp` (schema
  reflection incl. the nested per-Section-ID `Object` field, round-trip incl. sparse overrides,
  unknown-section-id-key and meseta-min-over-max error paths), `DropTableRollerTests.cpp`
  (guaranteed drops always included, common/rare gating, a zeroed Section-ID multiplier
  deterministically excluding an entry, Meseta range, seed-reproducibility),
  `LootDropSystemTests.cpp` (no-op on a non-lethal hit or a missing `DropTableComponent`, ground-item
  spawn + `CurrencyComponent` credit + message publication on a lethal player hit, Section-ID
  weighting reaching all the way from the player's component through to the roll), and
  `LootComponentsSchemaTests.cpp` (schema shape/authorable flags for the three new components plus a
  `JsonEntityLoader` round-trip). Content authoring (a real drop table wired onto `booma.json`) is
  the user's own work through the new editor, per `CLAUDE.md`'s division of labor — not done by
  Claude.
- **8.3 Photon crystals / stat materials:** Engine: consumable permanent-stat-boost items
  (Power/Mind/HP Material, etc.). Editor: material-effect fields on the item editor. UI:
  use-item confirmation + stat-gain feedback. **Not started** — see the addition immediately
  below, which covers a different (instant-recovery) consumable shape first.

  **Ship-readiness gap detail:** still not started. The core mechanism is small — a third
  `ConsumableEffect` value plus a permanent write to `StatsComponent`, on top of the
  `UseItemAction` path that already exists. What it needs beyond that is a per-stat use cap (PSO's
  materials are limited, and uncapped they trivialize the growth curve), and the stat-gain
  feedback this bullet's own UI lens names, which folds into M16.5's feedback pass.
- **Addition: recovery consumables (Monomate/Monofluid) & the use-item action.** Deviates from
  8.3's literal scope (permanent stat-boost materials) — this is the general-purpose "consume an
  item" mechanic plus instant HP/TP recovery items, chosen by the user's explicit direction as the
  next itemization step over 8.3 as originally scoped, since it follows directly from the
  freshly-landed inventory/pickup plumbing (see "M8 follow-ups" above) now having a real consumer.
  8.3's stat-materials scope is unaffected and still open. **Done:** new
  `App/Source/Components/ConsumableComponent.h` (`ConsumableEffect`: `RestoreHp`/`RestoreTp`, flat
  `amount` — same "just needs to round-trip as data for now" precedent Affix's `amount` set),
  registered alongside Weapon/Armor/Mod/Rarity. New `Core/Source/Engine/Combat/HealEvent.h`
  (`IncomingHealEvent`/`AfterHealEvent`) extends `HealthSystem` — still the sole writer of
  `HealthComponent::current_hp` — to handle healing in the same shape `IncomingDamageEvent`
  already does, just clamping up to `max_hp` instead of down to 0; TP restoration stays a direct
  `TPComponent` mutation instead, since (unlike HP) there's no existing "TP system" sole-writer to
  route through. New `App/Source/Actions/UseItemAction.h/.cpp`, an `IAction` sibling of
  `PickupAction`/`DropAction`: consumes the item at a given inventory index, applies its
  `ConsumableComponent` effect, destroys the item entity, and dispatches a new
  `AfterItemUseEvent` (`Core/Source/Engine/Items/ItemUseEvent.h`, sibling of
  `ItemPickupEvent.h`/`ItemDropEvent.h`). UI: fills the `HotbarSlotType::Item` gap
  `HotbarComponent`/`GameplayLayer::TryActivateSlot` had left stubbed since M7.2 (no item id
  space existed until now) — an Item slot's `id` is a consumable prefab NameId, resolved to
  whichever inventory slot currently holds a matching item at activation time, then submitted via
  `TurnCoordinator::SetPendingAction` directly (item use is always self-targeted, so it skips the
  target-select detour Photon Art/Technique slots go through). `CombatLogBridge` gained an
  `OnItemUse` subscriber (log line + HUD HP/TP republish, same shape `OnItemPickup`/`OnItemDrop`
  already use). Editor: a new "Consumable" Inspector card on `PrefabEditorLayer`, following the
  Rarity card's template. Two starter hotbar Item slots (8-9) are bound in
  `GameplayLayer::LoadNewGame` to `"consumables.monomate"`/`"consumables.monofluid"` prefab ids —
  ids only, not authored data; the slots stay inert until matching JSON exists. Content authoring
  (`consumables/monomate.json`, `consumables/monofluid.json`, through the new editor card) is the
  user's own work, per `CLAUDE.md`'s division of labor — not done by Claude. Catch2 coverage in
  `App-Test/Source/ConsumableComponentTests.cpp` (schema shape/authorable flag, `JsonEntityLoader`
  round-trip) and `App-Test/Source/UseItemActionTests.cpp` (HP heal clamped to `max_hp` with
  `AfterHealEvent`'s actually-applied amount, TP restore clamped to `max_tp`, free no-op on a
  missing `InventoryComponent`/out-of-range index/no-`ConsumableComponent` item, item removed from
  inventory and entity destroyed on success, `AfterItemUseEvent` dispatched).

## M9 — Mag Companion

**Status:** Not started

- **9.1 Mag entity & feeding:** Engine: Mag ECS entity, in-field feeding (consumable → Mag),
  evolution/stat-boost accumulation. Editor: Mag species/evolution editor (feed-response
  table, evolution thresholds). UI: Mag status panel, feed-prompt when holding a feedable
  item.

  **Ship-readiness gap detail:** the last wholly-unstarted *designed* system, and the only
  milestone in M1-M11 with no code behind it at all. Much of its plumbing now exists for free: it
  is an entity with components; it feeds on `ConsumableComponent` items through a
  `UseItemAction`-shaped path; its stat contribution can route through `ComputeEffectiveStats`
  the same way equipment already does; its species and evolution thresholds are a content library
  in the established five-file pattern; and its HUD panel is one more `hud.rml` block alongside
  the existing bars. Its genuinely new work is the feed-response table, the evolution rules, and a
  companion entity that follows the player across the turn scheduler *and* across hub-to-dungeon
  swaps — that last part interacts directly with `TransitionToWorld`'s selective entity
  destruction, which today preserves only the player and what is reachable from its inventory,
  equipment, and storage. A Mag must be added to that preserved set explicitly or it is destroyed
  on the first mission launch.

## M10 — Hub, Missions & Difficulty

**Status:** 10.1 implemented, **not yet build/manual-verified** (see its own note below) — this
project builds Windows-only (MSVC + vcpkg x64-windows, per Setup-Windows.bat/vcpkg.json), and the
session that wrote this milestone ran in a Linux sandbox with no Windows toolchain, no vcpkg, and
no network access to fetch entt/rapidjson/Catch2 headers for even a syntax-only check — every
other "Done" entry in this file states a real build/test/manual-run result; this one can't yet,
so it's flagged instead of claimed. 10.2/10.3 not started.

- **10.1 Persistent hub:** Engine: non-procedural hub scene, shop buy/sell, storage,
  mission-select gated by per-character unlocks. Editor: none new (consumes M4/M5.3 data). UI:
  mission-select/shop/storage/character-sheet screens. **Implemented, pending your own Windows
  build + manual verification:** `GameplayLayer::LoadNewGame()` is replaced by
  `SpawnNewCharacter()` (one-time: registry/schema/content-library setup, creates the player and
  its permanent components) + `TransitionToWorld(SceneKind, dungeon_id)` (rebuildable: destroys
  every world entity except the player and everything reachable from its Inventory/Equipment/
  Storage — see `DestroyWorldEntities`, keyed off every entity always carrying
  `EventHandlerComponent` — then either hand-instantiates the hub's single authored
  `DungeonPiece` directly, skipping `DungeonStitcher` entirely, or generates a Dungeon by id).
  Content libraries (`m_pieces`/`m_dungeons`/`m_photon_arts`/`m_techniques`/`m_status_effects`/
  `m_growth_curve`/`m_shop_stock`/`m_hub`) load once for the process lifetime instead of every
  restart, since `m_registry` itself is no longer reset on a scene swap (only individual entities
  are destroyed) — which in turn means every system holding just `Registry&`/`Grid&`/library
  references (not per-dungeon cached data) needed to stop being rebuilt every swap too, to avoid
  leaving a dangling player-`Subscribe()` handler bound to a destroyed instance
  (`EventHandlerComponent` has no unsubscribe-by-instance, only by-owner-type — see
  `TransitionToWorld`'s own doc comment): `CombatLogBridge`/`LootDropSystem`/`ExperienceSystem`/
  `VisualEffectSystem`/`MissFlashEffectSystem`/`OnHitEffectSystem`/`StatusEffectWorldMarkers`/
  `TurnCoordinator` are now lazy-once (built + player-`Subscribe`d exactly once, same idiom
  `EnsureRenderResources` already used for GPU resources); `RoomMap`/`RoomVisibilityTracker`/
  `SpawnWaveSystem`/`EnemyAiSystem`/`ProjectileAdvanceAction`/`TabTargetSystem` still rebuild every
  swap (they hold genuinely per-dungeon data). Two small Core/App defensive fixes fell out of
  this: `VisualEffectSystem::Update`/`StatusEffectWorldMarkers::ClearMarkers` now guard a cached
  entity with `Registry::IsValid` before removing/destroying it, since a scene swap's
  `DestroyWorldEntities` may already have destroyed it out from under them (an in-flight VFX or
  status marker straddling a Hub<->Dungeon transition) — previously latent since a "restart" used
  to wipe the whole registry at once rather than destroying entities individually.

  Mission flow: reaching a Dungeon's `PieceCategory::Exit` piece (checked once per frame via
  `RoomMap::GetRoom`) records the dungeon in a new `Missions::RunProgress` and returns to the hub;
  a new `'H'` keybind abandons a mission without credit; **confirmed with the user:** dying now
  returns to the hub with inventory/equipment/Meseta/level intact (full-heal, no reset) instead of
  the old full-registry wipe, since a full wipe would erase exactly what the hub is supposed to
  protect and real permadeath (M11.2) doesn't exist yet.

  Hub interaction, **confirmed with the user:** Shop/Storage/Mission-Select are not plain
  keybinds — each is a placed hub entity (shopkeeper/storage-terminal/teleprompter, via a new
  `InteractableComponent{InteractionType}` schema-registered component, none carrying
  `BlocksMovementComponent`) that the player walks onto (same tile) and opens with Space. New
  `Hub/HubInteraction.h`'s `FindInteractableAt` (pure `Registry`+`Grid` lookup, unit-tested) backs
  both a per-frame HUD prompt (`HubInteractionPromptMessage`, "Press SPACE to ...") and
  `GameplayLayer::OnEvent`'s Space interception — Space still falls through to `ActionMap`'s
  existing Wait binding everywhere else. `Hub/HubDefinition.h`/`HubDefinitionFile.h/.cpp` load a
  single hand-authored `App/Assets/Data/hub.json` naming which `DungeonPiece` is the hub (no
  dedicated editor, same "not worth one" precedent `growth_curve.json` already set); the hub
  layout itself (`App/Assets/Data/Pieces/hub_main.json`, a 5x5 room) and the three interactable
  entity prefabs (`App/Assets/Data/Entities/hub/{shopkeeper,storage_terminal,teleprompter}.json`)
  are placeholder content authored directly as JSON in this session (not through the Piece/Prefab
  Editor, which wasn't run) — left in place as minimal scaffolding for the user to replace/expand,
  same "throwaway fixture, not real content" carve-out CLAUDE.md already allows.

  Shop: new `Core/Source/Engine/ECS/ValueComponent.h` (an item's sell value, schema-registered —
  gets a Prefab Editor Inspector card automatically) and `Shop/ShopStock.h`/`ShopStockFile.h/.cpp`
  (the hub's fixed buy catalog, `App/Assets/Data/shop_stock.json`, seeded with a 3-entry
  placeholder: Monomate/Monofluid/`weapons.saber`). `Items/Shop.h`'s `BuyItem`/`SellItem` follow
  `Items/Equip.h`'s exact free/instant-mutation convention. New `Editor/Source/Layers/
  ShopStockEditorLayer` (a single always-open screen, no List/browse mode — exactly one document —
  reorderable rows via the existing `FieldWidgets::BuildRowList`, Add/Save/Back), wired into
  `EditorMenuLayer` as a new "Shop Stock" row; `Editor/Build-Editor.lua` gained an
  `App/Source/Shop/**` compile entry for it (mirrors the existing `Items/**` entry).

  Storage: new `Components/StorageComponent.h` (uncapped, unlike `InventoryComponent`) +
  `Items/Storage.h`'s `StoreItem`/`WithdrawItem`, same `Equip.h` convention.

  UI: three new modal screens cloning `CharacterScreenState`/`TechniquesScreenState`'s exact
  push/publish-on-enter/close-on-Escape shape (`MissionSelectState` — single flat list;
  `ShopState`/`StorageState` — two-panel, no context menu) plus matching `HudLayer` overlay blocks
  (row rendering, keyboard nav, `hud.rml`/`hud.rcss` markup) mirroring the existing Techniques
  screen's two-panel-no-context-menu template throughout.

  Unlock-gating seam: `Missions/RunProgress.h`'s `IsDungeonUnlocked` is unconditionally true this
  round (placeholder policy, explicitly unit-tested as such) — M4.5 (fixed area unlock order,
  still not started) extends its body with an area-predecessor check; M10.2 extends it with a
  tier parameter once tiers exist.

  Catch2 coverage (new, **not yet run** — see the status note above):
  `App-Test/Source/{HubInteraction,HubDefinitionFile,RunProgress,MissionSelectSnapshot,
  ShopStockFile,Shop,ShopSnapshot,Storage,StorageSnapshot}Tests.cpp`. `App-Test`/
  `Editor`'s `.lua` build files gained `Hub/`/`Missions/`/`Shop/` compile entries. Content
  authoring beyond the placeholder hub layout/shop catalog above (real hub geometry, real prices,
  M4.5's area-order data) is the user's own work through the editors, per `CLAUDE.md`'s division
  of labor.
- **10.2 Difficulty tiers:** Engine: Normal→Hard→Very Hard→Ultimate data (stat/population/drop
  deltas + M5.3 roster substitutions), per-character clear-gating. Editor: difficulty-tier
  editor (per-area deltas, substitution table). UI: tier selector, clear/unlock indicators.

  **Ship-readiness gap detail:** not started, and it is the other half of what gives a run shape -
  without tiers, every mission after the first is the same difficulty forever, no matter how far
  the character levels. Depends on M3.2 (per-area deltas need an area definition to hang off) and
  on M5.3 / M17.4 for roster substitution. `IsDungeonUnlocked`'s doc comment already anticipates
  the tier parameter. Its UI lens extends the existing Mission Select screen rather than adding a
  new one.
- **10.3 Character creation:** Engine: class lock (Hunter/Ranger/Force) + Section ID chosen at
  creation, persisted for the run. Editor: none (player-facing flow, not content data). UI:
  character-creation screen.

  **Ship-readiness gap detail: promoted to a blocker.** Today `GameplayLayer::SpawnNewCharacter`
  creates one hardcoded player from `player.json`, hardcodes `SectionId::Viridia`, and assigns no
  class at all. Two of the GDD's three central identity choices therefore do not exist in the
  game, and the class triad that the entire weapon, Photon Art, and Technique design rests on is
  unrepresented in code: there is no class component anywhere, and M11.1's growth curve was
  deliberately built class-agnostic precisely because there was nothing to key it on. This bullet
  needs a class component and its per-class starting kit, per-class growth curves replacing the
  single `growth_curve.json`, a Section ID choice that actually affects something, and the
  creation screen itself — which attaches to M14.1's title-screen "New Character" row, not to
  `OnAttach`.

## M11 — Progression & Permadeath

**Status:** 11.1 done, landed uncommitted alongside a session boundary and captured here
retroactively — the same lag the M6/M7/M8 follow-up sections already had. 11.2 not started.

- **11.1 XP & leveling:** Engine: combat XP, per-class stat growth curves. Editor:
  growth-curve editor. UI: level-up notification + stat-delta display. **Done:** new
  `ExperienceValueComponent` (`App/Source/Components/`, single `xp` field) is authored directly on
  enemy prefabs, same bespoke no-separate-library shape `DropTableComponent` already set — an
  entity without one (breakable boxes included) simply grants no XP. `LevelComponent`
  (`{level, xp}`, deliberately not schema-registered — never hand-authored, same "runtime-only
  player state" precedent `TabTargetComponent` set) is emplaced on the player at spawn. New
  `App/Source/Progression/GrowthCurve.h/.cpp` + `GrowthCurveFile.h/.cpp` load one **class-agnostic**
  curve (per the user's explicit deviation from this bullet's literal "per-class" wording — no
  character classes exist yet, per M10.3's own not-started status, so there's nothing to key a
  per-class curve on) from a single hand-authored `App/Assets/Data/growth_curve.json` — a plain
  file load, not a per-item content library, so **no dedicated growth-curve editor** was built
  (deviates from this bullet's literal editor wording; revisit once M10.3 gives classes a reason to
  split the curve). Each authored level holds absolute `max_hp`/`max_tp`/`StatsComponent` totals
  (not incremental deltas), so applying a level-up is a plain overwrite. New
  `App/Source/Systems/ExperienceSystem` (mirrors `LootDropSystem`'s shape: subscribed only on the
  player, since `AfterDamageEvent` is dispatched at the attacker) banks XP on a kill, loops through
  every level-up a single kill's XP crosses (carrying remainder XP forward), and fully restores
  current HP/TP on each level gained. Follow-up: `ApplyLevelUp` now diffs each stat against its
  pre-overwrite value and returns the gains as inline log markup (e.g. "HP +20, ATP +3", omitting
  anything that didn't move), appended to a "Player reached level N!" combat-log line; a single
  multi-level kill spawns exactly one "LEVEL UP!" floating text via `FloatingTextSystem` regardless
  of how many levels it crossed. UI: the HUD's always-visible bars panel shows `Lv<N>` + the
  player's name in place of the standalone Meseta counter (Meseta itself is unaffected, still
  tracked/shown elsewhere). Separately, `TPComponent` — wired since M7.2 but dormant because the
  player had no such component at all — is now emplaced on the player at spawn, lighting up
  everything that was already built against it (Technique/Photon-Art TP costs, `RestoreTp`
  consumables, and this milestone's own TP-scaling level-up data) plus a previously-always-hidden
  HUD TP bar; a new Monofluid item (a `RestoreTp` mirror of Monomate) fixes a previously-inert
  starter hotbar slot. Catch2 coverage in `App-Test/Source/ExperienceSystemTests.cpp` (XP banking,
  multi-level-up carrying remainder XP, stat/HP/TP overwrite-and-restore, the diffed gain-markup
  string, single floating text across a multi-level kill, no-op on a defeated entity with no
  `ExperienceValueComponent`) and new `GrowthCurveFileTests.cpp`/`GrowthCurveTests.cpp`. Content
  authoring (real per-enemy XP values beyond `booma.json`'s existing reference, and any further
  tuning of `growth_curve.json` beyond its current placeholder curve) is the user's own work, per
  `CLAUDE.md`'s division of labor.
- **11.2 Run persistence & permadeath:** Engine: `cereal`-backed save of in-run state between
  missions; permadeath wipes on death; full reset on new character (no meta-progression, per
  GDD). Editor: none. UI: death/run-summary screen, new-character flow.

  **Ship-readiness gap detail: promoted to a blocker, and considerably larger than this bullet
  makes it sound.** As written it reads as one serialization pass. In practice it is the widest
  cross-cutting change left in the project, because the thing being saved is a live `entt`
  registry rather than a plain struct.

  What must round-trip: the player entity and every component on it; the contents of
  `InventoryComponent`, `EquipmentComponent`, and `StorageComponent`, all three of which hold raw
  `entt::entity` handles that are meaningless across a process boundary and must be remapped;
  `KnownTechniquesComponent`, `HotbarComponent`, `LevelComponent`, `CurrencyComponent`,
  `SectionIdComponent`, `StatusEffectComponent`; and `Missions::RunProgress`, which is today an
  in-memory `unordered_set` carrying a doc comment naming this milestone as its storage.

  Decide the save-point scope explicitly and early, because it is the difference between a modest
  milestone and a very large one. A hub-only save point is the simpler and defensible choice:
  dungeons are non-persistent by design per the GDD, so saving on return to the hub avoids
  serializing the `Grid`, the `RoomMap`, the `RoomVisibilityTracker`, the `TurnQueue`, in-flight
  tweens, live projectiles, and pending spawn waves entirely.

  Three structural problems to solve before writing any `cereal` code:

  — **Entity handle stability.** Every cross-entity reference in the project is a raw
    `entt::entity`. Saving needs a stable identity — a persistent id component, or a save-time
    remap table — plus a load-time fixup pass.
  — **Nothing is registered for serialization.** The `entt::meta` reflection built in M1.2 covers
    *authorable* prefab fields only, and the components that matter most here
    (`InventoryComponent`, `EquipmentComponent`, `StorageComponent`, `HotbarComponent`,
    `LevelComponent`) are all deliberately not meta-registered. A second, separate persistence
    registration path is needed, and it has to be kept honest: a component added later and not
    registered fails silently by dropping player data.
  — **Versioning.** Saves must survive content and schema changes, or every update destroys every
    character. Version the format in the first commit, not later.

  Permadeath itself is then a policy layer on top. Death currently returns the player to the hub
  fully intact — a deliberate M10.1 decision, made because a full wipe would erase exactly what
  the hub exists to protect and because real permadeath did not exist yet. Making it real means
  deleting the save on death, and that demands the surrounding safety rails: an unambiguous death
  screen, a run summary, save-file integrity checking with a defined behavior on corruption,
  crash-safe writes (write to a temp file, then rename), and a deliberate decision about whether
  quitting mid-mission is allowed to be a save-scum. Also in scope: autosave on hub return and on
  mission completion, multiple character slots or an explicit single-slot decision, and a
  "Continue" row on the title screen that knows whether a save exists (M14.1).

---

# Phase C milestones — ship readiness

Everything below was produced by the ship-readiness analysis rather than the original
system-by-system plan, so each bullet states the **evidence** for the gap (what was checked and
what was found) before stating the work. Each keeps this file's existing three-lens
**Engine** / **UI** / **Editor** format, and each says explicitly when a lens is genuinely empty.

## M12 — Audio

**Status:** Not started. **Severity: blocker.**

**Evidence:** there is no audio code anywhere in the repository. `vcpkg.json` lists SDL3,
SDL3-image, FreeType, RmlUi, EnTT, rapidjson, and glslang — no mixer, no decoder, no audio
feature on the SDL3 entry. A case-insensitive search across `Core`, `App`, and `Editor` for
`audio`, `sound`, `music`, `mixer`, and the SDL audio entry points returns nothing. This is not a
partially-built system; it is the one major subsystem with zero foundation, and it is the most
visible thing missing from a build a player would try.

- **12.1 Audio engine & mixing.** Engine: add an audio backend to `vcpkg.json` and initialize a
  device in `Application::Initialize` alongside the window and renderer, torn down in
  `Shutdown()` in the same explicitly-ordered way RmlUi already is. A new
  `Core/Source/Engine/Audio/AudioEngine` owns the device, a decoded-clip cache keyed the same way
  `TextureAtlas` keys textures (filename stem hashed to a `NameId`), and a small bus model —
  master, music, SFX, UI — so a volume slider has something to move. Needs one-shot SFX playback
  with a voice cap and a same-frame duplicate-collapse rule (a four-hits-per-turn Mechgun must not
  play four fully overlapping samples at full gain), looping music with crossfade between tracks,
  and 2D panning/attenuation derived from a sound's tile distance to the camera centre so an
  off-screen enemy dying is quieter than an adjacent one. Playback must be non-blocking and must
  never be driven from the turn loop directly. UI: none itself. Editor: none itself.
- **12.2 Sound as authored content.** Engine: a `Sound` bespoke content type following the exact
  five-file family pattern (`Sound`/`SoundSchema`/`SoundSchemaEmitter`/`SoundLibrary`/
  `SoundLibraryFile`) that `Affix`, `PhotonArt`, `Technique`, and `StatusEffect` all already use —
  clip reference, gain, pitch-randomization range, bus, and a looping flag — so a sound is
  referenced by `NameId` from content rather than by a hardcoded path in C++. Alongside it, an
  `AudioEmitterComponent` for ambient loops placed into pieces (a humming terminal, a dripping
  cave), which is what makes an area feel authored rather than silent.
  Editor: a `BuildSoundField` widget, sibling to the existing `BuildTextureField`, backed by a
  `SoundPickerPopup` that rescans a new `EditorFilepaths::SoundsPath` — the `TexturePickerPopup`
  already establishes the whole pattern, including the rescan-on-`Open()` behavior. Then sound
  reference fields on the cards that need them: Weapon (swing, hit, miss), Technique and Photon
  Art (cast, impact), StatusEffect (apply, tick, expire), Consumable (use), Prefab
  (spawn, hurt, death, footstep), and Piece (ambient emitters). A new `SoundEditorLayer` row for
  the `Sound` library itself, mirroring `AffixEditorLayer`'s List/Edit shell.
- **12.3 Wiring a cue to every existing feedback moment.** Engine: this project already dispatches
  a well-factored event for nearly everything worth hearing — `AfterDamageEvent`,
  `AfterHealEvent`, `AfterItemPickupEvent`, `AfterItemDropEvent`, `AfterItemUseEvent`,
  `AfterStatusEffectsChangedEvent`, `AfterTurnEvent`, `MoveEvent`, `DeathEvent`,
  `TechniqueCastEvent`, `PhotonArtCastEvent`, plus the `MesetaChangedMessage` /
  `LootDropMessage` / `MissionCompletedMessage` / `PlayerDefeatedMessage` bus traffic. An
  `AudioBridge` system subscribing to those, mirroring `CombatLogBridge`'s shape almost exactly,
  is the right seam — one system, no scattered mixer calls, and it inherits `CombatLogBridge`'s
  existing lazy-once construction so a hub-to-dungeon swap does not leave a dangling handler.
  UI: every menu needs move, confirm, cancel, and denied cues, and every currently-silent visual
  feedback moment needs a paired audible one — a landed hit, a miss (the `MissFlashEffectSystem`
  flash is currently the only signal), a level-up, a purchase, a rejected purchase, an
  out-of-range cast, a full inventory. A denied action that is *silently* denied is the single
  most common source of "the game is broken" reports.
- **12.4 Music.** Engine: per-scene track selection with crossfade on the hub-to-dungeon
  transition (`TransitionToWorld` is the one call site), plus a combat-vs-explore intensity swap
  if wanted later. Editor: a music track field on the M3.2 area schema, and on `hub.json`. UI:
  music and SFX sliders, which land with M15's options screen.

## M13 — Game feel & visual presentation

**Status:** Not started, though several of its foundations exist. **Severity: medium-high.**

**Evidence:** the moving parts are unusually good already — `TweenSystem` with easing,
`AnimationClock` for sprite strips, `FloatingTextSystem`, `DamageTextSystem`,
`MissFlashEffectSystem`, `VisualEffectSystem`, `OnHitEffectSystem`, camera zoom, and smooth
camera follow (recorded as fixed in `issues_and_bugs.md`). What is missing is the layer that
turns those primitives into readable, weighty feedback. Concretely: `RenderableComponent` has
`texture_id`, `texture_size`, `uv`, two colors, `render_layer`, `frames`, and `frame_time` — and
no facing, no per-state clip selection. A search for `facing` or `direction` across
`App/Source/Components` returns only unrelated comments. So every entity faces the same way
forever, and a sprite has exactly one animation regardless of whether it is idle, walking,
attacking, hurt, or dying.

- **13.1 Facing & animation states.** Engine: a `FacingComponent` (or a facing field on
  `RenderableComponent`) written by `MoveAction` and by every targeted action's resolved
  direction, plus a small named-clip table on the renderable — idle / walk / attack / hurt /
  death, each a row offset and frame count into the same strip — so `AnimationClock` picks a row
  as well as a column. This is the difference between a turn-based game reading as animated and
  reading as a spreadsheet with sprites. Editor: clip rows on the Prefab Editor's Renderable
  card, previewed live in the existing preview canvas.
- **13.2 Impact weight.** Engine: hit-stop (a few frames of suspended tween advance on a landed
  hit, which `AnimationState` is already the natural owner of, since it already gates the turn
  loop on tween completion), a short directional knockback or shake on the struck entity, and a
  camera shake with a magnitude scaled by damage relative to max HP. Screen-space flash on player
  damage. All of these are cheap and all of them are the difference between "the number changed"
  and "that hurt." Editor: shake and hit-stop magnitudes belong on the Weapon / Technique /
  Photon Art cards as authored values, not constants in C++.
- **13.3 Death & spawn presentation.** Engine: `DeathSystem` currently destroys an entity
  outright. A death animation clip, a dissolve or fade, and a brief corpse or scorch decal give
  a kill a beat. Spawn waves appearing instantly is likewise jarring — a telegraphed spawn (a
  marker tile for one turn, then the enemy) is both better feel and better fairness.
- **13.4 Scene transitions.** Engine: `TransitionToWorld` swaps the hub and a dungeon in a single
  frame with no visual break, which reads as a glitch rather than a transition. A fade-out,
  swap, fade-in — with the fade owned by a thin overlay `Layer` so it composes over both RmlUi
  and the tile pass — plus an area-name title card on dungeon entry. UI: the title card and the
  fade are the whole surface. Editor: area display name comes from the M3.2 area schema.
- **13.5 UI motion.** UI: the six modal overlays currently appear and disappear instantly. RmlUi
  supports transitions and animations in RCSS; a consistent open/close treatment (a short fade
  and scale) applied once in `hud.rcss` covers every screen at once. Same for the HP bar, which
  should ease to its new value rather than snap, and for the log's newest line.

## M14 — Front end & session flow

**Status:** Not started. **Severity: blocker.**

**Evidence:** `App/Source/main.cpp` pushes `GameplayLayer` directly and calls `Run()`. There is no
title screen, no menu layer, and no state before gameplay. Worse, `Application::OnKeyPressed`
handles `SDLK_ESCAPE` by calling `RequestQuit()` — so from the Exploring state, where no layer
consumes Escape, pressing it closes the game instantly, with no confirmation and (since M11.2 has
not started) no save. That single line is the most severe player-facing defect in the project.
There is also no pause: closing a modal returns straight to the turn loop, and there is no way to
stop playing other than quitting.

- **14.1 Title screen & app state machine.** Engine: `main.cpp` should push a `MainMenuLayer`
  rather than `GameplayLayer`, with the existing `Application::TransitionTo` doing the swap — the
  mechanism is already built and already used by the Editor's own menu. An app-level state
  distinction (Title / Playing / Paused) above the existing in-game `GameStateMachine`. UI: title
  art, the game name, and rows for Continue (disabled without a save), New Character, Options,
  Credits, and Quit — the `EditorMenuLayer` keyboard-navigable row shell is a direct model.
  Editor: none; this is a player-facing flow, the same call M10.3 already makes.
- **14.2 Pause menu & escape hierarchy.** Engine: remove the unconditional Escape-quits handler
  from `Application::OnKeyPressed` and replace it with a proper hierarchy — Escape closes the
  topmost modal if one is open, otherwise cancels targeting, otherwise opens the pause menu, and
  only the pause menu's own Quit row ends the process. `Application::RequestQuit` stays, but
  nothing reaches it without passing a confirmation. The pause menu itself is a
  `GameStateMachine` state, identical in shape to `CharacterScreenState`, which already suspends
  the turn loop by sitting on top of the stack. UI: a `#pause-screen` overlay in `hud.rml`
  following the six existing overlays' convention, with Resume, Options, Help, Abandon Mission
  (folding in the current bare `H` keybind, which today abandons with no confirmation), Quit to
  Title, and Quit to Desktop. Every destructive row needs a confirm step.
- **14.3 Confirmation & destructive-action guards.** Engine: a reusable confirm-prompt sub-state,
  since several existing flows currently destroy things silently — abandoning a mission (`H`),
  dropping an item, selling an item, and eventually deleting a save. UI: one shared confirm
  widget rather than a bespoke one per screen.
- **14.4 New-character flow.** Engine: this is where M10.3's character creation actually attaches
  — the title's New Character row leads into class and Section ID selection, then into
  `SpawnNewCharacter`. Today `SpawnNewCharacter` is called unconditionally on attach with a
  hardcoded `player.json`, `Viridia`, and no class. See M10.3's own expanded entry.
- **14.5 Loading & first-frame cost.** Engine: `GameplayLayer::OnAttach` loads every content
  library, generates a dungeon, instantiates it, and builds GPU resources synchronously. That is
  fine at today's content volume and will not stay fine. A loading state with a visible indicator,
  and content-library loading hoisted out of the gameplay layer into a process-lifetime content
  service that the title screen can warm up behind its own art. UI: a loading screen.
- **14.6 Credits & attribution.** UI: a credits screen listing SDL3, RmlUi, EnTT, rapidjson,
  Catch2, FreeType, glslang, cereal, the font, and every asset licence — this is a legal
  requirement of shipping, not a nicety, and the README's existing PSO trademark note belongs in
  the build too, not only in the repository.

## M15 — Input, options & accessibility

**Status:** Not started. **Severity: high.**

**Evidence:** `App/Source/Content/KeyBindings.cpp` hardcodes every binding at construction —
arrows and numpad for movement, Space to wait, `G` to pick up. Another eight keys are intercepted
directly inside `GameplayLayer::OnEvent` rather than going through `ActionMap` at all (`C`, `T`,
`Tab`, `Escape`, `H`, `Space` in the hub, numpad `+`/`-`, and the digit row). Nothing reads a
config file; nothing can be changed by a player. There is no gamepad code anywhere — searches for
`gamepad`, `joystick`, and `controller` return nothing. Mouse support exists only where RmlUi
provides it for free, so the world itself is keyboard-only: no click-to-move, no click-to-target,
no scroll-to-zoom. And there is no options screen of any kind, no resolution or fullscreen
control, and no persisted settings.

- **15.1 Rebindable input.** Engine: consolidate the split — everything the player can press
  should be a named action in one table, including the eight keys currently special-cased inside
  `GameplayLayer::OnEvent`, so a rebinding screen has a single source of truth. `ActionMap<int>`
  already keys on an `int` keycode and is generic enough; what is missing is the layer above it
  mapping *semantic action* to *binding*, plus conflict detection and a restore-defaults path.
  UI: a rebinding screen with press-to-bind capture. Editor: none — bindings are player settings,
  not authored content, so they belong in a settings file rather than in `App/Assets/Data`.
- **15.2 Gamepad support.** Engine: SDL3's gamepad API, mapped onto the same semantic action
  table, with the analog stick driving the existing `InputBuffer` DAS repeat and the d-pad driving
  discrete steps. Every modal screen already has keyboard navigation, so gamepad navigation is
  mostly a matter of feeding the same events. UI: on-screen glyphs that switch between keyboard
  and gamepad labels based on the last-used device — the hint lines in `hud.rml` currently
  hardcode "Numpad to navigate, Space to select, Esc to close" in six places, which is both a
  gamepad problem and a rebinding problem, since those strings lie the moment anything is rebound.
  Those hints should be generated from the live binding table.
- **15.3 Mouse in the world.** Engine: click-to-move with pathfinding, click-to-target, hover to
  inspect a tile, and scroll-to-zoom. Note that pathfinding does not exist anywhere yet — the only
  navigation logic in the project is `EnemyAiSystem`'s single-step cardinal snap with one
  perpendicular retry. A real A* or Dijkstra pass over the `Grid` is a prerequisite here, and it
  is shared with M17's smarter AI, so build it once in `Core` as a grid-navigation utility.
- **15.4 Options screen & settings persistence.** Engine: a settings file in the platform's user
  data directory (`SDL_GetPrefPath`), covering display mode, resolution, vsync, frame cap, UI
  scale, master/music/SFX/UI volumes, bindings, and accessibility toggles — written on change,
  loaded before the window is created so the window opens at the right size. `ApplicationInitContext`
  currently carries only a title and a fixed 1280x720, and nothing reads a config. UI: the options
  screen, reachable from both the title screen and the pause menu, and identical in both.
- **15.5 Display & window.** Engine: fullscreen, borderless, and windowed modes with a toggle
  binding; resolution selection; and correct handling of the resize path that already exists
  (`OnWindowResize` updates the RmlUi context dimensions but nothing re-derives the viewport or
  UI scale). A UI scale factor matters more than usual here because the HUD is a fixed-pixel RmlUi
  layout and this is a tile game that people will run at 4K.
- **15.6 Accessibility.** Engine and UI: this project's rendering makes several of these unusually
  cheap, and skipping them is a real exclusion, not a polish item.
  — **Color:** every renderable already carries `color_1`/`color_2` for palette-swap shading, and
    combat-log markup is color-coded. Colorblind-safe palette options and a rule that color is
    never the *only* carrier of meaning (status chips need shapes or letters, not just tints).
  — **Text:** a UI scale and font-size setting; the HUD's fixed sizes are currently unadjustable.
  — **Motion:** a reduced-motion toggle disabling camera shake, screen flash, and tween easing —
    which matters most for the very effects M13 adds.
  — **Timing:** turn-based play is inherently forgiving here, but held-key DAS repeat speed should
    be adjustable, and no prompt should be timed.
  — **Remapping** is itself an accessibility feature, covered by 15.1.
  — **Audio:** subtitles or a visual indicator for any audio-only cue, which is a constraint on
    M12's design rather than separate work.

## M16 — UI polish & information architecture

**Status:** Not started. **Severity: high.**

**Evidence:** the HUD is further along than most of this list — bars, hotbar, target panel,
status chips, a markup-capable scrolling log, and six modal screens with keyboard navigation.
The gaps are in what the screens *tell* the player. Searches for `tooltip` and `compare` across
`App` and `Editor` return nothing, so an item's only representation anywhere is the decorated
display-name string built by `CharacterScreenSnapshot` — a player can equip a weapon without ever
seeing its ATP, range shape, hits per turn, element, or affix effect. `InventoryComponent` is a
flat `vector<entt::entity>` with `capacity = 20` and no stacking, so ten Monomates occupy ten of
those twenty slots. There is no minimap anywhere (searches for `minimap` and `world_map` return
nothing) in a game built on procedurally generated multi-room dungeons with fog of war. There is
no turn-order indicator, which M6.1's own bullet named as its UI deliverable and which was
deferred and never revisited. And every user-visible string is hardcoded in C++ or in `hud.rml`.

- **16.1 Item information.** UI: a detail panel or tooltip showing an item's real numbers, and a
  side-by-side comparison against the currently equipped item in that slot when browsing
  inventory — the single highest-value UI addition available, since the entire itemization
  milestone is currently invisible to the player. All the data already exists;
  `ComputeEffectiveStats` already computes exactly the deltas a comparison needs. Also: rarity
  coloring on names, an unidentified-item treatment if the GDD's optional tekking mechanic is ever
  taken up, and an icon per item rather than text-only rows.
- **16.2 Inventory usability.** Engine: stackable consumables (a count on the entry rather than
  one entity per unit) — twenty slots divided among Monomates is a hard usability wall, and it
  gets worse the moment drop tables are tuned. Sorting and filtering by type; a "new item" marker;
  a full-inventory warning at pickup time rather than a silent failure. UI: the Character screen's
  inventory panel. Editor: a stack-size field on the Consumable card.
- **16.3 Spatial awareness.** Engine and UI: a minimap or full map screen driven by the data that
  already exists — `RoomMap` maps every tile to its placed piece, `RoomVisibilityTracker` already
  tracks Hidden/Explored/Visible per room, and `DungeonLayout` knows the entrance, the exit, and
  which connections are locked. Rendering that as a room graph is a small amount of work on top of
  systems that are already built and tested, and without it a player has no way to know where the
  exit is or which rooms they have not visited. Also worth surfacing: an off-screen indicator for
  the tab-target, and a compass or exit direction hint.
- **16.4 Turn and combat readability.** UI: the turn-order indicator M6.1 promised and never
  delivered — with variable action costs and speed-scaled `ActionCost`, the player currently has
  no way to know who acts next or how a slow weapon changes that. Also: a movement-range or
  valid-move highlight (M6.2's own deferred UI bullet), a threat overlay showing which tiles are
  covered by an enemy's reach, an accuracy or damage estimate on the target panel, and a clearer
  attack-vs-move distinction when bumping.
- **16.5 Feedback for failure.** UI: the most common bug reports on games like this are actions
  that silently do nothing. Today, an unaffordable cast, an unlearned Technique, an out-of-range
  target, a full inventory, an equip attempt on a non-equippable item, and a hotbar activation
  with no matching inventory item all resolve as free no-ops with no message. Each needs a log
  line, a sound, and where appropriate a HUD flash. This is cheap and it is the difference between
  a game feeling broken and feeling strict.
- **16.6 Onboarding.** UI: a controls and help screen reachable from the pause menu, generated
  from the live binding table so it cannot go stale; contextual first-time hints for the hub
  interactables, the Character screen, hotbar assignment, and targeting; and a glossary for the
  PSO vocabulary the game uses without explaining (Meseta, Section ID, Photon Art, Technique
  Disk, Mag, ATP/ATA/MST/DFP/EVP/LCK). None of the stat abbreviations are expanded anywhere in the
  UI today.
- **16.7 Localization seam.** Engine: every player-visible string is currently a literal in C++ or
  in `hud.rml`, including the six hint lines, every combat-log format string built by
  `CombatLogBridge`, every screen title, and every enum display name. Retrofitting a string table
  later is far more expensive than routing through one now, even if only English ever ships —
  and the same indirection is what lets 15.2's binding-aware hint lines work. Also in scope:
  RmlUi font coverage for any non-Latin target, and number and date formatting.
- **16.8 Visual consistency pass.** UI: the six modal overlays grew one at a time by cloning each
  other, and `hud.rcss` is now 642 lines. A pass to unify spacing, panel chrome, focus treatment,
  hint-line placement, and the two different navigation idioms currently in use (context-menu
  rows on the Character screen, direct two-panel activation on Shop, Storage, and Techniques).
  Also: a consistent modal-close contract, and making sure every screen states its own controls.

## M17 — Encounter & simulation depth

**Status:** Not started. **Severity: high.** Distinct from content and balance: everything below
is a missing *mechanism*, not a missing number or a missing enemy.

**Evidence:** `AiBehavior` has exactly one value, `ChaseAndAttack`, and `EnemyAiSystem::Decide`
issues only `MoveAction` — an enemy's only way to attack is `MoveAction`'s bump-into-hostile
fallback. That means no enemy can use a ranged weapon, cast a Technique, use a Photon Art, apply
a status effect deliberately, flee, patrol, guard, or support another enemy, regardless of what
its prefab carries. `PieceCategory` includes `BossArena`, but nothing anywhere consumes it: there
is no boss component, no boss health bar, no arena lock, no phase system, and mission completion
is triggered by walking onto an `Exit` piece rather than by defeating anything.
`Combat/Hostility.h` is a two-line placeholder declaring the player hostile to every other entity
and vice versa, which makes friendly or neutral entities impossible — including the hub NPCs,
which are only safe today because they carry no `HealthComponent`. And M5.3's rare-enemy and
tier-reskin mechanics were never started.

- **17.1 AI behavior library.** Engine: `AiBehavior` needs to become a real set — ranged attacker
  (kite to preferred distance, fire), caster (spend TP on Techniques with the existing target
  resolution), support (heal or buff allies), skirmisher (attack then withdraw), guard (hold a
  tile until provoked), patrol (a route authored on the piece), and flee-at-low-HP. The action
  layer these need is already complete and already unit-tested — `AttackAction`, `TechniqueAction`,
  `PhotonArtAction`, and `UseItemAction` are all usable by any entity, not just the player, so
  this is decision-making work rather than new mechanics. It does need grid pathfinding
  (see 15.3) and a line-of-sight query, neither of which exists. Editor: behavior selection and
  per-behavior parameters on the Prefab Editor's AI card, which today has only `behavior` and
  `detection_range`.
- **17.2 Faction & hostility.** Engine: replace `IsHostile`'s placeholder with a real faction
  component and relationship table, so neutral wildlife, friendly NPCs, summoned allies, and
  enemy-infighting are all expressible. This also removes a latent trap — any hub NPC that ever
  gains a `HealthComponent` becomes attackable today. Editor: a faction field on the Prefab
  Editor.
- **17.3 Boss encounter framework.** Engine: a `BossComponent`, arena entry and exit locking
  (the `BossArena` piece category is already authored and already reaches the stitcher), an HP
  threshold phase system with per-phase behavior and ability sets, telegraphed multi-turn attacks,
  add-summoning that reuses the existing `SpawnWaveSystem`, and mission completion on boss defeat
  as an alternative to reaching an `Exit` piece. Depends on M5.4 (multi-tile entities &
  footprints) for bosses that are meant to visually and mechanically occupy more than one tile —
  without it a boss is just a reskinned regular enemy with more HP. UI: a boss health bar distinct
  from the target panel, a boss name and phase callout, and an arena-locked indicator. Editor:
  phase and ability authoring on the Prefab Editor, and a boss field on the Dungeon schema naming
  which piece and which boss end the mission.
- **17.4 Rare enemies & tier reskins (M5.3, restated here because it is a mechanism, not
  content).** Engine: a rare-variant roll at spawn time applying an alternate palette, a stat
  multiplier, and a guaranteed drop override; and a roster-substitution table so a higher
  difficulty swaps an enemy for a tougher counterpart rather than only inflating its numbers.
  The spawn path (`DungeonInstantiator` plus `SpawnWaveSystem`) is where the roll belongs, and
  `RenderableComponent`'s existing two-color palette swap already makes the alternate palette
  nearly free. UI: a rare-enemy visual and audible callout on spawn — the whole point of the
  mechanic is that the player notices. Editor: rare-variant and substitution fields on the Prefab
  Editor.
- **17.5 Population & spawn weighting.** Engine: M5.2 explicitly deferred a spawn-weight field
  because nothing consumed it. That is still true — every spawn is hand-placed on a piece — which
  means enemy variety per mission is fixed by piece authoring rather than rolled per run. A
  weighted per-area spawn table, rolled at instantiation against the area's race and difficulty
  tier, is what makes a generated dungeon feel generated. This is the natural consumer M5.2 was
  waiting for, and it depends on M3.2's area schema.
- **17.6 Environmental interaction.** Engine: the GDD names a per-area hazard type, and M3.2's
  schema bullet lists it, but no hazard mechanic exists — no traps, no damaging tiles, no doors
  beyond the abstract lock/key annotation the stitcher records, no destructible terrain beyond the
  breakable boxes. The lock/key system in particular is worth finishing: M4.3 deliberately left it
  as a verified-solvable annotation on the layout with no in-world entities, and items and
  interaction now both exist, so it can be made real.

## M18 — Release engineering & distribution

**Status:** Not started. **Severity: blocker.** Start this early and run it continuously — it is
listed last only because its final gates run last.

**Evidence:** there is no `.github` directory, so nothing builds or tests automatically. The
build is Windows-only and Visual-Studio-only by construction (a vendored premake `vs2026` binary,
an `x64-windows` vcpkg triplet hardcoded in `Build.lua`, and PowerShell and batch scripts as the
only entry points), which has already cost this project real work — M10.1 is recorded in this very
file as implemented but never built or verified, because the session that wrote it had no Windows
toolchain. `issues_and_bugs.md` records three App-Test failures confirmed present on `master`.
`Application::Run` computes a delta time from the performance counter but never calls
`SDL_SetRenderVSync` and imposes no frame cap, so the loop spins as fast as the GPU allows. There
is no packaging step, no installer, no version string, no crash handler, and logging is
`SDL_Log` to stdout with no file sink.

- **18.1 Continuous integration.** A workflow building `Core`, `App`, `Editor`, `Core-Test`, and
  `App-Test` on every push, running both Catch2 suites, and running `Run-ClangFormat.ps1 -Check`
  and `Run-ClangTidy.ps1` — both scripts already exist and are documented as manual-only, which
  is why nothing enforces them. Fix the three known `App-Test` failures first so the suite can be
  a gate rather than a warning: `TweenSystemTests.cpp:86` (a float-precision comparison that
  should use a tolerance), `MoveActionTests.cpp:174` (bump-to-attack queues four tweens where two
  are expected), and `CombatLogBridgeTests.cpp:106` (a lethal hit publishes one log line where a
  hit-plus-defeat pair is expected). The second and third are behavioral and may be real defects
  rather than stale expectations.
- **18.2 Build portability.** The Windows-only constraint is the single biggest tax on this
  project's own development, independent of whether Linux or macOS is ever a shipping target.
  Every dependency in `vcpkg.json` is cross-platform; the blockers are the vendored premake
  binary, the hardcoded triplet, and the PowerShell-only scripts. Even a headless
  compile-and-test configuration on Linux would have caught M10.1's unverified state.
- **18.3 Packaging & distribution.** A `Dist` configuration that produces a self-contained,
  runnable folder — executable, required runtime libraries, `Assets/`, compiled shaders, and
  licence files — with the asset copy done as a real install step rather than the current
  postbuild copy into `Binaries/`. Then an installer or archive, an application icon (the window
  has none), correct executable metadata, and a decision about whether the Editor ships alongside
  the game or stays a development tool. Also: a version string, surfaced in the title screen and
  in logs, and a place to record it.
- **18.4 Crash handling & diagnostics.** `main.cpp` catches `std::exception` around `Run()` and
  logs it, which is good but only covers the exception path. Needs a structured logger with a
  file sink in the user data directory, log rotation, a crash handler capturing a stack trace, an
  opt-in crash report (never automatic, never silent), and the build metadata to make a report
  actionable. Content-load failures deliberately throw loudly, per this file's own convention —
  that is correct for development and needs a player-facing error dialog for a shipped build,
  because "the process exited with code 1" is not a bug report.
- **18.5 Performance & frame pacing.** Set vsync or an explicit frame cap — an uncapped loop on a
  turn-based tile game pins a GPU for no reason and is the kind of thing that gets a build called
  broken on a laptop. Then measure: per-frame draw-call count as room count grows, `TextureAtlas`
  packing behavior with a real asset volume, generation time for large dungeons, memory growth
  across many hub-to-dungeon swaps (`TransitionToWorld`'s selective entity destruction is exactly
  the shape that leaks), and RmlUi layout cost with a full inventory. A simple in-build overlay for
  frame time, entity count, and draw calls pays for itself.
- **18.6 Robustness under bad input.** Content-load failures throwing is right; a *shipped* build
  should also survive a corrupt save, a partially-written settings file, a missing asset, a
  dungeon that fails to generate, and a display mode the hardware rejects. Each of those wants a
  defined fallback, and the generation path in particular needs a retry-with-a-new-seed rather
  than a hard failure.
- **18.7 Legal & compliance.** Third-party licence texts bundled in the build (SDL3, RmlUi, EnTT,
  rapidjson, Catch2, FreeType, glslang, cereal, the vendored RmlUi SDL backend, PixelCode, and
  every art and audio asset). Confirm the licence on every asset actually used. The README's PSO
  trademark note belongs in the shipped credits too. If the project ever moves past
  private and non-commercial, that note needs a real review, not a footnote.
- **18.8 Privacy.** No telemetry exists today, which is the correct default. If any is ever added,
  it must be opt-in and disclosed. A crash reporter counts as telemetry.

## Ship checklist

A condensed gate list, ordered by what a stranger installing the build would hit first. Content
and balance are deliberately excluded throughout, per this analysis's scope. Nothing here is new
scope — every row points at a bullet above.

**Blocking — a build cannot ship without these**

- [ ] Escape no longer quits the game from gameplay (14.2)
- [ ] Title screen, pause menu, and a confirmed quit path (14.1, 14.2, 14.3)
- [ ] Save and load, so closing the window does not destroy the character (11.2)
- [ ] Audio: an engine, a content type, and a cue on every feedback moment (12.1–12.3)
- [ ] Options screen with volume, display, and rebinding (15.1, 15.4, 15.5)
- [ ] Settings persisted to the user data directory (15.4)
- [ ] The three known `App-Test` failures fixed and CI gating both suites (18.1)
- [ ] A packaged, self-contained, runnable build with an icon and a version (18.3)
- [ ] Crash handling, file logging, and a player-facing error path (18.4)
- [ ] vsync or a frame cap (18.5)
- [ ] Third-party licences bundled in the build (18.7)
- [ ] Character creation, so the player is not always the one hardcoded prefab (10.3)

**High — a build shipping without these will be called unfinished**

- [ ] Item tooltips and equipped-item comparison (16.1)
- [ ] Consumable stacking, plus sorting and a full-inventory warning (16.2)
- [ ] A minimap or map screen (16.3)
- [ ] Turn-order indicator and movement-range highlight (16.4)
- [ ] Visible feedback for every currently-silent failed action (16.5)
- [ ] A controls and help screen generated from live bindings (16.6)
- [ ] Gamepad support, and hint lines that respect rebinding (15.2)
- [ ] Facing and per-state animation clips (13.1)
- [ ] Scene-transition fades and an area title card (13.4)
- [ ] More than one AI behavior, including ranged and casting enemies (17.1)
- [ ] A boss encounter framework, since `BossArena` is authored and unconsumed (17.3)
- [ ] Multi-tile entities & footprints, so bosses can occupy more than one tile (5.4)
- [ ] Area/biome schema and editor, which several other gaps depend on (3.2)
- [ ] Difficulty tiers (10.2) and fixed area unlock order (4.5)
- [ ] Accessibility baseline: colorblind-safe palettes, text scale, reduced motion (15.6)

**Medium — expected, and cheaper to do before release than after**

- [ ] Impact weight: hit-stop, knockback, camera shake (13.2)
- [ ] Death and spawn presentation, including telegraphed spawns (13.3)
- [ ] Localization seam, even if only English ships (16.7)
- [ ] Faction system replacing the placeholder hostility rule (17.2)
- [ ] Rare enemies and tier roster substitution (5.3, 17.4)
- [ ] Per-area weighted spawn tables (17.5)
- [ ] Mag companion (9.1)
- [ ] Stat materials (8.3)
- [ ] Environmental hazards and real in-world locks and keys (17.6)
- [x] Multi-level areas (Forest 1/2, Caves 1-3) and teleporters between them (4.6) — engine +
      editor done; Mission Select still lists every dungeon individually rather than one row per
      area (see 4.6's own note)
- [ ] Visual consistency pass across the modal screens (16.8)
- [ ] `ContentWatcher` actually wired into `App`, which was built and never connected (1.3)
- [ ] Build portability, so a session without Windows can still verify its own work (18.2)
