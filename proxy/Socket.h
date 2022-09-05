/*
 * Auther: ting
 * Date: 2022-08-31
 * Email: vlicecream@163.com
 * Detail: This file currently only supports connection pooling
 * */

#ifndef __SOCKETs_Socket_H__
#define __SOCKETs_Socket_H__


#include <time.h>
#include <string>
#include <vector>
#include <list>
#include <memory>

#include "SocketConfig.h"
#include "SocketInclude.h"

class ISocketHandler;
class SocketAddress;
class IFile;
class SocketThread;

class Socket {
public:
	// default
	Socket(ISocketHandler&);
	virtual ~Socket();

	/* Socket 实例化类的方法 */
	virtual Socket* Create() { return NULL; }
	
	/* 返回对拥有套接字的套接字处理程序的引用。如果套接字已分离，则这是对从套接字处理程序的引用 */
	ISocketHandler& Handler() const;

	/* 返回对拥有套接字的套接字处理程序的引用。这个总是返回对原始套接字处理程序的引用，即使套接字已分离 */
	ISocketHandler& MasterHandler() const;
	/* 
	 * 在accept之后但在socket被添加到handler之前由ListenSocket调用
	 * CTcpSocket使用它来创建它的ICrypt成员变量
	 * ICrypt成员变量是由虚方法创建的，因此它不能直接从CTcpSocket构造函数中调用
	 * 也用于判断传入的HTTP连接是否正常（80端口）或ssl（443端口）
	 * */
	virtual void Init();

	/* 创建 socket 文件描述符
	 * Params: af -> Address family AF_INET / AF_INET6 / ...
	 *				 type -> SOCK_STREAM / SOCK_DGRAM / ...
	 *				 protocol -> "TCP" / "UDP" / ...
	 * */
	SOCKET CreateSocket(int af, int type, const std::string& protocol = "");
	
	/* 为这个 socket 分配一个由 socket() 或其他方法 产生的文件描述符 */
	void Attach(SOCKET s);

	/* 返回这个socket分配过的文件描述符 */
	SOCKET GetSocket();

	/* 立刻关闭链接 - 内部使用 */
	virtual int Close();

	/* 当套接字文件描述符有效且套接字不打算关闭时返回true */
	virtual bool Ready();


	/* 返回指向在传入连接上创建此实例的 ListenSocket 的指针 */
	Socket *GetParent();

	/** 被 ListenSocket 用来设置新创建的父指针
	 *  套接字实例 */
	void SetParent(Socket *);

	/** 从 ListenSocket<> 获取监听端口 */
	virtual port_t GetPort();

	/* 设置 socket 非阻塞操作 */
	bool SetNonblocking(bool);

	/* 设置 socket 非阻塞操作 */
	bool SetNonblocking(bool, SOCKET);

	/* 实例的总生命周期 */
	time_t Uptime();

	/* 设置上次 connect() 的 地址/端口 */
	void SetClientRemoteAddress(SocketAddress&);

	/* 获得上次 connect() 的 地址/端口 */
	std::unique_ptr<SocketAddress> GetClientRemoteAddress();

	/* TCP & UDP 使用的公共接口 - 设置 缓冲区大小 */
	virtual void SendBuf(const char *,size_t,int = 0);

	/* TCP & UDP 使用的公共接口 - 发送 */
	virtual void Send(const std::string&,int = 0);

	/* 出站流量计数器 */
	virtual uint64_t GetBytesSent(bool clear = false);

	/* 进站流量计数器 */
	virtual uint64_t GetBytesReceived(bool clear = false);

	// LIST_TIMEOUT

	/* 使用超时控制 0=disable 超时检查 */
	void SetTimeout(time_t secs);

	bool CheckTimeout();

	/* 超时检查 / 到达超时限制就返回 true */
	bool Timeout(time_t tnow);

	/* 由 ListenSocket 使用 ipv4 and ipv6 */
	void SetRemoteAddress(SocketAddress&);

	// 名称事件回调
	
	/* 当文件描述符中有内容要读取时调用 */
	virtual void OnRead();

	/* 当文件描述符上有另一个写入空间时调用 */
	virtual void OnWrite();

	/* socket 异常调用 */
	virtual void OnException();

