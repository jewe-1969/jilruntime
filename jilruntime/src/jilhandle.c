//------------------------------------------------------------------------------
// File: JILHandle.c                                        (c) 2003 jewe.org
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
//	Provides functions for fast allocation, initialization and deallocation of
//	handles. Every datatype the virtual machine deals with is encapsulated by
//  a handle. The purpose of a handle is mainly providing the vm with
//  information about the data type of the encapsulated object and to do
//  reference counting for that object. If the reference count of a handle
//  becomes zero, it means the handle (and the encapsulated object) is no more
//  needed and can therefore safely be freed (destroyed).
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilhandle.h"
#include "jildebug.h"
#include "jilallocators.h"
#include "jilmachine.h"
#include "jilcallntl.h"
#include "jilcodelist.h"

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

const JILChar* JILGetExceptionString(JILState*, JILError);

//------------------------------------------------------------------------------
// JILInitHandles
//------------------------------------------------------------------------------

JILError JILInitHandles(JILState* pState, JILLong allocGrain)
{
	JILLong i;
	JILHandle* pBucket;
	JILError result = JIL_No_Exception;

	pState->vmHandleAllocGrain = allocGrain;
	pState->vmMaxHandles = allocGrain;
	pState->vmUsedHandles = 0;

	// allocate handle pointer arrays
	pState->vmppHandles = (JILHandle**) malloc( allocGrain * sizeof(JILHandle*) );
	pState->vmppFreeHandles = (JILHandle**) malloc( allocGrain * sizeof(JILHandle*) );

	// allocate a handle memory bucket
	pBucket = (JILHandle*) malloc( allocGrain * sizeof(JILHandle) );
	memset(pBucket, 0, allocGrain * sizeof(JILHandle));

	// init handle pointer arrays
	for( i = 0; i < allocGrain; i++ )
	{
		pState->vmppHandles[i] = pBucket;
		pState->vmppFreeHandles[i] = pBucket++;
	}
	pState->vmppHandles[0]->flags = HF_NEWBUCKET;
	return result;
}

//------------------------------------------------------------------------------
// JILGetNewHandle
//------------------------------------------------------------------------------

JILHandle* JILGetNewHandle(JILState* pState)
{
	JILHandle* pHandle;

	if( pState->vmUsedHandles >= pState->vmMaxHandles )
	{
		// must resize pointer arrays!
		JILLong i;
		JILLong grain = pState->vmHandleAllocGrain;
		JILLong oldMax = pState->vmMaxHandles;
		JILLong newMax = oldMax + grain;
		JILHandle** ppOldHandles = pState->vmppHandles;
		JILHandle** ppOldFreeHandles = pState->vmppFreeHandles;
		JILHandle** ppAll;
		JILHandle** ppFree;
		JILHandle* pBucket;

		// allocate new buffers
		pState->vmppHandles = (JILHandle**) malloc( newMax * sizeof(JILHandle*) );
		pState->vmMaxHandles = newMax;
		memcpy( pState->vmppHandles, ppOldHandles, oldMax * sizeof(JILHandle*) );
		pState->vmppFreeHandles = (JILHandle**) malloc( newMax * sizeof(JILHandle*) );
		memcpy( pState->vmppFreeHandles, ppOldFreeHandles, oldMax * sizeof(JILHandle*) );

		// allocate new handle memory bucket
		pBucket = (JILHandle*) malloc( grain * sizeof(JILHandle) );
		memset(pBucket, 0, grain * sizeof(JILHandle));

		// init handle arrays
		ppAll = pState->vmppHandles + oldMax;
		ppFree = pState->vmppFreeHandles + oldMax;
		for( i = oldMax; i < newMax; i++ )
			*ppAll++ = *ppFree++ = pBucket++;
		pState->vmppHandles[oldMax]->flags = HF_NEWBUCKET;

		// free old buffers
		free( ppOldHandles );
		free( ppOldFreeHandles );
	}
	// get a new handle and initialize it
	pHandle = pState->vmppFreeHandles[pState->vmUsedHandles++];
	pHandle->type = type_null;
	pHandle->flags &= HF_NEWBUCKET;
	pHandle->refCount = 1;
	return pHandle;
}

