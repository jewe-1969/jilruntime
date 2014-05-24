//------------------------------------------------------------------------------
// File: ntl_trex.cpp                                          (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description:
// ------------
//	A regular expression object for JewelScript.
//
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ntl_trex.h"
#include "jilstring.h"
#include "jilarray.h"
#include "jiltools.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------
// It is important to keep these index numbers in the same order as the function
// declarations in the class declaration string!

enum
{
	// constructors
	kCtor,
	kCctor,
	kConvString,

	// methods
	kMatch,
	kSearch1,
	kSearch2,
	kSearch3,
	kSearch4,
	kSlice,
	kReplace,
	kFormatMatch,
	kGetSubMatch,

	// accessors
	kSubMatchCount,
	kMatchStart,
	kMatchEnd,
	kMatchLength,
	kValid
};

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	TAG("Tiny-Rex, aka 'trex' is a regular expression class originally written by Alberto Demichelis. This native type wraps the original C code to make the class available in JewelScript.")
	"delegate		enumerator(trex regex);" TAG("Delegate type for functions that can be passed into the trex::search() method.")
	// constructors, convertor
	"method			trex(const string expr);" TAG("Constructs a new trex object from the given regular expression string.")
	"method			trex(const trex src);" TAG("Copy-constructs a new trex object from the given trex object.")
	"method string	convertor();" TAG("Converts the regular expression of this trex object back to a string.")
	// methods
	"method int		match(const string text);" TAG("Checks if the specified text contains any match to this regular expression. If no match is found, false is returned, otherwise true. Further information about the match can then be retrieved by getSubMatch(), formatMatch() or any of the match related properties.")
	"method int		search(const string text);" TAG("Finds and returns the zero-based position of the first match of the regular expression in the specified text. If no match is found, -1 is returned. Further information about the match can then be retrieved by getSubMatch(), formatMatch() or any of the match related properties.")
	"method int		search(const string text, const int start, const int length);" TAG("Finds and returns a match in the specified portion of the text. If the text in that section does not contain a match, returns -1.")
	"method array   search(const string text, const string format);" TAG("Returns an array of all matches to this regular expression in the specified text. If you specify a format string, each match will be formatted using that string before being added to the array. The format string can contain references to sub-matches.")
	"method 		search(const string text, enumerator fn);" TAG("Calls the specified delegate function or method for every match in the text. The trex object is passed to the delegate so it can examine each match by calling the match related properties.")
	"method array   slice(const string text);" TAG("Interprets this regular expression as a delimiter expression and based on it slices the given text into substrings. The substrings are returned in an array of strings.")
	"method	string	replace(const string text, const string replace);" TAG("Replaces all matches of this regular expression in the specified text by the specified string. The string can contain references to sub-matches.")
	"method string	formatMatch(const string format);" TAG("Returns a string formatted using this regular expression's matches. The format string can contain references to sub-matches.")
	"method string	getSubMatch(const int index);" TAG("Returns the sub-match with the specified index as a string. The sub-match 0 (zero) represents the whole match.")
	// read-only accessors
	"accessor int	subMatchCount();" TAG("Returns the number of sub-matches of this regular expression.")
	"accessor int	matchStart();" TAG("Returns the character position of the start of the match. If you have called getSubMatch() with an index greater than 0, this property reflects the properties of that sub-match, otherwise it will reflect the whole match.")
	"accessor int	matchEnd();" TAG("Returns the character position of the end of the match. If you have called getSubMatch() with an index greater than 0, this property reflects the properties of that sub-match, otherwise it will reflect the whole match.")
	"accessor int	matchLength();" TAG("Returns the length in characters of the match. If you have called getSubMatch() with an index greater than 0, this property reflects the properties of that sub-match, otherwise it will reflect the whole match.")
	"accessor int	valid();" TAG("Returns true if this regular expression object is valid. Returns false if you have constructed the object with a bad regular expression string.")
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"trex";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Tiny-Rex, aka 'trex' is a regular expression class originally written by Alberto Demichelis.";
static const char*	kTimeStamp		=	"08.10.2005";
static const int	kAuthorVersion	=	0x00000004;

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int TrexNew		(NTLInstance* pInst, NTrex** ppObject);
static int TrexDelete	(NTLInstance* pInst, NTrex* _this);
static int TrexCall		(NTLInstance* pInst, int funcID, NTrex* _this);

