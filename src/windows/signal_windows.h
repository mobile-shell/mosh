/*
 * signal_windows.h - Windows signal handling compatibility layer
 * 
 * Replaces Unix signal handling with Windows event-based mechanisms
 * Supports SIGWINCH (resize), SIGCHLD (child process), SIGUSR1, SIGUSR2, etc.
 * 
 * Mosh - Mobile Shell
 * Licensed under GPLv3
 */

#ifndef SIGNAL_WINDOWS_H
#define SIGNAL_WINDOWS_H

#ifdef _WIN32

#include <windows.h>
#include <csignal>
#include <functional>
#include <map>

namespace Mosh {
namespace Util {

typedef std::function<void(int)> SignalHandler;

/*
 * Windows Signal Handler Emulator
 * Maps Unix signals to Windows events/callbacks
 */
class SignalManager {
public:
  static SignalManager& instance();
  
  /* Register a handler for a "signal" equivalent event */
  typedef void (*signal_handler_t)(int);
  signal_handler_t handle(int sig, signal_handler_t handler);
  
  /* Trigger a simulated signal */
  void raise_signal(int sig);
  
  /* Initialize console resize event monitoring */
  void init_resize_monitor();
  
  /* Check for pending signals (called periodically from main loop) */
  void process_pending_signals();
  
private:
  SignalManager() = default;
  
  std::map<int, signal_handler_t> handlers;
  HANDLE console_input_handle;
  bool resize_event_pending = false;
};

/* 
 * Console resize event handler
 * Windows equivalent of SIGWINCH
 */
class ConsoleResizeMonitor {
public:
  static void start_monitoring();
  static void stop_monitoring();
  static bool check_resize();
  
private:
  static HANDLE resize_event_handle;
  static bool is_monitoring;
};

/*
 * Compatibility macro for signal handling
 * Maps to Windows equivalent where possible
 */
#define signal(sig, handler) SignalManager::instance().handle(sig, handler)

/* Windows doesn't have these signals naturally */
#ifndef SIGWINCH
#define SIGWINCH 28  /* Terminal window size change */
#endif

#ifndef SIGCHLD
#define SIGCHLD 17   /* Child process terminated */
#endif

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
#endif /* SIGNAL_WINDOWS_H */
