/*
 * signal_windows.cc - Windows signal handling implementation
 * 
 * Mosh - Mobile Shell
 * Licensed under GPLv3
 */

#ifdef _WIN32

#include "signal_windows.h"
#include <iostream>
#include <thread>

namespace Mosh {
namespace Util {

SignalManager& SignalManager::instance() {
  static SignalManager manager;
  return manager;
}

SignalManager::signal_handler_t SignalManager::handle(int sig, 
                                                      signal_handler_t handler) {
  signal_handler_t old_handler = nullptr;
  
  auto it = handlers.find(sig);
  if (it != handlers.end()) {
    old_handler = it->second;
  }
  
  handlers[sig] = handler;
  
  /* Initialize console input monitoring for resize events */
  if (sig == SIGWINCH && console_input_handle == NULL) {
    console_input_handle = GetStdHandle(STD_INPUT_HANDLE);
    init_resize_monitor();
  }
  
  return old_handler;
}

void SignalManager::raise_signal(int sig) {
  auto it = handlers.find(sig);
  if (it != handlers.end() && it->second != nullptr) {
    it->second(sig);
  }
}

void SignalManager::init_resize_monitor() {
  /* This would be called periodically from main event loop */
  if (console_input_handle != NULL && 
      console_input_handle != INVALID_HANDLE_VALUE) {
    ConsoleResizeMonitor::start_monitoring();
  }
}

void SignalManager::process_pending_signals() {
  /* Check for resize event */
  if (handlers.find(SIGWINCH) != handlers.end()) {
    if (ConsoleResizeMonitor::check_resize()) {
      raise_signal(SIGWINCH);
    }
  }
  
  /* Additional signal checking can be added here */
}

/* Static member initialization */
HANDLE ConsoleResizeMonitor::resize_event_handle = NULL;
bool ConsoleResizeMonitor::is_monitoring = false;

void ConsoleResizeMonitor::start_monitoring() {
  if (is_monitoring) {
    return;
  }
  
  HANDLE console_input = GetStdHandle(STD_INPUT_HANDLE);
  if (console_input == INVALID_HANDLE_VALUE) {
    return;
  }
  
  /* 
   * Enable console input events
   * This will trigger resize events through console input buffer
   */
  DWORD mode;
  if (GetConsoleMode(console_input, &mode)) {
    mode |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(console_input, mode);
  }
  
  is_monitoring = true;
}

void ConsoleResizeMonitor::stop_monitoring() {
  is_monitoring = false;
  if (resize_event_handle != NULL) {
    CloseHandle(resize_event_handle);
    resize_event_handle = NULL;
  }
}

bool ConsoleResizeMonitor::check_resize() {
  HANDLE console_input = GetStdHandle(STD_INPUT_HANDLE);
  if (console_input == INVALID_HANDLE_VALUE) {
    return false;
  }
  
  DWORD num_events = 0;
  if (!GetNumberOfConsoleInputEvents(console_input, &num_events)) {
    return false;
  }
  
  if (num_events == 0) {
    return false;
  }
  
  INPUT_RECORD input_record;
  DWORD num_read = 0;
  
  /* Peek at input without removing from buffer */
  if (!PeekConsoleInput(console_input, &input_record, 1, &num_read)) {
    return false;
  }
  
  if (num_read > 0 && input_record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
    /* Consume the event */
    ReadConsoleInput(console_input, &input_record, 1, &num_read);
    return true;
  }
  
  return false;
}

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
