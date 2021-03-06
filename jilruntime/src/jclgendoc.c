//------------------------------------------------------------------------------
// File: JCLGenDoc.c                                           (c) 2011 jewe.org
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
/// @file jclgendoc.c
/// Generate HTML documentation for all classes, methods and functions currently
/// known by the compiler, using the code annotations ("tags") as the basis.
/// A tag in source code looks like this:
/// <pre>
/// class Test {       ["This is a tag for the class Test"]
///     method Test();     ["This is a tag for the constructor."]
/// }
/// function int main(int); ["This is a tag for function main()"]
/// </pre>
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclstate.h"
#include "jiltable.h"
#include "jiltypelist.h"
#include "jilcallntl.h"

/*
 * ideas for the future:
 *	- include native type information in class overview (e.g. author name, time stamp, etc...)
 */

#if JIL_USE_HTML_CODEGEN && !JIL_NO_FPRINTF && JIL_USE_LOCAL_FILESYS

//------------------------------------------------------------------------------
// CSS stylesheet definitions
//------------------------------------------------------------------------------

const JILChar* kCSSTemplate =
" table, td { border-collapse:collapse; border:1px solid #222; }"
" td { font-family:sans-serif; font-size:10pt; text-align:left; margin:0px; padding:4px 4px; }"
" pre { font-family:monospace; font-size:10pt; margin:0px; padding:0px; }"
" p { font-family:sans-serif; font-size:12pt; }"
" td p { font-family:sans-serif; font-size:10pt; }"
" ul { font-family:sans-serif; font-size:10pt; }"
" a:link { text-decoration:none; color:#118; }"
" a:visited { text-decoration:none; color:#161; }"
" #content { margin:10px auto; width:800px; align:center; background:#fff; }"
" #column1 { width:300px; }"
" #table1 { width:800px; }"
" #light { background:#f4f4f4; }"
" #dark { background:#e4e4e4; }"
" #scroll { display:block; overflow:auto; width:790px; border:0; }"
" #footer { font-family:sans-serif; font-style:italic; font-size:8pt; text-align:center; }"
"\n";

static const JILChar* kIdentifierSpan = "0123456789:@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
static const JILChar* kUrlSpan = "0123456789.-_@?=:;/+%#$ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const JILChar* kIncludeSpan = "\r\n<";

//------------------------------------------------------------------------------
// Error handling macros
//------------------------------------------------------------------------------

#define ERROR_IF(COND,ERR,ARG,S)	do {if(COND){err = EmitError(_this,ARG,ERR); S;}} while(0)
#define ERROR(ERR,ARG,S)			do {err = EmitError(_this,ARG,ERR); S;} while(0)

//------------------------------------------------------------------------------
// external functions from JCLState
//------------------------------------------------------------------------------

JILError EmitError(JCLState* _this, const JCLString* pArg, JILError err);
JILLong FindClass(JCLState* _this, const JCLString* pName, JCLClass** ppClass);

//------------------------------------------------------------------------------
// internal helper functions
//------------------------------------------------------------------------------

typedef JILBool (*FnFilter)(JCLFunc*);
typedef JILBool (*ClFilter)(JCLClass*);

static JILBool HasTags(JCLClass* pClass);
static JILBool IsDocumentable(JCLClass* pClass);
static void GetFamilyAndTypeName(JCLState* _this, JCLClass* pClass, JCLString* pFamily, JCLString* pType, int flags);
static void WriteFunctionTable(JCLState* _this, JCLClass* pClass, FILE* pFile, FnFilter pFn, const JILChar* pText, int* pAnchor, JILTable* pDict);
static void WriteFunctionDesc(JCLState* _this, JCLClass* pClass, FILE* pFile, FnFilter pFn, int* pAnchor, JILTable* pDict);
static void WriteAliasTable(JCLState* _this, JCLClass* pClass, FILE* pFile, const JILChar* pText, int* pAnchor, JILTable* pDict);
static void ToDict(JILTable* pDict, JCLString* pHash, JCLString* pValue);
static JCLString* FromDict(JILTable* pDict, JCLString* pHash);
static void FunctionsToDict(JILTable* pDict, JCLClass* pClass, JCLString* pFileName, FnFilter pFn, int* pAnchor);
static void AutoLinkKeywords(JILTable* pDict, JCLString* workstr, JCLString* context);
static void AutoInsertVariables(JILTable* pDict, JCLString* workstr);
static void ScanTag(JILTable* pDict, JCLString* pTag);
static void SplitTag(JCLString* pTag, JCLString* pPart1, JCLString* pPart2);
static void WriteTypeTable(JCLState* _this, FILE* pFile, ClFilter pFn, const JILChar* pText, JILTable* pDict, JILLong startClass, JILLong endClass);
static void GetFileName(JCLString* result, const JCLClass* pClass);
static void WriteNativeDeclaration(JCLState* _this, JCLClass* pClass, FILE* pFile, const JILChar* pText, int* pAnchor, JILTable* pDict);
static void LoadTextInclude(JCLString* fileName, JCLString* text);
static void WrapIntoTag(JCLString* string, const JILChar* pTag);

//------------------------------------------------------------------------------
// filter functions
//------------------------------------------------------------------------------

static JILBool OnlyFunctions(JCLFunc* pFunc)	{ return !pFunc->miMethod; }
static JILBool OnlyCtors(JCLFunc* pFunc)		{ return pFunc->miCtor && !pFunc->miPrivate; }
static JILBool OnlyConvertors(JCLFunc* pFunc)	{ return pFunc->miConvertor; }
static JILBool OnlyMethods(JCLFunc* pFunc)		{ return pFunc->miMethod && !pFunc->miAccessor && !pFunc->miCtor && !pFunc->miConvertor; }
static JILBool OnlyProperties(JCLFunc* pFunc)	{ return pFunc->miAccessor; }

