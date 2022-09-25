/*
 * Auther: ting
 * Date: 2022-09-02
 * Email: vlicecream@163.com
 * */

#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include "Socket.h"
#include "Utility.h"
#include "SocketAddress.h"
#include "SocketHandler.h"
#include "Ipv4Address.h"
#include "SocketThread.h"
#include "SocketInclude.h"

#ifdef _DEBUG
#define DEB(x) x; fflush(stderr);
#else
#define DEB(x)
#endif

socketuid_t Socket::m_next_uid = 0;

Socket::Socket(ISocketHandler& h)
	:
		m_handler(h),
		m_socket(INVALID_SOCKET),
		m_bDel(false),
		m_bClose(false),
		m_tCreate(time(nullptr)),
		m_parent(nullptr),
		m_b_erased_by_handler(false),
		m_tClose(0),
		m_client_remote_address(nullptr),
		m_remote_address(nullptr),
		m_traffic_monitor(nullptr),
		m_timeout_start(0),
		m_timeout_limit(0),
		m_bLost(false),
		m_uid(++Socket::m_next_uid),
		m_call_on_connect(false),
		m_b_retry_connect(false),
#ifdef ENABLE_POOL
		m_socket_type(0),
		m_bClient(false),
		m_bRetain(false),
#endif
#ifdef ENABLE_DETACH
		m_detach(false),
		m_detached(false),
		m_pThread(NULL),
		m_slave_handler(NULL)
#endif
{
}

Socket::~Socket()
{
	Handler().Remove(this);
	if (m_socket != INVALID_SOCKET
#ifdef ENABLE_POOL
			&& !m_bRetain
#endif
			)
	{
		Close();
	}
}

ISocketHandler& Socket::Handler() const
{
#ifdef ENABLE_DETACH
	if (IsDetached()) {
		return *m_slave_handler;
	}
#endif
	return m_handler;
}

ISocketHandler& Socket::MasterHandler() const
{
	return m_handler;
}

void Socket::Init()
{
}

