/*
 * Auther:
 * Date: 2022-09-10
 * Email: vlicecream@163.com
 * */

#include "Ipv4Address.h"
#include "UdpSocket.h"
#include "ISocketHandler.h"
#include "Utility.h"

UdpSocket::UdpSocket(ISocketHandler& handler, int bufSize, bool ipv6, int retries) 
	: 
		Socket(handler),
		m_ibuf(new char[bufSize]),
		m_ibufsz(bufSize),
		m_bind_ok(false),
		m_port(0),
		m_last_size_written(-1),
		m_retries(retries)
{
	
}

UdpSocket::~UdpSocket()
{
	Close();
	delete [] m_ibuf;
}

int UdpSocket::Bind(port_t& port, int range)
{
	Ipv4Address ad(port);
	int n = Bind(ad, range);
	if (m_bind_ok) {
		port = m_port;
	}
	return n;
}

int UdpSocket::Bind(const std::string& intf, port_t& port, int range)
{
	Ipv4Address ad(port);
	if (ad.IsValid()) {
		int n = Bind(ad, range);
		if (m_bind_ok) {
			port = m_port;
		}
		return n;
	}
	// 设置关闭和删除以终止连接
	SetCloseAndDelete();
	return -1;
}

int UdpSocket::Bind(ipaddr_t ipaddr, port_t &port, int range)
{
	Ipv4Address ad(port);
	int n = Bind(ad, range);
	if (m_bind_ok) {
		port = m_port;
	}
	return n;
}

int UdpSocket::Bind(SocketAddress& ad, int range)
{
	// 如果获取文件描述符是-1 则新创建一个socket
	if (GetSocket() == INVALID_SOCKET) {
		Attach(CreateSocket(ad.GetFamily(), SOCK_DGRAM, "udp"));
	}

	// 如果文件描述符不等于-1
	if (GetSocket() != INVALID_SOCKET) {

		// 设置为非阻塞操作
		SetNonblocking(true);

		// 建立连接
		int n = bind(GetSocket(), ad, ad);
		// 重试连接
		int tries = range;
		while (n == 1 && tries--) {
			ad.SetPort(ad.GetPort() + 1);
			n = bind(GetSocket(), ad, ad);
		}

		// 重试完还是失败 直接输出log
		if (n == -1) {
			Handler().LogError(this, "bind", Errno, StrError(Errno), LOG_LEVEL_FATAL);
			SetCloseAndDelete();
			return -1;
		}
		m_bind_ok = true;
		m_port = ad.GetPort();
		return 0;
	}
	return -1;
}

bool UdpSocket::Open(ipaddr_t l, port_t port)
{
	Ipv4Address ad(l, port);
	return Open(ad);
}

bool UdpSocket::Open(std::string& host, port_t port)
{
	Ipv4Address ad(host, port);
	if (ad.IsValid()) {
		return Open(ad);	
	}
	return false;
}

bool UdpSocket::Open(SocketAddress& ad)
{
	if (GetSocket() == INVALID_SOCKET) {
		Attach(CreateSocket(ad.GetFamily(), SOCK_DGRAM, "udp"));
	}
	if (GetSocket() != INVALID_SOCKET) {
		SetNonblocking(true);
		if (connect(GetSocket(), ad, ad) == -1) {
			Handler().LogError(this, "connect", Errno, StrError(Errno), LOG_LEVEL_FATAL);
			return false;
		}
		SetConnected();
		return true;
	}
	return false;
}

void UdpSocket::CreateConnection()
{
	if (GetSocket() == INVALID_SOCKET) {
		SOCKET s = CreateSocket(AF_INET, SOCK_DGRAM, "udp");
		if (s == INVALID_SOCKET) {
			return;
		}
		SetNonblocking(true, s);
		Attach(s);
	}
}

void UdpSocket::SendBuf(const char* data, size_t len, int flags)
{
	if (!IsConnected()) {
		Handler().LogError(this, "SendBuf", 0, "not_connected", LOG_LEVEL_ERROR);
		return;
	}

	if ((m_last_size_written = send(GetSocket(), data, (int)len, flags)) == -1) {
		Handler().LogError(this, "SendBuf", 0, "not_connected", LOG_LEVEL_ERROR);
	}
}

void UdpSocket::Send(const std::string& str, int flags)
{
	SendBuf(str.c_str(), (int)str.size(), flags);
}

