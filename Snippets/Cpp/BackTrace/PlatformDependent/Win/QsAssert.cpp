#include "QSAssert.h"
#include <windows.h>
#include <Psapi.h>
#include <DbgHelp.h>
#include <map>
#include <stdio.h>
#include <stdarg.h>
//#include "QSMemoryDebugTrace.h"


BOOL					GIsCriticalError				= true;	

int QSGetVarArgsAnsi( char* Dest, SIZE_T DestSize ,int Count, const char*& Fmt, va_list ArgPtr)
{
	int Result = vsnprintf( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}
void  QSOutputDebugStringf( const char *Format, ... )
{
	char TempStr[4096];
	GET_VARARGS_ANSI( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
	OutputDebugStringA( TempStr );
}

/*-----------------------------------------------------------------------------
	Stack walking.
-----------------------------------------------------------------------------*/

typedef BOOL  (WINAPI *TFEnumProcesses)( DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL  (WINAPI *TFEnumProcessModules)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *TFGetModuleBaseName)(HANDLE hProcess, HMODULE hModule, LPSTR lpBaseName, DWORD nSize);
typedef DWORD (WINAPI *TFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
typedef BOOL  (WINAPI *TFGetModuleInformation)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);

static TFEnumProcesses			FEnumProcesses;
static TFEnumProcessModules		FEnumProcessModules;
static TFGetModuleBaseName		FGetModuleBaseName;
static TFGetModuleFileNameEx	FGetModuleFileNameEx;
static TFGetModuleInformation	FGetModuleInformation;


void* QSGetDllHandle( const char* Filename )
{
	QSCheck(Filename);	
	return LoadLibraryA(Filename);
}
//
// Free a DLL.
//
void QSFreeDllHandle( void* DllHandle )
{
	QSCheck(DllHandle);
	FreeLibrary( (HMODULE)DllHandle );
}

//
// Lookup the address of a DLL function.
//
void* QSGetDllExport( void* DllHandle, const char* ProcName )
{
	QSCheck(DllHandle);
	QSCheck(ProcName);
	return (void*)GetProcAddress( (HMODULE)DllHandle, ProcName );
}

/**
 * Loads modules for current process.
 */ 
static void LoadProcessModules()
{
	INT			ErrorCode = 0;	
	HANDLE		ProcessHandle = GetCurrentProcess(); 
	const INT	MAX_MOD_HANDLES = 1024;
	HMODULE		ModuleHandleArray[MAX_MOD_HANDLES];
	HMODULE*	ModuleHandlePointer = ModuleHandleArray;
	DWORD		BytesRequired;
	MODULEINFO	ModuleInfo;

	// Enumerate process modules.
	BOOL bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), &BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		ErrorCode = GetLastError();
		return;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	bool bNeedToFreeModuleHandlePointer = FALSE;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = TRUE;
		ModuleHandlePointer = (HMODULE*) malloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), &BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	INT	ModuleCount = BytesRequired / sizeof( HMODULE );

	// Load the modules.
	for( INT ModuleIndex=0; ModuleIndex<ModuleCount; ModuleIndex++ )
	{
		char ModuleName[1024];
		char ImageName[1024];
		FGetModuleInformation( ProcessHandle, ModuleHandleArray[ModuleIndex], &ModuleInfo,sizeof( ModuleInfo ) );
		FGetModuleFileNameEx( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, 1024 );
		FGetModuleBaseName( ProcessHandle, ModuleHandleArray[ModuleIndex], ModuleName, 1024 );

		// Set the search path to find PDBs in the same folder as the DLL.
		char SearchPath[1024];
		char* FileName = NULL;
		GetFullPathNameA( ImageName, 1024, SearchPath, &FileName );
		*FileName = 0;
		SymSetSearchPath( GetCurrentProcess(), SearchPath );

		// Load module.
		DWORD64 BaseAddress = SymLoadModule64( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, ModuleName, (DWORD64) ModuleInfo.lpBaseOfDll, (DWORD) ModuleInfo.SizeOfImage );
		if( !BaseAddress )
		{
			ErrorCode = GetLastError();
		}
	} 

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		free( ModuleHandlePointer );
	}
}

