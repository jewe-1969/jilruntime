//------------------------------------------------------------------------------
// File: JILTable.c                                            (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// DISCLAIMER:
// -----------
//  THIS SOFTWARE IS SUBJECT TO THE LICENSE AGREEMENT FOUND IN "jilapi.h" AND
//  "COPYING". BY USING THIS SOFTWARE YOU IMPLICITLY DECLARE YOUR AGREEMENT TO
//  THE TERMS OF THIS LICENSE.
//
// Description:
// ------------
/// @file jiltable.c
/// A very simple and straightforward Hash Table for the JILRuntime.
/// The table also supports a "native mode", storing native void* pointers
/// instead of JILHandle pointers. The native mode table is used by the runtime
/// internally.
//------------------------------------------------------------------------------

#include "jilstdinc.h"
#include "jiltable.h"
#include "jilhandle.h"
#include "jilstring.h"
#include "jilarray.h"
#include "jillist.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// struct JILTableNode
//------------------------------------------------------------------------------
// An entry in the JILTable is constructed out of several nodes.

typedef struct JILTableNode	JILTableNode;
struct JILTableNode
{
	JILUnknown*		pData;
	JILTableNode*	p[16];
};

typedef enum
{
	kTableModeManaged,						// the table contains JILHandle objects and manages their reference counting
	kTableModeNativeUnmanaged,				// the table contains native pointers, but does not manage their lifetime (objects are just stored, but not destroyed)
	kTableModeNativeManaged					// the table contains native pointers and manages their lifetime (objects are stored and destroyed when no longer needed)
} JILTableMode;

typedef enum
{
	copy_shallow,
	copy_deep
} JILTableCopyMode;

struct JILTable
{
	JILTableDestructor		pDestructor;
	JILState*				pState;
	struct JILTableNode*	pNode;
	JILTableMode			mode;			// native mode is used internally by the runtime, see NTLTypeNameToTypeID()
};

typedef struct JILTableMergeData
{
	JILState*	pState;
	JILTable*	pTableL;
	JILTable*	pTableR;
	JILTable*	pResult;
	JILHandle*	hTableL;
	JILHandle*	hTableR;
	JILHandle*	hResult;
	JILHandle*	hDelegate;
} JILTableMergeData;

static JILError JILTable_Merge(JILTableMergeData* pData);
static void DestroyNodeRecursive(JILTable*, JILTableNode*);
static void CopyNodeRecursive(JILState*, const JILTableNode*, JILTableNode**, JILTableCopyMode);
static void CopyNodeStructure(JILState*, const JILTableNode*, JILTableNode**);
static JILError EnumerateNodeRecursive(JILState*, const JILTableNode*, JILHandle*, JILHandle*);
static JILError MarkNodeRecursive(JILState*, const JILTableNode*);
static JILLong FreeEmptyNodeRecursive(JILTable*, JILTableNode*);
static JILError AddToArrayRecursive(const JILTableNode*, JILArray*);
static JILError AddToListRecursive(const JILTableNode*, JILString*, JILLong, JILBool, JILList*);
static JILError MergeNodeRecursive(const JILTableNode*, JILTableMergeData*, JILString*, JILLong, JILBool);

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------

enum
{
	kCtor,
	kCctor,
	kCtorArr,
	kCtorList,

	kGetItem,
	kSetItem,
	kDeepCopy,
	kEnumerate,
	kCleanup,
	kToArray,
	kToList,
	kMerge
};