static JILBool OnlyClasses(JCLClass* pClass)	{ return pClass->miFamily == tf_class; }
static JILBool OnlyInterfaces(JCLClass* pClass)	{ return pClass->miFamily == tf_interface; }
static JILBool OnlyDelegates(JCLClass* pClass)	{ return pClass->miFamily == tf_delegate; }
static JILBool OnlyCofunctions(JCLClass* pClass){ return pClass->miFamily == tf_thread; }

//------------------------------------------------------------------------------
// JCLCreateClassDoc
//------------------------------------------------------------------------------
// Generate HTML document from all tags.

JILError JCLCreateClassDoc(JCLState* _this, JCLClass* pClass, JILTable* pDict, const JILChar* pPath)
{
	JILError err = JCL_No_Error;
	FILE* pFile = NULL;
	JCLString* workstr = NULL;
	JCLString* tagstr1 = NULL;
	JCLString* tagstr2 = NULL;
	JCLString* familyName = NULL;
	JCLString* typeName = NULL;
	int Anchor = 0;

	// Check if this is a documentable class
	if( !IsDocumentable(pClass) )
		goto exit;

	// create filename
	workstr = NEW(JCLString);
	tagstr1 = NEW(JCLString);
	tagstr2 = NEW(JCLString);
	JCLSetString(workstr, pPath);
	JCLAppend(workstr, "\\");
	GetFileName(workstr, pClass);
	JCLAppend(workstr, ".html");

	// open the file
	pFile = fopen(JCLGetString(workstr), "wt");
	ERROR_IF(pFile == NULL, JCL_ERR_Native_Code_Generator, workstr, goto exit);

	// create type family name
	familyName = NEW(JCLString);
	typeName = NEW(JCLString);
	GetFamilyAndTypeName(_this, pClass, familyName, typeName, kClearFirst|kFullDecl);

	// header, title, etc
	fprintf(pFile, "<!DOCTYPE html>\n");
	fprintf(pFile, "<html>\n<head>\n<title>JewelScript %s %s Documentation</title>\n", JCLGetString(familyName), JCLGetString(typeName));
	fprintf(pFile, "<style type=\"text/css\">\n<!--\n");
	fprintf(pFile, kCSSTemplate);
	fprintf(pFile, "// -->\n</style>\n");
	fprintf(pFile, "</head>\n<body>\n<div id='content'>\n");
	fprintf(pFile, "");
	fprintf(pFile, "<h1>%s %s</h1>\n", JCLGetString(familyName), JCLGetString(typeName));
	// write class documentation
	workstr->Copy(workstr, pClass->mipTag);
	SplitTag(workstr, tagstr1, tagstr2);
	AutoLinkKeywords(pDict, tagstr1, pClass->mipName);
	AutoLinkKeywords(pDict, tagstr2, pClass->mipName);
	WrapIntoTag(tagstr1, "p");
	WrapIntoTag(tagstr2, "p");
	fprintf(pFile, "%s\n", JCLGetString(tagstr1));
	if( JCLGetLength(tagstr2) )
		fprintf(pFile, "%s\n", JCLGetString(tagstr2));
	// inheritance
	if( pClass->miFamily == tf_class && (pClass->miBaseType || pClass->miHybridBase) )
	{
		JCLClass* pBase = GetClass(_this, pClass->miBaseType);
		JCLSetString(workstr, JCLGetString(pBase->mipName));
		if( pClass->miHybridBase )
		{
			JCLClass* pHyb = GetClass(_this, pClass->miHybridBase);
			JCLAppend(workstr, " hybrid ");
			JCLAppend(workstr, JCLGetString(pHyb->mipName));
		}
		if( pBase->miFamily == tf_interface )
			JCLSetString(tagstr1, "implements");
		else
			JCLSetString(tagstr1, "extends");
		AutoLinkKeywords(pDict, workstr, pClass->mipName);
		fprintf(pFile, "<h3>Inheritance</h3>\n");
		fprintf(pFile, "<table id='table1'><tr><td id='light'><pre>class %s %s %s</pre></p></td></tr></table>\n", JCLGetString(pClass->mipName), JCLGetString(tagstr1), JCLGetString(workstr));
	}
	// write function tables
	WriteFunctionTable(_this, pClass, pFile, OnlyFunctions, "Global Functions", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyCtors, "Constructors", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyConvertors, "Convertors", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyMethods, "Methods", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyProperties, "Properties", &Anchor, pDict);
	WriteAliasTable(_this, pClass, pFile, "Aliases", &Anchor, pDict);
	// write native type declaration string
	if( pClass->miNative )
	{
		WriteNativeDeclaration(_this, pClass, pFile, "Type Declaration", &Anchor, pDict);
	}
	// write full function descriptions
	if( Anchor )
	{
		Anchor = 0;
		fprintf(pFile, "<h3>Reference</h3>\n");
		WriteFunctionDesc(_this, pClass, pFile, OnlyFunctions, &Anchor, pDict);
		WriteFunctionDesc(_this, pClass, pFile, OnlyCtors, &Anchor, pDict);
		WriteFunctionDesc(_this, pClass, pFile, OnlyConvertors, &Anchor, pDict);
		WriteFunctionDesc(_this, pClass, pFile, OnlyMethods, &Anchor, pDict);
		WriteFunctionDesc(_this, pClass, pFile, OnlyProperties, &Anchor, pDict);
	}
	// end of file
	JCLSetString(workstr, "{application} {appversion}");
	AutoLinkKeywords(pDict, workstr, NULL);
	fprintf(pFile, "<div id='footer'><a href='index.html'>%s class documentation</a>", JCLGetString(workstr));
	JCLFormatTime(workstr, "%Y-%m-%d %H:%M:%S", time(NULL));
	fprintf(pFile, " generated on %s</div>\n", JCLGetString(workstr));
	fprintf(pFile, "</div>\n</body>\n</html>");

exit:
	if( pFile )
	{
		fclose(pFile);
	}
	DELETE(workstr);
	DELETE(tagstr1);
	DELETE(tagstr2);
	DELETE(familyName);
	DELETE(typeName);
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// JCLAnalyzeClass
//------------------------------------------------------------------------------
// Analyze the class and fill the dictionary with keywords.

JILError JCLAnalyzeClass(JCLState* _this, JCLClass* pClass, JILTable* pDict)
{
	JCLString* htmlfile = NULL;
	JCLString* shortname = NULL;
	int i;
	int Anchor =0;

	if( !IsDocumentable(pClass) )
		goto exit;

	// create the filename
	shortname = NEW(JCLString);
	htmlfile = NEW(JCLString);
	GetFileName(htmlfile, pClass);
	JCLAppend(htmlfile, ".html");

	// scan the class tag for special tokens
	ScanTag(pDict, pClass->mipTag);

	// add the type name to the dictionary
	RemoveParentNamespace(shortname, pClass->mipName);
	ToDict(pDict, pClass->mipName, htmlfile);
	ToDict(pDict, shortname, htmlfile);

	FunctionsToDict(pDict, pClass, htmlfile, OnlyFunctions, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyCtors, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyConvertors, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyMethods, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyProperties, &Anchor);

	// add aliases
	for( i = 0; i < pClass->mipAlias->Count(pClass->mipAlias); i++ )
	{
		JCLString* pAlias = pClass->mipAlias->Get(pClass->mipAlias, i);
		RemoveParentNamespace(shortname, pAlias);
		ToDict(pDict, pAlias, htmlfile);
		ToDict(pDict, shortname, htmlfile);
	}

exit:
	DELETE(htmlfile);
	DELETE(shortname);
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// JCLCreateClassIndex
//------------------------------------------------------------------------------
// Generate HTML index document from all documented classes.

JILError JCLCreateClassIndex(JCLState* _this, JILTable* pDict, const JILChar* pPath, JILLong startClass, JILLong endClass)
{
	JILError err = JCL_No_Error;
	FILE* pFile = NULL;
	JCLString* workstr = NEW(JCLString);

	// open the file
	JCLSetString(workstr, pPath);
	JCLAppend(workstr, "\\index.html");
	pFile = fopen(JCLGetString(workstr), "wt");
	ERROR_IF(pFile == NULL, JCL_ERR_Native_Code_Generator, workstr, goto exit);

	// header, title, etc
	JCLSetString(workstr, "{application} {appversion}");
	AutoLinkKeywords(pDict, workstr, NULL);
	fprintf(pFile, "<!DOCTYPE html>\n");
	fprintf(pFile, "<html>\n<head>\n<title>%s Class Documentation</title>\n", JCLGetString(workstr));
	fprintf(pFile, "<style type=\"text/css\">\n<!--\n");
	fprintf(pFile, kCSSTemplate);
	fprintf(pFile, "// -->\n</style>\n");
	fprintf(pFile, "</head>\n<body>\n<div id='content'>\n");
	fprintf(pFile, "");
	fprintf(pFile, "<h1>%s Class Documentation</h1>\n", JCLGetString(workstr));
	fprintf(pFile, "<p>These are the documented interfaces, classes and delegates for this application.</p>\n");
	// write type tables
	WriteTypeTable(_this, pFile, OnlyInterfaces, "Interfaces", pDict, startClass, endClass);
	WriteTypeTable(_this, pFile, OnlyClasses, "Classes", pDict, startClass, endClass);
	WriteTypeTable(_this, pFile, OnlyDelegates, "Delegates", pDict, startClass, endClass);
	WriteTypeTable(_this, pFile, OnlyCofunctions, "Co-Functions", pDict, startClass, endClass);
	// end of file
	fprintf(pFile, "<p><br /></p>\n");
	fprintf(pFile, "<div id='footer'>%s class documentation", JCLGetString(workstr));
	JCLFormatTime(workstr, "%Y-%m-%d %H:%M:%S", time(NULL));
	fprintf(pFile, " generated on %s</div>\n", JCLGetString(workstr));
	fprintf(pFile, "</div>\n</body>\n</html>");

exit:
	if( pFile )
	{
		fclose(pFile);
	}
	DELETE(workstr);
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// JCLAnalyzeParameters
//------------------------------------------------------------------------------
// Parses the parameter string passed to the JCLGenerateDocs() function and
// fills the hash table with its definitions.

JILError JCLAnalyzeParameters(JCLState* _this, const JILChar* pParams, JILTable* pDict)
{
	JILError err = JCL_No_Error;
	JCLString* pValue = NEW(JCLString);
	JCLString* pName = NEW(JCLString);
	JCLString* pHash = NEW(JCLString);
	JCLString* pOptions = NEW(JCLString);

	if( pParams != NULL && strlen(pParams) > 0 )
	{
		JCLSetString(pOptions, pParams);
		while( !JCLAtEnd(pOptions) )
		{
			JCLSpanExcluding(pOptions, "=", pName);
			JCLTrim(pName);
			if( JCLAtEnd(pOptions) )
			{
				err = JCL_ERR_Native_Code_Generator;
				goto exit;
			}
			if( !JCLGetLength(pName) || !JCLContainsOnly(pName, kIdentifierSpan) )
			{
				EmitWarning(_this, JCL_WARN_Invalid_Docs_Parameter, 1, pName);
				JCLSpanExcluding(pOptions, ",;", pValue);
			}
			else
			{
				JCLFormat(pHash, "{%s}", JCLGetString(pName));
				JCLSeekForward(pOptions, 1);
				JCLSpanExcluding(pOptions, ",;", pValue);
				JCLTrim(pValue);
				if( JCLEquals(pName, "@ignore") )
				{
					JCLClass* pClass;
					if( FindClass(_this, pValue, &pClass) )
						JCLSetString(pClass->mipTag, "@ignore");
				}
				else
				{
					JCLString* newValue = NEW(JCLString); // not a leak! must put new instance into dictionary!
					newValue->Copy(newValue, pValue);
					JILTable_SetItem(pDict, JCLGetString(pHash), newValue);
				}
			}
			JCLSeekForward(pOptions, 1);
		}
	}

exit:
	DELETE(pName);
	DELETE(pHash);
	DELETE(pOptions);
	DELETE(pValue);
	return err;
}

//------------------------------------------------------------------------------
// HasTags
//------------------------------------------------------------------------------
// Checks if any function in this class has a tag.

static JILBool HasTags(JCLClass* pClass)
{
	int i;
	if( JCLFindString(pClass->mipTag, "@ignore", 0) >= 0 )
		return JILFalse;
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if (JCLGetLength(pFunc->mipTag) > 0)
			return JILTrue;
	}
	if( JCLGetLength(pClass->mipTag) )
		return JILTrue;
	return JILFalse;
}

//------------------------------------------------------------------------------
// SortFunctions
//------------------------------------------------------------------------------
// returns a sorted list of functions

static JCLFunc** SortFunctions(JCLClass* pClass)
{
	JCLFunc** ppRes;
	JCLFunc* swap;
	JILLong i, j, l;

	l = pClass->mipFuncs->Count(pClass->mipFuncs);
	ppRes = (JCLFunc**)malloc(l * sizeof(JCLFunc*));
	for( i = 0; i < l; i++ )
	{
		ppRes[i] = pClass->mipFuncs->Get(pClass->mipFuncs, i);
	}
    for( i = 1; i < l; i++ )
    {
        for( j = i; j >= 1; j-- )
        {
			if( strcmp(JCLGetString(ppRes[j - 1]->mipName), JCLGetString(ppRes[j]->mipName)) > 0 )
            {
				swap = ppRes[j - 1];
				ppRes[j - 1] = ppRes[j];
				ppRes[j] = swap;
            }
            else
            {
                break;
            }
        }
    }
	return ppRes;
}

//------------------------------------------------------------------------------
// WriteFunctionTable
//------------------------------------------------------------------------------
// Writes a filtered function / method table

static void WriteFunctionTable(JCLState* _this, JCLClass* pClass, FILE* pFile, FnFilter pFn, const JILChar* pText, int* pAnchor, JILTable* pDict)
{
	JCLString* workstr = NEW(JCLString);
	JCLString* tagstr = NEW(JCLString);
	JCLString* dummystr = NEW(JCLString);
	JCLFunc** sortedFuncs;
	int i;

	// first check if there is anything to document in this section
	JILBool skip = JILTrue;
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if (pFn(pFunc))
		{
			skip = JILFalse;
			break;
		}
	}
	if (!skip)
	{
		sortedFuncs = SortFunctions(pClass);
		fprintf(pFile, "<h3>%s</h3>\n", pText);
		fprintf(pFile, "<table id='table1'>\n<tbody>\n");
		for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
		{
			JCLFunc* pFunc = sortedFuncs[i];
			if (pFn(pFunc))
			{
				pFunc->ToString(pFunc, _this, workstr, kClearFirst|kNoClassName);
				RemoveClassNamespace(workstr, pClass);
				AutoLinkKeywords(pDict, workstr, pClass->mipName);
				SplitTag(pFunc->mipTag, tagstr, dummystr);
				AutoLinkKeywords(pDict, tagstr, pClass->mipName);
				fprintf(pFile, "<tr id='%s'><td id='column1'>%s</td><td>%s</td></tr>\n", (*pAnchor & 1)?"dark":"light", JCLGetString(workstr), JCLGetString(tagstr));
				*pAnchor = *pAnchor + 1;
			}
		}
		fprintf(pFile, "</tbody>\n</table>\n");
		free(sortedFuncs);
	}
	DELETE(workstr);
	DELETE(tagstr);
	DELETE(dummystr);
}

//------------------------------------------------------------------------------
// DescribeFunction
//------------------------------------------------------------------------------
// Describes a single function

static void DescribeFunction(JCLState* _this, JCLClass* pClass, FILE* pFile, JCLFunc* pFunc, JILTable* pDict)
{
	JCLString* workstr = NEW(JCLString);
	JCLString* tagstr1 = NEW(JCLString);
	JCLString* tagstr2 = NEW(JCLString);

	fprintf(pFile, "<table id='table1' cols='1'>\n<tbody>\n");
	pFunc->ToString(pFunc, _this, workstr, kClearFirst|kFullDecl|kIdentNames|kNoClassName);
	RemoveClassNamespace(workstr, pClass);
	SplitTag(pFunc->mipTag, tagstr1, tagstr2);
	AutoLinkKeywords(pDict, tagstr1, pClass->mipName);
	AutoLinkKeywords(pDict, tagstr2, pClass->mipName);
	WrapIntoTag(tagstr1, "p");
	WrapIntoTag(tagstr2, "p");
	fprintf(pFile, "<tr id='dark'><td><code>%s</code></td></tr><tr id='light'><td>%s", JCLGetString(workstr), JCLGetString(tagstr1));
	if( JCLGetLength(tagstr2) )
	{
		fprintf(pFile, "%s", JCLGetString(tagstr2));
	}
	fprintf(pFile, "</td></tr>\n");
	fprintf(pFile, "</tbody>\n</table>\n");
	fprintf(pFile, "<p><br /></p>\n");

	DELETE(workstr);
	DELETE(tagstr1);
	DELETE(tagstr2);
}

//------------------------------------------------------------------------------
// WriteFunctionDesc
//------------------------------------------------------------------------------
// Writes full function descriptions

static void WriteFunctionDesc(JCLState* _this, JCLClass* pClass, FILE* pFile, FnFilter pFn, int* pAnchor, JILTable* pDict)
{
	int i;
	JCLFunc** sortedFuncs;

	// first check if there is anything to document in this section
	JILBool skip = JILTrue;
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if (pFn(pFunc))
		{
			skip = JILFalse;
			break;
		}
	}
	if (!skip)
	{
		sortedFuncs = SortFunctions(pClass);
		for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
		{
			JCLFunc* pFunc = sortedFuncs[i];
			if (pFn(pFunc))
			{
				fprintf(pFile, "<a name=\"A%04d\"></a>\n", *pAnchor);
				DescribeFunction(_this, pClass, pFile, pFunc, pDict);
				*pAnchor = *pAnchor + 1;
			}
		}
		free(sortedFuncs);
	}
}