/**
 * Helper function performing the actual stack walk. This code relies on the symbols being loaded for best results
 * walking the stack albeit at a significant performance penalty.
 *
 * This helper function is designed to be called within a structured exception handler.
 *
 * @param	BackTrace			Array to write backtrace to
 * @param	MaxDepth			Maxium depth to walk - needs to be less than or equal to array size
 * @param	Context				Thread context information
 * @return	EXCEPTION_EXECUTE_HANDLER
 */
static INT CaptureStackTraceHelper( QWORD *BackTrace, DWORD MaxDepth, CONTEXT* Context )
{
	STACKFRAME64		StackFrame64;
	HANDLE				ProcessHandle;
	HANDLE				ThreadHandle;
	unsigned long		LastError;
	BOOL				bStackWalkSucceeded	= TRUE;
	DWORD				CurrentDepth		= 0;
	DWORD				MachineType			= IMAGE_FILE_MACHINE_I386;
	CONTEXT				ContextCopy = *Context;

	__try
	{
		// Get context, process and thread information.
		ProcessHandle	= GetCurrentProcess();
		ThreadHandle	= GetCurrentThread();

		// Zero out stack frame.
		memset( &StackFrame64, 0, sizeof(StackFrame64) );

		// Initialize the STACKFRAME structure.
		StackFrame64.AddrPC.Mode         = AddrModeFlat;
		StackFrame64.AddrStack.Mode      = AddrModeFlat;
		StackFrame64.AddrFrame.Mode      = AddrModeFlat;
#ifdef _WIN64
		StackFrame64.AddrPC.Offset       = Context->Rip;
		StackFrame64.AddrStack.Offset    = Context->Rsp;
		StackFrame64.AddrFrame.Offset    = Context->Rbp;
		MachineType                      = IMAGE_FILE_MACHINE_AMD64;
#else
		StackFrame64.AddrPC.Offset       = Context->Eip;
		StackFrame64.AddrStack.Offset    = Context->Esp;
		StackFrame64.AddrFrame.Offset    = Context->Ebp;
#endif

		// Walk the stack one frame at a time.
		while( bStackWalkSucceeded && (CurrentDepth < MaxDepth) )
		{
			bStackWalkSucceeded = StackWalk64(  MachineType, 
												ProcessHandle, 
												ThreadHandle, 
												&StackFrame64,
												&ContextCopy,
												NULL,
												SymFunctionTableAccess64,
												SymGetModuleBase64,
												NULL );

			BackTrace[CurrentDepth++] = StackFrame64.AddrPC.Offset;

			if( !bStackWalkSucceeded  )
			{
				// StackWalk failed! give up.
				LastError = GetLastError( );
				break;
			}

			// Stop if the frame pointer or address is NULL.
			if( StackFrame64.AddrFrame.Offset == 0 || StackFrame64.AddrPC.Offset == 0 )
			{
				break;
			}
		}
	} 
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		// We need to catch any exceptions within this function so they don't get sent to 
		// the engine's error handler, hence causing an infinite loop.
		return EXCEPTION_EXECUTE_HANDLER;
	} 

	// NULL out remaining entries.
	for ( ; CurrentDepth<MaxDepth; CurrentDepth++ )
	{
		BackTrace[CurrentDepth] = NULL;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

/**
 * Initializes the symbol engine if needed.
 */ 
bool GStackWalkingInitialized = false;
bool QSInitStackWalking()
{
	// Only initialize once.
	if( !GStackWalkingInitialized )
	{
		void* DllHandle = QSGetDllHandle( ("PSAPI.DLL") );
		if( DllHandle == NULL )
		{
			return FALSE;
		}

		// Load dynamically linked PSAPI routines.
		FEnumProcesses			= (TFEnumProcesses)			QSGetDllExport( DllHandle,("EnumProcesses"));
		FEnumProcessModules		= (TFEnumProcessModules)	QSGetDllExport( DllHandle,("EnumProcessModules"));
		FGetModuleFileNameEx	= (TFGetModuleFileNameEx)	QSGetDllExport( DllHandle,("GetModuleFileNameExA"));
		FGetModuleBaseName		= (TFGetModuleBaseName)		QSGetDllExport( DllHandle,("GetModuleBaseNameA"));
		FGetModuleInformation	= (TFGetModuleInformation)	QSGetDllExport( DllHandle,("GetModuleInformation"));

		// Abort if we can't look up the functions.
		if( !FEnumProcesses || !FEnumProcessModules || !FGetModuleFileNameEx || !FGetModuleBaseName || !FGetModuleInformation )
		{
			return FALSE;
		}

		// Set up the symbol engine.
		DWORD SymOpts = SymGetOptions();
		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_DEBUG;
		SymOpts |= SYMOPT_UNDNAME;
		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_FAIL_CRITICAL_ERRORS;
		SymOpts |= SYMOPT_DEFERRED_LOADS;
		SymOpts |= SYMOPT_ALLOW_ABSOLUTE_SYMBOLS;
		SymOpts |= SYMOPT_EXACT_SYMBOLS;
		SymOpts |= SYMOPT_CASE_INSENSITIVE;
		SymSetOptions ( SymOpts );

		// Initialize the symbol engine.
		SymInitialize ( GetCurrentProcess(), NULL, TRUE );
		LoadProcessModules();

		GStackWalkingInitialized = TRUE;
	}
	return GStackWalkingInitialized;
}


NTSYSAPI WORD NTAPI RtlCaptureStackBackTrace(
	__in DWORD FramesToSkip,
	__in DWORD FramesToCapture,
	__out_ecount(FramesToCapture) PVOID *BackTrace,
	__out_opt PDWORD BackTraceHash
	);

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 */


#pragma optimize("",off) 
void QSCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, CONTEXT* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

	if( Context )
	{
		CaptureStackTraceHelper( BackTrace, MaxDepth, Context );
	}
	else
	{

#if 0
		PVOID WinBackTrace[MAX_CALLSTACK_DEPTH];
		USHORT NumFrames = RtlCaptureStackBackTrace( 0, 26, WinBackTrace, NULL );
		for ( USHORT FrameIndex=0; FrameIndex < NumFrames; ++FrameIndex )
		{
			BackTrace[ FrameIndex ] = (QWORD) WinBackTrace[ FrameIndex ];
		}
		while ( NumFrames < MaxDepth )
		{
			BackTrace[ NumFrames++ ] = NULL;
		}
	
	
#elif defined(_WIN64)
		// Raise an exception so CaptureStackBackTraceHelper has access to context record.
		__try
		{
			RaiseException(	0,			// QSlication-defined exception code.
							0,			// Zero indicates continuable exception.
							0,			// Number of arguments in args array (ignored if args is NULL)
							NULL );		// Array of arguments
			}
		// Capture the back trace.
		__except( CaptureStackTraceHelper( BackTrace, MaxDepth, (GetExceptionInformation())->ContextRecord ) )
		{
		}
#else
		// Use a bit of inline assembly to capture the information relevant to stack walking which is
		// basically EIP and EBP.
		CONTEXT HelperContext;
		memset( &HelperContext, 0, sizeof(CONTEXT) );
		HelperContext.ContextFlags = CONTEXT_FULL;

		// Use a fake function call to pop the return address and retrieve EIP.

		__asm
		{
			call FakeFunctionCall
		FakeFunctionCall: 
			pop eax
			mov HelperContext.Eip, eax
			mov HelperContext.Ebp, ebp
			mov HelperContext.Esp, esp
		}

		// Capture the back trace.
		CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
#endif
	}
}
#pragma optimize("",on) 
/**
 * Converts the passed in program counter address to a human readable string and QSends it to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @param	HumanReadableString		String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	VerbosityFlags			Bit field of requested data for output.
 * @return	TRUE if the symbol was found, otherwise FALSE
 */ 
