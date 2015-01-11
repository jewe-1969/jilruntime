//------------------------------------------------------------------------------
// File: JCLFunc.c                                          (c) 2004 jewe.org
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
//	Describes a JewelScript function or method.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclstate.h"

//------------------------------------------------------------------------------
// forward declarations
//------------------------------------------------------------------------------

JILError			JCLLinkFunction		(JCLFunc* _this, JCLState* pCompiler);
static JCLString*	toString_JCLFunc	(JCLFunc*, JCLState*, JCLString*, JILLong);
static JCLString*	toXml_JCLFunc		(JCLFunc*, JCLState*, JCLString*);

/******************************************************************************/
/**************************** class JCLLiteral ********************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// JCLLiteral
//------------------------------------------------------------------------------
// constructor

void create_JCLLiteral( JCLLiteral* _this )
{
	_this->miType = type_null;
	_this->miHandle = 0;
	_this->miOffset = 0;
	_this->miLong = 0;
	_this->miFloat = 0.0;
	_this->miString = NEW(JCLString);
	_this->miLocator = 0;
	_this->miMethod = JILFalse;
	_this->mipFile = NULL;
	_this->mipStack = NEW(Array_JCLVar);
}

//------------------------------------------------------------------------------
// JCLLiteral
//------------------------------------------------------------------------------
// destructor

void destroy_JCLLiteral( JCLLiteral* _this )
{
	DELETE( _this->miString );
	DELETE( _this->mipStack );
}

//------------------------------------------------------------------------------
// JCLLiteral
//------------------------------------------------------------------------------
// copy data from another JCLLiteral

void copy_JCLLiteral( JCLLiteral* _this, const JCLLiteral* src )
{
	_this->miType = src->miType;
	_this->miHandle = src->miHandle;
	_this->miOffset = src->miOffset;
	_this->miLong = src->miLong;
	_this->miFloat = src->miFloat;
	JCLSetString(_this->miString, JCLGetString(src->miString));
	_this->miLocator = src->miLocator;
	_this->miMethod = src->miMethod;
	_this->mipFile = src->mipFile;
	_this->mipStack->Copy(_this->mipStack, src->mipStack);
}

/******************************************************************************/
/**************************** class JCLFuncType *******************************/
/******************************************************************************/

static JCLString* toString_JCLFuncType(JCLFuncType*, JCLState*, JCLString*, JCLString*, JILLong, JILLong);
static JCLString* toXml_JCLFuncType(JCLFuncType* _this, JCLState* pState, JCLString* pOut, JILLong);

//------------------------------------------------------------------------------
// JCLFuncType
//------------------------------------------------------------------------------
// constructor

void create_JCLFuncType( JCLFuncType* _this )
{
	_this->ToString = toString_JCLFuncType;
	_this->ToXml = toXml_JCLFuncType;

	_this->mipResult = NEW(JCLVar);
	_this->mipArgs = NEW(Array_JCLVar);
}

//------------------------------------------------------------------------------
// JCLFuncType
//------------------------------------------------------------------------------
// destructor

void destroy_JCLFuncType( JCLFuncType* _this )
{
	DELETE( _this->mipResult );
	DELETE( _this->mipArgs );
}

//------------------------------------------------------------------------------
// JCLFuncType
//------------------------------------------------------------------------------
// copy data from another JCLFuncType

void copy_JCLFuncType( JCLFuncType* _this, const JCLFuncType* src )
{
	_this->mipResult->Copy(_this->mipResult, src->mipResult);
	_this->mipArgs->Copy(_this->mipArgs, src->mipArgs);
}

//------------------------------------------------------------------------------
// JCLFuncType::ToString
//------------------------------------------------------------------------------
// Create a string representation of the declaration of this delegate.
// The resulting string is APPENDED to the given string.

