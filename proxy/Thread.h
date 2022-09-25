/*
 * Auther: 
 * Date: 2022-09-09
 * Email: vlicecream@163.com
 * Detail:
 * */

#ifndef __SOCKET_Thread_H__
#define __SOCKET_Thread_H__

#include <pthread.h>
#include "SocketConfig.h"
#include "Semaphore.h"

typedef void* threadfunc_t;
typedef void* threadparam_t;

class Thread
{
public:
	// 构造函数
	Thread(bool m_release = true);

	// 析构函数
	virtual ~Thread();

	static threadfunc_t __attribute__((stdcall)) StartThread(threadparam_t);

	virtual void Run() = 0;

	bool IsRunning();
	void SetRunning(bool x);
	bool IsReleased();
	void SetRelease(bool x);
	bool DeleteOnExit();
	void SetDeleteOnExit(bool x = true);

	bool IsDestructor();

	void Start() {
		SetRelease(true);
	}
	
	void Stop() {
		Start();
		SetRunning(false);
	}

	void wait();

protected:
	pthread_t m_thread;
	pthread_attr_t m_attr;

private:
	Thread(const Thread&) {};
	Thread& operator=(const Thread&) { return *this; };
	Semaphore m_sem;

	bool m_running;  //< 是否运行
	bool m_release;  //< 是否释放
	bool m_b_delete_on_exit;
	bool m_b_destructor;  // 是否析构
};

#endif // __SOCKET_Thread_H__
