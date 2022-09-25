/*
 * Auther
 * Date: 2022-09-04
 * Email:
 * Detail: 
 * */

#ifndef __SOCKET_Utility_H__
#define __SOCKET_Utility_H__

#include <map>
#include <ctype.h>
#include <memory>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "SocketConfig.h"
#include "SocketInclude.h"

#define TWIST_LEN 624

class SocketAddress;

/* 转换工具 */
class Utility
{
	class Rng {
	public:
		Rng(unsigned long seed);

		unsigned long Get();

	private:
		int m_value;
		unsigned long m_tmp[TWIST_LEN];
	};
public:

	/* 返回一个随机的32位整数 */
	static unsigned long Rnd();

	/* 检查字符串是否为有效的ipv4/ipv6 ip编号 */
	static bool isipv4 (const std::string&);
	static bool isipv6 (const std::string&);

	/* 主机名到ip解析ipv4，不是异步的 */
	static bool u2ip(const std::string&, ipaddr_t&);
	static bool u2ip(const std::string&, struct sockaddr_in& sa, int ai_flags = 0);

	/* 反向查找地址到主机名 */
	static bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string&, int flags = 0);
	static bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string& hostname, std::string& service, int flags = 0);
  
	/* 获取当前时间(秒/微秒) */
	static void GetTime(struct timeval*);

	/* 等待指定的ms数 */
	static void Sleep(int ms);

	static std::string l2string(long l);

	static const std::string Stack();

private:
	static std::string m_host; ///< local hostname
	static ipaddr_t m_ip; ///< local ip address
	static std::string m_addr; ///< local ip address in string 
	static bool m_local_resolved; ///< ResolveLocal has been called if true
};

#endif
