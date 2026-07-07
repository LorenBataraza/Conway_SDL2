#pragma once

/**
 * net_compat.h — capa de compatibilidad de sockets: POSIX (Linux) vs Winsock2
 * (Windows). Centraliza las diferencias para que server.cpp/client.cpp cambien
 * lo mínimo.
 *
 * IMPORTANTE: en Windows, <winsock2.h> debe incluirse ANTES que <windows.h>
 * (que a veces arrastra SDL). Por eso, en las unidades que mezclan red + SDL,
 * incluir "net_compat.h" primero.
 */

#include <cstdio>
#include <cstring>

#ifdef _WIN32

  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601   // Windows 7: habilita WSAPoll / inet_pton
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>

  using socket_t = SOCKET;
  using pollfd_t = WSAPOLLFD;
  inline const socket_t NET_INVALID_SOCKET_VALUE = INVALID_SOCKET;

  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0          // Windows no tiene SIGPIPE
  #endif

  inline int  net_init()            { WSADATA w; return WSAStartup(MAKEWORD(2, 2), &w); }
  inline void net_cleanup()         { WSACleanup(); }
  inline int  net_close(socket_t s) { return closesocket(s); }
  inline int  net_set_nonblocking(socket_t s) { u_long m = 1; return ioctlsocket(s, FIONBIO, &m); }
  inline int  net_poll(pollfd_t* fds, unsigned long n, int timeout_ms) { return WSAPoll(fds, n, timeout_ms); }
  inline bool net_would_block() {
      const int e = WSAGetLastError();
      return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
  }
  inline bool net_interrupted() { return false; }  // WSAPoll no se interrumpe por señales
  inline void net_perror(const char* msg) { std::fprintf(stderr, "%s: WSA error %d\n", msg, WSAGetLastError()); }

#else

  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <cerrno>

  using socket_t = int;
  using pollfd_t = struct pollfd;
  inline const socket_t NET_INVALID_SOCKET_VALUE = -1;

  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
  #endif

  inline int  net_init()            { return 0; }
  inline void net_cleanup()         {}
  inline int  net_close(socket_t s) { return ::close(s); }
  inline int  net_set_nonblocking(socket_t s) { return ::fcntl(s, F_SETFL, O_NONBLOCK); }
  inline int  net_poll(pollfd_t* fds, nfds_t n, int timeout_ms) { return ::poll(fds, n, timeout_ms); }
  inline bool net_would_block() { return errno == EWOULDBLOCK || errno == EAGAIN; }
  inline bool net_interrupted() { return errno == EINTR; }
  inline void net_perror(const char* msg) { std::fprintf(stderr, "%s: %s\n", msg, std::strerror(errno)); }

#endif
