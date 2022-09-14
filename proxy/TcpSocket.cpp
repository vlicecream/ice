/*
 * Auther: icecream-xu
 * Date: 2022-09-04
 * Email: vlicecream@163.com
 * */

#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <map>
#include <stdio.h>

#include "ISocketHandler.h"
#include "TcpSocket.h"
#include "Utility.h"
#include "Ipv4Address.h"
#include "IFile.h"
#include "Lock.h"
#include "SocketConfig.h"

#ifdef _DEBUG
#define DEB(x) x
#else
#define DEB(x) 
#endif

TcpSocket::TcpSocket(ISocketHandler& h) : 
	StreamSocket(h),
	ibuf(TCP_BUFSIZE_READ),
	m_b_input_buffer_disabled(false),
	m_bytes_sent(0),
	m_bytes_received(0),
	m_skip_c(false),
	m_line(Handler().MaxTcpLineSize()),
	m_line_ptr(0),
	m_obuf_top(NULL),
	m_transfer_limit(0),
	m_output_length(0),
	m_repeat_length(0)
{
}

TcpSocket::TcpSocket(ISocketHandler& h,size_t isize,size_t osize) :					
	StreamSocket(h),
	ibuf(isize),
	m_b_input_buffer_disabled(false),
	m_bytes_sent(0),
	m_bytes_received(0),
	m_skip_c(false),
	m_line(Handler().MaxTcpLineSize()),
	m_line_ptr(0),
	m_obuf_top(NULL),
	m_transfer_limit(0),
	m_output_length(0),
	m_repeat_length(0)
{
}

TcpSocket::~TcpSocket()
{
	// empty m_obuf
	while (m_obuf.size()) {
		output_l::iterator it = m_obuf.begin();
		OUTPUT *p = *it;
		delete p;
		m_obuf.erase(it);
	}
}

bool TcpSocket::Open(ipaddr_t ip,port_t port,bool skip_socks)
{
	Ipv4Address ad(ip, port);
	Ipv4Address local;
	return Open(ad, local, skip_socks);
}

bool TcpSocket::Open(SocketAddress& ad,bool skip_socks)
{
	Ipv4Address bind_ad("0.0.0.0", 0);
	return Open(ad, bind_ad, skip_socks);
}


bool TcpSocket::Open(SocketAddress& ad,SocketAddress& bind_ad,bool skip_socks)
{
	// 查看地址是否有效
	if (!ad.IsValid()) {
		Handler().LogError(this, "Open", 0, "Invalid SocketAddress", LOG_LEVEL_FATAL);
		// 设置关闭和删除以终止连接
		SetCloseAndDelete();
		return false;
	}
	// 如果 handler 处理的 socket 的数目大于最大的处理数目,则报错并设置关闭和删除以终止连接
	if (Handler().GetCount() >= Handler().MaxCount()) {
		Handler().LogError(this, "Open", 0, "no space left for more sockets", LOG_LEVEL_FATAL);
		SetCloseAndDelete();
		return false;
	}
	SetConnecting(false);

#ifdef ENABLE_POOL
	// 查看是否开启连接池
	if (Handler().PoolEnabled()) {
		// 查找可用的打开连接
		ISocketHandler::PoolSocket *pools = Handler().FindConnection(SOCK_STREAM, "tcp", ad);
		if (pools) {
			CopyConnection(pools);
			delete pools;

			SetIsClient();
			SetCallOnConnect(); // ISocketHandler 必须调用 OnConnect
			Handler().LogError(this, "SetCallOnConnect", 0, "Found pooled connection", LOG_LEVEL_INFO);
			return true;
		}
	}
#endif

	// 如果没有 创建一个新的连接
	SOCKET s = CreateSocket(ad.GetFamily(), SOCK_STREAM, "tcp");
	if (s == INVALID_SOCKET) {
		return false;
	}
	// 对于异步连接，套接字必须是非阻塞的
	if (!SetNonblocking(true, s)) {
		SetCloseAndDelete();
		closesocket(s);
		return false;
	}
#ifdef ENABLE_POOL
	SetIsClient(); // 客户端 因为我们要连接
#endif
	SetClientRemoteAddress(ad);
	int n = 0;
	if (bind_ad.GetPort() != 0) {
		bind(s, bind_ad, bind_ad);
	}
	n = connect(s, ad, ad);
	SetRemoteAddress(ad);
	if (n == -1) {
		// 检查表示正在连接的错误代码
		if (Errno == EINPROGRESS) {
			Attach(s);
			SetConnecting( true ); // 这个标志将控制 fd_set 的
		} else {
			Handler().LogError(this, "connect: failed", Errno, StrError(Errno), LOG_LEVEL_FATAL);
			SetCloseAndDelete();
			closesocket(s);
			return false;
		}
	} else {
		Attach(s);
		SetCallOnConnect(); // ISocketHandler 必须调用 OnConnect
	}

	// 'true' 表示已连接或正在连接（尚未连接）
  // 'false' 表示某事失败
	return true; //!Connecting();
}

