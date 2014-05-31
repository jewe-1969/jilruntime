//------------------------------------------------------------------------------
// File: JCLString.h                                        (c) 2004 jewe.org
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
/// @file jclstring.h
/// A dynamic string object especially for the tasks of parsing / compiling.
/// This file is included from JCLState.h.
//------------------------------------------------------------------------------

#ifndef JCLSTRING_H
#define JCLSTRING_H

#include "jiltypes.h"
#include "jcltools.h"

//------------------------------------------------------------------------------
// class JCLString
//------------------------------------------------------------------------------
// A string class with integrated parsing support.

FORWARD_CLASS( JCLString )
DECL_CLASS( JCLString )

	JILChar*	m_String;
	JILLong		m_Locator;
	JILLong		m_Length;
	JILLong		m_AllocatedLength;

END_CLASS( JCLString )

//------------------------------------------------------------------------------
// basic string functions (ignoring locator)
//------------------------------------------------------------------------------

JCLString*				JCLCopyString		(const JCLString* pSource);
void					JCLSetString		(JCLString* _this, const JILChar* string);
static JILLong			JCLGetLength		(const JCLString* _this)			{ return _this->m_Length; }
static const JILChar*	JCLGetString		(const JCLString* _this)			{ return (_this->m_Length > 0) ? _this->m_String : ""; }
static JILLong			JCLGetChar			(JCLString* _this, JILLong index)	{ return (index < _this->m_Length) ? (JILByte) _this->m_String[index] : 0; }
static JILLong			JCLGetLastChar		(JCLString* _this)					{ return (_this->m_Length > 0) ? (JILByte) _this->m_String[_this->m_Length - 1] : 0; }
JILBool					JCLCompare			(const JCLString* _this, const JCLString* other);
JILBool					JCLCompareNoCase	(const JCLString* _this, const JCLString* other);
JILBool					JCLEquals			(const JCLString* _this, const JILChar* other);
void					JCLClear			(JCLString* _this);
void					JCLAppend			(JCLString* _this, const JILChar* source);
void					JCLAppendChar		(JCLString* _this, JILLong chr);
void					JCLInsert			(JCLString* _this, const JCLString* source, JILLong index);
void					JCLRemove			(JCLString* _this, JILLong index, JILLong length);
JILLong					JCLReplace			(JCLString* _this, const JILChar* search, const JILChar* replace);
void					JCLSubString		(JCLString* _this, const JCLString* source, JILLong index, JILLong length);
void					JCLFill				(JCLString* _this, JILLong chr, JILLong size);
void					JCLTrim				(JCLString* _this);
void					JCLCollapseSpaces	(JCLString* _this);
void					JCLRandomIdentifier	(JCLString* _this, JILLong length);
JILLong					JCLFormat			(JCLString* _this, const JILChar* pFormat, ...);
JILLong					JCLFormatTime		(JCLString* _this, const JILChar* pFormat, time_t time);
JILLong					JCLFindChar			(const JCLString* _this, JILLong chr, JILLong index);
JILLong					JCLFindCharReverse	(const JCLString* _this, JILLong chr, JILLong index);
JILLong					JCLFindString		(const JCLString* _this, const JILChar* src, JILLong index);
JILLong					JCLReadTextFile		(JCLString* _this, const JILChar* pFile, JILState* pVM);
void					JCLEscapeXml		(JCLString* _this, const JCLString* pString);

//------------------------------------------------------------------------------
// advanced string parsing functions (locator based)
//------------------------------------------------------------------------------

static JILLong			JCLAtEnd			(const JCLString* _this)	{ return (_this->m_Locator == _this->m_Length); }
static JILLong			JCLGetLocator		(const JCLString* _this)	{ return _this->m_Locator; }
static JILLong			JCLGetCurrentChar	(const JCLString* _this)	{ return (_this->m_Locator < _this->m_Length) ? (JILByte) _this->m_String[_this->m_Locator] : 0; }
void					JCLSetLocator		(JCLString* _this, JILLong position);
JILBool					JCLBeginsWith		(const JCLString* _this, const JILChar* string);
JILLong					JCLSpanIncluding	(JCLString* _this, const JILChar* charSet, JCLString* result);
JILLong					JCLSpanExcluding	(JCLString* _this, const JILChar* charSet, JCLString* result);
JILLong					JCLSpanBetween		(JCLString* _this, JILChar startChr, JILChar endChr, JCLString* result);
JILLong					JCLSpanNumber		(JCLString* _this, JCLString* result, JILLong* type);
JILLong					JCLSeekWhile		(JCLString* _this, const JILChar* charSet);
JILLong					JCLSeekUntil		(JCLString* _this, const JILChar* charSet);
JILLong					JCLSeekForward		(JCLString* _this, JILLong length);
JILLong					JCLSeekString		(JCLString* _this, const JILChar* string);
JILLong					JCLContainsOneOf	(JCLString* _this, const JILChar* charSet);
JILLong					JCLContainsOnly		(JCLString* _this, const JILChar* charSet);

//------------------------------------------------------------------------------
// JCLDeclStruct
//------------------------------------------------------------------------------
/// A little helper struct used when importing native types

typedef struct
{
	JCLString*	pString;
	JILState*	pState;
} JCLDeclStruct;

#endif	// #ifndef JCLSTRING_H
