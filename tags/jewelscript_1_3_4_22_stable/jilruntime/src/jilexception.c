//------------------------------------------------------------------------------
// File: JILException.c                                     (c) 2003 jewe.org
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
//	Functions and data related to error codes and virtual machine exceptions.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilexception.h"
#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILExceptionStrings
//------------------------------------------------------------------------------
// This table defines all the error and exception strings.

const JILExceptionInfo JILExceptionStrings[JIL_Num_Exception_Strings] =
{
	JIL_No_Exception,					"No exception",

	// virtual machine exceptions			
	JIL_VM_Illegal_Instruction,			"ILLEGAL INSTRUCTION EXCEPTION",
	JIL_VM_Stack_Overflow,				"STACK OVERFLOW EXCEPTION",
	JIL_VM_Null_Reference,				"NULL REFERENCE EXCEPTION",
	JIL_VM_Unsupported_Type,			"UNSUPPORTED TYPE EXCEPTION",
	JIL_VM_Type_Mismatch,				"TYPE MISMATCH EXCEPTION",
	JIL_VM_Call_To_NonFunction,			"CALL TO NON-FUNCTION EXCEPTION",
	JIL_VM_Invalid_Operand,				"INVALID OPERAND EXCEPTION",
	JIL_VM_Division_By_Zero,			"DIVISION BY ZERO EXCEPTION",
	JIL_VM_Software_Exception,			"SOFTWARE EXCEPTION",
	JIL_VM_Trace_Exception,				"TRACE EXCEPTION",
	JIL_VM_Break_Exception,				"BREAK EXCEPTION",
	JIL_VM_Unhandled_Exception,			"UNHANDLED EXCEPTION",
	JIL_VM_Allocation_Failed,			"ALLOCATION FAIL EXCEPTION",
	JIL_VM_Invalid_Code_Address,		"INVALID CODE ADDRESS EXCEPTION",
	JIL_VM_Return_To_Native,			"RETURN TO NATIVE EXCEPTION",
	JIL_VM_Object_Copy_Exception,		"OBJECT COPY FAIL EXCEPTION",
	JIL_VM_Abort_Exception,				"ABORT EXCEPTION",
	JIL_VM_Native_CPP_Exception,		"NATIVE C++ EXCEPTION",

	// library error codes
	JIL_ERR_Generic_Error,				"Unspecified error",
	JIL_ERR_Illegal_Argument,			"Illegal function argument",
	JIL_ERR_Out_Of_Code,				"Out of code",
	JIL_ERR_Illegal_Type_Name,			"Illegal native type name",
	JIL_ERR_Register_Type_Failed,		"Register native type failed",
	JIL_ERR_Undefined_Type,				"Undefined native type",
	JIL_ERR_Unsupported_Native_Call,	"Native type doesn't support this feature or function",
	JIL_ERR_Invalid_Vector,				"Invalid exception vector",
	JIL_ERR_Invalid_Handle_Index,		"Invalid handle index",
	JIL_ERR_Invalid_Handle_Type,		"Invalid handle type",
	JIL_ERR_Invalid_Member_Index,		"Invalid member index",
	JIL_ERR_Invalid_Function_Index,		"Invalid function index",
	JIL_ERR_Invalid_Register,			"Invalid register number",
	JIL_ERR_Call_To_NonFunction,		"Call to non-function",
	JIL_ERR_Runtime_Locked,				"Runtime already initialized, the requested operation cannot be performed",
	JIL_ERR_Save_Chunk_Failed,			"Save chunk failed",
	JIL_ERR_Load_Chunk_Failed,			"Load chunk failed",
	JIL_ERR_No_Symbol_Table,			"No symbol table",
	JIL_ERR_Symbol_Exists,				"Symbol table entry already exists",
	JIL_ERR_Symbol_Not_Found,			"Symbol table entry not found",
	JIL_ERR_Incompatible_NTL,			"Incompatible native type",
	JIL_ERR_Detected_Memory_Leaks,		"Detected runtime memory leaks",
	JIL_ERR_Trace_Not_Supported,		"Trace exception not supported",
	JIL_ERR_Runtime_Blocked,			"Runtime blocked, cannot perform call to script function",
	JIL_ERR_Code_Not_Initialized,		"Code not initialized, call JILRun() first",
	JIL_ERR_Initialize_Failed,			"Initialize failed",
	JIL_ERR_No_Compiler,				"No compiler present, JCLFreeCompiler() was called",
	JIL_ERR_File_Open,					"File open failed",
	JIL_ERR_File_End,					"End of file reached",
	JIL_ERR_File_Generic,				"General file input error",
	JIL_ERR_Mark_Handle_Error,			"GC Mark handle error",

	// leave this at the end of the table
	JIL_Unknown_Exception,				"JIL UNKNOWN EXCEPTION"
};
