/*
 * Author: icecream-xu
 * Date: 2022-09-03
 * Email: vlicecream@163.com
 * Detail: tcp-socket
 * */

#ifndef __SOCKET_TcpSocket_H__
#define __SOCKET_TcpSocket_H__

#include "SocketConfig.h"
#include "StreamSocket.h"

#include "Mutex.h"
#include <map>

#define TCP_BUFSIZE_READ 16400
#define TCP_OUTPUT_CAPACITY 1024000

// flags used in OnDisconnect callback
#define TCP_DISCONNECT_WRITE 1
#define TCP_DISCONNECT_ERROR 2
#define TCP_DISCONNECT_SSL   4

class TcpSocket : public StreamSocket
{
protected:
	/* 包含一个读/写循环缓冲区的缓冲区类 */
	class CircularBuffer
	{
	public:
		CircularBuffer(size_t size);
		~CircularBuffer();

		/* 将 l 个字节从 p 追加到缓冲区 */
		bool Write(const char *p, size_t l);

		/* 从缓冲区复制 l 个字节到目标 */
		bool Read(char *dest, size_t l);

		/* 从缓冲区中跳过 l 个字节 */
		bool Remove(size_t l);

		/* 从缓冲区中读 l bytes, 返回 string */
		std::string ReadString(size_t l);

		/* 缓冲区总长度 */
		size_t GetLength();

		/* 指向循环缓冲区开始的指针 */
		const char *GetStart();

		/* 返回从循环缓冲区开始到缓冲区物理结束的字节数 */
		size_t GetL();

		/* 返回缓冲区中的可用空间，缓冲区溢出前的字节数 */
		size_t Space();

		/* 返回写入该缓冲区的总字节数 */
		unsigned long ByteCounter(bool clear = false);

	private:
		CircularBuffer(const CircularBuffer& ) {}
		CircularBuffer& operator=(const CircularBuffer& ) { return *this; }
		char *buf;
		size_t m_max;
		size_t m_q;
		size_t m_b;
		size_t m_t;
		unsigned long m_count;
	};

	/* 输出缓冲区结构	*/
	struct OUTPUT {
		OUTPUT();
		OUTPUT(const char *buf, size_t len);
		size_t Space();
		void Add(const char *buf, size_t len);
		size_t Remove(size_t len);
		const char *Buf();
		size_t Len();

		size_t _b;
		size_t _t;
		size_t _q;
		char _buf[TCP_OUTPUT_CAPACITY];
	};
	typedef std::list<OUTPUT *> output_l;

public:
	/* 具有输入/输出缓冲区标准值的构造函数 */
	TcpSocket(ISocketHandler&);
	TcpSocket(ISocketHandler&, size_t iSize, size_t oSize);
	~TcpSocket();

	/* 打开与远程服务器的连接
			如果您希望您的套接字连接到服务器，请始终在将套接字添加到套接字处理程序之前调用 Open 
			如果不是，则套接字处理程序将不会监视连接尝试 
			param ip -> ip address
			param port -> port number
			param skip_socks 不允许使用ipv4 哪怕已经配置了 */

	bool Open(ipaddr_t ip, port_t port, bool skip_socks = false);
	bool Open(SocketAddress&, bool skip_socks = false);
	bool Open(SocketAddress&, SocketAddress& bind_address, bool skip_socks = false);

	/* 打开连接
	  param host -> Hostname
		param port -> port number */
	bool Open(const std::string &host, port_t port);

	/* 连接超时回调 */
	void OnConnectTimeout();
	
	/* 关闭文件描述符 */
	int Close();

	/* 发送一个字符串 */
	void Send(const std::string &s, int f = 0);

	/* 使用 printf 格式发送字符串 */
	void Sendf(const char *format, ...);

	/* 发送字节缓冲区 
			param buf -> 指向数据的指针
			param len -> 数据的长度 */
	void SendBuf(const char *buf, size_t len, int f = 0);

	/* 在成功读取套接字之后执行此回调
		param buf -> 指向数据的指针
		param len -> 数据的长度 */
	virtual void OnRawData(const char *buf,size_t len);


	/* 当输出缓冲区已发送时调用.
	    Note: 仅当输出缓冲区已被使用时才会被调用	  
			在不需要输出缓冲区的情况下成功发送将不会生成对此方法的调用 */
	virtual void OnWriteComplete();
	/* 获得输入缓冲区的字节数 */
	size_t GetInputLength();

	/* 从输入缓冲区中读取数据 */
	size_t ReadInput(char *buf, size_t sz);

	/* 获取输出缓冲区的字节数. */
	size_t GetOutputLength();

	/* 当线路协议中的套接字读取一整行时触发回调
			param line -> Line read */
	void OnLine(const std::string& line);

	/* 获取接收字节数的计数器 */
	uint64_t GetBytesReceived(bool clear = false);

	/* 获取发送字节数的计数器 */
	uint64_t GetBytesSent(bool clear = false);

	/* Socks4 特定回调 */
	void OnSocks4Connect();
	/* Socks4 特定回调 */
	void OnSocks4ConnectFailed();
	/* Socks4 特定回调 */
	bool OnSocks4Read();

	/* 如果您仅使用 OnRawData 处理接收到的数据，请使用此选项 */
	void DisableInputBuffer(bool = true);

	void OnOptions(int,int,int,SOCKET);

	void SetLineProtocol(bool = true);


	/* 使用 SetLineProtocol = true 时获取未完成的行
	    完成的线路将始终通过调用 OnLine 来报告 */
	const std::string GetLine() const;

	// TCP 选项
	bool SetTcpNodelay(bool = true);

	virtual int Protocol();

	/* 回调 OnTransferLimit 的触发限制 */
	void SetTransferLimit(size_t sz);
	/* 当输出缓冲区低于 SetTransferLimit 设置的值时，将触发此回调							
			默认值：0（禁用） */
	virtual void OnTransferLimit();

protected:
	TcpSocket(const TcpSocket& );

	void OnRead();
	void OnRead( char *buf, size_t n );
	void OnWrite();

	CircularBuffer ibuf; //< 循环输入缓冲区

private:
	TcpSocket& operator=(const TcpSocket& ) { return *this; }

	/* */
	void SendFromOutputBuffer();
	/* 实际 send() */
	int TryWrite(const char *buf, size_t len);
	/* 将数据添加到输出缓冲区顶部 */
	void Buffer(const char *buf, size_t len);

private:
	bool m_b_input_buffer_disabled;
	uint64_t m_bytes_sent;
	uint64_t m_bytes_received;
	bool m_skip_c; //< 在 OnRead 中跳过 CRLF 或 LFCR 序列的第二个字符
	char m_c; //< CRLF 或 LFCR 序列中的第一个字符
	std::vector<char> m_line; //< 当前线路处于线路协议模式
	size_t m_line_ptr;
	output_l m_obuf; //< 输出缓冲区
	OUTPUT *m_obuf_top; ///< 顶部输出缓冲区
	size_t m_transfer_limit;
	size_t m_output_length;
	size_t m_repeat_length;
};

#endif // __SOCKET_TcpSocket_H__
