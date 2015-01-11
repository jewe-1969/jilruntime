//------------------------------------------------------------------------------
// File: JILAllocators.c                                    (c) 2003 jewe.org
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
//	Several allocator functions for example for objects, arrays and strings.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilallocators.h"
#include "jilhandle.h"
#include "jilmachine.h"

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILEXTERN JILHandle*	NTLNewObject			(JILState* pState, JILLong TypeID);

//------------------------------------------------------------------------------
// JILAllocObject
//------------------------------------------------------------------------------
// Allocates a JIL object of a given size.

JILHandle** JILAllocObject(JILState* pState, JILLong instSize)
{
	JILHandle** ppHandles;
	JILHandle** ppAll;
	JILHandle* pNull;
	JILLong i;

	ppAll = ppHandles = (JILHandle**) pState->vmMalloc( pState, instSize * sizeof(JILHandle*) );
	// init object with null handles
	pNull = JILGetNullHandle(pState);
	for( i = 0; i < instSize; i++ )
		*ppAll++ = pNull;
	pNull->refCount += instSize;
	return ppHandles;
}

//------------------------------------------------------------------------------
// JILAllocObjectNoInit
//------------------------------------------------------------------------------
// Allocates a JIL object without initializing it with null handles. This is
// only for cases where the object will get initialized immediately after this
// function returns. (For example in CopyHandle())

JILHandle** JILAllocObjectNoInit(JILState* pState, JILLong instSize)
{
	return (JILHandle**) pState->vmMalloc( pState, instSize * sizeof(JILHandle*) );
}

//------------------------------------------------------------------------------
// JILAllocArrayMulti
//------------------------------------------------------------------------------
// Allocates a (multi-dimensional) JIL array of a given number of dimensions.
// ATTENTION: This function relies on arguments on the virtual machine's stack,
// so do not call this unless you know exactly how to use this function!!!

JILArray* JILAllocArrayMulti(JILState* pState, JILLong type, JILLong dim, JILLong n)
{
	JILLong size, i;
	JILArray* pResult;
	JILHandle* pHandle;

	// special case if dim == 0
	if( dim == 0 )
		return JILArray_FillWithType(pState, type, 0);

	n++;

	// get the size for this dimension from the stack and allocate an array object of that size
	pHandle = pState->vmpContext->vmppDataStack[pState->vmpContext->vmDataStackPointer + dim - n];
	size = JILGetIntHandle(pHandle)->l;

	// if this is not the last dimension, recursively initialize sub-arrays
	if( n < dim )
	{
		pResult = JILArray_FillWithType(pState, type_null, size);
		for( i = 0; i < size; i++ )
		{
			JILArray* pElement = JILAllocArrayMulti(pState, type, dim, n);
			pHandle = JILGetNewHandle(pState);
			pHandle->type = type_array;
			JILGetArrayHandle(pHandle)->arr = pElement;
			JILRelease(pState, pResult->ppHandles[i]);
			pResult->ppHandles[i] = pHandle;
		}
	}
	else
	{
		pResult = JILArray_FillWithType(pState, type, size);
	}
	return pResult;
}

//------------------------------------------------------------------------------
// JILAllocString
//------------------------------------------------------------------------------
// Allocates a JIL string from a given string constant.

JILString* JILAllocString(JILState* pState, const JILChar* pStr)
{
	JILString* pString = JILString_New(pState);
	JILString_Assign(pString, pStr);
	return pString;
}

//------------------------------------------------------------------------------
// JILAllocStringFromCStr
//------------------------------------------------------------------------------
// Allocates a JIL string from a given string constant.

JILString* JILAllocStringFromCStr(JILState* pState, JILLong offsetString)
{
	JILString* pString = JILString_New(pState);
	JILString_Assign(pString, JILCStrGetString(pState, offsetString));
	return pString;
}

//------------------------------------------------------------------------------
// JILAllocDelegate
//------------------------------------------------------------------------------
// Allocates a JIL delegate object.

JILDelegate* JILAllocDelegate(JILState* pState, JILLong index, JILHandle* pObject)
{
	JILDelegate* pDel = (JILDelegate*) pState->vmMalloc(pState, sizeof(JILDelegate));
	pDel->index = index;
	pDel->pObject = pObject;
	pDel->pClosure = NULL;
	if( pObject )
		JILAddRef(pObject);
	return pDel;
}

//------------------------------------------------------------------------------
// JILAllocClosure
//------------------------------------------------------------------------------
// Allocates a JIL closure object.

JILDelegate* JILAllocClosure(JILState* pState, JILLong stackSize, JILLong addr, JILHandle* pObject)
{
	JILDelegate* pDelegate;
	JILByte* ptr;
	JILHandle** src;
	JILHandle** dst;
	JILLong i;
	JILLong size = sizeof(JILDelegate) + sizeof(JILClosure) + stackSize * sizeof(JILHandle*);

	ptr = (JILByte*) pState->vmMalloc(pState, size);
	pDelegate = (JILDelegate*) ptr;
	ptr += sizeof(JILDelegate);
	pDelegate->pClosure = (JILClosure*) ptr;
	ptr += sizeof(JILClosure);
	pDelegate->pClosure->stackSize = stackSize;
	pDelegate->pClosure->ppStack = (JILHandle**) ptr;
	pDelegate->index = addr;
	pDelegate->pObject = pObject;
	JILAddRef(pObject);

	src = pState->vmpContext->vmppDataStack + pState->vmpContext->vmDataStackPointer;
	dst = pDelegate->pClosure->ppStack;
	for( i = 0; i < stackSize; i++ )
	{
		JILAddRef(*src);
		*dst++ = *src++;
	}
	return pDelegate;
}

//------------------------------------------------------------------------------
// JILFreeDelegate
//------------------------------------------------------------------------------
// Frees a JIL delegate object.

void JILFreeDelegate(JILState* pState, JILDelegate* pDelegate)
{
	if( pDelegate->pObject )
		JILRelease(pState, pDelegate->pObject)
	if( pDelegate->pClosure )
	{
		JILLong i;
		JILLong n = pDelegate->pClosure->stackSize;
		JILHandle** src = pDelegate->pClosure->ppStack;
		for( i = 0; i < n; i++ )
			JILRelease(pState, *src++);
	}
	pState->vmFree(pState, pDelegate);
}

//------------------------------------------------------------------------------
// JILAllocFactory
//------------------------------------------------------------------------------

JILArray* JILAllocFactory(JILState* pState, JILLong interfaceId)
{
	JILArray* pArr = JILArray_New(pState);
	JILTypeInfo* pType;
	JILLong clas;
	// iterate over all type info elements
	for( clas = 0; clas < pState->vmUsedTypeInfoSegSize; clas++ )
	{
		pType = JILTypeInfoFromType(pState, clas);
		if( pType->family == tf_class && JILIsBaseType(pState, interfaceId, clas))
		{
			JILHandle* pH = NTLNewObject(pState, pType->type);
			JILArray_ArrMove(pArr, pH);
			JILRelease(pState, pH);
		}
	}
	return pArr;
}
