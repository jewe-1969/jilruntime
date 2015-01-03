//------------------------------------------------------------------------------
// File: JILArrayList.c                                        (c) 2014 jewe.org
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
/// @file jilarraylist.c
/// A combination of linked list and array. This class is a compromise between
/// the flexibility of a list and the efficiency of an array.
//------------------------------------------------------------------------------

#include "jilarraylist.h"
#include "jilarray.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// private struct JILArrayList
//------------------------------------------------------------------------------

typedef struct JILArrayListNode JILArrayListNode;
struct JILArrayListNode
{
	JILUnknown* pData;
	JILArrayListNode* pNext;
};

typedef struct JILArrayList JILArrayList;
struct JILArrayList
{
	JILArrayListDestructor	pDestructor;	// destruction callback
	JILState*				pState;			// VM state
	JILArrayListNode*		pHead;			// head node
	JILArrayListNode**		ppNodes;		// index
	JILLong					count;			// number of nodes (logical)
	JILLong					isize;			// size of index (physical)
};

//------------------------------------------------------------------------------
// global constants
//------------------------------------------------------------------------------

static const JILLong kArrayListAllocGrain = 32;

//------------------------------------------------------------------------------
// JILArrayList_New
//------------------------------------------------------------------------------

JILArrayList* JILArrayList_New(JILState* pVM, JILArrayListDestructor dtor)
{
	JILArrayList* _this = (JILArrayList*) pVM->vmMalloc(pVM, sizeof(JILArrayList));
	_this->count = 0;
	_this->isize = 0;
	_this->pDestructor = dtor;
	_this->pHead = NULL;
	_this->ppNodes = NULL;
	_this->pState = pVM;
	return _this;
}

//------------------------------------------------------------------------------
// JILArrayList_Delete
//------------------------------------------------------------------------------

void JILArrayList_Delete(JILArrayList* _this)
{
	JILArrayListNode *p, *q;
	JILState* ps = _this->pState;
	for( p = _this->pHead; p != NULL; p = q )
	{
		q = p->pNext;
		_this->pDestructor(ps, p->pData);
		ps->vmFree(ps, p);
	}
	if( _this->ppNodes )
		ps->vmFree(ps, _this->ppNodes);
	ps->vmFree(ps, _this);
}

//------------------------------------------------------------------------------
// JILArrayList_Copy
//------------------------------------------------------------------------------

void JILArrayList_Copy(JILArrayList* _this, const JILArrayList* src)
{
	JILArrayListNode* p = src->pHead;
	while( p )
	{
		NTLReferHandle(_this->pState, p->pData);
		JILArrayList_AddItem(_this, p->pData);
		p = p->pNext;
	}
}

//------------------------------------------------------------------------------
// JILArrayList_DeepCopy
//------------------------------------------------------------------------------

JILArrayList* JILArrayList_DeepCopy(const JILArrayList* src)
{
	JILArrayList* _this = JILArrayList_New(src->pState, JILArrayListRelease);
	JILArrayListNode* p = src->pHead;
	while( p )
	{
		JILArrayList_AddItem(_this, NTLCopyHandle(src->pState, p->pData));
		p = p->pNext;
	}
	return _this;
}

//------------------------------------------------------------------------------
// JILArrayListMark
//------------------------------------------------------------------------------

