/*
 * Auther:
 * Date: 2022-09-08
 * Email: vlicecrea@163.com
 * */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "SocketHandler.h"
#include "Utility.h"
#include "Mutex.h"
#include "SocketAddress.h"
#include "Lock.h"
#include "UdpSocket.h"
#include "SocketHandlerThread.h"
#include "Exception.h"
#include "StreamSocket.h"
#include "TcpSocket.h"

#ifdef _DEBUG
#define DEB(x) x; fflush(stderr);
#else
#define DEB(x)
#endif

SocketHandler::SocketHandler(StdLog* log) 
	:
		m_stdlog(log),
		m_mutex(m_mutex),
		m_b_use_mutex(false),
		m_parent(m_parent),
		m_b_parent_is_valid(false),

		m_maxsock(0),
		m_release(nullptr),
		m_tlast(0),
		m_b_check_close(false),
		m_b_check_retry(false),
		m_b_check_detach(false),
		m_b_check_timeout(false),
		m_b_check_callonconnect(false),
#ifdef ENABLE_POOL
		m_b_enable_pool(false),
#endif
#ifdef ENABLE_DETACH
		m_slave(false)
#endif
{
	FD_ZERO(&m_rfds);
	FD_ZERO(&m_wfds);
	FD_ZERO(&m_efds);
}

SocketHandler::SocketHandler(IMutex& mutex, StdLog* log)
	:
		m_stdlog(log),
		m_mutex(m_mutex),
		m_b_use_mutex(false),
		m_parent(m_parent),
		m_b_parent_is_valid(false),

		m_maxsock(0),
		m_release(nullptr),
		m_tlast(0),
		m_b_check_close(false),
		m_b_check_retry(false),
		m_b_check_detach(false),
		m_b_check_timeout(false),
		m_b_check_callonconnect(false),
#ifdef ENABLE_POOL
		m_b_enable_pool(false),
#endif
#ifdef ENABLE_DETACH
		m_slave(false)
#endif
{
	m_mutex.Lock();
	FD_ZERO(&m_rfds);
	FD_ZERO(&m_wfds);
	FD_ZERO(&m_efds);
}

SocketHandler::SocketHandler(IMutex& mutex, ISocketHandler& handler, StdLog* log) 
	:
		m_stdlog(log),
		m_mutex(m_mutex),
		m_b_use_mutex(false),
		m_parent(m_parent),
		m_b_parent_is_valid(false),

		m_maxsock(0),
		m_release(nullptr),
		m_tlast(0),
		m_b_check_close(false),
		m_b_check_retry(false),
		m_b_check_detach(false),
		m_b_check_timeout(false),
		m_b_check_callonconnect(false),
#ifdef ENABLE_POOL
		m_b_enable_pool(false),
#endif
#ifdef ENABLE_DETACH
		m_slave(false)
#endif
{
	m_mutex.Lock();
	FD_ZERO(&m_rfds);
	FD_ZERO(&m_wfds);
	FD_ZERO(&m_efds);
}

SocketHandler::~SocketHandler()
{
	for (std::list<SocketHandlerThread*>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
		SocketHandlerThread *p = *it;
		p -> Stop();
	}
	while (m_sockets.size()) {
		DEB(fprintf(stderr, "Emptying sockets list in SocketHandler destructor, %d instancs\n", (int)m_sockets.size());)
		socket_m::iterator it = m_sockets.begin();
		Socket *p = it -> second;
		if (p) {
			DEB(fprintf(stderr, "fd %d\n", p->GetSocket());)
			p->Close();
			DEB(fprintf(stderr, "fd close %d\n", p->GetSocket());)
			if (p -> DeleteByHandler()
#ifdef ENABLE_DETACH
					&& !(m_slave ^ p -> IsDetached())
#endif
				 ) {
				p -> SetErasedByHandler();
				delete p;
			}
			m_sockets.erase(it);
		} else {
			m_sockets.erase(it);
		}
		DEB(fprintf(stderr, "next\n");)
	}
	DEB(fprintf(stderr, "Emptying sockets list in SocketHandler destructor, %d instances\n", (int)m_sockets.size());)
	if (m_b_use_mutex) {
		m_mutex.Unlock();
	}
}

