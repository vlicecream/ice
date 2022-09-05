/*
 * Auther
 * Date: 2022-09-04
 * Email:
 * Detail: 
 * */

#ifndef __SOCKET_Utility_H__
#define __SOCKET_Utility_H__

#include <map>
#include <ctype.h>
#include <memory>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "SocketConfig.h"
#include "SocketInclude.h"

#define TWIST_LEN 624

class SocketAddress;

/* 转换工具 */
class Utility
{
	class Rng {
	public:
		Rng(unsigned long seed);

		unsigned long Get();
	
	private:
		int m_value;
		unsigned long m_tmp[TWIST_LEN];
	};

	class ncmap_compare {
	public:
		bool operator()(const std::string& x, const std::string& y) const;
	};
public:
	template<typename Y> class ncmap : public std::map<std::string, Y, ncmap_compare> {
	public:
		ncmap() {}
	};

	class Uri {
	public:
		Uri(const std::string& url);

		const std::string& Url() const { return m_url; }
		const std::string& UrlUri() const { return m_uri; }
		const std::string& QueryString() const { return m_query_string; }
		const std::string& Protocol() const { return m_protocol; }
		const std::string& Host() const { return m_host; }
		int Port() const { return m_port; }
		const std::string& Path() const { return m_path; }
		const std::string& File() const { return m_file; }
		const std::string& Extension() const { return m_ext; }

		const std::string& ToString() const { return m_url; }

		const std::string& User() const { return m_user; }
		const std::string& Auth() const { return m_auth; }
	private:
		std::string m_url;
		std::string m_uri;
		std::string m_query_string;
		std::string m_protocol;
		std::string m_host;
		int m_port;
		std::string m_path;
		std::string m_file;
		std::string m_ext;
		std::string m_user;
		std::string m_auth;
	};

	class Path
	{
	public:
		Path(const std::string& );

		const std::string& GetPath() const { return m_path; }
		const std::string& GetFilename() const { return m_file; }
		const std::string& GetExtension() const { return m_ext; }

	private:
		std::string m_path;
		std::string m_file;
		std::string m_ext;
	};

public:
	static std::string base64(const std::string& str_in);
	static std::string base64d(const std::string& str_in);
	static std::string l2string(long l);
	static std::string bigint2string(int64_t l);
	static std::string bigint2string(uint64_t l);
	static uint64_t atoi64(const std::string& str);
	static unsigned int hex2unsigned(const std::string& str);
	static std::string rfc1738_encode(const std::string& src);
	static std::string rfc1738_decode(const std::string& src);

	/** Checks whether a string is a valid ipv4/ipv6 ip number. */
	static bool isipv4(const std::string&);
	/** Checks whether a string is a valid ipv4/ipv6 ip number. */
	static bool isipv6(const std::string&);

	/** Hostname to ip resolution ipv4, not asynchronous. */
	static bool u2ip(const std::string&, ipaddr_t&);
	static bool u2ip(const std::string&, struct sockaddr_in& sa, int ai_flags = 0);


	/** Reverse lookup of address to hostname */
	static bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string&, int flags = 0);
	static bool reverse(struct sockaddr *sa, socklen_t sa_len, std::string& hostname, std::string& service, int flags = 0);

	static bool u2service(const std::string& name, int& service, int ai_flags = 0);

	/** Convert binary ip address to string: ipv4. */
	static void l2ip(const ipaddr_t,std::string& );
	static void l2ip(const in_addr&,std::string& );

	/** ResolveLocal (hostname) - call once before calling any GetLocal method. */
	static void ResolveLocal();
	/** Returns local hostname, ResolveLocal must be called once before using.
		\sa ResolveLocal */
	static const std::string& GetLocalHostname();
	/** Returns local ip, ResolveLocal must be called once before using.
		\sa ResolveLocal */
	static ipaddr_t GetLocalIP();
	/** Returns local ip number as string.
		\sa ResolveLocal */
	static const std::string& GetLocalAddress();

	/** Get environment variable */
	static const std::string GetEnv(const std::string& name);
	/** Set environment variable.
		\param var Name of variable to set
		\param value Value */
	static void SetEnv(const std::string& var,const std::string& value);
	/** Convert sockaddr struct to human readable string.
		\param sa Ptr to sockaddr struct */
	static std::string Sa2String(struct sockaddr *sa);

	/** Get current time in sec/microseconds. */
	static void GetTime(struct timeval *);

	static std::unique_ptr<SocketAddress> CreateAddress(struct sockaddr *,socklen_t);

	static unsigned long ThreadID();

	static std::string ToLower(const std::string& str);
	static std::string ToUpper(const std::string& str);

	static std::string ToString(double d);

	/** Returns a random 32-bit integer */
	static unsigned long Rnd();

	static const char *Logo;

	static const std::string Stack();

	/** Utf8 decrypt, encrypt. */
	static const std::string FromUtf8(const std::string& );
	static const std::string ToUtf8(const std::string&);

	/** File system stuff */
	static const Path CurrentDirectory();
	static bool ChangeDirectory(const Path& to_dir);

	/** wait a specified number of ms */
	static void Sleep(int ms);

private:
	static std::string m_host; ///< local hostname
	static ipaddr_t m_ip; ///< local ip address
	static std::string m_addr; ///< local ip address in string 
	static bool m_local_resolved; ///< ResolveLocal has been called if true

};

#endif
