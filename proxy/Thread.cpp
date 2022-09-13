/*
 * Auther:
 * Date: 2022-09-09
 * Email: vlicecream@163.com
 * */

#include <stdio.h>
#include "Thread.h"
#include "Utility.h"

Thread::Thread(bool release)
	:
		m_running(true),
		m_release(false),
		m_b_delete_on_exit(false),
		m_b_destructor(false)
{
	// 初始化thread属性
	pthread_attr_init(&m_attr);

	// 设置线程分离 如果设置了分离 这个线程就是独立状态 自己运行结束了 资源就会被回收
	pthread_attr_setdetachstate(&m_attr, PTHREAD_CREATE_DETACHED);

	/* 创建一个线程 */
	if (pthread_create(&m_thread, &m_attr, StartThread, this) == -1) {
		perror("Create Thread Failed");
		SetRunning(false);
	}
	m_release = release;
	if (release)
		m_sem.Post();
}

Thread::~Thread()
{
	m_b_destructor = true;
	if (m_running) {
		SetReleased(true);
		SetRunning(false);
		// 休眠一秒给线程类Run方法足够的时间从run loop中释放
		Utility::Sleep(1000);
	}
	pthread_attr_destroy(&m_attr);
}

threadfunc_t STDPREFIX Thread::StartThread(threadparam_t tpt)
{
	/* 在这里睡觉以等待派生线程类构造函数设置 vtable.... 只是看着它就很痛 */
	Utility::Sleep(5);

	Thread* p = (Thread*)tpt;

	p -> wait();
	if (p -> m_running) {
		p -> Run();
	}

	p -> SetRunning(false);
	if (p -> DeleteOnExit() && p -> IsDestructor()) {
		delete p;
	}

	return (threadfunc_t)NULL;
}

bool Thread::IsRunning()
{
	return m_running;
}

void Thread::SetRunning(bool x)
{
	m_running = x;
}

bool Thread::IsReleased()
{
	return IsReleased();
}

void Thread::SetReleased(bool x) 
{
	m_release = x;
	if (x) {
		m_sem.Post();
	}
}

bool Thread::DeleteOnExit()
{
	return m_b_delete_on_exit;
}

void Thread::SetDeleteOnExit(bool x)
{
	m_b_delete_on_exit = x;
}

bool Thread::IsDestructor()
{
	return m_b_destructor;
}

void Thread::wait()
{
	m_sem.Wait();
}
