//------------------------------------------------------------------------------
// File: JILArray.c                                         (c) 2004 jewe.org
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
/// @file jilarray.c
///	This defines the built-in array object the virtual machine uses.
/// The built-in array is a primitive data type, like the long and float types,
/// and does only support very basic operations. However, more functions might
/// be added here in the future, to make using and manipulating the array
/// object from native typelibs or the application using the jilruntime library
/// easier.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilarray.h"
#include "jilstring.h"
#include "jilhandle.h"
#include "jilmachine.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// array method index numbers
//------------------------------------------------------------------------------

enum
{
	// constructor
	kCtor,
	kConvStr,

	// accessors
	kLength,
	kTop,

	// methods
	kDeepCopy,
	kInsert,
	kInsertItem,
	kRemove,
	kSubArray,
	kSwap,
	kFormat,
	kToString,
	kProcess,
	kEnumerate,
	kPushItem,
	kPopItem,
	kSort,
	kIndexOf
};

//------------------------------------------------------------------------------
// array class declaration
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("This is the built-in array class. JewelScript arrays can dynamically grow depending on the index used to access elements from it. In general, setting an array element with an index that is out of range, will cause the array to grow to the required number of elements. Getting an array index with an index that is out of range will NOT resize the array, but return null instead. So one should be cautious to not use invalid array indices. The array index is a signed 32-bit value, so an array is limited to about 2 billion elements. Operator += can be used to add new elements to an array, as well as append an array to an array.")
	"delegate			enumerator(var element, var args);" TAG("Delegate type for the array::enumerate() method.")
	"delegate var		processor(var element, var args);" TAG("Delegate type for the array::process() method.")
	"delegate int		comparator(const var value1, const var value2);" TAG("Delegate for the array::sort() method. The delegate should handle null-references and unmatching types gracefully. It should return -1 if value1 is less than value2, 1 if it is greater, and 0 if they are equal.")
	"method				array();" TAG("Creates a new, empty array.")
	"explicit string	convertor();" TAG("Recursively converts all convertible elements of this array into a string. This method can be slow for very complex multi-dimensional arrays, so an explicit type cast is required to confirm that the conversion is really wanted.")
	"accessor int		length();" TAG("Returns the length of this array in elements.")
	"accessor var		top();" TAG("Returns the reference of the top level element in this array. The top level element is the element with the highest index. The operation is A[A.length - 1]. If the array is empty, this method will return null. If this array is multi-dimensional, this may return a sub-array.")
	"method array		deepCopy();" TAG("Recursively creates a deep-copy of this array. WARNING: All element data will be copied! If the array is multi-dimensional and / or contains script objects that have copy-constructors, this method can be very time consuming. It should only be called in cases where a shallow-copy would not suffice.")
	"method array		insert(const array src, const int index);" TAG("Inserts the specified array into this array at the given index. The result will be returned as a new array.")
	"method array		insertItem(var item, const int index);" TAG("Inserts the specified element into this array at the given index. The result will be returned as a new array.")
	"method array		remove(const int index, const int length);" TAG("Removes the specified range of elements from this array and returns the result as a new array.")
	"method array		subArray(const int index, const int length);" TAG("Returns the specified range of elements from this array as a new array.")
	"method				swap(const int index1, const int index2);" TAG("Exchanges the positions of the specified elements in the array.")
	"method string		format(const string format);" TAG("Formats this array into a string. The format string must contain ANSI format identifiers. Every subsequent identifier in the format string is associated with the next array element. This only works with one dimensional arrays.")
	"method string		toString();" TAG("Recursively converts all convertible elements of this array into a string. This method can be slow for very complex multi-dimensional arrays.")
	"method array		process(processor fn, var args);" TAG("Calls a delegate for every element in this array. The delegate may process the element and return it. It may also return null. The function will concatenate all non-null results of the delegate into a new array.")
	"method				enumerate(enumerator fn, var args);" TAG("Calls a delegate for every element in this array.")
	"method				push(var item);" TAG("Adds the specified item to the end of this array. This actually modifies the array and allows to use it like a stack.")
	"method	var			pop();" TAG("Removes the top level element from this array and returns it. If the array is currently empty, null is returned. This actually modifies the array and allows to use it like a stack.")
	"method	array		sort(comparator fn);" TAG("Sorts this array's elements depending on the specified comparator delegate.")
	"method int			indexOf(var item, const int index);" TAG("Searches 'item' in a one-dimensional array and returns the index of the first occurrence. The search starts at the given 'index' position. Integers, floats and strings will be compared by value, all other types will be compared by reference. If no element is found, -1 is returned.")
;

//------------------------------------------------------------------------------
// string class constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"array";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Built-in array class for JewelScript.";
static const char*	kTimeStamp		=	"06/15/2005";

static const int	kStaticBufferSize = 4096;

//------------------------------------------------------------------------------
// forward declare proc message handler functions
//------------------------------------------------------------------------------

static int ArrayInitialize	(NTLInstance* pInst);
static int ArrayNew			(NTLInstance* pInst, JILArray** ppObject);
static int ArrayMark		(NTLInstance* pInst, JILArray* _this);
static int ArrayCallStatic	(NTLInstance* pInst, int funcID);
static int ArrayCallMember	(NTLInstance* pInst, int funcID, JILArray* _this);
static int ArrayDelete		(NTLInstance* pInst, JILArray* _this);
static int ArrayTerminate	(NTLInstance* pInst);

