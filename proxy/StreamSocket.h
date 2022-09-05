/*
 * Auther: icecream-xu
 * Date: 2022-09-03
 * Email: vlicecream@163.com
 * Detail:
 **/

#ifndef __SOCKET_StreamSocket_H__
#define __SOCKET_StreamSocket_H__

#include "Socket.h"

class StreamSocket : public Socket
{
public:
	StreamSocket(ISocketHandler&);
	~StreamSocket();

	/* 套接字应该在 select() 的下一个写入事件上检查 Connect */
	void SetConnecting(bool = true);

	/* 检查连接标志
			如果套接字仍在尝试连接，则返回 true */
	bool Connecting();

	/* 当 socket 文件描述符有效，socket 连接建立 
	    socket 不关闭时返回 true */
	bool Ready();

	/* 设置连接超时时间
		  param x -> 超时时间 */
	void SetConnectTimeout(int x);

	/* 返回等待连接的秒数。
			返回连接超时（秒）*/
	int GetConnectTimeout();

	/* 在关闭之前设置刷新以使 tcp 套接字在关闭连接之前完全清空其输出缓冲区 */
	void SetFlushBeforeClose(bool = true);

	/* 在状态之前检查刷新
			如果套接字应该在关闭之前发送所有数据，则返回 true */
	bool GetFlushBeforeClose();

	/* 定义重新连接次数 (tcp only).
	    n = 0 - 不重试
	    n > 0 - 重试的次数
	    n = -1 - 一直重试 */
	void SetConnectionRetry(int n);

	/* 获得最大的重试连接次数 (tcp only). */
	int GetConnectionRetry();

	/* 增加实际连接重试次数 (tcp only). */
	void IncreaseConnectionRetries();

	/* 获取实际的连接重试次数 (tcp only). */
	int GetConnectionRetries();

	/* 重置实际连接次数 (tcp only). */
	void ResetConnectionRetries();

	/* 如果套接字处于线路协议模式，则在 OnRead 之后调用		
			\sa SetLineProtocol */
	/* 启用在线回调
			使用时不要创建自己的 OnRead* 回调 */
	virtual void SetLineProtocol(bool = true);

	/* 检查线路协议模式
			如果socket是线路协议模式，则返回true */
	bool LineProtocol();

	/* 设置关机状态 */
	void SetShutdown(int);

	/** 获得关机状态 */
	int GetShutdown();

	/* 返回 IPPROTO_TCP or IPPROTO_SCTP */
	virtual int Protocol() = 0;

protected:
	StreamSocket(const StreamSocket& src) : Socket(src) {} // copy constructor

private:
	StreamSocket& operator=(const StreamSocket& ) { return *this; } // assignment operator
	bool m_bConnecting; //< Flag indicating connection in progress
	int m_connect_timeout; //< 连接超时 (seconds)
	bool m_flush_before_close; //< 关闭前发送所有数据 (default true)
	int m_connection_retry; //< 最大的重新连接次数 (tcp)
	int m_retries; //< 实际的重新连接次数 (tcp)
	bool m_line_protocol; //< 线路协议模式标志	
	int m_shutdown; //< 关机状态

};

#endif // __SOCKET_StreamSocket_H__
