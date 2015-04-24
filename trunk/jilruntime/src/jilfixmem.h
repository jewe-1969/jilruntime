//------------------------------------------------------------------------------
// File: JILFixMem.h                                           (c) 2005 jewe.org
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
// A memory manager class for fast allocating and freeing fix-sized blocks of
// memory.
//------------------------------------------------------------------------------

#ifndef JILFIXMEM_H
#define JILFIXMEM_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------

JILFixMem*				New_FixMem				(JILLong maxBlocks, JILLong blockSize, JILLong bucketSize, JILMemStats* pStats);
void					Delete_FixMem			(JILFixMem* _this);
JILChar*				FixMem_Alloc			(JILFixMem* _this);
void					FixMem_Free				(JILFixMem* _this, JILChar* pBuffer);

// Global tool functions
JILLong					FixMem_GetBlockLength	(JILChar* pBuffer);
JILChar*				FixMem_AllocLargeBlock	(JILLong size, JILMemStats*);
void					FixMem_FreeLargeBlock	(JILChar* pBuffer, JILMemStats*);

#endif