bool TcpSocket::Open(const std::string &host,port_t port)
{
	ipaddr_t l;
	if (!Utility::u2ip(host,l))
	{
		SetCloseAndDelete();
		return false;
	}
	Ipv4Address ad(l, port);
	Ipv4Address local;
	return Open(ad, local);
}

void TcpSocket::OnRead()
{
	int n = 0;
	char buf[TCP_BUFSIZE_READ];
	n = recv(GetSocket(), buf, TCP_BUFSIZE_READ, MSG_NOSIGNAL);
	if (n == -1) {
		Handler().LogError(this, "read", Errno, StrError(Errno), LOG_LEVEL_FATAL);
		// 当检测到断开连接时 TODO: 应该重启连接
		OnDisconnect();
		OnDisconnect(TCP_DISCONNECT_ERROR, Errno);
		// 设置关闭和删除以终止连接
		SetCloseAndDelete(true);
		// 在关闭之前设置刷新以使 tcp 套接字在关闭连接之前完全清空其输出缓冲区 false 不清空
		SetFlushBeforeClose(false);
		// 连接丢失-读取/写入套接字时出错-仅TcpSocket
		SetLost();
		return;
	} else if (!n) {
		OnDisconnect();
		OnDisconnect(0, 0);
		SetCloseAndDelete(true);
		SetFlushBeforeClose(false);
		SetLost();
		SetShutdown(SHUT_WR);
		return;
	} else if (n > 0 && n <= TCP_BUFSIZE_READ) {
		m_bytes_received += n;
		// 写入文件
		if (GetTrafficMonitor()) {
			GetTrafficMonitor() -> fwrite(buf, 1, n);
		}
		if (!m_b_input_buffer_disabled && !ibuf.Write(buf,n)) {
			Handler().LogError(this, "OnRead", 0, "ibuf overflow", LOG_LEVEL_WARNING);
		}
	} else {
		Handler().LogError(this, "OnRead", n, "abnormal value from recv", LOG_LEVEL_ERROR);
	}
	//
	OnRead( buf, n );
}

