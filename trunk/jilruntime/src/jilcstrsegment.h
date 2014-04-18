//------------------------------------------------------------------------------
// File: JILCStrSegment.h                                   (c) 2003 jewe.org
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
/// @file jilcstrsegment.h
/// The global CStr segment has the purpose to store raw-data, such as c-string
/// constants or other binary data, that may referenced by a handle. This file
/// provides functions to initialize and destroy the CStr segment, as well as
/// adding data to the CStr segment.
//------------------------------------------------------------------------------

#ifndef JILCSTRSEGMENT_H
#define JILCSTRSEGMENT_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILInitCStrSegment
//------------------------------------------------------------------------------
// Allocates and initializes an empty cstr segment. Called from Initialize.
// The allocGrain specifies the number of BYTES added to the size of the
// segment, when it has to be resized.

JILError			JILInitCStrSegment		(JILState* pState, JILLong initialSize);

//------------------------------------------------------------------------------
// JILAddCStrData
//------------------------------------------------------------------------------
// Adds a byte block to the CStr segment. Return value is the address of the
// new data block, relative to the CStr segment. If the block does not fit
// anymore into the remaining free space, the CStr segment will automatically
// be resized. No return value is defined to indicate an error.

JILLong				JILAddCStrData			(JILState* pState, const void* pData, JILLong dataSize);

//------------------------------------------------------------------------------
// JILAddCStrData
//------------------------------------------------------------------------------
// Adds a string to the CStr segment, but checks if the string already exists
// somewhere in the pool. If it does, returns the offset of the existing string.
// If it doesn't, stores the string and returns the new offset. The 'strSize'
// parameter should include the terminating zero character of the string.

JILLong				JILAddCStrPoolData		(JILState* pState, const JILChar* pStr, JILLong strSize);

//------------------------------------------------------------------------------
// JILDestroyCStrSegment
//------------------------------------------------------------------------------
// Destroys the CStr segment. Called from Terminate.

JILError			JILDestroyCStrSegment	(JILState* pState);

#endif	// #ifndef JILCSTRSEGMENT_H