//------------------------------------------------------------------------------
// WriteAliasTable
//------------------------------------------------------------------------------
// Writes an alias table

static void WriteAliasTable(JCLState* _this, JCLClass* pClass, FILE* pFile, const JILChar* pText, int* pAnchor, JILTable* pDict)
{
	JCLString* workstr = NEW(JCLString);
	JCLString* tagstr = NEW(JCLString);
	JCLString* tmpstr = NEW(JCLString);
	int i;

	// first check if there is anything to document in this section
	if( !pClass->mipAlias->Count(pClass->mipAlias) )
		goto exit;

	fprintf(pFile, "<h3>%s</h3>\n", pText);
	fprintf(pFile, "<table id='table1'>\n<tbody>\n");
	for( i = 0; i < pClass->mipAlias->Count(pClass->mipAlias); i++ )
	{
		JCLString* pAlias = pClass->mipAlias->Get(pClass->mipAlias, i);
		GetFamilyAndTypeName(_this, pClass, tagstr, workstr, kClearFirst|kFullDecl|kIdentNames);
		if( pClass->miFamily == tf_delegate || pClass->miFamily == tf_thread )
			JCLFormat(tmpstr, "(%s)", JCLGetString(pClass->mipName));
		else
			tmpstr->Copy(tmpstr, pClass->mipName);
		JCLReplace(workstr, JCLGetString(tmpstr), JCLGetString(pAlias));
		fprintf(pFile, "<tr id='%s'><td>%s %s</td></tr>\n", (i & 1)?"dark":"light", JCLGetString(tagstr), JCLGetString(workstr));
	}
	fprintf(pFile, "</tbody>\n</table>\n");
	fprintf(pFile, "<p><br /></p>\n");

exit:
	DELETE(workstr);
	DELETE(tagstr);
	DELETE(tmpstr);
}

