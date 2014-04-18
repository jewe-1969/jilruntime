//------------------------------------------------------------------------------
// File: jillist.h											   (c) 2006 jewe.org
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
/// @file jillist.h
/// The built-in list and iterator classes the virtual machine uses.
//------------------------------------------------------------------------------

#ifndef JILLIST_H
#define JILLIST_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "jiltypes.h"

//------------------------------------------------------------------------------
// class JILListItem
//------------------------------------------------------------------------------

struct JILListItem
{
	JILList*		pList;		///< Pointer to owner list
	JILListItem*	pPrev;		///< Pointer to previous item in the list or NULL
	JILListItem*	pNext;		///< Pointer to next item in the list or NULL
	JILHandle*		pKey;		///< Pointer to 'key' associated with list item
	JILHandle*		pValue;		///< Pointer to actual data of the list item
	JILLong			numRef;		///< Number of iterator references to this item
};

//------------------------------------------------------------------------------
// class JILList
//------------------------------------------------------------------------------

struct JILList
{
	JILLong			length;		///< The number of items in the list
	JILListItem*	pFirst;		///< Pointer to the first item in the list, or NULL
	JILListItem*	pLast;		///< Pointer to the last item in the list, or NULL
	JILState*		pState;		///< The virtual machine object this list 'belongs' to
};

//------------------------------------------------------------------------------
// class JILIterator
//------------------------------------------------------------------------------

struct JILIterator
{
	JILListItem*	pItem;		///< Pointer to the current item, or NULL
	JILHandle*		pList;		///< Handle to the list this iterator references
	JILState*		pState;		///< The virtual machine object this list 'belongs' to
	JILLong			deleted;	///< Current item is marked deleted
};

//------------------------------------------------------------------------------
// JILList
//------------------------------------------------------------------------------

JILList*	JILList_New (JILState* pState);
void		JILList_Delete (JILList* _this);
void		JILList_Copy (JILList* _this, const JILList* pSource);
JILList*	JILList_DeepCopy (const JILList* _this);
void		JILList_FromArray(JILList* _this, const JILArray* pSource);

void		JILList_Add (JILList* _this, JILHandle* newKey, JILHandle* newValue);
void		JILList_AddOrSet (JILList* _this, JILHandle* pKey, JILHandle* newValue);
void		JILList_InsertBefore (JILList* _this, JILHandle* beforeKey, JILHandle* newKey, JILHandle* newValue);
void		JILList_InsertAfter (JILList* _this, JILHandle* afterKey, JILHandle* newKey, JILHandle* newValue);
void		JILList_InsertItem (JILListItem* item, JILHandle* newKey, JILHandle* newValue);
void		JILList_Swap (JILList* _this, JILHandle* pKey1, JILHandle* pKey2);
void		JILList_MoveToFirst (JILList* _this, JILHandle* pKey);
void		JILList_MoveToLast (JILList* _this, JILHandle* pKey);
void		JILList_Remove (JILList* _this, JILHandle* pKey);
void		JILList_Clear (JILList* _this);
JILError	JILList_Sort (JILList* _this, JILLong mode, JILHandle* pDelegate);
JILError	JILList_Enumerate (JILList* _this, JILHandle* pDelegate, JILHandle* pArgs);
JILArray*	JILList_ToArray (JILList* _this);

JILHandle*	JILList_ValueFromKey (JILList* _this, JILHandle* pKey);
JILHandle*	JILList_ValueFromIndex (JILList* _this, JILLong index);
JILHandle*	JILList_KeyFromIndex (JILList* _this, JILLong index);
JILLong		JILList_KeyExists (JILList* _this, JILHandle* pKey);

void		JILList_AddRef (JILState* pState, JILListItem* pItem);
void		JILList_Release (JILState* pState, JILListItem* pItem);
JILError	JILList_Mark (JILState* pState, JILListItem* pItem);
JILBool		JILList_InvalidKey (JILHandle* pKey);

//------------------------------------------------------------------------------
// JILListProc
//------------------------------------------------------------------------------

JILEXTERN JILError JILListProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

//------------------------------------------------------------------------------
// JILIteratorProc
//------------------------------------------------------------------------------

JILEXTERN JILError JILIteratorProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

#endif	// #ifndef JILLIST_H
