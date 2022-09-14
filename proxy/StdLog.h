/*
 * Auther: ting
 * Date: 2022-09-02
 * Email: vlicecream@163.com
 * */

#ifndef __SOCKET_LOG_H__
#define __SOCKET_LOG_H__


#include <string>
#include "SocketConfig.h"

typedef enum
{
	LOG_LEVEL_INFO = 0,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL
} loglevel_t;

class Socket;
class ISocketHandler;

class StdLog
{
public:
	virtual ~StdLog() {}

	virtual void error(
			ISocketHandler*,
			Socket*,
			const std::string& user_text,
			int err,
			const std::string& sys_err,
			loglevel_t = LOG_LEVEL_WARNING
		) = 0;
};

#endif // __SOCKET_LOG_H__
