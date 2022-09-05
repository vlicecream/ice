/*
 * Auther: ting
 * Date: 2022-09-02
 * Email: vlicecream@163.com
 * Detail: socket.cpp
 * */

#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

#include "Socket.h"
#include "ISocketHandler.h"
#include "SocketInclude.h"
#include "StdLog.h"
#include "SocketAddress.h"

Socket::Socket(ISocketHandler& h)
	:
		m_handler(h),
		m_bDel(false),
		m_bClose(false),
		m_tCreate(time(NULL)),
		m_parent(NULL),
		m_b_disable_read(false),
		m_connected(false),
		m_b_erased_by_handler(false),
		m_tClose(0),
		m_traffic_monitor(NULL),
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
		m_socket() 
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

void Socket::Init()
{
}

void Socket::OnRead()
{
}

void Socket::OnWrite()
{
}

// 异常处理
void Socket::OnException()
{
	int err = SoError();
	Handler().LogError(this, "exception on select", err , StrError(err));
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

int Socket::Close()
{
	if (m_socket == INVALID_SOCKET) {
		Handler().LogError(this, "Socket::Close", 0, "file descriptor invalid");	
		return 0;
	}
	int n;
	Handler().ISocketHandler_Del(this);
	if ((n = closesocket(m_socket)) == -1) {
		Handler().LogError(this, "close", Errno, StrError(Errno));
	}
	m_socket = INVALID_SOCKET;
	return n;
}

SOCKET Socket::CreateSocket(int af, int type, const std::string &protocol)
{
	struct protoent* p = NULL;
	SOCKET s;
#ifdef ENABLE_POOL
	m_socket_type = type;
	m_socket_protocol = protocol;
#endif
	if (protocol.size()) {
		p = getprotobyname(protocol.c_str());
		if (!p) {
			Handler().LogError(this, "getprotobuname", Errno, StrError(Errno));
			SetCloseAndDelete();
			return INVALID_SOCKET;
		}
	}
	int protno = p ? p -> p_proto : 0;
	s = socket(af, type, protno);
	if (s == INVALID_SOCKET)
	{
		Handler().LogError(this, "socket", Errno, StrError(Errno));
		SetCloseAndDelete();
		return INVALID_SOCKET;
	}
	Attach(s);
	OnOptions(af, type, protno, s);
	Attach(INVALID_SOCKET);
	return s;
}

int Socket::SoError()
{
	int value = 0;

	return value;
}

void Socket::Attach(SOCKET s)
{
	m_socket = s;
}

SOCKET Socket::GetSocket()
{
	return m_socket;
}


void Socket::SetDeleteByHandler(bool x)
{
	m_bDel = x;
}

bool Socket::DeleteByHandler()
{
	return m_bDel;
}

void Socket::SetCloseAndDelete(bool x)
{
	if (x != m_bClose) {
		m_bClose = x;
		if (x) {
			m_tClose = time(NULL);
			Handler().SetClose();
		}
	}
}

void Socket::SetRemoteAddress(SocketAddress& ad)
{
	m_remote_address = ad.GetCopy();
}

std::unique_ptr<SocketAddress> Socket::GetRemoteSocketAddress()
{
	return m_remote_address -> GetCopy();
}

ISocketHandler& Socket::Handler() const
{
	return m_handler;
}

ISocketHandler& Socket::MasterHandler() const
{
	return m_handler;
}

ipaddr_t Socket::GetRemoteIP4()
{
	ipaddr_t l = 0;
	if (m_remote_address.get() != NULL) {
		struct sockaddr *p = *m_remote_address;
		struct sockaddr_in *sa = (struct sockaddr_in*)p;
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
	if (!m_remote_address.get())
	{
		return "";
	}
	return m_remote_address -> Reverse();
}

bool Socket::SetNonblocking(bool bNb)
{
	if (bNb)
	{
		if (fcntl(m_socket, F_SETFL, O_NONBLOCK) == -1)
		{
			Handler().LogError(this, "fcntl(F_SETFL, O_NONBLOCK)", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
	else
	{
		if (fcntl(m_socket, F_SETFL, 0) == -1)
		{
			Handler().LogError(this, "fcntl(F_SETFL, 0)", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
	return true;
}

bool Socket::SetNonblocking(bool bNb, SOCKET s)
{
	if (bNb)
	{
		if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		{
			Handler().LogError(this, "fcntl(F_SETFL, O_NONBLOCK)", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
	else
	{
		if (fcntl(s, F_SETFL, 0) == -1)
		{
			Handler().LogError(this, "fcntl(F_SETFL, 0)", Errno, StrError(Errno), LOG_LEVEL_ERROR);
			return false;
		}
	}
	return true;
}

bool Socket::Ready()
{
	if (m_socket != INVALID_SOCKET && !CloseAndDelete()) return true;
	return false;
}


void Socket::OnLine(const std::string& )
{
}


void Socket::OnConnectFailed()
{
}


Socket *Socket::GetParent()
{
	return m_parent;
}


void Socket::SetParent(Socket *x)
{
	m_parent = x;
}

port_t Socket::GetPort()
{
	Handler().LogError(this, "GetPort", 0, "GetPort only implemented for ListenSocket", LOG_LEVEL_WARNING);
	return 0;
}


bool Socket::OnConnectRetry()
{
	return true;
}

time_t Socket::Uptime()
{
	return time(NULL) - m_tCreate;
}

void Socket::DisableRead(bool x)
{
	m_b_disable_read = x;
}


bool Socket::IsDisableRead()
{
	return m_b_disable_read;
}


void Socket::SendBuf(const char *,size_t,int)
{
}


void Socket::Send(const std::string&,int)
{
}

void Socket::SetConnected(bool x)
{
	m_connected = x;
}


bool Socket::IsConnected()
{
	return m_connected;
}


void Socket::OnDisconnect()
{
}


void Socket::OnDisconnect(short, int)
{
}


void Socket::SetLost()
{
	m_bLost = true;
}


bool Socket::Lost()
{
	return m_bLost;
}

void Socket::SetErasedByHandler(bool x)
{
	m_b_erased_by_handler = x;
}


bool Socket::ErasedByHandler()
{
	return m_b_erased_by_handler;
}


time_t Socket::TimeSinceClose()
{
	return time(NULL) - m_tClose;
}

void Socket::SetClientRemoteAddress(SocketAddress& ad)
{
	if (!ad.IsValid())
	{
		Handler().LogError(this, "SetClientRemoteAddress", 0, "remote address not valid", LOG_LEVEL_ERROR);
	}
	m_client_remote_address = ad.GetCopy();
}


std::unique_ptr<SocketAddress> Socket::GetClientRemoteAddress()
{
	if (!m_client_remote_address.get())
	{
		Handler().LogError(this, "GetClientRemoteAddress", 0, "remote address not yet set", LOG_LEVEL_ERROR);
	}
	return m_client_remote_address -> GetCopy();
}

uint64_t Socket::GetBytesSent(bool)
{
	return 0;
}


uint64_t Socket::GetBytesReceived(bool)
{
	return 0;
}


// void Socket::SetCallOnConnect(bool x)
// {
//   m_call_on_connect = x;
//   if (x)
//     Handler().SetCallOnConnect();
// }
//
//
// bool Socket::CallOnConnect()
// {
//   return m_call_on_connect;
// }
//
// void Socket::SetRetryClientConnect(bool x)
// {
//   m_b_retry_connect = x;
//   if (x)
//     Handler().SetRetry();
// }
//
//
// bool Socket::RetryClientConnect()
// {
//   return m_b_retry_connect;
// }

#ifdef ENABLE_POOL
void Socket::CopyConnection(Socket *sock)
{
	Attach( sock -> GetSocket() );

	SetSocketType( sock -> GetSocketType() );
	SetSocketProtocol( sock -> GetSocketProtocol() );

	SetClientRemoteAddress( *sock -> GetClientRemoteAddress() );
	SetRemoteAddress( *sock -> GetRemoteSocketAddress() );
}


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
	if (m_bClient)
		m_bRetain = true;
}


bool Socket::Retain()
{
	return m_bRetain;
}

#endif // ENABLE_POOL
	
int Socket::SoType()
{
	int value = 0;
#ifdef SO_TYPE
	socklen_t len = sizeof(value);
	if (getsockopt(GetSocket(), SOL_SOCKET, SO_TYPE, (char *)&value, &len) == -1)
	{
		Handler().LogError(this, "getsockopt(SOL_SOCKET, SO_TYPE)", Errno, StrError(Errno), LOG_LEVEL_FATAL);
	}
#else
	Handler().LogError(this, "socket option not available", 0, "SO_TYPE", LOG_LEVEL_INFO);
#endif
	return value;
}

void Socket::SetTimeout(time_t secs)
{
	if (!secs)
	{
		m_timeout_start = 0;
		m_timeout_limit = 0;
		return;
	}
	m_timeout_start = time(NULL);
	m_timeout_limit = secs;
	Handler().SetTimeout();
}

bool Socket::CheckTimeout()
{
	return m_timeout_start > 0 && m_timeout_limit > 0;
}


void Socket::OnTimeout()
{
}


void Socket::OnConnectTimeout()
{
}


bool Socket::Timeout(time_t tnow)
{
	if (m_timeout_start > 0 && tnow - m_timeout_start > m_timeout_limit)
		return true;
	return false;
}

/** Returns local port number for bound socket file descriptor. */
port_t Socket::GetSockPort()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr *)&sa, (socklen_t*)&sockaddr_length) == -1)
		memset(&sa, 0, sizeof(sa));
	return ntohs(sa.sin_port);
}

/** Returns local ipv4 address for bound socket file descriptor. */
ipaddr_t Socket::GetSockIP4()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr *)&sa, (socklen_t*)&sockaddr_length) == -1)
		memset(&sa, 0, sizeof(sa));
	ipaddr_t a;
	memcpy(&a, &sa.sin_addr, 4);
	return a;
}

/** Returns local ipv4 address as text for bound socket file descriptor. */
std::string Socket::GetSockAddress()
{
	struct sockaddr_in sa;
	socklen_t sockaddr_length = sizeof(struct sockaddr_in);
	if (getsockname(GetSocket(), (struct sockaddr *)&sa, (socklen_t*)&sockaddr_length) == -1)
		memset(&sa, 0, sizeof(sa));
	Ipv4Address addr( sa );
	return addr.Convert();
}
