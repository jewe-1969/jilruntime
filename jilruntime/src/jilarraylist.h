//------------------------------------------------------------------------------
// File: JILArrayList.h                                        (c) 2014 jewe.org
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
/// @file jilarraylist.h
/// A combination of linked list and array. This class is a compromise between
/// the flexibility of a list and the efficiency of an array.
//------------------------------------------------------------------------------

#ifndef JILARRAYLIST_H
#define JILARRAYLIST_H

#include "jilstdinc.h"
#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILArrayListDestructor
//------------------------------------------------------------------------------

typedef void (*JILArrayListDestructor)(JILState*, JILUnknown*);	// destructor callback for list data

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

BEGIN_JILEXTERN

JILArrayList*		JILArrayList_New				(JILState*, JILArrayListDestructor);
void				JILArrayList_Delete				(JILArrayList*);
void				JILArrayList_Copy				(JILArrayList*, const JILArrayList*);		// managed mode only
JILArrayList*		JILArrayList_DeepCopy			(const JILArrayList*);						// managed mode only
JILError			JILArrayList_FromArray			(JILArrayList*, const JILArray*);			// managed mode only
JILError			JILArrayList_ToArray			(JILArrayList*, JILArray*);					// managed mode only
JILError			JILArrayList_Enumerate			(JILArrayList*, JILHandle*, JILHandle*);	// managed mode only

JILUnknown*			JILArrayList_GetItem			(JILArrayList*, JILLong);
JILBool				JILArrayList_SetItem			(JILArrayList*, JILLong, JILUnknown*);
void				JILArrayList_AddItem			(JILArrayList*, JILUnknown*);
void				JILArrayList_RemoveItem			(JILArrayList*, JILLong);
void				JILArrayList_InsertItem			(JILArrayList*, JILLong, JILUnknown*);
JILLong				JILArrayList_Count				(JILArrayList*);

JILError			JILArrayList_Mark				(JILArrayList*);

//------------------------------------------------------------------------------
// Standard destructors
//------------------------------------------------------------------------------
// Pass one of these to JILArrayList_New(), or use your own callback.

void JILArrayListNone(JILState*, JILUnknown*);		//!< Do not destroy anything. You can pass this to JILArrayList_New() or use your own callback
void JILArrayListFree(JILState*, JILUnknown*);		//!< Use free() to destroy the data. You can pass this to JILArrayList_New()
void JILArrayListRelease(JILState*, JILUnknown*);	//!< Use JILRelease() to destroy the data. You can pass this to JILArrayList_New()

//------------------------------------------------------------------------------
// JILArrayListProc
//------------------------------------------------------------------------------

JILError JILArrayListProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

END_JILEXTERN

#endif	// JILARRAYLIST_H