//------------------------------------------------------------------------------
// GetFamilyAndTypeName
//------------------------------------------------------------------------------

static void GetFamilyAndTypeName(JCLState* _this, JCLClass* pClass, JCLString* familyName, JCLString* typeName, int flags)
{
	if (pClass->miFamily == tf_class)
	{
		JCLSetString(familyName, (pClass->miModifier & kModeStrict) ? "strict " : "");
		JCLAppend(familyName, "class");
		JCLSetString(typeName, JCLGetString(pClass->mipName));
	}
	else if (pClass->miFamily == tf_interface)
	{
		JCLSetString(familyName, (pClass->miModifier & kModeStrict) ? "strict " : "");
		JCLAppend(familyName, (pClass->miModifier & kModeNativeInterface) ? "native interface" : "interface");
		JCLSetString(typeName, JCLGetString(pClass->mipName));
	}
	else if (pClass->miFamily == tf_thread)
	{
		JCLSetString(familyName, "cofunction");
		pClass->mipFuncType->ToString(pClass->mipFuncType, _this, pClass->mipName, typeName, (flags & ~kFullDecl), pClass->miParentType);
		RemoveClassNamespace(typeName, GetClass(_this, pClass->miParentType));
	}
	else if (pClass->miFamily == tf_delegate)
	{
		JCLSetString(familyName, "delegate");
		pClass->mipFuncType->ToString(pClass->mipFuncType, _this, pClass->mipName, typeName, (flags & ~kFullDecl), pClass->miParentType);
		RemoveClassNamespace(typeName, GetClass(_this, pClass->miParentType));
	}
}

