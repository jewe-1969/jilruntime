//------------------------------------------------------------------------------
// File: JILCStrSegment.c                                   (c) 2003 jewe.org
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
//	The global CStr segment has the purpose to store raw-data, such as c-string
//	constants or other binary data, that may referenced by a handle. This file
//	provides functions to initialize and destroy the CStr segment, as well as
//	adding data to the CStr segment.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilcstrsegment.h"

//------------------------------------------------------------------------------
// JILInitCStrSegment
//------------------------------------------------------------------------------

JILError JILInitCStrSegment(JILState* pState, JILLong initialSize)
{
	JILError result = JIL_No_Exception;

	initialSize = ((initialSize + 3) & 0xFFFFFC);
	pState->vmpCStrSegment = NULL;
	pState->vmUsedCStrSegSize = 0;
	pState->vmMaxCStrSegSize = 0;

	// allocate handles buffer
	pState->vmpCStrSegment = (JILChar*) malloc( initialSize );
	pState->vmMaxCStrSegSize = initialSize;
	memset( pState->vmpCStrSegment, 0, initialSize );

	return result;
}

//------------------------------------------------------------------------------
// JILAddCStrData
//------------------------------------------------------------------------------

JILLong JILAddCStrData(JILState* pState, const void* pData, JILLong dataSize)
{
	JILLong result = 0;
	JILLong coarseSize = ((dataSize + 3) & 0xFFFFFC) + sizeof(JILLong);
	// check if there is space in the segment
	if( (pState->vmUsedCStrSegSize + coarseSize) >= pState->vmMaxCStrSegSize )
	{
		// must resize buffer!
		long grain = pState->vmCStrSegAllocGrain;
		long oldMax = pState->vmMaxCStrSegSize;
		long newMax = oldMax + coarseSize + grain;
		JILChar* pOldSegment = pState->vmpCStrSegment;

		// allocate new buffer
		pState->vmpCStrSegment = (JILChar*) malloc( newMax );
		pState->vmMaxCStrSegSize = newMax;
		memset( pState->vmpCStrSegment + oldMax, 0, grain + coarseSize );
		memcpy( pState->vmpCStrSegment, pOldSegment, oldMax );

		// free old buffer
		free( pOldSegment );
	}
	// write data to buffer and return address
	result = pState->vmUsedCStrSegSize;
	memcpy( pState->vmpCStrSegment + result, &coarseSize, sizeof(JILLong) );
	result += sizeof(JILLong);
	memcpy( pState->vmpCStrSegment + result, pData, dataSize );
	pState->vmUsedCStrSegSize += coarseSize;

	return result;
}

//------------------------------------------------------------------------------
// JILAddCStrPoolData
//------------------------------------------------------------------------------

JILLong JILAddCStrPoolData(JILState* pState, const JILChar* pStr, JILLong strSize)
{
	JILLong p, o;
	JILChar* pCStr;
	// search for string in CStr segment
	for( p = 0; p < pState->vmUsedCStrSegSize; p += o )
	{
		o = (*(JILLong*)(pState->vmpCStrSegment + p));
		pCStr = pState->vmpCStrSegment + p + sizeof(JILLong);
		if( strcmp(pCStr, pStr) == 0 )
			return p + sizeof(JILLong);
	}
	// nothing found
	return JILAddCStrData(pState, pStr, strSize);
}

//------------------------------------------------------------------------------
// JILDestroyCStrSegment
//------------------------------------------------------------------------------

JILError JILDestroyCStrSegment(JILState* pState)
{
	JILError result = JIL_No_Exception;

	if( pState->vmpCStrSegment )
	{
		free( pState->vmpCStrSegment );
		pState->vmpCStrSegment = NULL;
	}
	pState->vmUsedCStrSegSize = 0;
	pState->vmMaxCStrSegSize = 0;

	return result;
}
