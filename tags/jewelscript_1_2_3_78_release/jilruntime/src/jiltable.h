//------------------------------------------------------------------------------
// File: JILTable.h                                            (c) 2005 jewe.org
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
/// @file jiltable.h
/// A very simple and straightforward Hash Table for the JILRuntime.
/// The table also supports a "native mode", storing native void* pointers
/// instead of JILHandle pointers. The native mode table is used by the runtime
/// internally.
//------------------------------------------------------------------------------

#ifndef JILTABLE_H
#define JILTABLE_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// struct JILTable
//------------------------------------------------------------------------------

typedef void (*JILTableDestructor)(JILUnknown*);	// destructor callback for table data.

//------------------------------------------------------------------------------
// JILTable Functions
//------------------------------------------------------------------------------

BEGIN_JILEXTERN

JILTable*			JILTable_NewManaged			(JILState*);
JILTable*			JILTable_NewNativeUnmanaged	(JILState*);
JILTable*			JILTable_NewNativeManaged	(JILState*, JILTableDestructor);
void				JILTable_Delete				(JILTable*);
void				JILTable_Copy				(JILTable*, const JILTable*);			// kTableModeManaged only.
JILTable*			JILTable_DeepCopy			(const JILTable*);						// kTableModeManaged only.
JILError			JILTable_FromArray			(JILTable*, const JILArray*);			// kTableModeManaged only.
JILError			JILTable_FromList			(JILTable*, const JILList*);			// kTableModeManaged only.
JILError			JILTable_Enumerate			(JILTable*, JILHandle*, JILHandle*);	// kTableModeManaged only.
JILLong				JILTable_Cleanup			(JILTable* _this);
JILError			JILTable_ToArray			(JILTable* _this, JILArray* pArray);	// kTableModeManaged only.
JILError			JILTable_ToList				(JILTable* _this, JILList* pList);		// kTableModeManaged only.

JILUnknown*			JILTable_GetItem			(JILTable*, const JILChar*);
void				JILTable_SetItem			(JILTable*, const JILChar*, JILUnknown*);

//------------------------------------------------------------------------------
// JILTableProc
//------------------------------------------------------------------------------

JILError JILTableProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

END_JILEXTERN

#endif	// JILTABLE_H
