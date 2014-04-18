//------------------------------------------------------------------------------
// File: main.c                                                (c) 2012 jewe.org
//------------------------------------------------------------------------------
//
// JILRUNONLY LICENSE AGREEMENT
//
// This license is based on the zlib/libpng license.
// If the license text from the file "COPYING" should
// differ from this license text, then this text shall
// prevail.
//
// Copyright (c) 2005-2011 jewe.org
//
// This software is provided 'as-is', without any express
// or implied warranty. In no event will the authors be
// held liable for any damages arising from the use of this
// software.
//
// Permission is granted to anyone to use this software for
// any purpose, including commercial applications, and to
// alter it and redistribute it freely, subject to the
// following restrictions:
//
//   1. The origin of this software must not be
//   misrepresented; you must not claim that you
//   wrote the original software. If you use this
//   software in a product, an acknowledgment in
//   the product documentation would be
//   appreciated but is not required.
//
//   2. Altered source versions must be plainly
//   marked as such, and must not be
//   misrepresented as being the original
//   software.
//
//   3. This notice may not be removed or altered
//   from any source distribution.
//
// Description:
// ------------
//  This very simple command-line application demonstrates, how to use the
//  JILRuntime/JewelScript library in a program. The following things are
//  demonstrated:
//
//  1) How to initialize the runtime
//  2) How to register native types to the runtime
//  3) How to load and compile a script file (from command line parameter)
//  4) How to call a script function, pass it a parameter and obtain a result
//	5) How to terminate the virtual machine
//
// Compiling this project:
// -----------------------
//  In order to successfully compile this on other platforms / compilers, you
//  will need to set up a project for these files. The output type for the
//	project should be an executable for the OS'es native console.
//  Required include pathes for this project are:
//      "../externals/jilruntime/include"
//      "../externals/jilruntime/src"
//		"../externals/ansi"
//		"../externals/trex"
//
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
// JIL includes
//------------------------------------------------------------------------------

#include "jilapi.h"
#include "jilcompilerapi.h"
#include "jilarray.h"
#include "jilstring.h"
#include "jilcodelist.h"
#include "jildebug.h"

//------------------------------------------------------------------------------
// include native types (these are only intended for demonstration)
//------------------------------------------------------------------------------

#include "ntl_stdlib.h"
#include "ntl_math.h"
#include "ntl_file.h"
#include "ntl_trex.h"
#include "ntl_time.h"

//------------------------------------------------------------------------------
// version
//------------------------------------------------------------------------------

#define	VERSION		"0.3.1.61"

//------------------------------------------------------------------------------
// THROW
//------------------------------------------------------------------------------
// macro for error handling - to avoid obscuring the code by repeative error
// handling blocks.

#define THROW(CONDITION,ERR,TEXT) \
	if (CONDITION) { err = OnError(pMachine,ERR,TEXT); goto cleanup; }

//------------------------------------------------------------------------------
// virtual machine callbacks
//------------------------------------------------------------------------------

// log message handler
static void			CBOutputLogMessage		(JILState*, const char*);

// exception handlers
static void			CBMachineException		(JILState*);
static void			CBBreakException		(JILState*);

//------------------------------------------------------------------------------
// helper functions in this file
//------------------------------------------------------------------------------

static int			OnError					(JILState* pMachine, int e, const char* alt);
static JILHandle*	CreateParameterArray	(JILState* pMachine, int nArgs, char** ppArgList);
static int			PrintVersionInfo		(JILState* pMachine);
static int			SortArgList				(char** ppArgList, int argc, char* argv[], int nFile, int noSort);
static void			GetAppPath				(const char* pFilespec);
static void			WaitForEnter			();
static int			LoadBinary				(JILState* pMachine, const char* pFileName);
static int			SaveBinary				(JILState* pMachine, const char* pFileName);

//------------------------------------------------------------------------------
// static variables and constants
//------------------------------------------------------------------------------

static char gAppPath[1024];
static char gDefaultPath[1024];
static char gCompilerOptions[1024];
static char gExtension[32];
static char gBinaryName[1024];

