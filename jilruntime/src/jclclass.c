//------------------------------------------------------------------------------
// File: JCLClass.c                                         (c) 2004 jewe.org
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
//	Describes a JCL class. JCL functions can only be placed inside a class.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jclfunc.h"
#include "jclclass.h"

//------------------------------------------------------------------------------
// Implement array for JCLString
//------------------------------------------------------------------------------

IMPL_ARRAY( JCLString )

static JCLString* toXml_JCLClass(JCLClass* _this, JCLState* pState, JCLString* pOut);

//------------------------------------------------------------------------------
// JCLClass
//------------------------------------------------------------------------------
// constructor

void create_JCLClass( JCLClass* _this )
{
	_this->ToXml = toXml_JCLClass;

	_this->mipName = NEW(JCLString);
	_this->mipTag = NEW(JCLString);
	_this->miType = 0;
	_this->miBaseType = 0;
	_this->miHybridBase = 0;
	_this->miParentType = 0;
	_this->miModifier = 0;
	_this->miFamily = tf_undefined;
	_this->miNative = JILFalse;
	_this->miHasBody = JILFalse;
	_this->miHasVTable = JILFalse;
	_this->miHasCtor = JILFalse;
	_this->miHasMethod = JILFalse;
	_this->mipFuncs = NEW(Array_JCLFunc);
	_this->mipVars = NEW(Array_JCLVar);
	_this->mipAlias = NEW(Array_JCLString);
	_this->mipFuncType = NEW(JCLFuncType);
	_this->miMethodInfo.ctor = -1;
	_this->miMethodInfo.cctor = -1;
	_this->miMethodInfo.dtor = -1;
	_this->miMethodInfo.tostr = -1;
}

//------------------------------------------------------------------------------
// JCLClass
//------------------------------------------------------------------------------
// destructor

void destroy_JCLClass( JCLClass* _this )
{
	DELETE( _this->mipName );
	DELETE( _this->mipTag );
	DELETE( _this->mipFuncs );
	DELETE( _this->mipVars );
	DELETE( _this->mipAlias );
	DELETE( _this->mipFuncType );
}

//------------------------------------------------------------------------------
// JCLClass::Copy
//------------------------------------------------------------------------------
// Copy all members

void copy_JCLClass( JCLClass* _this, const JCLClass* src )
{
	_this->mipName->Copy(_this->mipName, src->mipName);
	_this->mipTag->Copy(_this->mipTag, src->mipTag);
	_this->miType = src->miType;
	_this->miBaseType = src->miBaseType;
	_this->miHybridBase = src->miHybridBase;
	_this->miParentType = src->miParentType;
	_this->miModifier = src->miModifier;
	_this->miFamily = src->miFamily;
	_this->miNative = src->miNative;
	_this->miHasBody = src->miHasBody;
	_this->miHasVTable = src->miHasVTable;
	_this->miHasCtor = src->miHasCtor;
	_this->miHasMethod = src->miHasMethod;
	_this->mipFuncs->Copy(_this->mipFuncs, src->mipFuncs);
	_this->mipVars->Copy(_this->mipVars, src->mipVars);
	_this->mipAlias->Copy(_this->mipAlias, src->mipAlias);
	_this->mipFuncType->Copy(_this->mipFuncType, src->mipFuncType);
	_this->miMethodInfo.ctor = src->miMethodInfo.ctor;
	_this->miMethodInfo.cctor = src->miMethodInfo.cctor;
	_this->miMethodInfo.dtor = src->miMethodInfo.dtor;
	_this->miMethodInfo.tostr = src->miMethodInfo.tostr;
}

//------------------------------------------------------------------------------
// JCLClass::ToXml()
//------------------------------------------------------------------------------

static JCLString* toXml_JCLClass(JCLClass* _this, JCLState* pState, JCLString* pOut)
{
	int i;
	JCLFunc* pFunc;
	JCLVar* pVar;
	JCLString* workstr = NEW(JCLString);

	JCLAppend(pOut, "<type family=\"");
	switch(_this->miFamily)
	{
		case tf_integral:
			JCLAppend(pOut, "integral");
			break;
		case tf_class:
			JCLAppend(pOut, "class");
			break;
		case tf_interface:
			JCLAppend(pOut, "interface");
			break;
		case tf_thread:
			JCLAppend(pOut, "thread");
			break;
		case tf_delegate:
			JCLAppend(pOut, "delegate");
			break;
	}
	JCLAppend(pOut, "\" name=\"");
	JCLAppend(pOut, JCLGetString(_this->mipName));
	JCLAppend(pOut, "\" typeid=\"");
	JCLFormat(workstr, "%d", _this->miType);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "\" parentid=\"");
	JCLFormat(workstr, "%d", _this->miParentType);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "\" baseid=\"");
	JCLFormat(workstr, "%d", _this->miBaseType);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "\" hybridid=\"");
	JCLFormat(workstr, "%d", _this->miHybridBase);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "\" mode=\"");
	if(_this->miModifier & kModiStrict)
	{
		JCLAppend(pOut, "strict ");
	}
	if(_this->miModifier & kModiNative)
	{
		JCLAppend(pOut, "native ");
	}
	if(_this->miModifier & kModiExtern)
	{
		JCLAppend(pOut, "extern ");
	}
	JCLAppend(pOut, "\" isnative=\"");
	if(_this->miNative)
	{
		JCLAppend(pOut, "true");
	}
	else
	{
		JCLAppend(pOut, "false");
	}
	JCLAppend(pOut, "\">\n");

	JCLAppend(pOut, "<functions>\n");
	for(i = 0; i < _this->mipFuncs->count; i++)
	{
		pFunc = _this->mipFuncs->Get(_this->mipFuncs, i);
		pFunc->ToXml(pFunc, pState, pOut);
	}
	JCLAppend(pOut, "</functions>\n");
	
	JCLAppend(pOut, "<variables>\n");
	for(i = 0; i < _this->mipVars->count; i++)
	{
		pVar = _this->mipVars->Get(_this->mipVars, i);
		pVar->ToXml(pVar, pState, pOut);
	}
	JCLAppend(pOut, "</variables>\n");

	JCLAppend(pOut, "<aliases>\n");
	for(i = 0; i < _this->mipAlias->count; i++)
	{
		JCLAppend(pOut, "<alias name=\"");
		JCLAppend(pOut, JCLGetString(_this->mipAlias->array[i]));
		JCLAppend(pOut, "\" />\n");
	}
	JCLAppend(pOut, "</aliases>\n");

	_this->mipFuncType->ToXml(_this->mipFuncType, pState, pOut);

	JCLAppend(pOut, "<tag>");
	JCLEscapeXml(workstr, _this->mipTag);
	JCLAppend(pOut, JCLGetString(workstr));
	JCLAppend(pOut, "</tag>\n");

	JCLAppend(pOut, "</type>\n");
	DELETE(workstr);
	return pOut;
}

//------------------------------------------------------------------------------
// Implement array template
//------------------------------------------------------------------------------

IMPL_ARRAY( JCLClass )
