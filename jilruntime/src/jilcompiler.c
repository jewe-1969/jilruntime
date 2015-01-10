//------------------------------------------------------------------------------
// File: JILCompiler.c                                      (c) 2004 jewe.org
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
//	Contains the implementation of most compiler API functions.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

//------------------------------------------------------------------------------
// JIL includes
//------------------------------------------------------------------------------

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclstate.h"

#include "jilapi.h"
#include "jilcompilerapi.h"
#include "jiltools.h"
#include "jilopcodes.h"
#include "jilprogramming.h"
#include "jiltypelist.h"
#include "jilhandle.h"
#include "jilsymboltable.h"
#include "jildebug.h"
#include "jilcodelist.h"
#include "jilmachine.h"
#include "jiltable.h"
#include "jiltypeinfo.h"

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILError JILHandleRuntimeOptions(JILState*, const JILChar*, const JILChar*);
JILError JCLCreateBindingCode(JCLState* _this, JCLClass* pClass, const JILChar* pPath);
JILError JCLCreateClassDoc(JCLState* _this, JCLClass* pClass, JILTable* pDict, const JILChar* pPath);
JILError JCLAnalyzeClass(JCLState* _this, JCLClass* pClass, JILTable* pDict);
JILError JCLCreateClassIndex(JCLState* _this, JILTable* pDict, const JILChar* pPath, JILLong startClass, JILLong endClass);
JILError JCLAnalyzeParameters(JCLState* _this, const JILChar* pParams, JILTable* pDict);
JILError JCLLinkerMain(JCLState* _this);
JILError JCLPostLink(JCLState* _this);

//------------------------------------------------------------------------------
// code templates
//------------------------------------------------------------------------------

static const JILChar* kAnonFunction	=			"function %s %s(%s){%s}";
static const JILChar* kDefaultImports =			"import string; import array; import list; import iterator; import arraylist; import table; ";
static const JILChar* kDefaultAlias	=			"alias int bool; alias int char; ";
static const JILChar* kInterfaceException =
	"strict interface exception {"
	TAG("Strict interface for all classes that can be thrown as exceptions.")
	"    method int    getError   ();" TAG("Returns the error code for this exception. This can be any non-zero value. Implementing script classes can just return <code>typeof(this)</code> here.")
	"    method string getMessage ();" TAG("Returns the error message for this exception. Implementing classes should return an empty string rather than null when no message is available.")
	"} import runtime::exception;";

const JILLong kInterfaceExceptionGetError =		0;		// method index of the getError() method
const JILLong kInterfaceExceptionGetMessage =	1;		// method index of the getMessage() method
static const JILLong kFileBufferSize =			1024;	// used by JCLLoadAndCompile()

JILEXTERN const JILChar*	kNameGlobalClass;

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static JILError	JCLAddGlobals		(JILState*);
static JILError JCLSetWorkingDir	(const JILChar* pPath);

//------------------------------------------------------------------------------
// JCLBeginCompile
//------------------------------------------------------------------------------

static JILError JCLBeginCompile(JILState* pVM, const JILChar* pName, const JILChar* pText, const JILChar* pPath)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLState* _this;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	pName = (pName == NULL) ? "unnamed code fragment" : pName;
	pPath = (pPath == NULL) ? "" : pPath;
	if( !_this->miNumCompiles )
	{
		// status message
		const JILVersionInfo* pInfo;
		char vers[32];
		const char* buf1;
		pInfo = JILGetRuntimeVersion(_this->mipMachine);
		JILGetVersionString(pInfo->CompilerVersion, vers);
		buf1 = (pInfo->BuildFlags & kDebugBuild) ? "Debug" : "Release";
		JCLVerbosePrint(_this, "*** JewelScript compiler v%s [%s] ***\n", vers, buf1);
		_this->miTimestamp = (JILFloat) clock();
	}
	_this->miNumCompiles++;
	JCLVerbosePrint(_this, "Compiling '%s'\n", (strlen(pPath) > 0) ? pPath : pName);
	// create a new file object
	pFile = _this->mipFile = NEW(JCLFile);
	// open it
	err = pFile->Open(pFile, pName, pText, pPath, GetGlobalOptions(_this));
	if( !err )
	{
		err = cg_resume_intro(_this);
		if( err )
			goto exit;
		// -- begin compiling, pass 1: Precompile
		err = p_compile(_this, kPassPrecompile);
		if( err )
			goto exit;
		// -- continue compiling, pass 2: Compile
		err = p_compile(_this, kPassCompile);
		if( err )
			goto exit;
	}

exit:
	DELETE(pFile);
	_this->mipFile = NULL;
	return err;
}

