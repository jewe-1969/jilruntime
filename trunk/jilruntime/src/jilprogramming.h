//------------------------------------------------------------------------------
// File: JILProgramming.h                                   (c) 2003 jewe.org
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
/// @file jilprogramming.h
/// Provides functions to program the virtual machine, such as functions to
/// create global data, build class descriptors, read and write program memory
/// and so forth.
//------------------------------------------------------------------------------

#ifndef JILPROGRAMMING_H
#define JILPROGRAMMING_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILCreateLong
//------------------------------------------------------------------------------
/// Create a long handle in the data segment. These handles can be used as global
/// constants or variables. The result is the index of the handle in the data segment.

JILEXTERN JILError		JILCreateLong			(JILState* pState, JILLong value, JILLong* hLong);

//------------------------------------------------------------------------------
// JILCreateFloat
//------------------------------------------------------------------------------
/// Create a float handle in the data segment. These handles can be used as global
/// constants or variables. The result is the index of the handle in the data segment.

JILEXTERN JILError		JILCreateFloat			(JILState* pState, JILFloat value, JILLong* hFloat);

//------------------------------------------------------------------------------
// JILCreateString
//------------------------------------------------------------------------------
/// Create a string handle in the data segment. These handles can be used as global
/// constants or variables. The result is the index of the handle in the data segment.

JILEXTERN JILError		JILCreateString			(JILState* pState, const JILChar* pStr, JILLong* hString);

//------------------------------------------------------------------------------
// JILCreateFunction
//------------------------------------------------------------------------------
/// Create a function entry in the function segment.
/// The result is the index of the function.

JILEXTERN JILError		JILCreateFunction		(JILState* pState, JILLong type, JILLong memberIdx, JILLong flags, const JILChar* pName, JILLong* pFuncIndex);

//------------------------------------------------------------------------------
// JILGetFunctionByName
//------------------------------------------------------------------------------
/// Search a function entry in function segment by function name and typeID.
/// Return value is the function index and a pointer to its JILFuncInfo struct
/// is written to ppOut.

JILEXTERN JILLong		JILGetFunctionByName	(JILState* pState, JILLong type, const JILChar* pName, JILFuncInfo** ppOut);

//------------------------------------------------------------------------------
// JILGetFunctionByAddr
//------------------------------------------------------------------------------
/// Search a function entry in function segment by code address. The given
/// address does not have to be the exact start-address of a function. Any
/// address that lies within a function's body will return the corresponding
/// function.
/// Return value is the function index and a pointer to its JILFuncInfo struct
/// is written to ppOut.

JILEXTERN JILLong		JILGetFunctionByAddr	(JILState* pState, JILLong addr, JILFuncInfo** ppOut);

//------------------------------------------------------------------------------
// JILGetFunctionByIndex
//------------------------------------------------------------------------------
/// Search a function entry in function segment by it's typeID and member index.
/// Return value is the function index and a pointer to its JILFuncInfo struct
/// is written to ppOut.

JILEXTERN JILLong		JILGetFunctionByIndex	(JILState* pState, JILLong type, JILLong index, JILFuncInfo** ppOut);

//------------------------------------------------------------------------------
// JILSetFunctionAddress
//------------------------------------------------------------------------------
/// This can be called to change the code address of an already existing function.
/// Pass the function index returned by JILCreateFunction() and the new address.
/// The address is a zero-based offset into the code segment, counted in
/// instruction words (an instruction word is an int).

JILEXTERN JILError		JILSetFunctionAddress	(JILState* pState, JILLong funcIndex, JILLong address, JILLong size);

//------------------------------------------------------------------------------
// JILCreateType
//------------------------------------------------------------------------------
/// Creates an entry for a new type in the global TypeInfo segment and returns
/// the type identifier number by writing it to pType. If the name specified was
/// already taken or is invalid for any other reason, the function will return
/// an error code and write 0 to pType, which corresponds to type_null.
/// When argument bNative is JILTrue, a native type with the name specified in
/// pName needs to be registered to the JIL runtime for this function to
/// succeed.

