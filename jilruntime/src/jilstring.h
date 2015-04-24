//------------------------------------------------------------------------------
// File: JILString.h                                           (c) 2015 jewe.org
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
/// @file jilstring.h
/// The built-in string class. This is not to be confused with the internal
/// string class of the compiler. This is the native type 'string' available
/// in JewelScript.
/// <p>The string is limited to a size of at most 2 GB of memory. It handles
/// raw data and zero bytes in the string graciously, so it can also be used
/// for UTF-8 strings. However, advanced string operations might corrupt UTF-8
/// encoded strings, since they use the standard ANSI C string functions.
/// The class does not support multi-byte strings like UTF-16 or higher.</p>
/// <p>The class also provides a few methods that make using the string from the
/// native C side easier. In general, all methods declared in this file may
/// be used from the native C side as well.</p>
/// <pre>
/// const char* str = JILString_String(pString)    - Returns a c-string (char*) from the given JILString object
/// JILString_Assign(pString, str)                 - Assigns a c-string (char*) to the given JILString object (the c-string is copied)
/// JILString_AppendCStr(pString, str)             - Appends a c-string (char*) to the given JILString object
/// JILLong length = JILString_Length(pString)     - Gets the length of a JILString object
/// </pre>
//------------------------------------------------------------------------------

#ifndef JILSTRING_H
#define JILSTRING_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
/// Describes the result of string matching operation as returned by the string::matchString() and string::matchArray() methods.

typedef struct NStringMatch
{
	JILLong	matchStart;		///< The character position where this match starts. For matchString() the position refers to 'this' string. For matchArray() the position refers to the element specified by 'arrayIndex'.
	JILLong	matchLength;	///< The length of the match in characters.
	JILLong	arrayIndex;		///< The array index of the matching element. For matchString() it specifies the array element that was found in 'this' string as a substring. For matchArray() it specifies the array element that contains 'this' string as a substring.

} NStringMatch;

//------------------------------------------------------------------------------
// struct JILString
//------------------------------------------------------------------------------
/// This is the built-in dynamic string object used by the virtual machine.

struct JILString
{
	JILLong		length;		///< The currently used length, in characters, of the string
	JILLong		maxLength;	///< The currently allocated size, in bytes; if length reaches this value, the string is resized
	JILChar*	string;		///< Pointer to the string buffer (the string is null-terminated, so it can be directly used as a C-string)
	JILState*	pState;		///< The virtual machine object this string 'belongs' to
};

//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------
// The following functions are both, imported into JewelScript and useful when
// using the string object from within C or C++.

BEGIN_JILEXTERN

JILString*		JILString_New(JILState* pState);
void			JILString_Delete(JILString* _this);
JILString*		JILString_Copy(const JILString* pSource);
void			JILString_Set(JILString* _this, const JILString* string);
void			JILString_Append(JILString* _this, const JILString* source);
void			JILString_Fill(JILString* _this, JILLong chr, JILLong length);
JILLong			JILString_Equal(const JILString* _this, const JILString* other);
JILLong			JILString_Compare(const JILString* _this, const JILString* other);
JILLong			JILString_CharAt(const JILString* _this, JILLong index);
JILLong			JILString_FindChar(const JILString* _this, JILLong chr, JILLong index);
JILLong			JILString_FindCharR(const JILString* _this, JILLong chr, JILLong index);
JILLong			JILString_FindString(const JILString* _this, const JILString* other, JILLong index);
JILLong			JILString_FindStringR(const JILString* _this, const JILString* other, JILLong index);
JILLong			JILString_BeginsWith(const JILString* _this, const JILString* other);
JILLong			JILString_EndsWith(const JILString* _this, const JILString* other);
JILLong			JILString_ContainsAt(const JILString* _this, const JILString* other, JILLong index);
JILLong			JILString_ContainsAnyOf(const JILString* _this, const JILArray* pArray);
JILLong			JILString_ContainsAllOf(const JILString* _this, const JILArray* pArray);