//------------------------------------------------------------------------------
// JCLCompile
//------------------------------------------------------------------------------

JILError JCLCompile(JILState* pVM, const JILChar* pName, const JILChar* pText)
{
	return JCLBeginCompile(pVM, pName, pText, "");
}

//------------------------------------------------------------------------------
// JCLLoadAndCompile
//------------------------------------------------------------------------------

JILError JCLLoadAndCompile(JILState* pVM, const JILChar* pPath)
{
	JILError err;
	JCLString* pScript;

	if( !pPath || !pVM )
		return JIL_ERR_Generic_Error;

	pScript = NEW(JCLString);
	// load the file into the string
	if( JCLReadTextFile(pScript, pPath, pVM) < 0 )
	{
		err = JIL_ERR_File_Open;
		goto exit;
	}
	// compile the string
	err = JCLBeginCompile(pVM, "", JCLGetString(pScript), pPath);

exit:
	DELETE(pScript);
	return err;
}

//------------------------------------------------------------------------------
// JCLLink
//------------------------------------------------------------------------------

JILError JCLLink(JILState* pVM)
{
	JILError err = JCL_No_Error;
	JCLState* _this;
	JILLong bytes;
	JILFloat time;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	JCLVerbosePrint(_this, "Linking ...\n");
	JCLLinkerMain(_this);
	JCLPostLink(_this);
	FlushErrorsAndWarnings(_this);

	// output details
	bytes = _this->miOptSizeBefore;
	if( _this->miOptSavedInstr )
	{
		JCLVerbosePrint(_this, "Saved %d instructions in total.\n", _this->miOptSavedInstr);
		JCLVerbosePrint(_this, "Code size reduced from %d to %d bytes in total.\n", _this->miOptSizeBefore, _this->miOptSizeAfter);
		bytes = _this->miOptSizeAfter;
	}
	time = (((JILFloat) clock()) - _this->miTimestamp) / ((JILFloat)CLOCKS_PER_SEC);
	JCLVerbosePrint(_this, "%d bytes, %d files, %d errors, %d warnings, %g seconds.\n", bytes, _this->miNumCompiles, _this->miNumErrors, _this->miNumWarnings, time);

	return err;
}

//------------------------------------------------------------------------------
// JCLGetErrorText
//------------------------------------------------------------------------------

const char* JCLGetErrorText(JILState* pVM)
{
	const char* pStr = NULL;
	JCLState* _this = pVM->vmpCompiler;
	if( _this && _this->miLastError < _this->mipErrors->Count(_this->mipErrors) )
	{
		pStr = JCLGetString(_this->mipErrors->Get(_this->mipErrors, _this->miLastError++));
	}
	return pStr;
}

//------------------------------------------------------------------------------
// JCLCompileAndRun
//------------------------------------------------------------------------------

JILError JCLCompileAndRun(JILState* pVM, const JILChar* pText)
{
	JILError err = JCL_No_Error;
	JILHandle* pFunc = NULL;
	JILHandle* pResult = NULL;

	if( !pVM->vmpCompiler )
		return JIL_ERR_No_Compiler;

	// generate anonymous function
	pFunc = JCLAddAnonFunction(pVM, "", "", pText);
	if( !pFunc )
	{
		err = JIL_ERR_Generic_Error;
		goto error;
	}
	// must run the new init-code to init globals
	err = JILRunInitCode(pVM);
	if( err )
		goto error;
	// call the function
	pResult = JILCallFunction(pVM, pFunc, 0);
	err = NTLHandleToError(pVM, pResult);

error:
	if( pResult )
		JILRelease(pVM, pResult);
	if( pFunc )
		JILRelease(pVM, pFunc);
	return err;
}

//------------------------------------------------------------------------------
// JCLAddAnonFunction
//------------------------------------------------------------------------------

JILHandle* JCLAddAnonFunction(JILState* pVM, const JILChar* pRes, const JILChar* pArgs, const JILChar* pText)
{
	JILError err = JCL_No_Error;
	JCLString* pString;
	JCLString* pIdent;
	JILHandle* pFunc;

	if( !pVM->vmpCompiler )
		return NULL;
	if( !pRes )
		pRes = "";
	if( !pArgs )
		pArgs = "";
	if( !pText )
		pText = "";

	pString = NEW(JCLString);
	pIdent = NEW(JCLString);
	// create a random identifier
	JCLRandomIdentifier(pString, 16);
	// create function name "anonymous_" + "random identifier"
	JCLSetString(pIdent, "anonymous_");
	JCLAppend(pIdent, JCLGetString(pString));
	// create function code from template
	JCLFormat(pString, kAnonFunction, pRes, JCLGetString(pIdent), pArgs, pText);
	// now try to compile the function
	err = JCLCompile(pVM, "anonymous function", JCLGetString(pString));
	if( err )
		goto exit;
	// try to link
	err = JCLLink(pVM);
	if( err )
		goto exit;
	// get handle of our function
	pFunc = JILGetFunction(pVM, NULL, NULL, JCLGetString(pIdent));
	// done
	DELETE( pIdent );
	DELETE( pString );
	return pFunc;

exit:
	DELETE( pIdent );
	DELETE( pString );
	return NULL;
}

