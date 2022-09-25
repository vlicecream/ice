/*
 * Auther: 
 * Date: 2022-09-09
 *
 * */


#include <semaphore.h>

#include "Semaphore.h"

Semaphore::Semaphore(value_t start_val)
{
	// param -> &m_sem 信号量对象
	// param -> 0 信号量的类型 (如果其值为0，则说明是进程中的局部信号量，进程之间不共享 / 否则进程中共享)
	// param -> start_val 信号量初始值 
	sem_init(&m_sem, 0, start_val);
}

Semaphore::~Semaphore()
{
	// 对信号量的清理
	sem_destroy(&m_sem);
}

int Semaphore::Post()
{
	// 信号量+1
	return sem_post(&m_sem);
}

int Semaphore::Wait()
{
	// 当前信号量>0时 信号量-1 / 当前信号量=0时 则调用sem_wait()函数的线程或进程被阻塞  
	return sem_wait(&m_sem);
}

int Semaphore::TryWait()
{
	// 信号量的当前值为0, 则返回错误 而不是阻塞调用
	return sem_trywait(&m_sem);
}

int Semaphore::GetValue(int& i)
{
	// 返回信号量的值 不知道什么系统的情况下会返回负数
	return sem_getvalue(&m_sem, &i);
}
