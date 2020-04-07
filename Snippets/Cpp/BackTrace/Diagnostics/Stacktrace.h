#ifndef STACKTRACE_H
#define STACKTRACE_H

#include <string>
#include <vector>

std::string GetStacktrace(int skipFrames = 3);

void GetStacktrace(char *trace, int maxSize, int maxFrames = INT_MAX);


//'lazy' Stacktrace options
//It's possible to save the current stacktrace and resolve it at a different time.

struct SavedStacktrace
{
	std::vector<void*> frames;
	uint32_t hash;

	SavedStacktrace() : hash(0)	{}

	SavedStacktrace(int capacity) :
		hash(0)
	{
		frames.reserve(capacity);
	}
};

uint32_t GetStacktracetNativeOption(void**trace, int bufferSize, int startFrame, bool fastNativeOnly);

inline uint32_t GetStacktrace(void**trace, int bufferSize, int startFrame)
{ 
	return GetStacktracetNativeOption (trace, bufferSize, startFrame, false);
}

void GetStacktrace(SavedStacktrace& trace, int maxFrames = 1024, int startFrame = 0);

std::string GetReadableStackTrace(const SavedStacktrace& trace);
void GetReadableStackTrace(char* output, int bufsize, void* const* input, int frameCount);

void InitializeStackWalker();
void CleanupStackWalker();

#endif
