//------------------------------------------------------------------------------
// File: JCLVar.c                                           (c) 2004 jewe.org
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
//	This is used to describe a storage location for the machine. It can be either
//	a register, or a location on the stack. The JCL parser allocates registers
//	and stack locations automatically and dynamically. Therefore, when writing
//	code in JCL, you don't have to care about register or stack address usage -
//	instead you just use variable names.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclstate.h"

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static void			copyType_JCLVar	(JCLVar*, const JCLVar*);
static JCLString*	toString_JCLVar	(JCLVar*, JCLState*, JCLString*, JILLong, JILLong);
static JCLString*	toXml_JCLVar	(JCLVar*, JCLState*, JCLString*, JILLong);

//------------------------------------------------------------------------------
// JCLVar
//------------------------------------------------------------------------------
// constructor

void create_JCLVar( JCLVar* _this )
{
	_this->CopyType = copyType_JCLVar;
	_this->ToString = toString_JCLVar;
	_this->ToXml = toXml_JCLVar;

	_this->miType = type_null;
	_this->miConst = JILFalse;
	_this->miRef = JILFalse;
	_this->miWeak = JILFalse;
	_this->miElemType = type_null;
	_this->miElemRef = JILFalse;

	_this->mipName = NEW(JCLString);
	_this->mipArrIdx = NULL;
	_this->miMode = kModeUnused;
	_this->miUsage = kUsageVar;
	_this->miIndex = 0;
	_this->miMember = 0;
	_this->miIniType = type_null;
	_this->miInited = JILFalse;
	_this->miUnique = JILFalse;
	_this->miConstP = JILFalse;
	_this->miOnStack = JILFalse;
	_this->miTypeCast = JILFalse;
	_this->miHidden = JILFalse;
}

//------------------------------------------------------------------------------
// JCLVar
//------------------------------------------------------------------------------
// destructor

void destroy_JCLVar( JCLVar* _this )
{
	// free objects in our struct
	DELETE( _this->mipName );
}

//------------------------------------------------------------------------------
// JCLVar::Copy
//------------------------------------------------------------------------------
// Copy all data from a JCLVar to this JCLVar

void copy_JCLVar(JCLVar* _this, const JCLVar* src)
{
	_this->miType = src->miType;
	_this->miConst = src->miConst;
	_this->miRef = src->miRef;
	_this->miWeak = src->miWeak;
	_this->miElemType = src->miElemType;
	_this->miElemRef = src->miElemRef;

	_this->mipName->Copy(_this->mipName, src->mipName);
	_this->miMode = src->miMode;
	_this->miUsage = src->miUsage;
	_this->miIndex = src->miIndex;
	_this->miMember = src->miMember;
	_this->miIniType = src->miIniType;
	_this->miInited = src->miInited;
	_this->miUnique = src->miUnique;
	_this->miConstP = src->miConstP;
	_this->miTypeCast = src->miTypeCast;
	_this->miHidden = src->miHidden;

	_this->miOnStack = JILFalse;
	_this->mipArrIdx = NULL;
}

//------------------------------------------------------------------------------
// JCLVar::CopyType
//------------------------------------------------------------------------------
// Copy only the type information from src to this

void copyType_JCLVar(JCLVar* _this, const JCLVar* src)
{
	_this->miType = src->miType;
	_this->miConst = src->miConst;
	_this->miRef = src->miRef;
	_this->miWeak = src->miWeak;
	_this->miElemType = src->miElemType;
	_this->miElemRef = src->miElemRef;
}

//------------------------------------------------------------------------------
// TypeToString
//------------------------------------------------------------------------------

static void TypeToString(JCLVar* _this, JCLState* pCompiler, JCLString* outString, JILLong type, JILLong hint)
{
	switch( type )
	{
		case type_null:
			JCLAppend(outString, "null");
			break;
		case type_int:
			JCLAppend(outString, "int");
			break;
		case type_float:
			JCLAppend(outString, "float");
			break;
		case type_var:
			JCLAppend(outString, "var");
			break;
		default:
		{
			JCLClass* pClass = GetClass(pCompiler, type);
			if( pClass )
			{
				if( pClass->miFamily == tf_delegate || pClass->miFamily == tf_thread )
				{
					if( hint )
					{
						JILLong i;
						JCLClass* pClass2 = GetClass(pCompiler, hint);
						for( i = 0; i < pClass->mipAlias->Count(pClass->mipAlias); i++ )
						{
							JCLString* str = pClass->mipAlias->Get(pClass->mipAlias, i);
							if( JCLBeginsWith(str, JCLGetString(pClass2->mipName)) )
							{
								JCLAppend(outString, JCLGetString(str));
								break;
							}
						}
						if( i == pClass->mipAlias->Count(pClass->mipAlias) )
							JCLAppend(outString, JCLGetString(pClass->mipName));
					}
					else
						JCLAppend(outString, JCLGetString(pClass->mipName));
				}
				else if (pClass->miFamily == tf_class || pClass->miFamily == tf_interface)
				{
					JCLAppend(outString, JCLGetString(pClass->mipName));
				}
				else
					JCLAppend(outString, "ERROR");
			}
			else
				JCLAppend(outString, "ERROR");
			break;
		}
	}
}

//------------------------------------------------------------------------------
// JCLVar::ToString
//------------------------------------------------------------------------------
// Create a string representation of the TYPE INFORMATION in a JCLVar. The
// result is APPENDED to the given string.