//------------------------------------------------------------------------------
// JCLSetFatalErrorHandler
//------------------------------------------------------------------------------

void JCLSetFatalErrorHandler(JILState* pVM, JCLFatalErrorHandler proc)
{
	JCLState* _this = pVM->vmpCompiler;
	if( _this && !_this->miFatalState )
		_this->miFatalErrorHandler = proc;
}

//------------------------------------------------------------------------------
// JCLSetGlobalOptions
//------------------------------------------------------------------------------

JILError JCLSetGlobalOptions(JILState* pVM, const JILChar* pOptionString)
{
	JILError err = JIL_No_Exception;
	JCLString* pStr;
	JCLString* pToken;
	JCLOption* pOptions;
	JCLState* _this;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	pStr = NEW(JCLString);
	pToken = NEW(JCLString);
	JCLSetString(pToken, pOptionString);
	pOptions = GetGlobalOptions(_this);
	while( !JCLAtEnd(pToken) )
	{
		// copy up to separator into pStr
		JCLSpanExcluding(pToken, ",;", pStr);
		// trim any spaces
		JCLTrim(pStr);
		// something left?
		if( JCLGetLength(pStr) )
		{
			// have option object parse it
			err = pOptions->ParseOption(pOptions, pStr, JILHandleRuntimeOptions, pVM);
			// handle warnings and errors
			if( err )
				goto exit;
		}
		// skip the separator(s)
		JCLSpanIncluding(pToken, ",;", pStr);
	}

exit:
	DELETE(pStr);
	DELETE(pToken);
	return err;
}

//------------------------------------------------------------------------------
// JCLGenerateBindings
//------------------------------------------------------------------------------

JILError JCLGenerateBindings(JILState* pVM, const JILChar* pPath)
{
	JILError err = JCL_No_Error;
	JILLong clas;
	JCLState* _this;
	JCLClass* pClass;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;
	JCLVerbosePrint(_this, "Generating C++ binding code...\n");
	// iterate over all classes
	for( clas = 0; clas < NumClasses(_this); clas++ )
	{
		pClass = GetClass(_this, clas);
		if( pClass->miFamily == tf_class && (pClass->miModifier & kModeNativeBinding) )
		{
			err = JCLCreateBindingCode(_this, pClass, pPath);
			if (err)
				goto exit;
		}
	}

exit:
	FlushErrorsAndWarnings(_this);
	return err;
}

//------------------------------------------------------------------------------
// JCLGenerateDocs
//------------------------------------------------------------------------------

static void JCLStringDestructor(JILUnknown* p) { DELETE(p); }

JILError JCLGenerateDocs(JILState* pVM, const JILChar* pPath, const JILChar* pParams)
{
#if JIL_USE_HTML_CODEGEN && !JIL_NO_FPRINTF && JIL_USE_LOCAL_FILESYS

	JILError err = JCL_No_Error;
	JILLong clas;
	JILLong startClass;
	JILLong endClass;
	JCLState* _this;
	JCLClass* pClass;
	JILTable* pTable = NULL;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;
	JCLVerbosePrint(_this, "Generating HTML documentation for all ");
	switch (pVM->vmDocGenMode)
	{
		case 0:
			startClass = kNumPredefTypes;
			endClass = NumClasses(_this);
			JCLVerbosePrint(_this, "user classes...\n");
			break;
		case 1:
			startClass = type_global;
			endClass = kNumPredefTypes;
			JCLVerbosePrint(_this, "built-in classes...\n");
			break;
		case 2:
			startClass = type_global;
			endClass = NumClasses(_this);
			JCLVerbosePrint(_this, "classes...\n");
			break;
	}
	pTable = JILTable_NewNativeManaged(pVM, JCLStringDestructor);
	// analyze classes
	for( clas = startClass; clas < endClass; clas++ )
	{
		pClass = GetClass(_this, clas);
		err = JCLAnalyzeClass(_this, pClass, pTable);
		if (err)
			goto exit;
	}
	// analyze optional parameters
	err = JCLAnalyzeParameters(_this, pParams, pTable);
	if (err)
		goto exit;
	// document all classes
	for( clas = startClass; clas < endClass; clas++ )
	{
		pClass = GetClass(_this, clas);
		err = JCLCreateClassDoc(_this, pClass, pTable, pPath);
		if (err)
			goto exit;
	}
	// create class index file
	err = JCLCreateClassIndex(_this, pTable, pPath, startClass, endClass);
	if (err)
		goto exit;

exit:
	JILTable_Delete(pTable);
	FlushErrorsAndWarnings(_this);
	return err;

#else

	return JCL_ERR_Feature_Not_Available;

#endif
}