void TcpSocket::OnRead( char *buf, size_t n )
{
	// 无缓冲
	if (n > 0 && n <= TCP_BUFSIZE_READ) {
		if (LineProtocol()) {
			buf[n] = 0;
			size_t i = 0;
			if (m_skip_c && (buf[i] == 13 || buf[i] == 10) && buf[i] != m_c) {
				m_skip_c = false;
				i++;
			}
			size_t x = i;
			for (; i < n && LineProtocol(); i++) {
				while ((buf[i] == 13 || buf[i] == 10) && LineProtocol()) {
					char c = buf[i];
					buf[i] = 0;
					if (buf[x]) {
						size_t sz = strlen(&buf[x]);
						if (m_line_ptr + sz < Handler().MaxTcpLineSize()) {
							memcpy(&m_line[m_line_ptr], &buf[x], sz);
							m_line_ptr += sz;
						} else {
							Handler().LogError(this, "TcpSocket::OnRead", (int)(m_line_ptr + sz), "maximum tcp_line_size exceeded, connection closed", LOG_LEVEL_FATAL);
							SetCloseAndDelete();
						}
					}
					if (m_line_ptr > 0)
						OnLine( std::string(&m_line[0], m_line_ptr) );
					else
						OnLine("");
					i++;
					m_skip_c = true;
					m_c = c;
					if (i < n && (buf[i] == 13 || buf[i] == 10) && buf[i] != c) {
						m_skip_c = false;
						i++;
					}
					x = i;
					m_line_ptr = 0;
				}
				if (!LineProtocol()) {
					break;
				}
			}
			if (!LineProtocol()) {
				if (i < n) {
					OnRawData(buf + i, n - i);
				}
			} else if (buf[x]) {
				size_t sz = strlen(&buf[x]);
				if (m_line_ptr + sz < Handler().MaxTcpLineSize()) {
					memcpy(&m_line[m_line_ptr], &buf[x], sz);
					m_line_ptr += sz;
				} else {
					Handler().LogError(this, "TcpSocket::OnRead", (int)(m_line_ptr + sz), "maximum tcp_line_size exceeded, connection closed", LOG_LEVEL_FATAL);
					SetCloseAndDelete();
				}
			}
		} else {
			OnRawData(buf, n);
		}
	}
	if (m_b_input_buffer_disabled) {
		return;
	}
}

void TcpSocket::OnWriteComplete()
{
}

void TcpSocket::OnWrite()
{
	// 查看是否正在连接
	if (Connecting()) {
		int err = SoError();

		// 不要在此处重置连接标志错误，我们希望稍后 OnConnectFailed 超时
		if (!err) {  // ok
			Handler().ISocketHandler_Mod(this, !IsDisableRead(), false);
			SetConnecting(false);
			SetCallOnConnect();
			return;
		}
		Handler().LogError(this, "tcp: connect failed", err, StrError(err), LOG_LEVEL_FATAL);
		Handler().ISocketHandler_Mod(this, false, false); // 由于连接失败，不再进行监控
		// failed
		if (GetConnectionRetry() == -1 || (GetConnectionRetry() && GetConnectionRetries() < GetConnectionRetry()) ) {
			// 即使连接一次失败，也只能在连接超时后重试。
			// 我们是否应该再次尝试连接，当 CheckConnect 返回 false 时，这是因为连接错误 - 而不是超时......
			return;
		}
		SetConnecting(false);
		SetCloseAndDelete( true );
		// TODO: 说明失败的原因
		OnConnectFailed();
		return;
	}

	SendFromOutputBuffer();
}

void TcpSocket::SendFromOutputBuffer()
{
	// 尝试在缓冲区中发送下一个块
	// 如果发送了完整块，请重复
	// 如果发送了所有块，则重置 m_wfds

	bool repeat = false;
	size_t sz = m_transfer_limit ? GetOutputLength() : 0;
	do {
		if (m_obuf.empty())
		{
			Handler().LogError(this, "OnWrite", (int)m_output_length, "Empty output buffer in OnWrite", LOG_LEVEL_ERROR);
			break;
		}
		output_l::iterator it = m_obuf.begin();
		OUTPUT *p = *it;
		repeat = false;
		int n = TryWrite(p -> Buf(), p -> Len());
		if (n > 0)
		{
			size_t left = p -> Remove(n);
			m_output_length -= n;
			if (!left)
			{
				delete p;
				m_obuf.erase(it);
				if (!m_obuf.size())
				{
					m_obuf_top = NULL;
					OnWriteComplete();
				}
				else
				{
					repeat = true;
				}
			}
		}
	} while (repeat);

	if (m_transfer_limit && sz > m_transfer_limit && GetOutputLength() < m_transfer_limit)
	{
		OnTransferLimit();
	}

	// 检查输出缓冲区集，相应地设置/重置 m_wfds
	{
		bool br = !IsDisableRead();
		if (m_obuf.size())
			Handler().ISocketHandler_Mod(this, br, true);
		else
			Handler().ISocketHandler_Mod(this, br, false);
	}
}