ISocketHandler* SocketHandler::Create(StdLog* log)
{
	return new SocketHandler(log);
}

ISocketHandler* SocketHandler::Create(IMutex& mutex, ISocketHandler& handler, StdLog* log)
{
	return new SocketHandler(mutex, handler, log);
}

bool SocketHandler::ParentHandlerIsValid()
{
	return m_b_parent_is_valid;
}

ISocketHandler& SocketHandler::ParentHandler()
{
	if (!m_b_parent_is_valid) {
		throw Exception("No parent sockethandler available");
	}
	return m_parent;
}

ISocketHandler& SocketHandler::GetRandomHandler()
{
	if (m_threads.empty()) {
		throw Exception("SocketHandler is not multithreaded");
	}
	size_t min_count = 99999;
	SocketHandlerThread* match = nullptr;

	for (std::list<SocketHandlerThread*>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
		SocketHandlerThread* thread = *it;
		ISocketHandler& h = thread -> Handler();
		{
			Lock lock(h.GetMutex());
			size_t sz = h.GetCount();
			if (sz < min_count) {
				min_count = sz;
				match = thread;
			}
		}
	}

	if (match) {
		return match -> Handler();
	}
	throw Exception("Can't locate free threaded sockethandler");
}

ISocketHandler& SocketHandler::GetEffectiveHandler()
{
	return m_b_parent_is_valid ? m_parent : *this;
}

void SocketHandler::SetNumberOfThreads(size_t n)
{
	if (!m_threads.empty()) {
		return;
	}

	if (n > 1 && n < 256) {
		for (int i = 1; i <= (int)n; i++) {
			SocketHandlerThread* p = new SocketHandlerThread(*this);
			m_threads.push_back(p);
			p -> SetDeleteOnExit();
			p -> Start();
			p -> Wait();
		}
	}
}

bool SocketHandler::IsThreaded()
{
	return !m_threads.empty();
}

void SocketHandler::EnableRelease()
{
	if (m_release) {
		return;
	}

	m_release = new UdpSocket(*this);
	m_release -> SetDeleteByHandler();
	port_t port = 0;
	m_release -> Bind("127.0.0.1", port);
	Add(m_release);
}

void SocketHandler::Release()
{
	if (!m_release) {
		return;
	}

	m_release -> SendTo("127.0.0.1", m_release -> GetPort(), "\n");
}

IMutex& SocketHandler::GetMutex() const
{
	return m_mutex;
}

void SocketHandler::RegStdLog(StdLog* log)
{
	m_stdlog = log;
}

void SocketHandler::LogError(Socket* p, const std::string& user_text, int err, const std::string& sys_err, loglevel_t t) 
{
	if (m_stdlog) {
		m_stdlog -> error(this, p, user_text, err, sys_err, t);
	}
}

void SocketHandler::Add(Socket* s)
{
	m_add.push_back(s);
}

void SocketHandler::ISocketHandler_Add(Socket* s, bool bRead, bool bWrite)
{
	Set(s, bRead, bWrite);
}

void SocketHandler::ISocketHandler_Mod(Socket* s, bool bRead, bool bWrite)
{
	Set(s, bRead, bWrite);
}

void SocketHandler::ISocketHandler_Del(Socket* s)
{
	Set(s, false, false);
}

int SocketHandler::Select(long sec, long usec)
{
	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	return Select(&tv);
}

int SocketHandler::Select()
{
	if (m_b_check_callonconnect || m_b_check_detach || m_b_check_timeout || m_b_check_retry || m_b_check_close) {
		return Select(0, m_b_check_detach ? 10000 : 2000000);
	}
	return Select(nullptr);
}

