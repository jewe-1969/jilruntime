//------------------------------------------------------------------------------
// File: JILAllocators.h                                    (c) 2003 jewe.org
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
/// @file jilallocators.h
/// Several allocator functions for example for objects, arrays and strings.
/// These functions are called during runtime by the virtual machine, for
/// example by the alloc and alloca instructions.
//------------------------------------------------------------------------------

#ifndef JILALLOCATORS_H
#define JILALLOCATORS_H

#include "jilstring.h"
#include "jilarray.h"

//------------------------------------------------------------------------------
// JILAllocObject
//------------------------------------------------------------------------------
/// Allocates a JIL object of a given size.

JILHandle**			JILAllocObject			(JILState* pState, JILLong size);

//------------------------------------------------------------------------------
// JILAllocObjectNoInit
//------------------------------------------------------------------------------
/// Allocates a JIL object without initializing it with null handles. This is
/// only for cases where the object will get initialized immediately after this
/// function returns. (For example in CopyHandle())

JILHandle**			JILAllocObjectNoInit	(JILState* pState, JILLong size);

//------------------------------------------------------------------------------
// JILAllocArrayMulti
//------------------------------------------------------------------------------
/// Allocates a (multi-dimensional) JIL array of a given number of dimensions.
/// ATTENTION: This function relies on arguments on the virtual machine's stack,
/// so do not call this unless you know exactly how to use this function!!!

JILArray*			JILAllocArrayMulti		(JILState* pState, JILLong type, JILLong dim, JILLong n);

//------------------------------------------------------------------------------
// JILAllocString
//------------------------------------------------------------------------------
/// Allocates a JIL string from a given string constant.

JILString*			JILAllocString			(JILState* pState, const JILChar* pString);

//------------------------------------------------------------------------------
// JILAllocStringFromCStr
//------------------------------------------------------------------------------
/// Allocates a JIL string from a given CStr offset.

JILString*			JILAllocStringFromCStr	(JILState* pState, JILLong offsetString);

//------------------------------------------------------------------------------
// JILAllocDelegate
//------------------------------------------------------------------------------
/// Allocates a JIL delegate object.

JILDelegate*		JILAllocDelegate		(JILState* pState, JILLong index, JILHandle* pObject);

//------------------------------------------------------------------------------
// JILAllocClosure
//------------------------------------------------------------------------------
/// Allocates a JIL closure object.

JILDelegate*		JILAllocClosure			(JILState* pState, JILLong stackSize, JILLong addr, JILHandle* pObject);

//------------------------------------------------------------------------------
// JILAllocDelegate
//------------------------------------------------------------------------------
/// Frees a JIL delegate object.

void				JILFreeDelegate			(JILState* pState, JILDelegate* pDelegate);

//------------------------------------------------------------------------------
// JILAllocFactory
//------------------------------------------------------------------------------
/// Allocates an object factory, which is an array of instances of implementors
/// of a given interface ID.

JILArray*			JILAllocFactory			(JILState* pState, JILLong interfaceID);

#endif	// #ifndef JILALLOCATORS_H