// The initial stack size the runtime is going to use (can be increased by script if neccessary)
const long kStackSize = 1024;

// Usage info when specifying an unknown option
const char* kUsageString =
	"Usage: jilrunonly [options] <file> [<para1> <para2> ...]\n"
	"<file>       JewelScript source file to compile and run\n"
	"<para>       optional parameters to be passed to the scripts main function\n"
	"-e <string>  specify script file extension to assume (default: jc)\n"
	"-l           output virtual assembler listing of compiled code\n"
	"-o <string>  specify compiler options (enclose in quotes)\n"
	"-rb          read <file> as a compiled binary program\n"
	"-v           output version info\n"
	"-w           wait for enter\n"
	"-wb <file>   write a compiled binary program using the given filename\n"
	"-x           exit without running the script\n"
	"-bind        generate C++ binding code\n"
	"-doc         generate HTML documentation\n";

// The path where new files are created (for option -bind and -doc)
const char* kFileOutputDir = ".";

// This forward declares our main entry point function
const char* kForwardDeclareMain =
	"function string main(const string[] args);" TAG("This is the main entry point function for any script executed by the jilrun command line application. Implement this function in your script. Any command line arguments will be passed as a string array in 'args'.");

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
// just doing my job.

int main(int argc, char* argv[])
{
	JILState* pMachine = NULL;
	JILHandle* hParameterArray = NULL;
	JILHandle* hResult = NULL;
	JILHandle* hFunctionMain = NULL;
	JILHandle* hException = NULL;
	const JILChar* pStr;
	char** ppArgList = NULL;
	int bListCode = 0;
	int bWait = 0;
	int bExit = 0;
	int bVers = 0;
	int bOptions = 0;
	int bSetExt = 0;
	int bReadBinary = 0;
	int bWriteBinary = 0;
	int bGenerateDoc = 0;
	int bGenerateBind = 0;
	int nFile = 1;
	int nArgs = 0;
	int err = 0;

	strcpy(gAppPath, "");
	strcpy(gDefaultPath, "");
	strcpy(gCompilerOptions, "");
	strcpy(gExtension, ".jc");
	// we need at least 1 parameter
	if( argc < 2 )
	{
		printf(	kUsageString );
		return 0;
	}
	// check for options
	while( argv[nFile][0] == '-' )
	{
		if( strcmp(argv[nFile], "-e") == 0 )
		{
			if( ++nFile == argc ) break;
			strcpy(gExtension + 1, argv[nFile]);
			bSetExt = 1;
		}
		else if( strcmp(argv[nFile], "-o") == 0 )
		{
			if( ++nFile == argc ) break;
			strcpy(gCompilerOptions, argv[nFile]);
			bOptions = 1;
		}
		else if( strcmp(argv[nFile], "-wb") == 0 )
		{
			if( ++nFile == argc ) break;
			strcpy(gBinaryName, argv[nFile]);
			bWriteBinary = 1;
		}
		else if( strcmp(argv[nFile], "-rb") == 0 )
			bReadBinary = 1;
		else if( strcmp(argv[nFile], "-l") == 0 )
			bListCode = 1;
		else if( strcmp(argv[nFile], "-v") == 0 )
			bVers = 1;
		else if( strcmp(argv[nFile], "-w") == 0 )
			bWait = 1;
		else if( strcmp(argv[nFile], "-x") == 0 )
			bExit = 1;
		else if( strcmp(argv[nFile], "-bind") == 0 )
			bGenerateBind = 1;
		else if( strcmp(argv[nFile], "-doc") == 0 )
			bGenerateDoc = 1;
		else
		{
			printf(	kUsageString );
			return 0;
		}
		if( ++nFile == argc ) break;
	}
	if( bSetExt )
	{
		if( bOptions )
			strcat(gCompilerOptions, ",");
		strcat(gCompilerOptions, "file-ext=");
		strcat(gCompilerOptions, gExtension + 1);
		bOptions = 1;
	}
	// get the path to this application
	GetAppPath( argv[0] );
	// allocate a new argument list
	ppArgList = (char**) malloc(argc * sizeof(char*));
	memset(ppArgList, 0, argc * sizeof(char*));
	// sort command line arguments
	nArgs = SortArgList(ppArgList, argc, argv, nFile, bReadBinary);

	/*
	 *	1) initialize the JIL virtual machine
	 *	-------------------------------------
	 */

	pMachine = JILInitialize( kStackSize, gCompilerOptions );
	THROW( !pMachine, -1, "The JIL virtual machine could not be initialized!" )

	// print version info, if requested
	if( bVers )
		PrintVersionInfo( pMachine );

	// leave if no file specified
	if( ppArgList[0] == NULL )
		goto cleanup;

	// install a log message handler (this is optional)
	// runtime and compiler will use this to output errors, warnings and other information
	JILSetLogCallback(pMachine, CBOutputLogMessage);

	// install exception handlers (optional)
	// for simplicity, we only catch 2 of the 4 possible types of exceptions
	err = JILSetExceptionVector(pMachine, JIL_Machine_Exception_Vector, CBMachineException);
	THROW( err, err, NULL )

	err = JILSetExceptionVector(pMachine, JIL_Break_Exception_Vector, CBBreakException);
	THROW( err, err, NULL )

	// Enable fixed memory management
	err = JILUseFixedMemDynamic(pMachine);
	THROW( err, err, NULL )

	/*
	 *	2) register our native types
	 *	----------------------------
	 */

	err = JILRegisterNativeType( pMachine, StdLibProc );
	THROW( err, err, NULL )

	err = JILRegisterNativeType( pMachine, MathProc );
	THROW( err, err, NULL )

	err = JILRegisterNativeType( pMachine, FileProc );
	THROW( err, err, NULL )

	err = JILRegisterNativeType( pMachine, TrexProc );
	THROW( err, err, NULL )

	err = JILRegisterNativeType( pMachine, TimeProc );
	THROW( err, err, NULL )

	if( !bReadBinary )
	{
		/*
		 *	3) try to load and compile the specified source file
		 *	----------------------------------------------------
		 */

		// compile defaults
		err = JCLCompile(pMachine, "default", kForwardDeclareMain);
		THROW( err, err, "" )

		// load and compile specified script file
		err = JCLLoadAndCompile(pMachine, ppArgList[0]);
		THROW( err, err, "" )

		// link...
		err = JCLLink(pMachine);
		THROW( err, err, "" )

		// generate bindings or html documentation?
		if( bGenerateBind )
		{
			err = JCLGenerateBindings(pMachine, kFileOutputDir);
			THROW( err, err, NULL );
		}
		if( bGenerateDoc )
		{
			err = JCLGenerateDocs(pMachine, kFileOutputDir, "application=JILRunOnly, @ignore=runtime" );
			THROW( err, err, NULL );
		}

		// optionally, to save some memory, we can free the compiler
		err = JCLFreeCompiler(pMachine);
		THROW( err, err, "The JewelScript compiler could not be freed!" )
	}
	else
	{
		err = LoadBinary(pMachine, ppArgList[0]);
		THROW( err, -1, "The specified binary file could not be loaded!" )
	}

	// save binary, if requested
	if( bWriteBinary )
	{
		err = SaveBinary(pMachine, gBinaryName);
		THROW( err, -1, "The specified binary file could not be written!" )
	}

	// list code, if requested
	if( bListCode )
		JILListCode(pMachine, 0, 0, 1, stdout);

	// exit without running, if requested
	if( bExit )
		goto cleanup;

	/*
	 *	5a) before calling anything else, we should run the init-code generated by the compiler
	 */

	err = JILRunInitCode(pMachine);
	THROW( err, err, NULL )

	/*
	 *	5b) call function 'main'
	 *	------------------------
	 */

	// get handle to global function "main"
	hFunctionMain = JILGetFunction(pMachine, NULL, NULL, "main");
	THROW( !hFunctionMain, -1, "Script does not define the entry-point function 'main'!" )

	// create an array out of the command line parameters
	hParameterArray = CreateParameterArray(pMachine, nArgs, ppArgList);
	THROW( !hParameterArray, -1, "Could not create parameter array!" )

	// call the function
	hResult = JILCallFunction(pMachine, hFunctionMain, 1, kArgHandle, hParameterArray);

	// check if the result is an error
	err = NTLHandleToError(pMachine, hResult);
	if( err == JIL_No_Exception )
	{
		// no exception - print the result to console
		pStr = NTLHandleToString(pMachine, hResult);
		if( pStr )
			fprintf(stdout, "%s\n", pStr);
	}
	else
	{
		// obtain the message string from the exception
		hException = NTLHandleToErrorMessage(pMachine, hResult);
		pStr = NTLHandleToString(pMachine, hException);
		if( pStr )
			fprintf(stdout, "%s\n    Error:   %d\n    Message: %s\n", NTLGetTypeName(pMachine, NTLHandleToTypeID(pMachine, hResult)), err, pStr);
	}

	/*
	 *	6) release handles we obtained and terminate virtual machine
	 *	------------------------------------------------------------
	 */

	// release the exception string handle
	NTLFreeHandle(pMachine, hException);
	hException = NULL;

	// release the result handle
	NTLFreeHandle(pMachine, hResult);
	hResult = NULL;

	// release our parameter array
	NTLFreeHandle(pMachine, hParameterArray);
	hParameterArray = NULL;

	// release the function handle
	NTLFreeHandle(pMachine, hFunctionMain);
	hFunctionMain = NULL;

	// terminate virtual machine
	err = JILTerminate(pMachine);
	pMachine = NULL;
	THROW( err, err, "The virtual machine could not be terminated!" )

	free( ppArgList );
	ppArgList = NULL;

	if( bWait)
		WaitForEnter();

	return 0;

cleanup:
	/*
	 *	We get here by the THROW macro, if an error has occurred
	 */
	if( pMachine )
	{
		NTLFreeHandle(pMachine, hParameterArray);
		NTLFreeHandle(pMachine, hResult);
		NTLFreeHandle(pMachine, hFunctionMain);
		JILTerminate(pMachine);
	}
	if( ppArgList )
		free( ppArgList );

	if( bWait)
		WaitForEnter();

	return err;
}

