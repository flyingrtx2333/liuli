# v0.4 source-frozen checkpoint

2026-10-27, assistant-owned Linux cloud. Source checkpoint built and verified before later isolated input/accessibility work.

- `final-check.log`: 39 automated tests passed in66.282s, then strict scripted/live builds and stdin smoke; aggregate exit0.
- `final-task-build.log`: generic host built against task-list model/layout through CMake, exit0.
- `actual-responsive-qa.md`: final actual X11 responsive and task-list workflows, save-immediate-close/restart, corruption overwrite protection and clean exits.
- `async-final.sha256`: actual-tested executable hashes. Main source had no modifications during this final desktop QA.
- Final responsive scene RSS24132KiB (~23.57MiB), peak37936KiB; task scene RSS20112KiB. Each2-second idle sample had zero additional100Hz CPU ticks. Separate scene/binary observations, not a full-app comparison or energy test.
- `model-storage.md` and allocation JSONL files: before/after allocation traffic microbenchmark; not RSS.
- `docs/cloud-x11.md` and scoped SDL recipe: initial MIT-SHM failures preserved and isolated; final official non-SHM SDL configuration works without global/system changes.

Unproven: Windows/macOS runtime and distribution, system IME, complete grapheme/mouse editing, platform accessibility, GPU materials, multiwindow, disk-full/power-loss durability, real battery/energy superiority and compatibility with existing HTML/CSS/JS apps. Independent ATK/text work following this checkpoint is not included as completed functionality.