int SocketHandler::Select(struct timeval* tsel)
{
	if (!m_add.empty()) {
		AddIncoming();
	}

	int n = ISocketHandler_Select(tsel);
	if (m_b_check_callonconnect) {
	CheckCallOnConnect();
	}

	#ifdef ENABLE_DETACH
	if (!m_slave && m_b_check_detach) {
		CheckDetach();
	}
	#endif

	if (m_b_check_timeout) {
		time_t tnow = time(nullptr);
		if (tnow != m_tlast) {
			CheckTimeout(tnow);
			m_tlast = tnow;
		}
	}

	if (m_b_check_retry) {
		CheckRetry();
	}

	if (m_b_check_close) {
		CheckClose();
	}

	if (!m_fds_erase.empty()) {
		CheckErasedSockets();
	}

	while (m_delete.size()) {
		std::list<Socket*>::iterator it = m_delete.begin();
		Socket* p = *it;
		p -> OnDelete();
		m_delete.erase(it);
		if (p -> DeleteByHandler()
		#ifdef ENABLE_DETACH
		&& !(m_slave ^ p -> IsDetached())
		#endif
		) {
			p -> SetErasedByHandler();
			delete p;
		}
	}
	return n;
}

bool SocketHandler::Valid(Socket* s) 
{
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (s == p) {
			return true;
		}
	}
	return false;
}

bool SocketHandler::Valid(socketuid_t uid)
{
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (p -> UniqueIdentifier() == uid) {
			return true;
		}
	}
	return false;
}

size_t SocketHandler::GetCount()
{
	return m_sockets.size() + m_add.size() + m_delete.size();
}

void SocketHandler::SetCallOnConnect(bool b)
{
	m_b_check_callonconnect = b;
}

void SocketHandler::SetDetach(bool b)
{
	m_b_check_detach = b;
}

void SocketHandler::SetTimeout(bool b)
{
	m_b_check_timeout = b;
}

void SocketHandler::SetRetry(bool b)
{
	m_b_check_retry = b;
}

void SocketHandler::SetClose(bool b)
{
	m_b_check_close = b;
}

bool SocketHandler::OkToAccept(Socket*)
{
	return true;
}

#ifdef ENABLE_POOL
ISocketHandler::PoolSocket* SocketHandler::FindConnection(int type, const std::string& protocol, SocketAddress& ad)
{
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end() && m_sockets.size(); ++it) {
		PoolSocket* pools = dynamic_cast<PoolSocket*>(it -> second);
		if (pools) {
			if (pools -> GetSocketType() == type &&
				pools -> GetSocketProtocol() == protocol &&
				*pools -> GetClientRemoteAddress() == ad) {
					m_sockets.erase(it);
					pools -> SetRetain();
					return pools;
				}
		}
	}
	return nullptr;
}

void SocketHandler::EnablePool(bool b)
{
	m_b_enable_pool = b;
}

bool SocketHandler::PoolEnabled()
{
	return m_b_enable_pool;
}

#endif

#ifdef ENABLE_DETACH

void SocketHandler::SetSlave(bool b) 
{
	m_slave = b;
}

bool SocketHandler::IsSlave()
{
	return m_slave;
}

#endif

#ifdef ENABLE_EPOLL