//------------------------------------------------------------------------------
// CBOutputLogMessage
//------------------------------------------------------------------------------
// Callback function the virtual machine uses to output log messages

static void CBOutputLogMessage(JILState* pMachine, const char* pString)
{
	fprintf(stdout, pString);
}

//------------------------------------------------------------------------------
// CBBreakException
//------------------------------------------------------------------------------
// Handle an exception generated by the "brk" instruction. This instruction can
// be generated in JewelScript using the __brk keyword; it is intended to help
// debugging the code.
// An exception handler can choose whether the VM should continue to run
// after the handler returns, or whether execution should be aborted.
// If the exception handler returns without clearing the exception state
// of the VM, then code execution is aborted and an error is returned.
// To clear the exception state, the handler can call ClearExceptionState();

static void CBBreakException(JILState* pState)
{
	char str[128];

	printf( "\nJIL BREAK EXCEPTION AT %d: %d %s\n",
		pState->errProgramCounter,
		pState->errException,
		JILGetExceptionString(pState, pState->errException)
		);

	JILListInstruction( pState, pState->errProgramCounter, str, 128, 1 );
	printf( "%s\n", str );
	printf( "\nContinue execution? (Y/N) " );
	scanf( "%s", str );
	if( strcmp(str, "Y") == 0 || strcmp(str, "y") == 0 )
		JILClearExceptionState(pState);
}