static JCLString* toString_JCLVar(JCLVar* _this, JCLState* pCompiler, JCLString* outString, JILLong flags, JILLong hint)
{
	if( flags & kClearFirst )
		JCLClear(outString);
	if( _this->miConst )
		JCLAppend(outString, "const ");
	if( _this->miWeak )
		JCLAppend(outString, "weak ");
	if( _this->miType == type_array )
	{
		TypeToString(_this, pCompiler, outString, _this->miElemType, hint);
		JCLAppend(outString, "[]");
	}
	else
	{
		TypeToString(_this, pCompiler, outString, _this->miType, hint);
	}
	if( (flags & kIdentNames) && JCLGetLength(_this->mipName) )
	{
		JCLAppend(outString, " ");
		if( (flags & kCurrentScope) )
		{
			JCLClass* pClass = GetClass(pCompiler, pCompiler->miClass);
			JCLAppend(outString, JCLGetString(pClass->mipName));
			JCLAppend(outString, "::");
		}
		JCLAppend(outString, JCLGetString(_this->mipName));
	}
	return outString;
}

//------------------------------------------------------------------------------
// JCLVar::ToXml
//------------------------------------------------------------------------------

static JCLString* toXml_JCLVar(JCLVar* _this, JCLState* pState, JCLString* pOut, JILLong hint)
{
	JCLAppend(pOut, "<var type=\"");
	if( _this->miType == type_array )
	{
		TypeToString(_this, pState, pOut, _this->miElemType, hint);
		JCLAppend(pOut, "[]");
	}
	else
	{
		TypeToString(_this, pState, pOut, _this->miType, hint);
	}
	JCLAppend(pOut, "\" name=\"");
	JCLAppend(pOut, JCLGetString(_this->mipName));
	JCLAppend(pOut, "\" mode=\"");
	if( _this->miConst )
		JCLAppend(pOut, "const ");
	if( _this->miWeak )
		JCLAppend(pOut, "weak ");
	JCLAppend(pOut, "\" />\n");
	return pOut;
}

//------------------------------------------------------------------------------
// Implement array templates
//------------------------------------------------------------------------------

IMPL_DATA_ARRAY( JILLong )

//------------------------------------------------------------------------------
// JCLClrTypeInfo
//------------------------------------------------------------------------------
// Clear (fill with zeroes) a type info structure.

void JCLClrTypeInfo(TypeInfo* _this)
{
	memset(_this, 0, sizeof(TypeInfo));
}

//------------------------------------------------------------------------------
// JCLSetTypeInfo
//------------------------------------------------------------------------------
// Manually fills out a type info structure.

void JCLSetTypeInfo( TypeInfo* _this, JILLong type, JILBool bConst, JILBool bRef, JILBool bWeak, JILLong eType, JILBool eRef)
{
	_this->miType = type;
	_this->miConst = bConst;
	_this->miRef = bRef;
	_this->miWeak = bWeak;
	_this->miElemType = eType;
	_this->miElemRef = eRef;
}

//------------------------------------------------------------------------------
// JCLTypeInfoFromVar
//------------------------------------------------------------------------------
// Copies the type info from a given JCLVar into the struct.

void JCLTypeInfoFromVar(TypeInfo* _this, const JCLVar* pVar)
{
	_this->miType = pVar->miType;
	_this->miConst = pVar->miConst;
	_this->miRef = pVar->miRef;
	_this->miWeak = pVar->miWeak;
	_this->miElemType = pVar->miElemType;
	_this->miElemRef = pVar->miElemRef;
}

//------------------------------------------------------------------------------
// JCLTypeInfoSrcDst
//------------------------------------------------------------------------------
// For a source to dest operation, this function automatically chooses the right
// type info to copy. The destination is the L-Value of the operation and can
// also be NULL, if the expression has no L-Value.

void JCLTypeInfoSrcDst(TypeInfo* _this, const JCLVar* src, const JCLVar* dst)
{
	// do we have an L-Value?
	if( dst )
	{
		if( src->miType != type_var && dst->miType == type_var )
		{
			JCLTypeInfoFromVar(_this, src);
		}
		else
		{
			// take pure type from dst, but modifiers const / ref from src
			JCLTypeInfoFromVar(_this, dst);
			_this->miConst = src->miConst;
			_this->miRef = src->miRef;
			_this->miElemRef = src->miElemRef;
		}
	}
	else
	{
		JCLTypeInfoFromVar(_this, src);
	}
}

//------------------------------------------------------------------------------
// JCLTypeInfoToVar
//------------------------------------------------------------------------------
// Copies the type info from a TypeInfo into the given JCLVar.

void JCLTypeInfoToVar(const TypeInfo* _this, JCLVar* pVar)
{
	pVar->miType = _this->miType;
	pVar->miConst = _this->miConst;
	pVar->miRef = _this->miRef;
	pVar->miWeak = _this->miWeak;
	pVar->miElemType = _this->miElemType;
	pVar->miElemRef = _this->miElemRef;
}

//------------------------------------------------------------------------------
// JCLTypeInfoCopy
//------------------------------------------------------------------------------
// Copies the type info from the source to this type info.

void JCLTypeInfoCopy(TypeInfo* _this, const TypeInfo* src)
{
	_this->miType = src->miType;
	_this->miConst = src->miConst;
	_this->miRef = src->miRef;
	_this->miWeak = src->miWeak;
	_this->miElemType = src->miElemType;
	_this->miElemRef = src->miElemRef;
}