//------------------------------------------------------------------------------
// ToDict
//------------------------------------------------------------------------------

static void ToDict(JILTable* pDict, JCLString* pHash, JCLString* pValue)
{
	JCLString* workstr = NEW(JCLString);
	workstr->Copy(workstr, pValue);
	JILTable_SetItem(pDict, JCLGetString(pHash), workstr);
}

//------------------------------------------------------------------------------
// FromDict
//------------------------------------------------------------------------------

static JCLString* FromDict(JILTable* pDict, JCLString* pHash)
{
	return (JCLString*)JILTable_GetItem(pDict, JCLGetString(pHash));
}

//------------------------------------------------------------------------------
// FunctionsToDict
//------------------------------------------------------------------------------

static void FunctionsToDict(JILTable* pDict, JCLClass* pClass, JCLString* pFileName, FnFilter pFn, int* pAnchor)
{
	JCLString* workstr = NEW(JCLString);
	JCLString* tagstr = NEW(JCLString);
	JCLFunc** sortedFuncs;
	int i;

	// first check if there is anything to document in this section
	JILBool skip = JILTrue;
	for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
	{
		JCLFunc* pFunc = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if (pFn(pFunc))
		{
			skip = JILFalse;
			break;
		}
	}
	if (!skip)
	{
		sortedFuncs = SortFunctions(pClass);
		for( i = 0; i < pClass->mipFuncs->Count(pClass->mipFuncs); i++ )
		{
			JCLFunc* pFunc = sortedFuncs[i];
			if (pFn(pFunc))
			{
				// add with full-qualified class name
				JCLSetString(workstr, JCLGetString(pClass->mipName));
				JCLAppend(workstr, "::");
				JCLAppend(workstr, JCLGetString(pFunc->mipName));
				JCLFormat(tagstr, "%s#A%04d", JCLGetString(pFileName), *pAnchor);
				ToDict(pDict, workstr, tagstr);
				// add with only class name
				JCLSetString(workstr, JCLGetString(pClass->mipName));
				RemoveParentNamespace(workstr, workstr);
				JCLAppend(workstr, "::");
				JCLAppend(workstr, JCLGetString(pFunc->mipName));
				ToDict(pDict, workstr, tagstr);
				// scan the function tag for special tokens
				ScanTag(pDict, pFunc->mipTag);
				*pAnchor = *pAnchor + 1;
			}
		}
		free(sortedFuncs);
	}
	DELETE(workstr);
	DELETE(tagstr);
}

