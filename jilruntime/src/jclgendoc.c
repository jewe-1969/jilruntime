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
" #footer { font-family:sans-serif; font-style:italic; font-size:8pt; text-align:center; }"
"\n";

static const JILChar* kIdentifierSpan = "0123456789:@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
static const JILChar* kUrlSpan = "0123456789.-_@?=:;/+%#$ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

//------------------------------------------------------------------------------
// Error handling macros
//------------------------------------------------------------------------------

#define ERROR_IF(COND,ERR,ARG,S)	do {if(COND){err = EmitError(_this,ARG,ERR); S;}} while(0)
#define ERROR(ERR,ARG,S)			do {err = EmitError(_this,ARG,ERR); S;} while(0)

//------------------------------------------------------------------------------
// external functions from JCLState
//------------------------------------------------------------------------------

JILError EmitError(JCLState* _this, const JCLString* pArg, JILError err);
JILLong FindClass( JCLState* _this, const JCLString* pName, JCLClass** ppClass );

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
static void ScanTag(JILTable* pDict, JCLString* pTag);
static void SplitTag(JCLString* pTag, JCLString* pPart1, JCLString* pPart2);
static void WriteTypeTable(JCLState* _this, FILE* pFile, ClFilter pFn, const JILChar* pText, JILTable* pDict, JILLong startClass, JILLong endClass);
static void GetFileName(JCLString* result, const JCLClass* pClass);

//------------------------------------------------------------------------------
// filter functions
//------------------------------------------------------------------------------

