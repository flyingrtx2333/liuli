# Typed conditions (model/compiler only)

Language document version remains 1; existing int/text/texts programs retain their behavior.
`bool` is a strict scalar with JSON `true`/`false` literals and initial values, not 0/1.
It never coerces to/from int. `to_text` remains int-only; use `if` with text branches
for custom boolean labels.

Expressions use the existing `{"op": "...", "args": [...]}` shape:

| Operation | Input | Output |
| --- | --- | --- |
| `not` | bool | bool |
| `and`, `or` | bool, bool | bool |
| `eq` | two values of exactly the same type | bool |
| `lt`, `le`, `gt`, `ge` | int, int | bool |
| `if` | bool, T, T (identical branch types) | T |

Equality supports int, bool, text (exact UTF-8 bytes), and texts (ordered element
comparison). No locale collation or truthiness. `if` evaluates only its selected
branch; `and`/`or` short-circuit left to right. Every branch is still statically
validated. Dependency guards conservatively include references in both branches;
this does not eagerly evaluate unselected expressions. Separately declared derived
nodes are still evaluated in declaration order, so guard a fallible expression
inside the conditional rather than referencing an already evaluated fallible node.

Generated `Value` adds a bool alternative; `state_bool(id)` and `view_bool(id)`
return bool, and diagnostic snapshots emit actual JSON booleans. `apply` and
persistence restoration reject integer values for boolean keys and vice versa.
Named actions continue staging from the committed snapshot, with atomic rollback
of all state, derived values and counters when any evaluated expression throws.
Non-scalar values retain immutable shared storage. A conditional yielding text or texts
currently materializes the selected result before comparison; even two direct-reference
branches can copy the selected string/vector on reevaluation. No unselected branch
is copied or evaluated. This is not an allocation-free selector optimization; direct
reference derived nodes (without a conditional) retain their existing zero-copy alias path.

## Persistence

The writer emits `EUISTATE 3` when any persisted value is bool, otherwise retains
`EUISTATE 2`. Version 3 adds record tag `B`, with exactly `true` or `false` payload
(case-sensitive; no integer alias). Its key/value byte lengths and separators are
the same as I/T/A records. Version 3 accepts all existing I/T/A types. The reader
still accepts v1 I/T and v2 I/T/A; B records in older versions are rejected.
Older readers cannot read files containing booleans. Existing v1/v2 data is not
rewritten until explicitly saved; scalar-only writes retain the v2 format.
File bounds and atomic replacement behavior are unchanged.

`examples/conditions.eui.json` is a tiny executable model. Generate its scripted
program with `python3 compiler.py examples/conditions.eui.json -o build/conditions.cpp`.
This change does **not** add checkbox/toggle widgets, conditional visibility,
layout bindings, live-host bool commands, or renderer integration.
