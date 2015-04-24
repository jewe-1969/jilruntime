//------------------------------------------------------------------------------
// File: JILRuntime.c                                       (c) 2003 jewe.org
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
//	The main implementation of the JIL runtime.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

//------------------------------------------------------------------------------
// JIL includes
//------------------------------------------------------------------------------

#include "jilapi.h"
#include "jilcstrsegment.h"
#include "jilsymboltable.h"
#include "jilhandle.h"
#include "jilmachine.h"
#include "jiltypelist.h"
#include "jilchunk.h"
#include "jildebug.h"
#include "jiltools.h"
#include "jilversion.h"
#include "jiltypeinfo.h"
#include "jilnativetypeex.h"
#include "jilarray.h"
#include "jilstring.h"
#include "jilfixmem.h"
#include "jillist.h"
#include "jiltable.h"
#include "jilprogramming.h"
#include "jilallocators.h"

//------------------------------------------------------------------------------
// other constants or variables
//------------------------------------------------------------------------------

static const JILLong kCStrAllocGrain		= 256;		// cstr segment
static const JILLong kHandleAllocGrain		= 1024;		// handles
static const JILLong kTypeAllocGrain		= 16;		// native types

//------------------------------------------------------------------------------
// extern references
//------------------------------------------------------------------------------

JILEXTERN JILLong g_NewCalls;
JILEXTERN JILLong g_DeleteCalls;
JILEXTERN const JILChar* kNameGlobalClass;
JILEXTERN const JILExceptionInfo JILExceptionStrings[JIL_Num_Exception_Strings];
JILEXTERN JILError JILInitializeCompiler(JILState*, const JILChar*);
JILEXTERN JILError JCLFreeCompiler(JILState*);
JILEXTERN JILError JILRuntimeProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);
JILEXTERN JILError JILRuntimeExceptionProc(NTLInstance*, JILLong, JILLong, JILUnknown*, JILUnknown**);
JILEXTERN JILError JILArrayListProc(NTLInstance*, JILLong, JILLong, JILUnknown*, JILUnknown**);

//------------------------------------------------------------------------------
// Default callbacks
//------------------------------------------------------------------------------

static JILUnknown* DefaultMallocProc(JILState*, JILLong);
static void DefaultFreeProc(JILState*, JILUnknown*);
static JILUnknown* FixedMallocProc(JILState*, JILLong);
static void FixedFreeProc(JILState*, JILUnknown*);
static int DefaultFileInputProc(JILState*, int, char*, int, void**);
static void JILClearGCEventList(JILState*);

JILError JILInitializeRuntime(JILState*, const JILChar*, JILBool);
JILError JILTerminateRuntime(JILState*);
const JILChar* JCLGetErrorString(JILError err);

//------------------------------------------------------------------------------
// Implement Segments
//------------------------------------------------------------------------------

IMPL_SEGMENT( JILDataHandle )
IMPL_SEGMENT( JILLong )
IMPL_SEGMENT( JILFuncInfo )

//------------------------------------------------------------------------------
// JILInitialize
//------------------------------------------------------------------------------

