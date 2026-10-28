# Native window verification scope

Environment discovery (2026-10-27): command sandbox has no DISPLAY or X11 socket and no SDL/X11/Cairo headers. It has SDL2, X11, Cairo runtime libraries. The assistant cloud desktop is separately accessible through the supported desktop-control tool. A terminal on that desktop reports DISPLAY=:0 and sees this same project directory. No user computer, new package, or display server installation is needed.

Acceptance checks:
1. Launch built native host on the existing cloud desktop and identify its window.
2. Verify initial counter/list and visibly change count through mouse input.
3. Focus search, enter text, check matching list, backspace/clear, and repeat.
4. Exercise keyboard focus/activation supported by current implementation, and resize or expose mapping.
5. Confirm close terminates process cleanly.
6. Preserve tests distinguishing synthetic SDL dummy events from actual desktop input.

Limits: this is a Linux cloud desktop, not macOS, Windows, iOS, GPU efficiency, accessibility certification, or battery testing.