//------------------------------------------------------------------------------
// AutoLinkKeywords
//------------------------------------------------------------------------------

static void AutoLinkKeywords(JILTable* pDict, JCLString* workstr, JCLString* context)
{
	int len;
	JCLString* oldstr = NEW(JCLString);
	JCLString* tempstr = NEW(JCLString);
	JCLString* tempstr2 = NEW(JCLString);
	oldstr->Copy(oldstr, workstr);
	JCLClear(workstr);
	JCLSetLocator(oldstr, 0);

	while( JCLGetLocator(oldstr) < JCLGetLength(oldstr) )
	{
		len = JCLSpanIncluding(oldstr, kIdentifierSpan, tempstr);
		if( len )
		{
			JCLString* match = NULL;
			// skip @define directive
			if( JCLEquals(tempstr, "@define") )
			{
				JILLong pos = JCLGetLocator(oldstr);
				JCLSpanIncluding(oldstr, " \t", tempstr2);
				len = JCLSpanIncluding(oldstr, kIdentifierSpan, tempstr2);
				if( len )
				{
					JCLSpanIncluding(oldstr, " \t", tempstr2);
					len = JCLSpanBetween(oldstr, '{', '}', tempstr2);
					if( len >= 0 )
					{
						JCLSpanIncluding(oldstr, " \t", tempstr2);
						continue;
					}
				}
				JCLSetLocator(oldstr, pos);
			}
			else if( JCLEquals(tempstr, "@include") )
			{
				JCLString* text;
				JCLSpanIncluding(oldstr, " \t", tempstr2);
				len = JCLSpanExcluding(oldstr, kIncludeSpan, tempstr2);
				if( len == 0 )
					continue;
				// expand include argument
				AutoInsertVariables(pDict, tempstr2);
				// load text
				text = NEW(JCLString);
				LoadTextInclude(tempstr2, text);
				// expand include text
				AutoLinkKeywords(pDict, text, context);
				// add to workstr
				JCLAppend(workstr, JCLGetString(text));
				DELETE(text);
				continue;
			}
			if( context )
			{
				JCLFormat(tempstr2, "%s::%s", JCLGetString(context), JCLGetString(tempstr));
				match = FromDict(pDict, tempstr2);
				if( match )
				{
					JCLFormat(tempstr2, "<a href=\"%s\">%s</a>", JCLGetString(match), JCLGetString(tempstr));
					JCLAppend(workstr, JCLGetString(tempstr2));
				}
			}
			if( match == NULL )
			{
				match = FromDict(pDict, tempstr);
				if( match )
				{
					JCLFormat(tempstr2, "<a href=\"%s\">%s</a>", JCLGetString(match), JCLGetString(tempstr));
					JCLAppend(workstr, JCLGetString(tempstr2));
				}
			}
			if( match == NULL )
			{
				JCLSetString(tempstr2, "http:");
				if( JCLCompareNoCase(tempstr, tempstr2) )
				{
					JCLSpanIncluding(oldstr, kUrlSpan, tempstr2);
					JCLAppend(tempstr, JCLGetString(tempstr2));
					JCLFormat(tempstr2, "<a href=\"%s\">%s</a>", JCLGetString(tempstr), JCLGetString(tempstr));
					JCLAppend(workstr, JCLGetString(tempstr2));
				}
				else
				{
					JCLAppend(workstr, JCLGetString(tempstr));
				}
			}
		}
		else
		{
			JCLSpanExcluding(oldstr, kIdentifierSpan, tempstr);
			JCLAppend(workstr, JCLGetString(tempstr));
		}
	}
	DELETE(oldstr);
	DELETE(tempstr);
	DELETE(tempstr2);

	AutoInsertVariables(pDict, workstr);
}

//------------------------------------------------------------------------------
// AutoInsertVariables
//------------------------------------------------------------------------------

static void AutoInsertVariables(JILTable* pDict, JCLString* workstr)
{
	JILLong len;
	JCLString* oldstr = NEW(JCLString);
	JCLString* tempstr = NEW(JCLString);
	JCLString* tempstr2 = NEW(JCLString);
	oldstr->Copy(oldstr, workstr);
	JCLClear(workstr);
	JCLSetLocator(oldstr, 0);

	while( JCLGetLocator(oldstr) < JCLGetLength(oldstr) )
	{
		if( JCLBeginsWith(oldstr, "{") )
		{
			len = JCLSpanBetween(oldstr, '{', '}', tempstr);
			if( len > 0 )
			{
				JCLString* match;
				JCLFormat(tempstr2, "{%s}", JCLGetString(tempstr));
				match = FromDict(pDict, tempstr2);
				if( match )
				{
					JCLAppend(workstr, JCLGetString(match));
					continue;
				}
			}
		}
		JCLSpanExcluding(oldstr, "{", tempstr);
		JCLAppend(workstr, JCLGetString(tempstr));
	}

	DELETE(oldstr);
	DELETE(tempstr);
	DELETE(tempstr2);
}

//------------------------------------------------------------------------------
// ScanTag
//------------------------------------------------------------------------------

