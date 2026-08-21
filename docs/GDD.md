# Game Design Document

This is a living design document for the game itself — its genre, pillars, systems, and
worldbuilding. It complements [ARCHITECTURE.md](../ARCHITECTURE.md) (runtime engine design),
which tracks *how* the engine is built; this doc tracks *what the game is and why*. It is not
gated on any milestone and is expected to be revisited and refined across many sessions.

**How to read the status callouts:** this is an aspirational document — it describes the full
intended vision, not just what exists today. Every section below carries a **Current
implementation** callout so it stays honest about the gap between vision and reality. As of this
pass, every callout still reads "nothing yet" — this is a design-only pass with no code behind it.

**Accuracy note:** the PSO-specific names, races, rosters, and Section IDs below were cross-checked
against Phantasy Star Wiki, the Ephinea PSO Wiki, and PSCave (2026-08-19) — area/race/boss mappings,
the Caves/Mines/Ruins rosters, and all ten Section ID names confirmed accurate. Still worth a closer
look before any exact numbers (drop rates, stat values) are authored, since that level of detail
wasn't verified here.

This doc is a **design reference**, not authored game content: per [CLAUDE.md](../CLAUDE.md), the
systems below are meant to be built data-driven with editor support, with the user authoring the
real content (specific weapons, enemies, missions, drop tables) once each system exists — Claude
writes content only when explicitly asked to.

## Overview

- **Genre:** 2D, top-down, turn-based grid roguelike RPG.
- **Mechanical inspirations:** classic roguelikes (discrete turns, permadeath, tile-based
  exploration).
- **Thematic inspiration:** *Phantasy Star Online* (GameCube) — Hunters/Rangers/Forces, weapon
  variety, evolving Mag companions, elemental Techniques, area themes (Forest/Caves/Mines/Ruins),
  rare enemies and rare drops.
- **Design intent:** take PSO's moment-to-moment *hack-and-slash dungeon crawl* and reinterpret it
  as a *turn-based* roguelike — same fantasy (a Hunter descending into procedurally-regenerated
  ruins for loot and rare monster kills), different pacing model. This is explicitly a
  reinterpretation, not a 1:1 port of PSO's real-time action combat.