SOCKET Socket::CreateSocket(int af, int type, const std::string& protocol)
{
	struct protoent* p = nullptr;

	SOCKET s;

#ifdef ENABLE_POOL
	m_socket_type = type;
	m_socket_protocol = protocol;
#endif
	if (protocol.size()) {
		p = getprotobyname(protocol.c_str());
		if (!p) {
			Handler().LogError(this, "getprotobyname", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			SetCloseAndDelete(true);
			return INVALID_SOCKET;
		}
	}

	int protno = p ? p->p_proto : 0;

	s = socket(af, type, protno);
	if (s == INVALID_SOCKET) {
		Handler().LogError(this, "socket", Errno, StrError(Errno), LOG_LEVEL_FATAL);
		SetCloseAndDelete(true);
		return INVALID_SOCKET;
	}

	Attach(s);
	OnOptions(af, type, protno, s);
	// Attach(INVALID_SOCKET);
	return s;
}

// SOCKET Socket::CreateSocket_Epoll(int af, int type, const std::string &protocol)
// {
//   struct protoent* p = nullptr;
//   SOCKET fd;
// #ifdef ENABLE_POOL
//   m_socket_type = type;
//   m_socket_protocol = protocol;
// #endif
//   if (protocol.size()) {
//     p = getprotobyname(const char *__name)
//   }
// }

void Socket::Attach(SOCKET s) 
{
	m_socket = s;
}


SOCKET Socket::GetSocket()
{
	return m_socket;
}

int Socket::Close()
{
	if (m_socket == INVALID_SOCKET) {
		Handler().LogError(this, "Socket::Close", 0, "file descriptor invaild", LOG_LEVEL_WARNING);
		return 0;
	}

	int n;
	Handler().ISocketHandler_Del(this);

  if ((n = closesocket(m_socket)) == -1) {
		Handler().LogError(this, "close", Errno,StrError(Errno), LOG_LEVEL_ERROR);
	}
	m_socket = INVALID_SOCKET;

	return n;
}

bool Socket::Ready()
{
	if (m_socket != INVALID_SOCKET && !CloseAndDelete()) {
		return true;
	}
	return false;
}

Socket* Socket::GetParent()
{
	return m_parent;
}

void Socket::SetParent(Socket* s)
{
	m_parent = s;
}

port_t Socket::GetPort()
{
	return 0;
}

bool Socket::SetNonblocking(bool b)
{
#ifdef ENABLE_SELECT
	if (b) {
		if (fcntl(m_socket, F_SETFL, O_NONBLOCK) == -1) {
			Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	} else {
		if (fcntl(m_socket, F_SETFL, 0) == -1) {
			Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
#endif

#ifdef ENABLE_EPOLL
	if (b) {
		if (fcntl(m_socket, F_GETFD, 0) < 0) {
			// 设置连接为非阻塞模式
			int flags = fcntl(m_socket, F_GETFD);
			if (flags < 0) {
				Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
				return false;
			}
			if (fcntl(m_socket, flags | O_NONBLOCK) < 0) {
				Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			}
		}
	}
#endif
	return true;
}

bool Socket::SetNonblocking(bool b, SOCKET s)
{
#ifdef ENABLE_SELECT
	if (b) {
		if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
			Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	} else {
		if (fcntl(s, F_SETFL, 0) == -1) {
			Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
#endif

#ifdef ENABLE_EPOLL
	if (b) {
		if (fcntl(s, F_GETFD, 0) < 0) {
			// 设置连接为非阻塞模式
			int flags = fcntl(m_socket, F_GETFD);
			if (flags < 0) {
				Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
				return false;
			}
			if (fcntl(s, flags | O_NONBLOCK) < 0) {
				Handler().LogError(this, "SetNonblocking", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			}
		}
	}
#endif
	return true;
}

time_t Socket::Uptime()
{
		return time(nullptr) - m_tCreate;
}

void Socket::SetClientRemoteAddress(SocketAddress& sa)
{
	if (!sa.IsValid()) {
		Handler().LogError(this, "SetClientRemoteAddress", Errno, StrError(Errno), LOG_LEVEL_ERROR);
	}
	m_client_remote_address = sa.GetCopy();
}

std::unique_ptr<SocketAddress> Socket::GetClientRemoteAddress()
{
	if (!m_client_remote_address.get()) {
		Handler().LogError(this, "GetClientRemoteAddress", 0, "remote address not yet set", LOG_LEVEL_ERROR);
	}
	return m_client_remote_address->GetCopy();
}

void Socket::SendBuf(const char*, size_t, int)
{
}

void Socket::Send(const std::string&, int)
{
}

uint64_t Socket::GetBytesSent(bool)
{
	return 0;
}

uint64_t Socket::GetBytesReceived(bool)
{
}

void Socket::SetTimeout(time_t secs)
{
	if (!secs) {
		m_timeout_limit = 0;
		m_timeout_start = 0;
		return;
	}
	m_timeout_start = time(nullptr);
	m_timeout_limit = secs;

	Handler().SetTimeout();
}

bool Socket::CheckTimeout()
{
	return m_timeout_start > 0 && m_timeout_limit > 0;
}

bool Socket::Timeout(time_t tnow)
{
	if (m_timeout_start > 0 && tnow - m_timeout_start > m_timeout_limit) {
		return true;
	}
	return false;
}

void Socket::SetRemoteAddress(SocketAddress& sa)
{
	m_remote_address = sa.GetCopy();
}

void Socket::OnRead()
{
}

void Socket::OnWrite()
{
}

void Socket::OnException()
{
	int err = SoError();
	Handler().LogError(this, "exception on select", err, StrError(err), LOG_LEVEL_FATAL);
	SetCloseAndDelete();
}

void Socket::OnDelete()
{
}

void Socket::OnConnect()
{
}

void Socket::OnAccept()
{
}

void Socket::OnLine(const std::string&)
{
}

void Socket::OnConnectFailed()
{
}


bool Socket::OnConnectRetry()
{
	return true;
}

void Socket::OnDisconnect()
{
}

void Socket::OnDisconnect(short info, int code)
{
}

void Socket::OnTimeout()
{
}

void Socket::OnConnectTimeout()
{
}

void Socket::SetDeleteByHandler(bool b)
{
	m_bDel = b;
}

bool Socket::DeleteByHandler()
{
	return m_bDel;
}

void Socket::SetCloseAndDelete(bool b) 
{
	if (b != m_bClose) {
		m_bClose = b;
		if (b) {
			m_tClose = time(nullptr);
			Handler().SetClose();
		}
	}	
}

bool Socket::CloseAndDelete()
{
	return m_bClose;
}

time_t Socket::TimeSinceClose()
{
	return time(nullptr) - m_tClose;
}

void Socket::DisableRead(bool b)
{
	m_b_disable_read = b;
}

bool Socket::IsDisableRead()
{
	return m_b_disable_read;
}

void Socket::SetConnected(bool b){
	m_connected = b;
}

bool Socket::IsConnected()
{
	return m_connected;
}

void Socket::SetLost()
{
	m_bLost = true;
}

bool Socket::Lost()
{
	return m_bLost;
}

void Socket::SetErasedByHandler(bool b)
{
	m_b_erased_by_handler = b;
}

bool Socket::ErasedByHandler()
{
	return m_b_erased_by_handler;
}

void Socket::SetCallOnConnect(bool b)
{
	m_call_on_connect = b;
	if (b) {
		Handler().SetCallOnConnect();
	}
}

bool Socket::CallOnConnect()
{
	return m_call_on_connect;
}

void Socket::SetRetryClientConnect(bool b) 
{
	m_b_retry_connect = b;
	if (b) {
		Handler().SetRetry();	
	}
}

bool Socket::RetryClientConnect()
{
	return m_b_retry_connect;
}

std::unique_ptr<SocketAddress> Socket::GetRemoteSocketAddress()
{
	return m_remote_address -> GetCopy();
}

ipaddr_t Socket::GetRemoteIP4()
{
	ipaddr_t l = 0;
	if (m_remote_address.get() != nullptr) {
		struct sockaddr* p = *m_remote_address;
		struct sockaddr_in* sa = (struct sockaddr_in*) p;
		memcpy(&l, &sa -> sin_addr, sizeof(struct in_addr));
	}
	return l;
}

port_t Socket::GetRemotePort()
{
	if (!m_remote_address.get()) {
		return 0;
	}

	return m_remote_address -> GetPort();
}

std::string Socket::GetRemoteAddress()
{
	if (!m_remote_address.get()) {
		return "";
	}
	return m_remote_address -> Convert(false);
}

std::string Socket::GetRemoteHostname()
{
	if (!m_remote_address.get()){
		return "";
	}

	return m_remote_address -> Reverse();
}

port_t Socket::GetSockPort()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr*)&sa, (socklen_t*)&sockaddr_length) == -1) {
		memset(&sa, 0, sizeof(sa));
	}
	return ntohs(sa.sin_port);
}

ipaddr_t Socket::GetSockIP4()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr*)&sa, (socklen_t*)&sockaddr_length) == -1) {
		memset(&sa, 0, sizeof(sa));
	}
	ipaddr_t a;
	memcpy(&a, &sa.sin_addr, 4);
	return a;
}