void UdpSocket::SendToBuf(const std::string& str, port_t port, const char*data, int len, int flags)
{
	Ipv4Address ad(str, port);
	if (ad.IsValid()) {
		SendToBuf(ad, data, len, flags);
	}
}

void UdpSocket::SendToBuf(ipaddr_t ipaddr, port_t port, const char* data, int len, int flags)
{
	Ipv4Address ad(ipaddr, port);
	SendToBuf(ad, data, len, flags);
}

void UdpSocket::SendToBuf(SocketAddress &ad, const char* data, int len, int flags)
{
	if (GetSocket() == INVALID_SOCKET) {
		Attach(CreateSocket(ad.GetFamily(), SOCK_DGRAM, "udp"));
	}

	if (GetSocket() != INVALID_SOCKET) {
		SetNonblocking(true);
		if ((m_last_size_written = sendto(GetSocket(), data, len, flags, ad, ad))== -1) {
			Handler().LogError(this, "sendto Failed", errno, StrError(errno), LOG_LEVEL_ERROR);
		}
	}
}

void UdpSocket::SendTo(const std::string& str, port_t port, const std::string& data, int flags)
{
	SendToBuf(str, port, data.c_str(), (int)data.size(), flags);
}

void UdpSocket::SendTo(ipaddr_t ipaddr, port_t port, const std::string& data, int flags)
{
	SendToBuf(ipaddr, port, data.c_str(), (int)data.size(), flags);
}

void UdpSocket::SendTo(SocketAddress &ad, const std::string& data, int flags)
{
	SendTo(ad, data, flags);
}

void UdpSocket::OnRead()
{
	struct sockaddr_in sa;
	socklen_t sa_len = sizeof(sa);
	if (m_b_read_ts) {
		struct timeval ts;
		Utility::GetTime(&ts);

		int n = recvfrom(GetSocket(), m_ibuf, m_ibufsz, 0, (struct sockaddr*)&sa, &sa_len);

		if (n > 0) {
			this -> OnRawDate(m_ibuf, n, (struct sockaddr*)&sa, sa_len, &ts);
		} else if (n == -1) {
			if (Errno != EWOULDBLOCK) {
				Handler().LogError(this, "recvform Failed", errno, StrError(errno), LOG_LEVEL_ERROR);
			}
		}
		return;
	}
	int n = recvfrom(GetSocket(), m_ibuf, m_ibufsz, 0, (struct sockaddr*)&sa, &sa_len);
	int q = m_retries;
	while (n > 0) {
		if (sa_len != sizeof(sa)) {
			Handler().LogError(this, "recvfrom", 0, "unexpected address struct size", LOG_LEVEL_WARNING);
		}
		this -> OnRawDate(m_ibuf, m_ibufsz, (struct sockaddr*)&sa, sa_len);
		if (!q--) {
			int n = recvfrom(GetSocket(), m_ibuf, m_ibufsz, 0, (struct sockaddr*)&sa, &sa_len);
		}
	}
	if (n == -1) {
		if (Errno != EWOULDBLOCK) {
			Handler().LogError(this, "recvfrom", Errno, StrError(Errno), LOG_LEVEL_ERROR);
		}
	}
}

void UdpSocket::SetBroadcast(bool b)
{
	// one -> 不为0 则开启广播
	// zero -> 为0 关闭广播
	int one = 1;
	int zero = 0;

	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	if (b) {
		/* 用于任意类型、任意状态套接口的设置选项值
			 SO_BROADCAST -> 允许或禁止广播 */ 
		if (setsockopt(GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof(one)) == -1) {
			Handler().LogError(this, "SetBroadcast", Errno, StrError(Errno), LOG_LEVEL_WARNING);
		}
	} else {
		if (setsockopt(GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof(zero)) == -1) {
			Handler().LogError(this, "SetBroadcast", Errno, StrError(Errno), LOG_LEVEL_WARNING);
		}
	}
}