//------------------------------------------------------------------------------
// CBMachineException
//------------------------------------------------------------------------------
// Handle an exception generated by the virtual machine due to a runtime error.
// NOTE that extended runtime checks are normally disabled in release builds of
// the JILRuntime library (for performance reasons). So even if you manage to
// produce JewelScript code that would cause a machine exception, it's likely
// that the release build of the virtual machine will not detect this error and
// therefore not invoke the exception handler.
// However, the simplest way to force a runtime error is to generate a division
// by zero exception: (This is detected even with runtime checks disabled.)
//		float f = 0.0;
//		float g = 3 / f;

static void CBMachineException(JILState* pState)
{
	char str[128];

	printf( "\nJIL MACHINE EXCEPTION AT %d: %d %s\n",
		pState->errProgramCounter,
		pState->errException,
		JILGetExceptionString(pState, pState->errException)
		);

	JILListInstruction( pState, pState->errProgramCounter, str, 128, 1 );
	printf( "%s\n\n", str );

	printf( "Tracing back last 10 functions on callstack:\n" );
	JILListCallStack( pState, 10, stdout );
}

//------------------------------------------------------------------------------
// OnError
//------------------------------------------------------------------------------
// Handle an error.

static int OnError( JILState* pMachine, int e, const char* alt )
{
	if( alt )
	{
		printf( "%s\n", alt );
	}
	else
	{
		if( e > JIL_No_Exception && e < JIL_Unknown_Exception )
		{
			printf( "Error: %d %s\n", e, JILGetExceptionString(pMachine, (JILError) e) );
		}
		else
		{
			printf( "Error: %d\n", e );
		}
	}
	return e;
}

