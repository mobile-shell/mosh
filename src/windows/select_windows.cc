/*
 * select_windows.cc - Windows select() compatibility layer implementation
 * 
 * Mosh - Mobile Shell
 * Licensed under GPLv3
 */

#ifdef _WIN32

#include "select_windows.h"
#include <winsock2.h>
#include <mstcpip.h>
#include <algorithm>

namespace Mosh {
namespace Util {

AsyncIOMultiplexer& AsyncIOMultiplexer::instance() {
  static AsyncIOMultiplexer multiplexer;
  return multiplexer;
}

int AsyncIOMultiplexer::add_handle(HANDLE handle, int fd_equivalent) {
  if (handles.size() >= MAXIMUM_WAIT_OBJECTS) {
    return -1;  /* Too many handles */
  }
  
  handle_map[handle] = fd_equivalent;
  handles.push_back(handle);
  return 0;
}

bool AsyncIOMultiplexer::remove_handle(HANDLE handle) {
  auto it = std::find(handles.begin(), handles.end(), handle);
  if (it != handles.end()) {
    handles.erase(it);
    handle_map.erase(handle);
    return true;
  }
  return false;
}

int AsyncIOMultiplexer::wait(int timeout_ms) {
  if (handles.empty()) {
    return 0;
  }

  ready_fds.clear();
  
  DWORD wait_result = WaitForMultipleObjects(
    handles.size(),
    handles.data(),
    FALSE,  /* Wait for any one object */
    (timeout_ms < 0) ? INFINITE : timeout_ms
  );

  if (wait_result == WAIT_TIMEOUT) {
    return 0;
  }

  if (wait_result >= WAIT_OBJECT_0 && 
      wait_result < WAIT_OBJECT_0 + handles.size()) {
    
    HANDLE signaled_handle = handles[wait_result - WAIT_OBJECT_0];
    ready_fds.push_back(handle_map[signaled_handle]);
    return 1;
  }

  return -1;  /* Error */
}

std::vector<int> AsyncIOMultiplexer::get_ready_fds() const {
  return ready_fds;
}

/*
 * select_socket: Emulate select() behavior for sockets
 * 
 * This is a simplified implementation focusing on socket readability/writability
 * More complex select() behavior can be added as needed
 */
int SelectCompat::select_socket(int nfds, fd_set *readfds, fd_set *writefds,
                                 fd_set *exceptfds, struct timeval *timeout) {
  if (nfds <= 0) {
    return 0;
  }

  /* Convert timeout to milliseconds */
  int timeout_ms = INFINITE;
  if (timeout != NULL) {
    timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
  }

  /* For now, use Winsock select() directly if available */
  /* This requires proper socket descriptor mapping on Windows */
  
  /* TODO: Implement full Windows socket select equivalent using:
   * - WSAEventSelect() for async socket notification
   * - WSAWaitForMultipleEvents() for event multiplexing
   * - GetSockOpt(SO_ERROR) for socket status checking
   */
  
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

} // namespace Util
} // namespace Mosh

#endif /* _WIN32 */
