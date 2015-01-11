//------------------------------------------------------------------------------
// File: JILDebug.c                                         (c) 2003 jewe.org
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
//	Provides functions for debugging and tracing JIL bytecode, as well as
//	examining error information from within exception handlers.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jildebug.h"
#include "jilmachine.h"

//------------------------------------------------------------------------------
// JILGetErrException
//------------------------------------------------------------------------------

JILLong JILGetErrException(JILState* pState)
{
	return pState->errException;
}

//------------------------------------------------------------------------------
// JILGetErrDataStackPointer
//------------------------------------------------------------------------------

JILLong JILGetErrDataStackPointer(JILState* pState)
{
	return pState->errDataStackPointer;
}

//------------------------------------------------------------------------------
// JILGetErrCallStackPointer
//------------------------------------------------------------------------------

JILLong JILGetErrCallStackPointer(JILState* pState)
{
	return pState->errCallStackPointer;
}

//------------------------------------------------------------------------------
// JILGetErrProgramCounter
//------------------------------------------------------------------------------

JILLong JILGetErrProgramCounter(JILState* pState)
{
	return pState->errProgramCounter;
}

//------------------------------------------------------------------------------
// JILGetDataHandle
//------------------------------------------------------------------------------

JILError JILGetDataHandle(JILState* pState, JILLong hObject, JILHandle* pOutData)
{
	JILError result = JIL_ERR_Invalid_Handle_Index;
	JILDataHandle* pDH;
	if( hObject >= 0 && hObject < pState->vmpDataSegment->usedSize )
	{
		pDH = pState->vmpDataSegment->pData + hObject;
		pOutData->type = pDH->type;
		pOutData->flags = 0;
		pOutData->refCount = 1;
		memcpy(pOutData->data, pDH->data, sizeof(pDH->data));
		result = JIL_No_Exception;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILGetRuntimeHandle
//------------------------------------------------------------------------------

JILError JILGetRuntimeHandle(JILState* pState, JILLong hObject, JILHandle* pOutData)
{
	JILError result = JIL_ERR_Invalid_Handle_Index;
	// runtime handles are not in a linear block - but we can check if the handle
	// number is total out of space...
	if( hObject >= 0 && hObject < pState->vmMaxHandles )
	{
		// if the refCount is zero, don't return the handle, it's dead.
		if( pState->vmppHandles[hObject]->refCount > 0 )
		{
			*pOutData = *pState->vmppHandles[hObject];
			result = JIL_No_Exception;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILClearExceptionState
//------------------------------------------------------------------------------

void JILClearExceptionState(JILState* pState)
{
	pState->errException = JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILGetTraceFlag
//------------------------------------------------------------------------------

JILBool JILGetTraceFlag(JILState* pState)
{
	return pState->vmTraceFlag;
}

//------------------------------------------------------------------------------
// JILSetTraceFlag
//------------------------------------------------------------------------------

JILError JILSetTraceFlag(JILState* pState, JILBool flag)
{
	if( pState->vmVersion.BuildFlags & kTraceExceptionEnabled )
	{
		pState->vmTraceFlag = flag;
		return JIL_No_Exception;
	}
	return JIL_ERR_Trace_Not_Supported;
}
