/*
 * Auther:
 * Date: 2022-09-09
 * Email: vlicecream@163.com
 * Detail: pthread 信号量包装器
 * */

#ifndef __SOCKET_Semaphore_H__
#define __SOCKET_Semaphore_H__

#include <semaphore.h>
#include <pthread.h>
#include "SocketInclude.h"
#include "SocketConfig.h"

typedef unsigned int value_t;

class Semaphore
{
public:
	// 构造函数
	Semaphore(value_t start_val = 0);		

	// 析构函数
	~Semaphore();

	/* 如果成功则返回0 */
	int Post();

	/* 等待Post
			如果成功则返回0 */
	int Wait();

	/* win32 没有实现 */
	int TryWait();

	/* win32 没有实现 */
	int GetValue(int&);

private:
	Semaphore(const Semaphore&) {}  // 拷贝构造函数
	Semaphore& operator= (const Semaphore&) { return *this; };

	sem_t m_sem;
};

#endif // __SOCKET_Semaphore_H__
