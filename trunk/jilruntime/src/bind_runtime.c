//------------------------------------------------------------------------------
// File: bind_runtime.c
//------------------------------------------------------------------------------
// This is an automatically created binding code file for JewelScript.
// It allows you to easily bind your external C++ code to the script runtime,
// and to use your external functions and classes from within JewelScript.
//
// For more information see: http://blog.jewe.org/?p=29
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilapi.h"
#include "jilcompilerapi.h"
#include "jiltools.h"
#include "jilstring.h"
#include "jilcallntl.h"
#include "jilarray.h"
#include "jilcodelist.h"

//-----------------------------------------------------------------------------------
// function enumeration - this must be kept in sync with the class declaration below.
//-----------------------------------------------------------------------------------

enum {
	fn_traceException,
	fn_runtimeChecks,
	fn_debugBuild,
	fn_releaseBuild,
	fn_libraryVersion,
	fn_runtimeVersion,
	fn_compilerVersion,
	fn_typeInterfaceVersion,
	fn_stackSize,
	fn_instructionCounter,
	fn_getTypeName,
	fn_getTypeID,
	fn_getTypeFamily,
	fn_getBaseType,
	fn_isTypeNative,
	fn_getTypeVersion,
	fn_getTypeAuthor,
	fn_getTypeDescription,
	fn_getTypeTimeStamp,
	fn_getNumTypes,
	fn_generateBindings,
	fn_generateDocs,
	fn_printLog,
	fn_clone,
	fn_printDebugInfo,
	fn_disposeObject
};

//--------------------------------------------------------------------------------------------
// class declaration string - order of declarations must be kept in sync with the enumeration.
//--------------------------------------------------------------------------------------------

static const JILChar* kClassDeclaration =
	TAG("Static class that provides access to functions of the JewelScript runtime.")
	"function int traceException ();" TAG("Returns true if the virtual machine supports the trace exception.")
	"function int runtimeChecks ();" TAG("Returns true if the virtual machine performs extended runtime checks.")
	"function int debugBuild ();" TAG("Returns true if this is a debug build of the runtime.")
	"function int releaseBuild ();" TAG("Return true if this is a release build of the runtime.")
	"function string libraryVersion ();" TAG("Returns the version number of the JewelScript library.")
	"function string runtimeVersion ();" TAG("Returns the version number of the runtime.")
	"function string compilerVersion ();" TAG("Returns the version number of the compiler.")
	"function string typeInterfaceVersion ();" TAG("Returns the version number of the native type interface.")
	"function int stackSize ();" TAG("Returns the stack size specified when initializing this runtime.")
	"function float instructionCounter ();" TAG("Returns the current value of the instruction counter. The instruction counter is an unsigned 64-bit integer that is increased for each executed VM instruction. If this feature has been disabled, the result is always 0.")
	"function string getTypeName (int type);" TAG("Returns the type name for the specified type identifier number.")
	"function int getTypeID (const string name);" TAG("Returns the type ID for the specified type name. If the name is not a type name, returns 0.")
	"function int getTypeFamily (int type);" TAG("Returns the type family ID for the specified type ID.")
	"function int getBaseType (int type);" TAG("Returns the base class type ID for the specified type ID.")
	"function int isTypeNative (int type);" TAG("Returns true if the specified type is a native class.")
	"function string getTypeVersion (int type);" TAG("Returns the version string of the specified native type.")
	"function string getTypeAuthor (int type);" TAG("Returns the author string of the specified native type.")
	"function string getTypeDescription (int type);" TAG("Returns the description string of the specified native type.")
	"function string getTypeTimeStamp (int type);" TAG("Returns the build time stamp of the specified native type.")
	"function int getNumTypes ();" TAG("Returns the total number of types known to the runtime.")
	"function int generateBindings (const string path);" TAG("Generates native binding code at the specified path. To save memory, the application can free the compiler before executing the script, in which case this function will do nothing.")
	"function int generateDocs (const string path, const string args);" TAG("Generates HTML documentation at the specified path. To save memory, the application can free the compiler before executing the script, in which case this function will do nothing.")
	"function printLog (const string[] args);" TAG("Uses the runtime's logging callback to output the given string arguments. A line-feed is added after printing all arguments.")
	"function var clone (const var o);" TAG("Creates a copy of the given object by calling it's copy constructor. Script objects that have no copy constructor will be copied by the runtime. If a native object has no copy constructor, this function returns null.<p>Special care should be taken if the specified object has references to delegates. While this function will also copy the source object's delegates, these may reference the source object. If that is unwanted, it is recommended to add a copy constructor to the class and initialize these delegates manually. This is especially true for hybrid classes.</p><p>Script objects that inherit base class should also define a copy constructor to ensure the object is initialized properly.</p>")
	"function printDebugInfo (const var o);" TAG("Outputs information on the given object.")
	"function int disposeObject (var o);" TAG("Frees all members of the given script object and sets them to null. This can be used to automatically set all member variables of any script object to null. This may help you fix memory leaks due to reference-cycles.<p>You should only call this for objects that aren't needed anymore. Your script should not access any members of the specified object after this function returns, or you'll risk a null-reference exception. Calling this multiple times for the same script object is harmless.</p>If the specified reference is not a script object, the function returns an error. If it was successful, it returns zero.")
