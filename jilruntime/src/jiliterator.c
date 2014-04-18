//------------------------------------------------------------------------------
// File: jiliterator.c										   (c) 2006 jewe.org
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
/// @file jiliterator.c
/// The built-in iterator object for iterating over the built-in list object.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jillist.h"
#include "jiltools.h"
#include "jilhandle.h"
#include "jilstring.h"
#include "jilmachine.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------

enum
{
	kCtor,
	kCctor,
	kFirst,
	kLast,
	kPrev,
	kNext,
	kInsert,
	kDelete,
	kKey,
	kValueGet,
	kValueSet,
	kValid,
	kIsFirst,
	kIsLast
};

//------------------------------------------------------------------------------
// class declaration list
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("This is the built-in iterator class. Iterators are used to sequentially navigate over lists and examine their items. To create an iterator for a list, you can just initialize an iterator variable with a list object: <pre>list myList = new list();\\nmyList.add(\\\"hello\\\", \\\"Hello World!\\\");\\nfor( iterator it = myList; it.valid; it.next() )\\n{\\n    println(it.value);\\n}</pre>")
	"method					iterator(list);" TAG("Constructs a new iterator for the specified list.")
	"method					iterator(const iterator);" TAG("Copy-constructs a new iterator from an existing one. The new iterator will reference the same item from the same list as the specified iterator.")
	"method					first();" TAG("Moves the iterator to the beginning of the list.")
	"method					last();" TAG("Moves the iterator to the end of the list.")
	"method					prev();" TAG("Moves the iterator to the previous item in the list. If there is no previous item, the iterator will become invalid.")
	"method					next();" TAG("Moves the iterator to the next item in the list. If there is no next item, the iterator will become invalid.")
	"method					insert(const var key, var value);" TAG("Inserts a new item at the iterator's current position in the list. If the iterator is currently invalid, this call has no effect.")
	"method					delete();" TAG("Deletes the item currently referenced by the iterator. The item will not be deleted right away, but will be marked for deletion. It will get deleted once the iterator moves to a different item. If the iterator is currently invalid, this call has no effect.")
	"accessor const var		key();" TAG("Returns the currently referenced item's key. If the iterator is currently invalid, returns null.")
	"accessor var			value();" TAG("Returns the currently referenced item's value. If the iterator is currently invalid, returns null.")
	"accessor 				value(var value);" TAG("Sets the currently referenced item's value. If the iterator is currently invalid, this call has no effect.")
	"accessor int			valid();" TAG("Returns true if the iterator is currently valid. If the iterator has moved beyond the beginning or end of the list, it will become invalid and this property will return false.")
	"accessor int			isFirst();" TAG("Returns true if the iterator is currently referencing the first item in the list.")
	"accessor int			isLast();" TAG("Returns true if the iterator is currently referencing the last item in the list.")
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"iterator";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"An iterator class for JewelScript.";
static const char*	kTimeStamp		=	"17.02.2006";

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int IteratorNew			(NTLInstance* pInst, JILIterator** ppObject);
static int IteratorDelete		(NTLInstance* pInst, JILIterator* _this);
static int IteratorMark			(NTLInstance* pInst, JILIterator* _this);
static int IteratorCallMember	(NTLInstance* pInst, int funcID, JILIterator* _this);

static JILIterator*	JILIterator_New		(JILState* pState);
static void			JILIterator_Delete	(JILIterator* _this);

//------------------------------------------------------------------------------
// JILIteratorProc
//------------------------------------------------------------------------------

JILError JILIteratorProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	JILError result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return IteratorNew(pInst, (JILIterator**) ppDataOut);
		case NTL_CallStatic:			return JIL_ERR_Unsupported_Native_Call;
		case NTL_MarkHandles:			return IteratorMark(pInst, (JILIterator*) pDataIn);
		case NTL_CallMember:			return IteratorCallMember(pInst, param, (JILIterator*) pDataIn);
		case NTL_DestroyObject:			return IteratorDelete(pInst, (JILIterator*) pDataIn);
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
// IteratorNew
//------------------------------------------------------------------------------

