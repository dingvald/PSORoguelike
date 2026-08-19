# Game Design Document

This is a living design document for the game itself — its genre, pillars, systems, and
worldbuilding. It complements [ARCHITECTURE.md](../ARCHITECTURE.md) (runtime engine design),
which tracks *how* the engine is built; this doc tracks *what the game is and why*. It is not
gated on any milestone and is expected to be revisited and refined across many sessions.

**How to read the status callouts:** this is an aspirational document — it describes the full
intended vision, not just what exists today. Every section below carries a **Current
implementation** callout so it stays honest about the gap between vision and reality. As of this
first pass, every callout reads "nothing yet" — this milestone was engine scaffold only (a window,
RmlUi, an empty ECS); no gameplay content or systems exist yet.

This first pass is a **broad skeleton**: outline-depth coverage per pillar, not a full spec.
Deep-dive passes on individual systems (class balance, itemization tables, area-generation rules)
are future-session work.

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
- **Gameplay loop (intended):** descend into a themed area, fight through turn-based encounters,
  find gear and rare drops, return to a hub between runs; permadeath ends a run.

**Current implementation:** nothing yet. This pass delivered only the engine scaffold (SDL3
window, RmlUi hello-world document, an empty ECS wired up via `psr::Registry`) — no gameplay
systems, content, or even a placeholder game loop beyond `HelloWorldLayer`.

## Class triad

Hunter (melee, HP-tanky front-liner), Ranger (weapon-type-driven ranged damage), Force (elemental
Technique caster) — a turn-based reinterpretation of PSO's three class archetypes. Unlike PSO's
real-time combo/positioning-driven combat, each class's identity here should come from how its
turn economy works (e.g. a Hunter's melee attack costs one turn and hits hard; a Force's Technique
might cost more turns to cast but hit an area).

**Current implementation:** nothing yet — no classes, no player stats, no combat system exist.

**Open questions:**
- How many discrete "turns" does a PSO-style combo (e.g. a Ranger's rapid-fire Rifle volley)
  compress into? A flat 1:1 (one action = one turn) is the simplest starting point but may flatten
  what made each PSO weapon type feel distinct.
- Is class a fixed player choice at run start (roguelike-typical) or something built up during a
  run?

## Mag-like companion

PSO's Mag is a small evolving pet that levels via feeding and passively boosts stats. Reimagined
here as an evolving companion/stat-modifier entity that accompanies the player through a run.

**Current implementation:** nothing yet.

**Open questions:**
- Does a Mag persist across permadeath runs (meta-progression, softening the roguelike reset) or
  reset with everything else (pure roguelike)? This is a genre-tone decision, not just a numbers
  one — meta-progression across runs is a different kind of game than a pure single-run roguelike.
- What does "feeding" map to in turn-based play — consuming items found during the run, or a
  between-run hub activity?

## Elemental Techniques

PSO's Foie/Barta/Zonde-style elemental spell families (fire/ice/lightning, roughly), cast by
Forces. Reimagined as a turn-based spell system with elemental damage types and status effects
(burn/freeze/shock-adjacent mechanics), tiered by spell level the way PSO's Techniques level with
use.

**Current implementation:** nothing yet.

## Area themes

PSO's non-persistent, regenerated-per-visit dungeons (Forest, Caves, Mines, Ruins, and later
areas) map naturally onto standard roguelike procedural generation — this is one of the more
direct translations from PSO to roguelike form, not a stretch. Each area theme would drive tile
palette, enemy population, and hazard type, generated fresh each time the player enters.

**Current implementation:** nothing yet — no procgen framework exists in this scaffold (deferred,
along with persistence, to a future milestone; `rapidjson`/`cereal` were deliberately left out of
`vcpkg.json` for this pass).

**Open questions:**
- How does area-theme cycling map to a single run — one area per run (PSO's per-visit structure)
  or multiple areas/floors per run (more roguelike-typical "descend through many floors")?

## Itemization

Rare-enemy and rare-drop systems (PSO's low-probability alternate-colored rare monsters and their
exclusive drops) plus general weapon-type variety (sabers, swords, handguns, rifles, canes/staves
for Forces).

**Current implementation:** nothing yet.

## Permadeath

Standard roguelike death finality: a run ends on player death, no mid-run resurrection. How much
(if anything) carries forward to the next run is the open question tying together the Mag and
itemization sections above.

**Current implementation:** nothing yet.

## Cross-cutting open questions

- **Turn-pacing granularity**, restated from the class-triad section: the single biggest design
  risk in this reinterpretation is that PSO's feel comes largely from its real-time combat rhythm
  (dodging, positioning, combo timing), none of which a naive 1-action-per-turn translation
  preserves automatically. Worth a dedicated design pass once combat is actually being built,
  rather than deciding it here in the abstract.
- **Meta-progression vs. pure reset**, restated from the Mag section: whether *anything* (Mag,
  unlocked classes, cosmetic unlocks) persists across permadeath runs is a foundational tone
  decision that should be made before itemization/progression systems are designed in detail,
  since it changes what "a run" is for.
- **Single-player only, or coop-shaped systems even if coop itself is out of scope?** PSO's
  identity was heavily online-coop; nothing in the turn-based reinterpretation requires coop, but
  it's worth deciding explicitly rather than by default, since it would affect e.g. whether loot is
  designed around solo pacing.