;

//------------------------------------------------------------------------------
// class info constants
//------------------------------------------------------------------------------

static const JILChar*	kClassName		=	"runtime";
static const JILChar*	kPackageList	=	"";
static const JILChar*	kAuthorName		=	"www.jewe.org";
static const JILChar*	kAuthorString	=	"Static class that provides access to functions of the JewelScript runtime.";
static const JILChar*	kTimeStamp		=	"2015-03-28 22:07:07";

//------------------------------------------------------------------------------
// forward declare internal functions
//------------------------------------------------------------------------------

static JILError bind_runtime_GetDecl     (JILUnknown* pDataIn);
static JILError bind_runtime_CallStatic  (NTLInstance* pInst, JILLong funcID);

//------------------------------------------------------------------------------
// native type proc
//------------------------------------------------------------------------------
// This is the function you need to register with the script runtime.

JILEXTERN JILError JILRuntimeProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	int result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_OnImport:				break;
		case NTL_Initialize:			break;
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return bind_runtime_CallStatic(pInst, param);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;
		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const JILChar**) ppDataOut) = kClassName; break;
		case NTL_GetPackageString:		(*(const JILChar**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			return bind_runtime_GetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const JILChar**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const JILChar**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const JILChar**) ppDataOut) = kAuthorString; break;
		// return error on unknown messages
		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// bind_runtime_GetDecl
//------------------------------------------------------------------------------

