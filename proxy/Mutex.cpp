/*
 * Auther: icecream-xu
 * Date: 2022-09-04
 * Email: vlicecream@163.com
 * */

#include "Mutex.h"


Mutex::Mutex()
{
	pthread_mutex_init(&m_mutex, NULL);
}

Mutex::~Mutex()
{
	pthread_mutex_destroy(&m_mutex);
}

void Mutex::Lock() const
{
	pthread_mutex_lock(&m_mutex);
}

void Mutex::Unlock() const 
{
	pthread_mutex_unlock(&m_mutex);
}
