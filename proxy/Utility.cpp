/*
 * Auther:
 * Date: 2022-09-15
 * Email: vlicecream@163.com
 * */

#include <netdb.h>
#include <vector>
#include <sys/time.h>
#include <sys/types.h>

#include "Utility.h"

// defines for the random number generator
#define TWIST_IA        397
#define TWIST_IB       (TWIST_LEN - TWIST_IA)
#define UMASK           0x80000000
#define LMASK           0x7FFFFFFF
#define MATRIX_A        0x9908B0DF
#define TWIST(b,i,j)   ((b)[i] & UMASK) | ((b)[j] & LMASK)
#define MAGIC_TWIST(s) (((s) & 1) * MATRIX_A)



bool Utility::isipv4(const std::string& str)
{
	int dots = 0;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '.') {
			dots++;
		} else if (!isdigit(str[i])) {
			return false;
		}
	}
	return true;
}

// TODO: 未实现
bool Utility::isipv6(const std::string& str) {
	return false;
}


bool Utility::u2ip(const std::string& str, ipaddr_t& ipaddr)
{
	struct sockaddr_in sa;
	bool r = Utility::u2ip(str, sa);
	memcpy(&ipaddr, &sa.sin_addr, sizeof(ipaddr));
	return r;
}

bool Utility::u2ip(const std::string& host, struct sockaddr_in& sa, int ai_flags)
{
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = ai_flags;
	hints.ai_family = AF_INET;
	hints.ai_socktype = 0;
	hints.ai_protocol = 0;
	struct addrinfo* res;
	if (Utility::isipv4(host)) {
		hints.ai_flags |= AI_NUMERICHOST;
	}
	int n = getaddrinfo(host.c_str(), nullptr, &hints, &res);
	// n为0 则成功 / 反之则代表失败
	if (!n) {
		std::vector<struct addrinfo*> vec;
		struct addrinfo* ai = res;
		while (ai) {
			if (ai -> ai_addrlen == sizeof(sa)) {
				vec.push_back(ai);
			}
			ai = ai -> ai_next;
		}
		if (!vec.size()) {
			return false;
		}
		ai = vec[Utility::Rnd() % vec.size()];
		memcpy(&sa, ai -> ai_addr, ai -> ai_addrlen);
		freeaddrinfo(res);
		return true;
	}
	std::string error = "Error: ";
	return false;
}

bool Utility::reverse(struct sockaddr *sa, socklen_t sa_len, std::string& hostname, int flags)
{
	std::string service;
	return Utility::reverse(sa, sa_len, hostname, service, flags);
}

bool Utility::reverse(struct sockaddr *sa, socklen_t sa_len, std::string& hostname, std::string& service, int flags)
{
	hostname = "";

	char host[NI_MAXHOST];
	int n = getnameinfo(sa, sa_len, host, sizeof(host), nullptr, 0, flags);

	// 如果传回非0 则代表 getnameinfo failed
	if (n) {
		return false;
	}
	hostname = host;
	return true;
}

std::string Utility::l2string(long l)
{
	std::string str;
	char tmp[100];
	snprintf(tmp,sizeof(tmp),"%ld",l);
	str = tmp;
	return str;
}

void Utility::Sleep(int ms)
{
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	select(0, NULL, NULL, NULL, &tv);
}

void Utility::GetTime(struct timeval* p)
{
	gettimeofday(p, NULL);
}

unsigned long Utility::Rnd()
{
static	Utility::Rng generator( (unsigned long)time(NULL) );
	return generator.Get();
}

Utility::Rng::Rng(unsigned long seed) : m_value( 0 )
{
	m_tmp[0] = seed & 0xffffffffUL;
	for (int i = 1; i < TWIST_LEN; ++i)
	{
		m_tmp[i] = (1812433253UL * (m_tmp[i - 1] ^ (m_tmp[i - 1] >> 30)) + i);
	}
}

unsigned long Utility::Rng::Get()
{
	unsigned long val = m_tmp[m_value];
	++m_value;
	if (m_value == TWIST_LEN)
	{
		for (int i = 0; i < TWIST_IB; ++i)
		{
			unsigned long s = TWIST(m_tmp, i, i + 1);
			m_tmp[i] = m_tmp[i + TWIST_IA] ^ (s >> 1) ^ MAGIC_TWIST(s);
		}
		{
			for (int i = 0; i < TWIST_LEN - 1; ++i)
			{
				unsigned long s = TWIST(m_tmp, i, i + 1);
				m_tmp[i] = m_tmp[i - TWIST_IB] ^ (s >> 1) ^ MAGIC_TWIST(s);
			}
		}
		unsigned long s = TWIST(m_tmp, TWIST_LEN - 1, 0);
		m_tmp[TWIST_LEN - 1] = m_tmp[TWIST_IA - 1] ^ (s >> 1) ^ MAGIC_TWIST(s);

		m_value = 0;
	}
	return val;
}

const std::string Utility::Stack()
{
	return "Not available";
}
