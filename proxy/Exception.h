/*
 * Auther:
 * Date: 2022-09-15
 * Email: vlicecream@163.com
 * Detail:
 * */

#ifndef __SOCKET_Exception_H__
#define __SOCKET_Exception_H__

#include <string>

class Exception
{
public:
	// 构造函数
	Exception(const std::string& description);

	// 析构函数
	virtual ~Exception() {};

	virtual const std::string ToString() const;
	virtual const std::string Stack() const;

	Exception(const Exception&) {} // 拷贝构造函数
	Exception& operator=(const Exception&) { return *this; } // operator

private:
	std::string m_description;
	std::string m_stack;
};

#endif // __SOCKET_Exception_H__
