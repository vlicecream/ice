/*
 * Auther: ting
 * Date: 2022-09-02
 * Email: vlicecream@163.com
 * */

#ifndef __SOCKET_IHANDLER_H__
#define __SOCKET_IHANDLER_H__

#include <list>
#include <map>

#include "SocketConfig.h"
#include "SocketInclude.h"
#include "Socket.h"
#include "StdLog.h"

// class SocketAddress;
// class IMutex;

class ISocketHandler
{
	
	friend class Socket;

public:
	/* ISocketHandler 内部使用的连接池类 */
#ifdef ENABLE_POOL
	class PoolSocket : public Socket 
	{
		public:
			PoolSocket(ISocketHandler& h, Socket *src) : Socket(h) {
				CopyConnection(src);
				SetIsClient();
			}

			virtual ~PoolSocket() {};

			void OnRead() {
				Handler().LogError(this, "OnRead", 0, "data on hibernating socket", LOG_LEVEL_FATAL);
				SetCloseAndDelete();
			}

			void OnOptions(int,int,int,SOCKET) {}
	};
#endif

public:
	virtual ~ISocketHandler() {}

	/* 返回另一个实例 */
	virtual ISocketHandler* Create(StdLog* = nullptr) = 0;
	virtual ISocketHandler* ISocketHandler::Create(IMutex&, ISocketHandler&, StdLog* = nullptr) = 0;

	/* 使用父级创建的处理程序 */
	virtual bool ParentHandlerIsValid() = 0;

	/* 获得父类的 SocketHandler */
	virtual ISocketHandler& ParentHandler() = 0;

	/* 获取连接最少的线程处理程序 */
	virtual ISocketHandler& GetRandomHandler() = 0;

	/* 如果有效则返回父处理程序，否则返回正常处理程序 */
	virtual ISocketHandler& GetEffectiveHandler() = 0;

	/* 启用线程 */
	virtual void SetNumberOfThreads(size_t n) = 0;

	/* 线程已启用 */
	virtual bool IsThreaded() = 0;

	/* 启用 select 发布 */
	virtual void EnableRelease() = 0;

	/* 选择发布 */
	virtual void Release() = 0;

	/* 获取线程安全操作的互斥锁引用. */
	virtual IMutex& GetMutex() const = 0;

	/* 为错误回调注册 StdLog 对象。
			参数 log 指向日志类的指针 */
	virtual void RegStdLog(StdLog *log) = 0;

	/* 将错误记录到日志类以进行打印/存储 */
	virtual void LogError(Socket *p,const std::string& user_text,int err,const std::string& sys_err,loglevel_t t = LOG_LEVEL_WARNING) = 0;

	/* 将套接字实例添加到套接字映射
			删除总是自动的 */
	virtual void Add(Socket*) = 0;

protected:
	/* 从 Socket 类使用的套接字映射中删除套接字。 */
	virtual void Remove(Socket *) = 0;

	/* 实际调用 select() */
	virtual int ISocketHandler_Select(struct timeval *) = 0;

public:
	/* 设置读/写/异常文件描述符集 (fd_set) */
	virtual void ISocketHandler_Add(Socket *,bool bRead,bool bWrite) = 0;
	virtual void ISocketHandler_Mod(Socket *,bool bRead,bool bWrite) = 0;
	virtual void ISocketHandler_Del(Socket *) = 0;

	/* 等待事件，生成回调 */
	virtual int Select(long sec,long usec) = 0;
	/* 在检测到事件之前，此方法不会返回 */
	virtual int Select() = 0;

	/* 等待事件，生成回调 */
	virtual int Select(struct timeval *tsel) = 0;

	/* 检查一个套接字是否真的由这个套接字处理程序处理 */
	virtual bool Valid(Socket *) = 0;
	/* 首选方法-检查一个套接字是否仍然被这个套接字处理程序处理 */
	virtual bool Valid(socketuid_t) = 0;

	/* 返回此处理程序处理的套接字的数目 */
	virtual size_t GetCount() = 0;

	/* 返回允许的最大套接字数 */
	virtual size_t MaxCount() = 0;

	/* 覆盖并返回 false 以拒绝所有传入连接
			param p -> ListenSocket 类指针（使用 GetPort 识别是哪一个 */
	virtual bool OkToAccept(Socket *p) = 0;

	/* 小心使用，如果多线程，请始终使用 h.GetMutex() 锁定 */
	virtual const std::map<SOCKET, Socket *>& AllSockets() = 0;

	/* 覆盖以接受比 TCP_LINE_SIZE 更长的行 */
	virtual size_t MaxTcpLineSize() = 0;

	virtual void SetCallOnConnect(bool = true) = 0;
	virtual void SetDetach(bool = true) = 0;
	virtual void SetTimeout(bool = true) = 0;
	virtual void SetRetry(bool = true) = 0;
	virtual void SetClose(bool = true) = 0;

#ifdef ENABLE_POOL
	/* 查找可用的打开连接（由连接池使用） */
	virtual ISocketHandler::PoolSocket *FindConnection(int type,const std::string& protocol,SocketAddress&) = 0;

	/* 启用连接池（默认禁用）。 */
	virtual void EnablePool(bool = true) = 0;

	/* 检查连接池状态
		  如果启用返回 true */
	virtual bool PoolEnabled() = 0;
#endif // ENABLE_POOL

#ifdef ENABLE_RESOLVER
	/* 开启异步DNS
			param -> port 异步DNS服务器监听端口 */
	virtual void EnabelResolver(port_t = 16667) = 0;

	/* 检查解析器状态 (如果启用了解析器, 则返回 true) */
	virtual bool ResolverEnabled() = 0;

	/* 排队一个 dns 请求
			param -> host 要解析的主机名
			param -> port 端口号将在 Socket::OnResolved 回调中回显 */
	virtual int Resolve(Socket*, const std::string& host, port_t port) = 0;

	/* 做一个反向 dns 查找 */
	virtual int Resolve(Socket*, ipaddr_t ipaddr) = 0;

	/* 获取异步 dns 服务器的监听端口 */
	virtual port_t GetResolverPort() = 0;

	/* 解析器线程准备好查询 */
	virtual bool ResolverReady() = 0;

	/* 如果套接字等待解决事件，则返回true */
	virtual bool Resolving(Socket*) = 0;
#endif

#ifdef ENABLE_DETACH
	/* 表示处理程序在 SocketThread 下运行 */ 
	virtual void SetSlave(bool b = true) = 0;

	/* 表示处理程序在 SocketThread 下运行 */
	virtual bool IsSlave() = 0;
#endif // ENABLE_DETACH

};

#endif // __SOCKET_IHANDLER_H__
