//------------------------------------------------------------------------------
// File: bind_arraylist.cpp
//------------------------------------------------------------------------------
// This is an automatically created binding code file for JewelScript.
// It allows you to easily bind your external C++ code to the script runtime,
// and to use your external functions and classes from within JewelScript.
//
// For more information see: http://blog.jewe.org/?p=29
//------------------------------------------------------------------------------

#include "jilstdinc.h"
#include "jilapi.h"
#include "jiltools.h"

#include "jilarraylist.h"
#include "jilarray.h"

//-----------------------------------------------------------------------------------
// function enumeration - this must be kept in sync with the class declaration below.
//-----------------------------------------------------------------------------------

enum {
	fn_arraylist,
	fn_arraylist2,
	fn_arraylist3,
	fn_deepCopy,
	fn_toArray,
	fn_get,
	fn_set,
	fn_add,
	fn_add2,
	fn_insert,
	fn_remove,
	fn_enumerate1,
	fn_enumerate2,
	fn_count
};

//--------------------------------------------------------------------------------------------
// class declaration string - order of declarations must be kept in sync with the enumeration.
//--------------------------------------------------------------------------------------------

static const JILChar* kClassDeclaration =
	TAG("A combination of linked list and array. This class is a compromise between the flexibility of a list and the efficiency of an array. It is very fast when accessing items by their index. Adding and removing items is not as quick, but it is still a good alternative to the built-in list class.")
	"delegate enumerator(var element, var args);" TAG("Delegate type for the arraylist::enumerate() method.")
	"method arraylist ();" TAG("Constructs an empty arraylist instance.")
	"method arraylist (const arraylist src);" TAG("Constructs a shallow copy of the given arraylist instance.")
	"method arraylist (const var[] src);" TAG("Constructs a new instance from the given array.")
	"method arraylist deepCopy ();" TAG("Returns a deep copy constructed from this instance.")
	"method var[] toArray ();" TAG("Moves all values from this arraylist into a new array.")
	"method var get (const int index);" TAG("Returns the item at the specified index position. If the index is out of range, null is returned.")
	"method set (const int index, var item);" TAG("Sets the item at the specified index to a new value. If the index is out of range, the call is ignored.")
	"method add (var item);" TAG("Adds the specified item to the end of the arraylist.")
	"method append (var[] items);" TAG("Moves all items from the specified array to the end of this arraylist.")
	"method insert (const int index, var item);" TAG("Inserts the specified item into the arraylist at the specified index.")
	"method remove (const int index);" TAG("Removes an item from the arraylist at the specified index.")
	"method enumerate (enumerator fn);" TAG("Calls the specified delegate for every item in this arraylist.")
	"method enumerate (enumerator fn, var args);" TAG("Calls the specified delegate for every item in this arraylist.")
	"accessor int length ();" TAG("Returns the number of items currently stored in the arraylist.")
;

//------------------------------------------------------------------------------
// class info constants
//------------------------------------------------------------------------------

static const JILChar*	kClassName		=	"arraylist";
static const JILChar*	kPackageList	=	"";
static const JILChar*	kAuthorName		=	"jewe.org";
static const JILChar*	kAuthorString	=	"A combination of linked list and array. This class is a compromise between the flexibility of a list and the efficiency of an array.";
static const JILChar*	kTimeStamp		=	"2015-01-03 14:11:03";
static const JILChar*	kAuthorVersion	=	"1.0.0.0";

//------------------------------------------------------------------------------
// forward declare internal functions
//------------------------------------------------------------------------------

static JILError bind_arraylist_Register    (JILState* pVM);
static JILError bind_arraylist_GetDecl     (JILUnknown* pDataIn);
static JILError bind_arraylist_New         (NTLInstance* pInst, JILArrayList** ppObject);
static JILError bind_arraylist_Delete      (NTLInstance* pInst, JILArrayList* _this);
static JILError bind_arraylist_Mark        (NTLInstance* pInst, JILArrayList* _this);
static JILError bind_arraylist_CallStatic  (NTLInstance* pInst, JILLong funcID);
static JILError bind_arraylist_CallMember  (NTLInstance* pInst, JILLong funcID, JILArrayList* _this);

