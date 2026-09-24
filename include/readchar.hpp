#pragma once

/*
readchar.h

Provides readchar(), a tiny cross-platform terminal helper that reads one
character without requiring Enter.

Windows uses _getch(), which already performs immediate console input.
POSIX systems temporarily switch stdin from canonical line mode to a raw-ish
single-character mode, read one byte, then restore the original terminal mode.

The implementation uses:

- RAII: TerminalModeGuard restores terminal state in its destructor.
- Resource lifetime ownership: the guard owns the responsibility to restore
  the terminal mode it changed.
- Scope-bound cleanup: normal returns, early returns, and exceptions all run
  local destructors.
- Platform abstraction: readchar() hides Windows/POSIX API differences behind
  one function.
- Defensive system-call handling: interrupted POSIX reads retry on EINTR.

RAII covers normal C++ control flow. It does not guarantee cleanup after hard
process termination, abort, or unhandled fatal signals.
*/

#ifdef _WIN32

#include <conio.h>

inline char readchar() { return static_cast<char>(_getch()); }

#else

#include <cerrno>

#include <termios.h>
#include <unistd.h>

struct TerminalModeGuard {
  termios old_mode{};
  int fd = STDIN_FILENO;
  bool active = false;

  TerminalModeGuard() {
    if (tcgetattr(fd, &old_mode) != 0)
      return;

    /*
    POSIX terminals normally run in canonical mode. In canonical mode, the
    terminal driver edits a whole input line for the program: Backspace, Ctrl-U,
    and similar line-editing keys are handled before the program sees input, and
    read() usually returns only after the user presses Enter. That is good for
    shell-style text input, but wrong for single-key prompts.

    Disabling ICANON asks the terminal to make bytes available without waiting
    for a newline. Disabling ECHO prevents the terminal driver from printing the
    typed key automatically; the caller can decide if and how to display input.

    This is "raw-ish" mode, not full raw mode. Full raw mode commonly also
    disables signal generation, input translation, output translation, software
    flow control, parity processing, and more. For this helper, we only need
    immediate non-echoed character reads, so changing the minimum set of flags
    is less surprising and preserves useful terminal behavior such as Ctrl-C.

    termios itself does not define a standard built-in raw-mode function. Some
    systems provide cfmakeraw(), but it is not POSIX-standard and is broader
    than this function needs.
    */
    termios raw_mode = old_mode;
    raw_mode.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
    raw_mode.c_cc[VMIN] = 1;
    raw_mode.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &raw_mode) == 0)
      active = true;
  }

  ~TerminalModeGuard() {
    if (active)
      tcsetattr(fd, TCSANOW, &old_mode);
  }

  // Disable copying
  TerminalModeGuard(const TerminalModeGuard &) = delete;
  TerminalModeGuard &operator=(const TerminalModeGuard &) = delete;
};

inline char readchar() {
  TerminalModeGuard terminal;

  char ch = 0;
  while (true) {
    ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);

    if (bytes_read == 1)
      return ch;

    if (bytes_read == -1 && errno == EINTR)
      continue;

    return 0;
  }
}

#endif