void SocketHandler::ISocketHandler_Epoll(int epollFd)
{
	// 这个函数会阻塞 直到超时或者有响应事件发生
	int eNum = epoll_wait(epollFd, m_events, m_max_events, m_timeout);

	if (eNum == -1) {
		LogError(nullptr, "epoll_wait failed", Errno, StrError(Errno), LOG_LEVEL_ERROR);
		return;
	}

	// 遍历所有的事件
	// TODO: 可写事件 EPOLLOUT & 失败重写
	for (int i = 0; i < eNum; ++i) {
		for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
			// 判断这次是不是socket可读(是不是有新的连接)
			if (m_events[i].data.fd == it -> first) {
				if (this->m_events[i].events & EPOLLIN) {
					sockaddr_in cli_addr{};
					memset(&cli_addr, 0, sizeof(cli_addr));
					socklen_t length = sizeof(cli_addr);
					int fd = accept(it -> first, (sockaddr*)&cli_addr, &length);
					if (fd > 0) {
						// 判断是否为非阻塞模式
						if (this->m_b_nonblock) {
							bool isSuccess = it->second->SetNonblocking(true);
							if (!isSuccess) continue;
						}	
					}
					// 将新的连接添加到epoll中
          // epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &m_epev);
					AddEpoll(epollFd, fd);
				}
			}
			// 不是socket的响应事件  但是依旧要处理 因为连接断开和出错时 也会响应 ‘EPOLLIN’ 事件
			else {
				if (m_events[i].events & EPOLLERR || m_events[i].events & EPOLLHUP) {
					// 出错时,从epoll中删除对应的连接 第一个是要操作的epoll的描述符 因为是删除,所有event参数天null就可以了
					// epoll_ctl(epollFd, EPOLL_CTL_DEL, m_events[i].data.fd, nullptr);
					RemoveEpoll(epollFd, m_events[i].data.fd);
          // close(m_events[i].data.fd);
					EpollClose(&m_events[i]);
				}
				// 如果是可读事件
				else if (m_events[i].events & EPOLLIN) {
					// TODO: 读写数据
				}
			}
		}
	}
}

void SocketHandler::AddEpoll(int EpollFd, int listenFd)
{
	epoll_ctl(EpollFd, EPOLL_CTL_ADD, listenFd, &m_epev);
}

void SocketHandler::RemoveEpoll(int EpollFd, int listenFd) 
{
	epoll_ctl(EpollFd, EPOLL_CTL_DEL, listenFd, nullptr);
}

void SocketHandler::EpollClose(epoll_event* event) 
{
	close(event->data.fd);
}

int SocketHandler::CreateEpoll()
{
	int eFd;

	if (m_b_use_mutex) {
		m_mutex.Unlock();
		// 创建一个epoll size已经不起作用了 一般填1就好了
		eFd = epoll_create(1);
		m_mutex.Lock();
	}	
	else {
		// 创建一个epoll size已经不起作用了 一般填1就好了
		eFd = epoll_create(1);
	}

	// 拿到文件描述符
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		SOCKET i = it -> first;
		// 可以响应的事件，这里只响应可读事件即可
		m_epev.events = EPOLLIN;
		m_epev.data.fd = i;  // socket的 文件描述符

		// 添加到epoll中
		// epoll_ctl(eFd, EPOLL_CTL_ADD, m_epev.data.fd, &m_epev);
		AddEpoll(eFd, m_epev.data.fd);
	}

	return eFd;
}


#endif

int SocketHandler::ISocketHandler_Select(struct timeval* tv)
{
	fd_set rfds = m_rfds;
	fd_set wfds = m_wfds;
	fd_set efds = m_efds;
	
	int n;

	if (m_b_use_mutex) {
		m_mutex.Unlock();
		n = select((int)(m_maxsock)+1, &rfds, &wfds, &efds, tv);
		m_mutex.Lock();
	}
	else {
		n = select((int)(m_maxsock)+1, &rfds, &wfds, &efds, tv);
	}
	if (n == -1) {
		int err = Errno;
		switch (err) {
		case EBADF:
			RebuildFdset();
			break;
		case EINTR:
			break;
		case EINVAL:
			LogError(nullptr, "SocketHandler::Select", err, StrError(err), LOG_LEVEL_FATAL);
			throw Exception("select(n): n is negative. Or struct timeval contains bad time values (<0)");
		case ENOMEM:
			LogError(nullptr, "SocketHandler::Select", err, StrError(err), LOG_LEVEL_ERROR);
			break;
		}
		printf("error on select(): %d %s\n", Errno, StrError(Errno));
	}
	else if (!n) {
		// TODO: timeout
	} 
	else if (n > 0) {
		for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
			SOCKET i = it -> first;
			Socket* p = it -> second;

			if (FD_ISSET(i, &rfds)) {
				p -> OnRead();
			}

			if (FD_ISSET(i, &wfds)) {
				p -> OnWrite();
			}

			if (FD_ISSET(i, &efds)) {
				p -> OnException();
			}
		}
	}
	return n;
}

