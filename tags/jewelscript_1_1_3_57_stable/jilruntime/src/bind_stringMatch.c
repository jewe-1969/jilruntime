//------------------------------------------------------------------------------
// File: bind_stringMatch.c
//------------------------------------------------------------------------------
// This is an automatically created binding code file for JewelScript.
// It allows you to easily bind your external C++ code to the script runtime,
// and to use your external functions and classes from within JewelScript.
//
// For more information see: http://blog.jewe.org/?p=29
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilapi.h"
#include "jilstring.h"
#include "jiltools.h"

//-----------------------------------------------------------------------------------
// function enumeration - this must be kept in sync with the class declaration below.
//-----------------------------------------------------------------------------------

enum {
	fn_matchStart,
	fn_matchLength,
	fn_arrayIndex
};

//--------------------------------------------------------------------------------------------
// class declaration string - order of declarations must be kept in sync with the enumeration.
//--------------------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("Describes the result of string matching operation as returned by the string::matchString() and string::matchArray() methods.")
	"accessor int matchStart ();" TAG("Returns the character position where this match starts. For matchString() the position refers to 'this' string. For matchArray() the position refers to the element specified by 'arrayIndex'.")
	"accessor int matchLength ();" TAG("Returns the length of the match in characters.")
	"accessor int arrayIndex ();" TAG("Returns the array index of the matching element. For matchString() it specifies the array element that was found in 'this' string as a substring. For matchArray() it specifies the array element that contains 'this' string as a substring.")
;

//------------------------------------------------------------------------------
// class info constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"stringMatch"; // The class name that will be used in JewelScript.
static const char*	kPackageList	=	"";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Describes the result of string matching operation as returned by the string::matchString() and string::matchArray() methods."; // TODO: You can enter a description of your native type here
static const char*	kTimeStamp		=	"05/19/13 17:07:38";

//------------------------------------------------------------------------------
// forward declare internal functions
//------------------------------------------------------------------------------

static JILError bind_stringMatch_Register    (JILState* pVM);
static JILError bind_stringMatch_GetDecl     (JILUnknown* pDataIn);
static JILError bind_stringMatch_New         (NTLInstance* pInst, NStringMatch** ppObject);
static JILError bind_stringMatch_Delete      (NTLInstance* pInst, NStringMatch* _this);
static JILError bind_stringMatch_Mark        (NTLInstance* pInst, NStringMatch* _this);
static JILError bind_stringMatch_CallStatic  (NTLInstance* pInst, JILLong funcID);
static JILError bind_stringMatch_CallMember  (NTLInstance* pInst, JILLong funcID, NStringMatch* _this);

//------------------------------------------------------------------------------
// native type proc
//------------------------------------------------------------------------------
// This is the function you need to register with the script runtime.

JILError JILStringMatchProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	int result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				return bind_stringMatch_Register((JILState*) pDataIn);
		case NTL_Initialize:			break;
		case NTL_NewObject:				return bind_stringMatch_New(pInst, (NStringMatch**) ppDataOut);
		case NTL_DestroyObject:			return bind_stringMatch_Delete(pInst, (NStringMatch*) pDataIn);
		case NTL_MarkHandles:			return bind_stringMatch_Mark(pInst, (NStringMatch*) pDataIn);
		case NTL_CallStatic:			return bind_stringMatch_CallStatic(pInst, param);
		case NTL_CallMember:			return bind_stringMatch_CallMember(pInst, param, (NStringMatch*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;
		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetPackageString:		(*(const char**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			return bind_stringMatch_GetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;
		// return error on unknown messages
		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// bind_stringMatch_Register
//------------------------------------------------------------------------------

static JILError bind_stringMatch_Register(JILState* pVM)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_stringMatch_GetDecl
//------------------------------------------------------------------------------

static JILError bind_stringMatch_GetDecl(JILUnknown* pDataIn)
{
	NTLDeclareVerbatim(pDataIn, kClassDeclaration);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_stringMatch_New
//------------------------------------------------------------------------------

static JILError bind_stringMatch_New(NTLInstance* pInst, NStringMatch** ppObject)
{
	JILState* pVM = NTLInstanceGetVM(pInst);
	*ppObject = (NStringMatch*) pVM->vmMalloc(pVM, sizeof(NStringMatch));
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_stringMatch_Delete
//------------------------------------------------------------------------------

static JILError bind_stringMatch_Delete(NTLInstance* pInst, NStringMatch* _this)
{
	JILState* pVM = NTLInstanceGetVM(pInst);
	pVM->vmFree(pVM, _this);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_stringMatch_Mark
//------------------------------------------------------------------------------

static JILError bind_stringMatch_Mark(NTLInstance* pInst, NStringMatch* _this)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_stringMatch_CallStatic
//------------------------------------------------------------------------------

static JILError bind_stringMatch_CallStatic(NTLInstance* pInst, JILLong funcID)
{
	return JIL_ERR_Invalid_Function_Index;
}

//------------------------------------------------------------------------------
// bind_stringMatch_CallMember
//------------------------------------------------------------------------------

static JILError bind_stringMatch_CallMember(NTLInstance* pInst, JILLong funcID, NStringMatch* _this)
{
	JILError error = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM
	JILLong thisID = NTLInstanceTypeID(pInst);	// get the type-id of this class
	switch( funcID )
	{
		case fn_matchStart: // accessor int matchStart ()
		{
			NTLReturnInt(ps, _this->matchStart);
			break;
		}
		case fn_matchLength: // accessor int matchLength ()
		{
			NTLReturnInt(ps, _this->matchLength);
			break;
		}
		case fn_arrayIndex: // accessor int arrayIndex ()
		{
			NTLReturnInt(ps, _this->arrayIndex);
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

//------------------------------------------------------------------------------
// JILStringMatch_Create
//------------------------------------------------------------------------------
// Creates a new stringMatch instance with parameters and returns it as a JILHandle.

JILHandle* JILStringMatch_Create(JILState* pVM, JILLong start, JILLong length, JILLong index)
{
	NStringMatch* pMatch = (NStringMatch*) pVM->vmMalloc(pVM, sizeof(NStringMatch));
	pMatch->arrayIndex = index;
	pMatch->matchLength = length;
	pMatch->matchStart = start;
	return NTLNewHandleForObject(pVM, NTLTypeNameToTypeID(pVM, kClassName), pMatch);
}