static JILBool OnlyFunctions(JCLFunc* pFunc)	{ return !pFunc->miMethod; }
static JILBool OnlyCtors(JCLFunc* pFunc)		{ return pFunc->miCtor; }
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
	fprintf(pFile, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	fprintf(pFile, "<html>\n<head>\n<title>JewelScript %s %s Documentation</title>\n", JCLGetString(familyName), JCLGetString(typeName));
	fprintf(pFile, "<style type=\"text/css\">\n<!--\n");
	fprintf(pFile, kCSSTemplate);
	fprintf(pFile, "// -->\n</style>\n");
	fprintf(pFile, "</head>\n<body>\n<div id='content'>\n");
	fprintf(pFile, "");
	fprintf(pFile, "<h1>%s %s</h1>\n", JCLGetString(familyName), JCLGetString(typeName));
	// write class documentation
	workstr->Copy(workstr, pClass->mipTag);
	AutoLinkKeywords(pDict, workstr, pClass->mipName);
	SplitTag(workstr, tagstr1, tagstr2);
	fprintf(pFile, "<p>%s</p>\n", JCLGetString(tagstr1));
	if( JCLGetLength(tagstr2) )
		fprintf(pFile, "<p>%s</p>\n", JCLGetString(tagstr2));
	// inheritance
	if( pClass->miFamily == tf_class && (pClass->miBaseType || pClass->miHybridBase) )
	{
		JCLClass* pInt = GetClass(_this, pClass->miBaseType);
		JCLSetString(workstr, JCLGetString(pInt->mipName));
		if( pClass->miHybridBase )
		{
			JCLClass* pHyb = GetClass(_this, pClass->miHybridBase);
			JCLAppend(workstr, " hybrid ");
			JCLAppend(workstr, JCLGetString(pHyb->mipName));
		}
		AutoLinkKeywords(pDict, workstr, pClass->mipName);
		fprintf(pFile, "<h3>Inheritance</h3>\n");
		fprintf(pFile, "<table id='table1'><tr><td id='light'><pre>class %s : %s</pre></p></td></tr></table>\n", JCLGetString(pClass->mipName), JCLGetString(workstr));
	}
	// write function tables
	WriteFunctionTable(_this, pClass, pFile, OnlyFunctions, "Global Functions", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyCtors, "Constructors", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyConvertors, "Convertors", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyMethods, "Methods", &Anchor, pDict);
	WriteFunctionTable(_this, pClass, pFile, OnlyProperties, "Properties", &Anchor, pDict);
	WriteAliasTable(_this, pClass, pFile, "Aliases", &Anchor, pDict);
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
	JCLSetString(workstr, "{application}");
	AutoLinkKeywords(pDict, workstr, NULL);
	fprintf(pFile, "<div id='footer'><a href='index.html'>%s class documentation</a>", JCLGetString(workstr));
	JCLFormatTime(workstr, "%c", time(NULL));
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
	int i;
	int Anchor =0;

	if( !IsDocumentable(pClass) )
		goto exit;

	// create the filename
	htmlfile = NEW(JCLString);
	GetFileName(htmlfile, pClass);
	JCLAppend(htmlfile, ".html");

	// scan the class tag for special tokens
	ScanTag(pDict, pClass->mipTag);

	// add the type name as-is to the dictionary
	ToDict(pDict, pClass->mipName, htmlfile);

	FunctionsToDict(pDict, pClass, htmlfile, OnlyFunctions, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyCtors, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyConvertors, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyMethods, &Anchor);
	FunctionsToDict(pDict, pClass, htmlfile, OnlyProperties, &Anchor);

	// add aliases
	for( i = 0; i < pClass->mipAlias->Count(pClass->mipAlias); i++ )
	{
		JCLString* pAlias = pClass->mipAlias->Get(pClass->mipAlias, i);
		ToDict(pDict, pAlias, htmlfile);
	}

exit:
	DELETE(htmlfile);
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
	JCLSetString(workstr, "{application}");
	AutoLinkKeywords(pDict, workstr, NULL);
	fprintf(pFile, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
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
	JCLFormatTime(workstr, "%c", time(NULL));
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
	if( pParams != NULL && strlen(pParams) > 0 )
	{
		JCLString* pValue;
		JCLString* pName = NEW(JCLString);
		JCLString* pHash = NEW(JCLString);
		JCLString* pOptions = NEW(JCLString);
		JCLSetString(pOptions, pParams);
		while( !JCLAtEnd(pOptions) )
		{
			JCLSpanExcluding(pOptions, "=", pName);
			JCLTrim(pName);
			if( JCLAtEnd(pOptions) || !JCLGetLength(pName) || !JCLContainsOnly(pName, kIdentifierSpan) )
				return JCL_WARN_Invalid_Option_Value;
			JCLFormat(pHash, "{%s}", JCLGetString(pName));
			JCLSeekForward(pOptions, 1);
			pValue = NEW(JCLString);
			JCLSpanExcluding(pOptions, ",;", pValue);
			JCLTrim(pValue);
			if( strcmp(JCLGetString(pName), "@ignore") == 0 )
			{
				JCLClass* pClass;
				if( FindClass(_this, pValue, &pClass) )
					JCLSetString(pClass->mipTag, "@ignore");
			}
			else
			{
				JILTable_SetItem(pDict, JCLGetString(pHash), pValue);
			}
			JCLSeekForward(pOptions, 1);
		}
		DELETE(pName);
		DELETE(pHash);
		DELETE(pOptions);
	}
	return JCL_No_Error;
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
	SplitTag(pFunc->mipTag, tagstr1, tagstr2);
	AutoLinkKeywords(pDict, tagstr1, pClass->mipName);
	AutoLinkKeywords(pDict, tagstr2, pClass->mipName);
	fprintf(pFile, "<tr id='dark'><td><pre>%s</pre></td></tr><tr id='light'><td><p>%s</p>", JCLGetString(workstr), JCLGetString(tagstr1));
	if( JCLGetLength(tagstr2) )
	{
		fprintf(pFile, "<p>%s</p>", JCLGetString(tagstr2));
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
		JCLFormat(tmpstr, "(%s)", JCLGetString(pClass->mipName));
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
		JCLSetString(familyName, (pClass->miModifier & kModiStrict) ? "strict " : "");
		JCLAppend(familyName, "class");
		JCLSetString(typeName, JCLGetString(pClass->mipName));
	}
	else if (pClass->miFamily == tf_interface)
	{
		JCLSetString(familyName, (pClass->miModifier & kModiStrict) ? "strict " : "");
		JCLAppend(familyName, (pClass->miModifier & kModiNativeInterface) ? "native interface" : "interface");
		JCLSetString(typeName, JCLGetString(pClass->mipName));
	}
	else if (pClass->miFamily == tf_thread)
	{
		JCLSetString(familyName, "cofunction");
		pClass->mipFuncType->ToString(pClass->mipFuncType, _this, pClass->mipName, typeName, flags);
	}
	else if (pClass->miFamily == tf_delegate)
	{
		JCLSetString(familyName, "delegate");
		pClass->mipFuncType->ToString(pClass->mipFuncType, _this, pClass->mipName, typeName, flags);
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
				JCLSetString(workstr, JCLGetString(pClass->mipName));
				JCLAppend(workstr, "::");
				JCLAppend(workstr, JCLGetString(pFunc->mipName));
				JCLFormat(tagstr, "%s#A%04d", JCLGetString(pFileName), *pAnchor);
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
			if( strcmp(JCLGetString(tempstr), "@define") == 0 )
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
			if( JCLBeginsWith(oldstr, "{") )
			{
				JILLong pos = JCLGetLocator(oldstr);
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
				JCLSetLocator(oldstr, pos);
			}
			JCLSpanExcluding(oldstr, kIdentifierSpan, tempstr);
			JCLAppend(workstr, JCLGetString(tempstr));
		}
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
			if( strcmp(JCLGetString(tempstr), "@define") == 0 )
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

#else	// JIL_USE_HTML_CODEGEN

JILError JCLCreateClassDoc(JCLState* pState, JCLClass* pClass)
{
	return JCL_No_Error;
}

#endif	// JIL_USE_HTML_CODEGEN
