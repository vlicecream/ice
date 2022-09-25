/*
 * Auther: icecream-xu
 * Date: 2022-09-04
 * Email: vlicecream@163.com
 * */

#ifndef __SOCKET_Ipv4Address_H__
#define __SOCKET_Ipv4Address_H__

#include <netinet/in.h>
// #include "SocketConfig.h"
#include "SocketAddress.h"


/* ipv4地址实现 */
class Ipv4Address : public SocketAddress
{
public:
	/* 创建空Ipv4地址结构 */
	Ipv4Address(port_t port = 0);		

	/* 创建ipv4地址结构 
	    param a -> 网络字节顺序的套接字地址
			param port -> 按主机字节顺序排列的端口号 */
	Ipv4Address(ipaddr_t a, port_t port);

	/* 创建ipv4地址结构
			param a -> 网络字节序中的套接字地址
			param port -> 主机字节顺序的端口号 */
	Ipv4Address(struct in_addr& a,port_t port);

	/* 创建ipv4地址结构
			param host -> 待解析的主机名
			param port -> 按主机字节顺序排列的端口号 */
	Ipv4Address(const std::string& host,port_t port);
	Ipv4Address(struct sockaddr_in&);
	~Ipv4Address();

	// 套接字地址实现
	operator struct sockaddr*();
	operator socklen_t();
	bool operator==(SocketAddress&);

	void SetPort(port_t port);
	port_t GetPort();

	void SetAddress(struct sockaddr *sa);
	int GetFamily();

	bool IsValid();
	std::unique_ptr<SocketAddress> GetCopy();

	/* 将地址结构体转换为文本 */
	std::string Convert(bool include_port = false);
	std::string Reverse();

	/* 解析主机名 */
	static bool Resolve(const std::string& hostname,struct in_addr& a);
	/* 反向解析 (IP to hostname) */
	static bool Reverse(struct in_addr& a,std::string& name);
	/* 转换地址结构为文本 */
	static std::string Convert(struct in_addr& a);

private:
	Ipv4Address(const Ipv4Address& ) {} // copy constructor
	Ipv4Address& operator=(const Ipv4Address& ) { return *this; } // assignment operator
	struct sockaddr_in m_addr;
	bool m_valid;
};

#endif // __SOCKET_Ipv4Address_H__