int TcpSocket::TryWrite(const char *buf, size_t len)
{
	int n = 0;
	{
		n = send(GetSocket(), buf, (int)len, MSG_NOSIGNAL);
		if (n == -1)
		{
			if (Errno != EWOULDBLOCK)
			{	
				Handler().LogError(this, "send", Errno, StrError(Errno), LOG_LEVEL_FATAL);
				OnDisconnect();
				OnDisconnect(TCP_DISCONNECT_WRITE|TCP_DISCONNECT_ERROR, Errno);
				SetCloseAndDelete(true);
				SetFlushBeforeClose(false);
				SetLost();
			}
			return 0;
		}
	}
	if (n > 0)
	{
		m_bytes_sent += n;
		if (GetTrafficMonitor())
		{
			GetTrafficMonitor() -> fwrite(buf, 1, n);
		}
	}
	return n;
}

void TcpSocket::Buffer(const char *buf, size_t len)
{
	size_t ptr = 0;
	m_output_length += len;
	while (ptr < len)
	{
		// buf/len => pbuf/sz
		size_t space = 0;
		if ((space = m_obuf_top ? m_obuf_top -> Space() : 0) > 0)
		{
			const char *pbuf = buf + ptr;
			size_t sz = len - ptr;
			if (space >= sz)
			{
				m_obuf_top -> Add(pbuf, sz);
				ptr += sz;
			}
			else
			{
				m_obuf_top -> Add(pbuf, space);
				ptr += space;
			}
		}
		else
		{
			m_obuf_top = new OUTPUT;
			m_obuf.push_back( m_obuf_top );
		}
	}
}

void TcpSocket::Send(const std::string &str,int i)
{
	SendBuf(str.c_str(),str.size(),i);
}

void TcpSocket::SendBuf(const char *buf,size_t len,int)
{
	if (!Ready() && !Connecting())
	{
		Handler().LogError(this, "SendBuf", -1, "Attempt to write to a non-ready socket" ); // warning
		if (GetSocket() == INVALID_SOCKET)
			Handler().LogError(this, "SendBuf", 0, " * GetSocket() == INVALID_SOCKET", LOG_LEVEL_INFO);
		if (Connecting())
			Handler().LogError(this, "SendBuf", 0, " * Connecting()", LOG_LEVEL_INFO);
		if (CloseAndDelete())
			Handler().LogError(this, "SendBuf", 0, " * CloseAndDelete()", LOG_LEVEL_INFO);
		return;
	}
	if (!IsConnected())
	{
		Handler().LogError(this, "SendBuf", -1, "Attempt to write to a non-connected socket, will be sent on connect" ); // warning
		Buffer(buf, len);
		return;
	}
	if (m_obuf_top)
	{
		Buffer(buf, len);
		return;
	}

	int n = TryWrite(buf, len);
	if (n >= 0 && n < (int)len)
	{
		Buffer(buf + n, len - n);
	}


	// 检查输出缓冲区集，相应地设置/重置 m_wfds

	{
		bool br = !IsDisableRead();
		if (m_obuf.size())
			Handler().ISocketHandler_Mod(this, br, true);
		else
			Handler().ISocketHandler_Mod(this, br, false);
	}
}

void TcpSocket::OnLine(const std::string& )
{
}

TcpSocket::TcpSocket(const TcpSocket& s)
:StreamSocket(s)
,ibuf(0)
{
}