	/* 在 ISocketHandler 删除套接字类之前调用 */
	virtual void OnDelete();

	/* 当连接完成时 调用 */
	virtual void OnConnect();

	/* 当传入连接完成时调用 */
	virtual void OnAccept();
	/* 在读取完整行并且套接字在其中时调用
	 * 线路协议模式 */
	virtual void OnLine(const std::string& );

	/* 超时调用 (5s). */
	virtual void OnConnectFailed();

	/* 在创建客户端套接字时调用，以设置套接字选项
		param family -> AF_INET, AF_INET6, etc
		param type -> SOCK_STREAM, SOCK_DGRAM, etc
		param protocol -> Protocol number (tcp, udp, sctp, etc)
		param s Socket -> file descriptor
	*/
	virtual void OnOptions(int family,int type,int protocol,SOCKET s) = 0;

	/* 连接重试回调 - 返回 false 以中止连接尝试 */
	virtual bool OnConnectRetry();

// #ifdef ENABLE_RECONNECT
//   [>* a reconnect has been made <]
//   virtual void OnReconnect();
// #endif
//
//
	/* TcpSocket: 当检测到断开连接时 (recv/SSL_read returns 0 bytes) */
	virtual void OnDisconnect();

	/* TcpSocket: 当检测到断开连接时 (recv/SSL_read returns 0 bytes). 
		param info bit 0 read(0)/write(1)
		            bit 1 normal(read or write returned 0)/error(r/w returned -1)
		            bit 2 ssl
		param code error code from read/write call (errno / ssl error) */
	virtual void OnDisconnect(short info, int code);

	/* 超时回调 */
	virtual void OnTimeout();
	/* 连接超时 */
	virtual void OnConnectTimeout();

	/* 命名 Socket 模式标志, set/reset */

	/* 当您希望套接字处理程序在使用后删除套接字实例时，通过处理程序将 delete 设置为 true */
	void SetDeleteByHandler(bool = true);

	/* 通过处理程序标志检查删除
			如果套接字处理程序应删除此实例，则返回 true */
	bool DeleteByHandler();

	// LIST_CLOSE - 条件事件队列

	/* 设置关闭和删除以终止连接 */
	void SetCloseAndDelete(bool = true);

	/* 检查关闭和删除标志
			（如果应该关闭此套接字并删除实例，则返回 true */
	bool CloseAndDelete();

	/* 返回自从命令关闭套接字以来的秒数 \sa SetCloseAndDelete */
	time_t TimeSinceClose();

	/* 忽略只读取 socket 的事件. */
	void DisableRead(bool x = true);

	/* 检查忽略读取事件标志
			（如果应该忽略读取事件，则返回 true */
	bool IsDisableRead();

	/* 设置连接状态 */
	void SetConnected(bool = true);

	/* 检查连接状态
		  如果连接就返回true */
	bool IsConnected();

	/* 连接丢失-读取/写入套接字时出错-仅TcpSocket */
	void SetLost();

	/* 检查连接丢失状态标志，仅由 TcpSocket 使用
				如果在 r/w 导致套接字关闭时出现错误，则返回 true */
	bool Lost();

	/* 设置标志表明套接字正在被套接字处理程序主动删除 */
	void SetErasedByHandler(bool x = true);
	/* 获取指示套接字的标志值被套接字处理程序删除。 */
	bool ErasedByHandler();

	//@}

	/* name 远程连接信息 */
	//@{
	/* 返回远端地址 */
	std::unique_ptr<SocketAddress> GetRemoteSocketAddress();
	/* 返回远端地址: ipv4. */
	ipaddr_t GetRemoteIP4();
// #ifdef ENABLE_IPV6
//   [> 返回远端地址: ipv6. <]
// #ifdef IPPROTO_IPV6
//   struct in6_addr GetRemoteIP6();
// #endif
// #endif

	/* 返回远程端口号: ipv4 and ipv6. */
	port_t GetRemotePort();

	/* 返回远程ip为string? ipv4 and ipv6. */
	std::string GetRemoteAddress();
	//@}
	
	/* ipv4 and ipv6(未实现) */
	std::string GetRemoteHostname();

	/* 返回绑定套接字文件描述符的本地端口号 */
	port_t GetSockPort();