static void ScanTag(JILTable* pDict, JCLString* pTag)
{
	int len;
	JCLString* oldstr = NEW(JCLString);
	JCLString* tempstr = NEW(JCLString);
	JCLString* tempstr2 = NEW(JCLString);
	JCLString* newstr = NEW(JCLString);
	oldstr->Copy(oldstr, pTag);
	JCLSetLocator(oldstr, 0);

	while( JCLGetLocator(oldstr) < JCLGetLength(oldstr) )
	{
		len = JCLSpanIncluding(oldstr, kIdentifierSpan, tempstr);
		if( len )
		{
			if( JCLEquals(tempstr, "@define") )
			{
				JILLong pos = JCLGetLocator(oldstr);
				JCLSpanIncluding(oldstr, " \t", tempstr2);
				len = JCLSpanIncluding(oldstr, kIdentifierSpan, tempstr);
				if( len )
				{
					JCLSpanIncluding(oldstr, " \t", tempstr2);
					len = JCLSpanBetween(oldstr, '{', '}', newstr);
					if( len >= 0 )
					{
						JCLFormat(tempstr2, "{%s}", JCLGetString(tempstr));
						ToDict(pDict, tempstr2, newstr);
						JCLSpanIncluding(oldstr, " \t", tempstr2);
						continue;
					}
				}
				JCLSetLocator(oldstr, pos);
			}
		}
		else
		{
			JCLSpanExcluding(oldstr, kIdentifierSpan, tempstr);
		}
	}
	DELETE(oldstr);
	DELETE(tempstr);
	DELETE(tempstr2);
	DELETE(newstr);
}

//------------------------------------------------------------------------------
// SplitTag
//------------------------------------------------------------------------------

static void SplitTag(JCLString* pTag, JCLString* pPart1, JCLString* pPart2)
{
	JILLong pos;
	JILLong c;
	JCLClear(pPart1);
	JCLClear(pPart2);
	for( pos = 0; pos < JCLGetLength(pTag); )
	{
		pos = JCLFindChar(pTag, '.', pos);
		if( pos < 0 )
		{
			JCLSetString(pPart1, JCLGetString(pTag));
			return;
		}
		c = JCLGetChar(pTag, pos + 1);
		if( c == 0 || c == 32 )
		{
			JCLSubString(pPart1, pTag, 0, pos + 1);
			JCLSubString(pPart2, pTag, pos + 1, JCLGetLength(pTag) - pos - 1);
			JCLTrim(pPart2);
			return;
		}
		pos = pos + 1;
	}
}

//------------------------------------------------------------------------------
// IsDocumentable
//------------------------------------------------------------------------------
// Check if this is a documentable class

