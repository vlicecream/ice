/*
 * Auther: ting
 * Date: 2022-08-31
 * Email: vlicecream@163.com
 * Detail: 
 * */

#ifndef __SOCKET_Socket_include_H__
#define __SOCKET_Socket_include_H__

#define INVALID_SOCKET -1
/* 定义类型以唯一标识套接字实例 */
typedef unsigned long socketuid_t;

/* 文件描述符 */
typedef int SOCKET;

#define Errno errno
#define StrError strerror
const char *StrError(int x);

// <--- linux
typedef unsigned long ipaddr_t;
typedef unsigned short port_t;
// --->

// getaddrinfo / getnameinfo replacements
#ifdef NO_GETADDRINFO
#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST 1
#endif
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 1
#endif
#endif

/* Close the file descriptor FD.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int close (int __fd);

#define closesocket close
#endif // __SOCKET_Socket_include_H__