//------------------------------------------------------------------------------
// class declaration list
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("This is the built-in table class.")
	"delegate		enumerator(var element, var args);" TAG("Delegate type for the table::enumerate() and array::enumerate() methods.")
	"delegate		merger(const string key, const table t1, const table t2, table result);" TAG("Delegate type for the table::merge() function.")
	"method			table();" TAG("Constructs a new, empty hashtable.")
	"method			table(const table);" TAG("Copy constructs a new table from the specified one. The new table will be a shallow-copy, meaning values in the table will only be copied by reference.")
	"method			table(const array);" TAG("Constructs a new table from the specified array. The array is expected to be one-dimensional and have the following format: Every even element must be a string and is considered a key. Every odd element can be of any type and is considered a value.")
	"method			table(const list);" TAG("Constructs a new table from the specified list. The list's items will be added to the table by their keys, so keys should be unique for every item in the list. If the keys are not strings, they will be converted to strings by this function.")
	"method	var		get(const string key);" TAG("Retrieves a value from the table by the specified key. If no value exists in the table under the specified key, null is returned.")
	"method			set(const string key, var value);" TAG("Stores a value in the table under the specified key. If a value already exists under this key, it is overwritten. To clear a value in the table, you can just set it to null.")
	"method table	deepCopy();" TAG("Returns a deep-copy of this table. WARNING: All table data will be copied! This is a highly recursive operation. If the table contains script objects that have copy-constructors, this method can be very time consuming. It should only be called in cases where a shallow copy would not suffice.")
	"method			enumerate(enumerator fn, var args);" TAG("Calls the specified enumerator delegate for every value in this table. This is a highly recursive operation that can be very time consuming with large tables.")
	"method int		cleanup();" TAG("Frees all empty nodes in this table, releasing unneeded resources. This only affects internal infrastructure, all table data will remain intact. When storing and clearing large amounts of values in the table, calling this can improve memory footprint and performance of all other recursive table methods.")
	"method array	toArray();" TAG("Moves all values from this table into a new array. This is a highly recursive operation that can be very time consuming with complex tables.")
	"method list	toList();" TAG("Moves all keys and values from this table into a new list. This is a highly recursive operation that can be very time consuming with complex tables.")
	"function table merge(const table t1, const table t2, merger fn);" TAG("Merges the given tables according to the specified delegate and returns a new table. The function works as follows: First a reference table is created that contains all keys from both tables, but not their values. Then the reference table is iterated recursively. For every key in the reference table, the table::merger delegate is called. The current key, both source tables and a result table are passed to the delegate. The delegate defines how values from either or both source tables are stored in the result table.")
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*		kClassName		=	"table";
static const char*		kAuthorName		=	"www.jewe.org";
static const char*		kAuthorString	=	"A hashtable class for JewelScript.";
static const char*		kTimeStamp		=	"02/15/2007";

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static JILError TableNew		(NTLInstance* pInst, JILTable** ppObject);
static JILError TableDelete		(NTLInstance* pInst, JILTable* _this);
static JILError TableMark		(NTLInstance* pInst, JILTable* _this);
static JILError TableCallStatic	(NTLInstance* pInst, JILLong funcID);
static JILError TableCallMember	(NTLInstance* pInst, JILLong funcID, JILTable* _this);

//------------------------------------------------------------------------------
// JILTableProc
//------------------------------------------------------------------------------

JILError JILTableProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	JILError result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return TableNew(pInst, (JILTable**) ppDataOut);
		case NTL_CallStatic:			return TableCallStatic(pInst, param);
		case NTL_CallMember:			return TableCallMember(pInst, param, (JILTable*) pDataIn);
		case NTL_MarkHandles:			return TableMark(pInst, (JILTable*) pDataIn);
		case NTL_DestroyObject:			return TableDelete(pInst, (JILTable*) pDataIn);
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
// TableNew
//------------------------------------------------------------------------------

