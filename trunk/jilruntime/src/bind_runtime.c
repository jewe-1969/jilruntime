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

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("Static class containing functions that provide information about the JewelScript runtime.")
	"function int traceException ();" TAG("Returns true if the virtual machine supports the trace exception.")
	"function int runtimeChecks ();" TAG("Returns true if the virtual machine performs extended runtime checks.")
	"function int debugBuild ();" TAG("Returns true if this is a debug build of the runtime.")
	"function int releaseBuild ();" TAG("Return true if this is a release build of the runtime.")
	"function string libraryVersion ();" TAG("Returns the version number of the JewelScript library.")
	"function string runtimeVersion ();" TAG("Returns the version number of the runtime.")
	"function string compilerVersion ();" TAG("Returns the version number of the compiler.")
	"function string typeInterfaceVersion ();" TAG("Returns the version number of the native type interface.")
	"function int stackSize ();" TAG("Returns the stack size specified when initializing this runtime.")
	"function int instructionCounter ();" TAG("Returns the current value of the instruction counter. The instruction counter is a signed 32-bit integer that is increased for each executed VM instruction. If this feature has been disabled, the result is always 0.")
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
;

//------------------------------------------------------------------------------
// class info constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"runtime"; // The class name that will be used in JewelScript.
static const char*	kPackageList	=	"";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Provides access to the JewelScript compiler and runtime.";
static const char*	kTimeStamp		=	"16.02.2010";

//------------------------------------------------------------------------------
// forward declare internal functions
//------------------------------------------------------------------------------

static JILError bind_runtime_GetDecl		(JILUnknown* pDataIn);
static JILError bind_runtime_CallStatic		(NTLInstance* pInst, JILLong funcID);

//------------------------------------------------------------------------------
// native type proc
//------------------------------------------------------------------------------
// This is the function you need to register with the script runtime.

JILError JILRuntimeProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	int result = JIL_No_Exception;
	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return bind_runtime_CallStatic(pInst, param);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;
		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetPackageString:		(*(const char**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			return bind_runtime_GetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;
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
	// Dynamically build the class declaration
	NTLDeclareVerbatim(pDataIn, kClassDeclaration); // add the static part of the class declaration
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// bind_runtime_CallStatic
//------------------------------------------------------------------------------

static JILError bind_runtime_CallStatic(NTLInstance* pInst, JILLong funcID)
{
	JILError result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM
	switch( funcID )
	{
		case 0: // function int traceException ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kTraceExceptionEnabled) == kTraceExceptionEnabled);
			break;
		}
		case 1: // function int runtimeChecks ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kExtendedRuntimeChecks) == kExtendedRuntimeChecks);
			break;
		}
		case 2: // function int debugBuild ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kDebugBuild) == kDebugBuild);
			break;
		}
		case 3: // function int releaseBuild ()
		{
			NTLReturnInt(ps, (ps->vmVersion.BuildFlags & kReleaseBuild) == kReleaseBuild);
			break;
		}
		case 4: // function string libraryVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.LibraryVersion));
			break;
		}
		case 5: // function string runtimeVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.RuntimeVersion));
			break;
		}
		case 6: // function string compilerVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.CompilerVersion));
			break;
		}
		case 7: // function string typeInterfaceVersion ()
		{
			JILChar buf[32];
			NTLReturnString(ps, JILLongToRevision(buf, ps->vmVersion.TypeInterfaceVersion));
			break;
		}
		case 8: // function int stackSize ()
		{
			NTLReturnInt(ps, ps->vmDataStackSize);
			break;
		}
		case 9: // function int instructionCounter ()
		{
			NTLReturnInt(ps, ps->vmInstructionCounter);
			break;
		}
		case 10: // function string getTypeName (int type)
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
		case 11: // function int getTypeID (const string name)
		{
			const JILChar* arg_0 = NTLGetArgString(ps, 0);
			NTLReturnInt(ps, NTLTypeNameToTypeID(ps, arg_0));
			break;
		}
		case 12: // function int getTypeFamily (int type)
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
		case 13: // function int getBaseType (int type)
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
		case 14: // function int isTypeNative (int type)
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
		case 15: // function int getTypeVersion (int type)
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
		case 16: // function string getTypeAuthor (int type)
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
		case 17: // function string getTypeDescription (int type)
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
		case 18: // function string getTypeTimeStamp (int type)
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
		case 19: // function int getNumTypes ()
		{
			NTLReturnInt(ps, ps->vmUsedTypeInfoSegSize);
			break;
		}
		case 20: // function int generateBindings (const string path)
		{
			NTLReturnInt(ps, JCLGenerateBindings(ps, NTLGetArgString(ps, 0)));
			break;
		}
		case 21: // function int generateDocs (const string path, const string args)
		{
			NTLReturnInt(ps, JCLGenerateDocs(ps, NTLGetArgString(ps, 0), NTLGetArgString(ps, 1)));
			break;
		}
		case 22: // function printLog (const string[] args)
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
		default:
		{
			result = JIL_ERR_Invalid_Function_Index;
			break;
		}
	}
	return result;
}