static JILError bind_runtime_GetDecl(JILUnknown* pDataIn)
{
	NTLDeclareVerbatim(pDataIn, kClassDeclaration);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_CallStatic
//------------------------------------------------------------------------------

static JILError bind_runtime_CallStatic(NTLInstance* pInst, JILLong funcID)
{
	JILError error = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM
	switch( funcID )
	{
		case fn_traceException: // function int traceException ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kTraceExceptionEnabled) == kTraceExceptionEnabled);
			break;
		}
		case fn_runtimeChecks: // function int runtimeChecks ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kExtendedRuntimeChecks) == kExtendedRuntimeChecks);
			break;
		}
		case fn_debugBuild: // function int debugBuild ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kDebugBuild) == kDebugBuild);
			break;
		}
		case fn_releaseBuild: // function int releaseBuild ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kReleaseBuild) == kReleaseBuild);
			break;
		}
		case fn_libraryVersion: // function string libraryVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.LibraryVersion));
			break;
		}
		case fn_runtimeVersion: // function string runtimeVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.RuntimeVersion));
			break;
		}
		case fn_compilerVersion: // function string compilerVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.CompilerVersion));
			break;
		}
		case fn_typeInterfaceVersion: // function string typeInterfaceVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.TypeInterfaceVersion));
			break;
		}
		case fn_stackSize: // function int stackSize ()
		{
			NTLReturnInt(ps, ps->vmDataStackSize);
			break;
		}
		case fn_instructionCounter: // function float instructionCounter ()
		{
			NTLReturnFloat(ps, (JILFloat)ps->vmInstructionCounter);
			break;
		}
		case fn_getTypeName: // function string getTypeName (int type)
		{
			const JILChar* pResult = "(invalid)";
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				pResult = JILCStrGetString(ps, pTI->offsetName);
			}
			NTLReturnString(ps, pResult);
			break;
		}
		case fn_getTypeID: // function int getTypeID (const string name)
		{
			const JILChar* arg_0 = NTLGetArgString(ps, 0);
			NTLReturnInt(ps, NTLTypeNameToTypeID(ps, arg_0));
			break;
		}
		case fn_getTypeFamily: // function int getTypeFamily (int type)
		{
			JILLong tf = tf_undefined;
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				tf = pTI->family;
			}
			NTLReturnInt(ps, tf);
			break;
		}
		case fn_getBaseType: // function int getBaseType (int type)
		{
			JILLong base = 0;
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				base = pTI->base;
			}
			NTLReturnInt(ps, base);
			break;
		}
		case fn_isTypeNative: // function int isTypeNative (int type)
		{
			JILLong isNative = JILFalse;
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				isNative = pTI->isNative;
			}
			NTLReturnInt(ps, isNative);
			break;
		}
		case fn_getTypeVersion: // function int getTypeVersion (int type)
		{
			JILChar buf[32];
			JILLong version = 0;
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				if( pTI->isNative )
					version = pTI->authorVersion;
			}
			NTLReturnString(ps, JILLongToRevision(buf, version));
			break;
		}
		case fn_getTypeAuthor: // function string getTypeAuthor (int type)
		{
			const JILChar* pName = "(invalid)";
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				if( pTI->isNative )
					CallNTLGetAuthorName(pTI->typeProc, &pName);
			}
			NTLReturnString(ps, pName);
			break;
		}
		case fn_getTypeDescription: // function string getTypeDescription (int type)
		{
			const JILChar* pDescription = "(invalid)";
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				if( pTI->isNative )
					CallNTLGetAuthorString(pTI->typeProc, &pDescription);
			}
			NTLReturnString(ps, pDescription);
			break;
		}
		case fn_getTypeTimeStamp: // function string getTypeTimeStamp (int type)
		{
			const JILChar* pTimeStamp = "(invalid)";
			JILLong type = NTLGetArgInt(ps, 0);
			if( NTLIsValidTypeID(ps, type) )
			{
				JILTypeInfo* pTI = JILTypeInfoFromType(ps, type);
				if( pTI->isNative )
					CallNTLGetBuildTimeStamp(pTI->typeProc, &pTimeStamp);
			}
			NTLReturnString(ps, pTimeStamp);
			break;
		}
		case fn_getNumTypes: // function int getNumTypes ()
		{
			NTLReturnInt(ps, ps->vmUsedTypeInfoSegSize);
			break;
		}
		case fn_generateBindings: // function int generateBindings (const string path)
		{
			NTLReturnInt(ps, JCLGenerateBindings(ps, NTLGetArgString(ps, 0)));
			break;
		}
		case fn_generateDocs: // function int generateDocs (const string path, const string args)
		{
			NTLReturnInt(ps, JCLGenerateDocs(ps, NTLGetArgString(ps, 0), NTLGetArgString(ps, 1)));
			break;
		}
		case fn_printLog: // function printLog (const string[] args)
		{
			JILArray* pArray;
			JILString* pResult;
			pArray = (JILArray*) NTLGetArgObject(ps, 0, type_array);
			if( pArray )
			{
				pResult = JILArray_ToString(pArray);
				JILMessageLog(ps, "%s", JILString_String(pResult));
				JILString_Delete(pResult);
			}
			break;
		}
		case fn_clone: // function var clone (const var o)
		{
			JILHandle* hArg = NTLGetArgHandle(ps, 0);
			JILHandle* hResult = NTLCopyHandle(ps, hArg);
			NTLReturnHandle(ps, hResult);
			NTLFreeHandle(ps, hArg);
			NTLFreeHandle(ps, hResult);
			break;
		}
		case fn_printDebugInfo: // function printDebugInfo (const var o)
		{
			JILHandle* hArg = NTLGetArgHandle(ps, 0);
			JILLong* pL = (JILLong*) hArg->data;
			JILMessageLog(ps, "handle %X {flags = %X, refCount = %d, type = %s, data = %8.8X%8.8X}\n", hArg, hArg->flags, hArg->refCount, JILGetHandleTypeName(ps, hArg->type), pL[0], pL[1]);
			NTLFreeHandle(ps, hArg);
			break;
		}
		case fn_disposeObject: // function disposeObject (var o)
		{
			JILHandle* hArg = NTLGetArgHandle(ps, 0);
			NTLReturnInt(ps, NTLDisposeObject(ps, hArg));
			NTLFreeHandle(ps, hArg);
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
