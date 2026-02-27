/*
 * select_windows.h - Windows select() compatibility layer
 * 
 * Provides Windows async I/O multiplexing using WaitForMultipleObjects
 * as an alternative to Unix select()/pselect()
 * 
 * Mosh - Mobile Shell
 * Licensed under GPLv3
 */

#ifndef SELECT_WINDOWS_H
#define SELECT_WINDOWS_H

#ifdef _WIN32

#include <windows.h>
#include <vector>
#include <map>

namespace Mosh {
namespace Util {

/*
 * Windows event wrapper for async I/O
 * Maps HANDLE objects to file descriptors for compatibility
 */
class AsyncIOMultiplexer {
public:
  static AsyncIOMultiplexer& instance();
  
  /* Register a socket or handle for monitoring */
  int add_handle(HANDLE handle, int fd_equivalent);
  
  /* Remove a handle from monitoring */
  bool remove_handle(HANDLE handle);
  
  /* Wait for any registered handle to become signaled (equivalent to select) */
  int wait(int timeout_ms = -1);
  
  /* Get list of signaled handles */
  std::vector<int> get_ready_fds() const;
  
private:
  AsyncIOMultiplexer() = default;
  
  std::map<HANDLE, int> handle_map;
  std::vector<HANDLE> handles;
  std::vector<int> ready_fds;
};

/*
 * Wrapper for select()-like behavior on Windows
 * Currently handles socket I/O monitoring
 */
class SelectCompat {
public:
  /* 
   * Emulates select() for socket monitoring
   * Returns: number of ready sockets, or SOCKET_ERROR on error
   */
  static int select_socket(int nfds, fd_set *readfds, fd_set *writefds, 
                           fd_set *exceptfds, struct timeval *timeout);
};

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
#endif /* SELECT_WINDOWS_H */
