//------------------------------------------------------------------------------
// File: jillist.c											   (c) 2006 jewe.org
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
/// @file jillist.c
/// The built-in list object the virtual machine uses. The built-in list is
/// a primitive data type and does only support very basic operations. However,
/// more functions might be added here in the future, to make using and
/// manipulating the list object from native typelibs or the application
/// using the jilruntime library easier.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jillist.h"
#include "jilarray.h"
#include "jiltools.h"
#include "jilhandle.h"
#include "jilstring.h"
#include "jilmachine.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------

enum
{
	kCtor,
	kCctor,
	kCtorArray,
	kLength,

	kAdd,
	kAddOrSet,
	kInsert,
	kInsertAfter,
	kSwap,
	kMoveToFirst,
	kMoveToLast,
	kRemove,
	kClear,
	kSort,

	kValue,
	kValueFromIndex,
	kKeyFromIndex,
	kKeyExists,
	kEnumerate,
	kDeepCopy,
	kToArray
};

//------------------------------------------------------------------------------
// class declaration list
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("This is the built-in list class. It is a double-chained, associative list implementation. Items can be stored and retrieved by an associative 'key'. The key can be an integer, a floating-point number, or a string. The list does not enforce that an item's key is unique.<p>Note that all methods that access items by their key will actually search the list for the first occurrence of a matching key. This will only work reliably, if you use unique values for the keys. It also means that performance of these methods gets worse the larger the list gets. For very large collections, consider using the table class, or use the iterator class for sequential access to items.</p>")
	"delegate			enumerator(const var key, var value, var args);" TAG("Delegate type for the list::enumerate() method.")
	"delegate int		comparator(const var value1, const var value2);" TAG("Delegate type for the list::sort() and array::sort() methods. The delegate should handle null-references and unmatching types gracefully. It should return -1 if value1 is less than value2, 1 if it is greater, and 0 if they are equal.")
	"method				list();" TAG("Constructs a new, empty list.")
	"method				list(const list);" TAG("Copy-constructs a new list from the specified list. The new list will be a shall-copy, meaning items in the list will be copied only by reference.")
	"method				list(const array);" TAG("Constructs a list from the specified array. If the array is multi-dimensional, sub-arrays will be added to the list. The array index will be used as a key for every element added to the list.")
	"accessor int		length();" TAG("Returns current number of items in this list.")
	"method				add(const var key, var val);" TAG("Adds a new item to the list by key and value. The key must be an integer, floating-point value, or a string. No checking is performed wheter the key is already in the list. The value can be of any type.")
	"method				addOrSet(const var key, var val);" TAG("Sets an existing item in the list to a new value, or adds a new item. The method first checks if the specified key is already in the list. If it is, the associated value is replaced by the new value. If the key is not found in the list, a new item is added.")
	"method				insert(const var key, const var newKey, var newVal);" TAG("Inserts a new item in the list. The new item will be inserted before the specified item. If the specified item does not exist, the call has no effect.")
	"method				insertAfter(const var key, const var newKey, var newVal);" TAG("Inserst a new item in the list. The new item will be inserted after the specified item. If the specified item does not exist, the call has no effect.")
	"method				swap(const var key1, const var key2);" TAG("Exchanges the positions of the specified items in the list. If one or both items are not found, the call is ignored.")
	"method				moveToFirst(const var key);" TAG("Moves the specified item to the beginning of the list. If the specified item does not exist, the call has no effect.")
	"method				moveToLast(const var key);" TAG("Moves the specified item to the end of the list. If the specified item does not exist, the call has no effect.")
	"method				remove(const var key);" TAG("Removes the specified item from the list. If the specified item does not exist, the call has no effect.")
	"method				clear();" TAG("Removes all items from the list.")
	"method				sort(const int mode, comparator fn);" TAG("Sorts the list according to the specified mode and comparator delegate. 'mode' is defined as follows: <ol start='0'><li>sort by key first, ascending</li><li>sort by key first, descending</li><li>sort by value first, ascending</li><li>sort by value first, descending</li></ol>")
	"method var			value(const var key);" TAG("Returns the value from the list that is associated with the specified key. If the key is not found, null is returned.")
	"method var			valueFromIndex(const int index);" TAG("Returns the value from the list that is associated with the specified zero based index. If the index is out of range, null is returned.")
	"method const var	keyFromIndex(const int index);" TAG("Returns the key from the list that is associated with the specified zero based index. If the index is out of range, null is returned.")
	"method int			keyExists(const var key);" TAG("Returns true if the specified key exists in this list, otherwise false.")
	"method				enumerate(enumerator fn, var args);" TAG("Calls the specified enumerator delegate for every item in this list.")
	"method list		deepCopy();" TAG("Returns a deep-copy of this list. WARNING: All element data will be copied! If the list contains script objects that have copy-constructors, this method can be time consuming. It should only be called in cases where a shallow-copy would not suffice.")
	"method array		toArray();" TAG("Returns an array of all values in this list. The list item's keys will be disregarded.")
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"list";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"A list class for JewelScript.";
static const char*	kTimeStamp		=	"01/28/2006";

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int ListNew			(NTLInstance* pInst, JILList** ppObject);
static int ListDelete		(NTLInstance* pInst, JILList* _this);
static int ListMark			(NTLInstance* pInst, JILList* _this);
static int ListCallMember	(NTLInstance* pInst, int funcID, JILList* _this);

