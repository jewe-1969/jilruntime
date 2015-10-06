//------------------------------------------------------------------------------
// File: JILApiTypes.h                                    (c) 2005-2010 jewe.org
//------------------------------------------------------------------------------
//
// DISCLAIMER:
// -----------
//	THIS SOFTWARE IS SUBJECT TO THE LICENSE AGREEMENT FOUND IN "jilapi.h" AND
//	"COPYING". BY USING THIS SOFTWARE YOU IMPLICITLY DECLARE YOUR AGREEMENT TO
//	THE TERMS OF THIS LICENSE.
//
// Description:
// ------------
/// @file jilapitypes.h
/// All type definitions used by the JIL virtual machine.
///
/// This file defines all the types developers using the JIL virtual machine
/// will need to include. This file is automatically included when including
/// file "jilapi.h".
//------------------------------------------------------------------------------

#ifndef JILAPITYPES_H
#define JILAPITYPES_H

#include "jilplatform.h"
#include "jilversion.h"

//------------------------------------------------------------------------------
// struct forward declarations
//------------------------------------------------------------------------------

typedef struct JILState				JILState;
typedef struct JILContext			JILContext;
typedef struct JILHandle			JILHandle;
typedef struct JILTypeInfo			JILTypeInfo;
typedef struct JILTypeListItem		JILTypeListItem;
typedef struct JILVersionInfo		JILVersionInfo;
typedef struct JILSymTabEntry		JILSymTabEntry;
typedef struct JILFixMem			JILFixMem;
typedef struct JCLState				JCLState;
typedef struct NTLInstance			NTLInstance;
typedef struct JILStackFrame		JILStackFrame;
typedef struct JILTable				JILTable;
typedef struct JILMemStats			JILMemStats;
typedef struct JILFunctionTable		JILFunctionTable;
typedef struct JILGCEventRecord		JILGCEventRecord;
typedef struct JILFileHandle		JILFileHandle;

typedef struct Seg_JILDataHandle	Seg_JILDataHandle;
typedef struct Seg_JILLong			Seg_JILLong;
typedef struct Seg_JILFuncInfo		Seg_JILFuncInfo;

//------------------------------------------------------------------------------
// type definitions
//------------------------------------------------------------------------------

#if JIL_MACHINE_NO_64_BIT
typedef unsigned int		JILUInt64;
typedef float				JILFloat;
#else
typedef unsigned long long	JILUInt64;
typedef double				JILFloat;
#endif

typedef unsigned int		JILUInt32;
typedef unsigned short		JILUInt16;
typedef unsigned char		JILByte;

typedef int					JILError;
typedef int					JILLong;
typedef char				JILChar;
typedef int					JILBool;
typedef void				JILUnknown;

//------------------------------------------------------------------------------
// JIL exception vector numbers
//------------------------------------------------------------------------------
/// Enumerates the exception vectors that can be set to a callback when calling
/// JILSetExceptionVector().

typedef enum
{
	JIL_Machine_Exception_Vector = 0,	//!< Specify this value to set a C/C++ handler for (fatal) machine exceptions.
	JIL_Software_Exception_Vector,		//!< Specify this value to set a C/C++ handler for software (bytecode) generated exceptions.
	JIL_Trace_Exception_Vector,			//!< Specify this value to set a C/C++ handler for trace exceptions.
	JIL_Break_Exception_Vector			//!< Specify this value to set a C/C++ handler for break exceptions, generated by the brk instruction.

} JILExceptionVector;

//------------------------------------------------------------------------------
// Enum values for JILGetObject() / JILAttachObject()
//------------------------------------------------------------------------------
/// Pass one of these values to JILGetObject() or JILAttachObject() to attach
/// or retrieve a pointer associated with the virtual machine runtime.

typedef enum
{
	JIL_UserData = 0,					//!< Reserved for the "user" of the library, has no predefined meaning
	JIL_Application,					//!< Specify this to attach your application object to the runtime
	JIL_LogHandler,						//!< Specify this to attach an object for log output handling to the runtime
	JIL_Debugger,						//!< Reserved for the byte-code debugger, do not use

	kUserDataSize						//!< The total number of pointers the virtual machine can store

} JILAttachObjectID;

//------------------------------------------------------------------------------
// Enum values for JILCallFunction()
//------------------------------------------------------------------------------
/// Pass one of these values to JILCallFunction() to define the type of data
/// you wish to pass as an argument.

enum
{
	kArgInt = 0,						//!< The argument / result is a native int value
	kArgFloat,							//!< The argument / result is a native float value
	kArgString,							//!< The argument / result is a native char pointer
	kArgHandle							//!< The argument / result is a JILHandle pointer
};

