/*
 * network_windows.h - Windows network compatibility shim
 * 
 * Provides Windows Winsock2 compatibility for Unix socket APIs
 * 
 * Mosh - Mobile Shell
 * Licensed under GPLv3
 */

#ifndef NETWORK_WINDOWS_H
#define NETWORK_WINDOWS_H

#ifdef _WIN32

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <cstring>

/* Winsock2 already provides most of what we need */

/* Map some Unix equivalents for compatibility */
#define socklen_t int
#define in_addr_t unsigned long

/* Error handling compatibility */
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EAGAIN WSAEWOULDBLOCK
#define ECONNRESET WSAECONNRESET
#define EMSGSIZE WSAEMSGSIZE

/* Additional Winsock2 compatibility macros */
inline int set_nonblocking(SOCKET sock) {
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode);
}

inline int set_blocking(SOCKET sock) {
  u_long mode = 0;
  return ioctlsocket(sock, FIONBIO, &mode);
}

#else
/* Unix definitions - already correct */
#endif /* _WIN32 */

#endif /* NETWORK_WINDOWS_H */