static int IteratorNew(NTLInstance* pInst, JILIterator** ppObject)
{
	*ppObject = JILIterator_New( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// IteratorDelete
//------------------------------------------------------------------------------

static int IteratorDelete(NTLInstance* pInst, JILIterator* _this)
{
	JILIterator_Delete( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// IteratorMark
//------------------------------------------------------------------------------

static int IteratorMark(NTLInstance* pInst, JILIterator* _this)
{
	JILError err = JIL_No_Exception;
	err = NTLMarkHandle(_this->pState, _this->pList);
	if( err )
		return err;
	err = JILList_Mark(_this->pState, _this->pItem);
	return err;
}

//------------------------------------------------------------------------------
// IteratorCallMember
//------------------------------------------------------------------------------

static int IteratorCallMember(NTLInstance* pInst, int funcID, JILIterator* _this)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILList* list;
	JILHandle* key = NULL;
	JILHandle* val = NULL;
	JILBool flag;
	switch( funcID )
	{
		case kCtor:
			_this->pList = NTLGetArgHandle(ps, 0);
			list = (JILList*) NTLHandleToObject(ps, type_list, _this->pList);
			_this->pItem = list->pFirst;
			JILList_AddRef(_this->pState, _this->pItem);
			break;
		case kCctor:
		{
			JILHandle* hSrc = NTLGetArgHandle(ps, 0);
			JILIterator* pSrc = (JILIterator*) NTLHandleToObject(ps, NTLInstanceTypeID(pInst), hSrc);
			_this->pList = pSrc->pList;
			_this->pItem = pSrc->pItem;
			_this->deleted = pSrc->deleted;
			NTLReferHandle(ps, _this->pList);
			JILList_AddRef(_this->pState, _this->pItem);
			NTLFreeHandle(ps, hSrc);
			break;
		}
		case kFirst:
			list = (JILList*) NTLHandleToObject(ps, type_list, _this->pList);
			JILList_Release(_this->pState, _this->pItem);
			_this->pItem = list->pFirst;
			JILList_AddRef(_this->pState, _this->pItem);
			_this->deleted = JILFalse;
			break;
		case kLast:
			list = (JILList*) NTLHandleToObject(ps, type_list, _this->pList);
			JILList_Release(_this->pState, _this->pItem);
			_this->pItem = list->pLast;
			JILList_AddRef(_this->pState, _this->pItem);
			_this->deleted = JILFalse;
			break;
		case kPrev:
			if( _this->pItem )
			{
				JILListItem* item = _this->pItem->pPrev;
				JILList_Release(_this->pState, _this->pItem);
				_this->pItem = item;
				JILList_AddRef(_this->pState, item);
			}
			_this->deleted = JILFalse;
			break;
		case kNext:
			if( _this->pItem )
			{
				JILListItem* item = _this->pItem->pNext;
				JILList_Release(_this->pState, _this->pItem);
				_this->pItem = item;
				JILList_AddRef(_this->pState, item);
			}
			_this->deleted = JILFalse;
			break;
		case kInsert:
			key = NTLGetArgHandle(ps, 0);
			val = NTLGetArgHandle(ps, 1);
			if( JILList_InvalidKey(key) )
				goto invalid_key;
			if( _this->pItem )
			{
				JILList_InsertItem(_this->pItem, key, val);
			}
			else
			{
				list = (JILList*) NTLHandleToObject(ps, type_list, _this->pList);
				JILList_Add(list, key, val);
			}
			NTLFreeHandle(ps, val);
			NTLFreeHandle(ps, key);
			break;
		case kDelete:
			if( _this->pItem && !_this->deleted )
			{
				JILList_Release(_this->pState, _this->pItem);
				_this->deleted = JILTrue;	// block multiple delete calls on same item
			}
			break;
		case kKey:
			NTLReturnHandle(ps, _this->pItem ? _this->pItem->pKey : NULL);
			break;
		case kValueGet:
			NTLReturnHandle(ps, _this->pItem ? _this->pItem->pValue : NULL);
			break;
		case kValueSet:
			if( _this->pItem )
			{
				val = NTLGetArgHandle(ps, 0);
				NTLFreeHandle(ps, _this->pItem->pValue);
				_this->pItem->pValue = val;
				val = NULL;
			}
			break;
		case kValid:
			NTLReturnInt(ps, _this->pItem != NULL);
			break;
		case kIsFirst:
			flag = JILFalse;
			if( _this->pItem )
				flag = (_this->pItem == _this->pItem->pList->pFirst);
			NTLReturnInt(ps, flag);
			break;
		case kIsLast:
			flag = JILFalse;
			if( _this->pItem )
				flag = (_this->pItem == _this->pItem->pList->pLast);
			NTLReturnInt(ps, flag);
			break;
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
	return JIL_VM_Unsupported_Type;
}

//------------------------------------------------------------------------------
// JILIterator_New
//------------------------------------------------------------------------------

static JILIterator* JILIterator_New (JILState* pState)
{
	JILIterator* _this = (JILIterator*) pState->vmMalloc(pState, sizeof(JILIterator));
	memset(_this, 0, sizeof(JILIterator));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// JILIterator_Delete
//------------------------------------------------------------------------------

static void JILIterator_Delete (JILIterator* _this)
{
	JILList_Release(_this->pState, _this->pItem);
	if( _this->pList )
		JILRelease(_this->pState, _this->pList);
	_this->pState->vmFree( _this->pState, _this );
}
