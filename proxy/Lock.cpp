/*
 * Auther:
 * Date: 2022-09-05
 * Email: vlicecream@163.com
 * */

#include "IMutex.h"
#include "Lock.h"

#ifdef SOCKETS_NAMESPACE
namespace SOCKETS_NAMESPACE {
#endif


Lock::Lock(const IMutex& m) : m_mutex(m)
{
	m_mutex.Lock();
}


Lock::~Lock()
{
	m_mutex.Unlock();
}


