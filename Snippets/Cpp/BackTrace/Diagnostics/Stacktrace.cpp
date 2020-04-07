#include "../../stdafx.h"
#include "Stacktrace.h"
#include "../PlatformDependent/Win/StackWalkerImpl.h"

StringStackWalker* g_StringStackWalker = nullptr;
BufferStackWalker* g_BufferStackWalker = nullptr;

void InitializeStackWalker(){
	if (g_BufferStackWalker == nullptr){
		g_BufferStackWalker = new BufferStackWalker();
	}
	if (g_StringStackWalker == nullptr){
		g_StringStackWalker = new StringStackWalker();
	}
}

void CleanupStackWalker(){
	if(g_BufferStackWalker!=nullptr)
	{
		delete g_BufferStackWalker;
		g_BufferStackWalker = nullptr;
	}

	if (g_StringStackWalker != nullptr)
	{
		delete g_StringStackWalker;
		g_StringStackWalker = nullptr;
	}
}

std::string GetStacktrace(int skipFrames)
{
	std::string trace;
	InitializeStackWalker();
	g_StringStackWalker->SetOutputString(trace);
	g_StringStackWalker->SkipFrames(skipFrames);
	g_StringStackWalker->ShowCallstack();
	g_StringStackWalker->ClearOutputString();
	return trace;
}

uint32_t GetStacktracetNativeOption (void**trace, int bufferSize, int startFrame, bool fastNativeOnly)
{
	int framecount = 0;
	int maxFrames = bufferSize-1;
	std::vector<uint8_t> vec(sizeof(void*)*(bufferSize + startFrame));
	void** tempTrace = nullptr;
	reinterpret_cast<void*&>(tempTrace) = vec.data();

	if (fastNativeOnly)
	{
		framecount = RtlCaptureStackBackTrace(0, maxFrames + startFrame, tempTrace, NULL);
	}
	else
	{
		InitializeStackWalker();
		g_BufferStackWalker->GetCurrentCallstack(tempTrace, maxFrames + startFrame, framecount);
	}

	uint32_t hash = 0;
	int i = 0;
	for(; i < framecount - startFrame; i++)
	{
		trace[i] = tempTrace[i+startFrame];
		hash ^= reinterpret_cast<intptr_t>(trace[i]) ^ (hash << 7) ^ (hash >> 21);
	}
	trace[i] = nullptr;
	return hash;
}

void GetStacktrace(SavedStacktrace& trace, int maxFrames, int startFrame)
{
	std::vector<uint8_t> vec(sizeof(void*)*(maxFrames));
	void** tempStack = nullptr;
	reinterpret_cast<void*&>(tempStack) = vec.data();

	trace.hash = GetStacktrace(tempStack, maxFrames, startFrame);

	size_t framecount = 0;
	while (tempStack[framecount])
	{
		framecount++;
	}

	trace.frames.reserve(framecount);
	for (int i = startFrame; i < framecount; i++)
	{
		trace.frames.push_back(tempStack[i]);
	}

}

std::string GetReadableStackTrace(const SavedStacktrace& trace)
{
	std::string stacktraceOutput;

	InitializeStackWalker();
	g_StringStackWalker->SetOutputString(stacktraceOutput);
	g_StringStackWalker->GetStringFromStacktrace(trace.frames.data(), trace.frames.size ());
	g_StringStackWalker->ClearOutputString();
	return stacktraceOutput;
}

void GetReadableStackTrace(char* output, int bufsize, void* const* input, int frameCount)
{
	InitializeStackWalker();
	g_BufferStackWalker->SetOutputBuffer(output, bufsize);
	g_BufferStackWalker->GetStringFromStacktrace(const_cast<void**>(input), frameCount);
	g_BufferStackWalker->ClearOutputBuffer();
}

void GetStacktrace(char *trace, int maxSize, int maxFrames)
{
	InitializeStackWalker();
	g_BufferStackWalker->SetOutputBuffer(trace, maxSize);
	g_BufferStackWalker->ShowCallstack(
		GetCurrentThread(),
		nullptr,
		maxFrames);
	g_BufferStackWalker->ClearOutputBuffer();
}