//------------------------------------------------------------------------------
// JCLExportTypeInfo
//------------------------------------------------------------------------------

JILError JCLExportTypeInfo(JILState* pVM, const JILChar* pFilename)
{
	JILError err = JCL_No_Error;

#if !JIL_NO_FPRINTF && JIL_USE_LOCAL_FILESYS

	JILLong clas;
	JCLState* _this;
	JCLClass* pClass;
	JCLString* workstr = NULL;
	FILE* pFile = NULL;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;
	JCLVerbosePrint(_this, "Exporting type definitions to XML...\n");
	workstr = NEW(JCLString);

	// iterate over all classes
	for( clas = 0; clas < NumClasses(_this); clas++ )
	{
		pClass = GetClass(_this, clas);
		if( pClass->miFamily == tf_class
			|| pClass->miFamily == tf_interface
			|| pClass->miFamily == tf_thread
			|| pClass->miFamily == tf_delegate )
		{
			pClass->ToXml(pClass, _this, workstr);
		}
	}

	// write file
	pFile = fopen(pFilename, "wt");
	if( pFile )
	{
		fprintf(pFile, "<xml>\n");
		fputs(JCLGetString(workstr), pFile);
		fprintf(pFile, "</xml>\n");
		fclose(pFile);
	}

	FlushErrorsAndWarnings(_this);
	DELETE(workstr);

#endif

	return err;
}

//------------------------------------------------------------------------------
// JCLAddImportPath
//------------------------------------------------------------------------------

JILError JCLAddImportPath(JILState* pVM, const JILChar* pName, const JILChar* pPath)
{
	JILError err = JIL_No_Exception;
	JCLState* _this;
	JCLString* key;
	JCLString* data;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	// verify 'pName'
	if( JILCheckClassName(pVM, pName) )
		return JIL_ERR_Illegal_Argument;

	// make sure 'pName' is unique
	key = NEW(JCLString);
	data = NEW(JCLString);
	JCLSetString(key, pName);
	JCLSetString(data, pPath);
	if( Get_JCLCollection(_this->mipImportPaths, key) != NULL )
	{
		err = JIL_ERR_Illegal_Argument;
		goto exit;
	}
	Add_JCLCollection(_this->mipImportPaths, key, data);
	data = NULL;

exit:
	DELETE(key);
	DELETE(data);
	return err;
}

//------------------------------------------------------------------------------
// JCLForwardClass
//------------------------------------------------------------------------------

