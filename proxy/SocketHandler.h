/*
 * Auther:
 * Date: 2022-09-08
 * Email: vlicecream@163.com
 * Deatil: 
 * */

#ifndef __SOCKET_SocketHandler_H__
#define __SOCKET_SocketHandler_H__

#include <sys/epoll.h>
#include <map>
#include <list>

#include "SocketConfig.h"
#include "SocketInclude.h"
#include "StdLog.h"
#include "ISocketHandler.h"

class Socket;
class IMutex;
class UdpSocket;
class SocketHandlerThread;

/* socket 容器类 - 事件生成器 */
class SocketHandler : public ISocketHandler 
{
protected:
	/* 用于保存文件描述符/套接字对象指针的映射类型 */
	typedef std::map<SOCKET, Socket*> socket_m;	

public:
	/* 构造函数 */
	SocketHandler(StdLog *log = nullptr);

	/* 保证线程安全的构造函数 */
	SocketHandler(IMutex& mutex, StdLog *log = nullptr);
	SocketHandler(IMutex&, ISocketHandler& parent, StdLog* = nullptr);

	/* 析构函数 */
	~SocketHandler();

	virtual ISocketHandler* Create(IMutex&, ISocketHandler&, StdLog* = nullptr) override;
	virtual ISocketHandler* Create(StdLog* = nullptr) override;

	virtual bool ParentHandlerIsValid() override;

	virtual ISocketHandler& ParentHandler() override;
	virtual ISocketHandler& GetRandomHandler() override;
	virtual ISocketHandler& GetEffectiveHandler() override;

	virtual void SetNumberOfThreads(size_t n) override;
	virtual bool IsThreaded() override;

	/* UdpSocket */
	virtual void EnableRelease() override;
	virtual void Release() override;
	
	/* 获取线程安全操作的互斥量参考 */
	IMutex& GetMutex() const override;

	/* 为错误回调注册 StdLog 对象
			param -> 指向日志类的指针 */
	void RegStdLog(StdLog* log) override;

	/* 将错误记录到日志类以进行打印/存储 */
	void LogError(Socket *p, const std::string& user_text, int err, const std::string& sys_err, loglevel_t t = LOG_LEVEL_ERROR) override;

	/* 将套接字实例添加到套接字映射。删除总是自动的 */
	void Add(Socket*) override;

	/* 设置读/写/异常文件描述符集 (fd_set) */
	void ISocketHandler_Add(Socket*, bool bRead, bool bWrite) override;
	void ISocketHandler_Mod(Socket*, bool bRead, bool bWrite) override;
	void ISocketHandler_Del(Socket*) override;

	/* 等待事件 生成回调 */
	int Select(long sec, long usec) override;

	/* 在检测到事件之前，此方法不会返回 */
	int Select() override;

	/* 等待事件 生成回调 */
	int Select(struct timeval* tsel) override;

	/* 检查一个套接字是否真的被这个套接字处理程序处理了 */
	bool Valid(Socket*) override;
	bool Valid(socketuid_t) override;

	/* 返回此处理程序处理的套接字数目 */
	size_t GetCount() override;
	size_t MaxCount() override { return FD_SETSIZE; };
	
	/* 覆盖并返回 false 以拒绝所有传入连接
	   param -> p ListenSocket 类指针（使用 GetPort 识别是哪一个） */
	bool OkToAccept(Socket *p) override;

	/* 小心使用，如果是多线程的，一定要用h.GetMutex()锁定 */
	const std::map<SOCKET, Socket*>& AllSockets() override { return m_sockets; };

	size_t MaxTcpLineSize() override { return TCP_LINE_SIZE; };

	void SetCallOnConnect(bool = true) override;
	void SetDetach(bool = true) override;
	void SetTimeout(bool = true) override;
	void SetRetry(bool = true) override;
	void SetClose(bool = true) override;

public:
#ifdef ENABLE_POOL
	/* 查找可用的打开连接（由连接池使用） */
	ISocketHandler::PoolSocket* FindConnection(int type, const std::string& protocol, SocketAddress&) override;