void SocketHandler::Remove(Socket* p)
{
   if (p -> ErasedByHandler())
   {
     return;
   }

   for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it)
   {
     if (it -> second == p)
     {
       LogError(p, "Remove", -1, "Socket destructor called while still in use", LOG_LEVEL_WARNING);
       m_sockets.erase(it);
       return;
     }
   }

   for (std::list<Socket*>::iterator it2 = m_add.begin(); it2 != m_add.end(); ++it2)
   {
     if (*it2 == p)
     {
       LogError(p, "Remove", -2, "Socket destructor called while still in use", LOG_LEVEL_WARNING);
       m_add.erase(it2);
       return;
     }
   }

	 for (std::list<Socket *>::iterator it3 = m_delete.begin(); it3 != m_delete.end(); ++it3)
   {
     if (*it3 == p)
     {
       LogError(p, "Remove", -3, "Socket destructor called while still in use", LOG_LEVEL_WARNING);
       m_delete.erase(it3);
       return;
     }
   }
}

void SocketHandler::DeleteSocket(Socket* s)
{
	s -> OnDelete();
	if (s -> DeleteByHandler() && !s -> ErasedByHandler()) {
		s -> SetErasedByHandler();
	}

	m_fds_erase.push_back(s -> UniqueIdentifier());
}

void SocketHandler::AddIncoming()
{
	while (m_add.size() > 0) {
		if (m_sockets.size() >= MaxCount()) {
			LogError(nullptr, "Select", (int)m_sockets.size(), "socket limit reached", LOG_LEVEL_WARNING);
			break;
		}

		std::list<Socket*>::iterator it = m_add.begin();

		Socket* p = *it;
		SOCKET s = p -> GetSocket();

		if (s == INVALID_SOCKET) {
			LogError(p, "Add", -1, "Invalid socket", LOG_LEVEL_WARNING);
			m_delete.push_back(p);
			m_add.erase(it);
			continue;
		}

		socket_m::iterator it2;
		if ((it2 = m_sockets.find(s)) != m_sockets.end()) {
			Socket* found = it2 -> second;
			if (p -> UniqueIdentifier() > found -> UniqueIdentifier()) {
				LogError(p, "Add", (int)p -> GetSocket(), "正在替换已在受控队列中的套接字(更新的uid)", LOG_LEVEL_WARNING);
				DeleteSocket(found);
			} 
			else if (p -> UniqueIdentifier() == found -> UniqueIdentifier()) {
				LogError(p, "Add", (int)p -> GetSocket(), "尝试在受控队列中添加套接字(相同uid)", LOG_LEVEL_ERROR);
				if (p != found) {
					m_delete.push_back(p);
				}
				m_add.erase(it);
				continue;
			}
			else {
				LogError(p, "Add", (int)p -> GetSocket(), "尝试在受控队列中添加套接字(相同uid)", LOG_LEVEL_ERROR);
				// 这是一个dup，不要添加到删除队列，忽略它
				// m_delete.push_back(p)
				m_add.erase(it);
				continue;
			}
		}

		if (p -> CloseAndDelete()) {
			LogError(p, "Add", (int)p -> GetSocket(), "Added socket with SetCloseAndDelete() true", LOG_LEVEL_WARNING);
			m_sockets[s] = p;
			DeleteSocket(p);
			p -> Close();
		}
		else {
			m_b_check_callonconnect |= p -> CallOnConnect();
      m_b_check_detach |= p -> IsDetach();
      m_b_check_timeout |= p -> CheckTimeout();
			m_b_check_retry |= p -> RetryClientConnect();
      StreamSocket *scp = dynamic_cast<StreamSocket *>(p);
      if (scp && scp -> Connecting()) // 'Open' called before adding socket
      {
        ISocketHandler_Add(p,false,true);
			}
      else {
        TcpSocket *tcp = dynamic_cast<TcpSocket *>(p);
				bool bWrite = tcp ? tcp -> GetOutputLength() != 0 : false;
        if (p -> IsDisableRead()) {
          ISocketHandler_Add(p, false, bWrite);
        }
        else {
          ISocketHandler_Add(p, true, bWrite);
        }
      }
      m_maxsock = (s > m_maxsock) ? s : m_maxsock;
      m_sockets[s] = p;
		}
		m_add.erase(it);
	}
}

