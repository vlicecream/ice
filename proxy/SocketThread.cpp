/*
 * Auther:
 * Date:
 * Email: vlicecream@163.com
 * */

#include "SocketThread.h"
#include "Utility.h"

SocketThread::SocketThread(Socket* s) 
	:
		Thread(false),
		m_socket(s)
{
}

SocketThread::~SocketThread()
{
	if (IsRunning()) {
		SetRelease(true);
		SetRunning(false);
		m_h.Release();
		Utility::Sleep(5);
	}
}

void SocketThread::Run()
{
	m_h.SetSlave();

	m_h.Add(m_socket);
	m_socket -> SetSlaveHandler(&m_h);
	m_socket -> OnDetached();
	m_h.EnableRelease();
	while (m_h.GetCount() > 1 && IsRunning()) {
		m_h.Select(0, 50000);
	}

	SetDeleteOnExit();
}