	/* 返回绑定套接字文件描述符的本地ipv4地址 */
	ipaddr_t GetSockIP4();

	/* 返回绑定套接字文件描述符的本地 ipv4 地址 */
	std::string GetSockAddress();

// #ifdef ENABLE_IPV6
// #ifdef IPPROTO_IPV6
//   [> 返回绑定套接字文件描述符的本地 ipv6 地址 <]
//   struct in6_addr GetSockIP6();
//   [>* 返回本地 ipv6 地址作为绑定套接字文件描述符的文本. <]
//   std::string GetSockAddress6();
// #endif
// #endif
	socketuid_t UniqueIdentifier() { return m_uid; }

protected:
	/* 默认构造函数不可用 */
	Socket() : m_handler(m_handler) {}

	/* 复制构造函数不可用 */
	Socket(const Socket& s) : m_handler(s.m_handler) {}

	/* 赋值操作符不可用 */
	Socket& operator=(const Socket& ) { return *this; }

	/* 如果设置了 所有流量都将写入这个文件 */
	IFile *GetTrafficMonitor() { return m_traffic_monitor; }

	// #ifdef HAVE_OPENSSL
	// bool m_b_enable_ssl; //< 为此 TcpSocket 启用 SSL
	// bool m_b_ssl; //< SSL协商模式(TcpSocket)
	// bool m_b_ssl_server; //< 如果传入的是 SSL TcpSocket 则为 true
	// #endif

	// SOCKET options
	bool SoAcceptconn();
	bool SetSoBroadcast(bool x = true);
	bool SetSoDebug(bool x = true);
	int SoError();
	bool SetSoDontroute(bool x = true);
	bool SetSoLinger(int onoff, int linger);
	bool SetSoOobinline(bool x = true);
	bool SetSoRcvlowat(int);
	bool SetSoSndlowat(int);
	bool SetSoRcvtimeo(struct timeval&);
	bool SetSoSndtimeo(struct timeval&);
	bool SetSoRcvbuf(int);
	int SoRcvbuf();
	bool SetSoSndbuf(int);
	int SoSndbuf();
	int SoType();
	bool SetSoReuseaddr(bool x = true);
	bool SetSoKeepalive(bool x = true);

	#ifdef ENABLE_POOL
	int m_socket_type; //< 套接字类型，来自 socket() 调用
	std::string m_socket_protocol; //< Protocol, 来自 socket() 调用
	bool m_bClient; //< 只有客户端连接是池
	bool m_bRetain; //< 保持连接关闭
	#endif

	// #ifdef ENABLE_DETACH
	// bool m_detach; //< 套接字命令分离标志
	// bool m_detached; //< 套接字已分离
	// SocketThread *m_pThread; //< 分离套接字线程类指针
	// ISocketHandler *m_slave_handler; //< 分离时的实际套接字处理程序
	// #endif

private:
	ISocketHandler& m_handler;	//< socket 中的 headler 引用
	SOCKET m_socket; //< 文件描述符
	bool m_bDel; //< 删除 handler 标志
	bool m_bClose; //< 关闭并删除 标志
	time_t m_tCreate; //< socket 创建的时间
	Socket* m_parent; //< 指向 ListenSocket 类的指针 对传入的 socket 有效
	bool m_b_disable_read; //< 禁用读取时间检查
	bool m_connected; //< socket 是否连接 (tcp/udp)
	bool m_b_erased_by_handler; //< 在删除之前由 handler 设置
	time_t m_tClose; //< 下令关闭的时间（以秒为单位）
	std::unique_ptr<SocketAddress> m_client_remote_address; //< 最后一个connect()的地址
	std::unique_ptr<SocketAddress> m_remote_address; //< 远端地址	
	IFile *m_traffic_monitor;
	time_t m_timeout_start; //< 由 SetTimeout 设置
	time_t m_timeout_limit; //< 由 SetTimeout 定义
	bool m_bLost; //< 连接丢失
	static socketuid_t m_next_uid;
	socketuid_t m_uid;
	bool m_call_on_connect; //< OnConnect 将在下一个 ISocketHandler 循环中调用
	bool m_b_retry_connect; //< 在下一个 ISocketHandler 周期尝试另一个连接尝试



};

#endif // __SOCKETs_Socket_H__