void TcpSocket::Sendf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char slask[5000]; // vsprintf / vsnprintf temporary
	vsnprintf(slask, sizeof(slask), format, ap);
	va_end(ap);
	Send( slask );
}

int TcpSocket::Close()
{
	if (GetSocket() == INVALID_SOCKET) // 这可能发生
	{
		Handler().LogError(this, "Socket::Close", 0, "file descriptor invalid", LOG_LEVEL_WARNING);
		return 0;
	}
	int n;
	SetNonblocking(true);
	if (!Lost() && IsConnected() && !(GetShutdown() & SHUT_WR))
	{
		if (shutdown(GetSocket(), SHUT_WR) == -1)
		{
			// failed...
			Handler().LogError(this, "shutdown", Errno, StrError(Errno), LOG_LEVEL_ERROR);
		}
	}
	//
	char tmp[1000];
	if (!Lost() && (n = recv(GetSocket(),tmp,1000,0)) >= 0)
	{
		if (n)
		{
			Handler().LogError(this, "read() after shutdown", n, "bytes read", LOG_LEVEL_WARNING);
		}
	}

	return Socket::Close();
}

void TcpSocket::OnRawData(const char *buf_in,size_t len)
{
}


size_t TcpSocket::GetInputLength()
{
	return ibuf.GetLength();
}


size_t TcpSocket::ReadInput(char *buf, size_t max_sz)
{
	size_t sz = max_sz < GetInputLength() ? max_sz : GetInputLength();
	ibuf.Read(buf, sz);
	return sz;
}


size_t TcpSocket::GetOutputLength()
{
	return m_output_length;
}


uint64_t TcpSocket::GetBytesReceived(bool clear)
{
	uint64_t z = m_bytes_received;
	if (clear)
		m_bytes_received = 0;
	return z;
}


uint64_t TcpSocket::GetBytesSent(bool clear)
{
	uint64_t z = m_bytes_sent;
	if (clear)
		m_bytes_sent = 0;
	return z;
}

void TcpSocket::DisableInputBuffer(bool x)
{
	m_b_input_buffer_disabled = x;
}

void TcpSocket::OnOptions(int family,int type,int protocol,SOCKET s)
{
	DEB(	fprintf(stderr, "Socket::OnOptions()\n");)

	SetSoReuseaddr(true);
	SetSoKeepalive(true);
}

void TcpSocket::SetLineProtocol(bool x)
{
	StreamSocket::SetLineProtocol(x);
	DisableInputBuffer(x);
}


const std::string TcpSocket::GetLine() const
{
	if (!m_line_ptr)
		return "";
	return std::string(&m_line[0], m_line_ptr);
}

bool TcpSocket::SetTcpNodelay(bool x)
{

	Handler().LogError(this, "socket option not available", 0, "TCP_NODELAY", LOG_LEVEL_INFO);
	return false;
}

TcpSocket::CircularBuffer::CircularBuffer(size_t size)
:buf(new char[2 * size])
,m_max(size)
,m_q(0)
,m_b(0)
,m_t(0)
,m_count(0)
{
}


TcpSocket::CircularBuffer::~CircularBuffer()
{
	delete[] buf;
}



bool TcpSocket::CircularBuffer::Write(const char *s,size_t l)
{
	if (m_q + l > m_max)
	{
		return false; // 溢出
	}
	m_count += (unsigned long)l;
	if (m_t + l > m_max) // 块跨越圆形边界
	{
		size_t l1 = m_max - m_t; // 直到圆形过境点的剩余尺寸
		// 总是将完整块复制到缓冲区（buf）+顶部指针（m_t）
		// 因为出于性能原因我们将缓冲区大小增加了一倍
		memcpy(buf + m_t, s, l);
		memcpy(buf, s + l1, l - l1);
		m_t = l - l1;
		m_q += l;
	}
	else
	{
		memcpy(buf + m_t, s, l);
		memcpy(buf + m_max + m_t, s, l);
		m_t += l;
		if (m_t >= m_max)
			m_t -= m_max;
		m_q += l;
	}
	return true;
}

