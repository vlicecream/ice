/*
 * Auther: 
 * Date: 2022-09-05
 * Email: vlicecream@163.com
 * Detail: Encapsulate file operations / File Interface
 * */

#ifndef __SOCKET_IFile_H__
#define __SOCKET_IFile_H__

#include <string>

class IFile
{
public:
	virtual ~IFile() {}

	virtual bool fopen(const std::string&, const std::string&) = 0;
	virtual void fclose() const = 0;

	virtual size_t fread(char *, size_t, size_t) const = 0;
	virtual size_t fwrite(const char *, size_t, size_t) = 0;

	virtual char *fgets(char *, int) const = 0;
	virtual void fprintf(const char *format, ...) = 0;

	virtual off_t size() const = 0;
	virtual bool eof() const = 0;

	virtual void reset_read() const = 0;
	virtual void reset_write() = 0;

	virtual const std::string& Path() const = 0;
};

#endif // __SOCKET_IFile_H__
