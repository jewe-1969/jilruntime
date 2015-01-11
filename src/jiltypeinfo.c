//------------------------------------------------------------------------------
// File: JILTypeInfo.c                                      (c) 2005 jewe.org
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
// Provides functions to init, maintain and destroy the typeinfo segment.
// (( TODO: insert further documentation here ))
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jiltypeinfo.h"
#include "jilcstrsegment.h"
#include "jiltools.h"
#include "jiltable.h"

//------------------------------------------------------------------------------
// JILInitTypeInfoSegment
//------------------------------------------------------------------------------

JILError JILInitTypeInfoSegment(JILState* pState, JILLong initialSize)
{
	JILError result = JIL_No_Exception;

	pState->vmpTypeInfoSegment = NULL;
	pState->vmUsedTypeInfoSegSize = 0;
	pState->vmMaxTypeInfoSegSize = 0;

	// allocate initial buffer
	pState->vmpTypeInfoSegment = (JILTypeInfo*) malloc( initialSize * sizeof(JILTypeInfo) );
	pState->vmMaxTypeInfoSegSize = initialSize;
	memset( pState->vmpTypeInfoSegment, 0, initialSize * sizeof(JILTypeInfo) );

	return result;
}

//------------------------------------------------------------------------------
// JILNewTypeInfo
//------------------------------------------------------------------------------

JILLong JILNewTypeInfo(JILState* pState, const JILChar* pName, JILTypeInfo** ppOut)
{
	JILTypeInfo* pTypeInfo;
	long result;

	// check if the type already exists
	JILFindTypeInfo(pState, pName, ppOut);
	if( *ppOut )
		return (*ppOut)->type;
	// if we had created a type info hash table, delete it now
	if( pState->vmpTypeInfoTable )
	{
		JILTable_Delete( pState->vmpTypeInfoTable );
		pState->vmpTypeInfoTable = NULL;
	}
	if( pState->vmUsedTypeInfoSegSize >= pState->vmMaxTypeInfoSegSize )
	{
		// must resize buffer!
		long grain = pState->vmSegmentAllocGrain;
		long oldMax = pState->vmMaxTypeInfoSegSize;
		long newMax = oldMax + grain;
		JILTypeInfo* pOldHandles = pState->vmpTypeInfoSegment;

		// allocate new buffer
		pState->vmpTypeInfoSegment = (JILTypeInfo*) malloc( newMax * sizeof(JILTypeInfo) );
		pState->vmMaxTypeInfoSegSize = newMax;
		memset( pState->vmpTypeInfoSegment + oldMax, 0, grain * sizeof(JILTypeInfo) );
		memcpy( pState->vmpTypeInfoSegment, pOldHandles, oldMax * sizeof(JILTypeInfo) );

		// free old buffer
		free( pOldHandles );
	}
	// get a new type identifier
	result = pState->vmUsedTypeInfoSegSize++;

	// initialize type info
	pTypeInfo = pState->vmpTypeInfoSegment + result;
	memset( pTypeInfo, 0, sizeof(JILTypeInfo) );
	pTypeInfo->type = result;
	pTypeInfo->instance.typeID = result;
	pTypeInfo->instance.pState = pState;
	// add name to CStr segment
	pTypeInfo->offsetName = JILAddCStrPoolData(pState, pName, strlen(pName) + 1);

	// return result
	*ppOut = pTypeInfo;
	return result;
}

//------------------------------------------------------------------------------
// JILFindTypeInfo
//------------------------------------------------------------------------------

JILLong JILFindTypeInfo(JILState* pState, const JILChar* pName, JILTypeInfo** ppOut)
{
	JILLong i;
	for( i = 0; i < pState->vmUsedTypeInfoSegSize; i++ )
	{
		JILTypeInfo* pInfo = pState->vmpTypeInfoSegment + i;
		if( strcmp(JILCStrGetString(pState, pInfo->offsetName), pName) == 0 )
		{
			// return info
			*ppOut = pInfo;
			return i;
		}
	}
	*ppOut = NULL;
	return 0;
}

//------------------------------------------------------------------------------
// JILDestroyTypeInfoSegment
//------------------------------------------------------------------------------

JILError JILDestroyTypeInfoSegment(JILState* pState)
{
	JILError result = JIL_No_Exception;

	if( pState->vmpTypeInfoTable )
	{
		JILTable_Delete( pState->vmpTypeInfoTable );
		pState->vmpTypeInfoTable = NULL;
	}
	if( pState->vmpTypeInfoSegment )
	{
		free( pState->vmpTypeInfoSegment );
		pState->vmpTypeInfoSegment = NULL;
	}
	pState->vmUsedTypeInfoSegSize = 0;
	pState->vmMaxTypeInfoSegSize = 0;

	return result;
}