bool TcpSocket::CircularBuffer::Read(char *s,size_t l)
{
	if (l > m_q)
	{
		return false; // 没有足够的字符
	}
	if (m_b + l > m_max) // 块跨越圆形边界
	{
		size_t l1 = m_max - m_b;
		if (s)
		{
			memcpy(s, buf + m_b, l1);
			memcpy(s + l1, buf, l - l1);
		}
		m_b = l - l1;
		m_q -= l;
	}
	else
	{
		if (s)
		{
			memcpy(s, buf + m_b, l);
		}
		m_b += l;
		if (m_b >= m_max)
			m_b -= m_max;
		m_q -= l;
	}
	if (!m_q)
	{
		m_b = m_t = 0;
	}
	return true;
}

bool TcpSocket::CircularBuffer::Remove(size_t l)
{
	return Read(NULL, l);
}


size_t TcpSocket::CircularBuffer::GetLength()
{
	return m_q;
}


const char *TcpSocket::CircularBuffer::GetStart()
{
	return buf + m_b;
}


size_t TcpSocket::CircularBuffer::GetL()
{
	return (m_b + m_q > m_max) ? m_max - m_b : m_q;
}


size_t TcpSocket::CircularBuffer::Space()
{
	return m_max - m_q;
}


unsigned long TcpSocket::CircularBuffer::ByteCounter(bool clear)
{
	if (clear)
	{
		unsigned long x = m_count;
		m_count = 0;
		return x;
	}
	return m_count;
}


std::string TcpSocket::CircularBuffer::ReadString(size_t l)
{
	char *sz = new char[l + 1];
	if (!Read(sz, l)) {// 失败，在 Read() 方法中调试打印输出	{
		delete[] sz;
		return "";
	}
	sz[l] = 0;
	std::string tmp = sz;
	delete[] sz;
	return tmp;
}


void TcpSocket::OnConnectTimeout()
{
	Handler().LogError(this, "connect", -1, "connect timeout", LOG_LEVEL_FATAL);

	if (GetConnectionRetry() == -1 ||
		(GetConnectionRetry() && GetConnectionRetries() < GetConnectionRetry()) )
	{
		IncreaseConnectionRetries();
		// 如果我们应该继续尝试，请通过 OnConnectRetry 回调询问套接字
		if (OnConnectRetry())
		{
			SetRetryClientConnect();
		}
		else
		{
			SetCloseAndDelete( true );
			// TODO: 说明连接失败的原因
			OnConnectFailed();
			//
			SetConnecting(false);
		}
	}
	else
	{
		SetCloseAndDelete(true);
		// TODO: 说明连接失败的原因
		OnConnectFailed();
		//
		SetConnecting(false);
	}
}

int TcpSocket::Protocol()
{
	return IPPROTO_TCP;
}


void TcpSocket::SetTransferLimit(size_t sz)
{
	m_transfer_limit = sz;
}


void TcpSocket::OnTransferLimit()
{
}


TcpSocket::OUTPUT::OUTPUT() : _b(0), _t(0), _q(0)
{
}


TcpSocket::OUTPUT::OUTPUT(const char *buf, size_t len) : _b(0), _t(len), _q(len)
{
	memcpy(_buf, buf, len);
}


size_t TcpSocket::OUTPUT::Space()
{
	return TCP_OUTPUT_CAPACITY - _t;
}


void TcpSocket::OUTPUT::Add(const char *buf, size_t len)
{
	memcpy(_buf + _t, buf, len);
	_t += len;
	_q += len;
}


size_t TcpSocket::OUTPUT::Remove(size_t len)
{
	_b += len;
	_q -= len;
	return _q;
}

const char *TcpSocket::OUTPUT::Buf()
{
	return _buf + _b;
}


size_t TcpSocket::OUTPUT::Len()
{
	return _q;
}