void SocketHandler::CheckCallOnConnect()
{
	m_b_check_callonconnect = false;

	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (Valid(p) && Valid(p -> UniqueIdentifier()) && p -> CallOnConnect()) { 
			p -> SetConnected();
			TcpSocket* tcp = dynamic_cast<TcpSocket*>(p);
			if (tcp) {
				if (tcp -> GetOutputLength()) {
					p -> OnWrite();
				}
			}
// #ifdef ENABLE_RECONNECT
//       if (tcp && tcp -> IsRe)
// #endif
			p -> OnConnect();
			p -> SetCallOnConnect(false);
			m_b_check_callonconnect = true;
		}
	}
}

void SocketHandler::CheckErasedSockets()
{
	// 检查已删除的socket
	bool check_max_fd = false;
	while (m_fds_erase.size()) {
		std::list<socketuid_t>::iterator it = m_fds_erase.begin();
		socketuid_t uid = *it;
		for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
			Socket* p = it -> second;
			if (p -> UniqueIdentifier() == uid) {
				if (p -> ErasedByHandler()) {
					delete p;
				}
				m_sockets.erase(it);
				break;
			}
		}
		m_fds_erase.erase(it);
		check_max_fd = true;
	}
	if (check_max_fd) {
		m_maxsock = 0;
		for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
			SOCKET s = it -> first;
			m_maxsock = s > m_maxsock ? s : m_maxsock;
		}
	}
}

void SocketHandler::CheckDetach()
{
	m_b_check_detach = false;

	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (p -> IsDetach()) {
			ISocketHandler_Del(p);
			m_sockets.erase(it);
			p -> DetachSocket();
			m_b_check_detach = true;
			break;
		}
	}

	for (std::list<Socket*>::iterator it = m_add.begin(); it != m_add.end(); ++it) {
		Socket *p = *it;
		m_b_check_detach |= p -> IsDetach();
	}

}

void SocketHandler::CheckTimeout(time_t time)
{
	m_b_check_timeout = false;
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (Valid(p) && Valid(p -> UniqueIdentifier()) && p -> CheckTimeout()) {
			if (p -> Timeout(time)) {
				StreamSocket* scp = dynamic_cast<StreamSocket*>(p);
				p -> SetTimeout(0);
				if (scp && scp -> Connecting()) {
					p -> OnConnectTimeout();
					p -> SetTimeout(scp -> GetConnectTimeout());
				}
				else {
					p -> OnTimeout();
				}
			}
			m_b_check_timeout = true;
		}
	}
}

void SocketHandler::CheckRetry()
{
	m_b_check_retry = false;
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (Valid(p) && Valid(p -> UniqueIdentifier()) && p -> RetryClientConnect()) {
			TcpSocket* tcp = dynamic_cast<TcpSocket*>(p);
			tcp -> SetRetryClientConnect(false);

			p -> Close();
			std::unique_ptr<SocketAddress> ad = p -> GetClientRemoteAddress();
			if (*ad.get()) {
				tcp -> Open(*ad);
			}
			else {
				LogError(p, "RetryClientConnect", 0, "no address", LOG_LEVEL_ERROR);
			}
			Add(p);
			m_fds_erase.push_back(p -> UniqueIdentifier());
			m_b_check_retry = true;
		}
	}
}