JILState* JILInitialize(JILLong stackSize, const JILChar* options)
{
	JILError err;
	JILLong v;

	// allocate our virtual machine state object
	JILState* pState = (JILState*) malloc( sizeof(JILState) );
	memset(pState, 0, sizeof(JILState));

	if( stackSize < kMinimumStackSize )
		stackSize = kMinimumStackSize;
	pState->vmCallStackSize = stackSize / 4;
	pState->vmDataStackSize = stackSize;

	// malloc / free / file input procs
	pState->vmMalloc = DefaultMallocProc;
	pState->vmFree = DefaultFreeProc;
	pState->vmFileInput = DefaultFileInputProc;

	JIL_INSERT_DEBUG_CODE(
		pState->vmpStats   = (JILMemStats*) malloc(sizeof(JILMemStats));
		memset(pState->vmpStats, 0, sizeof(JILMemStats));
	)

	// build flags
	#if defined(_DEBUG) || JIL_TRACE_RELEASE
		pState->vmVersion.BuildFlags |= kTraceExceptionEnabled;
	#endif
	#if JIL_RUNTIME_CHECKS
		pState->vmVersion.BuildFlags |= kExtendedRuntimeChecks;
	#endif
	#ifdef _DEBUG
		pState->vmVersion.BuildFlags |= kDebugBuild;
	#else
		pState->vmVersion.BuildFlags |= kReleaseBuild;
	#endif
	#if JIL_STRING_POOLING
		pState->vmStringPooling = JILTrue;
	#endif
	// library version
	v = JILRevisionToLong( JIL_LIBRARY_VERSION );
	pState->vmVersion.LibraryVersion = v;
	// runtime version
	v = JILRevisionToLong( JIL_MACHINE_VERSION );
	pState->vmVersion.RuntimeVersion = v;
	// compiler version
	v = JILRevisionToLong( JIL_COMPILER_VERSION );
	pState->vmVersion.CompilerVersion = v;
	// type interface version
	v = JILRevisionToLong( JIL_TYPE_INTERFACE_VERSION );
	pState->vmVersion.TypeInterfaceVersion = v;

	// specify alloc grain for code and data segment
	pState->vmSegmentAllocGrain = kSegmentAllocGrain;
	pState->vmCStrSegAllocGrain = ((kCStrAllocGrain + 3) & 0xFFFFFC);

	// create native type list and register built-in types
	err = JILInitTypeList( pState, kTypeAllocGrain );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILStringProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILArrayProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILListProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILIteratorProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILArrayListProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILTableProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILRuntimeProc );
	if( err )
		goto exit;
	err = JILRegisterNativeType( pState, JILRuntimeExceptionProc );
	if( err )
		goto exit;

	// init runtime
	err = JILInitializeRuntime(pState, options, JILTrue);
	if( err )
		goto exit;

	return pState;

exit:
	JILTerminate(pState);
	return NULL;
}

//------------------------------------------------------------------------------
// JILTerminate
//------------------------------------------------------------------------------

JILError JILTerminate(JILState* pState)
{
	JILError err = JIL_No_Exception;
	JILError result;

	// terminate runtime
	result = JILTerminateRuntime(pState);
	if( result && !err )
		err = result;

	// destroy native type list
	result = JILDestroyTypeList(pState);
	if( result && !err )
		err = result;

	// clear GC event list
	JILClearGCEventList(pState);

	// free any fixed memory manager in use
	Delete_FixMem(pState->vmFixMem16);
	pState->vmFixMem16 = NULL;
	Delete_FixMem(pState->vmFixMem32);
	pState->vmFixMem32 = NULL;
	Delete_FixMem(pState->vmFixMem64);
	pState->vmFixMem64 = NULL;
	Delete_FixMem(pState->vmFixMem128);
	pState->vmFixMem128 = NULL;
	Delete_FixMem(pState->vmFixMem256);
	pState->vmFixMem256 = NULL;
	Delete_FixMem(pState->vmFixMem512);
	pState->vmFixMem512 = NULL;

	// check for runtime memory leaks
	JIL_INSERT_DEBUG_CODE(
		if( pState->vmpStats->numAlloc != pState->vmpStats->numFree && !err )
			err = JIL_ERR_Detected_Memory_Leaks;
		JILMessageLog(
			pState, "Runtime terminated with error code %d\n"
			"Compiler allocs/frees:                     %d/%d\n"
			"Runtime allocs/frees:                      %d/%d\n"
			"Runtime leaked handles:                    %d\n"
			"Runtime bytes currently requested:         %d\n"
			"Runtime max bytes requested ever:          %d\n"
			"Runtime number of buckets allocated:       %d\n"
			"Runtime total bytes allocated for buckets: %d\n",
			err, g_NewCalls, g_DeleteCalls,
			pState->vmpStats->numAlloc, pState->vmpStats->numFree,
			pState->errHandlesLeaked,
			pState->vmpStats->bytesUsed, pState->vmpStats->maxBytesUsed,
			pState->vmpStats->numBuckets, pState->vmpStats->bucketBytes);
		free( pState->vmpStats );
	)
	// free our state object
	free( pState );
	return err;
}

//------------------------------------------------------------------------------
// JILGetFunction
//------------------------------------------------------------------------------