//------------------------------------------------------------------------------
// JILFindHandleIndex
//------------------------------------------------------------------------------

JILLong JILFindHandleIndex(JILState* pState, JILHandle* pHandle)
{
	JILLong i;
	for( i = 0; i < pState->vmMaxHandles; i++ )
	{
		if( pState->vmppHandles[i] == pHandle )
			return i;
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILDestroyHandles
//------------------------------------------------------------------------------

JILError JILDestroyHandles(JILState* pState)
{
	JILLong i;
	JILError result = JIL_No_Exception;
	JILHandle* pHandle;
	JILGCEventRecord* pRecord;
	JILLong handlesLeaked = 0;
	JILLong leakedHandlesLeft = 0;

	// send shutdown events to everyone in the GC event list
	for( pRecord = pState->vmpFirstEventRecord; pRecord; pRecord = pRecord->pNext )
		pRecord->eventProc(pState, JIL_GCEvent_Shutdown, pRecord->pUserPtr);

	// STEP 1: List every handle that still exists at this point
	if( pState->vmLogGarbageMode == kLogGarbageAll )
	{
		for( i = 0; i < pState->vmMaxHandles; i++ )
		{
			pHandle = pState->vmppHandles[i];
			if( pHandle->refCount > 0 )
				JILMessageLog(pState, "Leaked handle %d, refCount = %d, type = %s\n", i, pHandle->refCount, JILGetHandleTypeName(pState, pHandle->type));
		}
	}
	// STEP 2: If there were leaks, destroy them now
	for( i = 0; i < pState->vmMaxHandles; i++ )
	{
		pHandle = pState->vmppHandles[i];
		if( pHandle->refCount > 0 )
		{
			if( pState->vmLogGarbageMode == kLogGarbageBrief )
				JILMessageLog(pState, "Collecting handle %d, refCount = %d, type = %s\n", i, pHandle->refCount, JILGetHandleTypeName(pState, pHandle->type));
			handlesLeaked++;
			pState->errHandlesLeaked++;
			pHandle->refCount = 1;
			JILRelease(pState, pHandle);
		}
	}
	if( handlesLeaked )
	{
		// STEP 3: Count how many leaks could not be destroyed
		for( i = 0; i < pState->vmMaxHandles; i++ )
		{
			pHandle = pState->vmppHandles[i];
			if( pHandle->refCount > 0 )
				leakedHandlesLeft++;
		}
		JILMessageLog(pState, "--- GC collected %d handles, %d left ---\n", handlesLeaked, leakedHandlesLeft);
	}
	// destroy all buckets
	for( i = 0; i < pState->vmMaxHandles; i++ )
	{
		pHandle = pState->vmppHandles[i];
		if( pHandle->flags & HF_NEWBUCKET )
		{
			i += pState->vmHandleAllocGrain - 1;
			free( pHandle );
		}
		else
		{
			JILMessageLog(pState, "Bucket expected, but handle not marked!\n");
		}
	}
	// destroy all handles
	if( pState->vmppHandles )
	{
		free( pState->vmppHandles );
		pState->vmppHandles = NULL;
	}
	// destroy free handle array
	if( pState->vmppFreeHandles )
	{
		free( pState->vmppFreeHandles );
		pState->vmppFreeHandles = NULL;
	}
	pState->vmMaxHandles = 0;
	pState->vmUsedHandles = 0;

	return result;
}

//------------------------------------------------------------------------------
// JILMarkHandle
//------------------------------------------------------------------------------

JILError JILMarkHandle(JILState* pState, JILHandle* pSource)
{
	JILError result = JIL_No_Exception;
	JILTypeInfo* pTypeInfo;
	// don't do it if it's already marked
	if( !pSource || (pSource->flags & HF_MARKED) || !pSource->refCount )
		return result;
	pSource->flags |= HF_MARKED;
	pTypeInfo = JILTypeInfoFromType(pState, pSource->type);
	// first check if the class is a native type
	if( pTypeInfo->isNative )
	{
		result = CallNTLMarkHandles(pTypeInfo, JILGetNObjectHandle(pSource)->ptr );
	}
	else	// not a native type
	{
		switch (pTypeInfo->family)
		{
			case tf_class:
			{
				JILLong i;
				JILHandle** ppS = JILGetObjectHandle(pSource)->ppHandles;
				JILLong size = pTypeInfo->instanceSize;
				for( i = 0; i < size; i++ )
				{
					result = JILMarkHandle(pState, *ppS++);
					if( result )
						break;
				}
				break;
			}
			case tf_delegate:
				result = JILMarkDelegate(pState, JILGetDelegateHandle(pSource)->pDelegate);
				break;
			case tf_thread:
				result = JILMarkContext(pState, JILGetContextHandle(pSource)->pContext);
				break;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILCopyHandle
//------------------------------------------------------------------------------

JILError JILCopyHandle(JILState* pState, JILHandle* pSource, JILHandle** ppOut)
{
	JILError result = JIL_No_Exception;
	JILHandle* pDest;
	// alloc a new handle
	pDest = JILGetNewHandle(pState);
	pDest->type = type_null;	// in case we fail to copy!
	switch( pSource->type )
	{
		case type_null:
			// release new handle
			JILRelease(pState, pDest);
			// return null handle
			pDest = JILGetNullHandle(pState);
			// add reference to null handle
			JILAddRef(pDest);
			*ppOut = pDest;
			return result;
		case type_int:
			JILGetIntHandle(pDest)->l = JILGetIntHandle(pSource)->l;
			break;
		case type_float:
			JILGetFloatHandle(pDest)->f = JILGetFloatHandle(pSource)->f;
			break;
		case type_string:
			JILGetStringHandle(pDest)->str = JILString_Copy( JILGetStringHandle(pSource)->str );
			break;
		case type_array:
			JILGetArrayHandle(pDest)->arr = JILArray_Copy( JILGetArrayHandle(pSource)->arr );
			break;
		default:	// everything else is a user defined type
		{
			JILUnknown* newPtr = NULL;
			JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, pSource->type);
			// first check if the class is a native type
			if( pTypeInfo->isNative )
			{
				// let native type handle copying
				JILHandleNObject* pDObj = JILGetNObjectHandle(pDest);
				// does the native type define a copy ctor?
				if (pTypeInfo->methodInfo.cctor != -1)
				{
					// create a new object
					result = CallNTLNewObject(pTypeInfo, &newPtr);
					if( result )
						goto _exit;
					// push source onto stack
					pState->vmpContext->vmppDataStack[--pState->vmpContext->vmDataStackPointer] = pSource;
					JILAddRef(pSource);
					// call copy ctor
					result = CallNTLCallMember(pTypeInfo, pTypeInfo->methodInfo.cctor, newPtr);
					// pop source from stack
					JILRelease(pState, pState->vmpContext->vmppDataStack[pState->vmpContext->vmDataStackPointer++]);
					if( result )
						goto _exit;
					pDObj->ptr = newPtr;
				}
				else
				{
					result = JIL_ERR_Unsupported_Native_Call;
					goto _exit;
				}
			}
			else	// not a native type
			{
				switch (pTypeInfo->family)
				{
					case tf_class:
					{
						// does the class have copy constructor?
						if (pTypeInfo->methodInfo.cctor == -1)
						{
							JILLong i;
							JILHandle** ppS;
							JILHandle** ppD;
							JILHandleObject* pDObj = JILGetObjectHandle(pDest);
							JILHandleObject* pSObj = JILGetObjectHandle(pSource);
							JILLong size = pTypeInfo->instanceSize;
							// allocate a new object
							pDObj->ppHandles = JILAllocObjectNoInit(pState, size);
							// copy references to source object's members
							ppS = pSObj->ppHandles;
							ppD = pDObj->ppHandles;
							for( i = 0; i < size; i++ )
							{
								result = JILCopyValueType(pState, *ppS++, ppD++);
								if (result)
									goto _exit;
							}
						}
						else	// call copy-constructor
						{
							// allocate a new object
							JILGetObjectHandle(pDest)->ppHandles = JILAllocObject(pState, pTypeInfo->instanceSize);
							// call copy-constructor
							pDest->type = pSource->type;
							result = JILCallCopyConstructor(pState, pDest, pSource);
							if (result)
								goto _exit;
						}
						break;
					}
					case tf_delegate:
					{
						JILDelegate* pDelegate = JILGetDelegateHandle(pSource)->pDelegate;
						JILGetDelegateHandle(pDest)->pDelegate = JILAllocDelegate(pState, pDelegate->index, pDelegate->pObject);
						break;
					}
					default:
					{
						// cannot copy thread contexts
						result = JIL_VM_Unsupported_Type;
						goto _exit;
					}
				}
			}
			break;
		}
	}
	pDest->type = pSource->type;
	*ppOut = pDest;
	return result;

_exit:
	JILRelease(pState, pDest);
	return result;
}

//------------------------------------------------------------------------------
// JILCopyValueType
//------------------------------------------------------------------------------

JILError JILCopyValueType(JILState* pState, JILHandle* pSource, JILHandle** ppOut)
{
	JILHandle* pDest;
	if (pSource->type == type_int)
	{
		pDest = JILGetNewHandle(pState);
		JILGetIntHandle(pDest)->l = JILGetIntHandle(pSource)->l;
	}
	else if(pSource->type == type_float)
	{
		pDest = JILGetNewHandle(pState);
		JILGetFloatHandle(pDest)->f = JILGetFloatHandle(pSource)->f;
	}
	else
	{
		// take new reference
		JILAddRef(pSource);
		pDest = pSource;
	}
	pDest->type = pSource->type;
	*ppOut = pDest;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateWeakRef
//------------------------------------------------------------------------------

JILHandle* JILCreateWeakRef(JILState* pState, JILHandle* srcHandle)
{
	JILHandle* pResult = JILGetNewHandle(pState);
	pResult->type = srcHandle->type;
	pResult->flags |= HF_PERSIST;
	memcpy(pResult->data, srcHandle->data, sizeof(JILHandleData));
	return pResult;
}

//------------------------------------------------------------------------------
// JILDestroyObject
//------------------------------------------------------------------------------

void JILDestroyObject(JILState* pState, JILHandle* pHandle)
{
	if( pHandle->flags & HF_PERSIST )
	{
		pHandle->flags &= ~HF_PERSIST;
	}
	else
	{
		JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, pHandle->type);
		switch( pHandle->type )
		{
			case type_null:
			case type_int:
			case type_float:
				// nothing to do
				break;
			case type_string:
				// free the string
				JILString_Delete( JILGetStringHandle(pHandle)->str );
				break;
			case type_array:
				JILArray_Delete( JILGetArrayHandle(pHandle)->arr );
				break;
			default:
			{
				if( pTypeInfo->isNative )
				{
					// call Destroy Object
					CallNTLDestroyObject(pTypeInfo, JILGetNObjectHandle(pHandle)->ptr);
				}
				else
				{
					switch(pTypeInfo->family)
					{
						case tf_class:
						{
							// release all class members
							JILLong i;
							JILHandle** ppObj = (JILHandle**) JILGetObjectHandle(pHandle)->ppHandles;
							JILLong size = pTypeInfo->instanceSize;
							for( i = 0; i < size; i++ )
							{
								JILRelease(pState, ppObj[i]);
							}
							// free the object
							pState->vmFree( pState, ppObj );
							break;
						}
						case tf_delegate:
						{
							JILFreeDelegate(pState, JILGetDelegateHandle(pHandle)->pDelegate);
							break;
						}
						case tf_thread:
						{
							JILFreeContext(pState, JILGetContextHandle(pHandle)->pContext);
							break;
						}
					}
				}
				break;
			}
		}
	}
}

//------------------------------------------------------------------------------
// JILCollectGarbage
//------------------------------------------------------------------------------

JILError JILCollectGarbage(JILState* ps)
{
	JILLong numColl = 0;
	JILError err;
	JILLong i;
	JILLong num;
	JILHandle* h;
	JILGCEventRecord* pRecord;
	JILFloat time = (JILFloat) clock();

	// for safety reasons, do nothing if currently executing byte-code
	if( ps->vmRunning )
		return JIL_ERR_Runtime_Locked;

	// mark data handles
	err = JILMarkDataHandles(ps);
	if( err )
		goto exit;

	// mark throw handle
	err = NTLMarkHandle(ps, ps->vmpThrowHandle);
	if( err )
		goto exit;

	// mark everything in the root context
	err = JILMarkContext(ps, ps->vmpRootContext);
	if( err )
		goto exit;

	// send mark events to everyone in the GC event list
	for( pRecord = ps->vmpFirstEventRecord; pRecord; pRecord = pRecord->pNext )
	{
		err = pRecord->eventProc(ps, JIL_GCEvent_Mark, pRecord->pUserPtr);
		if( err )
			goto exit;
	}

	// now run through all handles and free everything that's not marked
	num = ps->vmMaxHandles;
	if( ps->vmLogGarbageMode == kLogGarbageAll )
	{
		for( i = 0; i < num; i++ )
		{
			h = ps->vmppHandles[i];
			if( (h->refCount > 0) && !(h->flags & HF_MARKED) )
				JILMessageLog(ps, "Leaked handle %d, refCount = %d, type = %s\n", i, h->refCount, JILGetHandleTypeName(ps, h->type));
		}
	}
	for( i = 0; i < num; i++ )
	{
		h = ps->vmppHandles[i];
		if( (h->refCount > 0) && !(h->flags & HF_MARKED) )
		{
			if( ps->vmLogGarbageMode == kLogGarbageBrief )
				JILMessageLog(ps, "Collecting handle %d, refCount = %d, type = %s\n", i, h->refCount, JILGetHandleTypeName(ps, h->type));
			h->refCount = 1;
			JILRelease(ps, h);
			numColl++;
		}
		else
		{
			// unmark
			h->flags &= ~HF_MARKED;
		}
	}

	ps->vmTimeLastGC = (JILFloat) clock();
	if( numColl )
	{
		time = (ps->vmTimeLastGC - time) / ((JILFloat)CLOCKS_PER_SEC);
		ps->errHandlesLeaked += numColl;
		JILMessageLog(ps, "--- GC collected %d handles in %g seconds ---\n", numColl, time);
	}
	return JIL_No_Exception;

exit:
	JILMessageLog(ps, "GC MARK ERROR: %d (%s)\n", err, JILGetExceptionString(ps, err));
	return JIL_ERR_Mark_Handle_Error;
}

//------------------------------------------------------------------------------
// JILCreateException
//------------------------------------------------------------------------------

JILHandle* JILCreateException(JILState* pState, JILError err)
{
	JILHandle* pHandle;
	JILRuntimeException* pException;

	if( !pState->vmInitialized )
		return NULL;

	pHandle = NTLNewObject(pState, type_rt_exception);
	pException = NTLHandleToObject(pState, type_rt_exception, pHandle);
	if( pException != NULL )
	{
		pException->error = err;
		JILString_Assign(pException->pMessage, JILGetExceptionString(pState, err));
	}
	return pHandle;
}