std::string Socket::GetSockAddress()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr*)&sa, (socklen_t*)&sockaddr_length) == -1) {
		memset(&sa, 0, sizeof(sa));
	}
	Ipv4Address addr(sa);
	return addr.Convert();
}

int Socket::SoError()
{
	int value = 0;
	Handler().LogError(this, "socket option not available", 0, "SO_ERROR", LOG_LEVEL_ERROR);
	return value;
}

#ifdef ENABLE_POOL

void Socket::SetIsClient()
{
	m_bClient = true;
}


void Socket::SetSocketType(int x)
{
	m_socket_type = x;
}

int Socket::GetSocketType()
{
	return m_socket_type;
}

void Socket::SetSocketProtocol(const std::string& x)
{
	m_socket_protocol = x;
}

const std::string& Socket::GetSocketProtocol()
{
	return m_socket_protocol;
}

void Socket::SetRetain()
{
	if (m_bClient) {
		m_bRetain = true;
	}
}

bool Socket::Retain()
{
	return m_bRetain;
}

void Socket::CopyConnection(Socket* sock)
{
	Attach(sock -> GetSocket());
	SetSocketType(sock -> GetSocketType());
	SetSocketProtocol(sock -> GetSocketProtocol());

	SetClientRemoteAddress(*sock -> GetClientRemoteAddress());
	SetRemoteAddress(*sock -> GetRemoteSocketAddress());
}

#endif

#ifdef ENABLE_RESOLVER

int Socket::Resolve(const std::string& host, port_t port)
{
	return Handler().Resolve(this, host, port);
}

int Socket::Resolve(ipaddr_t a)
{
	return Handler().Resolve(this, a);
}

void Socket::OnResolved(int id, ipaddr_t ipaddr, port_t port)
{
}

void Socket::OnResolveFailed(int id)
{
}

void Socket::OnReverseResolved(int id, const std::string &name)
{
}

#endif

#ifdef ENABLE_DETACH

void Socket::OnDetached()
{
}

void Socket::SetDetach(bool b)
{
	m_detach = b;
	if (b) {
		Handler().SetDetach();
	}
}

bool Socket::IsDetached() const
{
	return m_detach;
}

bool Socket::Detach()
{
	if (!DeleteByHandler()) {
		return false;
	}
	if (m_pThread) {
		return false;
	}
	if (m_detached) {
		return false;
	}
	SetDetach();
	return true;
}

void Socket::SetDetached(bool b)
{
	m_detached = b;
}

void Socket::SetSlaveHandler(ISocketHandler* p)
{
 m_slave_handler = p;
}

void Socket::DetachSocket()
{
	SetDetached();
	m_pThread = new SocketThread(this);
	// m_pThread -> SetRelease(true);
}

#endif