JILError JILArrayList_Mark(JILArrayList* _this)
{
	JILError err = JIL_No_Exception;
	JILArrayListNode* p = _this->pHead;
	while( p )
	{
		err = NTLMarkHandle(_this->pState, p->pData);
		if( err )
			break;
		p = p->pNext;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILArrayList_FromArray
//------------------------------------------------------------------------------

JILError JILArrayList_FromArray(JILArrayList* _this, const JILArray* src)
{
	JILLong i;
	for( i = 0; i < src->size; i++ )
	{
		NTLReferHandle(_this->pState, src->ppHandles[i]);
		JILArrayList_AddItem(_this, src->ppHandles[i]);
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILArrayList_ToArray
//------------------------------------------------------------------------------

JILError JILArrayList_ToArray(JILArrayList* _this, JILArray* pArray)
{
	JILArrayListNode* p = _this->pHead;
	while( p )
	{
		JILArray_MoveTo(pArray, pArray->size, p->pData);
		p = p->pNext;
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILArrayList_Enumerate
//------------------------------------------------------------------------------

JILError JILArrayList_Enumerate(JILArrayList* _this, JILHandle* pDelegate, JILHandle* pArgs)
{
	JILError err = JIL_No_Exception;
	JILState* pVM = _this->pState;
	JILArrayListNode* p = _this->pHead;
	while( p )
	{
		JILHandle* pResult = JILCallFunction(pVM, pDelegate, 2, kArgHandle, p->pData, kArgHandle, pArgs);
		err = NTLHandleToError(pVM, pResult);
		NTLFreeHandle(pVM, pResult);
		if( err )
			break;
		p = p->pNext;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILArrayList_GetItem
//------------------------------------------------------------------------------

JILUnknown* JILArrayList_GetItem(JILArrayList* _this, JILLong index)
{
	JILUnknown* result = NULL;
	JILState* ps = _this->pState;
	if( index >= 0 && index < _this->count )
	{
		result = _this->ppNodes[index]->pData;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArrayList_SetItem
//------------------------------------------------------------------------------

JILBool JILArrayList_SetItem(JILArrayList* _this, JILLong index, JILUnknown* pData)
{
	JILState* ps = _this->pState;
	JILArrayListNode* p;
	JILBool bSuccess = JILFalse;
	if( index >= 0 && index < _this->count )
	{
		p = _this->ppNodes[index];
		_this->pDestructor(ps, p->pData);
		p->pData = pData;
		bSuccess = JILTrue;
	}
	else
	{
		_this->pDestructor(ps, pData);
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
// JILArrayList_AddItem
//------------------------------------------------------------------------------

void JILArrayList_AddItem(JILArrayList* _this, JILUnknown* pData)
{
	JILState* ps = _this->pState;
	JILArrayListNode* p, **pp, *o;
	JILLong ix;

	o = NULL;
	ix = _this->count;
	_this->count++;
	if( _this->isize < _this->count )
	{
		_this->isize = _this->count + kArrayListAllocGrain;
		if( _this->ppNodes != NULL )
			ps->vmFree(ps, _this->ppNodes);
		pp = _this->ppNodes = ps->vmMalloc(ps, _this->isize * sizeof(JILArrayListNode*));
		for( p = _this->pHead; p != NULL; o = p, p = p->pNext )
		{
			*pp++ = p;
		}
	}
	else if( ix > 0 )
	{
		o = _this->ppNodes[ix - 1];
	}
	p = ps->vmMalloc(ps, sizeof(JILArrayListNode));
	p->pData = pData;
	p->pNext = NULL;
	_this->ppNodes[ix] = p;
	if( o == NULL )
		_this->pHead = p;
	else
		o->pNext = p;
}

//------------------------------------------------------------------------------
// JILArrayList_RemoveItem
//------------------------------------------------------------------------------

void JILArrayList_RemoveItem(JILArrayList* _this, JILLong index)
{
	JILState* ps = _this->pState;
	JILArrayListNode* p, *o, **pp;
	if( index >= 0 && index < _this->count )
	{
		pp = _this->ppNodes + index;
		p = *pp;
		o = (index > 0) ? pp[-1] : NULL;
		if( o == NULL )
			_this->pHead = p->pNext;
		else
			o->pNext = p->pNext;
		// update index incrementally
		o = p->pNext;
		while( o )
		{
			*pp++ = o;
			o = o->pNext;
		}
		// destroy
		_this->pDestructor(ps, p->pData);
		ps->vmFree(ps, p);
		_this->count--;
	}
}

//------------------------------------------------------------------------------
// JILArrayList_InsertItem
//------------------------------------------------------------------------------

void JILArrayList_InsertItem(JILArrayList* _this, JILLong index, JILUnknown* pData)
{
	JILState* ps = _this->pState;
	JILArrayListNode* p, *o, **pp;
	if( index >= 0 && index < _this->count )
	{
		_this->count++;
		if( _this->isize < _this->count )
		{
			_this->isize = _this->count + kArrayListAllocGrain;
			if( _this->ppNodes != NULL )
				ps->vmFree(ps, _this->ppNodes);
			pp = _this->ppNodes = ps->vmMalloc(ps, _this->isize * sizeof(JILArrayListNode*));
			for( p = _this->pHead; p != NULL; p = p->pNext )
			{
				*pp++ = p;
			}
		}
		pp = _this->ppNodes + index;
		o = (index > 0) ? pp[-1] : NULL;
		p = ps->vmMalloc(ps, sizeof(JILArrayListNode));
		p->pData = pData;
		p->pNext = *pp;
		if( o == NULL )
			_this->pHead = p;
		else
			o->pNext = p;
		// update index incrementally
		while( p )
		{
			*pp++ = p;
			p = p->pNext;
		}
	}
	else
	{
		_this->pDestructor(ps, pData);
	}
}

//------------------------------------------------------------------------------
// JILArrayList_Count
//------------------------------------------------------------------------------

JILLong JILArrayList_Count(JILArrayList* _this)
{
	return _this->count;
}

//------------------------------------------------------------------------------
// JILArrayListNone
//------------------------------------------------------------------------------

void JILArrayListNone(JILState* state, JILUnknown* data)
{
}

//------------------------------------------------------------------------------
// JILArrayListFree
//------------------------------------------------------------------------------

void JILArrayListFree(JILState* state, JILUnknown* data)
{
	free(data);
}

//------------------------------------------------------------------------------
// JILArrayListRelease
//------------------------------------------------------------------------------

void JILArrayListRelease(JILState* state, JILUnknown* data)
{
	NTLFreeHandle(state, data);
}