JILHandle* JILGetFunction(JILState* pState, JILHandle* pObj, const JILChar* pClass, const JILChar* pName)
{
	JILHandle* pResult;
	JILFuncInfo* pFuncInfo;
	JILTypeInfo* pTypeInfo;
	JILLong index;

	if( pObj )	// instance method
	{
		// verify pObj is a class
		pTypeInfo = JILTypeInfoFromType(pState, pObj->type);
		if( pTypeInfo->family != tf_class )
			return NULL;
		// try to find the method
		index = JILGetFunctionByName(pState, pObj->type, pName, &pFuncInfo);
		if( index < 0 )
			return NULL;
		if( !JILFuncIsMethod(pFuncInfo->flags) ) // not a method
			return NULL;
		// success, create delegate
		pResult = JILGetNewHandle(pState);
		pResult->type = type_delegate;
		JILGetDelegateHandle(pResult)->pDelegate = JILAllocDelegate(pState, pFuncInfo->memberIdx, pObj);
		return pResult;
	}
	else	// global function
	{
		if( pClass == NULL )
			pClass = kNameGlobalClass;
		// get the class
		JILFindTypeInfo(pState, pClass, &pTypeInfo);
		if( !pTypeInfo || pTypeInfo->family != tf_class )
			return NULL;
		index = JILGetFunctionByName(pState, pTypeInfo->type, pName, &pFuncInfo);
		if( index < 0 )
			return NULL;
		if( JILFuncIsMethod(pFuncInfo->flags) ) // not a function
			return NULL;
		// success, create delegate
		pResult = JILGetNewHandle(pState);
		pResult->type = type_delegate;
		JILGetDelegateHandle(pResult)->pDelegate = JILAllocDelegate(pState, index, NULL);
		return pResult;
	}
}

//------------------------------------------------------------------------------
// JILCallFunction
//------------------------------------------------------------------------------

JILHandle* JILCallFunction(JILState* pState, JILHandle* pFunc, JILLong numArgs, ...)
{
	JILError err = JIL_No_Exception;
	JILStackFrame stackFrame;
	JILLong i, t;
	JILFloat f;
	JILChar* cstr;
	JILHandle* pHandle;
	JILHandle* pResult;
	JILHandle** sp;
	JILString* jstr;
	va_list marker;

	// bail, if not yet initialized or blocked
	if( !pState->vmInitialized || pState->vmBlocked )
		return NULL;

	// free any throw handle from a previous call
	if( pState->vmpThrowHandle )
	{
		JILRelease(pState, pState->vmpThrowHandle);
		pState->vmpThrowHandle = NULL;
	}

	// create new stack frame
	JILPushStackFrame(pState, &stackFrame);

	// reserve space on stack
	pState->vmpContext->vmDataStackPointer -= numArgs;
	sp = pState->vmpContext->vmppDataStack + pState->vmpContext->vmDataStackPointer;
	// push arguments onto stack
	va_start( marker, numArgs );	// Initialize variable arguments
	for( i = 0; i < numArgs; i++ )
	{
		t = va_arg( marker, JILLong );
		switch( t )
		{
			case kArgInt:
				t = va_arg( marker, JILLong );
				sp[i] = NTLNewHandleForObject(pState, type_int, &t);
				break;
			case kArgFloat:
				f = va_arg( marker, double ); // GCC insists we use 'double' here if JILFloat is 'float'
				sp[i] = NTLNewHandleForObject(pState, type_float, &f);
				break;
			case kArgString:
				cstr = va_arg( marker, JILChar* );
				jstr = JILString_New(pState);
				JILString_Assign(jstr, cstr);
				sp[i] = NTLNewHandleForObject(pState, type_string, jstr);
				break;
			case kArgHandle:
				pHandle = va_arg( marker, JILHandle* );
				JILAddRef( pHandle );
				sp[i] = pHandle;
				break;
			default:
				pHandle = JILGetNullHandle(pState);
				JILAddRef( pHandle );
				sp[i] = pHandle;
				err = JIL_ERR_Illegal_Argument;
				break;
		}
	}
	va_end( marker );

	// call function
	if( !err )
	{
		err = JILCallDelegate(pState, pFunc);
	}
	if( err && err != JIL_VM_Software_Exception )
	{
		pResult = JILCreateException(pState, err);
	}
	else if( pState->vmpThrowHandle )
	{
		pResult = pState->vmpThrowHandle;
		JILAddRef(pResult);
	}
	else
	{
		pResult = stackFrame.ctx->vmppRegister[kReturnRegister];
		JILAddRef(pResult);
	}

	// clean up the stack and return result
	JILPopStackFrame(pState, &stackFrame);
	return pResult;
}

//------------------------------------------------------------------------------
// JILGetExceptionVector
//------------------------------------------------------------------------------