//------------------------------------------------------------------------------
// JILListProc
//------------------------------------------------------------------------------

JILError JILListProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	JILError result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return ListNew(pInst, (JILList**) ppDataOut);
		case NTL_MarkHandles:			return ListMark(pInst, (JILList*) pDataIn);
		case NTL_CallStatic:			return JIL_ERR_Unsupported_Native_Call;
		case NTL_CallMember:			return ListCallMember(pInst, param, (JILList*) pDataIn);
		case NTL_DestroyObject:			return ListDelete(pInst, (JILList*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;

		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetDeclString:			(*(const char**) ppDataOut) = kClassDeclaration; break;
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;

		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// ListNew
//------------------------------------------------------------------------------

static int ListNew(NTLInstance* pInst, JILList** ppObject)
{
	*ppObject = JILList_New( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// ListDelete
//------------------------------------------------------------------------------

static int ListDelete(NTLInstance* pInst, JILList* _this)
{
	JILList_Delete( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// ListMark
//------------------------------------------------------------------------------

static int ListMark(NTLInstance* pInst, JILList* _this)
{
	JILError err = JIL_No_Exception;
	JILState* pState = _this->pState;
	JILListItem* pItem;
	for( pItem = _this->pFirst; pItem != NULL; pItem = pItem->pNext )
	{
		err = NTLMarkHandle(pState, pItem->pKey);
		if( err )
			break;
		err = NTLMarkHandle(pState, pItem->pValue);
		if( err )
			break;
	}
	return err;
}

//------------------------------------------------------------------------------
// ListCallMember
//------------------------------------------------------------------------------

static int ListCallMember(NTLInstance* pInst, int funcID, JILList* _this)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILHandle* key2 = NULL;
	JILHandle* key = NULL;
	JILHandle* val = NULL;
	JILLong index;
	switch( funcID )
	{
		case kCtor:
			break;
		case kCctor:
		{
			JILHandle* hSrc = NTLGetArgHandle(ps, 0);
			JILList_Copy(_this, NTLHandleToObject(ps, NTLInstanceTypeID(pInst), hSrc));
			NTLFreeHandle(ps, hSrc);
			break;
		}
		case kCtorArray:
		{
			JILHandle* hArr = NTLGetArgHandle(ps, 0);
			JILList_FromArray(_this, NTLHandleToObject(ps, type_array, hArr));
			NTLFreeHandle(ps, hArr);
		}
		case kLength:
			NTLReturnInt(ps, _this->length);
			break;
		case kAdd:
			key = NTLGetArgHandle(ps, 0);
			val = NTLGetArgHandle(ps, 1);
			if( JILList_InvalidKey(key) )
				goto invalid_key;
			JILList_Add(_this, key, val);
			NTLFreeHandle(ps, val);
			NTLFreeHandle(ps, key);
			break;
		case kAddOrSet:
			key = NTLGetArgHandle(ps, 0);
			val = NTLGetArgHandle(ps, 1);
			if( JILList_InvalidKey(key) )
				goto invalid_key;
			JILList_AddOrSet(_this, key, val);
			NTLFreeHandle(ps, val);
			NTLFreeHandle(ps, key);
			break;
		case kInsert:
			key2 = NTLGetArgHandle(ps, 0);
			key = NTLGetArgHandle(ps, 1);
			val = NTLGetArgHandle(ps, 2);
			if( JILList_InvalidKey(key) )
				goto invalid_key;
			JILList_InsertBefore(_this, key2, key, val);
			NTLFreeHandle(ps, val);
			NTLFreeHandle(ps, key);
			NTLFreeHandle(ps, key2);
			break;
		case kInsertAfter:
			key2 = NTLGetArgHandle(ps, 0);
			key = NTLGetArgHandle(ps, 1);
			val = NTLGetArgHandle(ps, 2);
			if( JILList_InvalidKey(key) )
				goto invalid_key;
			JILList_InsertAfter(_this, key2, key, val);
			NTLFreeHandle(ps, val);
			NTLFreeHandle(ps, key);
			NTLFreeHandle(ps, key2);
			break;
		case kSwap:
			key2 = NTLGetArgHandle(ps, 0);
			key = NTLGetArgHandle(ps, 1);
			JILList_Swap(_this, key2, key);
			NTLFreeHandle(ps, key);
			NTLFreeHandle(ps, key2);
			break;
		case kMoveToFirst:
			key = NTLGetArgHandle(ps, 0);
			JILList_MoveToFirst(_this, key);
			NTLFreeHandle(ps, key);
			break;
		case kMoveToLast:
			key = NTLGetArgHandle(ps, 0);
			JILList_MoveToLast(_this, key);
			NTLFreeHandle(ps, key);
			break;
		case kRemove:
			key = NTLGetArgHandle(ps, 0);
			JILList_Remove(_this, key);
			NTLFreeHandle(ps, key);
			break;
		case kClear:
			JILList_Clear(_this);
			break;
		case kSort:
		{
			JILHandle* pDel = NTLGetArgHandle(ps, 1);
			result = JILList_Sort(_this, NTLGetArgInt(ps, 0), pDel);
			NTLFreeHandle(ps, pDel);
			break;
		}
		case kValue:
			key = NTLGetArgHandle(ps, 0);
			NTLReturnHandle(ps, JILList_ValueFromKey(_this, key));
			NTLFreeHandle(ps, key);
			break;
		case kValueFromIndex:
			index = NTLGetArgInt(ps, 0);
			NTLReturnHandle(ps, JILList_ValueFromIndex(_this, index));
			break;
		case kKeyFromIndex:
			index = NTLGetArgInt(ps, 0);
			NTLReturnHandle(ps, JILList_KeyFromIndex(_this, index));
			break;
		case kKeyExists:
			key = NTLGetArgHandle(ps, 0);
			NTLReturnInt(ps, JILList_KeyExists(_this, key));
			NTLFreeHandle(ps, key);
			break;
		case kEnumerate:
		{
			JILHandle* pDel = NTLGetArgHandle(ps, 0);
			JILHandle* pArg = NTLGetArgHandle(ps, 1);			
			result = JILList_Enumerate(_this, pDel, pArg);
			NTLFreeHandle(ps, pArg);
			NTLFreeHandle(ps, pDel);
			break;
		}
		case kDeepCopy:
		{
			JILList* pNew = JILList_DeepCopy(_this);
			JILHandle* ph = NTLNewHandleForObject(ps, NTLInstanceTypeID(pInst), pNew);
			NTLReturnHandle(ps, ph);
			NTLFreeHandle(ps, ph);
			break;
		}
		case kToArray:
		{
			JILArray* pArr = JILList_ToArray(_this);
			JILHandle* ph = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, ph);
			NTLFreeHandle(ps, ph);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;

invalid_key:
	if( val )
		NTLFreeHandle(ps, val);
	if( key )
		NTLFreeHandle(ps, key);
	if( key2 )
		NTLFreeHandle(ps, key2);
	return JIL_VM_Unsupported_Type;
}

//------------------------------------------------------------------------------
// NewListItem
//------------------------------------------------------------------------------

static JILListItem* NewListItem(JILState* pState, JILHandle* key, JILHandle* value)
{
	JILListItem* item = (JILListItem*) pState->vmMalloc(pState, sizeof(JILListItem));
	NTLReferHandle(pState, key);
	item->pKey = key;
	NTLReferHandle(pState, value);
	item->pValue = value;
	item->pNext = NULL;
	item->pPrev = NULL;
	item->pList = NULL;
	item->numRef = 1;
	return item;
}

//------------------------------------------------------------------------------
// ListCompareHandle
//------------------------------------------------------------------------------

static JILLong ListCompareHandle(JILHandle* h1, JILHandle* h2, JILState* ps, JILHandle* pDelegate, JILError* pErr)
{
	*pErr = JIL_No_Exception;
	if( h1->type > h2->type )
		return 1;
	else if( h1->type < h2->type )
		return -1;
	switch( h1->type )
	{
		case type_int:
			if( JILGetIntHandle(h1)->l > JILGetIntHandle(h2)->l )
				return 1;
			else if( JILGetIntHandle(h1)->l < JILGetIntHandle(h2)->l )
				return -1;
			else
				return 0;
		case type_float:
			if( JILGetFloatHandle(h1)->f > JILGetFloatHandle(h2)->f )
				return 1;
			else if( JILGetFloatHandle(h1)->f < JILGetFloatHandle(h2)->f )
				return -1;
			else
				return 0;
		case type_string:
			return JILString_Compare(JILGetStringHandle(h1)->str, JILGetStringHandle(h2)->str);
		default:
			if( pDelegate )
			{
				JILHandle* pResult = JILCallFunction(ps, pDelegate, 2, kArgHandle, h1, kArgHandle, h2);
				JILLong res = NTLHandleToInt(ps, pResult);
				*pErr = NTLHandleToError(ps, pResult);
				NTLFreeHandle(ps, pResult);
				return res;
			}
			break;
	}
	return 0;
}

//------------------------------------------------------------------------------
// ListIsPredessor
//------------------------------------------------------------------------------

static JILLong ListIsPredessor(JILListItem* item1, JILListItem* item2, JILLong mode, JILState* ps, JILHandle* pDelegate, JILError* pErr)
{
	JILLong result;
	JILHandle* first1;
	JILHandle* first2;
	JILHandle* second1;
	JILHandle* second2;
	if( mode & 1 )
	{
		JILListItem* i = item1;
		item1 = item2;
		item2 = i;
	}
	if( mode & 2 )
	{
		first1 = item1->pValue;
		first2 = item2->pValue;
		second1 = item1->pKey;
		second2 = item2->pKey;
	}
	else
	{
		first1 = item1->pKey;
		first2 = item2->pKey;
		second1 = item1->pValue;
		second2 = item2->pValue;
	}
	result = ListCompareHandle(first1, first2, ps, pDelegate, pErr);
	if( *pErr )
		return 0;
	if( result == 0 )
		result = ListCompareHandle(second1, second2, ps, pDelegate, pErr);
	if( *pErr )
		return 0;
	return result;
}

//------------------------------------------------------------------------------
// ItemFromKey
//------------------------------------------------------------------------------

static JILListItem* ItemFromKey(JILList* _this, JILHandle* pKey)
{
	JILListItem* item = NULL;
	switch( pKey->type )
	{
		case type_int:
			for(item = _this->pFirst; item != NULL; item = item->pNext)
			{
				if( item->pKey->type == type_int && JILGetIntHandle(item->pKey)->l == JILGetIntHandle(pKey)->l  )
					break;
			}
			break;
		case type_float:
			for(item = _this->pFirst; item != NULL; item = item->pNext)
			{
				if( item->pKey->type == type_float && JILGetFloatHandle(item->pKey)->f == JILGetFloatHandle(pKey)->f  )
					break;
			}
			break;
		case type_string:
			for(item = _this->pFirst; item != NULL; item = item->pNext)
			{
				if( item->pKey->type == type_string && JILString_Equal(JILGetStringHandle(item->pKey)->str, JILGetStringHandle(pKey)->str)  )
					break;
			}
			break;
	}
	return item;
}

//------------------------------------------------------------------------------
// ItemFromIndex
//------------------------------------------------------------------------------

static JILListItem* ItemFromIndex(JILList* _this, JILLong index)
{
	JILListItem* item;
	if( index < 0 )
		return NULL;
	for(item = _this->pFirst; item != NULL; item = item->pNext)
	{
		if( !index )
			break;
		index--;
	}
	return item;
}

//------------------------------------------------------------------------------
// ItemInsert
//------------------------------------------------------------------------------

static void ItemInsert(JILListItem* pItem, JILListItem* pNew)
{
	JILList* list = pItem->pList;
	if( list )
	{
		JILListItem* pNext = pItem;
		JILListItem* pPrev = pItem->pPrev;
		pNew->pList = list;
		pNew->pPrev = pPrev;
		pNew->pNext = pNext;
		pNext->pPrev = pNew;
		if( pPrev )
			pPrev->pNext = pNew;
		else
			list->pFirst = pNew;
		list->length++;
	}
}

//------------------------------------------------------------------------------
// ItemRemove
//------------------------------------------------------------------------------

static void ItemRemove(JILListItem* pItem)
{
	JILList* list = pItem->pList;
	if( list )
	{
		JILListItem* pPrev = pItem->pPrev;
		JILListItem* pNext = pItem->pNext;
		if( pPrev )
			pPrev->pNext = pNext;
		if( pNext )
			pNext->pPrev = pPrev;
		pItem->pList = NULL;
		pItem->pNext = pItem->pPrev = NULL;
		if( list->pFirst == pItem )
			list->pFirst = pNext;
		if( list->pLast == pItem )
			list->pLast = pPrev;
		list->length--;
	}
}

//------------------------------------------------------------------------------
// ItemAdd
//------------------------------------------------------------------------------

static void ItemAdd(JILList* _this, JILListItem* pItem)
{
	if( _this->pLast )
	{
		_this->pLast->pNext = pItem;
		pItem->pPrev = _this->pLast;
		_this->pLast = pItem;
	}
	else
	{
		_this->pFirst = pItem;
		_this->pLast = pItem;
	}
	pItem->pList = _this;
	_this->length++;
}

//------------------------------------------------------------------------------
// JILList_New
//------------------------------------------------------------------------------
///

JILList* JILList_New (JILState* pState)
{
	JILList* _this = (JILList*) pState->vmMalloc(pState, sizeof(JILList));
	memset(_this, 0, sizeof(JILList));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// JILList_Delete
//------------------------------------------------------------------------------
///

void JILList_Delete (JILList* _this)
{
	JILList_Clear(_this);
	_this->pState->vmFree( _this->pState, _this );
}

//------------------------------------------------------------------------------
// JILList_Copy
//------------------------------------------------------------------------------
/// Copy-construct this list from a source list.

void JILList_Copy (JILList* _this, const JILList* pSource)
{
	JILState* pState = pSource->pState;
	JILListItem* pItem;
	JILHandle* newKey;
	JILHandle* newValue;
	for( pItem = pSource->pFirst; pItem != NULL; pItem = pItem->pNext )
	{
		newKey = NTLCopyValueType(pState, pItem->pKey);
		newValue = NTLCopyValueType(pState, pItem->pValue);
		JILList_Add(_this, newKey, newValue);
		NTLFreeHandle(pState, newKey);
		NTLFreeHandle(pState, newValue);
	}
}

//------------------------------------------------------------------------------
// JILList_DeepCopy
//------------------------------------------------------------------------------
/// Deep-copy this list and return the new instance.

JILList* JILList_DeepCopy (const JILList* _this)
{
	JILState* pState = _this->pState;
	JILListItem* pItem;
	JILHandle* newKey;
	JILHandle* newValue;
	JILList* pNew = JILList_New(pState);
	for( pItem = _this->pFirst; pItem != NULL; pItem = pItem->pNext )
	{
		newKey = NTLCopyHandle(pState, pItem->pKey);
		newValue = NTLCopyHandle(pState, pItem->pValue);
		JILList_Add(pNew, newKey, newValue);
		NTLFreeHandle(pState, newKey);
		NTLFreeHandle(pState, newValue);
	}
	return pNew;
}

//------------------------------------------------------------------------------
// JILList_FromArray
//------------------------------------------------------------------------------
/// Construct a list from an array.

void JILList_FromArray (JILList* _this, const JILArray* pSource)
{
	JILState* pState = pSource->pState;
	JILLong i;
	JILHandle* newKey;
	for( i = 0; i < pSource->size; i++ )
	{
		newKey = NTLNewHandleForObject(pState, type_int, &i);
		JILList_Add(_this, newKey, pSource->ppHandles[i]);
		NTLFreeHandle(pState, newKey);
	}
}

//------------------------------------------------------------------------------
// JILList_Add
//------------------------------------------------------------------------------
///

void JILList_Add (JILList* _this, JILHandle* newKey, JILHandle* newValue)
{
	ItemAdd(_this, NewListItem(_this->pState, newKey, newValue));
}

//------------------------------------------------------------------------------
// JILList_AddOrSet
//------------------------------------------------------------------------------
///

void JILList_AddOrSet (JILList* _this, JILHandle* pKey, JILHandle* newValue)
{
	JILListItem* item = ItemFromKey(_this, pKey);
	if( item )
	{
		NTLReferHandle(_this->pState, newValue);
		NTLFreeHandle(_this->pState, item->pValue);
		item->pValue = newValue;
	}
	else
	{
		JILList_Add(_this, pKey, newValue);
	}
}

//------------------------------------------------------------------------------
// JILList_InsertBefore
//------------------------------------------------------------------------------
///

void JILList_InsertBefore (JILList* _this, JILHandle* beforeKey, JILHandle* newKey, JILHandle* newValue)
{
	JILListItem* pItem = ItemFromKey(_this, beforeKey);
	if( pItem )
	{
		ItemInsert(pItem, NewListItem(_this->pState, newKey, newValue));
	}
}

//------------------------------------------------------------------------------
// JILList_InsertAfter
//------------------------------------------------------------------------------
///

void JILList_InsertAfter (JILList* _this, JILHandle* afterKey, JILHandle* newKey, JILHandle* newValue)
{
	JILListItem* pItem = ItemFromKey(_this, afterKey);
	if( pItem )
	{
		pItem = pItem->pNext;
		if( pItem )
		{
			ItemInsert(pItem, NewListItem(_this->pState, newKey, newValue));
		}
		else
		{
			JILList_Add(_this, newKey, newValue);
		}
	}
}

//------------------------------------------------------------------------------
// JILList_InsertItem
//------------------------------------------------------------------------------
///

void JILList_InsertItem (JILListItem* item, JILHandle* newKey, JILHandle* newValue)
{
	if( item )
	{
		ItemInsert(item, NewListItem(item->pList->pState, newKey, newValue));
	}
}

//------------------------------------------------------------------------------
// JILList_Swap
//------------------------------------------------------------------------------
///

void JILList_Swap (JILList* _this, JILHandle* pKey1, JILHandle* pKey2)
{
	JILListItem* pItemA = ItemFromKey(_this, pKey1);
	JILListItem* pItemB = ItemFromKey(_this, pKey2);
	if( pItemA && pItemB && pItemA != pItemB )
	{
		JILHandle* key = pItemA->pKey;
		JILHandle* val = pItemA->pValue;
		pItemA->pKey = pItemB->pKey;
		pItemA->pValue = pItemB->pValue;
		pItemB->pKey = key;
		pItemB->pValue = val;
	}
}

//------------------------------------------------------------------------------
// JILList_MoveToFirst
//------------------------------------------------------------------------------
///

void JILList_MoveToFirst (JILList* _this, JILHandle* pKey)
{
	JILListItem* pItem = ItemFromKey(_this, pKey);
	if( pItem && _this->pFirst != pItem )
	{
		ItemRemove(pItem);
		ItemInsert(_this->pFirst, pItem);
	}
}

//------------------------------------------------------------------------------
// JILList_MoveToLast
//------------------------------------------------------------------------------
///

void JILList_MoveToLast (JILList* _this, JILHandle* pKey)
{
	JILListItem* pItem = ItemFromKey(_this, pKey);
	if( pItem && _this->pFirst != pItem )
	{
		ItemRemove(pItem);
		ItemAdd(_this, pItem);
	}
}

//------------------------------------------------------------------------------
// JILList_Remove
//------------------------------------------------------------------------------
///

void JILList_Remove (JILList* _this, JILHandle* pKey)
{
	JILListItem* pItem = ItemFromKey(_this, pKey);
	if( pItem )
	{
		ItemRemove(pItem);
		JILList_Release(_this->pState, pItem);
	}
}

//------------------------------------------------------------------------------
// JILList_Clear
//------------------------------------------------------------------------------
///

void JILList_Clear (JILList* _this)
{
	JILListItem* pItem;
	JILListItem* pNext;
	for( pItem = _this->pFirst; pItem != NULL; pItem = pNext )
	{
		pNext = pItem->pNext;
		pItem->pList = NULL;
		pItem->pNext = pItem->pPrev = NULL;
		JILList_Release(_this->pState, pItem);
	}
	_this->length = 0;
	_this->pFirst = NULL;
	_this->pLast = NULL;
}

//------------------------------------------------------------------------------
// JILList_Sort
//------------------------------------------------------------------------------
/// Sort all items in the list. Parameter 'mode' selects the sorting order:
/// <p>
/// 0 = sort by key first, ascending<br>
/// 1 = sort by key first, descending<br>
/// 2 = sort by value first, ascending<br>
/// 3 = sort by value first, descending<br>
/// </p>

JILError JILList_Sort (JILList* _this, JILLong mode, JILHandle* pDelegate)
{
	JILError result = JIL_No_Exception;
	if( _this->pFirst )
	{
		JILListItem* pIter = _this->pFirst;
		pIter = pIter->pNext;
		while( pIter )
		{
			JILListItem* pIter2 = pIter;
			while( pIter2->pPrev )
			{
				JILListItem* pPrev = pIter2->pPrev;
				JILLong eq = ListIsPredessor(pIter2, pPrev, mode, _this->pState, pDelegate, &result);
				if( result )
					return result;
				if( eq < 0 )
				{
					// swap item data
					JILHandle* key = pIter2->pKey;
					JILHandle* val = pIter2->pValue;
					pIter2->pKey = pPrev->pKey;
					pIter2->pValue = pPrev->pValue;
					pPrev->pKey = key;
					pPrev->pValue = val;
				}
				else
					break;
				pIter2 = pPrev;
			}
			pIter = pIter->pNext;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILList_Enumerate
//------------------------------------------------------------------------------
/// Calls a delegate for every item in this list.

JILError JILList_Enumerate (JILList* _this, JILHandle* pDelegate, JILHandle* pArgs)
{
	JILError err = JIL_No_Exception;
	if( _this->pFirst )
	{
		JILState* ps = _this->pState;
		JILListItem* pIter = _this->pFirst;
		while( pIter )
		{
			JILHandle* pResult = JILCallFunction(ps, pDelegate, 3, kArgHandle, pIter->pKey, kArgHandle, pIter->pValue, kArgHandle, pArgs);
			err = NTLHandleToError(ps, pResult);
			NTLFreeHandle(ps, pResult);
			if( err )
				break;
			pIter = pIter->pNext;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// JILList_ToArray
//------------------------------------------------------------------------------
/// Moves all data items from this list into a new array.

JILArray* JILList_ToArray (JILList* _this)
{
	JILListItem* pIter;
	JILHandle** ppHandles;
	JILArray* pArr = JILArray_NewNoInit(_this->pState, _this->length);
	ppHandles = pArr->ppHandles;
	for( pIter = _this->pFirst; pIter != NULL; pIter = pIter->pNext )
	{
		JILAddRef(pIter->pValue);
		*ppHandles++ = pIter->pValue;
	}
	return pArr;
}

//------------------------------------------------------------------------------
// JILList_ValueFromKey
//------------------------------------------------------------------------------
///

JILHandle* JILList_ValueFromKey (JILList* _this, JILHandle* pKey)
{
	JILHandle* result = NULL;
	JILListItem* pItem = ItemFromKey(_this, pKey);
	if( pItem )
		result = pItem->pValue;
	return result;
}

//------------------------------------------------------------------------------
// JILList_ValueFromIndex
//------------------------------------------------------------------------------
///

JILHandle* JILList_ValueFromIndex (JILList* _this, JILLong index)
{
	JILHandle* result = NULL;
	JILListItem* pItem = ItemFromIndex(_this, index);
	if( pItem )
		result = pItem->pValue;
	return result;
}

//------------------------------------------------------------------------------
// JILList_KeyFromIndex
//------------------------------------------------------------------------------
///

JILHandle* JILList_KeyFromIndex (JILList* _this, JILLong index)
{
	JILHandle* result = NULL;
	JILListItem* pItem = ItemFromIndex(_this, index);
	if( pItem )
		result = pItem->pKey;
	return result;
}

//------------------------------------------------------------------------------
// JILList_KeyExists
//------------------------------------------------------------------------------
///

JILLong JILList_KeyExists (JILList* _this, JILHandle* pKey)
{
	return ItemFromKey(_this, pKey) != NULL;
}

//------------------------------------------------------------------------------
// JILList_AddRef
//------------------------------------------------------------------------------
/// Add another reference to the given list item.

void JILList_AddRef(JILState* pState, JILListItem* pItem)
{
	if( pItem )
		pItem->numRef++;
}

//------------------------------------------------------------------------------
// JILList_Release
//------------------------------------------------------------------------------
/// Release a reference to the given list item.

void JILList_Release(JILState* pState, JILListItem* pItem)
{
	if( pItem && pItem->numRef )
	{
		pItem->numRef--;
		if( !pItem->numRef )
		{
			ItemRemove(pItem);
			NTLFreeHandle(pState, pItem->pKey);
			NTLFreeHandle(pState, pItem->pValue);
			pState->vmFree(pState, pItem);
		}
	}
}

//------------------------------------------------------------------------------
// JILList_Mark
//------------------------------------------------------------------------------
/// Mark the given list item.

JILError JILList_Mark(JILState* pState, JILListItem* pItem)
{
	JILError err = JIL_No_Exception;
	if( pItem )
	{
		err = NTLMarkHandle(pState, pItem->pKey);
		if( err )
			return err;
		err = NTLMarkHandle(pState, pItem->pValue);
		if( err )
			return err;
	}
	return err;
}

//------------------------------------------------------------------------------
// JILList_InvalidKey
//------------------------------------------------------------------------------
/// Helper function to check the type of a key handle.

JILBool JILList_InvalidKey(JILHandle* pKey)
{
	switch( pKey->type )
	{
		case type_int:
		case type_float:
		case type_string:
			return JILFalse;
	}
	return JILTrue;
}
