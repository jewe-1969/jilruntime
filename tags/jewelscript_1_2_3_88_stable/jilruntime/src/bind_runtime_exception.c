//------------------------------------------------------------------------------
// File: bind_runtime_exception.cpp
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
#include "jiltypes.h"
#include "jilstring.h"

JILEXTERN JILError JILRuntimeExceptionProc(NTLInstance*, JILLong, JILLong, JILUnknown*, JILUnknown**);

//-----------------------------------------------------------------------------------
// function enumeration - this must be kept in sync with the class declaration below.
//-----------------------------------------------------------------------------------

enum {
	fn_getError,
	fn_getMessage
};

//--------------------------------------------------------------------------------------------
// class declaration string - order of declarations must be kept in sync with the enumeration.
//--------------------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("When the runtime detects an error during a function call, it will generate and return an instance of this class. This object is not used when user code throws an exception, but in all cases where the error is generated internally.")
	"method int getError ();"
	"method string getMessage ();"
;

//------------------------------------------------------------------------------
// class info constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"runtime::exception"; // The class name that will be used in JewelScript.
static const char*	kBaseName		=	"exception"; // The base interface this class inherits from.
static const char*	kPackageList	=	"";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Built-in exception class for JewelScript.";
static const char*	kTimeStamp		=	"04/16/14 14:23:26";

//------------------------------------------------------------------------------
// forward declare internal functions
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_Register    (JILState* pVM);
static JILError bind_runtime_exception_GetDecl     (JILUnknown* pDataIn);
static JILError bind_runtime_exception_New         (NTLInstance* pInst, JILRuntimeException** ppObject);
static JILError bind_runtime_exception_Delete      (NTLInstance* pInst, JILRuntimeException* _this);
static JILError bind_runtime_exception_Mark        (NTLInstance* pInst, JILRuntimeException* _this);
static JILError bind_runtime_exception_CallStatic  (NTLInstance* pInst, JILLong funcID);
static JILError bind_runtime_exception_CallMember  (NTLInstance* pInst, JILLong funcID, JILRuntimeException* _this);

//------------------------------------------------------------------------------
// native type proc
//------------------------------------------------------------------------------
// This is the function you need to register with the script runtime.

JILEXTERN JILError JILRuntimeExceptionProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	int result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				return bind_runtime_exception_Register((JILState*) pDataIn);
		case NTL_Initialize:			break;
		case NTL_NewObject:				return bind_runtime_exception_New(pInst, (JILRuntimeException**) ppDataOut);
		case NTL_DestroyObject:			return bind_runtime_exception_Delete(pInst, (JILRuntimeException*) pDataIn);
		case NTL_MarkHandles:			return bind_runtime_exception_Mark(pInst, (JILRuntimeException*) pDataIn);
		case NTL_CallStatic:			return bind_runtime_exception_CallStatic(pInst, param);
		case NTL_CallMember:			return bind_runtime_exception_CallMember(pInst, param, (JILRuntimeException*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;
		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetBaseName:			(*(const char**) ppDataOut) = kBaseName; break;
		case NTL_GetPackageString:		(*(const char**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			return bind_runtime_exception_GetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;
		// return error on unknown messages
		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_Register
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_Register(JILState* pVM)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_GetDecl
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_GetDecl(JILUnknown* pDataIn)
{
	// Dynamically build the class declaration
	NTLDeclareVerbatim(pDataIn, kClassDeclaration); // add the static part of the class declaration
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_New
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_New(NTLInstance* pInst, JILRuntimeException** ppObject)
{
	// Allocate memory and write the pointer to ppObject
	JILRuntimeException* _this = (JILRuntimeException*)malloc(sizeof(JILRuntimeException));
	_this->pMessage = JILString_New(NTLInstanceGetVM(pInst));
	*ppObject = _this;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_Delete
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_Delete(NTLInstance* pInst, JILRuntimeException* _this)
{
	// Destroy native instance
	JILString_Delete(_this->pMessage);
	free(_this);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_Mark
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_Mark(NTLInstance* pInst, JILRuntimeException* _this)
{
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_CallStatic
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_CallStatic(NTLInstance* pInst, JILLong funcID)
{
	return JIL_ERR_Invalid_Function_Index;
}

//------------------------------------------------------------------------------
// bind_runtime_exception_CallMember
//------------------------------------------------------------------------------

static JILError bind_runtime_exception_CallMember(NTLInstance* pInst, JILLong funcID, JILRuntimeException* _this)
{
	JILError error = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM
	JILLong thisID = NTLInstanceTypeID(pInst);	// get the type-id of this class
	switch( funcID )
	{
		case fn_getError: // method int getError ()
		{
			JILLong result = _this->error;
			NTLReturnInt(ps, result);
			break;
		}
		case fn_getMessage: // method string getMessage ()
		{
			const JILChar* result = JILString_String(_this->pMessage);
			NTLReturnString(ps, result);
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