BOOL QSProgramCounterToHumanReadableString( QWORD ProgramCounter, char* HumanReadableString, SIZE_T HumanReadableStringSize, DWORD VerbosityFlags )
{
	char			SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 512];
	PIMAGEHLP_SYMBOL64	Symbol;
	DWORD				SymbolDisplacement		= 0;
	DWORD64				SymbolDisplacement64	= 0;
	DWORD				LastError;
	BOOL				bSymbolFound = FALSE;

	HANDLE				ProcessHandle = GetCurrentProcess();

	// Initialize stack walking as it loads up symbol information which we require.
	QSInitStackWalking();

	// Initialize symbol.
	Symbol					= (PIMAGEHLP_SYMBOL64) SymbolBuffer;
	Symbol->SizeOfStruct	= sizeof(SymbolBuffer);
	Symbol->MaxNameLength	= 512;

	// Get symbol from address.
	if( SymGetSymFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement64, Symbol ) )
	{
		char			FunctionName[1024];

		// Skip any funky chars in the beginning of a function name.
		INT Offset = 0;
		while( Symbol->Name[Offset] < 32 || Symbol->Name[Offset] > 127 )
		{
			Offset++;
		}

		// Write out function name if there is sufficient space.
		sprintf( FunctionName,  ("%s() "), (const char*)(Symbol->Name + Offset) );
		//QSStrcatANSI( HumanReadableString, HumanReadableStringSize, FunctionName );
		strcat( HumanReadableString, FunctionName );
		bSymbolFound = TRUE;
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError( );
	}

	if( VerbosityFlags & VF_DISPLAY_FILENAME )
	{
		IMAGEHLP_LINE64		ImageHelpLine;
		char			FileNameLine[1024];

		// Get Line from address
		ImageHelpLine.SizeOfStruct = sizeof( ImageHelpLine );
		if( SymGetLineFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement, &ImageHelpLine) )
		{
			sprintf( FileNameLine, ("0x%-8x + %d bytes [File=%s:%d] "), (DWORD) ProgramCounter, SymbolDisplacement, (const char*)(ImageHelpLine.FileName), ImageHelpLine.LineNumber );
		}
		else    
		{
			// No line number found.  Print out the logical address instead.
			sprintf( FileNameLine, "Address = 0x%-8x (filename not found) ", (DWORD) ProgramCounter );
		}
		strcat( HumanReadableString, FileNameLine );
	}

	if( VerbosityFlags & VF_DISPLAY_MODULE )
	{
		IMAGEHLP_MODULE64	ImageHelpModule;
		char			ModuleName[1024];

		// Get module information from address.
		ImageHelpModule.SizeOfStruct = sizeof( ImageHelpModule );
		if( SymGetModuleInfo64( ProcessHandle, ProgramCounter, &ImageHelpModule) )
		{
			// Write out Module information if there is sufficient space.
			sprintf( ModuleName, "[in %s]", (const char*)(ImageHelpModule.ImageName) );
			strcat( HumanReadableString, ModuleName );
		}
		else
		{
			LastError = GetLastError( );
		}
	}

	return bSymbolFound;
}