//------------------------------------------------------------------------------
// JIL handle types
//------------------------------------------------------------------------------
/// Enumerates the predefined type identifier numbers the virtual machine uses.
/// These enum constants are used in the JILHandle::type member and identify
/// the built-in type of a value the virtual machine deals with. User defined
/// types are all type identifiers >= kNumPredefTypes.
/// <p>Do not change these type identifier numbers unless you know exactly what
/// you are doing!</p>
/// @see JILHandle, JILTypeInfo

typedef enum
{
	// predefined type identifiers
	type_null			= 0,		//!< Type ID of type 'null'
	type_var			= 1,		//!< Type ID used for typeless variables
	type_int			= 2,		//!< Type ID of an int value (C++ int32)
	type_float			= 3,		//!< Type ID of a float value (C++ double)
	type_global			= 4,		//!< Type ID of the global class, which is the root of all following types
	type_string			= 5,		//!< Type ID of the built-in string class
	// reserved			= 6,		//   Type ID used by internal 'string::match' class
	// reserved			= 7,		//   Type ID used by delegate defined in string class
	type_array			= 8,		//!< Type ID of the built-in array class
	// reserved			= 9,		//   Type ID used by delegate defined in array class
	// reserved			= 10,		//   Type ID used by delegate defined in array class
	// reserved			= 11,		//   Type ID used by delegate defined in array class
	type_list			= 12,		//!< Type ID of the built-in list class
	// reserved			= 13,		//   Type ID used by delegate defined in list class
	type_iterator		= 14,		//!< Type ID of the built-in iterator class
	type_arraylist		= 15,		//!< Type ID of the built-in arraylist class
	type_table			= 16,		//!< Type ID of the built-in table class
	// reserved			= 17,		//   Type ID used by delegate defined in table class
	type_exception		= 18,		//!< Type ID of the exception interface
	type_rt_exception	= 19,		//!< Type ID of the runtime_exception class
	type_delegate		= 20,		//!< Generic delegate type ID used by the API, user defined delegates have their own type ID

	// additional constants
	kNumPredefTypes				//!< This and every following value is a type ID for a user defined type

} JILHandleType;

//------------------------------------------------------------------------------
// JILTypeFamily
//------------------------------------------------------------------------------
/// This enum defines values for the NTLGetTypeFamily() function.

typedef enum
{
	tf_undefined = 0,			//!< Type family is not defined
	tf_integral,				//!< Type is an integral type (int or float)
	tf_class,					//!< Type is a class (native or script)
	tf_interface,				//!< Type is an interface
	tf_thread,					//!< Type is a cooperative thread (cofunction)
	tf_delegate,				//!< Type is a delegate (reference to a function or method)

} JILTypeFamily;

//------------------------------------------------------------------------------
// notification messages used by the garbage collector
//------------------------------------------------------------------------------

enum
{
	JIL_GCEvent_Mark = 0,		//!< Sent to the event handler proc before the GC collects unused objects. Call NTLMarkHandle() for all your objects.
	JIL_GCEvent_Shutdown		//!< Sent to the event handler proc when the VM is about to shut down. Call NTLFreeHandle() for all your objects.
};

//------------------------------------------------------------------------------
// modes sent to callback JILFileInputProc
//------------------------------------------------------------------------------

enum
{
	JILFileInputOpen = 0,		//!< Sent to JILFileInputProc to open a file, pBuffer points to file name.
	JILFileInputRead,			//!< Sent to JILFileInputProc to read the given amount of bytes to pBuffer.
	JILFileInputSeek,			//!< Sent to JILFileInputProc to seek to the given position in the file. If not applicable in your custom file proc, return JIL_ERR_File_Generic.
	JILFileInputLength,			//!< Sent to JILFileInputProc to determine the length of the file in bytes. If not applicable in your custom file proc, return 0.
	JILFileInputClose,			//!< Sent to JILFileInputProc to close the file.
	JILFileInputGetCwd			//!< Sent to JILFileInputProc to retrieve the current working directory. If not applicable in your custom file proc, return empty string.
};

//------------------------------------------------------------------------------
// modes for HTML documentation generator
//------------------------------------------------------------------------------

enum
{
	JIL_GenDocs_User = 0,		//!< Generate documentation for user classes only (not built-in classes)
	JIL_GenDocs_BuiltIn,		//!< Generate documentation for built-in classes only (not user classes)
	JIL_GenDocs_All				//!< Generate documentation for all classes
};

//------------------------------------------------------------------------------
// misc constants
//------------------------------------------------------------------------------

