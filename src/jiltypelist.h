//------------------------------------------------------------------------------
// File: JILTypeList.h                                      (c) 2003 jewe.org
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
/// @file jiltypelist.h
///	Manages a list of class names and callback procs for native type libs.
///	Also provides functions to register a type library and unregister all
///	type libs.
//------------------------------------------------------------------------------

#ifndef JILTYPELIST_H
#define JILTYPELIST_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------

JILError				JILInitTypeList				(JILState* pState, long size);
JILError				JILDestroyTypeList			(JILState* pState);
JILError				JILCheckClassName			(JILState* pState, const char* pClassName);
JILTypeListItem*		JILNewNativeType			(JILState* pState, const char* pClassName, JILTypeProc proc);
JILTypeListItem*		JILGetNativeType			(JILState* pState, const char* pClassName);
JILError				JILRegisterNativeType		(JILState* pState, JILTypeProc proc);
JILError				JILUnregisterNativeTypes	(JILState* pState);

#endif	// #ifndef JILTYPELIST_H