static JILError TableNew(NTLInstance* pInst, JILTable** ppObject)
{
	*ppObject = JILTable_NewManaged( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// TableDelete
//------------------------------------------------------------------------------

static JILError TableDelete(NTLInstance* pInst, JILTable* _this)
{
	JILTable_Delete( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// TableMark
//------------------------------------------------------------------------------

static JILError TableMark(NTLInstance* pInst, JILTable* _this)
{
	if( _this->mode != kTableModeManaged )
		return JIL_ERR_Unsupported_Native_Call;
	// we use the GC event to clean up the table
	FreeEmptyNodeRecursive(_this, _this->pNode);
	// now mark all items
	return MarkNodeRecursive(_this->pState, _this->pNode);
}

//------------------------------------------------------------------------------
// TableCallMember
//------------------------------------------------------------------------------

static JILError TableCallStatic(NTLInstance* pInst, JILLong funcID)
{
	JILError result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	switch( funcID )
	{
		case kMerge:
		{
			JILTableMergeData* pData = (JILTableMergeData*) ps->vmMalloc(ps, sizeof(JILTableMergeData));
			pData->pState = ps;
			pData->hTableL = NTLGetArgHandle(ps, 0);
			pData->hTableR = NTLGetArgHandle(ps, 1);
			pData->hDelegate = NTLGetArgHandle(ps, 2);
			pData->pResult = JILTable_NewManaged(ps);
			pData->pTableL = NTLHandleToObject(ps, type_table, pData->hTableL);
			pData->pTableR = NTLHandleToObject(ps, type_table, pData->hTableR);
			pData->hResult = NTLNewHandleForObject(ps, type_table, pData->pResult);
			result = JILTable_Merge(pData);
			NTLReturnHandle(ps, pData->hResult);
			NTLFreeHandle(ps, pData->hDelegate);
			NTLFreeHandle(ps, pData->hResult);
			NTLFreeHandle(ps, pData->hTableL);
			NTLFreeHandle(ps, pData->hTableR);
			ps->vmFree(ps, pData);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// TableCallMember
//------------------------------------------------------------------------------

static JILError TableCallMember(NTLInstance* pInst, JILLong funcID, JILTable* _this)
{
	JILError result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILHandle* pHandle;
	switch( funcID )
	{
		case kCtor:
			break;
		case kCctor:
			pHandle = NTLGetArgHandle(ps, 0);
			JILTable_Copy(_this, NTLHandleToObject(ps, NTLInstanceTypeID(pInst), pHandle));
			NTLFreeHandle(ps, pHandle);
			break;
		case kCtorArr:
			pHandle = NTLGetArgHandle(ps, 0);
			result = JILTable_FromArray(_this, NTLHandleToObject(ps, type_array, pHandle));
			NTLFreeHandle(ps, pHandle);
			break;
		case kCtorList:
			pHandle = NTLGetArgHandle(ps, 0);
			result = JILTable_FromList(_this, NTLHandleToObject(ps, type_list, pHandle));
			NTLFreeHandle(ps, pHandle);
			break;
		case kGetItem:
			NTLReturnHandle(ps, JILTable_GetItem(_this, NTLGetArgString(ps, 0)));
			break;
		case kSetItem:
			pHandle = NTLGetArgHandle(ps, 1);
			JILTable_SetItem(_this, NTLGetArgString(ps, 0), pHandle);
			NTLFreeHandle(ps, pHandle);
			break;
		case kDeepCopy:
		{
			JILTable* pNew = JILTable_DeepCopy(_this);
			JILHandle* ph = NTLNewHandleForObject(ps, NTLInstanceTypeID(pInst), pNew);
			NTLReturnHandle(ps, ph);
			NTLFreeHandle(ps, ph);
			break;
		}
		case kEnumerate:
		{
			JILHandle* pDel = NTLGetArgHandle(ps, 0);
			JILHandle* pArg = NTLGetArgHandle(ps, 1);			
			result = JILTable_Enumerate(_this, pDel, pArg);
			NTLFreeHandle(ps, pArg);
			NTLFreeHandle(ps, pDel);
			break;
		}
		case kToArray:
		{
			JILHandle* pH;
			JILArray* pArray = JILArray_New(ps);
			result = JILTable_ToArray(_this, pArray);
			pH = NTLNewHandleForObject(ps, type_array, pArray);
			NTLReturnHandle(ps, pH);
			NTLFreeHandle(ps, pH);
			break;
		}
		case kToList:
		{
			JILHandle* pH;
			JILList* pList = JILList_New(ps);
			result = JILTable_ToList(_this, pList);
			pH = NTLNewHandleForObject(ps, type_list, pList);
			NTLReturnHandle(ps, pH);
			NTLFreeHandle(ps, pH);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILTable_DefaultDestructor
//------------------------------------------------------------------------------
// Default destructor callback. Just uses free() on the item to destroy.

static void JILTable_DefaultDestructor(JILUnknown* pItem)
{
	free(pItem);
}

//------------------------------------------------------------------------------
// JILTable_NewManaged
//------------------------------------------------------------------------------
// Create a new managed table.

JILTable* JILTable_NewManaged(JILState* pVM)
{
	JILTable* _this = (JILTable*) pVM->vmMalloc(pVM, sizeof(JILTable));
	_this->pDestructor = NULL; // destructor callback not used in managed mode
	_this->pState = pVM;
	_this->pNode = NULL;
	_this->mode = kTableModeManaged;
	return _this;
}

//------------------------------------------------------------------------------
// JILTable_NewNativeUnmanaged
//------------------------------------------------------------------------------
// Create a new unmanaged native table.

JILTable* JILTable_NewNativeUnmanaged(JILState* pVM)
{
	JILTable* _this = (JILTable*) pVM->vmMalloc(pVM, sizeof(JILTable));
	_this->pDestructor = JILTable_DefaultDestructor;
	_this->pState = pVM;
	_this->pNode = NULL;
	_this->mode = kTableModeNativeUnmanaged;
	return _this;
}

//------------------------------------------------------------------------------
// JILTable_NewNativeManaged
//------------------------------------------------------------------------------
// Create a new managed native table.

JILTable* JILTable_NewNativeManaged(JILState* pVM, JILTableDestructor pDestructor)
{
	JILTable* _this = (JILTable*) pVM->vmMalloc(pVM, sizeof(JILTable));
	_this->pDestructor = (pDestructor == NULL) ? JILTable_DefaultDestructor : pDestructor;
	_this->pState = pVM;
	_this->pNode = NULL;
	_this->mode = kTableModeNativeManaged;
	return _this;
}

//------------------------------------------------------------------------------
// JILTable_Delete
//------------------------------------------------------------------------------
// Destroy a table.

void JILTable_Delete(JILTable* _this)
{
	DestroyNodeRecursive( _this, _this->pNode );
	_this->pState->vmFree(_this->pState, _this);
}

//------------------------------------------------------------------------------
// JILTable_Copy
//------------------------------------------------------------------------------
// Copy-construct a table. This does NOT work in native mode.

void JILTable_Copy(JILTable* _this, const JILTable* pSrc)
{
	if( pSrc->mode != kTableModeManaged )
		return;
	_this->mode = pSrc->mode;
	CopyNodeRecursive(_this->pState, pSrc->pNode, &(_this->pNode), JILFalse);
}

//------------------------------------------------------------------------------
// JILTable_DeepCopy
//------------------------------------------------------------------------------
// Deep-copy this table and return the new instance. This does NOT work in native mode.

JILTable* JILTable_DeepCopy(const JILTable* _this)
{
	JILTable* pNew;
	if( _this->mode != kTableModeManaged )
		return NULL;
	pNew = JILTable_NewManaged(_this->pState);
	CopyNodeRecursive(pNew->pState, _this->pNode, &(pNew->pNode), JILTrue);
	return pNew;
}

//------------------------------------------------------------------------------
// JILTable_FromArray
//------------------------------------------------------------------------------
// Construct a table from an array. The table is interpreted as a list of
// key / value pairs: { "key1", value1, "key2", value2 }
// This does NOT work in native mode.

JILError JILTable_FromArray(JILTable* _this, const JILArray* arr)
{
	JILLong i;
	JILString* newKey;
	JILHandle* newHandle;
	JILState* ps = _this->pState;
	if( _this->mode != kTableModeManaged )
		return JIL_ERR_Unsupported_Native_Call;
	// if number of elements in array not even, fail!
	if( (arr->size & 1) == 1 )
		return JIL_ERR_Illegal_Argument;
	for (i = 0; i < arr->size; i += 2)
	{
		newKey = (JILString*)NTLHandleToObject(ps, type_string, arr->ppHandles[i]);
		if (newKey == NULL)
			return JIL_ERR_Illegal_Argument;
		newHandle = NTLCopyValueType(ps, arr->ppHandles[i + 1]);
		JILTable_SetItem(_this, newKey->string, newHandle);
		NTLFreeHandle(ps, newHandle);
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILTable_FromList
//------------------------------------------------------------------------------
// Constructs a new table from the specified list. The list's keys should be
// strings, otherwise they will be converted to strings by this function.
// This does NOT work in native mode.

JILError JILTable_FromList(JILTable* _this, const JILList* pList)
{
	JILString* newKey;
	JILListItem* pIter;
	JILState* ps = _this->pState;
	JILChar buf[32];
	if( _this->mode != kTableModeManaged )
		return JIL_ERR_Unsupported_Native_Call;
	for (pIter = pList->pFirst; pIter != NULL; pIter = pIter->pNext)
	{
		if (pIter->pKey->type == type_string)
		{
			newKey = (JILString*)NTLHandleToObject(ps, type_string, pIter->pKey);
			if (newKey == NULL)
				return JIL_ERR_Illegal_Argument;
			JILTable_SetItem(_this, newKey->string, pIter->pValue);
		}
		else if (pIter->pKey->type == type_int)
		{
			JILSnprintf(buf, 32, "%d", JILGetIntHandle(pIter->pKey)->l);
			JILTable_SetItem(_this, buf, pIter->pValue);
		}
		else if (pIter->pKey->type == type_float)
		{
			JILSnprintf(buf, 32, "%f", JILGetFloatHandle(pIter->pKey)->f);
			JILTable_SetItem(_this, buf, pIter->pValue);
		}
		else
			return JIL_ERR_Illegal_Argument;
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILTable_Enumerate
//------------------------------------------------------------------------------
// Call a delegate for every element in the table. This does NOT work in native mode.

JILError JILTable_Enumerate(JILTable* _this, JILHandle* pDel, JILHandle* pArgs)
{
	if( _this->mode != kTableModeManaged )
		return JIL_ERR_Unsupported_Native_Call;
	return EnumerateNodeRecursive(_this->pState, _this->pNode, pDel, pArgs);
}

//------------------------------------------------------------------------------
// JILTable_Cleanup
//------------------------------------------------------------------------------
// Free all empty nodes in the table to consolidate resources.

JILLong JILTable_Cleanup(JILTable* _this)
{
	JILLong isFree = FreeEmptyNodeRecursive(_this, _this->pNode);
	if( isFree )
	{
		_this->pNode = NULL;
	}
	return isFree;
}

//------------------------------------------------------------------------------
// JILTable_ToArray
//------------------------------------------------------------------------------

JILError JILTable_ToArray(JILTable* _this, JILArray* pArray)
{
	if (_this->mode != kTableModeManaged)
		return JIL_ERR_Unsupported_Native_Call;
	return AddToArrayRecursive(_this->pNode, pArray);
}

//------------------------------------------------------------------------------
// JILTable_ToList
//------------------------------------------------------------------------------

JILError JILTable_ToList(JILTable* _this, JILList* pList)
{
	JILError err;
	JILString* pKey;
	JILLong i;
	if (_this->mode != kTableModeManaged)
		return JIL_ERR_Unsupported_Native_Call;
	pKey = JILString_New(_this->pState);
	for( i = 0; i < 16; i++ )
	{
		if( _this->pNode->p[i] )
		{
			err = AddToListRecursive(_this->pNode->p[i], pKey, i, JILFalse, pList);
			if( err )
				break;
		}
	}
	JILString_Delete(pKey);
	return err;
}

//------------------------------------------------------------------------------
// JILTable_GetItem
//------------------------------------------------------------------------------
// Get an item from the table.

JILUnknown* JILTable_GetItem(JILTable* _this, const JILChar* pKey)
{
	JILUnknown* pResult = NULL;
	if( pKey && *pKey )
	{
		JILLong c;
		JILTableNode* pNode;

		if( _this->pNode == NULL )
			goto done;
		pNode = _this->pNode;
		while( (c = (unsigned char) *pKey++) )
		{
			pNode = pNode->p[c >> 4];
			if( !pNode )
				goto done;
			pNode = pNode->p[c & 15];
			if( !pNode )
				goto done;
		}
		if( !pNode->pData )
			goto done;
		pResult = pNode->pData;
	}
done:
	if( !pResult && _this->mode == kTableModeManaged )
		pResult = JILGetNullHandle(_this->pState);
	return pResult;
}

//------------------------------------------------------------------------------
// JILTable_SetItem
//------------------------------------------------------------------------------
// Put an item into the table.

void JILTable_SetItem(JILTable* _this, const JILChar* pKey, JILUnknown* pData)
{
	if( pKey && *pKey )
	{
		JILLong c;
		JILTableNode* pNode;
		JILTableNode** ppNode;
		JILState* pVM = _this->pState;

		if( _this->pNode == NULL )
		{
			// if we don't have a root node yet, allocate it now
			_this->pNode = (JILTableNode*) pVM->vmMalloc(pVM, sizeof(JILTableNode));
			memset(_this->pNode, 0, sizeof(JILTableNode));
		}
		pNode = _this->pNode;
		while( (c = (unsigned char) *pKey++) )
		{
			ppNode = pNode->p + (c >> 4);
			if( *ppNode == NULL )
			{
				*ppNode = (JILTableNode*) pVM->vmMalloc(pVM, sizeof(JILTableNode));
				memset(*ppNode, 0, sizeof(JILTableNode));
			}
			ppNode = (*ppNode)->p + (c & 15);
			if( *ppNode == NULL )
			{
				*ppNode = (JILTableNode*) pVM->vmMalloc(pVM, sizeof(JILTableNode));
				memset(*ppNode, 0, sizeof(JILTableNode));
			}
			pNode = *ppNode;
		}
		if( _this->mode == kTableModeManaged )
		{
			JILHandle* ph = (JILHandle*)pData;
			if( NTLHandleToTypeID(pVM, ph) == type_null )
				ph = NULL;
			else
				NTLReferHandle(pVM, ph);
			if( pNode->pData )
				NTLFreeHandle(pVM, ((JILHandle*)pNode->pData));
			pNode->pData = ph;
		}
		else if( _this->mode == kTableModeNativeManaged )
		{
			if( pNode->pData )
				_this->pDestructor(pNode->pData);
			pNode->pData = pData;
		}
		else if( _this->mode == kTableModeNativeUnmanaged )
		{
			pNode->pData = pData;
		}
	}
}

//------------------------------------------------------------------------------
// JILTable_Merge
//------------------------------------------------------------------------------

static JILError JILTable_Merge(JILTableMergeData* pData)
{
	JILError err = JIL_No_Exception;
	JILString* pKey;
	JILLong i;
	JILTable* pReference;
	pReference = JILTable_NewNativeUnmanaged(pData->pState);
	CopyNodeStructure(pData->pState, pData->pTableL->pNode, &pReference->pNode);
	CopyNodeStructure(pData->pState, pData->pTableR->pNode, &pReference->pNode);
	pKey = JILString_New(pData->pState);
	for( i = 0; i < 16; i++ )
	{
		if( pReference->pNode->p[i] )
		{
			err = MergeNodeRecursive(pReference->pNode->p[i], pData, pKey, i, JILFalse);
			if( err )
				break;
		}
	}
	JILString_Delete(pKey);
	JILTable_Delete(pReference);
	return err;
}

//------------------------------------------------------------------------------
// DestroyNodeRecursive
//------------------------------------------------------------------------------
// Helper function to destroy the table.

static void DestroyNodeRecursive(JILTable* pTable, JILTableNode* pNode)
{
	if( pNode )
	{
		JILLong i;
		for( i = 0; i < 16; i++ )
		{
			DestroyNodeRecursive(pTable, pNode->p[i]);
			pNode->p[i] = NULL;
		}
		if( pNode->pData )
		{
			if( pTable->mode == kTableModeManaged )
			{
				NTLFreeHandle(pTable->pState, pNode->pData);
			}
			else if( pTable->mode == kTableModeNativeManaged )
			{
				pTable->pDestructor(pNode->pData);
			}
			pNode->pData = NULL;
		}
		pTable->pState->vmFree(pTable->pState, pNode);
	}
}

//------------------------------------------------------------------------------
// CopyNodeRecursive
//------------------------------------------------------------------------------
// Helper function to copy the table. Does NOT work in native mode.

static void CopyNodeRecursive(JILState* pVM, const JILTableNode* pSrc, JILTableNode** ppDest, JILTableCopyMode mode)
{
	if( pSrc )
	{
		JILLong i;
		JILTableNode* pNode;
		pNode = (JILTableNode*) pVM->vmMalloc(pVM, sizeof(JILTableNode));
		memset(pNode, 0, sizeof(JILTableNode));
		*ppDest = pNode;
		for( i = 0; i < 16; i++ )
			CopyNodeRecursive(pVM, pSrc->p[i], &(pNode->p[i]), mode);
		if( pSrc->pData )
		{
			if( mode == copy_deep )
				pNode->pData = NTLCopyHandle(pVM, pSrc->pData);
			else
				pNode->pData = NTLCopyValueType(pVM, pSrc->pData);
		}
	}
}

//------------------------------------------------------------------------------
// CopyNodeStructure
//------------------------------------------------------------------------------
// Recursively copies the table structure, but not the table data. This is meant
// for unmanaged native tables only!

static void CopyNodeStructure(JILState* pVM, const JILTableNode* pSrc, JILTableNode** ppDest)
{
	if( pSrc )
	{
		JILLong i;
		JILTableNode* pNode = *ppDest;
		if( pNode == NULL )
		{
			pNode = (JILTableNode*) pVM->vmMalloc(pVM, sizeof(JILTableNode));
			memset(pNode, 0, sizeof(JILTableNode));
			*ppDest = pNode;
		}
		for( i = 0; i < 16; i++ )
			CopyNodeStructure(pVM, pSrc->p[i], &(pNode->p[i]));
		if( pSrc->pData )
			pNode->pData = pNode;
	}
}

//------------------------------------------------------------------------------
// EnumerateNodeRecursive
//------------------------------------------------------------------------------
// Calls a delegate for every item in this table. Obviously does not work in native mode.

static JILError EnumerateNodeRecursive(JILState* pVM, const JILTableNode* pSrc, JILHandle* pDelegate, JILHandle* pArgs)
{
	JILError err = JIL_No_Exception;
	if( pSrc )
	{
		JILLong i;
		for( i = 0; i < 16; i++ )
		{
			err = EnumerateNodeRecursive(pVM, pSrc->p[i], pDelegate, pArgs);
			if( err )
				return err;
		}
		if( pSrc->pData )
		{
			JILHandle* pResult = JILCallFunction(pVM, pDelegate, 2, kArgHandle, pSrc->pData, kArgHandle, pArgs);
			err = NTLHandleToError(pVM, pResult);
			NTLFreeHandle(pVM, pResult);
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// MarkNodeRecursive
//------------------------------------------------------------------------------
// Marks all handles in this table. Obviously does not work in native mode.

static JILError MarkNodeRecursive(JILState* pVM, const JILTableNode* pSrc)
{
	JILError err = JIL_No_Exception;
	if( pSrc )
	{
		JILLong i;
		for( i = 0; i < 16; i++ )
		{
			JILError e = MarkNodeRecursive(pVM, pSrc->p[i]);
			if( e )
				err = e;
		}
		if( pSrc->pData )
		{
			JILError e = NTLMarkHandle(pVM, pSrc->pData);
			if( e )
				err = e;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// FreeEmptyNodeRecursive
//------------------------------------------------------------------------------
// Helper function to free empty table nodes.

static JILLong FreeEmptyNodeRecursive(JILTable* pTable, JILTableNode* pNode)
{
	JILLong nodeIsFree = JILFalse;
	if( pNode )
	{
		JILLong i;
		JILLong freeSub = 0;
		for( i = 0; i < 16; i++ )
		{
			if( FreeEmptyNodeRecursive(pTable, pNode->p[i]) )
			{
				pNode->p[i] = NULL;
				freeSub++;
			}
		}
		if( freeSub == 16 && pNode->pData == NULL )
		{
			pTable->pState->vmFree(pTable->pState, pNode);
			nodeIsFree = JILTrue;
		}
	}
	return nodeIsFree;
}

//------------------------------------------------------------------------------
// AddToArrayRecursive
//------------------------------------------------------------------------------
// Helper function to add all table items to an array.

static JILError AddToArrayRecursive(const JILTableNode* pSrc, JILArray* pArray)
{
	JILError err = JIL_No_Exception;
	JILLong i;
	if( pSrc )
	{
		for( i = 0; i < 16; i++ )
		{
			err = AddToArrayRecursive(pSrc->p[i], pArray);
			if( err )
				return err;
		}
		if( pSrc->pData )
		{
			JILArray_ArrMove(pArray, pSrc->pData);
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// AddToListRecursive
//------------------------------------------------------------------------------
// Helper function to add all table items to a list.

static JILError AddToListRecursive(const JILTableNode* pSrc, JILString* pKey, JILLong nibble, JILBool isByte, JILList* pList)
{
	JILError err = JIL_No_Exception;
	JILLong i;
	JILHandle* newKey;
	JILString* keyStr;
	JILString* pKeyCopy = pKey;
	JILState* ps = pList->pState;
	JILChar buf[2] = {0};
	if( isByte )
	{
		pKeyCopy = JILString_Copy(pKey);
		buf[0] = nibble;
		JILString_AppendCStr(pKeyCopy, buf);
	}
	for( i = 0; i < 16; i++ )
	{
		if( pSrc->p[i] )
		{
			err = AddToListRecursive(pSrc->p[i], pKeyCopy, (nibble << 4 | i) & 0xFF, !isByte, pList);
			if( err )
				goto exit;
		}
	}
	if( pSrc->pData )
	{
		keyStr = JILString_Copy(pKeyCopy);
		newKey = NTLNewHandleForObject(ps, type_string, keyStr);
		JILList_Add(pList, newKey, pSrc->pData);
		NTLFreeHandle(ps, newKey);
	}

exit:
	if( isByte )
	{
		JILString_Delete(pKeyCopy);
	}
	return err;
}

//------------------------------------------------------------------------------
// MergeNodeRecursive
//------------------------------------------------------------------------------

static JILError MergeNodeRecursive(const JILTableNode* pNode, JILTableMergeData* pData, JILString* pKey, JILLong nibble, JILBool isByte)
{
	JILError err = JIL_No_Exception;
	JILLong i;
	JILHandle* hNewKey;
	JILHandle* hException;
	JILString* keyStr;
	JILString* pKeyCopy = pKey;
	JILState* ps = pData->pState;
	JILChar buf[2] = {0};
	if( isByte )
	{
		pKeyCopy = JILString_Copy(pKey);
		buf[0] = nibble;
		JILString_AppendCStr(pKeyCopy, buf);
	}
	for( i = 0; i < 16; i++ )
	{
		if( pNode->p[i] )
		{
			err = MergeNodeRecursive(pNode->p[i], pData, pKeyCopy, (nibble << 4 | i) & 0xFF, !isByte);
			if( err )
				goto exit;
		}
	}
	if( pNode->pData )
	{
		keyStr = JILString_Copy(pKeyCopy);
		hNewKey = NTLNewHandleForObject(ps, type_string, keyStr);
		hException = JILCallFunction(ps, pData->hDelegate, 4,
			kArgHandle, hNewKey,
			kArgHandle, pData->hTableL,
			kArgHandle, pData->hTableR,
			kArgHandle, pData->hResult);
		err = NTLHandleToError(ps, hException);
		NTLFreeHandle(ps, hException);
		NTLFreeHandle(ps, hNewKey);
	}

exit:
	if( isByte )
	{
		JILString_Delete(pKeyCopy);
	}
	return err;
}
