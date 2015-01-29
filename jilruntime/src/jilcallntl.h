//------------------------------------------------------------------------------
// File: jilcallntl.h                                          (c) 2005 jewe.org
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
/// @file jilcallntl.h
/// This is the "interface" between the virtual machine and a native type
/// library (NTL). The inline functions here wrap calls to the main entry point
/// function of the NTL, the type proc.
/// This file is a useful reference, if you are writing a native type library
/// and are unsure what parameters your type proc will receive for a specific
/// message.
//------------------------------------------------------------------------------

#ifndef JILCALLNTL_H
#define JILCALLNTL_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// TO_INSTANCE
//------------------------------------------------------------------------------

#define TO_INSTANCE(X) (&(X->instance))

//------------------------------------------------------------------------------
// runtime messages
//------------------------------------------------------------------------------

JILINLINE JILError CallNTLRegister(JILTypeProc proc, JILLong interfaceVersion, JILState* pState)
{
	return proc(NULL, NTL_Register, interfaceVersion, pState, NULL);
}

JILINLINE JILError CallNTLOnImport(JILTypeProc proc, JILState* pState)
{
	return proc(NULL, NTL_OnImport, 0, pState, NULL);
}

JILINLINE JILError CallNTLInitialize(JILTypeInfo* pti)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_Initialize, 0, NULL, NULL);
}

JILINLINE JILError CallNTLNewObject(JILTypeInfo* pti, JILUnknown** ppOut)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_NewObject, 0, NULL, ppOut);
}

JILINLINE JILError CallNTLMarkHandles(JILTypeInfo* pti, JILUnknown* pObj)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_MarkHandles, 0, pObj, NULL);
}

JILINLINE JILError CallNTLCallStatic(JILTypeInfo* pti, JILLong funcIdx)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_CallStatic, funcIdx, NULL, NULL);
}

JILINLINE JILError CallNTLCallMember(JILTypeInfo* pti, JILLong funcIdx, JILUnknown* pObj)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_CallMember, funcIdx, pObj, NULL);
}

JILINLINE JILError CallNTLDestroyObject(JILTypeInfo* pti, JILUnknown* pObj)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_DestroyObject, 0, pObj, NULL);
}

JILINLINE JILError CallNTLTerminate(JILTypeInfo* pti)
{
	return pti->typeProc(TO_INSTANCE(pti), NTL_Terminate, 0, NULL, NULL);
}

JILINLINE JILError CallNTLUnregister(JILTypeProc proc, JILState* pState)
{
	return proc(NULL, NTL_Unregister, 0, pState, NULL);
}

//------------------------------------------------------------------------------
// class information queries
//------------------------------------------------------------------------------
// NOTE that these functions do NOT get passed an NTLInstance pointer!

JILINLINE JILError CallNTLGetClassName(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetClassName, 0, NULL, (void**) ppOut);
}

JILINLINE JILError CallNTLGetBaseName(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetBaseName, 0, NULL, (void**) ppOut);
}

JILINLINE JILLong CallNTLGetInterfaceVersion(JILTypeProc proc)
{
	return proc(NULL, NTL_GetInterfaceVersion, 0, NULL, NULL);
}

JILINLINE JILLong CallNTLGetAuthorVersion(JILTypeProc proc)
{
	return proc(NULL, NTL_GetAuthorVersion, 0, NULL, NULL);
}

JILINLINE JILError CallNTLGetBuildTimeStamp(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetBuildTimeStamp, 0, NULL, (void**) ppOut);
}

JILINLINE JILError CallNTLGetAuthorName(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetAuthorName, 0, NULL, (void**) ppOut);
}

JILINLINE JILError CallNTLGetAuthorString(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetAuthorString, 0, NULL, (void**) ppOut);
}

JILINLINE JILError CallNTLGetDeclString(JILTypeProc proc, void* pIn, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetDeclString, 0, pIn, (void**) ppOut);
}

JILINLINE JILError CallNTLGetPackageString(JILTypeProc proc, const JILChar** ppOut)
{
	return proc(NULL, NTL_GetPackageString, 0, NULL, (void**) ppOut);
}

#endif	// #ifdef JILCALLNTL_H