//------------------------------------------------------------------------------
// JILArrayProc
//------------------------------------------------------------------------------

JILError JILArrayProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	JILError result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			return ArrayInitialize(pInst);
		case NTL_NewObject:				return ArrayNew(pInst, (JILArray**) ppDataOut);
		case NTL_MarkHandles:			return ArrayMark(pInst, (JILArray*) pDataIn);
		case NTL_CallStatic:			return ArrayCallStatic(pInst, param);
		case NTL_CallMember:			return ArrayCallMember(pInst, param, (JILArray*) pDataIn);
		case NTL_DestroyObject:			return ArrayDelete(pInst, (JILArray*) pDataIn);
		case NTL_Terminate:				return ArrayTerminate(pInst);
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
// ArrayInitialize
//------------------------------------------------------------------------------

static int ArrayInitialize(NTLInstance* pInst)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// ArrayNew
//------------------------------------------------------------------------------

static int ArrayNew(NTLInstance* pInst, JILArray** ppObject)
{
	*ppObject = JILArray_New( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// ArrayMark
//------------------------------------------------------------------------------

static int ArrayMark(NTLInstance* pInst, JILArray* _this)
{
	JILLong i;
	JILError err = JIL_No_Exception;
	for( i = 0; i < _this->size; i++ )
	{
		err = NTLMarkHandle(_this->pState, _this->ppHandles[i]);
		if( err )
			break;
	}
	return err;
}

//------------------------------------------------------------------------------
// ArrayCallStatic
//------------------------------------------------------------------------------

static int ArrayCallStatic(NTLInstance* pInst, int funcID)
{
	return JIL_ERR_Unsupported_Native_Call;
}

//------------------------------------------------------------------------------
// ArrayCallMember
//------------------------------------------------------------------------------

static int ArrayCallMember(NTLInstance* pInst, int funcID, JILArray* _this)
{
	int result = JIL_No_Exception;
	JILArray* pArr;
	JILHandle* hArr;
	JILString* pStr;
	JILHandle* hStr;
	JILState* ps = NTLInstanceGetVM(pInst);

	switch( funcID )
	{
		case kCtor:
			// nothing to do in standard ctor
			break;
		case kConvStr:
		case kToString:
		{
			pStr = JILArray_ToString( _this );
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		}
		case kLength:
			NTLReturnInt(ps, _this->size);
			break;
		case kTop:
			if( _this->size > 0 )
				NTLReturnHandle(ps, JILArray_GetFrom(_this, _this->size - 1));
			else
				NTLReturnHandle(ps, NULL);
			break;
		case kDeepCopy:
			pArr = JILArray_DeepCopy(_this);
			hArr = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			break;
		case kInsert:
			pArr = JILArray_Insert(_this,
				NTLGetArgObject(ps, 0, type_array),
				NTLGetArgInt(ps, 1 ));
			hArr = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			break;
		case kInsertItem:
			hStr = NTLGetArgHandle(ps, 0);
			pArr = JILArray_InsertItem(_this,
				hStr,
				NTLGetArgInt(ps, 1 ));
			hArr = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			NTLFreeHandle(ps, hStr);
			break;
 		case kRemove:
			pArr = JILArray_Remove(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hArr = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			break;
 		case kSubArray:
			pArr = JILArray_SubArray(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hArr = NTLNewHandleForObject(ps, type_array, pArr);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			break;
 		case kSwap:
			JILArray_Swap(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			break;
		case kFormat:
			pStr = JILArray_Format(_this,
				NTLGetArgObject(ps, 0, type_string));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kProcess:
		{
			JILHandle* hDel = NTLGetArgHandle(ps, 0);
			JILHandle* hArg = NTLGetArgHandle(ps, 1);
			result = JILArray_Process(_this, hDel, hArg, &pArr);
			if( result == JIL_No_Exception )
			{
				hArr = NTLNewHandleForObject(ps, type_array, pArr);
				NTLReturnHandle(ps, hArr);
				NTLFreeHandle(ps, hArr);
			}
			NTLFreeHandle(ps, hArg);
			NTLFreeHandle(ps, hDel);
			break;
		}
		case kEnumerate:
		{
			JILHandle* hDel = NTLGetArgHandle(ps, 0);
			JILHandle* hArg = NTLGetArgHandle(ps, 1);
			result = JILArray_Enumerate(_this, hDel, hArg);
			NTLFreeHandle(ps, hArg);
			NTLFreeHandle(ps, hDel);
			break;
		}
		case kPushItem:
			hStr = NTLGetArgHandle(ps, 0);
			JILArray_MoveTo(_this, _this->size, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kPopItem:
			if( _this->size > 0 )
			{
				hStr = JILArray_GetFrom(_this, _this->size - 1);
				NTLReferHandle(ps, hStr);
				JILArray_SetSize(_this, _this->size - 1);
			}
			else
			{
				hStr = NTLGetNullHandle(ps);
			}
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSort:
		{
			JILHandle* hDel = NTLGetArgHandle(ps, 0);
			result = JILArray_Sort(_this, hDel, &pArr);
			if( result == JIL_No_Exception )
			{
				hArr = NTLNewHandleForObject(ps, type_array, pArr);
				NTLReturnHandle(ps, hArr);
				NTLFreeHandle(ps, hArr);
			}
			NTLFreeHandle(ps, hDel);
			break;
		}
		case kIndexOf:
		{
			JILHandle* hItem = NTLGetArgHandle(ps, 0);
			JILLong index = NTLGetArgInt(ps, 1);
			NTLReturnInt(ps, JILArray_IndexOf(_this, hItem, index));
			NTLFreeHandle(ps, hItem);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// ArrayDelete
//------------------------------------------------------------------------------

static int ArrayDelete(NTLInstance* pInst, JILArray* _this)
{
	JILArray_Delete( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// ArrayTerminate
//------------------------------------------------------------------------------

static int ArrayTerminate(NTLInstance* pInst)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// global constants
//------------------------------------------------------------------------------

static const JILLong kArrayAllocGrain = 32; ///< When the array resizes, it will add this number of new elements at once; increasing this value will increase performance as well as memory spoilage

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

void JILArrayHandleToStringF(JILState*, JILString*, const JILString*, JILHandle*);
static JILArray* JILArrayPreAlloc(JILState*, JILLong);
static void JILArrayReAlloc(JILArray*, JILLong, JILLong);
static void JILArrayDeAlloc(JILArray*);

//------------------------------------------------------------------------------
// JILArray_New
//------------------------------------------------------------------------------
/// Allocate a new, empty array object. The array will contain zero elements.

JILArray* JILArray_New(JILState* pState)
{
	JILArray* _this = (JILArray*) pState->vmMalloc(pState, sizeof(JILArray));
	memset(_this, 0, sizeof(JILArray));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// JILArray_NewNoInit
//------------------------------------------------------------------------------
/// Allocate an array of a specified size and NOT with null handles!
/// ATTENTION: The array will NOT be initialized! The caller MUST initialize all
/// elements with valid pointers to handles, otherwise this will crash badly.
/// This function is intended to provide C developers with the means to
/// efficiently pre-construct an array of a given size and fill it with handles.
/// Doing this is considerably more efficient than letting the array dynamically
/// grow using JILArray_ArrMove() or using JILArray_SetSize().

JILArray* JILArray_NewNoInit(JILState* pState, JILLong size)
{
	return JILArrayPreAlloc(pState, size);
}

//------------------------------------------------------------------------------
// JILArray_Delete
//------------------------------------------------------------------------------
/// Destroy an array object and release all contained elements.

void JILArray_Delete(JILArray* _this)
{
	JILArrayDeAlloc(_this);
	_this->pState->vmFree(_this->pState, _this);
}

//------------------------------------------------------------------------------
// JILArray_SetSize
//------------------------------------------------------------------------------
/// Resize the array. If the new size is smaller than the current size, the
/// exceeding items will be released. All existing items still fitting into the
/// resized array will be kept.

void JILArray_SetSize(JILArray* _this, JILLong newSize)
{
	JILArrayReAlloc(_this, newSize, JILTrue);
}

//------------------------------------------------------------------------------
// JILArray_FillWithType
//------------------------------------------------------------------------------
/// Creates and returns a new array that is filled with instances of a given
/// type. This works only for the data types type_int, type_float and
/// type_string. If the given type ID number does not specify one of these
/// types, the array is filled with 'null' references.

JILArray* JILArray_FillWithType(JILState* ps, JILLong type, JILLong size)
{
	JILLong i;
	JILHandle* pHandle;
	JILArray* _this;
	JILString* pEmptyString;
	if( size < 0 )
		size = 0;
	_this = JILArrayPreAlloc(ps, size);	// create a new, uninitialized array
	switch( type )
	{
		case type_int:
			for( i = 0; i < size; i++ )
			{
				pHandle = JILGetNewHandle(ps);
				pHandle->type = type;
				JILGetIntHandle(pHandle)->l = 0;
				_this->ppHandles[i] = pHandle;
			}
			break;
		case type_float:
			for( i = 0; i < size; i++ )
			{
				pHandle = JILGetNewHandle(ps);
				pHandle->type = type;
				JILGetFloatHandle(pHandle)->f = 0.0;
				_this->ppHandles[i] = pHandle;
			}
			break;
		case type_string:
			for( i = 0; i < size; i++ )
			{
				pEmptyString = JILString_New(ps);
				pHandle = JILGetNewHandle(ps);
				pHandle->type = type;
				JILGetStringHandle(pHandle)->str = pEmptyString;
				_this->ppHandles[i] = pHandle;
			}
			break;
		default:
			for( i = 0; i < size; i++ )
				_this->ppHandles[i] = JILGetNullHandle(ps);
			JILGetNullHandle(ps)->refCount += size;
			break;
	}
	return _this;
}

//------------------------------------------------------------------------------
// JILArray_Copy
//------------------------------------------------------------------------------
/// Create a copy of an array. The new array will be filled with references to
/// the source arrays elements, except for int and float, which will be copied.

JILArray* JILArray_Copy(const JILArray* pSource)
{
	JILArray* result = JILArrayPreAlloc(pSource->pState, pSource->size);
	JILLong i;
	JILLong size = pSource->size;
	JILHandle** ppS = pSource->ppHandles;
	JILHandle** ppD = result->ppHandles;
	JILState* pState = pSource->pState;
	for( i = 0; i < size; i++ )
	{
		if( (*ppS)->type == type_array )
			*ppD++ = NTLCopyHandle(pState, *ppS++);
		else
			*ppD++ = NTLCopyValueType(pState, *ppS++);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_ArrMove
//------------------------------------------------------------------------------
/// Add a new data item to the end of this array, the item is moved by reference.
/// If the data item happens to be an array, that array's elements will be moved,
/// not the array itself.

void JILArray_ArrMove(JILArray* _this, JILHandle* pHandle)
{
	if( pHandle->type == type_array )
	{
		JILLong i;
		JILArray* src = JILGetArrayHandle(pHandle)->arr;
		JILLong size = src->size;
		JILLong offs = _this->size;
		JILState* pState = _this->pState;
		JILArrayReAlloc(_this, _this->size + size, JILTrue);
		for( i = 0; i < size; i++, offs++ )
		{
			JILHandle* elemS = src->ppHandles[i];
			JILHandle* elemD;
			if( elemS->type == type_array )
				elemD = NTLCopyHandle(pState, elemS);
			else
				elemD = NTLCopyValueType(pState, elemS);
			JILRelease(pState, _this->ppHandles[offs]);
			_this->ppHandles[offs] = elemD;
		}
	}
	else
	{
		JILArray_MoveTo(_this, _this->size, pHandle);
	}
}

//------------------------------------------------------------------------------
// JILArray_ArrCopy
//------------------------------------------------------------------------------
/// Add a new data item to the end of this array, the item is copied. If the
/// data item happens to be an array, that array's elements will be copied, not
/// the array itself.

void JILArray_ArrCopy(JILArray* _this, JILHandle* pHandle)
{
	if( pHandle->type == type_array )
	{
		JILLong i;
		JILArray* src = JILGetArrayHandle(pHandle)->arr;
		JILLong size = src->size;
		JILLong offs = _this->size;
		JILState* pState = _this->pState;
		JILArrayReAlloc(_this, _this->size + size, JILTrue);
		for( i = 0; i < size; i++, offs++ )
		{
			JILHandle* elemS = src->ppHandles[i];
			JILHandle* elemD = NTLCopyHandle(pState, elemS);
			JILRelease(pState, _this->ppHandles[offs]);
			_this->ppHandles[offs] = elemD;
		}
	}
	else
	{
		JILArray_CopyTo(_this, _this->size, pHandle);
	}
}

//------------------------------------------------------------------------------
// JILArray_MoveTo
//------------------------------------------------------------------------------
/// Move a reference to an item into a location of this array. Any previous item
/// in the destination location will be released.

void JILArray_MoveTo(JILArray* _this, JILLong index, JILHandle* pHandle)
{
	JILHandle* pNewHandle;
	JILHandle** ppD;
	if( index < 0 )
		return;
	// if the index exceeds current size, resize array
	if( index >= _this->size )
		JILArrayReAlloc(_this, index + 1, JILTrue);
	ppD = _this->ppHandles + index;
	pNewHandle = NTLCopyValueType(_this->pState, pHandle);
	JILRelease(_this->pState, *ppD);
	*ppD = pNewHandle;
}

//------------------------------------------------------------------------------
// JILArray_CopyTo
//------------------------------------------------------------------------------
/// Copy an item into a location of this array. Any previous item in the
/// destination will be released.

void JILArray_CopyTo(JILArray* _this, JILLong index, JILHandle* pHandle)
{
	JILHandle* pNewHandle;
	JILHandle** ppD;
	if( index < 0 )
		return;
	// if the index exceeds current size, resize array
	if( index >= _this->size )
		JILArrayReAlloc(_this, index + 1, JILTrue);
	ppD = _this->ppHandles + index;
	pNewHandle = NTLCopyHandle(_this->pState, pHandle);
	JILRelease(_this->pState, *ppD);
	*ppD = pNewHandle;
}

//------------------------------------------------------------------------------
// JILArray_GetFrom
//------------------------------------------------------------------------------
/// Get a handle from a location of this array; the caller must JILAddRef the
/// returned handle!

JILHandle* JILArray_GetFrom(JILArray* _this, JILLong index)
{
	// if the index exceeds current size, return null handle
	if( index < 0 || index >= _this->size )
		return JILGetNullHandle(_this->pState);
	return _this->ppHandles[index];
}

//------------------------------------------------------------------------------
// JILArray_GetEA
//------------------------------------------------------------------------------
/// Get the effective handle address of a location in this array.

JILHandle** JILArray_GetEA(JILArray* _this, JILLong index)
{
	if( index < 0 )
		return NULL;
	// if the index exceeds current size, resize array
	if( index >= _this->size )
		JILArrayReAlloc(_this, index + 1, JILTrue);
	return _this->ppHandles + index;
}

//------------------------------------------------------------------------------
// JILArray_DeepCopy
//------------------------------------------------------------------------------
/// This creates a deep copy of the source array. The difference to JILArray_Copy
/// is that this function will create copies of all elements in this array,
/// regardless of their type.

JILArray* JILArray_DeepCopy(const JILArray* pSource)
{
	JILArray* result = JILArrayPreAlloc(pSource->pState, pSource->size);
	JILLong i;
	JILLong size = pSource->size;
	JILHandle** ppS = pSource->ppHandles;
	JILHandle** ppD = result->ppHandles;
	JILState* pState = pSource->pState;
	for( i = 0; i < size; i++ )
	{
		*ppD++ = NTLCopyHandle(pState, *ppS++);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_Insert
//------------------------------------------------------------------------------
/// Insert the source array's elements at the given position into this array and
/// return the result as a new array. This array will not be modified.

JILArray* JILArray_Insert(JILArray* _this, JILArray* source, JILLong index)
{
	JILArray* result;
	if( source->size )
	{
		JILLong i;
		if( index < 0 )
			index = 0;
		if( index > _this->size )
			index = _this->size;
		// allocate a new array
		result = JILArrayPreAlloc(_this->pState, _this->size + source->size);
		// copy left part
		memcpy( result->ppHandles, _this->ppHandles, index * sizeof(JILHandle*) );
		// copy middle part
		memcpy( result->ppHandles + index, source->ppHandles, source->size * sizeof(JILHandle*) );
		// copy right part
		memcpy( result->ppHandles + index + source->size, _this->ppHandles + index, (_this->size - index) * sizeof(JILHandle*) );
		// add a reference to all items in the new array
		for( i = 0; i < result->size; i++ )
		{
			JILAddRef( result->ppHandles[i] );
		}
	}
	else
	{
		result = JILArray_Copy(_this);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_InsertItem
//------------------------------------------------------------------------------
/// Insert the source element at the given position into this array and
/// return the result as a new array. This array will not be modified.
/// The source element will be inserted by reference, it will not be copied.

JILArray* JILArray_InsertItem(JILArray* _this, JILHandle* source, JILLong index)
{
	JILArray* result;
	JILLong i;
	if( index < 0 )
		index = 0;
	if( index > _this->size )
		index = _this->size;
	// allocate a new array
	result = JILArrayPreAlloc(_this->pState, _this->size + 1);
	// copy left part
	memcpy( result->ppHandles, _this->ppHandles, index * sizeof(JILHandle*) );
	// copy middle part
	result->ppHandles[index] = source;
	// copy right part
	memcpy( result->ppHandles + index + 1, _this->ppHandles + index, (_this->size - index) * sizeof(JILHandle*) );
	// add a reference to all items in the new array
	for( i = 0; i < result->size; i++ )
	{
		JILAddRef( result->ppHandles[i] );
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_Remove
//------------------------------------------------------------------------------
/// Remove 'length' elements from position 'index' of this array and return
/// the result as a new array. This array will not be modified.

JILArray* JILArray_Remove(JILArray* _this, JILLong index, JILLong length)
{
	JILArray* result;
	if( index < 0 )
		index = 0;
	if( (index < _this->size) && (length > 0) )
	{
		JILLong i;
		if( (index + length) > _this->size )
			length = _this->size - index;
		result = JILArrayPreAlloc(_this->pState, _this->size - length);
		// copy left part
		memcpy(result->ppHandles, _this->ppHandles, index * sizeof(JILHandle*));
		// copy right part
		memcpy(result->ppHandles + index, _this->ppHandles + index + length, (_this->size - (index + length)) * sizeof(JILHandle*));
		// add a reference to all items in the new array
		for( i = 0; i < result->size; i++ )
		{
			JILAddRef( result->ppHandles[i] );
		}
	}
	else
	{
		result = JILArray_Copy(_this);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_SubArray
//------------------------------------------------------------------------------
/// Extracts elements from this array and returns them in a new array. This
/// array will not be modified by this operation.

JILArray* JILArray_SubArray(JILArray* _this, JILLong index, JILLong length)
{
	JILArray* result;
	if( index < 0 )
		index = 0;
	if( (index < _this->size) && (length > 0) )
	{
		JILLong i;
		if( (index + length) > _this->size )
			length = _this->size - index;
		result = JILArrayPreAlloc(_this->pState, length);
		memcpy(result->ppHandles, _this->ppHandles + index, length * sizeof(JILHandle*));
		// add a reference to all items in the new array
		for( i = 0; i < result->size; i++ )
		{
			JILAddRef( result->ppHandles[i] );
		}
	}
	else
	{
		result = JILArray_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArray_Swap
//------------------------------------------------------------------------------
/// Exchange two elements in this array. Element 'index1' will be moved to
/// 'index2' and element 'index2' will be moved to 'index1'. This function does
/// modify this array.

void JILArray_Swap(JILArray* _this, JILLong index1, JILLong index2)
{
	if( index1 < 0 )
		index1 = 0;
	if( index2 < 0 )
		index2 = 0;
	if( index1 < _this->size && index2 < _this->size && index1 != index2 )
	{
		JILHandle* ph = _this->ppHandles[index1];
		_this->ppHandles[index1] = _this->ppHandles[index2];
		_this->ppHandles[index2] = ph;
	}
}

//------------------------------------------------------------------------------
// JILArray_Format
//------------------------------------------------------------------------------
/// Print the contents of an array formatted into a string.

JILString* JILArray_Format(JILArray* _this, JILString* pFormat)
{
	int index;
	int pos;
	int start;
	int len;
	int l;
	JILString* pOutStr;
	JILString* pTmpStr;
	JILString* pFmtStr;

	pOutStr = JILString_New(_this->pState);
	pTmpStr = JILString_New(_this->pState);
	pFmtStr = JILString_New(_this->pState);
	index = 0;
	start = 0;
	len = JILString_Length(pFormat);
	while( start < len )
	{
		// search for a % character in format string
		pos = JILString_FindChar(pFormat, '%', start);
		if( pos < 0 || index >= _this->size )
		{
			// no more formats, literally copy rest of format string
			JILString_SubStr(pTmpStr, pFormat, start, len - start);
			JILString_Append(pOutStr, pTmpStr);
			break;
		}
		// found a % literally copy format string up to position of %
		JILString_SubStr(pTmpStr, pFormat, start, pos - start);
		JILString_Append(pOutStr, pTmpStr);
		// second % following this one?
		if( JILString_CharAt(pFormat, pos + 1) == '%' )
		{
			// add single % and continue
			JILString_Assign(pTmpStr, "%");
			JILString_Append(pOutStr, pTmpStr);
			start = pos + 2;
		}
		else
		{
			// define a character set, containing all possible format 'types'
			JILString_Assign(pFmtStr, "CdiouxXeEfgGnpsS");
			// span from % to format type
			l = JILString_SpanExcl(pFormat, pFmtStr, pos);
			if( l )
			{
				// isolate the format specification
				JILString_SubStr(pFmtStr, pFormat, pos, l + 1);
				// write handle data formatted to string
				JILArrayHandleToStringF(_this->pState, pTmpStr, pFmtStr, _this->ppHandles[index++]);
				JILString_Append(pOutStr, pTmpStr);
			}
			start = pos + l + 1;
		}
	}

	JILString_Delete(pTmpStr);
	JILString_Delete(pFmtStr);
	return pOutStr;
}

//------------------------------------------------------------------------------
// JILArray_ToString
//------------------------------------------------------------------------------
/// Print the array contents unformatted into a string.

JILString* JILArray_ToString(JILArray* _this)
{
	int i;
	JILString* pTempStr;
	JILString* pStr;
	JILState* ps = _this->pState;
	pTempStr = JILString_New(ps);
	pStr = JILString_New(ps);
	for( i = 0; i < _this->size; i++ )
	{
		JILArrayHandleToString(ps, pTempStr, _this->ppHandles[i]);
		JILString_Append(pStr, pTempStr);
	}
	JILString_Delete(pTempStr);
	return pStr;
}

//------------------------------------------------------------------------------
// JILArray_Process
//------------------------------------------------------------------------------
/// Calls a delegate function for every element in this array and creates a new
/// array from the results of the delegate calls. If the result of the delegate
/// is not null, it is placed in the destination array.
/// If the given array is multi-dimensional, this function recursively processes
/// all elements and creates a multi-dimensional destination array.

JILError JILArray_Process(const JILArray* _this, JILHandle* pDelegate, JILHandle* pArgs, JILArray** ppNew)
{
	JILError result = JIL_No_Exception;
	JILLong i;
	JILState* ps = _this->pState;
	JILHandle* pResult;
	JILArray* pNewArr;

	pNewArr = JILArray_New(ps);
	for( i = 0; i < _this->size; i++ )
	{
		if(_this->ppHandles[i]->type == type_array)
		{
			JILArray* pSubArray;
			JILHandleArray* pH = JILGetArrayHandle(_this->ppHandles[i]);
			result = JILArray_Process(pH->arr, pDelegate, pArgs, &pSubArray);
			if( result )
				goto error;
			pResult = NTLNewHandleForObject(ps, type_array, pSubArray);
			JILArray_ArrMove(pNewArr, pResult);
			NTLFreeHandle(ps, pResult);
		}
		else
		{
			pResult = JILCallFunction(ps, pDelegate, 2, kArgHandle, _this->ppHandles[i], kArgHandle, pArgs);
			result = NTLHandleToError(ps, pResult);
			if( result )
			{
				NTLFreeHandle(ps, pResult);
				goto error;
			}
			if( pResult->type != type_null )
				JILArray_ArrMove(pNewArr, pResult);
			NTLFreeHandle(ps, pResult);
		}
	}
	*ppNew = pNewArr;
	return result;

error:
	JILArray_Delete(pNewArr);
	*ppNew = NULL;
	return result;
}

//------------------------------------------------------------------------------
// JILArray_Enumerate
//------------------------------------------------------------------------------
/// Calls a delegate function for every element in this array. The delegate can
/// read or modify each element as it is passed to it. If the given array is
/// multi-dimensional, this function recursively processes all elements.

JILError JILArray_Enumerate(JILArray* _this, JILHandle* pDelegate, JILHandle* pArgs)
{
	JILLong i;
	JILError err = JIL_No_Exception;
	JILState* ps = _this->pState;

	for( i = 0; i < _this->size; i++ )
	{
		if(_this->ppHandles[i]->type == type_array)
		{
			JILHandleArray* pH = JILGetArrayHandle(_this->ppHandles[i]);
			JILArray_Enumerate(pH->arr, pDelegate, pArgs);
		}
		else
		{
			JILHandle* pResult = JILCallFunction(ps, pDelegate, 2, kArgHandle, _this->ppHandles[i], kArgHandle, pArgs);
			err = NTLHandleToError(ps, pResult);
			NTLFreeHandle(ps, pResult);
			if( err  )
				break;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// JILArray_Sort
//------------------------------------------------------------------------------
/// Calls a comparator delegate and sorts this array. This will fail if the
/// array is multi-dimensional.

JILError JILArray_Sort(JILArray* _this, JILHandle* pDelegate, JILArray** ppNew)
{
	JILLong i, j, res;
	JILError err;
	JILHandle* pResult;
	JILArray* pNew;
	JILState* ps = _this->pState;

	*ppNew = pNew = JILArray_Copy(_this);
	for( i = 1; i < pNew->size; i++ )
    {
        for( j = i; j >= 1; j-- )
        {
			// call our delegate to compare both elements
			pResult = JILCallFunction(ps, pDelegate, 2, kArgHandle, pNew->ppHandles[j - 1], kArgHandle, pNew->ppHandles[j]);
			res = NTLHandleToInt(ps, pResult);
			err = NTLHandleToError(ps, pResult);
			NTLFreeHandle(ps, pResult);
			if( err )
				goto exit;
            if( res > 0 )
            {
				JILArray_Swap(pNew, j - 1, j);
            }
            else
            {
                break;
            }
        }
    }
	return JIL_No_Exception;

exit:
	JILArray_Delete(pNew);
	*ppNew = NULL;
	return err;
}

//------------------------------------------------------------------------------
// JILArray_IndexOf
//------------------------------------------------------------------------------
/// Finds the index of an item in this array.

JILLong	JILArray_IndexOf(JILArray* _this, JILHandle* hItem, JILLong index)
{
	JILLong result = -1;
	JILLong i;
	if( index < 0 )
		return -1;
	for( i = index; i < _this->size; i++ )
	{
		JILHandle* pH = _this->ppHandles[i];
		if (pH->type == hItem->type)
		{
			JILBool equal = JILFalse;
			switch( pH->type )
			{
				case type_int:
					equal = (JILGetIntHandle(pH)->l == JILGetIntHandle(hItem)->l);
					break;
				case type_float:
					equal = (JILGetFloatHandle(pH)->f == JILGetFloatHandle(hItem)->f);
					break;
				case type_string:
					equal = (JILString_Compare(JILGetStringHandle(pH)->str, JILGetStringHandle(hItem)->str) == 0);
					break;
				default:
					equal = (pH == hItem);
					break;
			}
			if( equal )
			{
				result = i;
				break;
			}
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILArrayHandleToStringF
//------------------------------------------------------------------------------
/// Writes the value referred to by a given handle formatted into a string.

void JILArrayHandleToStringF(JILState* ps, JILString* pOutStr, const JILString* pFormat, JILHandle* handle)
{
	JILLong typeID;
	JILUnknown* vec;
	char* pBuffer;
	typeID = NTLHandleToTypeID(ps, handle);
	vec = NTLHandleToObject(ps, typeID, handle);
	pBuffer = (char*) malloc(kStaticBufferSize);
	switch( typeID )
	{
		case type_int:
		{
			JILLong* pLong = (JILLong*) vec;
			JILSnprintf(pBuffer, kStaticBufferSize, JILString_String(pFormat), *pLong);
			JILString_Assign(pOutStr, pBuffer);
			break;
		}
		case type_float:
		{
			JILFloat* pFloat = (JILFloat*) vec;
			JILSnprintf(pBuffer, kStaticBufferSize, JILString_String(pFormat), *pFloat);
			JILString_Assign(pOutStr, pBuffer);
			break;
		}
		case type_string:
		{
			JILString* pStr = (JILString*) vec;
			JILSnprintf(pBuffer, kStaticBufferSize, JILString_String(pFormat), JILString_String(pStr));
			JILString_Assign(pOutStr, pBuffer);
			break;
		}
		default:
		{
			JILString_Assign(pOutStr, NTLGetTypeName(ps, typeID));
			break;
		}
	}
	free( pBuffer );
}

//------------------------------------------------------------------------------
// JILArrayHandleToString
//------------------------------------------------------------------------------
/// Writes the value referred to by a given handle unformatted into a string.

void JILArrayHandleToString(JILState* ps, JILString* pOutStr, JILHandle* handle)
{
	JILLong typeID;
	JILUnknown* vec;
	JILChar pBuffer[64];
	typeID = NTLHandleToTypeID(ps, handle);
	vec = NTLHandleToObject(ps, typeID, handle);
	switch( typeID )
	{
		case type_int:
		{
			JILLong* pLong = (JILLong*) vec;
			JILSnprintf(pBuffer, 64, "%d", *pLong);
			JILString_Assign(pOutStr, pBuffer);
			break;
		}
		case type_float:
		{
			JILFloat* pFloat = (JILFloat*) vec;
			JILSnprintf(pBuffer, 64, "%g", *pFloat);
			JILString_Assign(pOutStr, pBuffer);
			break;
		}
		case type_string:
		{
			JILString* pStr = (JILString*) vec;
			JILString_Assign(pOutStr, JILString_String(pStr));
			break;
		}
		case type_array:
		{
			int i;
			JILString* pStr;
			JILArray* pArray = (JILArray*) vec;
			pStr = JILString_New(ps);
			JILString_Clear( pOutStr );
			for( i = 0; i < pArray->size; i++ )
			{
				JILArrayHandleToString(ps, pStr, pArray->ppHandles[i]);
				JILString_Append(pOutStr, pStr);
			}
			JILString_Delete(pStr);
			break;
		}
		default:
		{
			JILString_Assign(pOutStr, NTLGetTypeName(ps, typeID));
			break;
		}
	}
}

//------------------------------------------------------------------------------
// JILArrayPreAlloc
//------------------------------------------------------------------------------
/// Allocate a new array object and pre-allocate a buffer big enough to
/// accommodate the given number of elements. Note that we do not take into
/// account the array grain size here, for simplicity and performance reasons.

static JILArray* JILArrayPreAlloc(JILState* pState, JILLong length)
{
	JILArray* _this = (JILArray*) pState->vmMalloc(pState, sizeof(JILArray));
	_this->pState = pState;
	_this->maxSize = length;
	_this->size = length;
	if( length > 0 )
		_this->ppHandles = _this->pState->vmMalloc( _this->pState, length * sizeof(JILHandle*) );
	else
		_this->ppHandles = NULL;
	return _this;
}

//------------------------------------------------------------------------------
// JILArrayReAlloc
//------------------------------------------------------------------------------
/// Throw away old array and allocate a new one.<br>
/// If keepData is TRUE:<br>
///   The handles currently in the array will be taken into the new array, up to
///   the specified size. If the new size is smaller than the old size, the
///   exceeding handles will be released. If the new size is larger, the added
///   handle pointers will be initialized with null handles.<br>
/// If keepData is FALSE:<br>
///   All handles currently in the array will be released. A new buffer will be
///   allocated, but NOT initialized. The caller MUST initialize the buffer with
///   new handle pointers immediately!<br>

static void JILArrayReAlloc(JILArray* _this, JILLong newSize, JILLong keepData)
{
	JILState* pState;
	JILLong newMaxSize;
	// new size == 0?
	if( newSize == 0 )
	{
		JILArrayDeAlloc(_this);
		return;
	}
	pState = _this->pState;
	newMaxSize = ((newSize / kArrayAllocGrain) + 1) * kArrayAllocGrain;
	// first we check if reallocation is necessary
	if( newMaxSize != _this->maxSize || !keepData )
	{
		JILLong i;
		JILHandle** ppNewBuffer;
		JILHandle** ppS;
		JILHandle* pNull = JILGetNullHandle(pState);
		ppNewBuffer = (JILHandle**) pState->vmMalloc( pState, newMaxSize * sizeof(JILHandle*) );
		if( ppNewBuffer )
		{
			if( keepData )
			{
				JILLong numKeep = 0;
				if( _this->ppHandles )
				{
					numKeep = (newSize < _this->size) ? newSize : _this->size;
					memcpy( ppNewBuffer, _this->ppHandles, numKeep * sizeof(JILHandle*) );
					// release handles not kept
					ppS = _this->ppHandles + numKeep;
					for( i = numKeep; i < _this->maxSize; i++ )
						JILRelease(pState, *ppS++);
				}
				// fill rest of array with null handles
				ppS = ppNewBuffer + numKeep;
				for( i = numKeep; i < newMaxSize; i++ )
					*ppS++ = pNull;
				pNull->refCount += (newMaxSize - numKeep);
			}
			else
			{
				// release old handles
				if( _this->ppHandles )
				{
					ppS = _this->ppHandles;
					for( i = 0; i < _this->maxSize; i++ )
						JILRelease(pState, *ppS++);
				}
				// fill rest of array with null handles
				ppS = ppNewBuffer + newSize;
				for( i = newSize; i < newMaxSize; i++ )
					*ppS++ = pNull;
				pNull->refCount += (newMaxSize - newSize);
			}
			// destroy old buffer
			if( _this->ppHandles )
				pState->vmFree( pState, _this->ppHandles );
			_this->ppHandles = ppNewBuffer;
			_this->maxSize = newMaxSize;
		}
	}
	_this->size = newSize;
}

//------------------------------------------------------------------------------
// JILArrayDeAlloc
//------------------------------------------------------------------------------
/// Deallocate all data contained in this array, but do not deallocate the array
/// object itself. The result will be an array with zero elements.

static void JILArrayDeAlloc(JILArray* _this)
{
	if( _this->ppHandles )
	{
		// release handles
		JILLong i;
		JILHandle** ppS = _this->ppHandles;
		JILState* pState = _this->pState;
		for( i = 0; i < _this->maxSize; i++ )
			JILRelease(pState, *ppS++);
		pState->vmFree( pState, _this->ppHandles );
		_this->ppHandles = NULL;
	}
	_this->size = 0;
	_this->maxSize = 0;
}
