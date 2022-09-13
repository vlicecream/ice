/*
 * Auther:
 * Date: 2022-09-11
 * Email: vlicecream@163.com
 * Deatil: 
 * */

#ifndef __SOCKET_SocketThread_H__
#define __SOCKET_SocketThread_H__


#include "SocketConfig.h"
#ifdef ENABLE_DETACH
#include "Thread.h"
#include "SocketHandler.h"
#endif

class Socket;

class SocketThread : public Thread
{
public:
	SocketThread(Socket *p);
	~SocketThread();

	void Run() override;
private:
	SocketThread(const SocketThread& s) : m_socket(s.m_socket) {}
	SocketThread& operator=(const SocketThread&) { return *this; }

	SocketHandler m_h;
	Socket* m_socket;
};

#endif // __SOCKET_SocketThread_H__
