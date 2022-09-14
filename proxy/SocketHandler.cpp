/*
 * Auther:
 * Date: 2022-09-08
 * Email: vlicecrea@163.com
 * */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "SocketHandler.h"
#include "Utility.h"
#include "IMutex.h"
#include "SocketAddress.h"
#include "Lock.h"

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
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		// m_resolver(nullptr),
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
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		// m_resolver(nullptr),
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
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		// m_resolver(nullptr),
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
#ifdef ENABLE_RESOLVER
	// if (m_resolver) {
	// 	// todo: m_resolver -> Quit();
	// }
#endif
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
#ifdef ENABLE_RESOLVER
		// if (m_resolver) {
		// 	delete m_resolver;
		// }
#endif
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
		// todo: throw Exception("No parent sockethandler available");
	}
	return m_parent;
}

ISocketHandler& SocketHandler::GetRandomHandler()
{
	if (m_threads.empty()) {
		// todo: 
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
	// todo: throw Exception("Can't locate free threaded sockethandler")
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
		CheckClose;
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

#ifdef ENABLE_RESOLVER

void SocketHandler::EnabelResolver(port_t port)
{
	// if (!m_resolver) {
	// 	m_resolver_port = port;
	// 	m_resolver = new ResolveServer(port);
	// }
}

bool SocketHandler::ResolverEnabled()
{
	// return m_resolver ? true : false;
	return false;
}

int SocketHandler::Resolve(Socket* s, const std::string& host, port_t port)
{
	// todo:check cache
	// ResolveSocket *resolv = new ResolvSocket(*this, p, host, port);
	// resolv -> SetId(++m_resolv_id)
	return 0;
}

int SocketHandler::Resolve(Socket* s, ipaddr_t ipaddr)
{
	// todo
	return 0;
}

port_t SocketHandler::GetResolverPort()
{
	return m_resolver_port;
}

bool SocketHandler::ResolverReady()
{
	// return m_resolver ? true : false;
	return false;
}

bool SocketHandler::Resolving(Socket* s)
{
	std::map<socketuid_t, bool>::iterator it = m_resolve_q.find(s -> UniqueIdentifier());
	return it != m_resolve_q.end();
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

int SocketHandler::ISocketHandler_Select(struct timeval* tv)
{
	//todo
}

void SocketHandler::Remove(Socket* s)
{
	//todo
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
	// todo
}

void SocketHandler::CheckCallOnConnect()
{
	// todo
}

void SocketHandler::CheckErasedSockets()
{
	// todo
}

void SocketHandler::CheckDetach()
{
	// todo
}

void SocketHandler::CheckTimeout(time_t time)
{
	// todo
}

void SocketHandler::CheckRetry()
{
	// todo
}

void SocketHandler::CheckClose()
{
	// todo
}

void SocketHandler::RebuildFdset()
{
  // todo
}

void SocketHandler::Set(Socket* s, bool bRead, bool bWrite)
{

}