static JCLString* toString_JCLFuncType(JCLFuncType* _this, JCLState* pCompiler, JCLString* pName, JCLString* outString, JILLong flags, JILLong hint)
{
	JCLVar* pVar;
	Array_JCLVar* pArgs;
	JILLong i;

	if( flags & kClearFirst )
	{
		JCLClear(outString);
		flags &= ~kClearFirst;
	}
	// in compact mode, omit identifier names
	if( flags & kCompact )
		flags &= ~kIdentNames;
	// write keyword 'delegate' in full decl mode
	if( flags & kFullDecl )
		JCLAppend(outString, "delegate ");
	// write result (if the function has a result)
	pVar = _this->mipResult;
	if( pVar->miType != type_null )
	{
		pVar->ToString(pVar, pCompiler, outString, (flags & ~kIdentNames), hint);
		JCLAppend(outString, " ");
	}
	// write function name
	if( pName )
	{
		if( flags & kFullDecl )
		{
			JCLAppend(outString, JCLGetString(pName));
		}
		else
		{
			JCLAppend(outString, "(");
			JCLAppend(outString, JCLGetString(pName));
			JCLAppend(outString, ")");
		}
	}
	if( flags & kCompact )
		JCLAppend(outString, "(");
	else
		JCLAppend(outString, " (");
	// write function arguments
	pArgs = _this->mipArgs;
	for( i = 0; i < pArgs->Count(pArgs); i++ )
	{
		// write argument
		pVar = pArgs->Get(pArgs, i);
		pVar->ToString(pVar, pCompiler, outString, flags, hint);
		// write comma if this wasn't the last arg
		if( (i + 1) < pArgs->Count(pArgs) )
		{
			if( flags & kCompact )
				JCLAppend(outString, ",");
			else
				JCLAppend(outString, ", ");
		}
	}
	// write end
	JCLAppend(outString, ")");
	return outString;
}

//------------------------------------------------------------------------------
// JCLFuncType::ToXml
//------------------------------------------------------------------------------

static JCLString* toXml_JCLFuncType(JCLFuncType* _this, JCLState* pState, JCLString* pOut, JILLong hint)
{
	int i;
	JCLVar* pVar;

	JCLAppend(pOut, "<signature>\n");
	JCLAppend(pOut, "<result>\n");
	if( _this->mipResult->miType != type_null )
		_this->mipResult->ToXml(_this->mipResult, pState, pOut, hint);
	JCLAppend(pOut, "</result>\n");

	JCLAppend(pOut, "<args>\n");
	for( i = 0; i < _this->mipArgs->Count(_this->mipArgs); i++ )
	{
		pVar = _this->mipArgs->Get(_this->mipArgs, i);
		pVar->ToXml(pVar, pState, pOut, hint);
	}
	JCLAppend(pOut, "</args>\n");
	JCLAppend(pOut, "</signature>\n");
	return pOut;
}