void SocketHandler::CheckClose()
{
	m_b_check_close = false;
	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		Socket* p = it -> second;
		if (Valid(p) && Valid(p -> UniqueIdentifier()) && p -> CloseAndDelete()) {
			TcpSocket* tcp = dynamic_cast<TcpSocket*>(p);
			if (p -> Lost()) {
				DeleteSocket(p);
			}
			else if (tcp && p -> IsConnected() && tcp -> GetFlushBeforeClose() && p -> TimeSinceClose() < 5) {
				if (tcp -> GetOutputLength()) {
					LogError(p, "Closing", (int)tcp -> GetOutputLength(), "Sending all data before closing", LOG_LEVEL_INFO);
				}
				else if (!(tcp -> GetShutdown() & SHUT_WR)) {
					SOCKET nn = it -> first;
					if (nn != INVALID_SOCKET && shutdown(nn, SHUT_WR) == -1) {
						LogError(p, "graceful shuadowm", Errno, StrError(Errno), LOG_LEVEL_ERROR);
					}
					tcp -> SetShutdown(SHUT_WR);
				}
				else {
					ISocketHandler_Del(p);
					tcp -> Close();
					DeleteSocket(p);
				}
			}
			else {
				if (tcp && p -> IsConnected() && tcp -> GetOutputLength()) {
					LogError(p, "Closing", (int)tcp -> GetOutputLength(), "Closing socket while data still left to send", LOG_LEVEL_WARNING);
				}
#ifdef ENABLE_POOL
				if (tcp && p -> IsConnected() && tcp -> GetOutputLength()) {
					PoolSocket* p2 = new PoolSocket(*this, p);
					p2 -> SetDeleteByHandler();
					Add(p2);
					p -> SetCloseAndDelete(false);
				}
				else 
#endif
				{
					ISocketHandler_Del(p);
					p -> Close();
				}
				DeleteSocket(p);
			}
			m_b_check_close = true;
		}
	}
}

void SocketHandler::RebuildFdset()
{
	fd_set rfds;
	fd_set wfds;
	fd_set efds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		SOCKET s = it -> first;
		Socket* p = it -> second;
		if (s == p -> GetSocket() && s >= 0) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(s, &fds);

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			int n = select((int)s + 1, &fds, nullptr, nullptr, &tv);
			if (n == -1 && Errno == EBADF) {
				LogError(p, "Select", (int)s, "Bad fd in fd_set (2)", LOG_LEVEL_ERROR);
				if (Valid(p) && Valid(p -> UniqueIdentifier())) {
					DeleteSocket(p);
				}
			}
			else {
				if (FD_ISSET(s, &m_rfds)) {
					FD_SET(s, &rfds);
				}
				if (FD_ISSET(s, &m_wfds)) {
					FD_SET(s, &wfds);
				}
				if (FD_ISSET(s, &m_efds)) {
					FD_SET(s, &efds);
				}
			}
		}
		else {
			LogError(p, "Select", (int)s, "Bad fd in fd_set (3)", LOG_LEVEL_ERROR);
			DeleteSocket(p);
		}
	}
	m_rfds = rfds;
	m_wfds = wfds;
	m_efds = efds;
}

void SocketHandler::Set(Socket* sck, bool bRead, bool bWrite)
{
	SOCKET s = sck -> GetSocket();
	if (s >= 0) {
		bool bException = true;

		if (bRead) {
			if (!FD_ISSET(s, &m_rfds)) {
				FD_SET(s, &m_rfds);
			}
		} else {
			FD_CLR(s, &m_rfds);
		}

		if (bWrite) {
			if (!FD_ISSET(s, &m_wfds)) {
				FD_SET(s, &m_wfds);
			}
		} else {
			FD_CLR(s, &m_wfds);
		}

		if (bException) {
			if (!FD_ISSET(s, &m_efds)) {
				FD_SET(s, &m_efds);
			}
		} else {
			FD_CLR(s, &m_efds);
		}
	}
}