JILExceptionProc JILGetExceptionVector(JILState* pState, JILLong vector)
{
	switch( vector )
	{
		case JIL_Machine_Exception_Vector:
			return pState->vmMachineException;
		case JIL_Software_Exception_Vector:
			return pState->vmSoftwareException;
		case JIL_Trace_Exception_Vector:
			return pState->vmTraceException;
		case JIL_Break_Exception_Vector:
			return pState->vmBreakException;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// JILSetExceptionVector
//------------------------------------------------------------------------------

JILError JILSetExceptionVector(JILState* pState, JILLong vector, JILExceptionProc pProc)
{
	switch( vector )
	{
		case JIL_Machine_Exception_Vector:
			pState->vmMachineException = pProc;
			return JIL_No_Exception;
		case JIL_Software_Exception_Vector:
			pState->vmSoftwareException = pProc;
			return JIL_No_Exception;
		case JIL_Trace_Exception_Vector:
			pState->vmTraceException = pProc;
			return JIL_No_Exception;
		case JIL_Break_Exception_Vector:
			pState->vmBreakException = pProc;
			return JIL_No_Exception;
	}
	return JIL_ERR_Invalid_Vector;
}

//------------------------------------------------------------------------------
// JILAttachObject
//------------------------------------------------------------------------------

void* JILAttachObject(JILState* pState, int objectID, void* pData)
{
	void* pOld;

	if( objectID < 0 || objectID >= kUserDataSize )
		return NULL;

	pOld = pState->vmpUser[objectID];
	pState->vmpUser[objectID] = pData;
	return pOld;
}

//------------------------------------------------------------------------------
// JILGetObject
//------------------------------------------------------------------------------

void* JILGetObject(JILState* pState, int objectID)
{
	if( objectID < 0 || objectID >= kUserDataSize )
		return NULL;

	return pState->vmpUser[objectID];
}

//------------------------------------------------------------------------------
// JILGetExceptionString
//------------------------------------------------------------------------------

const JILChar* JILGetExceptionString(JILState* pState, JILError e)
{
	int i;
	for( i = 0; JILExceptionStrings[i].e != JIL_Unknown_Exception; i++ )
	{
		if( JILExceptionStrings[i].e == e )
			return JILExceptionStrings[i].s;
	}
	return JCLGetErrorString(e);
}

//------------------------------------------------------------------------------
// JILGetRuntimeVersion
//------------------------------------------------------------------------------

const JILVersionInfo* JILGetRuntimeVersion(JILState* pState)
{
	return &(pState->vmVersion);
}

//------------------------------------------------------------------------------
// JILGetVersionString
//------------------------------------------------------------------------------

const JILChar* JILGetVersionString(JILLong version, JILChar* pBuffer)
{
	return JILLongToRevision(pBuffer, version);
}

//------------------------------------------------------------------------------
// JILSetLogCallback
//------------------------------------------------------------------------------

void JILSetLogCallback(JILState* pState, JILLogOutputProc proc)
{
	pState->vmLogOutputProc = proc;
}

//------------------------------------------------------------------------------
// JILUseFixedMemory
//------------------------------------------------------------------------------

JILError JILUseFixedMemory(JILState* pState, JILLong max16, JILLong max32, JILLong max64, JILLong max128, JILLong max256, JILLong max512)
{
	JILError err = JIL_No_Exception;

	// Are we set up already?
	if( pState->vmInitialized )
		return JIL_ERR_Runtime_Locked;
	if( pState->vmFixMem16 )
		return JIL_ERR_Runtime_Locked;

	pState->vmFixMem16  = New_FixMem( 16, max16, 512, pState->vmpStats);
	pState->vmFixMem32  = New_FixMem( 32, max32, 256, pState->vmpStats);
	pState->vmFixMem64  = New_FixMem( 64, max64, 128, pState->vmpStats);
	pState->vmFixMem128 = New_FixMem(128, max128, 64, pState->vmpStats);
	pState->vmFixMem256 = New_FixMem(256, max256, 32, pState->vmpStats);
	pState->vmFixMem512 = New_FixMem(512, max512, 16, pState->vmpStats);

	pState->vmMalloc = FixedMallocProc;
	pState->vmFree = FixedFreeProc;

	return err;
}

//------------------------------------------------------------------------------
// JILUseFixedMemDynamic
//------------------------------------------------------------------------------

JILError JILUseFixedMemDynamic(JILState* pState)
{
	return JILUseFixedMemory(pState, 0, 0, 0, 0, 0, 0);
}

//------------------------------------------------------------------------------
// JILMalloc
//------------------------------------------------------------------------------

JILUnknown* JILMalloc(JILState* pState, JILLong size)
{
	return pState->vmMalloc(pState, size);
}

//------------------------------------------------------------------------------
// JILMfree
//------------------------------------------------------------------------------

void JILMfree(JILState* pState, JILUnknown* ptr)
{
	pState->vmFree(pState, ptr);
}

//------------------------------------------------------------------------------
// JILSetBlocked
//------------------------------------------------------------------------------

JILBool JILSetBlocked(JILState* pState, JILBool flag)
{
	JILBool blocked = pState->vmBlocked;
	if( flag >= 0 )
		pState->vmBlocked = flag;
	return blocked;
}

//------------------------------------------------------------------------------
// JILSetFileInputProc
//------------------------------------------------------------------------------

void JILSetFileInputProc(JILState* pState, JILFileInputProc proc)
{
	pState->vmFileInput = proc;
}

//------------------------------------------------------------------------------
// JILInitializeRuntime
//------------------------------------------------------------------------------

JILError JILInitializeRuntime(JILState* pState, const JILChar* options, JILBool initSegments)
{
	JILError err;
	JILDataHandle* pHandle;

	if( !pState )
		return JIL_ERR_Initialize_Failed;

	// construct all buffers
	pState->vmpCodeSegment = (Seg_JILLong*) malloc(sizeof(Seg_JILLong));
	pState->vmpDataSegment = (Seg_JILDataHandle*) malloc(sizeof(Seg_JILDataHandle));
	pState->vmpFuncSegment = (Seg_JILFuncInfo*) malloc(sizeof(Seg_JILFuncInfo));

	// construct runtime handles
	err = JILInitHandles( pState, kHandleAllocGrain );
	if( err ) return err;

	// construct segments
	if( initSegments )
	{
		err = InitSegment_JILLong( pState->vmpCodeSegment, kInitialSegmentSize );
		if( err ) return err;
		err = InitSegment_JILFuncInfo( pState->vmpFuncSegment, kInitialSegmentSize );
		if( err ) return err;
		err = JILInitCStrSegment( pState, kInitialSegmentSize );
		if( err ) return err;
		err = JILInitTypeInfoSegment( pState, kInitialSegmentSize );
		if( err ) return err;
		err = InitSegment_JILDataHandle( pState->vmpDataSegment, kInitialSegmentSize );
		if( err ) return err;

		// spoil first data handle (null handle)
		NewElement_JILDataHandle(pState->vmpDataSegment, &pHandle);
		pState->vmInitDataIncr = 1;

		// initialize the compiler (can only do this if we have segments!)
		err = JILInitializeCompiler(pState, options);
		if( err ) return err;
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILTerminateRuntime
//------------------------------------------------------------------------------

JILError JILTerminateRuntime(JILState* pState)
{
	JILError err = JIL_No_Exception;
	JILError result;

	// the order is critical here!
	result = JCLFreeCompiler(pState);
	if( result && !err )
		err = result;
	result = JILTermVM( pState );
	if( result && !err )
		err = result;
	result = JILDestroyHandles( pState );
	if( result && !err )
		err = result;
	result = DestroySegment_JILDataHandle( pState->vmpDataSegment );
	if( result && !err )
		err = result;
	result = JILDestroyTypeInfoSegment( pState );
	if( result && !err )
		err = result;
	result = JILDestroyCStrSegment( pState );
	if( result && !err )
		err = result;
	result = DestroySegment_JILFuncInfo( pState->vmpFuncSegment );
	if( result && !err )
		err = result;
	result = DestroySegment_JILLong( pState->vmpCodeSegment );
	if( result && !err )
		err = result;
	result = JILRemoveSymbolTable( pState );
	if( result && !err )
		err = result;

	// free any chunk buffer that might still exist
	if( pState->vmpChunkBuffer )
	{
		free( pState->vmpChunkBuffer );
		pState->vmpChunkBuffer = NULL;
	}
	free( pState->vmpCodeSegment );
	pState->vmpCodeSegment = NULL;
	free( pState->vmpDataSegment );
	pState->vmpDataSegment = NULL;
	free( pState->vmpFuncSegment );
	pState->vmpFuncSegment = NULL;

	return err;
}

//------------------------------------------------------------------------------
// JILHandleRuntimeOptions
//------------------------------------------------------------------------------

JILError JILHandleRuntimeOptions(JILState* pState, const JILChar* pName, const JILChar* pValue)
{
	JILError err = JIL_No_Exception;
	JILLong nValue = atoi(pValue);
	if( strcmp(pName, "call-stack-size") == 0 )
	{
		if( pState->vmInitialized )
			return JIL_ERR_Runtime_Locked;
		if( nValue < (kMinimumStackSize / 4) )
			return JCL_WARN_Invalid_Option_Value;
		pState->vmCallStackSize = nValue;
	}
	else if( strcmp(pName, "data-stack-size") == 0 )
	{
		if( pState->vmInitialized )
			return JIL_ERR_Runtime_Locked;
		if( nValue < kMinimumStackSize )
			return JCL_WARN_Invalid_Option_Value;
		pState->vmDataStackSize = nValue;
	}
	else if( strcmp(pName, "stack-size") == 0 )
	{
		if( pState->vmInitialized )
			return JIL_ERR_Runtime_Locked;
		if( nValue < kMinimumStackSize )
			return JCL_WARN_Invalid_Option_Value;
		pState->vmDataStackSize = nValue;
		pState->vmCallStackSize = nValue / 4;
	}
	else if( strcmp(pName, "log-garbage") == 0 )
	{
		if( strcmp(pValue, "all") == 0 )
			pState->vmLogGarbageMode = kLogGarbageAll;
		else if( strcmp(pValue, "brief") == 0 )
			pState->vmLogGarbageMode = kLogGarbageBrief;
		else if( strcmp(pValue, "none") == 0 )
			pState->vmLogGarbageMode = kLogGarbageNone;
		else
			return JCL_WARN_Invalid_Option_Value;
	}
	else if( strcmp(pName, "document") == 0 )
	{
		if( strcmp(pValue, "user") == 0 || strcmp(pValue, "default") == 0 )
			pState->vmDocGenMode = JIL_GenDocs_User;
		else if( strcmp(pValue, "builtin") == 0 )
			pState->vmDocGenMode = JIL_GenDocs_BuiltIn;
		else if( strcmp(pValue, "all") == 0 )
			pState->vmDocGenMode = JIL_GenDocs_All;
		else
			return JCL_WARN_Invalid_Option_Value;
	}
	else
	{
		err = JCL_WARN_Unknown_Option;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILGetFunctionTable
//------------------------------------------------------------------------------

JILFunctionTable* JILGetFunctionTable(JILState* pState, JILHandle* pObj)
{
	JILFunctionTable* pResult;
	JILTypeInfo* pTypeInfo;
	JILLong fn;
	if( !pObj )
		return NULL;
	// verify pObj is a class
	pTypeInfo = JILTypeInfoFromType(pState, pObj->type);
	if( pTypeInfo->family != tf_class )
		return NULL;
	// allocate everything in one big chunk
	pResult = (JILFunctionTable*) malloc(sizeof(JILFunctionTable) + pTypeInfo->sizeVtab * sizeof(JILHandle*));
	pResult->func = (JILHandle**) (pResult + 1);
	pResult->size = pTypeInfo->sizeVtab;
	// get function handles
	for( fn = 0; fn < pState->vmpFuncSegment->usedSize; fn++ )
	{
		JILFuncInfo* pFunc = JILGetFunctionInfo(pState, fn);
		if( pFunc->type == pObj->type )
		{
			JILHandle* pHandle = JILGetNewHandle(pState);
			pHandle->type = type_delegate;
			if( JILFuncIsMethod(pFunc->flags) ) // TODO: Should we filter out accessors, convertors, etc here?
				JILGetDelegateHandle(pHandle)->pDelegate = JILAllocDelegate(pState, pFunc->memberIdx, pObj);
			else
				JILGetDelegateHandle(pHandle)->pDelegate = JILAllocDelegate(pState, fn, NULL);
			pResult->func[pFunc->memberIdx] = pHandle;
		}
	}
	return pResult;
}

//------------------------------------------------------------------------------
// JILFreeFunctionTable
//------------------------------------------------------------------------------

void JILFreeFunctionTable(JILState* pState, JILFunctionTable* pTable)
{
	JILLong fn;
	if( !pTable )
		return;
	for( fn = 0; fn < pTable->size; fn++ )
	{
		JILRelease(pState, pTable->func[fn]);
	}
	free( pTable );
}

//------------------------------------------------------------------------------
// JILMarkFunctionTable
//------------------------------------------------------------------------------

JILError JILMarkFunctionTable(JILState* pState, JILFunctionTable* pTable)
{
	JILError err = JIL_No_Exception;
	JILLong fn;
	if( !pTable )
		return err;
	for( fn = 0; fn < pTable->size; fn++ )
	{
		err = JILMarkHandle(pState, pTable->func[fn]);
		if( err )
			break;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILRegisterGCEvent
//------------------------------------------------------------------------------

JILError JILRegisterGCEvent(JILState* pState, JILGCEventHandler proc, JILUnknown* pUser)
{
	JILError err = JIL_ERR_Illegal_Argument;
	JILGCEventRecord* pRecord;
	if( !proc )
		return err;

	// first try to find pUser in our list
	for( pRecord = pState->vmpFirstEventRecord; pRecord; pRecord = pRecord->pNext )
	{
		if( pRecord->pUserPtr == pUser )
			return err;
	}
	// not in list: add
	pRecord = (JILGCEventRecord*) malloc(sizeof(JILGCEventRecord));
	pRecord->eventProc = proc;
	pRecord->pUserPtr = pUser;
	pRecord->pNext = pState->vmpFirstEventRecord;
	pState->vmpFirstEventRecord = pRecord;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILUnregisterGCEvent
//------------------------------------------------------------------------------

JILError JILUnregisterGCEvent(JILState* pState, JILUnknown* pUser)
{
	JILError err = JIL_ERR_Illegal_Argument;
	JILGCEventRecord* pRecord;
	JILGCEventRecord* pPrev = NULL;
	if( !pUser || !pState )
		return err;

	// try to find pUser in our list
	for( pRecord = pState->vmpFirstEventRecord; pRecord; pPrev = pRecord, pRecord = pRecord->pNext )
	{
		if( pRecord->pUserPtr == pUser )
		{
			if( pPrev )
				pPrev->pNext = pRecord->pNext;
			else
				pState->vmpFirstEventRecord = pRecord->pNext;
			free( pRecord );
			return JIL_No_Exception;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// JILGetTimeLastGC
//------------------------------------------------------------------------------

JILFloat JILGetTimeLastGC(JILState* pState)
{
	return pState->vmTimeLastGC;
}

//------------------------------------------------------------------------------
// JILGetImplementors
//------------------------------------------------------------------------------

JILLong JILGetImplementors(JILState* pVM, JILLong* pOutArray, JILLong outSize, JILLong interfaceId)
{
	JILTypeInfo* pType;
	JILLong clas;
	JILLong numWritten = 0;

	if( pOutArray == NULL )
		return -1;
	if( !NTLIsValidTypeID(pVM, interfaceId) )
		return -2;
	pType = JILTypeInfoFromType(pVM, interfaceId);
	if( pType->family != tf_interface )
		return -3;

	// iterate over all type info elements
	for( clas = 0; clas < pVM->vmUsedTypeInfoSegSize; clas++ )
	{
		pType = JILTypeInfoFromType(pVM, clas);
		if( pType->family == tf_class && pType->base == interfaceId )
		{
			if( numWritten >= outSize )
				break;
			pOutArray[numWritten] = clas;
			numWritten++;
		}
	}
	return numWritten;
}

//------------------------------------------------------------------------------
// JILClearGCEventList
//------------------------------------------------------------------------------

static void JILClearGCEventList(JILState* pState)
{
	JILGCEventRecord* pRecord;
	JILGCEventRecord* pNext;
	for( pRecord = pState->vmpFirstEventRecord; pRecord; pRecord = pNext )
	{
		pNext = pRecord->pNext;
		free( pRecord );
	}
	pState->vmpFirstEventRecord = NULL;
}

//------------------------------------------------------------------------------
// DefaultMallocProc
//------------------------------------------------------------------------------

static JILUnknown* DefaultMallocProc(JILState* pState, JILLong numBytes)
{
	JIL_INSERT_DEBUG_CODE( pState->vmpStats->numAlloc++; )

	return malloc(numBytes);
}

//------------------------------------------------------------------------------
// DefaultFreeProc
//------------------------------------------------------------------------------

static void DefaultFreeProc(JILState* pState, JILUnknown* pBuffer)
{
	JIL_INSERT_DEBUG_CODE( pState->vmpStats->numFree++; )

	free(pBuffer);
}

//------------------------------------------------------------------------------
// FixedMallocProc
//------------------------------------------------------------------------------

static JILUnknown* FixedMallocProc(JILState* pState, JILLong numBytes)
{
	JILUnknown* pResult;

	if( numBytes <= 0 )
		return NULL;
	else if( numBytes <= 16 )
		pResult = FixMem_Alloc(pState->vmFixMem16);
	else if( numBytes <= 32 )
		pResult = FixMem_Alloc(pState->vmFixMem32);
	else if( numBytes <= 64 )
		pResult = FixMem_Alloc(pState->vmFixMem64);
	else if( numBytes <= 128 )
		pResult = FixMem_Alloc(pState->vmFixMem128);
	else if( numBytes <= 256 )
		pResult = FixMem_Alloc(pState->vmFixMem256);
	else if( numBytes <= 512 )
		pResult = FixMem_Alloc(pState->vmFixMem512);
	else
		pResult = FixMem_AllocLargeBlock(numBytes, pState->vmpStats);

	JIL_INSERT_DEBUG_CODE(
		if( pResult == NULL )
			JILMessageLog(pState, "ERROR: Out of memory in FixedMallocProc()\n");
	)

	return pResult;
}

//------------------------------------------------------------------------------
// FixedFreeProc
//------------------------------------------------------------------------------

static void FixedFreeProc(JILState* pState, JILUnknown* pBuffer)
{
	JILLong numBytes;

	if( !pBuffer )
		return;
	numBytes = FixMem_GetBlockLength(pBuffer);
	if( numBytes <= 16 )
		FixMem_Free(pState->vmFixMem16, pBuffer);
	else if( numBytes <= 32 )
		FixMem_Free(pState->vmFixMem32, pBuffer);
	else if( numBytes <= 64 )
		FixMem_Free(pState->vmFixMem64, pBuffer);
	else if( numBytes <= 128 )
		FixMem_Free(pState->vmFixMem128, pBuffer);
	else if( numBytes <= 256 )
		FixMem_Free(pState->vmFixMem256, pBuffer);
	else if( numBytes <= 512 )
		FixMem_Free(pState->vmFixMem512, pBuffer);
	else
		FixMem_FreeLargeBlock(pBuffer, pState->vmpStats);
}

//------------------------------------------------------------------------------
// DefaultFileInputProc
//------------------------------------------------------------------------------

static JILError DefaultFileInputProc(JILState* pState, JILLong mode, JILChar* pBuffer, JILLong size, JILUnknown** ppFile)
{
	JILError result = JIL_ERR_Generic_Error;

#if JIL_USE_LOCAL_FILESYS

	switch( mode )
	{
		// Open file name in 'pBuffer'.
		case JILFileInputOpen:
		{
			FILE* pFile = fopen(pBuffer, "rb");
			if( pFile )
			{
				*ppFile = pFile;
				result = JIL_No_Exception;
			}
			break;
		}
		// Read 'size' bytes from file to 'pBuffer'.
		case JILFileInputRead:
		{
			FILE* pFile = *ppFile;
			result = fread(pBuffer, sizeof(JILChar), size, pFile);
			break;
		}
		// Seek to position in 'size'. If not applicable in your custom file proc, return JIL_ERR_File_Generic.
		case JILFileInputSeek:		
		{
			FILE* pFile = *ppFile;
			result = fseek(pFile, size, SEEK_SET);
			break;
		}
		// Return file length. If not applicable in your custom file proc, return 0.
		case JILFileInputLength:	
		{
			FILE* pFile = *ppFile;
			result = fseek(pFile, 0, SEEK_END);
			if( !result )
				result = ftell(pFile);
			break;
		}
		// Close the file.
		case JILFileInputClose:		
		{
			FILE* pFile = *ppFile;
			fclose( pFile );
			result = JIL_No_Exception;
			break;
		}
		// Copy current working directory to 'pBuffer'. If not applicable in your custom file proc, return empty string.
		case JILFileInputGetCwd:	
		{
			#if WIN32
				char* buf = _getcwd(NULL, 0);
				if( buf != NULL )
				{
					JILStrncpy(pBuffer, size, buf, strlen(buf));
					free(buf);
				}
			#else
				*pBuffer = 0;
			#endif
			result = JIL_No_Exception;
			break;
		}
	}

#endif

	return result;
}