JILError JCLForwardClass(JILState* pState, const JILChar* pClassName)
{
	JILError err = JCL_No_Error;
	JCLState* _this;
	JILTypeInfo* pInfo;
	JILLong typeID;

	if( (_this = pState->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	if( JILFindTypeInfo(pState, pClassName, &pInfo) )
	{
		if( pInfo->family != tf_class )
			err = JIL_ERR_Illegal_Type_Name;
	}
	else
	{
		JILBool bNative = (JILGetNativeType(pState, pClassName) != NULL);
		err = JCLCreateType(pState->vmpCompiler, pClassName, type_global, tf_class, bNative, &typeID);
	}
	return err;
}

//------------------------------------------------------------------------------
// JCLImportClass
//------------------------------------------------------------------------------

JILError JCLImportClass(JILState* pState, const JILChar* pClassName)
{
	JILError err = JCL_No_Error;
	JCLState* _this;
	JCLString* className;
	JCLFile* pFile;

	if( (_this = pState->vmpCompiler) == NULL )
		return JIL_ERR_No_Compiler;

	className = NEW(JCLString);
	pFile = NEW(JCLFile);
	JCLSetString(className, pClassName);
	pFile->Open(pFile, "JCLImportClass", "", "", GetGlobalOptions(_this));
	_this->mipFile = pFile;
	_this->miPass = kPassPrecompile;
	err = p_import_class(_this, className);
	if( err )
		goto exit;
	_this->miPass = kPassCompile;
	err = p_import_class(_this, className);

exit:
	if( _this != NULL )
		_this->mipFile = NULL;
	DELETE(pFile);
	DELETE(className);
	return err;
}

//------------------------------------------------------------------------------
// JCLFreeCompiler
//------------------------------------------------------------------------------

JILError JCLFreeCompiler(JILState* pVM)
{
	JILError err = JIL_No_Exception;
	JILBool verbose;
	JCLState* _this;

	if( (_this = pVM->vmpCompiler) == NULL )
		return JIL_No_Exception;

	// destroy JCL
	verbose = GetOptions(_this)->miVerboseEnable;
	DELETE(_this);
	pVM->vmpCompiler = NULL;

	if( verbose )
	{
		JILMessageLog(pVM, "Compiler terminated.\n");
		JILMessageLog(pVM, "Compiler allocs:frees %d:%d\n", g_NewCalls, g_DeleteCalls);
	}
	return err;
}

//------------------------------------------------------------------------------
// JILInitializeCompiler
//------------------------------------------------------------------------------

JILError JILInitializeCompiler(JILState* pMachine, const JILChar* options)
{
	JILError err = JIL_No_Exception;
	JILLong type;
	JCLState* _this;

	// don't allocate with macro NEW before these 2 lines!
	g_NewCalls = 0;
	g_DeleteCalls = 0;

	// construct our main object
	_this = NEW(JCLState);

	// assign virtual machine
	pMachine->vmpCompiler = _this;
	_this->mipMachine = pMachine;
	_this->miClass = type_global; // we are at global scope...

	// set global options
	JCLSetGlobalOptions(pMachine, options);

	// create basic types
	err = JCLCreateType(_this, "null", 0, tf_undefined, JILFalse, &type);
	if( err )
		goto error;
	err = JCLCreateType(_this, "var", 0, tf_undefined, JILFalse, &type);
	if( err )
		goto error;
	err = JCLCreateType(_this, "int", 0, tf_integral, JILFalse, &type);
	if( err )
		goto error;
	err = JCLCreateType(_this, "float", 0, tf_integral, JILFalse, &type);
	if( err )
		goto error;
	err = JCLCreateType(_this, kNameGlobalClass, 0, tf_class, JILFalse, &type);
	if( err )
		goto error;

	// import built-in types
	err = JCLCompile(pMachine, NULL, kDefaultImports);
	if( err )
		goto error;

	// compile 'exception' interface declaration
	err = JCLCompile(pMachine, NULL, kInterfaceException);
	if( err )
		goto error;

	// generic delegate type, only used when calling JILGetFunction() API function
	err = JCLCreateType(_this, "__delegate", type_global, tf_delegate, JILFalse, &type);
	if( err )
		goto error;
	if( type != type_delegate )
	{
		FatalError(_this, __FILE__, __LINE__, "Type constants and runtime type-IDs are not in sync!", "JILInitializeCompiler");
		err = JIL_ERR_Initialize_Failed;
		goto error;
	}

	// create global "intro" code
	err = cg_begin_intro( _this );
	if( err )
		goto error;

	// import built-in aliases and other declarations
	err = JCLCompile(pMachine, NULL, kDefaultAlias);
	if( err )
		goto error;

	// done
	_this->miNumCompiles = 0;

error:
	return err;
}

//------------------------------------------------------------------------------
// JCLGetAbsolutePath
//------------------------------------------------------------------------------
// Uses the file input proc to prepend the current working directory to the given file name.

void JCLGetAbsolutePath(JCLState* _this, JCLString* pOut, const JCLString* instr)
{
	JILState* ps = _this->mipMachine;
	JILBool fail = JILTrue;
	JCLString* pIn = (JCLString*) instr;
	if( ps->vmFileInput != NULL
		&& JCLGetChar(pIn, 1) != ':'
		&& JCLGetChar(pIn, 0) != JIL_PATHSEPARATOR )
	{
		JCLFill(pOut, 32, 4096);
		if( ps->vmFileInput(ps, JILFileInputGetCwd, pOut->m_String, 4096, NULL) == JIL_No_Exception )
		{
			pOut->m_Length = strlen(pOut->m_String);
			if( pOut->m_Length )
			{
				if( JCLGetLastChar(pOut) != JIL_PATHSEPARATOR )
					JCLAppend(pOut, JIL_PATHSEPARATORSTR);
				JCLAppend(pOut, JCLGetString(pIn));
				fail = JILFalse;
			}
		}
	}
	if( fail )
	{
		JCLSetString(pOut, JCLGetString(pIn));
	}
}