//------------------------------------------------------------------------------
// CreateParameterArray
//------------------------------------------------------------------------------
// Create a string array out of the command line parameters.

static JILHandle* CreateParameterArray(JILState* pMachine, int nArgs, char** ppArgList)
{
	JILArray* pArray;
	JILString* pString;
	JILHandle* hString;
	JILHandle* hArray = NULL;
	int i;

	// allocate a new array
	pArray = JILArray_New(pMachine);
	if( pArray )
	{
		for( i = 0; i < nArgs; i++ )
		{
			// allocate a JILString
			pString = JILString_New(pMachine);
			// assign the command line parameter
			JILString_Assign( pString, ppArgList[i] );
			// create a handle for the JILString
			hString = NTLNewHandleForObject( pMachine, type_string, pString );
			// move the handle into our array (append)
			JILArray_ArrMove( pArray, hString );
			// we are done with the JILString handle, release it!
			NTLFreeHandle( pMachine, hString );
		}
		// create a handle for the JILArray
		hArray = NTLNewHandleForObject( pMachine, type_array, pArray );
	}
	return hArray;
}

//------------------------------------------------------------------------------
// PrintVersionInfo
//------------------------------------------------------------------------------
// Print version info about this program and the embedded JILRuntime library to
// the console.

static int PrintVersionInfo(JILState* pMachine)
{
	const JILVersionInfo* pInfo;
	JILChar pVersion[16];

	// get version info
	pInfo = JILGetRuntimeVersion( pMachine );

	// print version info
	printf( "Program version:        " VERSION "\n\n");
	printf( "Library version:        %s\n", JILGetVersionString(pInfo->LibraryVersion, pVersion));
	printf( "Runtime version:        %s\n", JILGetVersionString(pInfo->RuntimeVersion, pVersion));
	printf( "Compiler version:       %s\n", JILGetVersionString(pInfo->CompilerVersion, pVersion));
	printf( "Type interface version: %s\n", JILGetVersionString(pInfo->TypeInterfaceVersion, pVersion));

	printf( "VM build flags:\n" );
	if( pInfo->BuildFlags & kDebugBuild )
		printf( "- Is a debug build\n" );
	else
		printf( "- Is a release build\n" );
	if( pInfo->BuildFlags & kTraceExceptionEnabled )
		printf( "- Supports trace exception\n" );
	else
		printf( "- Does not support trace exception\n" );
	if( pInfo->BuildFlags & kExtendedRuntimeChecks )
		printf( "- Performs extended runtime checks\n\n" );
	else
		printf( "- Extended runtime checks are disabled\n\n" );

	return 0;
}

//------------------------------------------------------------------------------
// SortArgList
//------------------------------------------------------------------------------
// Sort the command line argument list so that the first file with a ".jc"
// extension is sorted to the begin of the argument list. This will allow users
// to run this application from the desktop environment, instead of the
// terminal / command prompt. When multiple files, including a script file,
// are dragged on the application icon, the actual order of the files in the
// argument list is not always defined (depending on the OS you are on). The
// script file might appear in the middle or at the end of the argument list.
// Therefore this function will search for any file with the extension ".jc"
// in the argument list and move it to the start of the list.
// If no such file is found at all, the function will insert "default.jc"
// at the beginning of the list. This allows users to have a default script
// together with this application in a folder, which is executed when the user
// drags files onto the application.

