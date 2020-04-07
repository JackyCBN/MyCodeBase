#ifndef __Core_QSAssert_H__
#define __Core_QSAssert_H__
#include <Windows.h>
#define VARARGS     __cdecl					/* Functions with variable arguments */
#define CALLSTACK_IGNOREDEPTH 2
#ifndef QWORD
typedef unsigned __int64	QWORD;		// 64-bit unsigned.
#endif

#define VF_DISPLAY_BASIC		0x00000000
#define VF_DISPLAY_FILENAME		0x00000001
#define VF_DISPLAY_MODULE		0x00000002
#define VF_DISPLAY_ALL			0xffffffff

#ifndef _LINUX_VERSION_
void  QSCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, CONTEXT* Context );

int  QSGetVarArgsAnsi( char* Dest, SIZE_T DestSize, int Count, const char*& Fmt, va_list ArgPtr );

/** Failed assertion handler.  Warning: May be called at library startup time. */
void   QSFailAssertFunc( const char* Expr, const char* File, int Line, const char* Format="", ... );
void   QSFailAssertFunc2(CONTEXT* Context, const char* Expr, const char* File, int Line, const char* Format = "", ...);

/** Failed assertion handler.  This version only calls QSOutputDebugString. */
bool   QSFailAssertFuncDebug( const char* Expr, const char* File, int Line, const char* Format="", ... );

/** Breaks into the debugger.  Forces a GPF in non-debug builds. */
void   QSDebugBreak();

bool  QSInitStackWalking();
/**
 * Walks the stack and QSends the human readable string to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	HumanReadableString	String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	IgnoreCount			Number of stack entries to ignore (some are guaranteed to be in the stack walking code)
 * @param	Context				Optional thread context information
 */ 
void  QSStackWalkAndDump( char* HumanReadableString, SIZE_T HumanReadableStringSize, INT IgnoreCount, CONTEXT* Context = nullptr );

/**
 * Converts the passed in program counter address to a human readable string and QSends it to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @param	HumanReadableString		String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
 * @return	TRUE if the symbol was found, otherwise FALSE
 */ 
BOOL QSProgramCounterToHumanReadableString( QWORD ProgramCounter, char* HumanReadableString, SIZE_T HumanReadableStringSize, DWORD VerbosityFlags = VF_DISPLAY_ALL );



//define in UE3
#define GET_VARARGS(msg,msgsize,len,lastarg,fmt) { va_list ap; va_start(ap,lastarg);QSGetVarArgs(msg,msgsize,len,fmt,ap); }
#define GET_VARARGS_ANSI(msg,msgsize,len,lastarg,fmt) { va_list ap; va_start(ap,lastarg);QSGetVarArgsAnsi(msg,msgsize,len,fmt,ap); }
#define GET_VARARGS_RESULT(msg,msgsize,len,lastarg,fmt,result) { va_list ap; va_start(ap,lastarg); result = QSGetVarArgs(msg,msgsize,len,fmt,ap); }
#define GET_VARARGS_RESULT_ANSI(msg,msgsize,len,lastarg,fmt,result) { va_list ap; va_start(ap,lastarg); result = QSGetVarArgsAnsi(msg,msgsize,len,fmt,ap); }
#define ARRAY_COUNT( array ) \
	( sizeof(array) / sizeof((array)[0]) )
#endif

#ifndef _SHIPPING
#ifndef _LINUX_VERSION_
	#pragma warning( push )
#endif
	#define QSFailAssert(expr,file,line,...)	{ if(QSFailAssertFuncDebug(expr, file, line, ##__VA_ARGS__)) {QSDebugBreak();} QSFailAssertFunc(expr, file, line, ##__VA_ARGS__); }

	#define QSASSERT(expr) QSCheck(expr)
	
	#define QSCheckCode( Code )		do { Code } while ( false );
	#define QSCheckMsg(expr,msg)	{ if(!(expr)) {QSFailAssert( #expr " : " #msg , __FILE__, __LINE__ ); }  }
    #define QSCheckFunc(expr,func)	{ if(!(expr)) {func; QSFailAssert( #expr, __FILE__, __LINE__ ); }  }
	#define QSVerify(expr)			{ if(!(expr)) QSFailAssert( #expr, __FILE__, __LINE__ ); }
	#define QSCheck(expr)			{ if(!(expr)) QSFailAssert( #expr, __FILE__, __LINE__ ); }

	/* if assert expr, return.
	 * add by joewanchen, 8/21/2011 
	 */
	#define QSASSERT_RT(expr)		{bool bFailed = !(expr); QSASSERT(!bFailed); if (bFailed) return;			}
	#define QSASSERT_RV(expr, val)	{bool bFailed = !(expr); QSASSERT(!bFailed); if (bFailed) return (val);		}
	#define QSASSERT_R0(expr)		QSASSERT_RV(expr, 0)
	#define QSASSERT_RF(expr)		QSASSERT_RV(expr, false)
	#define QSASSERT_RN(expr)		QSASSERT_RV(expr, NULL)

	/**
	 * Denotes codepaths that should never be reached.
	 */
	#define QSCheckNoEntry()       { QSFailAssert( "Enclosing block should never be called", __FILE__, __LINE__ ); }

	/**
	 * Denotes codepaths that should not be executed more than once.
	 */
	#define QSCheckNoReentry()     { static bool s_beenHere##__LINE__ = false;                                         \
	                               QSCheckMsg( !s_beenHere##__LINE__, Enclosing block was called more than once );   \
								   s_beenHere##__LINE__ = true; }

#ifndef _LINUX_VERSION_
	#pragma warning(disable : 4512)
#endif
	class FRecursionScopeMarker
	{
	public: 
		FRecursionScopeMarker(WORD &InCounter) : Counter( InCounter ) { ++Counter; }
		~FRecursionScopeMarker() { --Counter; }
		WORD& Counter;

	};


	/**
	 * Denotes codepaths that should never be called recursively.
	 */
	#define QSCheckNoRecursion()  static WORD RecursionCounter##__LINE__ = 0;                                            \
	                            QSCheckMsg( RecursionCounter##__LINE__ == 0, Enclosing block was entered recursively );  \
	                            const FRecursionScopeMarker ScopeMarker##__LINE__( RecursionCounter##__LINE__ )


	#define QSCheckAtCompileTime(expr, msg)		typedef char COMPILE_ERROR_##msg[1][(expr)]

	#define QSVerifyf(expr, ...)				{ if(!(expr)) QSFailAssert( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); }
	#define QSCheckf(expr, ...)				{ if(!(expr)) QSFailAssert( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); }
#ifndef _LINUX_VERSION_
	#pragma warning( pop )
#endif
#else
	#define QSASSERT(x) 			   {}
	#define QSASSERT_RT(x) 			   {}
	#define QSASSERT_RV(expr, val)     {}
	#define QSASSERT_R0(expr)          {}
	#define QSASSERT_RF(expr)		   {}
	#define QSASSERT_RN(expr)		   {}
	#define QSCheckCode(Code)          {}
	#define QSCheck(expr)              {}
	#define QSCheckMsg(expr,msg)       {}
	#define QSCheckFunc(expr,func)     {}    
	#define QSCheckNoEntry()           {}
	#define QSCheckNoReentry()         {}
	#define QScheckNoRecursion()       {}
	#define QScheckAtCompileTime(expr) {}
	#define QSCheckf(expr, ...)						{  }	
	#define QSVerify(expr)						{ if(!(expr)){} }
	#define QSVerifyf(expr, ...)				{ if(!(expr)){} }

#endif

#endif//__Core_QSAssert_H__