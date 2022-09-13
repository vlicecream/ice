/*
 * Auther:
 * Date: 2022-09-08
 * Email: vlicecream@163.com
 * Deatil: 
 * */

#ifndef __SOCKET_SocketHandler_H__
#define __SOCKET_SocketHandler_H__

#include <map>
#include <list>

#include "UdpSocket.h"
#include "SocketConfig.h"
#include "ISocketHandler.h"
#include "SocketInclude.h"
// #include "Socket.h"
#include "StdLog.h"
#include "IMutex.h"
#include "SocketHandlerThread.h"

class ResolveServer;

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
	void ISocketHandler_Add(Socket *, bool bRead, bool bWrite) override;
  void ISocketHandler_Mod(Socket *, bool bRead, bool bWrite) override;
  void ISocketHandler_Del(Socket *) override;

	/* 等待事件 生成回调 */
	int Select(long sec, long usec) override;

	/* 在检测到事件之前，此方法不会返回 */
	int Select() override;

	/* 等待事件 生成回调 */
	int Select(struct timeval* tsel) override;

	/* 检查一个套接字是否真的被这个套接字处理程序处理了 */
	bool Valid(Socket *) override;
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

private:
	static FILE *m_event_file;
	static unsigned long m_evet_counter;

public:
#ifdef ENABLE_POOL
	/* 查找可用的打开连接（由连接池使用） */
	ISocketHandler::PoolSocket* FindConnection(int type, const std::string& protocol, SocketAddress&) override;

	/* 启用连接池(默认禁用) */
	void EnablePool(bool x = true) override;

	/* 检查池状态
			如果启用了连接池，则返回 true */
	bool PoolEnabled() override;
#endif

#ifdef ENABLE_RESOLVER
	/* 开启异步DNS
			param -> port 异步DNS服务器监听端口 */
	void EnabelResolver(port_t = 16667) override;

	/* 检查解析器状态 (如果启用了解析器, 则返回 true) */
	bool ResolverEnabled() override;

	/* 排队一个 dns 请求
			param -> host 要解析的主机名
			param -> port 端口号将在 Socket::OnResolved 回调中回显 */
	int Resolve(Socket*, const std::string& host, port_t port) override;

	/* 做一个反向 dns 查找 */
	int Resolve(Socket*, ipaddr_t ipaddr) override;

	/* 获取异步 dns 服务器的监听端口 */
	port_t GetResolverPort() override;

	/* 解析器线程准备好查询 */
	bool ResolverReady() override;

	/* 如果套接字等待解决事件，则返回true */
	bool Resolving(Socket*) override;
#endif

#ifdef ENABLE_DETACH

	/* 表示处理程序在 SocketThread 下运行 */ 
	void SetSlave(bool b = true) override;

	/* 表示处理程序在 SocketThread 下运行 */
	bool IsSlave() override;
#endif // ENABLE_DETACH
			 //
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

	StdLog* m_stdlog;
	IMutex& m_mutex;
	bool m_b_use_mutex;
	ISocketHandler& m_parent;
	bool m_b_parent_is_valid;

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

	std::list<socketuid_t> m_fds_erase;  //< 要从m_sockets中删除的文件描述符

	bool m_b_check_callonconnect;
  bool m_b_check_detach;
  bool m_b_check_timeout;
  bool m_b_check_retry;
  bool m_b_check_close;
	
#ifdef ENABLE_POOL
	bool m_b_enable_poll;
#endif 

#ifdef ENABLE_RESOLVER
	int m_resolv_id; //< 解析器 id 计数器
	ResolveServer* m_resolver; //< 解析器线程指针
	port_t m_resolver_port; //< 解析器监听端口
	std::map<socketuid_t, bool> m_resolve_q; //< 解决队列
#endif

#ifdef ENABLE_DETACH
	bool m_slave; //< 指示这是一个在SocketThread中运行的ISocketHandler
#endif

};


#endif // __SOCKET_SocketHandler_H__
