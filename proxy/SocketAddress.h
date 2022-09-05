/*
 * Auther: icecream-xu
 * Date: 2022-09-02
 * Emale: vlicecream@163.com
 * */

#ifndef __SocketAddress_H__
#define __SocketAddress_H__

#include <string>
#include <sys/socket.h>
#include <memory>
#include "SocketConfig.h"
#include "SocketInclude.h"

/*
   此类及其子类旨在用作内部数据类型“ipaddr_t”和整个库中发现的各种 IPv6 寻址实现的替代品
	'ipaddr_t' 是网络字节顺序的 IPv4 地址。
	'port_t' 是主机字节顺序的端口号
	'struct in6_addr' 是 IPv6 地址
	'struct in_addr'isan IPv4 地址
*/
class SocketAddress
{
public:
	virtual ~SocketAddress() {}

	/* 得到一个地址结构的指针 */
	virtual operator struct sockaddr *() = 0;

	/* 获取地址结构的长度 */
	virtual operator socklen_t() = 0;

	/* 比较两个地址 */
	virtual bool operator==(SocketAddress&) = 0;

	/* 设置主机端口号
			param port -> 主机字节序中的端口号 */
	virtual void SetPort(port_t port) = 0;

	/* 得到一个主机的端口号.
			return 主机字节序中的端口号 */
	virtual port_t GetPort() = 0;

	/* 设置 socket 地址
			param sa -> 指向 'struct sockaddr_in' or 'struct sockaddr_in6' */
	virtual void SetAddress(struct sockaddr *sa) = 0;

	/* 将地址转换为文本 */
	virtual std::string Convert(bool include_port) = 0;

	/* 反向查找地址 */
	virtual std::string Reverse() = 0;

	/* 获取地址族 */
	virtual int GetFamily() = 0;

	/* 地址结构是否有效 */
	virtual bool IsValid() = 0;

	/* 获取此 SocketAddress 对象的副本。 */
	virtual std::unique_ptr<SocketAddress> GetCopy() = 0;
};


#endif // __SocketAddress_H__