bool UdpSocket::IsBroadcast()
{
	int is_broadcast = 0;
	socklen_t size = sizeof(int);

	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	/* 用于获取任意类型、任意状态套接口的选项当前值，并将结果存入optval
	    optval 对应 is_broadcast
			若无错误发生，getsockopt()返回0。否则的话，返回SOCKET_ERROR(-1)错误 */
	if (getsockopt(GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&is_broadcast, &size) == -1) {
		Handler().LogError(this, "IsBroadcast", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}
	
	return is_broadcast != 0;
}

void UdpSocket::SetMulticastTTL(int ttl)
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	if (setsockopt(GetSocket(), SOL_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(int)) == -1) {
		Handler().LogError(this, "SetMulticastTTL", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}
}

int UdpSocket::GetMulticastTTL()
{
	int ttl = 0;
	socklen_t size = sizeof(int);

	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	if (getsockopt(GetSocket(), SOL_IP, IP_MULTICAST_TTL, (char*)&ttl, &size) == -1) {
		Handler().LogError(this, "GetMulticastTTL", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}
	return ttl;
}

void UdpSocket::SetMulticastLoop(bool b)
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	int val = b ? 1 : 0;
		if (setsockopt(GetSocket(), SOL_IP, IP_MULTICAST_LOOP, (char*)&val, sizeof(int)) == -1) {
		Handler().LogError(this, "SetMulticastLoop(ipv4)", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}
}

bool UdpSocket::IsMulticastLoop()
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	int is_loop = 0;
	socklen_t size = sizeof(int);

		if (getsockopt(GetSocket(), SOL_IP, IP_MULTICAST_LOOP, (char*)&is_loop, &size) == -1) {
		Handler().LogError(this, "IsMulticastLoop(ipv4)", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}

	return is_loop ? true : false;
}

void UdpSocket::AddMulticastMembership(const std::string& group, const std::string& local_if, int if_index)
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	struct ip_mreq x;
	ipaddr_t addr;

	if (Utility::u2ip(group, addr)) {
		memcpy(&x.imr_multiaddr, &addr, sizeof(addr));	
		Utility::u2ip(local_if, addr);
		memcpy(&x.imr_interface, &addr,sizeof(addr));

		if (setsockopt(GetSocket(), SOL_IP, IP_ADD_MEMBERSHIP, (char*)&x, sizeof(struct ip_mreq)) == -1) {
					Handler().LogError(this, "AddMulticastMembership(ipv4)", Errno, StrError(Errno), LOG_LEVEL_WARNING);
				}
	}
}

void UdpSocket::DropMulticastMembership(const std::string& group, const std::string& local_if, int if_index)
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	struct ip_mreq x;

	ipaddr_t addr;
	if (Utility::u2ip(group, addr)) {
		memcpy(&x.imr_multiaddr, &addr, sizeof(addr));	
		Utility::u2ip(local_if, addr);
		memcpy(&x.imr_interface, &addr,sizeof(addr));

		if (setsockopt(GetSocket(), SOL_IP, IP_ADD_MEMBERSHIP, (char*)&x, sizeof(struct ip_mreq)) == -1) {
					Handler().LogError(this, "DropMulticastMembership(ipv4)", Errno, StrError(Errno), LOG_LEVEL_WARNING);
				}
	}
}

bool UdpSocket::IsBound()
{
	return m_bind_ok;
}

void UdpSocket::OnRawDate(const char *buf, size_t len, struct sockaddr *sa, socklen_t sa_len)
{
}

void UdpSocket::OnRawDate(const char *buf, size_t len, struct sockaddr *sa, socklen_t sa_len, struct timeval *ts)
{
}

port_t UdpSocket::GetPort()
{
	return m_port;
}

int UdpSocket::GetLastSizeWritten()
{
	return m_last_size_written;
}

void UdpSocket::SetTimestamp(bool b)
{
	m_b_read_ts = b;
}

void UdpSocket::SetMulticastDefaultInterface(const std::string &intf, int if_index)
{
	if (GetSocket() == INVALID_SOCKET) {
		CreateConnection();
	}

	ipaddr_t a;
	if (Utility::u2ip(intf, a)) {
		SetMulticastDefaultInterface(a, if_index);
	}
}

void UdpSocket::SetMulticastDefaultInterface(ipaddr_t ipaddr, int if_index)
{
	struct in_addr x;
	memcpy(&x.s_addr, &ipaddr, sizeof(x.s_addr));
	if (setsockopt(GetSocket(), IPPROTO_IP, IP_MULTICAST_IF, &x, sizeof(x)) == -1) {
		Handler().LogError(this, "SetMulticastDefaultInterface(ipv4)", Errno, StrError(Errno), LOG_LEVEL_WARNING);
	}
}