static JILBool IsDocumentable(JCLClass* pClass)
{
	if (pClass->miFamily == tf_class || pClass->miFamily == tf_interface)
	{
		if (pClass->miHasBody)
			return HasTags(pClass);
	}
	else if (pClass->miFamily == tf_thread || pClass->miFamily == tf_delegate)
	{
		return HasTags(pClass);
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// SortClasses
//------------------------------------------------------------------------------
// returns a sorted list of classes / types

static JCLClass** SortClasses(JCLState* _this, JILLong startClass, JILLong endClass)
{
	JCLClass** ppRes;
	JCLClass* swap;
	JILLong i, j, l;

	l = endClass - startClass;
	ppRes = (JCLClass**)malloc(l * sizeof(JCLClass*));
	for( i = 0; i < l; i++ )
	{
		ppRes[i] = GetClass(_this, startClass + i);
	}
    for( i = 1; i < l; i++ )
    {
        for( j = i; j >= 1; j-- )
        {
			if( strcmp(JCLGetString(ppRes[j - 1]->mipName), JCLGetString(ppRes[j]->mipName)) > 0 )
            {
				swap = ppRes[j - 1];
				ppRes[j - 1] = ppRes[j];
				ppRes[j] = swap;
            }
            else
            {
                break;
            }
        }
    }
	return ppRes;
}

//------------------------------------------------------------------------------
// WriteTypeTable
//------------------------------------------------------------------------------
// Writes a filtered class/interface/delegate/cofunction table

static void WriteTypeTable(JCLState* _this, FILE* pFile, ClFilter pFn, const JILChar* pText, JILTable* pDict, JILLong startClass, JILLong endClass)
{
	JCLString* workstr = NEW(JCLString);
	JCLString* tagstr = NEW(JCLString);
	JCLString* dummystr = NEW(JCLString);
	int i, count = 0;
	JCLClass** sortedClass;

	// first check if there is anything to document in this section
	JILBool skip = JILTrue;
	for( i = startClass; i < endClass; i++ )
	{
		JCLClass* pClass = GetClass(_this, i);
		if (IsDocumentable(pClass) && pFn(pClass))
		{
			skip = JILFalse;
			break;
		}
	}
	if (!skip)
	{
		JILLong num = endClass - startClass;
		sortedClass = SortClasses(_this, startClass, endClass);
		fprintf(pFile, "<h3>%s</h3>\n", pText);
		fprintf(pFile, "<table id='table1'>\n<tbody>\n");
		for( i = 0; i < num; i++ )
		{
			JCLClass* pClass = sortedClass[i];
			if (IsDocumentable(pClass) && pFn(pClass))
			{
				workstr->Copy(workstr, pClass->mipName);
				AutoLinkKeywords(pDict, workstr, NULL);
				SplitTag(pClass->mipTag, tagstr, dummystr);
				AutoLinkKeywords(pDict, tagstr, pClass->mipName);
				fprintf(pFile, "<tr id='%s'><td id='column1'>%s</td><td>%s</td></tr>\n", (count & 1)?"dark":"light", JCLGetString(workstr), JCLGetString(tagstr));
				count++;
			}
		}
		fprintf(pFile, "</tbody>\n</table>\n");
		free(sortedClass);
	}
	DELETE(workstr);
	DELETE(tagstr);
	DELETE(dummystr);
}

//------------------------------------------------------------------------------
// GetFileName
//------------------------------------------------------------------------------
// Converts the class name to a file name and appends it to the given string.

static void GetFileName(JCLString* result, const JCLClass* pClass)
{
	JCLString* str = NEW(JCLString);
	str->Copy(str, pClass->mipName);
	JCLReplace(str, "::", "_");
	JCLAppend(result, JCLGetString(str));
	DELETE(str);
}

//------------------------------------------------------------------------------
// WriteNativeDeclaration
//------------------------------------------------------------------------------

static void WriteNativeDeclaration(JCLState* _this, JCLClass* pClass, FILE* pFile, const JILChar* pText, int* pAnchor, JILTable* pDict)
{
	JILError err;
	JILBool bFirst;
	JCLString* pToken;
	JCLString* pToken2;
	JILTypeListItem* pItem;
	const char* pDecl = NULL;
	const char* pBase = NULL;
	JCLDeclStruct declStruct = {0};

	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pItem = JILGetNativeType( _this->mipMachine, JCLGetString(pClass->mipName) );
	if( pItem )
	{
		// try to get class declaration
		declStruct.pString = NEW(JCLString);
		declStruct.pState = _this->mipMachine;
		err = CallNTLGetDeclString(pItem->typeProc, &declStruct, &pDecl);
		if( err )
			goto exit;
		// try to get base class / interface name
		CallNTLGetBaseName(pItem->typeProc, &pBase);
		// assemble string
		JCLSetString(pToken, "native class ");
		JCLAppend(pToken, JCLGetString(pClass->mipName));
		if( pBase )
		{
			JCLAppend(pToken, " implements ");
			JCLAppend(pToken, pBase);
		}
		JCLAppend(pToken, "\n{\n");
		JCLAppend(pToken, pDecl ? pDecl : JCLGetString(declStruct.pString));
		JCLAppend(pToken, "\n}\n");
		JCLEscapeXml(pToken, pToken);
		// kill any tabulators
		JCLCollapseSpaces(pToken);

		// insert line feeds into declaration
		bFirst = JILTrue;
		JCLSeekUntil(pToken, "{");
		while( !JCLAtEnd(pToken) )
		{
			JCLSeekForward(pToken, 1);
			JCLSeekWhile(pToken, " \t\n");
			if( JCLGetCurrentChar(pToken) == '[' )
			{
				JCLSetString(pToken2, bFirst ? "    " : " ");
				JCLInsert(pToken, pToken2, JCLGetLocator(pToken));
				JCLSeekForward(pToken, JCLGetLength(pToken2));
				JCLSpanBetween(pToken, '[', ']', pToken2);
			}
			bFirst = JILFalse;
			JCLSeekWhile(pToken, " \t\n");
			if( JCLGetCurrentChar(pToken) == '}' )
				break;
			JCLSetString(pToken2, "\n    ");
			JCLInsert(pToken, pToken2, JCLGetLocator(pToken));
			JCLSeekForward(pToken, JCLGetLength(pToken2));
			JCLSeekUntil(pToken, ";");
			if( JCLAtEnd(pToken) )
				break;
		}

		fprintf(pFile, "<h3>%s</h3>\n", pText);
		fprintf(pFile, "<table id='table1'>\n<tbody>\n");
		fprintf(pFile, "<tr id='light'>\n<td id='scroll'><pre>%s</pre></td></tr>\n", JCLGetString(pToken));
		fprintf(pFile, "</tbody>\n</table>\n");
	}

exit:
	DELETE( pToken );
	DELETE( pToken2 );
	DELETE( declStruct.pString );
}

//------------------------------------------------------------------------------
// LoadTextInclude
//------------------------------------------------------------------------------

static void LoadTextInclude(JCLString* fileName, JCLString* text)
{
	FILE* file;
	size_t length;

	JCLReplace(fileName, "/", JIL_PATHSEPARATORSTR);
	JCLClear(text);
	file = fopen(JCLGetString(fileName), "rb");
	if( file != NULL )
	{
		fseek(file, 0, SEEK_END);
		length = ftell(file);
		fseek(file, 0, SEEK_SET);
		JCLFill(text, ' ', length);
		length = fread(text->m_String, length, 1, file);
		fclose(file);
	}
}

//------------------------------------------------------------------------------
// WrapIntoTag
//------------------------------------------------------------------------------
// Wraps the given text into a tag, while avoiding nesting of the tag.
// Will only work for simple tags without attributes, actually just intended
// to avoid nesting <p> tags.

static void WrapIntoTag(JCLString* string, const JILChar* pTag)
{
	JILLong pos;
	JCLString* tag = NEW(JCLString);
	JCLFormat(tag, "<%s>", pTag);
	pos = JCLFindString(string, JCLGetString(tag), 0);
	if( pos < 0 )
	{
		tag->Copy(tag, string);
		JCLFormat(string, "<%s>%s</%s>", pTag, JCLGetString(tag), pTag);
	}
	else
	{
		JCLString* workstr = NEW(JCLString);
		JCLSubString(workstr, string, 0, pos); // get beginning of string up to <p>
		JCLAppend(tag, JCLGetString(workstr)); // add to result
		JCLFormat(workstr, "</%s>", pTag); // create end-tag
		JCLAppend(tag, JCLGetString(workstr)); // add to result
		JCLSubString(workstr, string, pos, JCLGetLength(string) - pos); // get rest of string
		JCLAppend(tag, JCLGetString(workstr)); // add to result
		string->Copy(string, tag); // copy result
		DELETE(workstr);
	}
	DELETE(tag);
}

#else	// JIL_USE_HTML_CODEGEN

JILError JCLCreateClassDoc(JCLState* pState, JCLClass* pClass)
{
	return JCL_ERR_Feature_Not_Available;
}

#endif	// JIL_USE_HTML_CODEGEN
