//------------------------------------------------------------------------------
// File: JILDebug.h                                         (c) 2003 jewe.org
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
/// @file jildebug.h
///	Provides definitions for debugging and tracing JIL bytecode, as well as
///	examining error information from within exception handlers.
//------------------------------------------------------------------------------

#ifndef JILDEBUG_H
#define JILDEBUG_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JIL_INSERT_DEBUG_CODE
//------------------------------------------------------------------------------
/// @def JIL_INSERT_DEBUG_CODE
/// Macro that inserts code that does additional safety checks and should not be
/// present in final release builds for maximum performance.

#if JIL_RUNTIME_CHECKS
	#define		JIL_INSERT_DEBUG_CODE( _S_ )	_S_
#else
	#define		JIL_INSERT_DEBUG_CODE( _S_ )
#endif

//------------------------------------------------------------------------------
// JILGetErrException
//------------------------------------------------------------------------------
/// Returns the most recently invoked exception number.

JILEXTERN JILLong		JILGetErrException			(JILState* pState);

//------------------------------------------------------------------------------
// JILGetErrDataStackPointer
//------------------------------------------------------------------------------
/// Returns the data stack pointer as it was saved when the last exception
/// was invoked.

JILEXTERN JILLong		JILGetErrDataStackPointer	(JILState* pState);

//------------------------------------------------------------------------------
// JILGetErrCallStackPointer
//------------------------------------------------------------------------------
/// Returns the call stack pointer as it was saved when the last exception
/// was invoked.

JILEXTERN JILLong		JILGetErrCallStackPointer	(JILState* pState);

//------------------------------------------------------------------------------
// JILGetErrProgramCounter
//------------------------------------------------------------------------------
/// Returns the program counter as it was saved when the last exception
/// was invoked.

JILEXTERN JILLong		JILGetErrProgramCounter		(JILState* pState);

//------------------------------------------------------------------------------
// JILGetDataHandle
//------------------------------------------------------------------------------
/// Copies the data of a handle from the data segment. This is only intended for
/// monitor or debugger programs that want to display the contents of a handle.

JILEXTERN JILError		JILGetDataHandle			(JILState* pState, JILLong hObject, JILHandle* pOutData);

//------------------------------------------------------------------------------
// JILGetRuntimeHandle
//------------------------------------------------------------------------------
/// Copies the data of a runtime handle. This is only intended for monitor or
/// debugger programs that want to display the contents of a handle.

JILEXTERN JILError		JILGetRuntimeHandle			(JILState* pState, JILLong hObject, JILHandle* pOutData);

//------------------------------------------------------------------------------
// JILClearExceptionState
//------------------------------------------------------------------------------
/// This can be called from an exception handler to clear the exception state of
/// the virtual machine. If an exception handler returns and the exception state
/// is cleared, the virtual machine assumes the error has been fixed and
/// continues execution. If the exception state is not cleared, the virtual
/// machine aborts execution and exits with an error code.
/// If an exception handler causes an exception, the virtual machine aborts
/// execution immediately and exits with an error code.

JILEXTERN void			JILClearExceptionState		(JILState* pState);

//------------------------------------------------------------------------------
// JILGetTraceFlag
//------------------------------------------------------------------------------
/// This can be called to find out whether the trace mode is currently enabled
/// or not.

JILEXTERN JILBool		JILGetTraceFlag				(JILState* pState);

//------------------------------------------------------------------------------
// JILSetTraceFlag
//------------------------------------------------------------------------------
/// Enable or disable TRACE exception. This usually only works in <b>debug</b>
/// builds of the JILRuntime, because it decreases performance even when tracing
/// is disabled. You can set the macro JIL_TRACE_RELEASE to 1 if you want to
/// enable the TRACE exception also for release builds.

JILEXTERN JILError		JILSetTraceFlag				(JILState* pState, JILBool flag);

#endif	// #ifndef JILDEBUG_H
