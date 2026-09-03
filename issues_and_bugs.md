# Issues and Bugs

## General
- [x] Move components and systems out from `Core` into `App`

## Editor
- [x] Zoom should scale the entity textures (applies to dungeon and piece editors)
- [x] Editor menu should be centered
- [x] Sockets should no longer be entities, but piece-specific data
  - [x] Socket editor should allow filling in all info (tags, connects_to_tags, edge, fallback id)
- [x] No need for a status effect editor
- [x] Similar to sockets, there should be Spawns that can be set for spawn waves
- [] Socket tag and connects_to_tag text box fields look terrible

## App
- [x] Smooth camera movement that follows the player, except during attack tweening
- [] Three pre-existing App-Test failures, confirmed present on `master` independent of any
  in-progress work (reproduced with `App-Test.exe "[TweenSystem]","[MoveAction]","[CombatLogBridge]"`
  both on `master` and with unrelated WIP stashed out): `TweenSystemTests.cpp:86` (a hairline float
  precision mismatch, `0.050000004f` vs `0.050000001f`), `MoveActionTests.cpp:174` (bump-to-attack
  fallback queues 4 tweens instead of the expected lunge-and-return pair of 2), and
  `CombatLogBridgeTests.cpp:106` (a lethal `AfterDamageEvent` only publishes 1 combat-log line
  instead of the expected hit+defeat pair of 2). Fail in isolation, not order-dependent.

