#pragma once

#include "StackWalker.h"

#include <winnt.h>


class MyStackWalker : public StackWalker
{
	int m_SkipFrames;
protected:
	MyStackWalker()
	{
		m_SkipFrames = 0;
	}
	virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName) {}
	virtual void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion) {}
	std::string GetLastPathNameComponent(const std::string& pathName)
	{
		return GetLastPathNameComponent(pathName.c_str(), pathName.size());
	}

	const char* GetLastPathNameComponent(const char* path, size_t length)
	{
		for (size_t i = 0; i < length; i++)
		{
			if (path[length - i - 1] == '\\')
				return path + length - i;
		}
		return path;
	}

	virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry)
	{
		if (m_SkipFrames > 0)
		{
			m_SkipFrames--;
			return;
		}
		if ((eType != lastEntry) && (entry.offset != 0))
		{
			const char* moduleName = entry.moduleName[0] == 0 ? "(<unknown>)" : entry.moduleName;
			char buffer[1024 * 10];
			if (entry.lineFileName[0] != 0)
			{
				snprintf(buffer, 1024 * 10, "0x%p (%s) [%s:%d] %s \n",
					reinterpret_cast<LPVOID>(entry.offset), moduleName,
					GetLastPathNameComponent(entry.lineFileName).c_str(),entry.lineNumber, entry.undFullName);
			}
			else
			{
				snprintf(buffer, 1024 * 10,  "0x%p (%s) %s\n", 
					reinterpret_cast<LPVOID>(entry.offset), moduleName, 
					entry.undFullName);
			}
				
			OnOutput(buffer);
		}
	}

	void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr) override {}
public:
	void SkipFrames(int skipFrames)
	{
		m_SkipFrames = skipFrames;
	}
};

class StringStackWalker : public MyStackWalker
{
public:
	StringStackWalker() : result(NULL) { }

	void SetOutputString(std::string &_result) { result = &_result; }
	void ClearOutputString() { result = NULL; }
protected:
	std::string *result;

	virtual void OnOutput(LPCSTR szTest)
	{
		if (result)
			*result += szTest;
	}
};

class BufferStackWalker : public MyStackWalker
{
public:
	BufferStackWalker() : maxSize(0), buffer(NULL) { }

	void SetOutputBuffer(char *_buffer, int _maxSize)
	{
		buffer = _buffer;
		maxSize = _maxSize;
	}
	void ClearOutputBuffer() { buffer = NULL; }
protected:
	char *buffer;
	int maxSize;

	virtual void OnOutput(LPCSTR szTest)
	{
		if (buffer == NULL)
			return;

		while (maxSize > 1 && *szTest != '\0')
		{
			*(buffer++) = *(szTest++);
			maxSize--;
		}
		*buffer = '\0';
	}
};