//------------------------------------------------------------------------------
// File: JILMachine.c                                          (c) 2003 jewe.org
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
//	The "kernel" of the virtual machine.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilmachine.h"
#include "jilhandle.h"
#include "jiltypelist.h"
#include "jilallocators.h"
#include "jilcallntl.h"
#include "jilprogramming.h"

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static JILLong JILExecuteByteCode	(JILState*, JILContext*, JILLong, JILHandle*);
static JILLong JILInitNativeType	(JILState*, JILTypeInfo*, JILLong);
static JILError JILCallFunc			(JILState* pState, JILLong hFunction);
static JILError JILCallMethod		(JILState* pState, JILHandle* object, JILLong nIndex);
static JILError JILCallClosure		(JILState* pState, JILDelegate* pDelegate);

//------------------------------------------------------------------------------
// External references
//------------------------------------------------------------------------------

extern const JILLong kInterfaceExceptionGetError;
extern const JILLong kInterfaceExceptionGetMessage;

JILLong JILExecuteInfinite(JILState*, JILContext*);
void JILCheckInstructionTables(JILState* ps);

//------------------------------------------------------------------------------
// JILInitVM
//------------------------------------------------------------------------------

JILError JILInitVM(JILState* pState)
{
	JILLong i;
	JILLong err;
	JILHandle* pDest;
	JILHandle* pNull;
	JILDataHandle* pSource;
	Seg_JILDataHandle* pDataSegment = pState->vmpDataSegment;

	// check if first initialization
	if( !pState->vmInitialized )
	{
		// re-order free handles stack, so handle order remains consistent with the code!
		for( i = 0; i < pState->vmMaxHandles; i++ )
			pState->vmppFreeHandles[i] = pState->vmppHandles[i];

		// allocate null handle, we need it for initializing the registers!
		pNull = JILGetNewHandle(pState);

		// allocate and use root context
		pState->vmpRootContext = JILAllocContext(pState, 0, 0);
		pState->vmpContext = pState->vmpRootContext;

		JIL_INSERT_DEBUG_CODE( JILCheckInstructionTables(pState) );
	}

	// incrementally create handles from data segment
	for( i = pState->vmInitDataIncr; i < pDataSegment->usedSize; i++ )
	{
		pSource = pDataSegment->pData + i;
		pDest = JILGetNewHandle(pState);
		pSource->index = JILFindHandleIndex(pState, pDest);
		pDest->type = pSource->type;
		// do type specific initialization
		switch( pSource->type )
		{
			case type_int:
				JILGetIntHandle(pDest)->l = JILGetDataHandleLong(pSource);
				break;
			case type_float:
				JILGetFloatHandle(pDest)->f = JILGetDataHandleFloat(pSource);
				break;
			case type_string:
				JILGetStringHandle(pDest)->str = JILAllocStringFromCStr(pState, JILGetDataHandleLong(pSource));
				break;
			default:
				return JIL_VM_Unsupported_Type;
		}
	}

	// incrementally initialize native type libraries
	for( i = pState->vmInitTypeIncr; i < pState->vmUsedTypeInfoSegSize; i++ )
	{
		JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, i);
		if( pTypeInfo->isNative )
		{
			err = JILInitNativeType(pState, pTypeInfo, i);
			if( err )
				return err;
		}
		pTypeInfo->typeNamePtr = JILCStrGetString(pState, pTypeInfo->offsetName);
	}

	// we are initialized
	pState->vmInitDataIncr = pDataSegment->usedSize;
	pState->vmInitTypeIncr = pState->vmUsedTypeInfoSegSize;
	pState->vmInitialized = JILTrue;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILRunInitCode
//------------------------------------------------------------------------------

JILError JILRunInitCode(JILState* pState)
{
	JILError result = JIL_No_Exception;
	if( pState->vmBlocked )
		return JIL_ERR_Runtime_Blocked;
	// check if we need to init
	result = JILInitVM(pState);
	if( result )
		return result;
	// execute byte code
	result = JILExecuteByteCode( pState, pState->vmpRootContext, pState->vmRunInitIncr, NULL );
	pState->vmRunInitIncr = pState->vmpRootContext->vmProgramCounter;
	return result;
}

//------------------------------------------------------------------------------
// JILCallFunc
//------------------------------------------------------------------------------

