/*
 * Auther: ting
 * Date: 2022-08-31
 * Email: vlicecream@163.com
 * Detail: this is the socket config file
 * */

#ifndef __SOCKET_CONFIG_H__
#define __SOCKET_CONFIG_H__

/* 作出限制 */
#define TCP_LINE_SIZE 8192
#define MAX_HTTP_HEADER_COUNT 200

/* 如果这些被定义 则取消定义*/
#ifndef __RUN_DP__
#undef HAVE_OPENSSL
#undef ENABLE_IPV6
#undef USE_SCTP
#undef NO_GETADDRINFO
#undef ENABLE_POOL
#undef ENABLE_SOCKS4
#undef ENABLE_RESOLVER
#undef ENABLE_RECONNECT
#undef ENABLE_DETACH
// #undef ENABLE_EXCEPTIONS
// #undef ENABLE_XML
#endif // __RUN_DP__

/* 支持 OpenSSL */
// #define HAVE_OPENSSL

/* 支持 ipv6 */
#define ENABLE_IPV6

/* 异步解析器 */
#define ENABLE_RESOLVER

/* 失去连接时 启用 tcp 重新连接 */
// #define ENABLE_RECONNECT

/* 如果您的操作系统不支持“getaddrinfo”和“getnameinfo”函数调用
		请定义 NO_GETADDRINFO */
#define NO_GETADDRINFO

/* 启用 socket 线程分离功能 */
#define ENABLE_DETACH

/* 启用连接池 */
#define ENABLE_POOL

/* 关闭 socket 线程分离功能 */
#ifndef ENABLE_DETACH
#undef ENABLE_RESOLVER
#endif

#endif // __SOCKET_CONFIG_H__
