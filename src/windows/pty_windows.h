/*
 * pty_windows.h - Windows PTY compatibility layer
 * 
 * This file provides Windows ConPTY support for Mosh,
 * replacing Unix pty/termios functionality.
 * 
 * Mosh - Mobile Shell
 * Copyright (C) 2012-2016 Keith Winstein and contributors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef PTY_WINDOWS_H
#define PTY_WINDOWS_H

#ifdef _WIN32

#include <windows.h>
#include <string>

namespace Mosh {
namespace Util {

/* Windows Console/ConPTY Helper Functions */
class PTYWindows {
public:
  /* Initialize console for Mosh client */
  static bool initialize_console();
  
  /* Set console input mode (raw mode equivalent) */
  static bool set_raw_mode();
  
  /* Restore console to original mode */
  static bool restore_console_mode();
  
  /* Get console window size */
  static bool get_window_size(int &rows, int &cols);
  
  /* Set console window size */
  static bool set_window_size(int rows, int cols);
  
private:
  static DWORD original_console_mode;
  static HANDLE console_handle;
};

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
#endif /* PTY_WINDOWS_H */