	/* 启用连接池(默认禁用) */
	void EnablePool(bool b = true) override;

	/* 检查池状态
			如果启用了连接池，则返回 true */
	bool PoolEnabled() override;
#endif

#ifdef ENABLE_DETACH
	/* 表示处理程序在 SocketThread 下运行 */ 
	void SetSlave(bool b = true) override;

	/* 表示处理程序在 SocketThread 下运行 */
	bool IsSlave() override;
#endif // ENABLE_DETACH



protected:
	socket_m m_sockets;  //< 活动 socket 映射
	std::list<Socket*> m_add;  //< 套接字将被添加到套接字映射中
	std::list<Socket*> m_delete;  //< 要删除的套接字（添加时失败）
																
protected:
	/* 实际调用 select() */
	int ISocketHandler_Select(struct timeval*) override;

	/* 从socket映射中移除socket，由socket类使用 */
	void Remove(Socket*) override;

	/* 计划删除套接字 */
	void DeleteSocket(Socket*);

	void AddIncoming();
	void CheckCallOnConnect();
	void CheckErasedSockets();
	void CheckDetach();
	void CheckTimeout(time_t);
	void CheckRetry();
	void CheckClose();
#ifdef ENABLE_EPOLL

	/* 实际调用epoll 处理事件 */
	 void ISocketHandler_Epoll(int epollFd);

	/* 创建一个epoll 并把socket包装成一个epoll_event对象 添加到epoll中 */
	int CreateEpoll();

	/* 关闭连接 */
	void EpollClose(epoll_event* event);

	/* 新增连接 */
	void AddEpoll(int EpollFd, int listenFd);

	/* 删除连接 */
	void RemoveEpoll(int EpollFd, int listenFd);

#endif

protected:
	StdLog* m_stdlog;
	IMutex& m_mutex;
	bool m_b_use_mutex;
	ISocketHandler& m_parent;
	bool m_b_parent_is_valid;



private:
	static FILE *m_event_file;
	static unsigned long m_evet_counter;
private:
	void RebuildFdset();
	void Set(Socket*, bool, bool);
private:
	std::list<SocketHandlerThread*> m_threads;
	UdpSocket* m_release;
	SOCKET m_maxsock;  //< 活动套接字列表中最高的文件描述符+ 1
	fd_set m_rfds;  //< 监控读取事件的文件描述符集
	fd_set m_wfds;  //< 监控写事件的文件描述符集
	fd_set m_efds;  //< 监控异常的文件描述符集
	time_t m_tlast;  //< 超时控制

	bool m_b_nonblock; //< 是否设置为非阻塞模式

	std::list<socketuid_t> m_fds_erase;  //< 要从m_sockets中删除的文件描述符

	bool m_b_check_callonconnect;
	bool m_b_check_detach;
	bool m_b_check_timeout;
	bool m_b_check_retry;
	bool m_b_check_close;
	
#ifdef ENABLE_POOL
	bool m_b_enable_pool;
#endif 

#ifdef ENABLE_DETACH
	bool m_slave; //< 指示这是一个在SocketThread中运行的ISocketHandler
#endif

#ifdef ENABLE_EPOLL

	epoll_event m_epev;

	int m_max_events;  // 最大接收事件

	int m_timeout; // epoll_wait 的 超时时间 0 为立即返回 -1 为永久阻塞

	epoll_event* m_events{};  //回调事件的数组,当epoll中有响应事件时,通过这个数组返回

	bool m_b_epoll_in;  // 是否注册可读事件
	bool m_b_epoll_et;  // 是否改为ET边缘模式
	bool m_b_epoll_out;  // 是否注册可写事件
#endif

};


#endif // __SOCKET_SocketHandler_H__
