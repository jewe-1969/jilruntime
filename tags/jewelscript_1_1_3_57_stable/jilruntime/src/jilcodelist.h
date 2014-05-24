//------------------------------------------------------------------------------
// File: JILCodeList.h                                         (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description: 
// ------------
/// @file jilcodelist.h
/// Provides functions to generate an ASCII text listing out of JIL bytecode.
//------------------------------------------------------------------------------

#ifndef JILCODELIST_H
#define JILCODELIST_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILGetInstructionSize
//------------------------------------------------------------------------------
/// Get the size, in instruction words of a complete instruction (including
/// data for operands, etc.). If the given opcode is undefined, 0 will be
/// returned.

JILEXTERN JILLong				JILGetInstructionSize	(JILLong opcode);

//------------------------------------------------------------------------------
// JILGetOperandSize
//------------------------------------------------------------------------------
/// Get the size, in instruction words of an operand of the specified type.
/// See enum JILOperandType in jiltypes.h for predefined operand types.
/// You can obtain the operand type(s) of an instruction by examining the
/// JILInstrInfo structure returned by JILGetInstructionInfo() and
/// JILGetInfoFromOpcode().

JILEXTERN JILLong				JILGetOperandSize		(JILLong operandType);

//------------------------------------------------------------------------------
// JILGetInstructionIndex
//------------------------------------------------------------------------------
/// Find an entry in the instruction table by the given name of the mnemonic.
/// This can be used by a virtual machine assembler program.

JILEXTERN JILLong				JILGetInstructionIndex	(const JILChar* pName, JILLong startIndex);

//------------------------------------------------------------------------------
// JILGetInstructionInfo
//------------------------------------------------------------------------------
/// Get the instruction info from the instruction table by the given table index.

JILEXTERN const JILInstrInfo*	JILGetInstructionInfo	(JILLong index);

//------------------------------------------------------------------------------
// JILGetInstructionInfo
//------------------------------------------------------------------------------
/// Find the instruction info from the instruction table by the given virtual
/// machine instruction opcode.

JILEXTERN const JILInstrInfo*	JILGetInfoFromOpcode	(JILLong opcode);

//------------------------------------------------------------------------------
// JILGetHandleTypeName
//------------------------------------------------------------------------------
/// Returns a string constant for the given handle type.
/// See also enum JILHandleType in file "jilapitypes.h".

JILEXTERN const JILChar*		JILGetHandleTypeName	(JILState* pState, JILLong type);

//------------------------------------------------------------------------------
// JILListCode
//------------------------------------------------------------------------------
/// Outputs multiple lines of virtual machine instructions to given stream.

JILEXTERN void					JILListCode				(JILState* pState, JILLong from, JILLong to, JILLong extInfo, JILFILE* stream);

//------------------------------------------------------------------------------
// JILListCallStack
//------------------------------------------------------------------------------
/// Outputs function names on the callstack to the given stream. 'maxTraceback'
/// specifies the maximum number of nested calls to print out. This function
/// relies heavily on the symbol table and will not work if no symbol table
/// is present for the current program.
/// WARNING: This function is very costly in terms of CPU usage, so it should
/// only be called for debugging purposes!

JILEXTERN void					JILListCallStack		(JILState* pState, JILLong maxTraceback, JILFILE* stream);

//------------------------------------------------------------------------------
// JILListInstruction
//------------------------------------------------------------------------------
/// Creates a string from the instruction at the given address. The parameter
/// 'maxLength' specifies the maximum number of bytes the function is allowed to
/// write into 'pString'. Return value is the size of the listed instruction in
/// instruction words or 0 if an error occurred.

JILEXTERN JILLong				JILListInstruction		(JILState* pState, JILLong address, JILChar *pString, JILLong maxLength, JILLong extInfo);

//------------------------------------------------------------------------------
// JILListHandle
//------------------------------------------------------------------------------
/// Creates a string representation from the specified handle index,
/// as well as a comment string containing more information about the handle.
/// The parameters 'maxString' and 'maxComment' specify how many bytes the
/// function is allowed to write into 'pString' and 'pComment'.

JILEXTERN JILLong				JILListHandle			(JILState* pState, JILLong hObj, JILChar* pString, JILLong maxString, JILChar* pComment, JILLong maxComment, JILLong flags);

//------------------------------------------------------------------------------
// JILGetFunctionName
//------------------------------------------------------------------------------
/// Seaches the function segment for a function that starts at the given code
/// address and writes the function name to the given string buffer. If a
/// function is found that starts at the given code address, the return value
/// is 1 and the string buffer contains the function name, otherwise it is 0
/// and the string buffer state is undefined.
/// The parameter 'maxLength' specifies the maximum amount of bytes the function
/// is allowed to write into 'pString'.

JILEXTERN JILLong				JILGetFunctionName		(JILState* pState, JILChar* pString, JILLong maxLength, JILLong codeAddr);

#endif	// #ifndef JILCODELIST_H
