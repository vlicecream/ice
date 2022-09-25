/*
 * Auther:
 * Date: 2022-09-10
 * Email: vlicecream@163.com
 * */

#ifndef __SOCKET_UdpSocket_H__
#define __SOCKET_UdpSocket_H__


#include <sys/socket.h>
#include "SocketInclude.h"
#include "SocketConfig.h"
#include "Socket.h"

class SocketAddress;

class UdpSocket : public Socket
{
public:
	/* 构造函数
			param -> ISocketHandler socket执行者
			param -> bufSize 接收消息的最大值 多余部分直接截断
			param -> ipv6 'true' 如果这是一个 ipv6 套接字 */
	UdpSocket(ISocketHandler& handler, int bufSize = 16384, bool ipv6 = false, int retries = 0);	

	// 析构函数
	~UdpSocket();

	/* 收到传入数据时调用
	   param -> buf 指向数据的指针
		 param -> len 数据的长度
		 param -> sa 指向发件人的 sockaddr 结构的指针 
		 param -> socklen_t sockaddr 结构体的长度 */
	virtual void OnRawDate(const char* buf, size_t len, struct sockaddr* sa, socklen_t sa_len);

	/* 在接收到传入数据并启用读取时间戳时调用 */
	virtual void OnRawDate(const char* buf, size_t len, struct sockaddr* sa, socklen_t sa_len, struct timeval* ts);

	/* 要接收传入数据，请调用 Bind 设置传入端口 
	    param -> port 传入端口号码 
			param -> range 端口范围，如果端口已经在使用，则尝试
				如果成功 返回0 */
	int Bind(port_t& port, int range = 1);

	/* 要在特定接口上接收数据：端口，请使用此
	   param -> intf ip/hostname interface
		 param -> port 端口号码
		 param -> range 端口范围，如果端口已经在使用，则尝试
			如果成功 返回0 */
	int Bind(const std::string& intf, port_t& port, int range = 1);

	/* 要在特定接口上接收数据：端口，请使用此 
			param -> ipaddr ip地址
			param -> port 端口号码
			param -> range 端口随机范围 
			 如果成功 返回0 */
	int Bind(ipaddr_t ipaddr, port_t& port, int range = 1);

	/* 要在特定接口上接收数据：端口，请使用此
			param -> ad socket地址
			range -> 端口范围 
				如果成功 返回0 */
	int Bind(SocketAddress& ad, int range = 1);

	/* 定义远程主机 
	    param -> l 远程主机地址
			param -> port 端口号码 
			 如果成功 返回true */
	bool Open(ipaddr_t l, port_t port);

	/* 定义远程主机
			param -> 主机名
			param -> 端口号码 
			`如果成功 返回true */
	bool Open(std::string& host, port_t port);

	/* 定义远程主机
	    param -> ad socket地址 
				如果成功 返回true */
	bool Open(SocketAddress& ad);

	/* 发送到指定主机 */
	void SendToBuf(const std::string&, port_t, const char* data, int len, int flags = 0);

	/* 发送到指定地址 */
	void SendToBuf(ipaddr_t, port_t, const char* data, int len, int flags = 0);

	/* 发送到指定的socket地址 */
	void SendToBuf(SocketAddress& ad, const char* data, int len, int flags = 0);

	/* 发送字符串到指定主机 */
	void SendTo(const std::string&, port_t, const std::string&, int flags = 0);

	/* 发送字符串到指定地址 */
	void SendTo(ipaddr_t, port_t, const std::string&, int flags = 0);

	/* 发送字符串到指定socket地址 */
	void SendTo(SocketAddress& ad, const std::string&, int flags = 0);

	/* 发送到连接地址 */
	void SendBuf(const char* data, size_t, int flags = 0);

	/* 发送字符串到连接地址 */
	void Send(const std::string&, int flags = 0);

	/* 广播 */
	void SetBroadcast(bool b = true);

	/* 检查广播信号 */
	bool IsBroadcast();

	// 多播 {

	void SetMulticastTTL(int ttl = 1);
	int GetMulticastTTL();
	void SetMulticastLoop(bool = true);
	bool IsMulticastLoop();
	void SetMulticastDefaultInterface(ipaddr_t ipaddr, int if_index = 0);
	void SetMulticastDefaultInterface(const std::string& intf, int if_index = 0);
	void AddMulticastMembership(const std::string& group, const std::string& intf = "0.0.0.0", int if_index = 0);
	void DropMulticastMembership(const std::string& group, const std::string& intf = "0.0.0.0", int if_index = 0);

	// }

	/* 如果 bind 成功 就返回true */
	bool IsBound();

	/* 返回 bind 的端口号 */
	port_t GetPort();

	void OnOptions(int, int, int, SOCKET) {};

	int GetLastSizeWritten();

	/* 还要从传入消息中读取时间戳信息 */
	void SetTimestamp(bool = true);

protected:
	UdpSocket(const UdpSocket& s) : Socket(s) {} 

	void OnRead();

private:
	UdpSocket& operator=(const UdpSocket&) { return *this; }; 

	/* 在使用 sendto 方法之前创建 */
	void CreateConnection();
private:
	char* m_ibuf;  //< 输入缓冲区
	int m_ibufsz;  //< 输入缓冲区的大小
	bool m_bind_ok;  //< 绑定成功完成
	port_t m_port;  //< 绑定的端口
	int m_last_size_written;
	int m_retries;
	bool m_b_read_ts;
};

#endif // __SOCKET_UdpSocket_H__

