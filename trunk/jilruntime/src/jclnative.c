//------------------------------------------------------------------------------
// File: jclnative.c                                           (c) 2010 jewe.org
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
/// @file jclnative.c
/// Contains functions that use the compiler's data structures to generate
/// native C/C++ binding code. The code generator will run on any JewelScript
/// class declared with the modifier keyword 'native' and generate a C source
/// code file containing binding code for that class.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclstate.h"

#if JIL_USE_BINDING_CODEGEN && !JIL_NO_FPRINTF && JIL_USE_LOCAL_FILESYS

//------------------------------------------------------------------------------
// Error handling macros
//------------------------------------------------------------------------------

#define ERROR_IF(COND,ERR,ARG,S)	do {if(COND){err = EmitError(_this,ARG,ERR); S;}} while(0)
#define ERROR(ERR,ARG,S)			do {err = EmitError(_this,ARG,ERR); S;} while(0)

//------------------------------------------------------------------------------
// external functions from JCLState
//------------------------------------------------------------------------------

JILError EmitError(JCLState* _this, const JCLString* pArg, JILError err);

//------------------------------------------------------------------------------
// internal helper functions
//------------------------------------------------------------------------------

static JILError GenerateCallCode(JCLState*, JCLClass*, JCLFunc*, FILE*, const JILChar*);
static JILBool NativeTypeNameFromTypeID(JCLState*, JCLString*, JILLong);
static JILLong GetNumFuncs(JCLClass*, JILBool);
static void DerivePackageString(JCLState*, JCLClass*, JCLString*);
static void DelegateToString(JCLState*, JCLFuncType*, const JILChar*, JCLString*);
static JILError SearchClassDelegates(JCLState*, JCLClass*, FILE*);
static void AddToEnum(Array_JCLString* pArr, JCLString* workstr);

//------------------------------------------------------------------------------
// The header of the file to generate
//------------------------------------------------------------------------------

static const JILChar* kHeaderComment =
"// This is an automatically created binding code file for JewelScript.\n"
"// It allows you to easily bind your external C++ code to the script runtime,\n"
"// and to use your external functions and classes from within JewelScript.\n"
"//\n"
"// For more information see: http://blog.jewe.org/?p=29\n"
;

//------------------------------------------------------------------------------
// JCLCreateBindingCode
//------------------------------------------------------------------------------
// Main entry point from the compiler to generate binding code for the given class.