JILEXTERN JILError		JILCreateType			(JILState* pState, const JILChar* pName, JILTypeFamily family, JILBool bNative, JILLong* pType);

//------------------------------------------------------------------------------
// JILSetClassInstanceSize
//------------------------------------------------------------------------------
/// This updates the type info struct of a previously allocated type with a new
/// instance size. This is intended for the compiler to update the type info of
/// a class written in script code with the size of an instance, once that it is
/// known.

JILEXTERN JILError		JILSetClassInstanceSize	(JILState* pState, JILLong type, JILLong instanceSize);

//------------------------------------------------------------------------------
// JILSetClassVTable
//------------------------------------------------------------------------------
/// This updates the type info struct of a previously allocated type with a
/// v-table. The v-table is basically an array of longs, which are indexes
/// of function handles.

JILEXTERN JILError		JILSetClassVTable		(JILState* pState, JILLong type, JILLong size, JILLong* pVtab);

//------------------------------------------------------------------------------
// JILSetClassMethodInfo
//------------------------------------------------------------------------------
/// This sets the method info struct of the given type. The method info contains
/// the method indices of certain methods the runtime needs to know, for example
/// the type's constructor, copy constructor and destructor.

JILEXTERN JILError		JILSetClassMethodInfo	(JILState* pState, JILLong type, JILMethodInfo* pInfo);

//------------------------------------------------------------------------------
// JILSetGlobalObjectSize
//------------------------------------------------------------------------------
/// Called from the compiler to set / update the size of the global object.

JILEXTERN JILError		JILSetGlobalObjectSize	(JILState* pState, JILLong type, JILLong size);

//------------------------------------------------------------------------------
// JILSetMemory
//------------------------------------------------------------------------------
/// Write a data block to a specific address in the code segment. 'size' and
/// 'address' are counted in instruction words, e. g. address 2 is the 3rd
/// instruction word in the segment. If the given address is outside of the
/// current size of the code segment, it will be resized.

JILEXTERN JILError		JILSetMemory			(JILState* pState, JILLong address, const JILLong* pData, JILLong size);

//------------------------------------------------------------------------------
// JILGetMemory
//------------------------------------------------------------------------------
/// Read a data block from a specific address in the code segment. 'size' and
/// 'address' are counted in instruction words. If 'address' is outside the
/// current size of the code segment, a JIL_OutOfCodeSegment error will be
/// returned.

JILEXTERN JILError		JILGetMemory			(JILState* pState, JILLong address, JILLong* pData, JILLong size);

//------------------------------------------------------------------------------
// JILCreateRestorePoint
//------------------------------------------------------------------------------
/// This takes a "snapshot" of the virtual machine state and allows later to
/// return the virtual machine back to this snapshot. This can be used to "undo"
/// changes made to the virtual machine while programming it. (For example undo
/// creation of code, handles, and data.)
/// Note that this function DOES NOT save a complete state of the virtual machine,
/// so it is not suitable to suspend to disk / resume a running VM program.

JILEXTERN void			JILCreateRestorePoint	(JILState* pState, JILRestorePoint* pRP);

//------------------------------------------------------------------------------
// JILGotoRestorePoint
//------------------------------------------------------------------------------
/// This returns the virtual machine to a previously created restore point.
/// @see JILCreateRestorePoint

JILEXTERN void			JILGotoRestorePoint		(JILState* pState, JILRestorePoint* pRP);

//------------------------------------------------------------------------------
// JILGetCodeLength
//------------------------------------------------------------------------------
/// This returns the currently used size of the code-segment in instruction
/// words.

JILEXTERN JILLong		JILGetCodeLength		(JILState* pState);

#endif	// #ifndef JILPROGRAMMING_H
