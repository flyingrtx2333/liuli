# Exact mouse-close follow-up (2026-10-27 11:43–11:45 UTC)

Feature source and example binary were not changed during this investigation.
binary.sha256 was checked again after the three example runs: OK.

The initial error is **not fixed or closed** by the previous Alt-F4 successes.
The exact titlebar mouse-close route was therefore tested again:

| Run | Binary | PID | X11 client window | Exit | app stderr |
|---|---|---:|---:|---:|---|
| mouse1 | frozen enabled-controls | 321 | 29360130 / 0x1c00002 | 0 | empty |
| mouse2 | frozen enabled-controls | 324 | 29360130 / 0x1c00002 | 0 | empty |
| mouse3 | frozen enabled-controls, foreground | 327 | 29360130 / 0x1c00002 | 0 | empty |
| baseline1 | main build/a11y-integrated/eui-window | 330 | 29360130 / 0x1c00002 | 0 | empty |

Each example run used this original input sequence on the same observed 1120x840
client window at (92,92):
- disabled Increment click (440,160), Return, Tab, Tab;
- Return (Toggle), Tab, Return (Increment);
- Toggle click (230,160), disabled Increment click (440,160), Return;
- titlebar close click (1196,75).

Each finished with enabled=false, Count: 1, patches=5, renders=8.
Window IDs/rectangles were freshly observed using CUA/Sky before the actions;
each post-close inventory showed no eui-window. No desktop automation other
than CUA/Sky was used. The first two launch wrappers used background + wait to
capture the app PID; mouse3 reproduced foreground launch with exec to retain
a recorded PID. Main baseline used that same foreground wrapper, increment,
Tab traversal, Enter, click outside controls, then the same titlebar close.

Separate .stdout/.stderr files redirect only the app (exec wrapper has no
post-exec behavior). .exit records its actual wait/foreground exit status.
No helper X error appeared in tool results during these four runs.

## Original failure attribution / remaining uncertainty

The original script redirected only the eui-window command with
"> desktop.log 2>&1", then recorded "$?". Its tool-observed log contained the
normal window-closed telemetry followed by:
  X Error of failed request: BadWindow (invalid Window parameter)
  Major opcode: 25 (X_SendEvent)
  Resource: 0x1c00002
  Serial: 66; current serial: 68
and recorded exit 1. Thus that observation belongs to the app command's
combined output, not merely a CUA helper error. The old file was overwritten
by earlier reruns before this follow-up; the quoted data is recovered from
the original tool output, not presented as a preserved original file.

The failure was not reproduced in the three exact mouse-close reruns or the
one baseline comparison. This is evidence of non-reproduction, not a fix or
proof of absence. Without a failing run/stack trace it is not possible to
attribute the X_SendEvent to SDL teardown, WM behavior, clipboard or focus.
No causal attribution, upstream-fix claim, X-error suppression, SDL patch,
global setting, or feature-code change was made. If it returns, preserve its
separate stderr and obtain an Xlib error call stack before selecting a patch.
