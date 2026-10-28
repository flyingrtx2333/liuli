# Rejected-input history audit

Base code: ae520f4 (production source unchanged from 3007dde). This patch adds
regression tests only; no production defect was demonstrated in this bounded audit.

## Mechanism and proof

The host stages TextEdit by value, validates all affected/mirrored editor states,
then applies the generated model's transactional candidate. Editor publication is
noexcept after model success. Rejected operations therefore discard staged history
rather than mutating the live native history first. TextEdit itself validates
UTF-8 and byte limits before pushing history or clearing redo.

The new host test compiles a real generated model whose lazy conditional overflows
for BAD, or for A while a separate gate is set. It verifies the actual expected
exception reason, all persisted values, aggregate evaluation/patch/materialization
metrics, persistence/UI revisions, status/focus, and each editor's full undo and
redo replay sequence including text/selection/preedit coordinates and stack counts.

Covered cases:
- Existing redo plus oversized clipboard paste, single-line rejection or malformed
  UTF-8. SDL text/key events exercise the host entry path where applicable.
- Derived overflow on edit; another state invalidating an old undo target; relaxing
  that state allows the preserved undo entry to work again.
- Derived rejection of a composition commit, malformed composition range, and
  action/restore failure while composition remains active.
- Atomic peer/mirror history preservation; bad later-field restore validation.
- Explicit focus loss intentionally cancels composition without changing committed
  text/history; this is a documented transition, not a rejected edit.
- Standalone TextEdit existing-redo rejection using the same A→AB→undo scenario
  that failed the isolated Qt Widgets experiment.

## Limits

This uses synthetic SDL dummy events and direct host transaction calls, not a real
system IME, clipboard manager, accessibility client or screen reader. Pointer focus
changes before a failed button action can intentionally cancel old composition;
the test's direct action failure does not claim pointer focus rollback. Successful
external action/load replacements may intentionally clear changed-field history.
Rendering/publication failures after model commit remain fatal by contract, not
rollback-capable input rejections. No allocation fault-injection claim is added.

Run: python3 -m unittest discover -s tests -p test_input_rejection.py -v
Full suite: CMAKE=/path/to/cmake EUI_A11Y_SDK=/path/to/sdk sh scripts/check.sh
See focused.log and final-check.log. check.log is the intermediate run before
expected-exception diagnostic matching was strengthened; do not use it as final
source proof. Final full-suite result is recorded after completion below.

Final frozen source: **57 tests PASS**, 106.273 s; generated demo and live-host
build/smoke PASS; complete scripts/check.sh exit0. Focused final test PASS,
5.985 s. No GUI was opened and no production source was modified.