static int SortArgList(char** ppArgList, int argc, char* argv[], int nFile, int noSort)
{
	int nArgs = 0;
	int i, j;
	char* pArg, *pos;

	// try to find a file that has the extension ".jc"
	for( i = nFile; i < argc; i++ )
	{
		pArg = argv[i];
		if( pArg )
		{
			pos = strrchr(pArg, '.');
			if( pos && strcmp(pos, gExtension) == 0 )
				break;
		}
	}
	// if i == nFile just copy arguments
	if( i == nFile || noSort )
	{
		for( j = nFile; j < argc; j++ )
			ppArgList[nArgs++] = argv[j];
	}
	else if( i >= argc )
	{
		// no script file found, insert default
		strcpy(gDefaultPath, gAppPath);
		strcat(gDefaultPath, "default");
		strcat(gDefaultPath, gExtension);
		ppArgList[nArgs++] = gDefaultPath;
		// copy arguments
		for( j = nFile; j < argc; j++ )
			ppArgList[nArgs++] = argv[j];
	}
	else
	{
		// found a script file in the middle
		ppArgList[nArgs++] = argv[i];
		// copy arguments
		for( j = nFile; j < argc; j++ )
		{
			if( j != i )
				ppArgList[nArgs++] = argv[j];
		}
	}
	return nArgs;
}

//------------------------------------------------------------------------------
// GetAppPath
//------------------------------------------------------------------------------
// Get the path to this application for later use.

static void GetAppPath(const char* pFilespec)
{
	char* pos;
	size_t lpos;
	gAppPath[0] = 0;
	pos = strrchr(pFilespec, JIL_PATHSEPARATOR);
	if( pos )
	{
		lpos = (size_t) (pos - pFilespec);
		strncpy(gAppPath, pFilespec, lpos + 1);
		gAppPath[lpos + 1] = 0;
	}
}

//------------------------------------------------------------------------------
// WaitForEnter
//------------------------------------------------------------------------------

static void WaitForEnter()
{
	char string[16];
	fgets(string, 15, stdin);
}

//------------------------------------------------------------------------------
// LoadBinary
//------------------------------------------------------------------------------
// Load a JIL program from a binary file.

static int LoadBinary(JILState* pMachine, const char* pFileName)
{
	int err = JIL_ERR_Load_Chunk_Failed;
	FILE* stream = NULL;
	char* pData = NULL;
	size_t size;

	// open the file
	stream = fopen(pFileName, "rb");
	if( stream == NULL )
		goto error;

	// get file size in bytes
	if( fseek(stream, 0, SEEK_END) )
		goto error;
	size = ftell(stream);
	if( size <= 0 )
		goto error;
	if( fseek(stream, 0, SEEK_SET) )
		goto error;

	// load the data into a buffer
	pData = (char*) malloc( size );
	if( pData == NULL )
		goto error;
	if( fread(pData, size, 1, stream) != 1 )
		goto error;

	// finally, load the data into the virtual machine
	err = JILLoadBinary(pMachine, pData, size);

error:
	if( stream )
		fclose( stream );
	if( pData )
		free( pData );
	return err;
}

//------------------------------------------------------------------------------
// SaveBinary
//------------------------------------------------------------------------------
// Save a JIL program as a binary file.

static int SaveBinary(JILState* pMachine, const char* pFileName)
{
	int err = JIL_ERR_Save_Chunk_Failed;
	FILE* stream = NULL;
	void* pData = NULL;
	int size;

	// open the file
	stream = fopen(pFileName, "wb");
	if( stream == NULL )
		goto error;

	// get the data from the virtual machine
	err = JILSaveBinary(pMachine, &pData, &size);
	if( err )
		goto error;

	// write it
	if( fwrite(pData, size, 1, stream) != 1 )
		err = JIL_ERR_Save_Chunk_Failed;

error:
	if( stream )
		fclose( stream );
	return err;
}

