/*
 * Auther: icecream-xu
 * Date: 2022-09-04
 * Email: vlicecream@163.com
 * Deatil: IMutex Interface
 * */

#ifndef __SOCKET_IMutex_H__
#define __SOCKET_IMutex_H__

// #include "SocketInclude.h"

class IMutex
{
public:
	virtual ~IMutex() {}

	virtual void Lock() const = 0;
	virtual void Unlock() const = 0;
};

#endif // __SOCKET_IMutex_H__