// The following functions directly modify the given string object, which is
// useful when using the string object from within C or C++.

void			JILString_SetSize(JILString* _this, JILLong newSize);
void			JILString_Assign(JILString* _this, const JILChar* str);
void			JILString_AppendCStr(JILString* _this, const JILChar* str);
void			JILString_AppendChar(JILString* _this, JILChar chr);
void			JILString_Clear(JILString* _this);
void			JILString_SubStr(JILString* _this, const JILString* source, JILLong index, JILLong length);
JILLong			JILString_SpanIncl(const JILString* _this, const JILString* pCharSet, JILLong index);
JILLong			JILString_SpanExcl(const JILString* _this, const JILString* pCharSet, JILLong index);
void			JILString_InsChr(JILString* _this, JILLong chr, JILLong index);
void			JILString_InsStr(JILString* _this, const JILString* source, JILLong index);
void			JILString_RemChrs(JILString* _this, JILLong index, JILLong length);

// The following functions are imported into JewelScript. All of them do
// not modify the given string object, but return a modified copy of it.

JILString*		JILString_SpanIncluding(const JILString* _this, const JILString* chrSet, JILLong index);
JILString*		JILString_SpanExcluding(const JILString* _this, const JILString* chrSet, JILLong index);
JILString*		JILString_Insert(const JILString* _this, const JILString* other, JILLong index);
JILString*		JILString_InsertChar(const JILString* _this, JILLong chr, JILLong index);
JILString*		JILString_Remove(const JILString* _this, JILLong index, JILLong length);
JILString*		JILString_ReplaceChar(const JILString* _this, JILLong schr, JILLong rchr);
JILString*		JILString_Replace(const JILString* _this, const JILString* search, const JILString* replace);
JILString*		JILString_ReplaceRange(const JILString* _this, JILLong start, JILLong length, const JILString* replace);
JILString*		JILString_Reverse(const JILString* _this);
JILString*		JILString_SubString(const JILString* _this, JILLong index, JILLong length);
JILString*		JILString_Trim(const JILString* _this);
JILString*		JILString_ToUpper(const JILString* _this);
JILString*		JILString_ToLower(const JILString* _this);
JILString*		JILString_EncodeURL(const JILString* _this, const JILString* exceptChars);
JILString*		JILString_DecodeURL(const JILString* _this, const JILString* exceptChars);
JILError		JILString_Process(const JILString* _this, JILHandle* pDelegate, JILHandle* pArgs, JILString** ppNew);
JILString*		JILString_Join(const JILArray* _array, const JILString* seperator);
JILArray*		JILString_Split(const JILString* _this, const JILString* seperators, JILBool bDiscard);
JILArray*		JILString_MatchString(const JILString* _this, const JILArray* pArray);
JILArray*		JILString_MatchArray(const JILString* _this, const JILArray* pArray);

//------------------------------------------------------------------------------
// JILString_String
//------------------------------------------------------------------------------
/// Returns a safe pointer to the string buffer; if the string is empty, a
/// pointer to an empty C-string will be returned.

JILINLINE const JILChar* JILString_String(const JILString* _this)	{ return (_this->length > 0) ? _this->string : ""; }

//------------------------------------------------------------------------------
// JILString_Length
//------------------------------------------------------------------------------
/// Return the length of a JILString.

JILINLINE JILLong JILString_Length(const JILString* _this)	{ return _this->length; }

//------------------------------------------------------------------------------
// JILString_LastChar
//------------------------------------------------------------------------------
/// Returns the last character of the string, or 0 if the string is empty.

JILINLINE JILLong JILString_LastChar(const JILString* _this)	{ return (JILByte)((_this->length > 0) ? _this->string[_this->length - 1] : 0); }

//------------------------------------------------------------------------------
// JILStringProc
//------------------------------------------------------------------------------
/// The main proc of the built-in string class.

JILError JILStringProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

END_JILEXTERN

#endif	// #ifndef JILSTRING_H
