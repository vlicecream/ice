/*
 * Auther:
 * Date: 2022-09-08
 * Email: vlicecrea@163.com
 * */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "SocketHandler.h"
#include "TcpSocket.h"
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
		m_b_enable_poll(false),
#endif
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		m_resolver(nullptr),
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
		m_b_enable_poll(false),
#endif
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		m_resolver(nullptr),
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
		m_b_enable_poll(false),
#endif
#ifdef ENABLE_RESOLVER
		m_resolv_id(0),
		m_resolver(nullptr),
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
	if (m_resolver) {
		m_resolver -> Quit();
	}
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
		if (m_resolver) {
			delete m_resolver;
		}
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