enum
{
	JILFalse			= 0,
	JILTrue				= 1,
	kNumRegisters		= 32,		//!< Critical minimum: 4, recommended minimum: 8, recommended maximum: 256 (there is no upper limit)
	kReturnToNative		= -1,
	kReturnRegister		= 1,		//!< Register r1, DO NOT CHANGE THIS!!!
	kMinimumStackSize	= 128,		//!< Minimum size for the data stack

	// build flags for version info
	kTraceExceptionEnabled	=	1 << 0,		//!< Flag for JILVersionInfo::BuildFlags; VM can generate trace exception
	kExtendedRuntimeChecks	=	1 << 1,		//!< Flag for JILVersionInfo::BuildFlags; VM does extended runtime checks
	kDebugBuild				=	1 << 2,		//!< Flag for JILVersionInfo::BuildFlags; VM is a debug build
	kReleaseBuild			=	1 << 3,		//!< Flag for JILVersionInfo::BuildFlags; VM is a release build

	// segment related
	kInitialSegmentSize	= 0x100,
	kSegmentAllocGrain = 0x100
};

//------------------------------------------------------------------------------
// JILExceptionProc
//------------------------------------------------------------------------------
/// Implement this callback in your application to catch exceptions.

typedef	void (*JILExceptionProc) (JILState*);

//------------------------------------------------------------------------------
// JILLogOutputProc
//------------------------------------------------------------------------------
/// Implement this callback in your application to output or log critical errors
/// reported by the runtime library. @see JILSetLogCallback ()

typedef	void (*JILLogOutputProc) (JILState*, const JILChar*);

//------------------------------------------------------------------------------
// JILGCEventHandler
//------------------------------------------------------------------------------
/// Event handler callback used by garbage collector.

typedef	JILError (*JILGCEventHandler) (JILState*, JILLong, JILUnknown*);

//------------------------------------------------------------------------------
// JILTypeProc
//------------------------------------------------------------------------------
/// This is the prototype of a native types main entry point function, the type proc.