- **Gameplay loop:** a **mission** is one visit to one themed area, generated fresh each time
  (PSO's non-persistent dungeon model). A **run** is the entire lifetime of one character: many
  missions launched from a persistent hub between them, character/gear/Mag carrying over normally
  from mission to mission *within* the run. Permadeath ends the run — the character's whole
  life — not just the mission in progress.

**Current implementation:** nothing yet. The engine scaffold milestone delivered only a window,
RmlUi, and an empty ECS — no gameplay systems, content, or game loop beyond `HelloWorldLayer`.

## Class triad

Hunter (melee, HP-tanky front-liner), Ranger (weapon-type-driven ranged damage), Force (elemental
Technique caster) — a turn-based reinterpretation of PSO's three class archetypes, each built
around a distinct weapon category:

- **Hunter — melee weapons:** Saber, Sword, Dagger, Partisan, Slicer, Claw. Differentiated by
  range shape (single adjacent tile / cone / line) and an ATP-(power) vs ATA-(accuracy) tradeoff
  per weapon type.
- **Ranger — ranged weapons:** Handgun, Rifle, Mechgun, Shot. No ammo or reload (photon-charged,
  matching PSO) — differentiated by range, spread (single-target vs area-of-effect line/cone), and
  hits-per-turn.
- **Force — Cane/Wand:** Cane boosts Technique power when equipped; Wand is the channel Techniques
  are cast through. Techniques are the class's primary damage source, spending Force's own TP pool
  (kept separate from Hunter/Ranger's PP — see
  [Weapons, Photon Arts & combat skills](#weapons-photon-arts--combat-skills) below); melee is a
  fallback only when TP runs low.

**Turn pacing (resolved):** energy-based turns with variable action costs — most actions cost one
turn's worth of energy in the common case, but specific actions/abilities can cost more or less.
This mirrors a proven pattern rather than inventing a new one. The *specific* per-action energy
costs (how much a heavy Sword swing costs vs. a light Dagger stab, etc.) are a follow-on balancing
task, not decided here.

**Class lock (resolved):** class is a fixed choice made at character creation, for the whole run —
standard roguelike pattern, and matches PSO (class is also a creation-time choice there).

**Current implementation:** nothing yet — no classes, no player stats, no combat system exist.

## Weapons, Photon Arts & combat skills

- **Stat framework** (reused directly from PSO): **ATP** (attack power), **ATA** (accuracy),
  **MST** (technique/magic power), **DFP** (defense), **EVP** (evasion), **LCK** (luck — biases
  rare triggers).
- **Four-race damage system:** every weapon can carry bonus damage % against one of four enemy
  races — **Native** (plant/animal creatures), **A.Beast** (bio-engineered beasts), **Machine**
  (robotic constructs), **Dark** (demonic/humanoid dark units). This ties directly into
  [Areas, enemies & bosses](#areas-enemies--bosses) below, since each area is dominated by one
  race — a weapon built for one area's race trades off effectiveness elsewhere.
- **Photon Arts:** weapon-attached special attacks, spending **PP** (see below). **Design
  adaptation from PSO:** in the source game these trigger probabilistically on a normal hit; here
  they're reinterpreted as an **actively chosen attack option that costs PP** — a deliberate
  tactical choice fits turn-based play better than a hidden proc chance. Example families (exact
  effects/naming are a follow-on balancing pass, not decided here): a drain-family art trades PP
  for HP/PP drain on hit; a status-family art inflicts an ailment (Freeze/Poison/Shock/Confuse).
- **PP vs. TP (resolved):** kept split, matching PSO exactly, rather than unified. **PP** fuels
  Hunter/Ranger weapon Photon Arts; **TP** is Force's separate resource for
  [Techniques](#elemental-techniques). Two distinct resource pools from the start, giving Force
  its own identity rather than sharing a pool with the other two classes.

**Current implementation:** nothing yet.

## Elemental Techniques

PSO's Foie/Barta/Zonde-style elemental spell families (fire/ice/lightning, roughly), cast by
Forces, spending their own **TP** pool — kept separate from Hunter/Ranger Photon Arts' PP (see
[Weapons, Photon Arts & combat skills](#weapons-photon-arts--combat-skills) above). Reimagined as
a turn-based spell system with elemental damage types and status effects (burn/freeze/shock-
adjacent mechanics), tiered by spell level the way PSO's Techniques level with use.

**Current implementation:** nothing yet.

## Areas, enemies & bosses

Four areas, each matching one of the four races above and ending in a signature boss — pulled
directly from PSO Episode 1's area/race/boss structure:

| Area | Race | Notable enemies | Boss |
|---|---|---|---|
| Forest | Native | Booma family, Rag Rappy, Savage Wolf, Monest/Mothmant swarm, Hildebear | Dragon |
| Caves | A.Beast | Poison Lily, Nano Dragon, Evil Shark family, Pan Arms, Grass Assassin | De Rol Le |
| Mines | Machine | Canadine family, Dubchic/Gilchic, Sinow Beat/Gold, Garanz | Vol Opt |
| Ruins | Dark | Delsaber, Chaos Sorcerer/Bringer, Dark Belra, Bulclaw | Dark Falz |

**Rare enemies:** a low-probability alt-colored variant of a normal spawn, with boosted stats and
a guaranteed better/exclusive drop (e.g. Rag Rappy's rare form) — the itemization hook for hunting
specific enemies rather than just clearing missions.

**Difficulty-tier reskins carry real detail, not just stat bumps:** PSO sometimes swaps an
enemy for a tougher counterpart at higher difficulty rather than just scaling its numbers — e.g.
Mines' Garanz is replaced by Baranz on Ultimate. Worth keeping as a pattern for
[difficulty tiers](#progression-hub--difficulty) generally: some tier-ups should swap roster
entries, not only inflate stats.

**Current implementation:** nothing yet — no procgen framework exists yet (deferred, along with
persistence, to a future milestone; `rapidjson`/`cereal` were deliberately left out of
`vcpkg.json` for the engine scaffold pass).

## Area themes / mission structure

A **mission** is one visit to one area (resolved: not a multi-floor descent, not a multi-area
run — matches PSO's per-visit dungeon model directly). Areas unlock in a **fixed order**: Forest,
then Caves, then Mines, then Ruins, matching PSO's own pacing and giving early missions a natural
difficulty ramp. Each area theme drives tile palette, enemy population (its race), and hazard
type, generated fresh every time it's entered — non-persistent, like PSO's dungeons.

**Current implementation:** nothing yet.

## Progression, hub & difficulty

- **Hub:** a persistent, Hunter's-Guild-style counter the character returns to between missions —
  mission select, shop (buy/sell), storage. The hub itself is not procedurally generated and isn't
  part of any mission.
- **Mission selection:** from the hub, pick any area+difficulty combination currently unlocked for
  this character as the next mission.
- **Difficulty tiers:** Normal → Hard → Very Hard → Ultimate. Same area, reskinned tougher (enemy
  stat/population changes, better drop tables at higher tiers). **Clear-gated per character:**
  finishing a mission at a given tier unlocks the next tier up for that same area, for that
  character only.
- **Character growth:** combat XP raises level, which raises stats along a class-specific growth
  curve (Hunter/Ranger/Force each grow differently, matching PSO). Mag stat-boosts (see
  [Mag-like companion](#mag-like-companion)) stack on top of this, but — per the meta-progression
  decision below — none of it survives past the run.

**Current implementation:** nothing yet.

## Itemization & Section IDs

- **Section ID:** a character-creation choice from PSO's ten IDs — Viridia, Greenill, Skyly,
  Bluefull, Purplenum, Pinkal, Redria, Oran, Yellowboze, Whitill — that skews which rare
  items/enemies can appear for that character. A build-variety lever chosen once per
  character/run, since nothing persists across runs anyway (see meta-progression below).
- **Drop tables:** each enemy has a common base table plus a low-probability rare table, both
  weighted by the character's Section ID; bosses have their own guaranteed-meaningful table.
- **Meseta:** the currency. Drops commonly, spent at hub shops.
- **Photon crystals / materials:** consumable items granting a small permanent per-stat boost
  (Power Material → ATP, Mind Material → MST, HP Material → max HP, etc.) — a meaningful
  investment specifically because nothing survives past the run.
- **Item identification ("Tekking") — flagged optional, not committed this pass:** PSO's mechanic
  where found weapons are unidentified until checked at a counter, with a small chance the result
  is a dud. Noted as optional texture for a later pass, not decided here.

Plus general weapon-type variety, covered in
[Weapons, Photon Arts & combat skills](#weapons-photon-arts--combat-skills) above.

**Current implementation:** nothing yet.

## Mag-like companion

PSO's Mag is a small evolving pet that levels via feeding and passively boosts stats. Reimagined
here as an evolving companion/stat-modifier entity that accompanies the player through a run, and
resets along with everything else at permadeath (see below).

**Feeding (resolved):** happens in the field — consumable items found during a mission can be fed
to the Mag mid-mission, tying its growth directly into exploration and itemization rather than
being a hub-only chore.

**Current implementation:** nothing yet.

## Permadeath

A run is the entire life of one character. Death during any mission ends the run outright — the
character's whole life, not just the mission in progress — with no mid-run resurrection. Returning
to the hub between missions does not end anything; only actual character death does.

**Meta-progression (resolved): nothing persists to a new character/run.** Starting a new character
after permadeath is a full reset — no Mag, no unlocks, no currency carried over. Everything that
*does* persist (gear, Mag, area/difficulty unlocks, character level) persists only *within* a run,
i.e. normal continuity between missions for the same still-living character, not meta-progression
across separate runs.

**Current implementation:** nothing yet.

## Scope decisions

- **Single-player only (resolved):** no coop-shaped design considerations — itemization, pacing,
  and difficulty are tuned entirely around a solo character. PSO's identity was heavily
  online-coop, but nothing about this turn-based reinterpretation requires it, and simplest scope
  wins by default here.

## Deferred to a future balancing pass

Not decided in this doc — explicitly out of scope until the corresponding system is actually being
built and can be tuned against real play:
- **Exact per-action energy costs** (how much a heavy Sword swing costs vs. a light Dagger stab,
  a Rifle volley vs. a single Handgun shot, etc.).
- **Difficulty-tier drop/challenge scaling specifics** (exact stat multipliers, drop-rate curves
  Normal → Ultimate).
- **Photon Art / Technique family effects and PP/TP costs** (the specific drain/status families
  sketched in [Weapons, Photon Arts & combat skills](#weapons-photon-arts--combat-skills) need
  concrete numbers once combat exists to test against).