//------------------------------------------------------------------------------
// native type proc
//------------------------------------------------------------------------------
// This is the function you need to register with the script runtime.

JILEXTERN JILError JILArrayListProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	int result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				return bind_arraylist_Register((JILState*) pDataIn);
		case NTL_Initialize:			break;
		case NTL_NewObject:				return bind_arraylist_New(pInst, (JILArrayList**) ppDataOut);
		case NTL_DestroyObject:			return bind_arraylist_Delete(pInst, (JILArrayList*) pDataIn);
		case NTL_MarkHandles:			return bind_arraylist_Mark(pInst, (JILArrayList*) pDataIn);
		case NTL_CallStatic:			return bind_arraylist_CallStatic(pInst, param);
		case NTL_CallMember:			return bind_arraylist_CallMember(pInst, param, (JILArrayList*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;
		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(kAuthorVersion);
		case NTL_GetClassName:			(*(const JILChar**) ppDataOut) = kClassName; break;
		case NTL_GetPackageString:		(*(const JILChar**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			return bind_arraylist_GetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const JILChar**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const JILChar**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const JILChar**) ppDataOut) = kAuthorString; break;
		// return error on unknown messages
		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// bind_arraylist_Register
//------------------------------------------------------------------------------

static JILError bind_arraylist_Register(JILState* pVM)
{
	// If your type library consists of multiple related classes, you could register any helper classes here.
	// That way your application only needs to register the main class to the script runtime.
	// JILError err = JILRegisterNativeType(pVM, bind_MyHelperClass_proc);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_arraylist_GetDecl
//------------------------------------------------------------------------------

static JILError bind_arraylist_GetDecl(JILUnknown* pDataIn)
{
	// Dynamically build the class declaration
	NTLDeclareVerbatim(pDataIn, kClassDeclaration); // add the static part of the class declaration
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_arraylist_New
//------------------------------------------------------------------------------

static JILError bind_arraylist_New(NTLInstance* pInst, JILArrayList** ppObject)
{
	*ppObject = (JILArrayList*) JILArrayList_New(NTLInstanceGetVM(pInst), JILArrayListRelease);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_arraylist_Delete
//------------------------------------------------------------------------------

static JILError bind_arraylist_Delete(NTLInstance* pInst, JILArrayList* _this)
{
	// Destroy native instance
	JILArrayList_Delete(_this);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_arraylist_Mark
//------------------------------------------------------------------------------

static JILError bind_arraylist_Mark(NTLInstance* pInst, JILArrayList* _this)
{
	return JILArrayList_Mark(_this);
}

//------------------------------------------------------------------------------
// bind_arraylist_CallStatic
//------------------------------------------------------------------------------

static JILError bind_arraylist_CallStatic(NTLInstance* pInst, JILLong funcID)
{
	return JIL_ERR_Invalid_Function_Index;
}

//------------------------------------------------------------------------------
// bind_arraylist_CallMember
//------------------------------------------------------------------------------

static JILError bind_arraylist_CallMember(NTLInstance* pInst, JILLong funcID, JILArrayList* _this)
{
	JILError error = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM
	JILLong thisID = NTLInstanceTypeID(pInst);	// get the type-id of this class
	switch( funcID )
	{
		case fn_arraylist: // method arraylist ()
		{
			break;
		}
		case fn_arraylist2: // method arraylist (const arraylist src)
		{
			JILHandle* h_arg_0 = NTLGetArgHandle(ps, 0);
			JILArrayList* arg_0 = (JILArrayList*)NTLHandleToObject(ps, thisID, h_arg_0);
			JILArrayList_Copy(_this, arg_0);
			NTLFreeHandle(ps, h_arg_0);
			break;
		}
		case fn_arraylist3: // method arraylist (const var[] src)
		{
			JILHandle* h_arg_0 = NTLGetArgHandle(ps, 0);
			JILArray* arg_0 = (JILArray*)NTLHandleToObject(ps, type_array, h_arg_0);
			JILArrayList_FromArray(_this, arg_0);
			NTLFreeHandle(ps, h_arg_0);
			break;
		}
		case fn_deepCopy:
		{
			JILHandle* hResult;
			JILArrayList* result = JILArrayList_DeepCopy(_this);
			hResult = NTLNewHandleForObject(ps, thisID, result);
			NTLReturnHandle(ps, hResult);
			NTLFreeHandle(ps, hResult);
			break;
		}
		case fn_toArray: // method var[] toArray ()
		{
			JILHandle* hResult;
			JILArray* result = JILArray_New(ps);
			JILArrayList_ToArray(_this, result);
			hResult = NTLNewHandleForObject(ps, type_array, result);
			NTLReturnHandle(ps, hResult);
			NTLFreeHandle(ps, hResult);
			break;
		}
		case fn_get: // method var get (const int index)
		{
			JILHandle* result = JILArrayList_GetItem(_this, NTLGetArgInt(ps, 0));
			NTLReturnHandle(ps, result);
			break;
		}
		case fn_set: // method set (const int index, var item)
		{
			JILHandle* h_arg_1 = NTLGetArgHandle(ps, 1);
			JILLong arg_0 = NTLGetArgInt(ps, 0);
			JILArrayList_SetItem(_this, arg_0, h_arg_1);
			break;
		}
		case fn_add: // method add (var item)
		{
			JILHandle* h_arg_0 = NTLGetArgHandle(ps, 0);
			JILArrayList_AddItem(_this, h_arg_0);
			break;
		}
		case fn_add2: // method add (var item)
		{
			JILLong i;
			JILHandle* h_arg_0 = NTLGetArgHandle(ps, 0);
			JILArray* pArr = NTLHandleToObject(ps, type_array, h_arg_0);
			for( i = 0; i < pArr->size; i++ )
			{
				NTLReferHandle(ps, pArr->ppHandles[i]);
				JILArrayList_AddItem(_this, pArr->ppHandles[i]);
			}
			NTLFreeHandle(ps, h_arg_0);
			break;
		}
		case fn_insert: // method insert (const int index, var item)
		{
			JILHandle* h_arg_1 = NTLGetArgHandle(ps, 1);
			JILLong arg_0 = NTLGetArgInt(ps, 0);
			JILArrayList_InsertItem(_this, arg_0, h_arg_1);
			break;
		}
		case fn_remove: // method remove (const int index)
		{
			JILArrayList_RemoveItem(_this, NTLGetArgInt(ps, 0));
			break;
		}
		case fn_enumerate1: // method enumerate (enumerator fn)
		{
			JILHandle* h1, *h2;
			h1 = NTLGetArgHandle(ps, 0);
			h2 = NTLGetNullHandle(ps);
			error = JILArrayList_Enumerate(_this, h1, h2);
			NTLFreeHandle(ps, h1);
			NTLFreeHandle(ps, h2);
			break;
		}
		case fn_enumerate2: // method enumerate (enumerator fn, var args)
		{
			JILHandle* h1, *h2;
			h1 = NTLGetArgHandle(ps, 0);
			h2 = NTLGetArgHandle(ps, 1);
			error = JILArrayList_Enumerate(_this, h1, h2);
			NTLFreeHandle(ps, h1);
			NTLFreeHandle(ps, h2);
			break;
		}
		case fn_count: // accessor int count ()
		{
			NTLReturnInt(ps, JILArrayList_Count(_this));
			break;
		}
		default:
		{
			error = JIL_ERR_Invalid_Function_Index;
			break;
		}
	}
	return error;
}