JILError JCLCreateBindingCode(JCLState* _this, JCLClass* pClass, const JILChar* pPath)
{
	JILError err = JCL_No_Error;

	JCLString* workstr = NULL;
	JCLString* workstr2 = NULL;
	FILE* outStream = NULL;
	JCLString* funcPrefix = NULL;	// prefix used for every native function generated
	JCLString* objectName = NULL;	// name used for the native class / struct
	JCLString* baseName = NULL;		// name of base interface if used
	JCLString* packageList = NULL;	// list of imports required to compile this class
	Array_JCLString* enumeration = NULL;	// list of enum names for functions
	const JILChar* sFuncPrefix;
	const JILChar* sObjectName;
	JILLong i;
	JILBool isInherit = (pClass->miBaseType != 0);

	workstr2 = NEW(JCLString);
	enumeration = NEW(Array_JCLString);

	// create base name
	baseName = NEW(JCLString);
	if (isInherit)
	{
		JCLClass* cbase = GetClass(_this, pClass->miBaseType);
		JCLSetString(baseName, JCLGetString(cbase->mipName));
	}

	// create package list
	packageList = NEW(JCLString);
	DerivePackageString(_this, pClass, packageList);

	// create native class name by prefixing script class name with 'C'
	objectName = NEW(JCLString);
	JCLSetString(objectName, "C");
	JCLAppend(objectName, JCLGetString(pClass->mipName));
	sObjectName = JCLGetString(objectName);

	// create function prefix
	funcPrefix = NEW(JCLString);
	JCLSetString(funcPrefix, "bind_");
	JCLAppend(funcPrefix, JCLGetString(pClass->mipName));
	JCLAppend(funcPrefix, "_");
	sFuncPrefix = JCLGetString(funcPrefix);

	// create output file name
	workstr = NEW(JCLString);
	JCLSetString(workstr, "bind_");
	JCLAppend(workstr, JCLGetString(pClass->mipName));
	JCLAppend(workstr, ".cpp");
	JCLFormat(workstr2, "%s\\%s", pPath, JCLGetString(workstr));

	// don't generate for classes that are only forwarded
	if( !pClass->miHasBody )
		goto exit;

	// open the file
	outStream = fopen(JCLGetString(workstr2), "w");
	ERROR_IF(outStream == NULL, JCL_ERR_Native_Code_Generator, workstr, goto exit);

	// write header
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// File: %s\n", JCLGetString(workstr));
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, kHeaderComment);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "#include <string>\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "#include \"jilapi.h\"\n");
	fprintf(outStream, "#include \"jiltools.h\"\n");
	fprintf(outStream, "\n");
	JCLSetString(workstr, sObjectName);
	JCLAppend(workstr, ".h");
	fprintf(outStream, "// TODO: This assumes your external C++ code is in \"%s\" and your native class is \"%s\" (use search/replace to change this if needed)\n", JCLGetString(workstr), sObjectName);
	fprintf(outStream, "// TODO: You may have to add more includes before this one to be able to include \"%s\"\n", JCLGetString(workstr));
	fprintf(outStream, "#include \"%s\"\n", JCLGetString(workstr));
	fprintf(outStream, "\n");
	fprintf(outStream, "// TODO: This forward declares your new type proc. Copy this line to the top of the file where you initialize the runtime and register your native types.\n");
	fprintf(outStream, "JILEXTERN JILError %sproc(NTLInstance*, JILLong, JILLong, JILUnknown*, JILUnknown**);\n", sFuncPrefix);
	fprintf(outStream, "\n");
	fprintf(outStream, "// TODO: You can use this code to register your new type to the runtime:\n");
	fprintf(outStream, "// JILError err = JILRegisterNativeType( pVM, %sproc );\n", sFuncPrefix);
	fprintf(outStream, "\n");

	// create function enumeration
	fprintf(outStream, "//-----------------------------------------------------------------------------------\n");
	fprintf(outStream, "// function enumeration - this must be kept in sync with the class declaration below.\n");
	fprintf(outStream, "//-----------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "enum {\n");
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		JCLSetString(workstr, "fn_");
		JCLAppend(workstr, JCLGetString(pFunc->mipName));
		AddToEnum(enumeration, workstr);
		fprintf(outStream, "\t%s", JCLGetString(workstr));
		if( i < (pClass->mipFuncs->Count(pClass->mipFuncs) - 1) )
			fprintf(outStream, ",");
		fprintf(outStream, "\n");
	}
	fprintf(outStream, "};\n");
	fprintf(outStream, "\n");

	// create class declaration string
	fprintf(outStream, "//--------------------------------------------------------------------------------------------\n");
	fprintf(outStream, "// class declaration string - order of declarations must be kept in sync with the enumeration.\n");
	fprintf(outStream, "//--------------------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static const JILChar* kClassDeclaration =\n");
	JCLSetString(workstr2, JCLGetString(pClass->mipName));
	JCLAppend(workstr2, "::");
	if( JCLGetLength(pClass->mipTag) )
		fprintf(outStream, "\tTAG(\"%s\")\n", JCLGetString(pClass->mipTag));
	else
		fprintf(outStream, "\tTAG(\"TODO: You can fill these tags with documentation. They will be used by the HTML documentation engine.\")\n");
	SearchClassDelegates(_this, pClass, outStream);
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		pFunc->ToString(pFunc, _this, workstr, kClearFirst|kFullDecl|kIdentNames|kNoClassName);
		ERROR_IF(pFunc->miCofunc, JCL_ERR_Native_Code_Generator, workstr, goto exit);
		JCLReplace(workstr, JCLGetString(workstr2), "");	// TODO: Must remove scope from local delegate types ("Foo::"), not a good solution
		fprintf(outStream, "\t\"%s;\" TAG(\"%s\")\n", JCLGetString(workstr), JCLGetString(pFunc->mipTag));
	}
	fprintf(outStream, ";\n");
	fprintf(outStream, "\n");

	// create class info constants
	JCLFormat(workstr, "A native %s class for JewelScript.", JCLGetString(pClass->mipName));
	if( JCLGetLength(pClass->mipTag) )
	{
		int pos = JCLFindChar(pClass->mipTag, '.', 0);
		if( pos < 0 )
			pos = JCLGetLength(pClass->mipTag) - 1;
		JCLSubString(workstr, pClass->mipTag, 0, pos + 1);
	}
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// class info constants\n");
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static const JILChar*	kClassName		=	\"%s\"; // The class name that will be used in JewelScript.\n", JCLGetString(pClass->mipName));
	if (isInherit)
		fprintf(outStream, "static const JILChar*	kBaseName		=	\"%s\"; // The base interface this class inherits from.\n", JCLGetString(baseName));
	fprintf(outStream, "static const JILChar*	kPackageList	=	\"%s\"; // TODO: Add any classes to this list that have to be imported before this one (comma seperated)\n", JCLGetString(packageList));
	fprintf(outStream, "static const JILChar*	kAuthorName		=	\"YOUR NAME HERE\"; // TODO: You can enter your name here\n");
	fprintf(outStream, "static const JILChar*	kAuthorString	=	\"%s\"; // TODO: You can enter a description of your native type here\n", JCLGetString(workstr));
	JCLFormatTime(workstr, "%c", time(NULL));
	fprintf(outStream, "static const JILChar*	kTimeStamp		=	\"%s\"; // TODO: You can enter a build time stamp here\n", JCLGetString(workstr));
	fprintf(outStream, "static const JILChar*	kAuthorVersion	=	\"1.0.0.0\"; // TODO: You can change the version number here\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// forward declare internal functions\n");
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sRegister    (JILState* pVM);\n", sFuncPrefix);
	fprintf(outStream, "static JILError %sGetDecl     (JILUnknown* pDataIn);\n", sFuncPrefix);
	fprintf(outStream, "static JILError %sNew         (NTLInstance* pInst, %s** ppObject);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "static JILError %sDelete      (NTLInstance* pInst, %s* _this);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "static JILError %sMark        (NTLInstance* pInst, %s* _this);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "static JILError %sCallStatic  (NTLInstance* pInst, JILLong funcID);\n", sFuncPrefix);
	fprintf(outStream, "static JILError %sCallMember  (NTLInstance* pInst, JILLong funcID, %s* _this);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "\n");

	// create main proc
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// native type proc\n");
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// This is the function you need to register with the script runtime.\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "JILEXTERN JILError %sproc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)\n", sFuncPrefix);
	fprintf(outStream, "{\n");
	fprintf(outStream, "	int result = JIL_No_Exception;\n");
	fprintf(outStream, "	switch( msg )\n");
	fprintf(outStream, "	{\n");
	fprintf(outStream, "		// runtime messages\n");
	fprintf(outStream, "		case NTL_Register:				return %sRegister((JILState*) pDataIn);\n", sFuncPrefix);
	fprintf(outStream, "		case NTL_Initialize:			break;\n");
	fprintf(outStream, "		case NTL_NewObject:				return %sNew(pInst, (%s**) ppDataOut);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "		case NTL_DestroyObject:			return %sDelete(pInst, (%s*) pDataIn);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "		case NTL_MarkHandles:			return %sMark(pInst, (%s*) pDataIn);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "		case NTL_CallStatic:			return %sCallStatic(pInst, param);\n", sFuncPrefix);
	fprintf(outStream, "		case NTL_CallMember:			return %sCallMember(pInst, param, (%s*) pDataIn);\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "		case NTL_Terminate:				break;\n");
	fprintf(outStream, "		case NTL_Unregister:			break;\n");
	fprintf(outStream, "		// class information queries\n");
	fprintf(outStream, "		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);\n");
	fprintf(outStream, "		case NTL_GetAuthorVersion:		return NTLRevisionToLong(kAuthorVersion);\n");
	fprintf(outStream, "		case NTL_GetClassName:			(*(const JILChar**) ppDataOut) = kClassName; break;\n");
	if (isInherit)
		fprintf(outStream, "		case NTL_GetBaseName:			(*(const JILChar**) ppDataOut) = kBaseName; break;\n");
	fprintf(outStream, "		case NTL_GetPackageString:		(*(const JILChar**) ppDataOut) = kPackageList; break;\n");
	fprintf(outStream, "		case NTL_GetDeclString:			return %sGetDecl(pDataIn);\n", sFuncPrefix);
	fprintf(outStream, "		case NTL_GetBuildTimeStamp:		(*(const JILChar**) ppDataOut) = kTimeStamp; break;\n");
	fprintf(outStream, "		case NTL_GetAuthorName:			(*(const JILChar**) ppDataOut) = kAuthorName; break;\n");
	fprintf(outStream, "		case NTL_GetAuthorString:		(*(const JILChar**) ppDataOut) = kAuthorString; break;\n");
	fprintf(outStream, "		// return error on unknown messages\n");
	fprintf(outStream, "		default:						result = JIL_ERR_Unsupported_Native_Call; break;\n");
	fprintf(outStream, "	}\n");
	fprintf(outStream, "	return result;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create register handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sRegister\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sRegister(JILState* pVM)\n", sFuncPrefix);
	fprintf(outStream, "{\n");
	fprintf(outStream, "	// If your type library consists of multiple related classes, you could register any helper classes here.\n");
	fprintf(outStream, "	// That way your application only needs to register the main class to the script runtime.\n");
	fprintf(outStream, "	// JILError err = JILRegisterNativeType(pVM, bind_MyHelperClass_proc);\n");
	fprintf(outStream, "	return JIL_No_Exception;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create get declaration handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sGetDecl\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sGetDecl(JILUnknown* pDataIn)\n", sFuncPrefix);
	fprintf(outStream, "{\n");
	fprintf(outStream, "	// Dynamically build the class declaration\n");
	fprintf(outStream, "	NTLDeclareVerbatim(pDataIn, kClassDeclaration); // add the static part of the class declaration\n");
	for( i = 0; i < pClass->mipVars->Count(pClass->mipVars); i++ )
	{
		JCLVar* pVar = pClass->mipVars->Get(pClass->mipVars, i);
		if (pVar->miConst)
		{
			if (pVar->miType == type_int)
				fprintf(outStream, "	NTLDeclareConstantInt(pDataIn, type_int, ");
			else if (pVar->miType == type_float)
				fprintf(outStream, "	NTLDeclareConstantFloat(pDataIn, type_float, ");
			else if (pVar->miType == type_string)
				fprintf(outStream, "	NTLDeclareConstantString(pDataIn, type_string, ");
			fprintf(outStream, "\"%s\", %s::%s);\n", JCLGetString(pVar->mipName), sObjectName, JCLGetString(pVar->mipName));
		}
	}
	fprintf(outStream, "	return JIL_No_Exception;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create new handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sNew\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sNew(NTLInstance* pInst, %s** ppObject)\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "{\n");
	if( GetNumFuncs(pClass, JILTrue) )
	{
		fprintf(outStream, "	// Allocate memory and write the pointer to ppObject\n");
		fprintf(outStream, "	*ppObject = (%s*)operator new(sizeof(%s));\n", sObjectName, sObjectName);
	}
	else
	{
		fprintf(outStream, "	// Nothing to do here since this class is pure static.\n");
	}
	fprintf(outStream, "	return JIL_No_Exception;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create delete handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sDelete\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sDelete(NTLInstance* pInst, %s* _this)\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "{\n");
	if( GetNumFuncs(pClass, JILTrue) )
	{
		fprintf(outStream, "	// Destroy native instance\n");
		fprintf(outStream, "	delete _this;\n");
	}
	else
	{
		fprintf(outStream, "	// Nothing to do here since this class is pure static.\n");
	}
	fprintf(outStream, "	return JIL_No_Exception;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create mark handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sMark\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sMark(NTLInstance* pInst, %s* _this)\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "{\n");
	fprintf(outStream, "	// TODO: Add the function below to your class if you want to use the garbage collector.\n");
	fprintf(outStream, "	// The garbage collector will call this to mark all objects that are not garbage.\n");
	fprintf(outStream, "	// Call NTLMarkHandle() for all JILHandle pointers your class owns.\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "	// _this->MarkHandles(NTLInstanceGetVM(pInst));  // TODO: Uncomment and implement if you want to use GC.\n");
	fprintf(outStream, "	return JIL_No_Exception;\n");
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create static function call handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sCallStatic\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sCallStatic(NTLInstance* pInst, JILLong funcID)\n", sFuncPrefix);
	fprintf(outStream, "{\n");
	if (GetNumFuncs(pClass, JILFalse))
	{
		fprintf(outStream, "	JILError error = JIL_No_Exception;\n");
		fprintf(outStream, "	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM\n");
		fprintf(outStream, "	JILLong thisID = NTLInstanceTypeID(pInst);	// get the type-id of this class\n");
		fprintf(outStream, "	switch( funcID )\n");
		fprintf(outStream, "	{\n");
		for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
		{
			JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
			if (!pFunc->miMethod)
			{
				pFunc->ToString(pFunc, _this, workstr, kClearFirst|kFullDecl|kIdentNames|kNoClassName);
				fprintf(outStream, "		case %s: // %s\n", JCLGetString(enumeration->Get(enumeration, pFunc->miFuncIdx)), JCLGetString(workstr));
				fprintf(outStream, "		{\n");
				err = GenerateCallCode(_this, pClass, pFunc, outStream, sObjectName);
				if (err)
					goto exit;
				fprintf(outStream, "			break;\n");
				fprintf(outStream, "		}\n");
			}
		}
		fprintf(outStream, "		default:\n");
		fprintf(outStream, "		{\n");
		fprintf(outStream, "			error = JIL_ERR_Invalid_Function_Index;\n");
		fprintf(outStream, "			break;\n");
		fprintf(outStream, "		}\n");
		fprintf(outStream, "	}\n");
		fprintf(outStream, "	return error;\n");
	}
	else
	{
		fprintf(outStream, "	return JIL_ERR_Invalid_Function_Index;\n");
	}
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

	// create member function call handler
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "// %sCallMember\n", sFuncPrefix);
	fprintf(outStream, "//------------------------------------------------------------------------------\n");
	fprintf(outStream, "\n");
	fprintf(outStream, "static JILError %sCallMember(NTLInstance* pInst, JILLong funcID, %s* _this)\n", sFuncPrefix, sObjectName);
	fprintf(outStream, "{\n");
	if (GetNumFuncs(pClass, JILTrue))
	{
		fprintf(outStream, "	JILError error = JIL_No_Exception;\n");
		fprintf(outStream, "	JILState* ps = NTLInstanceGetVM(pInst);		// get pointer to VM\n");
		fprintf(outStream, "	JILLong thisID = NTLInstanceTypeID(pInst);	// get the type-id of this class\n");
		fprintf(outStream, "	switch( funcID )\n");
		fprintf(outStream, "	{\n");
		for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
		{
			JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
			if (pFunc->miMethod)
			{
				pFunc->ToString(pFunc, _this, workstr, kClearFirst|kFullDecl|kIdentNames|kNoClassName);
				fprintf(outStream, "		case %s: // %s\n", JCLGetString(enumeration->Get(enumeration, pFunc->miFuncIdx)), JCLGetString(workstr));
				fprintf(outStream, "		{\n");
				err = GenerateCallCode(_this, pClass, pFunc, outStream, sObjectName);
				if (err)
					goto exit;
				fprintf(outStream, "			break;\n");
				fprintf(outStream, "		}\n");
			}
		}
		fprintf(outStream, "		default:\n");
		fprintf(outStream, "		{\n");
		fprintf(outStream, "			error = JIL_ERR_Invalid_Function_Index;\n");
		fprintf(outStream, "			break;\n");
		fprintf(outStream, "		}\n");
		fprintf(outStream, "	}\n");
		fprintf(outStream, "	return error;\n");
	}
	else
	{
		fprintf(outStream, "	return JIL_ERR_Invalid_Function_Index;\n");
	}
	fprintf(outStream, "}\n");
	fprintf(outStream, "\n");

exit:
	if( outStream != NULL )
		fclose(outStream);
	DELETE(workstr);
	DELETE(workstr2);
	DELETE(funcPrefix);
	DELETE(objectName);
	DELETE(baseName);
	DELETE(packageList);
	DELETE(enumeration);

	return err;
}

//------------------------------------------------------------------------------
// GenerateCallCode
//------------------------------------------------------------------------------

static void AddToEnum(Array_JCLString* pArr, JCLString* workstr)
{
	int exists;
	int idx;
	int i;
	JCLString* temp = NEW(JCLString);
	temp->Copy(temp, workstr);
	for( idx = 2; ; idx++ )
	{
		exists = JILFalse;
		for( i = 0; i < pArr->Count(pArr); i++ )
		{
			if( JCLCompare(pArr->Get(pArr, i), workstr) )
			{
				exists = JILTrue;
				break;
			}
		}
		if( !exists )
			break;
		JCLFormat(workstr, "%s%d", JCLGetString(temp), idx);
	}
	DELETE(temp);
	temp = pArr->New(pArr);
	temp->Copy(temp, workstr);
}

//------------------------------------------------------------------------------
// GenerateCallCode
//------------------------------------------------------------------------------
// Helper function that generates the binding code to a native function call.

static JILError GenerateCallCode(JCLState* _this, JCLClass* pClass, JCLFunc* pFunc, FILE* outStream, const JILChar* sObjectName)
{
	JILError err = JCL_No_Error;
	JILLong i;
	Array_JCLVar* pArgs = pFunc->mipArgs;
	JILLong numArg = pArgs->Count(pArgs);
	JCLVar* pArg;
	JCLString* workstr = NULL;
	JCLString* callPrefix = NULL;
	JILBool bFreeResult = JILFalse;

	workstr = NEW(JCLString);
	callPrefix = NEW(JCLString);

	// 1. extract arguments
	for( i = 0; i < numArg; i++ )
	{
		pArg = pArgs->Get(pArgs, i);
		if (pArg->miType != type_int && pArg->miType != type_float && pArg->miType != type_string)
			fprintf(outStream, "\t\t\tJILHandle* h_arg_%d = NTLGetArgHandle(ps, %d);\n", i, i);
	}
	for( i = 0; i < numArg; i++ )
	{
		pArg = pArgs->Get(pArgs, i);
		if (pArg->miType == type_int)
			fprintf(outStream, "\t\t\tJILLong arg_%d = NTLGetArgInt(ps, %d);\n", i, i);
		else if (pArg->miType == type_float)
			fprintf(outStream, "\t\t\tJILFloat arg_%d = NTLGetArgFloat(ps, %d);\n", i, i);
		else if (pArg->miType == type_string)
			fprintf(outStream, "\t\t\tconst JILChar* arg_%d = NTLGetArgString(ps, %d);\n", i, i);
		else
		{
			if (pClass->miType == pArg->miType)
			{
				fprintf(outStream, "\t\t\t%s* arg_%d = (%s*)NTLHandleToObject(ps, thisID, h_arg_%d);\n",
					sObjectName, i,
					sObjectName, i);
			}
			else if (NativeTypeNameFromTypeID(_this, workstr, pArg->miType))
			{
				JCLClass* ctype = GetClass(_this, pArg->miType);
				fprintf(outStream, "\t\t\t%s arg_%d = (%s)NTLHandleToObject(ps, NTLTypeNameToTypeID(ps, \"%s\"), h_arg_%d);\n",
					JCLGetString(workstr), i,
					JCLGetString(workstr),
					JCLGetString(ctype->mipName), i);
			}
		}
	}
	// 2. call function or method
	if (pFunc->miCtor)
	{
		if (numArg == 0)
		{
			fprintf(outStream, "\t\t\tnew (_this) %s(); // using placement new to instantiate into '_this'\n", sObjectName);
		}
		else if (numArg == 1 && pFunc->mipArgs->Get(pFunc->mipArgs, 0)->miType == pClass->miType) // special case for copy-constructor
		{
			fprintf(outStream, "\t\t\tnew (_this) %s(*arg_0); // TODO: Make sure your C++ class implements the copy-constructor correctly\n", sObjectName);
		}
		else
		{
			fprintf(outStream, "\t\t\tnew (_this) %s(", sObjectName);
			for( i = 0; i < numArg; i++ )
			{
				pArg = pArgs->Get(pArgs, i);
				if (NativeTypeNameFromTypeID(_this, workstr, pArg->miType) || pArg->miType == pClass->miType)
					fprintf(outStream, "arg_%d", i);
				else
					fprintf(outStream, "h_arg_%d", i);
				if (i < (numArg - 1))
					fprintf(outStream, ", ", i);
			}
			fprintf(outStream, "); // using placement new to instantiate into '_this'\n");
		}
	}
	else
	{
		if (pFunc->miMethod)
		{
			JCLSetString(callPrefix, "_this->");
		}
		else
		{
			JCLSetString(callPrefix, sObjectName);
			JCLAppend(callPrefix, "::");
		}
		if (pFunc->miConvertor)
		{
			JCLClass* res = GetClass(_this, pFunc->mipResult->miType);
			JCLAppend(callPrefix, JCLGetString(res->mipName));
			JCLAppend(callPrefix, "_");
		}
		JCLAppend(callPrefix, JCLGetString(pFunc->mipName));
		if (pFunc->mipResult->miMode == kModeUnused)
		{
			// no return value
			fprintf(outStream, "\t\t\t%s(", JCLGetString(callPrefix));
		}
		else
		{
			// return value
			pArg = pFunc->mipResult;
			if (pArg->miType == type_int)
			{
				fprintf(outStream, "\t\t\tJILLong result = %s(", JCLGetString(callPrefix));
			}
			else if (pArg->miType == type_float)
			{
				fprintf(outStream, "\t\t\tJILFloat result = %s(", JCLGetString(callPrefix));
			}
			else if (pArg->miType == type_string)
			{
				fprintf(outStream, "\t\t\tconst JILChar* result = %s(", JCLGetString(callPrefix));
			}
			else if (NativeTypeNameFromTypeID(_this, workstr, pArg->miType))
			{
				// can get native pointer
				fprintf(outStream, "\t\t\t%s result = %s(", JCLGetString(workstr), JCLGetString(callPrefix));
			}
			else
			{
				// cant get native pointer
				fprintf(outStream, "\t\t\tJILHandle* result = %s(", JCLGetString(callPrefix));
				bFreeResult = JILTrue;
			}
		}
		for( i = 0; i < numArg; i++ )
		{
			pArg = pArgs->Get(pArgs, i);
			if (NativeTypeNameFromTypeID(_this, workstr, pArg->miType) || pArg->miType == pClass->miType)
				fprintf(outStream, "arg_%d", i);
			else
				fprintf(outStream, "h_arg_%d", i);
			if (i < (numArg - 1))
				fprintf(outStream, ", ", i);
		}
		fprintf(outStream, ");");
		if (pFunc->miConvertor)
			fprintf(outStream, " // TODO: Your C++ class must have this convertor method.");
		fprintf(outStream, "\n");
	}
	// 3. return result
	pArg = pFunc->mipResult;
	if (pArg->miMode != kModeUnused)
	{
		if (pArg->miType == type_int)
		{
			fprintf(outStream, "\t\t\tNTLReturnInt(ps, result);\n");
		}
		else if (pArg->miType == type_float)
		{
			fprintf(outStream, "\t\t\tNTLReturnFloat(ps, result);\n");
		}
		else if (pArg->miType == type_string)
		{
			fprintf(outStream, "\t\t\tNTLReturnString(ps, result);\n");
		}
		else
		{
			if (bFreeResult)
			{
				// result is handle
				fprintf(outStream, "\t\t\tNTLReturnHandle(ps, result);\n");
			}
			else
			{
				// result is pointer
				if (pArg->miType == pClass->miType)
				{
					fprintf(outStream, "\t\t\tJILHandle* hResult = NTLNewHandleForObject(ps, thisID, result);\n");
				}
				else
				{
					JCLClass* ctype = GetClass(_this, pArg->miType);
					fprintf(outStream, "\t\t\tJILHandle* hResult = NTLNewHandleForObject(ps, NTLTypeNameToTypeID(ps, \"%s\"), result);\n", JCLGetString(ctype->mipName));
				}
				fprintf(outStream, "\t\t\tNTLReturnHandle(ps, hResult);\n");
				fprintf(outStream, "\t\t\tNTLFreeHandle(ps, hResult);\n");
			}
		}
	}
	// 4. clean up
	for( i = 0; i < numArg; i++ )
	{
		pArg = pArgs->Get(pArgs, i);
		if (pArg->miType != type_int && pArg->miType != type_float && pArg->miType != type_string)
			fprintf(outStream, "\t\t\tNTLFreeHandle(ps, h_arg_%d);\n", i);
	}
	if (bFreeResult)
	{
		fprintf(outStream, "\t\t\tNTLFreeHandle(ps, result);\n");
	}

	DELETE(workstr);
	DELETE(callPrefix);
	return err;
}

//------------------------------------------------------------------------------
// NativeTypeNameFromTypeID
//------------------------------------------------------------------------------
// Helper function that returns a native type name from a type-id.
// If this returns JILFalse, it means the type is private to the runtime, and
// a pointer to the object can't be obtained, the value can only be passed as
// handle pointer to native code.

static JILBool NativeTypeNameFromTypeID(JCLState* _this, JCLString* outString, JILLong type)
{
	JILBool canObtainPointer = JILTrue;
	JCLClear(outString);
	switch (type)
	{
		case type_int:
			JCLSetString(outString, "JILLong");
			break;
		case type_float:
			JCLSetString(outString, "JILFloat");
			break;
		case type_string:
			JCLSetString(outString, "JILString*");
			break;
		case type_array:
			JCLSetString(outString, "JILArray*");
			break;
		case type_list:
			JCLSetString(outString, "JILList*");
			break;
		case type_iterator:
			JCLSetString(outString, "JILIterator*");
			break;
		case type_table:
			JCLSetString(outString, "JILTable*");
			break;
		default:
		{
			JCLClass* pClass = GetClass(_this, type);
			switch (pClass->miFamily)
			{
				case tf_class:	// user defined class
					if (pClass->miNative || (pClass->miModifier & kModiNativeBinding))
					{
						JCLSetString(outString, "C");
						JCLAppend(outString, JCLGetString(pClass->mipName));
						JCLAppend(outString, "*");
					}
					else
					{
						canObtainPointer = JILFalse;
					}
					break;
				case tf_interface:	// treat interfaces as handles!
				case tf_undefined:
				case tf_thread:
				case tf_delegate:
					canObtainPointer = JILFalse;
					break;
			}
		}
	}
	return canObtainPointer;
}

//------------------------------------------------------------------------------
// GetNumFuncs
//------------------------------------------------------------------------------
// Counts and returns the number of global functions (bMethods = false),
// or member functions (bMethods = true).

static JILLong GetNumFuncs(JCLClass* pClass, JILBool bMethods)
{
	JILLong i;
	JILLong num = 0;
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if ((pFunc->miMethod && bMethods) || (!pFunc->miMethod && !bMethods))
		{
			num++;
		}
	}
	return num;
}

//------------------------------------------------------------------------------
// DerivePackageString
//------------------------------------------------------------------------------
// Derives a package string (comma seperated list of native class names) from
// all types declared in all functions of the given class.

static void DerivePackageString(JCLState* _this, JCLClass* pClass, JCLString* outstr)
{
	JILLong fn;
	JILLong p;
	JILLong type;
	JCLClass* pType;
	Array_JCLString* pList;

	pList = NEW(Array_JCLString);
	JCLClear(outstr);

	for( fn = 0; fn < pClass->mipFuncs->Count(pClass->mipFuncs); fn++)
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, fn);
		for( p = -1; p < pFunc->mipArgs->Count(pFunc->mipArgs); p++)
		{
			JCLVar* pArg;
			if( p >= 0)
				pArg = pFunc->mipArgs->Get(pFunc->mipArgs, p);
			else
				pArg = pFunc->mipResult;
			type = pArg->miType;
			// ignore built-in types
			if( (type < kNumPredefTypes) || (type == pClass->miType) ) // TODO: Exclusion of default imports hardcoded :-/
				continue;
			pType = GetClass(_this, type);
			if( pType->miFamily == tf_class && (pType->miNative || (pType->miModifier & kModiNativeBinding)) )
			{
				JILLong i;
				// check if we already have this type in the list
				for( i = 0; i < pList->Count(pList); i++ )
				{
					if( JCLCompare(pType->mipName, pList->Get(pList, i)) )
						break;
				}
				// if not found, add to list
				if( i == pList->Count(pList) )
				{
					JCLString* str = pList->New(pList);
					JCLSetString(str, JCLGetString(pType->mipName));
				}
			}
		}
	}
	// build package string from list
	if( pList->Count(pList) )
	{
		JILLong i;
		for( i = 0; i < pList->Count(pList); i++ )
		{
			JCLAppend(outstr, JCLGetString(pList->Get(pList, i)));
			if ( i < (pList->Count(pList) - 1))
				JCLAppend(outstr, ", ");
		}
	}

	DELETE(pList);
}

//------------------------------------------------------------------------------
// DelegateToString
//------------------------------------------------------------------------------
// Create a string representation of this delegate.

static void DelegateToString(JCLState* _this, JCLFuncType* pFuncType, const JILChar* pDelegateName, JCLString* outString)
{
	JCLVar* pVar;
	Array_JCLVar* pArgs;
	JILLong i;

	JCLSetString(outString, "delegate ");

	// write delegate result
	pVar = pFuncType->mipResult;
	if( pVar->miType != type_null )
	{
		pVar->ToString(pVar, _this, outString, 0);
		JCLAppend(outString, " ");
	}

	// write delegate name
	JCLAppend(outString, pDelegateName);
	JCLAppend(outString, " (");

	// write delegate arguments
	pArgs = pFuncType->mipArgs;
	for( i = 0; i < pArgs->Count(pArgs); i++ )
	{
		// write argument
		pVar = pArgs->Get(pArgs, i);
		pVar->ToString(pVar, _this, outString, kIdentNames);
		// write comma if this wasn't the last arg
		if( i < (pArgs->Count(pArgs) - 1) )
			JCLAppend(outString, ", ");
	}
	// write end
	JCLAppend(outString, ")");
}

//------------------------------------------------------------------------------
// SearchClassDelegates
//------------------------------------------------------------------------------
// Search and output all delegates declared in this class.

static JILError SearchClassDelegates(JCLState* _this, JCLClass* pClass, FILE* outStream)
{
	JILError err = JCL_No_Error;
	JCLClass* pType;
	JILLong cl;
	JILLong al;
	JCLString* classtr = NEW(JCLString);
	JCLString* workstr = NEW(JCLString);

	JCLSetString(classtr, JCLGetString(pClass->mipName));
	JCLAppend(classtr, "::");

	for( cl = 0; cl < _this->mipClasses->Count(_this->mipClasses); cl++ )
	{
		pType = GetClass(_this, cl);
		if( pType->miFamily == tf_delegate )
		{
			for( al = 0; al < pType->mipAlias->Count(pType->mipAlias); al++ )
			{
				JCLString* alias = pType->mipAlias->Get(pType->mipAlias, al);
				if( JCLFindString(alias, JCLGetString(classtr), 0) == 0 )
				{
					DelegateToString(_this, pType->mipFuncType, JCLGetString(alias), workstr);
					JCLReplace(workstr, JCLGetString(classtr), "");
					fprintf(outStream, "\t\"%s;\" TAG(\"%s\")\n", JCLGetString(workstr), JCLGetString(pType->mipTag));
				}
			}
		}
	}

	DELETE(classtr);
	DELETE(workstr);
	return err;
}

#else	// #if JIL_USE_BINDING_CODEGEN

JILError JCLCreateBindingCode(JCLState* _this, JCLClass* pClass)
{
	return JCL_No_Error;
}

#endif	// #if JIL_USE_BINDING_CODEGEN