typedef	JILError (*JILTypeProc) (NTLInstance* pInst, JILLong opcode, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

//------------------------------------------------------------------------------
// file input proc
//------------------------------------------------------------------------------
/// This callback can be used to customize how the library reads data from a
/// file. @see JILSetFileInputProc ()

typedef JILError (*JILFileInputProc) (JILState*, JILLong, JILChar*, JILLong, JILUnknown**);

//------------------------------------------------------------------------------
// JCLFatalErrorHandler
//------------------------------------------------------------------------------
/// This callback can be installed by the Application using the compiler API to
/// get notified when the compiler has detected a <b>fatal error</b> from which
/// it cannot recover. This handler will be called to give an Application the
/// opportunity to graciously terminate itself.
/// <p>Be aware that if you let your fatal error handler return normally, there
/// is a good chance that this might result in a crash, because then the
/// compiler will try to graciously return with an error, trying to deallocate
/// all objects, while probably beeing in an inconsistent state due to the
/// fatal error condition. For the same reason, you should <b>not</b> try to
/// un-initialize (terminate) the compiler from within the error handler.
/// Rather accept a memory leak than the probability of the Application
/// crashing when trying to terminate it properly!</p>
/// Typical implementations of a fatal error handler would be either terminating
/// the application after saving all unsaved data and informing the user, or
/// entering an infinite sleep loop in order to save the Application from
/// crashing.
/// @see JCLSetFatalErrorHandler

typedef void (*JCLFatalErrorHandler) (JILState*, const JILChar*);

//------------------------------------------------------------------------------
// malloc / free hooks
//------------------------------------------------------------------------------

typedef JILUnknown* (*JILMallocProc) (JILState*, JILLong); //!< This callback can be used to override or customize the memory allocation behaviour of the runtime. Note that this only affects allocations during <b>runtime</b> of the virtual machine, not allocations made by the compiler or other parts of the library.
typedef void(*JILFreeProc) (JILState*, JILUnknown*); //!< This callback can be used to override or customize the memory deallocation behaviour of the runtime. Note that this only affects deallocations during <b>runtime</b> of the virtual machine, not deallocations made by the compiler or other parts of the library.

//------------------------------------------------------------------------------
// struct JILVersionInfo
//------------------------------------------------------------------------------
/// This is the struct returned by JILGetRuntimeVersion().

struct JILVersionInfo
{
	JILLong				BuildFlags;					//!< See flags in anonymous enum
	JILLong				LibraryVersion;				//!< Integer version number of the whole library. You can use the JILGetVersionString() function to convert this number into a null-terminated string.
	JILLong				RuntimeVersion;				//!< Integer version number of the virtual machine. You can use the JILGetVersionString() function to convert this number into a null-terminated string.
	JILLong				CompilerVersion;			//!< Integer version number of the compiler. You can use the JILGetVersionString() function to convert this number into a null-terminated string.
	JILLong				TypeInterfaceVersion;		//!< Integer version number of the native type interface. You can use the JILGetVersionString() function to convert this number into a null-terminated string.
};

//------------------------------------------------------------------------------
// struct JILContext
//------------------------------------------------------------------------------
/// This struct contains the thread context during byte-code execution.

struct JILContext
{
	JILLong				vmDataStackPointer;			//!< Current data stack pointer
	JILLong				vmCallStackPointer;			//!< Current call stack pointer
	JILLong				vmProgramCounter;			//!< Current program counter
	JILHandle**			vmppRegister;				//!< The bank of virtual machine registers
	JILHandle**			vmppDataStack;				//!< Points to the data stack, this stack consists out of JILHandle pointers
	JILLong*			vmpCallStack;				//!< Points to the call stack, this stack consists out of offsets into the code segment
	JILContext*			vmpYieldContext;			//!< The context that this context will yield to
};

//------------------------------------------------------------------------------
// struct JILFunctionTable
//------------------------------------------------------------------------------
/// Table of handles to functions and methods of a (script-)object. Returned
/// by JILGetFunctionTable().

struct JILFunctionTable
{
	JILLong				size;						//!< Number of functions in the table.
	JILHandle**			func;						//!< Pointer to array of handles
};

//------------------------------------------------------------------------------
// struct JILState
//------------------------------------------------------------------------------
/// This is the main virtual machine object. The JILState object holds all
/// virtual machine relevant data. It must be passed into all library functions
/// that deal with the virtual machine. You should not write to the struct
/// members directly.

struct JILState
{
	// thread context
	JILContext*			vmpContext;					//!< Points to the currently active thread context
	JILContext*			vmpRootContext;				//!< Points to the root context created by JILInitialize

	// error information
	JILError			errException;				//!< Most recent exception number, for informative purposes
	JILLong				errDataStackPointer;		//!< Data stack pointer saved when the most recent exception occurred
	JILLong				errCallStackPointer;		//!< Call stack pointer saved when the most recent exception occurred
	JILLong				errProgramCounter;			//!< Program counter saved when the most recent exception occurred
	JILLong				errHandlesLeaked;			//!< After terminating the virtual machine, contains the number of leaked handles, if any

	// global flags
	JILBool				vmTraceFlag;				//!< Is JILTrue when the VM is currently in TRACE mode
	JILBool				vmExceptionFlag;			//!< Is JILTrue when the VM is currently in exception state
	JILBool				vmInitialized;				//!< Is JILTrue when the VM is fully initialized
	JILBool				vmRunning;					//!< Is JILTrue when the VM is currently executing bytecode
	JILBool				vmBlocked;					//!< Can be set to JILTrue to block all further execution of script code
	JILBool				vmStringPooling;			//!< Can be set to JILFalse to disable string pooling when compiling programs

	JILLong				vmDataStackSize;			//!< The (fixed) size of the data stack, given in JILInitialize()
	JILLong				vmCallStackSize;			//!< The (fixed) size of the call stack, given in JILInitialize()
	JILLong				vmSegmentAllocGrain;		//!< The block size used when resizing the data or code segments
	JILLong				vmCStrSegAllocGrain;		//!< The block size used when resizing the cstr segment
	JILLong				vmInitDataIncr;				//!< Counter for incremental data handle initialization
	JILLong				vmInitTypeIncr;				//!< Counter for incremental native type initialization
	JILLong				vmRunInitIncr;				//!< Code address for incremental init-code execution
	JILLong				vmRunLevel;					//!< Counts the number of nested byte-code execution calls
	JILLong				vmDocGenMode;				//!< Mode value for documentation generator
	JILLong				vmLogGarbageMode;			//!< Mode for the runtime option "log-garbage"
	JILUInt64			vmInstructionCounter;		//!< Incremented for each executed instruction (only if JIL_USE_INSTRUCTION_COUNTER is enabled)
	JILFloat			vmTimeLastGC;				//!< Time (ANSI clocks) when the GC was last executed

	Seg_JILDataHandle*	vmpDataSegment;				//!< Pointer to the data segment
	Seg_JILLong*		vmpCodeSegment;				//!< Pointer to the code segment
	Seg_JILFuncInfo*	vmpFuncSegment;				//!< Pointer to the function info segment

	// cstr segment (raw-data)
	JILLong				vmMaxCStrSegSize;			//!< The currently allocated size of the cstr segment
	JILLong				vmUsedCStrSegSize;			//!< The currently used size of the cstr segment
	JILChar*			vmpCStrSegment;				//!< Pointer to the cstr segment, which is a block of bytes (entries are 4-byte aligned, however)

	// TypeInfo segment
	JILLong				vmMaxTypeInfoSegSize;		//!< The currently allocated size of the TypeInfo segment
	JILLong				vmUsedTypeInfoSegSize;		//!< The currently used size of the TypeInfo segment
	JILTypeInfo*		vmpTypeInfoSegment;			//!< Pointer to the TypeInfo segment, which an array of TypeInfo structs

	// symbol table
	JILSymTabEntry*		vmpSymTabFirst;				//!< Points to the first symbol entry in the symbol table
	JILSymTabEntry*		vmpSymTabLast;				//!< Points to the last symbol entry in the symbol table

	// handles
	JILLong				vmMaxHandles;				//!< The number of currently allocated runtime handles
	JILLong				vmUsedHandles;				//!< The number of currently used runtime handles
	JILLong				vmHandleAllocGrain;			//!< The number of handles per bucket, runtime handles are allocated in buckets
	JILHandle**			vmppHandles;				//!< Pointer array of all existing (used or unused) runtime handles
	JILHandle**			vmppFreeHandles;			//!< Pointer array (a stack) of unused runtime handles, for fast reuse of free handles

	// native types
	JILLong				vmMaxNativeTypes;			//!< The currently allocated size of the native type library list
	JILLong				vmUsedNativeTypes;			//!< The number of currently registered native type libraries
	JILLong				vmNativeTypeGrain;			//!< The number of native type items to allocate at once
	JILTypeListItem*	vmpTypeList;				//!< Pointer to the native type list

	// misc pointers
	JILHandle*			vmpThrowHandle;				//!< Stores thrown exception object
	JILUnknown*			vmpUser[kUserDataSize];		//!< Pointers to user data, see JILAttachObject(), JILGetObject()
	JILUnknown*			vmpChunkBuffer;				//!< A chunk buffer used internally for JILSaveBinary()
	JILTable*			vmpTypeInfoTable;			//!< A hash table for fast retrieval of type info
	JILGCEventRecord*	vmpFirstEventRecord;		//!< Pointer to a list of JILGCEventRecords
	JILLogOutputProc	vmLogOutputProc;			//!< Pointer to a C/C++ log output handler, or NULL
	JILFileInputProc	vmFileInput;				//!< Pointer to user file input callback, or NULL
	JCLState*			vmpCompiler;				//!< The main compiler object

	// exception procs
	JILExceptionProc	vmMachineException;			//!< Pointer to a C/C++ exception handler, or NULL
	JILExceptionProc	vmSoftwareException;		//!< Pointer to a C/C++ exception handler, or NULL
	JILExceptionProc	vmTraceException;			//!< Pointer to a C/C++ exception handler, or NULL
	JILExceptionProc	vmBreakException;			//!< Pointer to a C/C++ exception handler, or NULL

	// malloc / free
	JILMallocProc		vmMalloc;					//!< Pointer to memory allocator used during runtime. If you want to set this to your custom allocator, you should do so directly after JILInitialize() to avoid inconsistencies.
	JILFreeProc			vmFree;						//!< Pointer to memory deallocator used during runtime. If you want to set this to your custom deallocator, you should do so directly after JILInitialize() to avoid inconsistencies.

	// fixed memory support
	JILFixMem*			vmFixMem16;					//!< Pointer to fixed memory manager object for blocks up to 16 byte, or NULL
	JILFixMem*			vmFixMem32;					//!< Pointer to fixed memory manager object for blocks up to 32 byte, or NULL
	JILFixMem*			vmFixMem64;					//!< Pointer to fixed memory manager object for blocks up to 64 byte, or NULL
	JILFixMem*			vmFixMem128;				//!< Pointer to fixed memory manager object for blocks up to 128 byte, or NULL
	JILFixMem*			vmFixMem256;				//!< Pointer to fixed memory manager object for blocks up to 256 byte, or NULL
	JILFixMem*			vmFixMem512;				//!< Pointer to fixed memory manager object for blocks up to 512 byte, or NULL
	JILMemStats*		vmpStats;					//!< Pointer to memory usage statistics (DEBUG only)

	// version info
	JILVersionInfo		vmVersion;					//!< Contains version information, see JILGetRuntimeVersion()
};

#endif	// #ifndef JILAPITYPES_H