/**
 * Walks the stack and QSends the human readable string to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	HumanReadableString	String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	IgnoreCount			Number of stack entries to ignore (some are guaranteed to be in the stack walking code)
 * @param	Context				Optional thread context information
 */ 
void QSStackWalkAndDump( char* HumanReadableString, SIZE_T HumanReadableStringSize, INT IgnoreCount, CONTEXT* Context )
{	
	// Initialize stack walking... loads up symbol information.
	QSInitStackWalking();

	// Temporary memory holding the stack trace.
	#define MAX_DEPTH 100
	DWORD64 StackTrace[MAX_DEPTH];
	memset( StackTrace, 0, sizeof(StackTrace) );

	// Capture stack backtrace.
	QSCaptureStackBackTrace( StackTrace, MAX_DEPTH, Context );

	// Skip the first two entries as they are inside the stack walking code.
	INT CurrentDepth = IgnoreCount;
	// Allow the first entry to be NULL as the crash could have been caused by a call to a NULL function pointer,
	// which would mean the top of the callstack is NULL.
	while( StackTrace[CurrentDepth] || ( CurrentDepth == IgnoreCount ) )
	{
		QSProgramCounterToHumanReadableString( StackTrace[CurrentDepth], HumanReadableString, HumanReadableStringSize );
		strcat( HumanReadableString, "\r\n" );
		CurrentDepth++;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// Failed assertion handler.
//warning: May be called at library startup time.
void   QSFailAssertFunc( const char* Expr, const char* File, int Line, const char* Format/*=TEXT("")*/, ... )
{
	// Ignore this assert if we're already forcibly shutting down because of a critical error.
	// Note that QSFailAssertFuncDebug() is still called.
	if ( GIsCriticalError )
	{
		char TempStr[4096];
		GET_VARARGS_ANSI( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
		const SIZE_T StackTraceSize = 65535;
		char* StackTrace = (char*) malloc( StackTraceSize );
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory.
		QSStackWalkAndDump( StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH );
		
		//QSOutputDebugStringf( TEXT("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: %s"), Expr, File, Line, TempStr, StackTrace );

		char MessageBoxBuf[ 65535 ];
		sprintf( MessageBoxBuf,  ("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: %s"), Expr, File, Line, TempStr, StackTrace );
	//	if ( IsDebuggerPresent()) //we need pop message box anycase, chaofan's requirement for easy trace problem
		{
			::MessageBoxA( NULL,MessageBoxBuf,"ERROR",MB_OK);
		}
		free( StackTrace );
		//if (GIsCriticalError )
		//{
		//	INT* Dummy	= NULL;
		//	*Dummy		= 0;
		//}

	}
}

void   QSFailAssertFunc2(CONTEXT* Context, const char* Expr, const char* File, int Line, const char* Format/*=TEXT("")*/, ...)
{
	// Ignore this assert if we're already forcibly shutting down because of a critical error.
	// Note that QSFailAssertFuncDebug() is still called.
	if (GIsCriticalError)
	{
		char TempStr[4096];
		GET_VARARGS_ANSI(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Format, Format);
		const SIZE_T StackTraceSize = 65535;
		char* StackTrace = (char*)malloc(StackTraceSize);
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory.
		QSStackWalkAndDump(StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH, Context);

		//QSOutputDebugStringf( TEXT("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: %s"), Expr, File, Line, TempStr, StackTrace );

		char MessageBoxBuf[65535];
		sprintf(MessageBoxBuf, ("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: %s"), Expr, File, Line, TempStr, StackTrace);
		//	if ( IsDebuggerPresent()) //we need pop message box anycase, chaofan's requirement for easy trace problem
		{
			::MessageBoxA(NULL, MessageBoxBuf, "ERROR", MB_OK);
		}
		free(StackTrace);
		//if (GIsCriticalError )
		//{
		//	INT* Dummy	= NULL;
		//	*Dummy		= 0;
		//}

	}
}

/////////////////////////////////////////////////////////////////////////////////////
bool   QSFailAssertFuncDebug( const char* Expr, const char* File, int Line, const char* Format/*=TEXT("")*/, ... )
{
	static std::map<const char*, std::map<int, bool> > SuppressMap;
	if(!SuppressMap.count(File) || !SuppressMap[File].count(Line))
		SuppressMap[File][Line] = false;

	char TempStr[4096];
	GET_VARARGS_ANSI( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );

	char TempStr2[4096];
	sprintf_s(TempStr2, ("%s(%i): Assertion failed: %s\n%s\n"), File, Line, Expr, TempStr);
	QSOutputDebugStringf( TempStr2 );
//	if ( IsDebuggerPresent()) //we need pop message box anycase, chaofan's requirement for easy trace problem
	if(!SuppressMap[File][Line])
	{
		SuppressMap[File][Line] = ::MessageBoxA( NULL,(std::string(TempStr2) + "\nPress Cancel to suppress further popup.").c_str(),"ERROR",MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL;
	}

	return !SuppressMap[File][Line];
}

//
// Breaks into the debugger.  Force a GPF in non-debug builds.
//
void  QSDebugBreak()
{

	if( IsDebuggerPresent() )
	{
		//always use break, still be able to go on
//#ifdef _DEBUG
 		DebugBreak();
//#else
//		// We rather force a GPF as release builds tend to have their callstack clobbered by DebugBreak.
//		INT* Dummy	= NULL;
//		*Dummy		= 0;
//#endif
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////