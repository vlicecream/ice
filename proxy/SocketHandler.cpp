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
		m_b_enable_poll(false)
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
		m_b_enable_poll(false)
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
		m_b_enable_poll(false)
#endif
{
	m_mutex.Lock();
	FD_ZERO(&m_rfds);
	FD_ZERO(&m_wfds);
	FD_ZERO(&m_efds);
}

SocketHandler::~SocketHandler()
{

}