static JILError JILCallFunc(JILState* pState, JILLong hFunction)
{
	JILFuncInfo* pFuncInfo;
	JILTypeInfo* pTypeInfo;
	JILError result = JIL_No_Exception;

	if( pState->vmBlocked )
		return JIL_ERR_Runtime_Blocked;
	// check if we need to init
	result = JILInitVM(pState);
	if( result )
		return result;
	// get function info
	pFuncInfo = JILGetFunctionInfo(pState, hFunction);
	if( pFuncInfo == NULL )
		return JIL_ERR_Invalid_Function_Index;
	// get type info
	pTypeInfo = JILTypeInfoFromType(pState, pFuncInfo->type);
	// native or script code?
	if( pTypeInfo->isNative )
	{
		result = CallNTLCallStatic(pTypeInfo, pFuncInfo->memberIdx);
	}
	else
	{
		result = JILExecuteByteCode(pState, pState->vmpContext, pFuncInfo->codeAddr, NULL);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILCallMethod
//------------------------------------------------------------------------------

static JILError JILCallMethod(JILState* pState, JILHandle* pObject, JILLong nIndex)
{
	JILTypeInfo* pTypeInfo;
	JILFuncInfo* pFuncInfo;
	JILLong* pVt;
	JILError result;

	if( pState->vmBlocked )
		return JIL_ERR_Runtime_Blocked;
	// check if we need to init
	result = JILInitVM(pState);
	if( result )
		return result;
	pTypeInfo = JILTypeInfoFromType(pState, pObject->type);
	// verify we have an object
	if( pTypeInfo->family != tf_class )
		return JIL_ERR_Invalid_Handle_Type;
	// native or script code?
	if( pTypeInfo->isNative )
	{
		result = CallNTLCallMember(pTypeInfo, nIndex, JILGetNObjectHandle(pObject)->ptr);
	}
	else
	{
		// get function handle
		pVt = JILCStrGetVTable(pState, pTypeInfo->offsetVtab);
		pFuncInfo = JILGetFunctionInfo(pState, pVt[nIndex]);
		if( pFuncInfo == NULL )
			return JIL_ERR_Invalid_Member_Index;
		// execute the bytecode
		result = JILExecuteByteCode(pState, pState->vmpContext, pFuncInfo->codeAddr, pObject);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILCallDelegate
//------------------------------------------------------------------------------

JILError JILCallDelegate(JILState* pState, JILHandle* pDelegate)
{
	JILError result = JIL_No_Exception;
	JILTypeInfo* pTypeInfo;
	JILDelegate* pdg;

	if( pState->vmBlocked )
		return JIL_ERR_Runtime_Blocked;
	// check if we need to init
	result = JILInitVM(pState);
	if( result )
		return result;
	// if the reference is null, return null, don't generate exception (TODO: do we want to keep it that way?)
	if( pDelegate->type == type_null )
	{
		JILAddRef( JILGetNullHandle(pState) );
		JILRelease( pState, pState->vmpContext->vmppRegister[kReturnRegister] );
		pState->vmpContext->vmppRegister[kReturnRegister] = JILGetNullHandle(pState);
		return result;
	}
	// if the reference is not a delegate, fail
	pTypeInfo = JILTypeInfoFromType(pState, pDelegate->type);
	if( pTypeInfo->family != tf_delegate )
		return JIL_ERR_Invalid_Handle_Type;
	// get delegate and call
	pdg = JILGetDelegateHandle(pDelegate)->pDelegate;
	if( pdg->pClosure )
	{
		result = JILCallClosure(pState, pdg);
	}
	else if( pdg->pObject )
	{
		result = JILCallMethod(pState, pdg->pObject, pdg->index);
	}
	else
	{
		result = JILCallFunc(pState, pdg->index);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILCallClosure
//------------------------------------------------------------------------------

static JILError JILCallClosure(JILState* pState, JILDelegate* pDelegate)
{
	JILError result;
	JILFuncInfo* pFuncInfo;
	JILHandle** saveSP = pState->vmpContext->vmppDataStack + pState->vmpContext->vmDataStackPointer;
	JILHandle** psp;
	JILHandle** pcl;
	JILLong oldSP = pState->vmpContext->vmDataStackPointer;
	JILLong i;
	JILLong n;
	// check function index
	pFuncInfo = JILGetFunctionInfo(pState, pDelegate->index);
	if( pFuncInfo == NULL )
		return JIL_ERR_Invalid_Function_Index;
	// push parent stack onto stack
	n = pDelegate->pClosure->stackSize;
	pcl = pDelegate->pClosure->ppStack + n - 1;
	for( i = 0; i < n; i++ )
	{
		JILAddRef(*pcl);
		pState->vmpContext->vmppDataStack[--pState->vmpContext->vmDataStackPointer] = *pcl--;
		if( pState->vmpContext->vmDataStackPointer < 0 )
			return JIL_VM_Stack_Overflow;
	}
	// shuffle function arguments back to top of stack
	n = pFuncInfo->args;
	psp = saveSP + n - 1;
	for( i = 0; i < n; i++ )
	{
		JILAddRef(*psp);
		pState->vmpContext->vmppDataStack[--pState->vmpContext->vmDataStackPointer] = *psp--;
		if( pState->vmpContext->vmDataStackPointer < 0 )
			return JIL_VM_Stack_Overflow;
	}
	// execute the bytecode
	result = JILExecuteByteCode(pState, pState->vmpContext, pFuncInfo->codeAddr, pDelegate->pObject);
	// move stack back to closure
	n = pDelegate->pClosure->stackSize;
	psp = saveSP - n;
	pcl = pDelegate->pClosure->ppStack;
	for( i = 0; i < n; i++ )
	{
		JILAddRef(*psp);
		JILRelease(pState, *pcl);
		*pcl++ = *psp++;
	}
	// pop everything from stack
	n = oldSP - pState->vmpContext->vmDataStackPointer;
	psp = pState->vmpContext->vmppDataStack + pState->vmpContext->vmDataStackPointer;
	for( i = 0; i < n; i++ )
	{
		JILRelease(pState, *psp++);
	}
	pState->vmpContext->vmDataStackPointer = oldSP;
	return result;
}

//------------------------------------------------------------------------------
// JILCallCopyConstructor
//------------------------------------------------------------------------------
// Executes the copy constructor to initialize the given object, passing 'src'
// into the constructor as source reference.

JILError JILCallCopyConstructor(JILState* pState, JILHandle* object, JILHandle* src)
{
	JILError result = JIL_No_Exception;
	JILTypeInfo* pTypeInfo1;
	JILTypeInfo* pTypeInfo2;
	JILStackFrame stackFrame;

	pTypeInfo1 = JILTypeInfoFromType(pState, object->type);
	pTypeInfo2 = JILTypeInfoFromType(pState, src->type);

	if (object->type != src->type || pTypeInfo1->family != tf_class)
		return JIL_ERR_Invalid_Handle_Type;

	// save current context
	JILPushStackFrame(pState, &stackFrame);
	// push source onto stack
	pState->vmpContext->vmppDataStack[--pState->vmpContext->vmDataStackPointer] = src;
	JILAddRef(src);
	// call copy constructor
	result = JILCallMethod(pState, object, pTypeInfo1->methodInfo.cctor);
	// restore context
	JILPopStackFrame(pState, &stackFrame);
	return result;
}

//------------------------------------------------------------------------------
// JILTermVM
//------------------------------------------------------------------------------

JILError JILTermVM(JILState* pState)
{
	JILLong i;
	JILError err = JIL_No_Exception;
	JILDataHandle* pDataHandle;

	if( pState->vmInitialized )
	{
		// block script function calls, we're tearing down the runtime
		pState->vmBlocked = JILTrue;
		// free our root context, this should free all other contexts aswell
		JILFreeContext( pState, pState->vmpRootContext );
		pState->vmpRootContext = NULL;
		pState->vmpContext = NULL;
		// release throw handle
		if( pState->vmpThrowHandle )
		{
			JILRelease(pState, pState->vmpThrowHandle);
		}
		// release all handles we created from the data segment
		for( i = 0; i < pState->vmpDataSegment->usedSize; i++ )
		{
			pDataHandle = pState->vmpDataSegment->pData + i;
			JILRelease(pState, pState->vmppHandles[pDataHandle->index]);
		}
		// terminate all native type libraries used
		for( i = 0; i < pState->vmUsedTypeInfoSegSize; i++ )
		{
			JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, i);
			if( pTypeInfo->isNative )
			{
				err = CallNTLTerminate(pTypeInfo);
				if( err )
					return err;
			}
		}
		pState->vmInitialized = JILFalse;
		pState->vmRunning = JILFalse;
		pState->vmRunLevel = 0;
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILGenerateException
//------------------------------------------------------------------------------

JILError JILGenerateException(JILState* pState, JILError e)
{
	// instantly return in case of rethrow
	if( pState->errException )
		return pState->errException;
	// avoid recursions
	if( !pState->vmExceptionFlag )
	{
		// save machine state
		pState->errException = e;
		pState->errCallStackPointer = pState->vmpContext->vmCallStackPointer;
		pState->errDataStackPointer = pState->vmpContext->vmDataStackPointer;

		// do callback if exists
		pState->vmExceptionFlag = JILTrue;
		switch( e )
		{
			case JIL_VM_Software_Exception:
			{
				if( pState->vmSoftwareException )
					pState->vmSoftwareException( pState );
				e = pState->errException;	// reload exception to recognize if handler has cleared exception state
				break;
			}
			case JIL_VM_Trace_Exception:
			{
				if( pState->vmTraceException )
					pState->vmTraceException( pState );
				e = pState->errException;
				break;
			}
			case JIL_VM_Break_Exception:
			{
				if( pState->vmBreakException )
					pState->vmBreakException( pState );
				e = pState->errException;
				break;
			}
			case JIL_VM_Abort_Exception:
			{
				// ABORT EXCEPTION just quietly ends all script execution
				break;
			}
			default:
			{
				if( pState->vmMachineException )
					pState->vmMachineException( pState );
				e = pState->errException;
				break;
			}
		}
		pState->vmExceptionFlag = JILFalse;
	}
	return e;
}

//------------------------------------------------------------------------------
// JILPushStackFrame
//------------------------------------------------------------------------------

JILStackFrame* JILPushStackFrame(JILState* pState, JILStackFrame* pStackFrame)
{
	JILHandle* h;
	JILContext* c;
	// save context
	c = pStackFrame->ctx = pState->vmpContext;
	// save program counter
	pStackFrame->pc = c->vmProgramCounter;
	// save call stack pointer
	pStackFrame->cstp = c->vmCallStackPointer;
	// push r0 onto the stack
	h = c->vmppRegister[0];
	JILAddRef(h);
	c->vmppDataStack[--c->vmDataStackPointer] = h;
	// push r1 onto the stack
	h = c->vmppRegister[1];
	JILAddRef(h);
	c->vmppDataStack[--c->vmDataStackPointer] = h;
	// save data stack pointer
	pStackFrame->dstp = c->vmDataStackPointer;
	return pStackFrame;
}

//------------------------------------------------------------------------------
// JILPopStackFrame
//------------------------------------------------------------------------------

JILStackFrame* JILPopStackFrame(JILState* pState, JILStackFrame* pStackFrame)
{
	JILHandle* h;
	JILContext* c;
	c = pStackFrame->ctx;
	// unroll data stack
	while( c->vmDataStackPointer < pStackFrame->dstp )
	{
		h = c->vmppDataStack[c->vmDataStackPointer++];
		JILRelease(pState, h);
	}
	// pop r1 from the stack
	h = c->vmppDataStack[c->vmDataStackPointer++];
	JILRelease(pState, c->vmppRegister[1]);
	c->vmppRegister[1] = h;
	// pop r0 from the stack
	h = c->vmppDataStack[c->vmDataStackPointer++];
	JILRelease(pState, c->vmppRegister[0]);
	c->vmppRegister[0] = h;
	// restore call stack pointer
	c->vmCallStackPointer = pStackFrame->cstp;
	// restore program counter
	c->vmProgramCounter = pStackFrame->pc;
	// restore context
	pState->vmpContext = c;
	return pStackFrame;
}

//------------------------------------------------------------------------------
// JILAllocContext
//------------------------------------------------------------------------------

JILContext* JILAllocContext(JILState* pState, JILLong numArgs, JILLong funcAddr)
{
	JILLong i;
	JILChar* block;
	JILContext* pContext;

	// allocate memory in one big chunk
	block = (JILChar*) malloc(
		sizeof(JILContext) +
		pState->vmCallStackSize * sizeof(JILLong) +
		pState->vmDataStackSize * sizeof(JILHandle*)
	);

	// assign memory portions
	pContext = (JILContext*) block;
	block += sizeof(JILContext);
	pContext->vmpCallStack  = (JILLong*) block;
	block += pState->vmCallStackSize * sizeof(JILLong);
	pContext->vmppDataStack = (JILHandle**) block;

	pContext->vmProgramCounter = funcAddr;
	pContext->vmpYieldContext = NULL;

	// init stack pointers
	pContext->vmCallStackPointer = pState->vmCallStackSize;
	pContext->vmDataStackPointer = pState->vmDataStackSize;

	// set register window - registers are just a bank of values on the stack
	pContext->vmDataStackPointer -= kNumRegisters;
	pContext->vmppRegister = pContext->vmppDataStack + pContext->vmDataStackPointer;

	// init registers with null-handle
	pState->vmppHandles[0]->refCount += kNumRegisters;
	for( i = 0; i < kNumRegisters; i++ )
		pContext->vmppRegister[i] = pState->vmppHandles[0];

	// move r2 (__global) from root context to new context (create a weak reference)
	if( pState->vmpRootContext )
	{
		JILRelease(pState, pContext->vmppRegister[2]);
		pContext->vmppRegister[2] = JILCreateWeakRef(pState, pState->vmpRootContext->vmppRegister[2]);
	}

	// move arguments onto the new stack
	if( numArgs )
	{
		JILHandle** ppSrc;
		JILHandle** ppDst;
		pContext->vmDataStackPointer -= numArgs;
		ppSrc = pState->vmpContext->vmppDataStack + pState->vmpContext->vmDataStackPointer;
		ppDst = pContext->vmppDataStack + pContext->vmDataStackPointer;
		while( numArgs-- )
		{
			JILAddRef( *ppSrc );
			*ppDst++ = *ppSrc++;
		}
	}
	return pContext;
}

//------------------------------------------------------------------------------
// JILFreeContext
//------------------------------------------------------------------------------

void JILFreeContext(JILState* pState, JILContext* pContext)
{
	JILLong i;

	// release all handles still on the stack, this includes our register window on the stack
	for( i = pContext->vmDataStackPointer; i < pState->vmDataStackSize; i++ )
	{
		JILRelease(pState, pContext->vmppDataStack[i]);
		pContext->vmppDataStack[i] = NULL;
	}

	// free memory
	free(pContext);
}

//------------------------------------------------------------------------------
// JILMarkContext
//------------------------------------------------------------------------------

JILError JILMarkContext(JILState* pState, JILContext* pContext)
{
	JILError result = JIL_No_Exception;
	JILLong i;

	// mark all handles on the stack, including our register window
	for( i = pContext->vmDataStackPointer; i < pState->vmDataStackSize; i++ )
	{
		result = JILMarkHandle(pState, pContext->vmppDataStack[i]);
		if( result )
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILMarkDelegate
//------------------------------------------------------------------------------

JILError JILMarkDelegate(JILState* pState, JILDelegate* pDelegate)
{
	// mark 'this' reference in method delegate
	JILError result = JILMarkHandle(pState, pDelegate->pObject);
	if( pDelegate->pClosure != NULL && result == JIL_No_Exception )
	{
		// mark all handles in closure too!
		JILLong i;
		JILLong n = pDelegate->pClosure->stackSize;
		JILHandle** ppHandles = pDelegate->pClosure->ppStack;
		for( i = 0; i < n; i++ )
		{
			result = JILMarkHandle(pState, *ppHandles++);
			if( result )
				break;
		}
	}
	return result;
}


//------------------------------------------------------------------------------
// JILExecuteByteCode
//------------------------------------------------------------------------------

static JILLong JILExecuteByteCode(JILState* pState, JILContext* pContext, JILLong address, JILHandle* pObj)
{
	// check address
	if( address < 0 || address >= pState->vmpCodeSegment->usedSize )
		return JIL_VM_Invalid_Code_Address;
	// push "return to native" marker onto call stack
	pContext->vmpCallStack[--pContext->vmCallStackPointer] = kReturnToNative;
	// set new program counter
	pContext->vmProgramCounter = address;
	// move 'this' into r0
	if( pObj )
	{
		JILAddRef(pObj);
		JILRelease(pState, pContext->vmppRegister[0]);
		pContext->vmppRegister[0] = pObj;
	}
	return JILExecuteInfinite(pState, pContext);
}

//------------------------------------------------------------------------------
// JILInitNativeType
//------------------------------------------------------------------------------

static JILLong JILInitNativeType(JILState* pState, JILTypeInfo* pTypeInfo, JILLong type)
{
	JILLong err;
	JILLong authorVersion;
	JILTypeProc proc;
	const JILChar* pName;
	JILTypeListItem* pItem;

	// get class name
	pName = JILCStrGetString(pState, pTypeInfo->offsetName);

	// see if class name is registered
	pItem = JILGetNativeType( pState, pName );
	if( !pItem )
		return JIL_ERR_Undefined_Type;

	// check version numbers of native object desc against what the proc now returns
	// to detect if a type lib has changed and might be incompatible!
	proc = pItem->typeProc;
	authorVersion = CallNTLGetAuthorVersion(proc);
	if( (pTypeInfo->interfaceVersion > JILRevisionToLong(JIL_TYPE_INTERFACE_VERSION)) || (pTypeInfo->authorVersion > authorVersion) )
		return JIL_ERR_Incompatible_NTL;

	// update 'typeProc' member in native object desc
	pTypeInfo->typeProc = proc;

	// update 'pState' member in NTLInstance struct
	pTypeInfo->instance.pState = pState;

	// Notify the type lib that it is going to be used
	err = CallNTLInitialize(pTypeInfo);
	if( err )
		return err;

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILIsBaseType
//------------------------------------------------------------------------------

JILBool JILIsBaseType(JILState* ps, JILLong base, JILLong type)
{
	JILTypeInfo* pTypeInfo;
	if( type == base )
		return JILTrue;
	pTypeInfo = JILTypeInfoFromType(ps, type);
	while( pTypeInfo->base )
	{
		if( pTypeInfo->base == base )
			return JILTrue;
		pTypeInfo = JILTypeInfoFromType(ps, pTypeInfo->base);
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// JILRTCheck
//------------------------------------------------------------------------------

JILBool JILRTCheck(JILState* ps, JILLong type, JILHandle* pObj)
{
	if( pObj->type == type )
	{
		return JILFalse;
	}
	else if( pObj->type == type_null )
	{
		return JILFalse;
	}
	else if( pObj->type == type_delegate )
	{
		return (JILTypeInfoFromType(ps, type)->family != tf_delegate);
	}
	else
	{
		return !JILIsBaseType(ps, type, pObj->type);
	}
}

//------------------------------------------------------------------------------
// JILMarkDataHandles
//------------------------------------------------------------------------------

JILError JILMarkDataHandles(JILState* ps)
{
	JILError err = JIL_No_Exception;
	JILLong i, x;
	for( i = 0; i < ps->vmpDataSegment->usedSize; i++ )
	{
		x = ps->vmpDataSegment->pData[i].index;
		err = JILMarkHandle(ps, ps->vmppHandles[x]);
		if( err )
			break;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILExceptionCallGetError
//------------------------------------------------------------------------------

JILError JILExceptionCallGetError(JILState* ps, JILHandle* hException)
{
	JILError error;
	JILTypeInfo* pTypeInfo;
	JILStackFrame stackFrame;

	pTypeInfo = JILTypeInfoFromType(ps, hException->type);
	if (pTypeInfo->base != type_exception || pTypeInfo->family != tf_class)
		return JIL_No_Exception;

	// save current context
	JILPushStackFrame(ps, &stackFrame);
	// call getError()
	error = JILCallMethod(ps, hException, kInterfaceExceptionGetError);
	if( error == JIL_No_Exception )
	{
		JILHandle* pResult = stackFrame.ctx->vmppRegister[kReturnRegister];
		if( pResult->type == type_int )
			error = JILGetIntHandle(pResult)->l;
	}
	// restore context
	JILPopStackFrame(ps, &stackFrame);

	return error;
}

//------------------------------------------------------------------------------
// JILExceptionCallGetMessage
//------------------------------------------------------------------------------

JILHandle* JILExceptionCallGetMessage(JILState* ps, JILHandle* hException)
{
	JILHandle* pMessage = NULL;
	JILTypeInfo* pTypeInfo;
	JILStackFrame stackFrame;

	pTypeInfo = JILTypeInfoFromType(ps, hException->type);
	if (pTypeInfo->base != type_exception || pTypeInfo->family != tf_class)
		return NULL;

	// save current context
	JILPushStackFrame(ps, &stackFrame);
	// call getError()
	if( JILCallMethod(ps, hException, kInterfaceExceptionGetMessage) == JIL_No_Exception )
	{
		JILHandle* pResult = stackFrame.ctx->vmppRegister[kReturnRegister];
		if( pResult->type == type_string )
		{
			pMessage = pResult;
			JILAddRef(pMessage);
		}
	}
	// restore context
	JILPopStackFrame(ps, &stackFrame);

	return pMessage;
}

//------------------------------------------------------------------------------
// JILCallFactory
//------------------------------------------------------------------------------

JILError JILCallFactory(JILState* ps, JILArray* pArr, JILLong funcIndex)
{
	JILError err = JIL_No_Exception;
	JILLong i;
	JILHandle** ppR0;
	JILHandle* saveR0, *saveR1, *hObj;
	// save R0
	ppR0 = ps->vmpContext->vmppRegister;
	saveR0 = *ppR0;
	JILAddRef(saveR0);
	// save R1
	saveR1 = ppR0[1];
	JILAddRef(saveR1);
	// for each object in the array
	for( i = 0; i < pArr->size; i++ )
	{
		// set R0 to instance
		hObj = pArr->ppHandles[i];
		JILAddRef(hObj);
		JILRelease(ps, *ppR0);
		*ppR0 = hObj;
		// call constructor
		err = JILCallMethod(ps, hObj, funcIndex);
		if( err != JIL_No_Exception )
			break;
	}
	// restore R0
	JILAddRef(saveR0);
	JILRelease(ps, *ppR0);
	*ppR0 = saveR0;
	JILRelease(ps, saveR0);
	// restore R1
	JILAddRef(saveR1);
	JILRelease(ps, ppR0[1]);
	ppR0[1] = saveR1;
	JILRelease(ps, saveR1);
	return err;
}

//------------------------------------------------------------------------------
// JILDynamicConvert
//------------------------------------------------------------------------------

JILError JILDynamicConvert(JILState* ps, JILLong dType, JILHandle* sObj, JILHandle** ppOut)
{
	JILError err = JIL_No_Exception;
	JILChar buf[32];
	const JILChar* pStr = buf;
	JILTypeInfo* pti = NULL;

	// TODO: only conversions to 'string' supported at this time
	if( dType == type_string )
	{
		switch( sObj->type )
		{
			case type_null:
			{
				JILAddRef(sObj);
				*ppOut = sObj;
				break;
			}
			case type_int:
			{
				JILSnprintf(buf, 32, "%d", JILGetIntHandle(sObj)->l);
				goto use_buf;
			}
			case type_float:
			{
				JILSnprintf(buf, 32, "%.15g", JILGetFloatHandle(sObj)->f);
				goto use_buf;
			}
			case type_string:
			{
				JILAddRef(sObj);
				*ppOut = sObj;
				break;
			}
			default:
			{
				JILHandle* pH;
				JILStackFrame stackFrame;
				pti = JILTypeInfoFromType(ps, sObj->type);
				if( pti->methodInfo.tostr < 0 )
					goto use_typeinfo;
				JILPushStackFrame(ps, &stackFrame);
				err = JILCallMethod(ps, sObj, pti->methodInfo.tostr);
				if( err == JIL_No_Exception )
				{
					pH = stackFrame.ctx->vmppRegister[kReturnRegister];
					JILAddRef(pH);
					*ppOut = pH;
				}
				JILPopStackFrame(ps, &stackFrame);
				break;
			}
		}
	}
	return err;

use_typeinfo:
	pStr = JILCStrGetString(ps, pti->offsetName);

use_buf:
	{
		JILHandle* pResult = JILGetNewHandle(ps);
		JILString* pString = JILString_New(ps);
		pResult->type = type_string;
		JILGetStringHandle(pResult)->str = pString;
		JILString_Assign(pString, pStr);
		*ppOut = pResult;
	}
	return err;
}