/******************************************************************************/
/****************************   class JCLFunc   *******************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// JCLFunc
//------------------------------------------------------------------------------
// constructor
 
void create_JCLFunc( JCLFunc* _this )
{
	JILLong i;

	_this->LinkCode = JCLLinkFunction;
	_this->ToString = toString_JCLFunc;
	_this->ToXml = toXml_JCLFunc;

	// alloc objects in our struct
	_this->mipName = NEW(JCLString);
	_this->mipTag = NEW(JCLString);
	_this->mipResult = NEW(JCLVar);
	_this->mipArgs = NEW(Array_JCLVar);
	_this->mipCode = NEW(Array_JILLong);
	_this->mipLiterals = NEW( Array_JCLLiteral );
	_this->miHandle = 0;
	_this->miFuncIdx = 0;
	_this->miClassID = 0;
	_this->miLnkAddr = 0;
	_this->miLnkDelegate = -1;
	_this->miLnkMethod = -1;
	_this->miLnkClass = 0;
	_this->miLnkBaseVar = 0;
	_this->miLnkRelIdx = -1;
	_this->miLnkVarOffset = 0;
	_this->miRetFlag = JILFalse;
	_this->miYieldFlag = JILFalse;
	_this->miMethod = JILFalse;
	_this->miCtor = JILFalse;
	_this->miConvertor = JILFalse;
	_this->miAccessor = JILFalse;
	_this->miCofunc = JILFalse;
	_this->miAnonymous = JILFalse;
	_this->miExplicit = JILFalse;
	_this->miStrict = JILFalse;
	_this->miVirtual = JILFalse;
	_this->miNoOverride = JILFalse;
	_this->miPrivate = JILFalse;
	_this->miLinked = JILFalse;
	_this->miNaked = JILFalse;
	_this->miOptLevel = 0;
	_this->mipParentStack = NULL;

	for( i = 0; i < kNumRegisters; i++ )
	{
		_this->miLocalRegs[i] = 0;
		_this->miRegUsage[i] = 0;
	}
}

//------------------------------------------------------------------------------
// JCLFunc
//------------------------------------------------------------------------------
// destructor

void destroy_JCLFunc( JCLFunc* _this )
{
	// free objects in our struct
	DELETE( _this->mipName );
	DELETE( _this->mipTag );
	DELETE( _this->mipResult );
	DELETE( _this->mipArgs );
	DELETE( _this->mipCode );
	DELETE( _this->mipLiterals );
}

//------------------------------------------------------------------------------
// JCLFunc::Copy
//------------------------------------------------------------------------------
// Copy data from another JCLFunc to this one.

void copy_JCLFunc(JCLFunc* _this, const JCLFunc* src)
{
	JILLong i;

	_this->mipName->Copy(_this->mipName, src->mipName);
	_this->mipTag->Copy(_this->mipTag, src->mipTag);
	_this->mipResult->Copy(_this->mipResult, src->mipResult);
	_this->mipArgs->Copy(_this->mipArgs, src->mipArgs);
	_this->mipCode->Copy(_this->mipCode, src->mipCode);
	_this->mipLiterals->Copy(_this->mipLiterals, src->mipLiterals);
	_this->miHandle = src->miHandle;
	_this->miFuncIdx = src->miFuncIdx;
	_this->miClassID = src->miClassID;
	_this->miLnkAddr = src->miLnkAddr;
	_this->miLnkClass = src->miLnkClass;
	_this->miLnkBaseVar = src->miLnkBaseVar;
	_this->miLnkVarOffset = src->miLnkVarOffset;
	_this->miRetFlag = src->miRetFlag;
	_this->miMethod = src->miMethod;
	_this->miCtor = src->miCtor;
	_this->miConvertor = src->miConvertor;
	_this->miAccessor = src->miAccessor;
	_this->miCofunc = src->miCofunc;
	_this->miAnonymous = src->miAnonymous;
	_this->miExplicit = src->miExplicit;
	_this->miOptLevel = src->miOptLevel;
	_this->miStrict = src->miStrict;
	_this->miVirtual = src->miVirtual;
	_this->miNoOverride = src->miNoOverride;
	_this->miPrivate = src->miPrivate;
	_this->miLinked = src->miLinked;
	_this->miNaked = src->miNaked;
	_this->mipParentStack = src->mipParentStack;

	// make sure other classes do not inherit these
	_this->miLnkDelegate = -1;
	_this->miLnkMethod = -1;
	_this->miLnkRelIdx = -1;

	for( i = 0; i < kNumRegisters; i++ )
	{
		_this->miLocalRegs[i] = src->miLocalRegs[i];
		_this->miRegUsage[i] = src->miRegUsage[i];
	}
}

//------------------------------------------------------------------------------
// JCLFunc::ToString
//------------------------------------------------------------------------------
// Create a string representation of the declaration of this function.
// The resulting string is APPENDED to the given string.

static JCLString* toString_JCLFunc(JCLFunc* _this, JCLState* pCompiler, JCLString* outString, JILLong flags)
{
	JCLVar* pVar;
	Array_JCLVar* pArgs;
	JILLong i;
	JCLClass* pClass;

	if( flags & kClearFirst )
	{
		JCLClear(outString);
		flags &= ~kClearFirst;
	}
	// in compact mode, omit identifier names
	if( flags & kCompact )
		flags &= ~kIdentNames;

	// write keyword 'function' or 'method' in full decl mode
	if( flags & kFullDecl )
	{
		if( _this->miExplicit )
			JCLAppend(outString, "explicit ");
		if( _this->miAccessor )
			JCLAppend(outString, "accessor ");
		else if( _this->miMethod )
			JCLAppend(outString, "method ");
		else if( _this->miCofunc )
			JCLAppend(outString, "cofunction ");
		else
			JCLAppend(outString, "function ");
	}

	// write result (if the function has a result)
	pVar = _this->mipResult;
	if( pVar->miType != type_null )
	{
		pVar->ToString(pVar, pCompiler, outString, (flags & ~kIdentNames), _this->miClassID);
		JCLAppend(outString, " ");
	}

	// write class name (if method)
	pClass = GetClass(pCompiler, _this->miClassID);
	if( pClass->miFamily == tf_class && !(flags & kNoClassName) )
	{
		JCLAppend(outString, JCLGetString(pClass->mipName));
		JCLAppend(outString, "::");
	}

	// write function name
	JCLAppend(outString, JCLGetString(_this->mipName));
	if( flags & kCompact )
		JCLAppend(outString, "(");
	else
		JCLAppend(outString, " (");

	// write function arguments
	pArgs = _this->mipArgs;
	for( i = 0; i < pArgs->Count(pArgs); i++ )
	{
		// write argument
		pVar = pArgs->Get(pArgs, i);
		pVar->ToString(pVar, pCompiler, outString, flags, _this->miClassID);
		// write comma if this wasn't the last arg
		if( (i + 1) < pArgs->Count(pArgs) )
		{
			if( flags & kCompact )
				JCLAppend(outString, ",");
			else
				JCLAppend(outString, ", ");
		}
	}
	// write end
	JCLAppend(outString, ")");
	return outString;
}

//------------------------------------------------------------------------------
// JCLFunc::ToXml
//------------------------------------------------------------------------------

static JCLString* toXml_JCLFunc(JCLFunc* _this, JCLState* pState, JCLString* pOut)
{
	int i;
	JCLVar* pVar;
	JCLString* workstr = NEW(JCLString);

	JCLAppend(pOut, "<function type=\"");
	if( _this->miAccessor )
		JCLAppend(pOut, "accessor");
	else if( _this->miCtor )
		JCLAppend(pOut, "constructor");
	else if( _this->miConvertor )
		JCLAppend(pOut, "convertor");
	else if( _this->miMethod )
		JCLAppend(pOut, "method");
	else if( _this->miCofunc )
		JCLAppend(pOut, "cofunction");
	else
		JCLAppend(pOut, "function");
	JCLAppend(pOut, "\" name=\"");
	JCLAppend(pOut, JCLGetString(_this->mipName));
	JCLAppend(pOut, "\" mode=\"");
	if( _this->miStrict || _this->miExplicit )
	{
		if( _this->miStrict )
			JCLAppend(pOut, "strict ");
		if( _this->miExplicit )
			JCLAppend(pOut, "explicit ");
	}
	JCLFormat(workstr, "%d", _this->miFuncIdx);
	JCLAppend(pOut, "\" index=\"");
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "\">\n");

	JCLAppend(pOut, "<result>\n");
	if( _this->mipResult->miType != type_null )
		_this->mipResult->ToXml(_this->mipResult, pState, pOut, _this->miClassID);
	JCLAppend(pOut, "</result>\n");

	JCLAppend(pOut, "<args>\n");
	for( i = 0; i < _this->mipArgs->Count(_this->mipArgs); i++ )
	{
		pVar = _this->mipArgs->Get(_this->mipArgs, i);
		pVar->ToXml(pVar, pState, pOut, _this->miClassID);
	}
	JCLAppend(pOut, "</args>\n");

	JCLAppend(pOut, "<tag>");
	JCLEscapeXml(workstr, _this->mipTag);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "</tag>\n");

	JCLAppend(pOut, "</function>\n");
	DELETE(workstr);
	return pOut;
}

//------------------------------------------------------------------------------
// GetFuncInfoFlags
//------------------------------------------------------------------------------

JILLong GetFuncInfoFlags(JCLFunc* func)
{
	JILLong flags = 0;
	if( func->miMethod )	flags |= fi_method;
	if( func->miCtor )		flags |= fi_ctor;
	if( func->miConvertor )	flags |= fi_convertor;
	if( func->miAccessor )	flags |= fi_accessor;
	if( func->miCofunc )	flags |= fi_cofunc;
	if( func->miAnonymous )	flags |= fi_anonymous;
	if( func->miExplicit )	flags |= fi_explicit;
	if( func->miStrict )	flags |= fi_strict;
	if( func->miVirtual )	flags |= fi_virtual;
	return flags;
}