static void Trex_Update	(NTrex* _this, const char* pText);

//------------------------------------------------------------------------------
// our main proc
//------------------------------------------------------------------------------
// This is the function you will need to register with the JIL virtual machine.
// Whenever the virtual machine needs to communicate with your native type,
// it will call this proc.

int TrexProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut)
{
	int result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return TrexNew(pInst, (NTrex**) ppDataOut);
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			break;
		case NTL_CallMember:			return TrexCall(pInst, param, (NTrex*) pDataIn);
		case NTL_DestroyObject:			return TrexDelete(pInst, (NTrex*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;

		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return kAuthorVersion;
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetDeclString:			(*(const char**) ppDataOut) = kClassDeclaration; break;
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;

		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// TrexNew
//------------------------------------------------------------------------------
// Return a new instance of your class in ppObject.

static int TrexNew(NTLInstance* pInst, NTrex** ppObject)
{
	*ppObject = New_Trex( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// TrexDelete
//------------------------------------------------------------------------------
// Destroy the instance of your class given in pData

static int TrexDelete(NTLInstance* pInst, NTrex* _this)
{
	Delete_Trex( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// TrexCall
//------------------------------------------------------------------------------
// Called when the VM wants to call one of the methods of this class.
// Process parameters on from the stack by calling the NTLGetArg...() functions.
// The argument index indicates the position of an argument on the stack:
// The first argument is at index 0, the second at 1, and so on.
// To return a value, call one of the NTLReturn...() functions.
// The address of the object (this pointer) is passed in '_this'.

static int TrexCall(NTLInstance* pInst, int funcID, NTrex* _this)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILString* str;
	JILHandle* hObj;
	JILHandle* hArr;
	JILUnknown* vec;
	switch( funcID )
	{
		case kCtor:
			Trex_Create(_this, NTLGetArgString(ps, 0));
			break;
		case kCctor:
		{
			NTrex* pSrc;
			hObj = NTLGetArgHandle(ps, 0);
			pSrc = (NTrex*)NTLHandleToObject(ps, NTLInstanceTypeID(pInst), hObj);
			Trex_Create(_this, JILString_String(pSrc->pRegEx));
			NTLFreeHandle(ps, hObj);
			break;
		}
		case kConvString:
			NTLReturnString(ps, JILString_String(_this->pRegEx));
			break;
		case kMatch:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			NTLReturnInt(ps, Trex_Match(_this, vec));
			NTLFreeHandle(ps, hObj);
			break;
		case kSearch1:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			NTLReturnInt(ps, Trex_Search(_this, vec));
			NTLFreeHandle(ps, hObj);
			break;
		case kSearch2:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			NTLReturnInt(ps,
				Trex_SearchRange(
					_this,
					vec,
					NTLGetArgInt(ps, 1),
					NTLGetArgInt(ps, 2)));
			NTLFreeHandle(ps, hObj);
			break;
		case kSearch3:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			hArr = Trex_MultiSearch(_this, vec, NTLGetArgString(ps, 1));
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			NTLFreeHandle(ps, hObj);
			break;
		case kSearch4:
		{
			JILHandle* hThis;
			JILHandle* hDelegate;
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			hDelegate = NTLGetArgHandle(ps, 1);
			hThis = NTLNewWeakRefForObject(ps, NTLInstanceTypeID(pInst), _this);
			result = Trex_DelegateSearch(_this, vec, hThis, hDelegate);
			NTLFreeHandle(ps, hObj);
			NTLFreeHandle(ps, hDelegate);
			NTLFreeHandle(ps, hThis);
			break;
		}
		case kSlice:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			hArr = Trex_Slice(_this, vec);
			NTLReturnHandle(ps, hArr);
			NTLFreeHandle(ps, hArr);
			NTLFreeHandle(ps, hObj);
			break;
		case kReplace:
			hObj = NTLGetArgHandle(ps, 0);
			vec = NTLHandleToObject(ps, type_string, hObj);
			str = JILString_New(ps);
			Trex_Replace(_this, vec, NTLGetArgString(ps, 1), str);
			NTLReturnString(ps, JILString_String(str));
			JILString_Delete(str);
			NTLFreeHandle(ps, hObj);
			break;
		case kFormatMatch:
			str = JILString_New(ps);
			Trex_SubstSubMatch(_this, NTLGetArgString(ps, 0), str);
			NTLReturnString(ps, JILString_String(str));
			JILString_Delete(str);
			break;
		case kGetSubMatch:
			str = JILString_New(ps);
			Trex_SubMatch(_this, NTLGetArgInt(ps, 0), str);
			NTLReturnString(ps, JILString_String(str));
			JILString_Delete(str);
			break;
		case kSubMatchCount:
			if( _this->pTrex )
				NTLReturnInt(ps, trex_getsubexpcount(_this->pTrex));
			else
				NTLReturnInt(ps, 0);
			break;
		case kMatchStart:
			NTLReturnInt(ps, _this->matchStart);
			break;
		case kMatchEnd:
			NTLReturnInt(ps, _this->matchEnd);
			break;
		case kMatchLength:
			NTLReturnInt(ps, _this->matchEnd - _this->matchStart);
			break;
		case kValid:
			NTLReturnInt(ps, _this->pTrex ? JILTrue : JILFalse);
			break;
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// Trex_Update
//------------------------------------------------------------------------------
// Update the sub-match strings stored in the NTrex object.

static void Trex_Update	(NTrex* _this, const char* pText)
{
	int i;
	if( pText && _this->pRegEx )
	{
		TRexMatch subExp;
		for( i = 0; i < 10; i++ )
		{
			if( trex_getsubexp(_this->pTrex, i, &subExp) )
			{
				JILString_Fill(_this->pSubMatch[i], 32, subExp.len);
				JILStrncpy(_this->pSubMatch[i]->string, subExp.len + 1, subExp.begin, subExp.len);
				if( i == 0 )
				{
					_this->matchStart = (JILLong) (subExp.begin - pText);
					_this->matchEnd = _this->matchStart + subExp.len;
				}
			}
		}
	}
	else
	{
		for( i = 0; i < 10; i++ )
			JILString_Clear(_this->pSubMatch[i]);
		_this->matchStart = _this->matchEnd = 0;
	}
}

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------
// Construct an instance of class NTrex.

void Trex_Create (NTrex* _this, const JILChar* pRegEx)
{
	char* pDummy;
	int i;
	// trex_compile() can't handle empty strings, so
	if( *pRegEx )
		_this->pTrex = trex_compile(pRegEx, &pDummy);
	else
		_this->pTrex = NULL;
	_this->pRegEx = JILString_New(_this->pState);
	JILString_Assign(_this->pRegEx, pRegEx);
	for( i = 0; i < 10; i++ )
	{
		_this->pSubMatch[i] = JILString_New(_this->pState);
	}
}

//------------------------------------------------------------------------------
// Destroy
//------------------------------------------------------------------------------
// Destruct an instance of class NTrex.

void Trex_Destroy (NTrex* _this)
{
	int i;
	if( _this->pTrex )
		trex_free(_this->pTrex);
	if( _this->pRegEx )
		JILString_Delete(_this->pRegEx);
	for( i = 0; i < 10; i++ )
		JILString_Delete(_this->pSubMatch[i]);
}

//------------------------------------------------------------------------------
// Trex_Match
//------------------------------------------------------------------------------
// Returns true if the text exactly matches the regular expression.

JILLong Trex_Match (NTrex* _this, const JILString* pText)
{
	JILLong result = JILFalse;
	Trex_Update(_this, NULL);
	if( _this->pTrex
	&&	JILString_Length(pText)
	&&	trex_match(_this->pTrex, JILString_String(pText)) )
	{
		result = JILTrue;
		Trex_Update(_this, JILString_String(pText));
	}
	return result;
}

//------------------------------------------------------------------------------
// Trex_Search
//------------------------------------------------------------------------------
// Searches in the text for the regular expression and returns the starting
// index position. Additionally, start and end index position can be read from
// matchStart and matchEnd.

JILLong Trex_Search (NTrex* _this, const JILString* pText)
{
	JILLong result = -1;
	JILChar* pBegin = NULL;
	JILChar* pEnd = NULL;
	const JILChar* ptr = JILString_String(pText);
	Trex_Update(_this, NULL);
	if( _this->pTrex
	&&	JILString_Length(pText)
	&&	trex_search(_this->pTrex, ptr, &pBegin, &pEnd) )
	{
		result = (JILLong) (pBegin - ptr);
		Trex_Update(_this, JILString_String(pText));
	}
	return result;
}

//------------------------------------------------------------------------------
// Trex_SearchRange
//------------------------------------------------------------------------------
// Searches in the text for the regular expression and returns the starting
// index position. The search is started at the given start index position and
// ends when the given length has been reached.
// Start and end index position can also be read from matchStart and matchEnd.

JILLong Trex_SearchRange (NTrex* _this, const JILString* pText, JILLong start, JILLong length)
{
	JILLong result = -1;
	JILChar* pBegin = NULL;
	JILChar* pEnd = NULL;
	const JILChar* pTextBegin;
	const JILChar* pTextEnd;
	const JILChar* ptr = JILString_String(pText);
	JILLong len = JILString_Length(pText);
	if( start < 0 )
		start = 0;
	if( start >= len )
		return result;
	if( (start + length) > len )
		length = len - start;
	pTextBegin = ptr + start;
	pTextEnd = ptr + start + length;
	Trex_Update(_this, NULL);
	if( _this->pTrex && trex_searchrange(_this->pTrex, pTextBegin, pTextEnd, &pBegin, &pEnd) )
	{
		result = (JILLong) (pBegin - ptr);
		Trex_Update(_this, JILString_String(pText));
	}
	return result;
}

//------------------------------------------------------------------------------
// Trex_MultiSearch
//------------------------------------------------------------------------------
// Search all matches inside text and return them in an array of strings.
// The second string 'pFormat' specifies the format in which the individual
// result strings are written into the array. This result expression can contain
// sub-match identifiers ($0 - $9).

JILHandle* Trex_MultiSearch (NTrex* _this, const JILString* pText, const JILChar* pFormat)
{
	JILHandle* hArray;
	JILHandle* hString = 0;
	JILArray* pArray = NULL;
	JILString* pString = NULL;
	JILState* pState = _this->pState;
	JILLong start, length;

	pArray = JILArray_New(pState);
	start = 0;
	length = JILString_Length(pText);
	while( start < length )
	{
		start = Trex_SearchRange(_this, pText, start, length);	// length will be clipped
		if( start < 0 )
			break;
		pString = JILString_New(pState);
		Trex_SubstSubMatch(_this, pFormat, pString);
		hString = NTLNewHandleForObject(pState, type_string, pString);
		JILArray_ArrMove( pArray, hString );
		NTLFreeHandle(pState, hString);
		start = _this->matchEnd;
	}
	hArray = NTLNewHandleForObject( pState, type_array, pArray );
	Trex_Update(_this, NULL);
	return hArray;
}

//------------------------------------------------------------------------------
// Trex_DelegateSearch
//------------------------------------------------------------------------------
// Search all matches inside text and call delegate function for each match.

JILError Trex_DelegateSearch (NTrex* _this, const JILString* pText, JILHandle* hThis, JILHandle* hDelegate)
{
	JILError err = JIL_No_Exception;
	JILState* pState = _this->pState;
	JILHandle* pResult;
	JILLong start, length;

	start = 0;
	length = JILString_Length(pText);
	while( start < length )
	{
		start = Trex_SearchRange(_this, pText, start, length);	// length will be clipped
		if( start < 0 )
			break;
		pResult = JILCallFunction(pState, hDelegate, 1, kArgHandle, hThis);
		err = NTLHandleToError(pState, pResult);
		NTLFreeHandle(pState, pResult);
		if( err )
			break;
		start = _this->matchEnd;
	}
	Trex_Update(_this, NULL);
	return err;
}

//------------------------------------------------------------------------------
// Trex_Slice
//------------------------------------------------------------------------------
// Slice the string into sub-strings and return them as an array. The regexp
// defines the separators used to slice the array and are not included in the
// returned strings.

JILHandle* Trex_Slice (NTrex* _this, const JILString* pText)
{
	JILHandle* hArray;
	JILHandle* hString = 0;
	JILArray* pArray = NULL;
	JILString* pString = NULL;
	JILState* pState = _this->pState;
	JILLong start, prev, length;

	pArray = JILArray_New(pState);
	start = 0;
	length = JILString_Length(pText);
	for(;;)
	{
		prev = start;
		start = Trex_SearchRange(_this, pText, start, length);	// length will be clipped
		if( start < 0 )
		{
			pString = JILString_New(pState);
			JILString_SubStr(pString, pText, prev, length - prev);
			hString = NTLNewHandleForObject(pState, type_string, pString);
			JILArray_ArrMove( pArray, hString );
			NTLFreeHandle(pState, hString);
			break;
		}
		pString = JILString_New(pState);
		JILString_SubStr(pString, pText, prev, start - prev);
		hString = NTLNewHandleForObject(pState, type_string, pString);
		JILArray_ArrMove( pArray, hString );
		NTLFreeHandle(pState, hString);
		start = _this->matchEnd;
	}
	hArray = NTLNewHandleForObject( pState, type_array, pArray );
	Trex_Update(_this, NULL);
	return hArray;
}

//------------------------------------------------------------------------------
// Trex_Replace
//------------------------------------------------------------------------------
// Replace all matches in pText with pReplace and return pResult.

void Trex_Replace (NTrex* _this, const JILString* pText, const JILChar* pReplace, JILString* pResult)
{
	JILLong start, prev, length;
	JILString* pRepStr;
	JILString* pWork;
	JILState* pState = _this->pState;

	pWork = JILString_New(pState);
	pRepStr = JILString_New(pState);
	JILString_Clear(pResult);
	start = 0;
	length = JILString_Length(pText);
	for(;;)
	{
		prev = start;
		start = Trex_SearchRange(_this, pText, prev, length);
		if( start < 0 )
		{
			JILString_SubStr(pWork, pText, prev, length - prev);
			JILString_Append(pResult, pWork);
			break;
		}
		JILString_SubStr(pWork, pText, prev, start - prev);
		JILString_Append(pResult, pWork);
		Trex_SubstSubMatch(_this, pReplace, pRepStr);
		JILString_Append(pResult, pRepStr);
		start = _this->matchEnd;
	}
	Trex_Update(_this, NULL);
	JILString_Delete(pWork);
	JILString_Delete(pRepStr);
}

//------------------------------------------------------------------------------
// Trex_SubstSubMatch
//------------------------------------------------------------------------------
// Substitute symbolic references to sub-matches ($0, $1 ... $9) in the format
// string by the actual sub-match strings from the most recent search action and
// return the result in pResult. Any character that is not recognized as a
// reference to a sub-match, will be copied verbatim into the result string.

void Trex_SubstSubMatch(NTrex* _this, const JILChar* pFormat, JILString* pResult)
{
	int i, l, c;
	JILString* pWork = JILString_New(_this->pState);
	JILString_Clear(pResult);
	l = strlen(pFormat);
	for( i = 0; i < l; i++ )
	{
		if( pFormat[i] == '$' )
		{
			i++;
			if( i == l )
				break;
			c = ((unsigned char) pFormat[i]);
			JILString_Clear(pWork);
			if( c == '$' )
			{
				JILString_Assign(pWork, "$");
			}
			else if( c >= '0' && c <= '9' )
			{
				Trex_SubMatch(_this, c - '0', pWork);
			}
			JILString_Append(pResult, pWork);
		}
		else
		{
			JILString_InsChr(pResult, pFormat[i], JILString_Length(pResult));
		}
	}
	JILString_Delete(pWork);
}

//------------------------------------------------------------------------------
// Trex_SubMatch
//------------------------------------------------------------------------------
// Returns the string of the sub-match with the given index number. Index number
// 0 (zero) returns the global match, 1 thru 9 return the sub-match $1 - $9.

JILLong Trex_SubMatch (NTrex* _this, JILLong i, JILString* pOut)
{
	JILLong result = JILFalse;
	if( i >= 0 && i < 10 )
	{
		JILString_Set(pOut, _this->pSubMatch[i]);
		result = JILTrue;
	}
	return result;
}

//------------------------------------------------------------------------------
// New_Trex
//------------------------------------------------------------------------------
// Allocate an instance of class NTrex. Constructor function will be called
// separately.

NTrex* New_Trex (JILState* pState)
{
	NTrex* _this = (NTrex*) pState->vmMalloc(pState, sizeof(NTrex));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// Delete_Trex
//------------------------------------------------------------------------------
// Destroy an instance of class NTrex. Destructor function is called
// automatically.

void Delete_Trex (NTrex* _this)
{
	Trex_Destroy(_this);
	_this->pState->vmFree( _this->pState, _this );
}
