//------------------------------------------------------------------------------
// File: JILTypeInfo.h                                   (c) 2005 jewe.org
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
/// @file jiltypeinfo.h
/// Provides functions to init, maintain and destroy the typeinfo segment.
/// (( TODO: insert further documentation here ))
//------------------------------------------------------------------------------

#ifndef JILTYPEINFO_H
#define JILTYPEINFO_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILInitTypeInfoSegment
//------------------------------------------------------------------------------
/// Intializes the TypeInfo segment. Called from Initialize.

JILError			JILInitTypeInfoSegment		(JILState* pState, JILLong initialSize);

//------------------------------------------------------------------------------
// JILNewTypeInfo
//------------------------------------------------------------------------------
/// Creates a new TypeInfo entry in the TypeInfo segment. If there is no more
/// space in the segment, it will be automatically resized. If a name is
/// specified, it will be written to the Cstr segment and an offset to it is
/// stored in the TypeInfo struct. If the name does not yet exist in the whole
/// segment, a new TypeInfo will be added. However, if the name does already
/// exist, the existing TypeInfo will be returned instead of adding a new entry.
/// NULL can be specified if the type is not a class, in which case the offset
/// to the type name will remain 0. This is reserved for the built-in types
/// though and should not be used for classes. The result is the index of the
/// TypeInfo in the TypeInfo segment. Additionally, the absolute address of the
/// TypeInfo struct is written to 'ppOut'.

JILLong				JILNewTypeInfo				(JILState* pState, const JILChar* pName, JILTypeInfo** ppOut);

//------------------------------------------------------------------------------
// JILFindTypeInfo
//------------------------------------------------------------------------------
/// Searches the TypeInfo segment for a type with a specific name. Returns the
/// type identifier number of the type and a pointer to the JILTypeInfo struct
/// in ppOut. If not found, returns 0 (type_null) and a NULL pointer.

JILLong				JILFindTypeInfo				(JILState* pState, const JILChar* pName, JILTypeInfo** ppOut);

//------------------------------------------------------------------------------
// JILDestroyTypeInfoSegment
//------------------------------------------------------------------------------
/// Destroys the TypeInfo segment. Called from Terminate.

JILError			JILDestroyTypeInfoSegment	(JILState* pState);

#endif	// #ifndef JILTYPEINFO_H
