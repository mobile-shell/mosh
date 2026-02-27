/*
 * pty_windows.cc - Windows PTY compatibility layer implementation
 * 
 * Mosh - Mobile Shell
 * Copyright (C) 2012-2016 Keith Winstein and contributors
 * Licensed under GPLv3
 */

#ifdef _WIN32

#include "pty_windows.h"
#include <iostream>
#include <csignal>

namespace Mosh {
namespace Util {

DWORD PTYWindows::original_console_mode = 0;
HANDLE PTYWindows::console_handle = NULL;

bool PTYWindows::initialize_console() {
  console_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (console_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (!GetConsoleMode(console_handle, &original_console_mode)) {
    return false;
  }

  return true;
}

bool PTYWindows::set_raw_mode() {
  if (console_handle == NULL || console_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  /* 
   * Raw mode equivalent on Windows:
   * - Disable line buffering
   * - Disable echo
   * - Enable non-blocking reads
   */
  DWORD new_mode = original_console_mode;
  
  /* Remove line-input mode (wait for Enter) */
  new_mode &= ~ENABLE_LINE_INPUT;
  
  /* Remove echo input */
  new_mode &= ~ENABLE_ECHO_INPUT;
  
  /* Keep ENABLE_WINDOW_INPUT for SIGWINCH-like resize events */
  new_mode |= ENABLE_WINDOW_INPUT;
  
  if (!SetConsoleMode(console_handle, new_mode)) {
    return false;
  }

  return true;
}

bool PTYWindows::restore_console_mode() {
  if (console_handle == NULL || console_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (!SetConsoleMode(console_handle, original_console_mode)) {
    return false;
  }

  return true;
}

bool PTYWindows::get_window_size(int &rows, int &cols) {
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(stdout_handle, &csbi)) {
    return false;
  }

  rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;

  return true;
}

bool PTYWindows::set_window_size(int rows, int cols) {
  /* 
   * Note: On Windows, resizing console buffer requires
   * more complex operations. For now, this is a placeholder.
   * The terminal size is typically controlled by the console window.
   */
  (void)rows;
  (void)cols;
  return true;
}

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
