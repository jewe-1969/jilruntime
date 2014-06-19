//------------------------------------------------------------------------------
// File: JILString.c                                        (c) 2004 jewe.org
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
/// @file jilstring.c
/// The built-in string object the virtual machine uses. The built-in string is
/// a primitive data type, like the long and float types, and does only support
/// very basic operations. However, more functions might be added here in the
/// future, to make using and manipulating the string object from native typelibs
/// or the application using the jilruntime library easier.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilstring.h"
#include "jilarray.h"
#include "jiltools.h"
#include "jilapi.h"

//------------------------------------------------------------------------------
// string method index numbers
//------------------------------------------------------------------------------

enum
{
	// constructors
	kCtor,
	kCtorLong,
	kCtorFloat,

	// convertors
	kConvLong,
	kConvFloat,

	// accessors
	kLength,
	kLastChar,

	// methods
	kCharAt,
	kIndexOfChar,
	kIndexOfString,
	kLastIndexOfChar,
	kLastIndexOfString,
	kStartsWith,
	kEndsWith,
	kContainsAt,
	kContainsAnyOf,
	kContainsAllOf,
	kSpanIncluding,
	kSpanExcluding,
	kInsertChar,
	kInsertString,
	kRemove,
	kRemove2,
	kReplaceChar,
	kReplaceString,
	kReplaceRange,
	kReverse,
	kSubString1,
	kSubString2,
	kTrim,
	kToUpper,
	kToLower,
	kEncodeURL,
	kDecodeURL,
	kProcess,
	kClone,
	kSplit,
	kSplit2,
	kMatchString,
	kMatchArray,

	// functions
	kFill,
	kAscii,
	kFormat,
	kCompare,
	kJoin,
	kCharIsDigit,
	kCharIsLetter,
	kCharIsUpperCase,
	kCharIsLowerCase,
	kCharIsPunctuation,
	kCharIsLetterOrDigit,
	kCharIsControl,
	kCharIsWhitespace,
	kCharIsValidUrl,
	kCharIsValidPath,
	kIsEmpty
};

//------------------------------------------------------------------------------
// string class declaration
//------------------------------------------------------------------------------

static const JILChar* kClassDeclaration =
	TAG("This is the built-in string class. In general, a JewelScript string is an immutable object, meaning once created, it's state can't be altered. There is one exception to that rule: Operator += will actually modify the string object left from the operator. JewelScript strings are limited to 8-bit ANSI characters. Their length is limited to 2 gigabytes. Zero-bytes in the string are handled gracefully by the runtime. However, they may cause problems when passing strings to other native functions, so they are discouraged.")
	"delegate int		processor(int chr, var args);" TAG("Delegate type for the string::process() method.")
	"method				string();" TAG("Constructs a new, empty string.")
	"method				string(const int);" TAG("Constructs a new string representation of the specified integer number.")
	"method				string(const float);" TAG("Constructs a new string representation of the specified floating-point number.")
	"explicit int		convertor();" TAG("Tries to parse this string into an integer number. This conversion requires an explicit type cast to confirm that a conversion is wanted. If parsing the string fails, the result is 0.")
	"explicit float		convertor();" TAG("Tries to parse this string into a floating-point number. This conversion requires an explicit type cast to confirm that a conversion is wanted. If parsing the string fails, the result is 0.")
	"accessor int		length();" TAG("Returns the length of this string in characters.")
	"accessor int		lastChar();" TAG("Returns the last character in this string. If the string is empty, zero is returned.")
	"method int			charAt(const int index);" TAG("Returns the character at the specified zero-based index from this string. If the index is out of range, zero is returned.")
	"method int			indexOf(const int chr, const int index);" TAG("Returns the index of the first occurrence of the specified character in this string. The search starts at the given index. If the specified character was not found, -1 is returned.")
	"method int			indexOf(const string s, const int index);" TAG("Returns the index of the first occurrence of the specified substring in this string. The search starts at the given index. If the specified substring was not found, -1 is returned.")
	"method int			lastIndexOf(const int chr, const int index);" TAG("Returns the index of the last occurrence of the specified character in this string. The search starts at the given index. If the specified character was not found, -1 is returned.")
	"method int			lastIndexOf(const string s, const int index);" TAG("Returns the index of the last occurrence of the specified substring in this string. The search starts at the given index. If the specified substring was not found, -1 is returned.")
	"method int			startsWith(const string s);" TAG("Returns true if this string starts with the specified substring.")
	"method int			endsWith(const string s);" TAG("Returns true if this string ends with the specified substring.")
	"method int			containsAt(const int index, const string s);" TAG("Returns true if this string contains the specified substring at the specified index position.")
	"method int			containsAnyOf(const string[] keywords);" TAG("Returns true if this string contains any of the keywords in the given array, otherwise false. If you need to know which keyword matched the string and where, use matchString() instead.")
	"method int			containsAllOf(const string[] keywords);" TAG("Returns true if this string contains all of the keywords in the given array, otherwise false. If you need to know where the keywords matched the string, use matchString() instead.")
	"method string		spanIncluding(const string charSet, const int index);" TAG("Starting from index, returns all characters, which are included in the specified character set. The method stops at the first character that is not included in the character set.")
	"method string		spanExcluding(const string charSet, const int index);" TAG("Starting from index, returns all characters, which are NOT included in the specified character set. The method stops at the first character that is included in the character set.")
	"method string		insert(const int chr, const int index);" TAG("Inserts the specified character at the given index position into this string and returns the result as a new string.")
	"method string		insert(const string s, const int index);" TAG("Inserts the specified substring at the given index position into this string and returns the result as a new string.")
	"method string		remove(const int index);" TAG("Removes all characters from the given index position to the end of the string. The result is returned as a new string.")
	"method string		remove(const int index, const int length);" TAG("Removes all characters in the specified range from this string. The result is returned as a new string.")
	"method string		replace(const int schr, const int rchr);" TAG("Replaces all occurrences of the specified character by the given character. The result is returned as a new string.")
	"method string		replace(const string search, const string replace);" TAG("Replaces all occurrences of the specified search string by the given replace string. The result is returned as a new string.")
	"method string		replace(const int index, const int length, const string replace);" TAG("Replaces the specified character range by the replace string. The replace string may be longer or shorter than the source range. The result is returned as a new string.")
	"method string		reverse();" TAG("Reverses the order of all characters in this string and returns the result as a new string.")
	"method string		subString(const int index);" TAG("Returns all characters from the given starting index to the end of this string as a substring.")
	"method string		subString(const int index, const int length);" TAG("Returns the specified character range from this string as a substring.")
	"method string		trim();" TAG("Removes all control and white space characters from the beginning and end of this string. The result is returned as a new string.")
	"method string		toUpper();" TAG("Converts all characters of this string into upper case using the current ANSI locale and returns the result as a new string.")
	"method string		toLower();" TAG("Converts all characters of this string into lower case using the current ANSI locale and returns the result as a new string.")
	"method string		encodeUrl(const string except);" TAG("Encodes all invalid URL characters in this string using %hh entities. The optional character set 'except' can list characters which should NOT be encoded. The result is returned as a new string.")
	"method string		decodeUrl(const string except);" TAG("Decodes all URL entities in the format %hh in this string. The optional character set 'except' can list characters which should NOT be decoded. The result is returned as a new string.")
	"method string		process(processor fn, var args);" TAG("Calls the specified delegate for all characters in this string. The delegate can convert the given character or return zero. All non-zero results of the delegate will be concatenated to a new string that is then returned.")
	"method string		clone();" TAG("Returns a true copy of this string.")
	"method string[]	split(const string separators);" TAG("Splits this string into an array of tokens based on the given set of delimiter characters. The returned array will contain all text between delimiters, but not the delimiters itself. A delimiter directly following another delimiter will produce an empty string element. If this string does not match any delimiter, the resulting array will contain a single element, which is a copy of this string.")
	"method string[]	split(const string separators, const int discard);" TAG("Splits this string into an array of tokens based on the given set of delimiter characters. The returned array will contain all text between delimiters, but not the delimiters itself. If 'discard' is 'false', a delimiter directly following another delimiter will produce an empty string element in the array. If set to 'true', empty strings will not be added to the array. If this string does not match any delimiter, the resulting array will contain a single element, which is a copy of this string.")
	"method match[]		matchString(const string[] keywords);" TAG("Treats the given array as a list of keywords and searches this string for occurrences of these keywords. Any matches are returned as an array of match instances. If no match is found, an array with zero length is returned.")
	"method match[]		matchArray(const string[] strings);" TAG("Treats this string as a keyword and searches the given array for occurrences of the keyword. Any matches are returned as an array of match instances. If no match is found, an array with zero length is returned.")
	"function string	fill(const int chr, const int length);" TAG("Creates a string filled with the specified amount of the specified character.")
	"function string	ascii(const int chr);" TAG("Returns a string containing the specified ANSI character.")
	"function string	format(const string format, const var v);" TAG("Creates a formatted string using ANSI format specifiers. Multiple arguments may be passed to this function by directly passing an array expression:<pre>string str = string::format(\\\"%s, %d, %f\\\", {x, y, z})</pre>")
	"function int		compare(const var value1, const var value2);" TAG("Compares two strings. This function can be used as comparator delegate for the list::sort() and array::sort() methods.")
	"function string	join(const string[] values, const string seperator);" TAG("Concatenates all elements from the given string array into one string. The elements will be seperated by the specified seperator string.")
	"function int		charIsDigit(const int c);" TAG("Returns true if the specified character is a digit.")
	"function int		charIsLetter(const int c);" TAG("Returns true if the specified character is a letter.")
	"function int		charIsUpperCase(const int c);" TAG("Returns true if the specified character is an upper case letter.")
	"function int		charIsLowerCase(const int c);" TAG("Returns true if the specified character is a lower case letter.")
	"function int		charIsPunctuation(const int c);" TAG("Returns true if the specified character is a puctuation character.")
	"function int		charIsLetterOrDigit(const int c);" TAG("Returns true if the specified character is a letter or digit.")
	"function int		charIsControl(const int c);" TAG("Returns true if the specified character is a control character.")
	"function int		charIsWhitespace(const int c);" TAG("Returns true if the specified character is a white space character.")
	"function int		charIsValidForUrl(const int c);" TAG("Returns true if the specified character is a valid URL character.")
	"function int		charIsValidForPath(const int c);" TAG("Returns true if the specified character is a valid file name / path name character.")
	"function int		isEmpty(const string s);" TAG("Returns true if the specified string is null or empty.")
;

//------------------------------------------------------------------------------
// string class constants
//------------------------------------------------------------------------------

static const JILChar*	kClassName		=	"string";
static const JILChar*	kPackageList	=	"string::match";
static const JILChar*	kAuthorName		=	"www.jewe.org";
static const JILChar*	kAuthorString	=	"Built-in string class for JewelScript.";
static const JILChar*	kTimeStamp		=	"08/30/2007";

static const JILChar*	kValidUrlChars		=	"_&,/:;=@#%.$+-?[]()ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static const JILChar*	kInvalidPathChars	=	"\\\"/:*?<>|";

JILEXTERN JILError JILStringMatchProc(NTLInstance*, JILLong, JILLong, JILUnknown*, JILUnknown**);
JILHandle* JILStringMatch_Create(JILState* pVM, JILLong start, JILLong length, JILLong index);

//------------------------------------------------------------------------------
// forward declare proc message handler functions
//------------------------------------------------------------------------------

static JILError StringRegister		(JILState* pVM);
static JILError StringNew			(NTLInstance* pInst, JILString** ppObject);
static JILError StringCallStatic	(NTLInstance* pInst, JILLong funcID);
static JILError StringCallMember	(NTLInstance* pInst, JILLong funcID, JILString* _this);
static JILError StringDelete		(NTLInstance* pInst, JILString* _this);

//------------------------------------------------------------------------------
// JILStringProc
//------------------------------------------------------------------------------

JILError JILStringProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut)
{
	JILError result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				return StringRegister((JILState*) pDataIn);
		case NTL_Initialize:			break;
		case NTL_NewObject:				return StringNew(pInst, (JILString**) ppDataOut);
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return StringCallStatic(pInst, param);
		case NTL_CallMember:			return StringCallMember(pInst, param, (JILString*) pDataIn);
		case NTL_DestroyObject:			return StringDelete(pInst, (JILString*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;

		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return NTLRevisionToLong(JIL_LIBRARY_VERSION);
		case NTL_GetClassName:			(*(const JILChar**) ppDataOut) = kClassName; break;
		case NTL_GetPackageString:		(*(const char**) ppDataOut) = kPackageList; break;
		case NTL_GetDeclString:			(*(const JILChar**) ppDataOut) = kClassDeclaration; break;
		case NTL_GetBuildTimeStamp:		(*(const JILChar**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const JILChar**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const JILChar**) ppDataOut) = kAuthorString; break;

		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// StringRegister
//------------------------------------------------------------------------------

static JILError StringRegister(JILState* pVM)
{
	JILError err = JILRegisterNativeType( pVM, JILStringMatchProc );
	return err;
}

//------------------------------------------------------------------------------
// StringNew
//------------------------------------------------------------------------------

static JILError StringNew(NTLInstance* pInst, JILString** ppObject)
{
	*ppObject = JILString_New( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// StringCallStatic
//------------------------------------------------------------------------------

static JILError StringCallStatic(NTLInstance* pInst, JILLong funcID)
{
	JILError result = JIL_No_Exception;
	JILHandle* hStr;
	JILString* pStr;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILLong chr;

	switch( funcID )
	{
		case kFill:
			pStr = JILString_New(ps);
			JILString_Fill(pStr,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kAscii:
			pStr = JILString_New(ps);
			JILString_Fill(pStr, NTLGetArgInt(ps, 0), 1);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kFormat:
		{
			JILString* pFormat;
			JILHandle* pHandle;
			pStr = JILString_New(ps);
			pFormat = NTLGetArgObject(ps, 0, type_string);
			if( pFormat )
			{
				if( NTLGetArgTypeID(ps, 1) == type_array )
				{
					JILArray* pArray = NTLGetArgObject(ps, 1, type_array);
					if( pArray )
					{
						JILString_Delete(pStr);
						pStr = JILArray_Format(pArray, pFormat);
					}
				}
				else
				{
					pHandle = NTLGetArgHandle(ps, 1);
					JILArrayHandleToStringF(ps, pStr, pFormat, pHandle);
					NTLFreeHandle(ps, pHandle);
				}
			}
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		}
		case kCompare:
			NTLReturnInt(ps, JILString_Compare(
				NTLGetArgObject(ps, 0, type_string),
				NTLGetArgObject(ps, 1, type_string)));
			break;
		case kJoin:
		{
			JILArray* pArray;
			JILString* pSeperator;
			pArray = NTLGetArgObject(ps, 0, type_array);
			pSeperator = NTLGetArgObject(ps, 1, type_string);
			pStr = JILString_Join(pArray, pSeperator);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		}
		case kCharIsDigit:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, isdigit(chr));
			break;
		case kCharIsLetter:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, isupper(chr) || islower(chr));
			break;
		case kCharIsUpperCase:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, isupper(chr));
			break;
		case kCharIsLowerCase:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, islower(chr));
			break;
		case kCharIsPunctuation:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, ispunct(chr));
			break;
		case kCharIsLetterOrDigit:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, isalnum(chr));
			break;
		case kCharIsControl:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, iscntrl(chr));
			break;
		case kCharIsWhitespace:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, isspace(chr));
			break;
		case kCharIsValidUrl:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, strchr(kValidUrlChars, chr) != NULL);
			break;
		case kCharIsValidPath:
			chr = NTLGetArgInt(ps, 0);
			NTLReturnInt(ps, strchr(kInvalidPathChars, chr) == NULL);
			break;
		case kIsEmpty:
			pStr = NTLGetArgObject(ps, 0, type_string);
			NTLReturnInt(ps, (pStr == NULL || pStr->length == 0));
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// StringCallMember
//------------------------------------------------------------------------------

static JILError StringCallMember(NTLInstance* pInst, JILLong funcID, JILString* _this)
{
	JILError result = JIL_No_Exception;
	JILHandle* hStr;
	JILString* pStr;
	JILState* ps = NTLInstanceGetVM(pInst);

	switch( funcID )
	{
		case kCtor:
			// nothing to do in standard ctor
			break;
		case kCtorLong:
		{
			JILChar buf[32];
			JILSnprintf(buf, 32, "%d", NTLGetArgInt(ps, 0));
			JILString_Assign(_this, buf);
			break;
		}
		case kCtorFloat:
		{
			JILChar buf[32];
			JILSnprintf(buf, 32, "%.15g", NTLGetArgFloat(ps, 0));
			JILString_Assign(_this, buf);
			break;
		}
		case kConvLong:
		{
			char* dummy;
			NTLReturnInt(ps, strtol(JILString_String(_this), &dummy, 10));
			break;
		}
		case kConvFloat:
		{
			NTLReturnFloat(ps, atof(JILString_String(_this)));
			break;
		}
		case kLength:
			NTLReturnInt(ps, JILString_Length(_this));
			break;
		case kLastChar:
			NTLReturnInt(ps, JILString_LastChar(_this));
			break;
		case kCharAt:
			NTLReturnInt(ps, JILString_CharAt(_this, NTLGetArgInt(ps, 0)));
			break;
		case kIndexOfChar:
			NTLReturnInt(ps,
				JILString_FindChar(_this,
					(JILChar) NTLGetArgInt(ps, 0),
					NTLGetArgInt(ps, 1)));
			break;
		case kLastIndexOfChar:
			NTLReturnInt(ps,
				JILString_FindCharR(_this,
					(JILChar) NTLGetArgInt(ps, 0),
					NTLGetArgInt(ps, 1)));
			break;
		case kIndexOfString:
			NTLReturnInt(ps,
				JILString_FindString(_this,
					NTLGetArgObject(ps, 0, type_string),
					NTLGetArgInt(ps, 1)));
			break;
		case kLastIndexOfString:
			NTLReturnInt(ps,
				JILString_FindStringR(_this,
					NTLGetArgObject(ps, 0, type_string),
					NTLGetArgInt(ps, 1)));
			break;
		case kStartsWith:
			NTLReturnInt(ps,
				JILString_BeginsWith(_this,
					NTLGetArgObject(ps, 0, type_string)));
			break;
		case kEndsWith:
			NTLReturnInt(ps,
				JILString_EndsWith(_this,
					NTLGetArgObject(ps, 0, type_string)));
			break;
		case kContainsAt:
			NTLReturnInt(ps,
				JILString_ContainsAt(_this,
					NTLGetArgObject(ps, 0, type_string),
					NTLGetArgInt(ps, 1)));
			break;
		case kContainsAnyOf:
			NTLReturnInt(ps,
				JILString_ContainsAnyOf(_this,
					NTLGetArgObject(ps, 0, type_array)));
			break;
		case kContainsAllOf:
			NTLReturnInt(ps,
				JILString_ContainsAllOf(_this,
					NTLGetArgObject(ps, 0, type_array)));
			break;
		case kSpanIncluding:
			pStr = JILString_SpanIncluding(_this,
				NTLGetArgObject(ps, 0, type_string),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSpanExcluding:
			pStr = JILString_SpanExcluding(_this,
				NTLGetArgObject(ps, 0, type_string),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kInsertChar:
			pStr = JILString_InsertChar(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kInsertString:
			pStr = JILString_Insert(_this,
				NTLGetArgObject(ps, 0, type_string),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kRemove:
			pStr = JILString_Remove(_this,
				NTLGetArgInt(ps, 0),
				_this->length);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kRemove2:
			pStr = JILString_Remove(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kReplaceChar:
			pStr = JILString_ReplaceChar(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kReplaceString:
			pStr = JILString_Replace(_this,
				NTLGetArgObject(ps, 0, type_string),
				NTLGetArgObject(ps, 1, type_string));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kReplaceRange:
			pStr = JILString_ReplaceRange(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1),
				NTLGetArgObject(ps, 2, type_string));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kReverse:
			pStr = JILString_Reverse(_this);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSubString1:
			pStr = JILString_SubString(_this,
				NTLGetArgInt(ps, 0),
				_this->length);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSubString2:
			pStr = JILString_SubString(_this,
				NTLGetArgInt(ps, 0),
				NTLGetArgInt(ps, 1));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kTrim:
			pStr = JILString_Trim(_this);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kToUpper:
			pStr = JILString_ToUpper(_this);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kToLower:
			pStr = JILString_ToLower(_this);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kEncodeURL:
			pStr = JILString_EncodeURL(_this, NTLGetArgObject(ps, 0, type_string));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kDecodeURL:
			pStr = JILString_DecodeURL(_this, NTLGetArgObject(ps, 0, type_string));
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kProcess:
		{
			JILHandle* hDel = NTLGetArgHandle(ps, 0);
			JILHandle* hArg = NTLGetArgHandle(ps, 1);
			result = JILString_Process(_this, hDel, hArg, &pStr);
			if( result == JIL_No_Exception )
			{
				hStr = NTLNewHandleForObject(ps, type_string, pStr);
				NTLReturnHandle(ps, hStr);
				NTLFreeHandle(ps, hStr);
			}
			NTLFreeHandle(ps, hArg);
			NTLFreeHandle(ps, hDel);
			break;
		}
		case kClone:
			pStr = JILString_Copy(_this);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSplit:
		{
			JILString* pSeperators = NTLGetArgObject(ps, 0, type_string);
			JILArray* pArray = JILString_Split(_this, pSeperators, JILFalse);
			JILHandle* hArray = NTLNewHandleForObject(ps, type_array, pArray);
			NTLReturnHandle(ps, hArray);
			NTLFreeHandle(ps, hArray);
			break;
		}
		case kSplit2:
		{
			JILString* pSeperators = NTLGetArgObject(ps, 0, type_string);
			JILLong bDiscard = NTLGetArgInt(ps, 1);
			JILArray* pArray = JILString_Split(_this, pSeperators, bDiscard);
			JILHandle* hArray = NTLNewHandleForObject(ps, type_array, pArray);
			NTLReturnHandle(ps, hArray);
			NTLFreeHandle(ps, hArray);
			break;
		}
		case kMatchString:
		{
			JILArray* pArray = JILString_MatchString(_this, NTLGetArgObject(ps, 0, type_array));
			JILHandle* hArray = NTLNewHandleForObject(ps, type_array, pArray);
			NTLReturnHandle(ps, hArray);
			NTLFreeHandle(ps, hArray);
			break;
		}
		case kMatchArray:
		{
			JILArray* pArray = JILString_MatchArray(_this, NTLGetArgObject(ps, 0, type_array));
			JILHandle* hArray = NTLNewHandleForObject(ps, type_array, pArray);
			NTLReturnHandle(ps, hArray);
			NTLFreeHandle(ps, hArray);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// StringDelete
//------------------------------------------------------------------------------

static JILError StringDelete(NTLInstance* pInst, JILString* _this)
{
	JILString_Delete( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// global constants
//------------------------------------------------------------------------------

static const JILLong kStringAllocGrain = 32; ///< When the string resizes, it will increase it's size this number of bytes at once; increasing this value will increased performance as well as memory spoilage

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static JILString* JILStringPreAlloc(JILState* pState, JILLong length);
static void JILStringReAlloc(JILString* _this, JILLong newSize, JILLong keepData);
static void JILStringDeAlloc(JILString* _this);

//------------------------------------------------------------------------------
// JILString_New
//------------------------------------------------------------------------------
/// Allocate a new, empty string object.

JILString* JILString_New(JILState* pState)
{
	JILString* _this = (JILString*) pState->vmMalloc(pState, sizeof(JILString));
	memset(_this, 0, sizeof(JILString));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// JILString_Delete
//------------------------------------------------------------------------------
/// Destroy a string object.

void JILString_Delete(JILString* _this)
{
	if( _this )
	{
		JILStringDeAlloc(_this);
		_this->pState->vmFree( _this->pState, _this );
	}
}

//------------------------------------------------------------------------------
// JILString_Copy
//------------------------------------------------------------------------------
/// Create a copy from a given string object.

JILString* JILString_Copy(const JILString* pSource)
{
	JILString* _this = JILString_New(pSource->pState);

	JILStringReAlloc(_this, JILString_Length(pSource), JILFalse);
	if( JILString_Length(pSource) )
	{
		memcpy(_this->string, JILString_String(pSource), JILString_Length(pSource));
	}
	_this->string[_this->length] = 0;
	return _this;
}

//------------------------------------------------------------------------------
// JILString_Set
//------------------------------------------------------------------------------
/// Assign the contents of a source string to this string.

void JILString_Set(JILString* _this, const JILString* pSource)
{
	if( JILString_Length(pSource) )
	{
		JILStringReAlloc(_this, JILString_Length(pSource), JILFalse);
		memcpy(_this->string, JILString_String(pSource), JILString_Length(pSource));
		_this->string[_this->length] = 0;
	}
	else
	{
		JILStringDeAlloc(_this);
	}
}

//------------------------------------------------------------------------------
// JILString_Append
//------------------------------------------------------------------------------
/// Append the contents of a source string to this string.

void JILString_Append(JILString* _this, const JILString* pSource)
{
	JILLong oldLen;
	if( JILString_Length(pSource) )
	{
		// clone, if given object is this
		JILString* pSrc = (JILString*) pSource;
		JILBool bThis = (pSrc == _this);
		if( bThis )
			pSrc = JILString_Copy(_this);
		// now we can continue
		oldLen = _this->length;
		JILStringReAlloc(_this, _this->length + JILString_Length(pSrc), JILTrue);
		memcpy( _this->string + oldLen, JILString_String(pSrc), JILString_Length(pSrc) );
		_this->string[_this->length] = 0;
		// destruct clone
		if( bThis )
			JILString_Delete(pSrc);
	}
}

//------------------------------------------------------------------------------
// JILString_Fill
//------------------------------------------------------------------------------
/// Fill this string with the given number of characters. This is often used to
/// reserve a certain number of bytes in the string buffer.

void JILString_Fill(JILString* _this, JILLong chr, JILLong length)
{
	if( length > 0 )
	{
		JILStringReAlloc(_this, length, JILFalse);
		memset(_this->string, chr, length);
		_this->string[length] = 0;
	}
	else
	{
		JILStringDeAlloc(_this);
	}
}

//------------------------------------------------------------------------------
// JILString_Equal
//------------------------------------------------------------------------------
/// Returns true if both strings are equal, otherwise false.

JILLong JILString_Equal(const JILString* _this, const JILString* other)
{
	return (strcmp(JILString_String(_this), JILString_String(other)) == 0);
}

//------------------------------------------------------------------------------
// JILString_Compare
//------------------------------------------------------------------------------
/// Returns < 0 if "str2" is smaller than "str1", 0 if they are equal, and
/// > 0 if "str2" is greater than "str1".

JILLong JILString_Compare(const JILString* str1, const JILString* str2)
{
	if (str1 == NULL && str2 == NULL)
		return 0;
	else if (str1 == NULL && str2 != NULL)
		return 1;
	else if (str1 != NULL && str2 == NULL)
		return -1;
	else
		return strcmp(JILString_String(str1), JILString_String(str2));
}

//------------------------------------------------------------------------------
// JILString_CharAt
//------------------------------------------------------------------------------
/// Returns the character at the given position or 0 if the position exceeds the
/// length of the string.

JILLong JILString_CharAt(const JILString* _this, JILLong index)
{
	return (JILByte)((index < _this->length) ? _this->string[index] : 0);
}

//------------------------------------------------------------------------------
// JILString_FindChar
//------------------------------------------------------------------------------
/// Find a character in this string, searching from the given start position,
/// advancing right, to the end of the string.

JILLong JILString_FindChar(const JILString* _this, JILLong chr, JILLong index)
{
	JILLong result = -1;
	if( index < 0 )
		index = 0;
	if( index < _this->length )
	{
		char* pos = strchr( _this->string + index, chr );
		if( pos )
		{
			result = (JILLong) (pos - _this->string);
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_FindCharR
//------------------------------------------------------------------------------
/// Find a character in this string, searching from the given start position,
/// advancing left, to the start of the string.

JILLong JILString_FindCharR(const JILString* _this, JILLong chr, JILLong index)
{
	JILLong result = -1;
	if( _this->length && index >= 0 )
	{
		char* pos;
		if( index >= _this->length )
			index = _this->length - 1;
		for( pos = _this->string + index; pos >= _this->string; pos-- )
		{
			if( *pos == chr )
			{
				result = (JILLong) (pos - _this->string);
				break;
			}
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_FindString
//------------------------------------------------------------------------------
/// Find a sub string in this string, searching from the given start position,
/// advancing right, to the end of the string.

JILLong JILString_FindString(const JILString* _this, const JILString* other, JILLong index)
{
	JILLong result = -1;
	if( index < 0 )
		index = 0;
	if( index < _this->length )
	{
		char* pos = strstr( _this->string + index, JILString_String(other) );
		if( pos )
		{
			result = (JILLong) (pos - _this->string);
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_FindStringR
//------------------------------------------------------------------------------
/// Find a sub string in this string, searching from the given start position,
/// advancing left, to the start of the string.

JILLong JILString_FindStringR(const JILString* _this, const JILString* other, JILLong index)
{
	JILLong result = -1;
	index = _this->length - index;
	if( index < 0 )
		index = 0;
	if( index < _this->length )
	{
		char* pos;
		JILString* thisCopy = JILString_Reverse(_this);
		JILString* othrCopy = JILString_Reverse(other);
		pos = strstr( thisCopy->string + index, JILString_String(othrCopy) );
		if( pos )
		{
			pos += othrCopy->length;
			result = thisCopy->length - ((JILLong)(pos - thisCopy->string));
		}
		JILString_Delete(thisCopy);
		JILString_Delete(othrCopy);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_BeginsWith
//------------------------------------------------------------------------------
// Checks if the string begins with the given sub-string.

JILLong JILString_BeginsWith(const JILString* _this, const JILString* other)
{
	return JILString_ContainsAt(_this, other, 0);
}

//------------------------------------------------------------------------------
// JILString_EndsWith
//------------------------------------------------------------------------------
// Checks if the string ends with the given sub-string.

JILLong JILString_EndsWith(const JILString* _this, const JILString* other)
{
	return JILString_ContainsAt(_this, other, _this->length - other->length);
}

//------------------------------------------------------------------------------
// JILString_ContainsAt
//------------------------------------------------------------------------------
// Checks if this string matches the given string at the given position.

JILLong JILString_ContainsAt(const JILString* _this, const JILString* other, JILLong index)
{
	JILLong result = JILFalse;
	if( index >= 0 &&
		index < _this->length &&
		other->length > 0 &&
		other->length + index <= _this->length )
	{
		if( strncmp(_this->string + index, other->string, other->length) == 0 )
			result = JILTrue;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_ContainsAnyOf
//------------------------------------------------------------------------------
// Checks if ANY of the keywords in the given array occur in this string.

JILLong JILString_ContainsAnyOf(const JILString* _this, const JILArray* pArray)
{
	if( JILString_Length(_this) > 0 )
	{
		JILLong i;
		for( i = 0; i < pArray->size; i++ )
		{
			JILString* pStr = (JILString*)NTLHandleToObject(_this->pState, type_string, pArray->ppHandles[i]);
			if( pStr != NULL && JILString_Length(pStr) > 0 )
			{
				if( strstr(JILString_String(_this), JILString_String(pStr)) != NULL )
					return JILTrue;
			}
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// JILString_ContainsAllOf
//------------------------------------------------------------------------------
// Checks if ALL of the keywords in the given array occur in this string.

JILLong JILString_ContainsAllOf(const JILString* _this, const JILArray* pArray)
{
	JILLong i;
	for( i = 0; i < pArray->size; i++ )
	{
		JILString* pStr = (JILString*)NTLHandleToObject(_this->pState, type_string, pArray->ppHandles[i]);
		if( pStr != NULL && JILString_Length(pStr) > 0 )
		{
			if( strstr(JILString_String(_this), JILString_String(pStr)) == NULL )
				return JILFalse;
		}
	}
	return JILTrue;
}

//------------------------------------------------------------------------------
// JILString_SetSize
//------------------------------------------------------------------------------
/// Resize this string to the given number of characters. If the new length is
/// smaller than the current length, the exceeding characters will be freed.
/// All existing characters still fitting into the resized string will be kept.

void JILString_SetSize(JILString* _this, JILLong len)
{
	JILStringReAlloc(_this, len, JILTrue);
}

//------------------------------------------------------------------------------
// JILString_Assign
//------------------------------------------------------------------------------
/// Assign a c-string to this string.

void JILString_Assign(JILString* _this, const JILChar* str)
{
	if( str )
	{
		JILLong len = strlen(str);
		JILStringReAlloc(_this, len, JILFalse);
		memcpy(_this->string, str, len);
		_this->string[_this->length] = 0;
	}
	else
	{
		JILStringDeAlloc(_this);
	}
}

//------------------------------------------------------------------------------
// JILString_AppendCStr
//------------------------------------------------------------------------------
/// Append a c-string to this string.

void JILString_AppendCStr(JILString* _this, const JILChar* str)
{
	JILLong oldLen, len;
	if( str && *str )
	{
		len = strlen(str);
		oldLen = _this->length;
		JILStringReAlloc(_this, _this->length + len, JILTrue);
		memcpy( _this->string + oldLen, str, len );
		_this->string[_this->length] = 0;
	}
}

//------------------------------------------------------------------------------
// JILString_AppendChar
//------------------------------------------------------------------------------
/// Append a single character to this string.

void JILString_AppendChar(JILString* _this, JILChar chr)
{
	JILChar* pStr;
	if( (_this->length + 2) >= _this->maxLength )				// + 2 because termination
		JILStringReAlloc(_this, _this->length + 1, JILTrue);	// + 1 because function accounts for termination
	pStr = _this->string + _this->length;
	*pStr++ = chr;
	*pStr = 0;
	_this->length++;
}

//------------------------------------------------------------------------------
// JILString_Clear
//------------------------------------------------------------------------------
/// Clear this string. This function directly modifies the given string.

void JILString_Clear(JILString* _this)
{
	JILStringDeAlloc(_this);
}

//------------------------------------------------------------------------------
// JILString_SubStr
//------------------------------------------------------------------------------
/// Get a sub string from this string, starting at position 'index', extracting
/// 'length' number of characters. This function differs from JILString_SubString
/// only in that this function assigns the result of the operation back to this
/// string.

void JILString_SubStr(JILString* _this, const JILString* source, JILLong index, JILLong length)
{
	// clone, if given object is this
	const JILString* pSrc = source;
	JILLong bThis = (pSrc == _this);
	if( bThis )
		pSrc = JILString_Copy(_this);
	// now we can continue
	if( index < 0 )
		index = 0;
	if( (index >= pSrc->length) || (length <= 0) )
	{
		JILStringDeAlloc(_this);
	}
	else
	{
		if( (index + length) > pSrc->length )
			length = pSrc->length - index;
		JILStringReAlloc(_this, length, JILFalse);
		memcpy(_this->string, JILString_String(pSrc) + index, length);
		_this->string[_this->length] = 0;
	}
	// destruct clone
	if( bThis )
		JILString_Delete( (JILString*) pSrc );
}

//------------------------------------------------------------------------------
// JILString_SpanIncl
//------------------------------------------------------------------------------
/// Count all characters from the start position, that are included in the given
/// character-set. Returns the number of characters.

JILLong JILString_SpanIncl(const JILString* _this, const JILString* pCharSet, JILLong index)
{
	JILLong result = 0;
	if( index < 0 )
		index = 0;
	if( index < JILString_Length(_this) )
	{
		result = strspn( JILString_String(_this) + index, JILString_String(pCharSet) );
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_SpanExcl
//------------------------------------------------------------------------------
/// Count all characters from the start position, that are not included in the
/// given character-set. Returns the number of characters.

JILLong JILString_SpanExcl(const JILString* _this, const JILString* pCharSet, JILLong index)
{
	JILLong result = 0;
	if( index < 0 )
		index = 0;
	if( index < JILString_Length(_this) )
	{
		result = strcspn( JILString_String(_this) + index, JILString_String(pCharSet) );
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_InsChr
//------------------------------------------------------------------------------
/// Insert a character at the given position in this string. As opposed to
/// JILString_InsertChar, which returns a modified copy, this function directly
/// modifies this string.

void JILString_InsChr(JILString* _this, JILLong chr, JILLong index)
{
	JILLong oldLen;
	char* pOldStr;
	if( index < 0 )
		index = 0;
	if( index > _this->length )
		index = _this->length;
	// save current string
	oldLen = _this->length;
	pOldStr = _this->string;
	_this->string = NULL;
	_this->length = 0;
	_this->maxLength = 0;
	// allocate a new buffer
	JILStringReAlloc(_this, oldLen + 1, JILFalse);
	// copy left part
	if( pOldStr )
		memcpy( _this->string, pOldStr, index );
	// copy middle part
	_this->string[index] = (char) chr;
	// copy right part
	if( pOldStr )
		memcpy( _this->string + index + 1, pOldStr + index, oldLen - index );
	_this->string[_this->length] = 0;
	// destruct old string
	if( pOldStr )
		_this->pState->vmFree( _this->pState, pOldStr );
}

//------------------------------------------------------------------------------
// JILString_InsStr
//------------------------------------------------------------------------------
/// Insert a string into this string. This function directly modifies the given
/// string.

void JILString_InsStr(JILString* _this, const JILString* source, JILLong index)
{
	if( JILString_Length(source) )
	{
		JILLong oldLen;
		char* pOldStr;
		// clone, if given object is this
		const JILString* pSrc = source;
		JILLong bThis = (pSrc == _this);
		if( bThis )
			pSrc = JILString_Copy(_this);
		// now we can continue
		if( index < 0 )
			index = 0;
		if( index > _this->length )
			index = _this->length;
		// save current string
		oldLen = _this->length;
		pOldStr = _this->string;
		_this->string = NULL;
		_this->length = 0;
		_this->maxLength = 0;
		// allocate a new buffer
		JILStringReAlloc(_this, oldLen + JILString_Length(pSrc), JILFalse);
		// copy left part
		if( pOldStr )
			memcpy( _this->string, pOldStr, index );
		// copy middle part
		memcpy( _this->string + index, JILString_String(pSrc), JILString_Length(pSrc) );
		// copy right part
		if( pOldStr )
			memcpy( _this->string + index + JILString_Length(pSrc), pOldStr + index, oldLen - index );
		_this->string[_this->length] = 0;
		// destruct old string
		if( pOldStr )
			_this->pState->vmFree( _this->pState, pOldStr );
		pOldStr = NULL;
		// destruct clone
		if( bThis )
			JILString_Delete( (JILString*) pSrc );
	}
}

//------------------------------------------------------------------------------
// JILString_RemChrs
//------------------------------------------------------------------------------
/// Remove characters inside this string. This function directly modifies this
/// string.

void JILString_RemChrs(JILString* _this, JILLong index, JILLong length)
{
	if( index < 0 )
		index = 0;
	if( (index < _this->length) && (length > 0) )
	{
		JILLong end;
		if( (index + length) > _this->length )
			length = _this->length - index;
		if( length )
		{
			end = length + index;
			memmove(_this->string + index, _this->string + end, _this->length - end);
			_this->length -= length;
			_this->string[_this->length] = 0;
		}
		else
		{
			JILStringDeAlloc(_this);
		}
	}
}

//------------------------------------------------------------------------------
// JILString_SpanIncluding
//------------------------------------------------------------------------------
/// Count all characters in the source string, starting at 'index', that are
/// INCLUDED in the given character set, and return them as a sub string.

JILString* JILString_SpanIncluding(const JILString* _this, const JILString* chrSet, JILLong index)
{
	JILString* result;
	JILLong len = 0;
	if( index < 0 )
		index = 0;
	if( index < _this->length )
	{
		len = strspn( _this->string + index, JILString_String(chrSet) );
		result = JILString_SubString(_this, index, len);
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_SpanExcluding
//------------------------------------------------------------------------------
/// Count all characters in the source string, starting at 'index', that are
/// NOT INCLUDED in the given character set, and return them as a sub string.

JILString* JILString_SpanExcluding(const JILString* _this, const JILString* chrSet, JILLong index)
{
	JILString* result;
	JILLong len = 0;
	if( index < 0 )
		index = 0;
	if( index < _this->length )
	{
		len = strcspn( _this->string + index, JILString_String(chrSet) );
		result = JILString_SubString(_this, index, len);
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_Insert
//------------------------------------------------------------------------------
/// Insert the given source string at position 'index' of this string and return
/// the result as a new string. This string is not modified.

JILString* JILString_Insert(const JILString* _this, const JILString* source, JILLong index)
{
	JILString* result;
	if( source->length )
	{
		if( index < 0 )
			index = 0;
		if( index > _this->length )
			index = _this->length;
		// allocate a new string
		result = JILStringPreAlloc(_this->pState, _this->length + source->length);
		// copy left part
		memcpy( result->string, _this->string, index );
		// copy middle part
		memcpy( result->string + index, JILString_String(source), source->length );
		// copy right part
		memcpy( result->string + index + source->length, _this->string + index, _this->length - index );
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_Copy(_this);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_InsertChar
//------------------------------------------------------------------------------
/// Insert the given character at the given position in this string and return
/// the result as a new string. This string is not modified.

JILString* JILString_InsertChar(const JILString* _this, JILLong chr, JILLong index)
{
	JILString* result;
	if( index < 0 )
		index = 0;
	if( index > _this->length )
		index = _this->length;
	// allocate a new string
	result = JILStringPreAlloc(_this->pState, _this->length + 1);
	// copy left part
	memcpy( result->string, _this->string, index );
	// copy middle part
	result->string[index] = (JILChar) chr;
	// copy right part
	memcpy( result->string + index + 1, _this->string + index, _this->length - index );
	result->string[result->length] = 0;
	return result;
}

//------------------------------------------------------------------------------
// JILString_Remove
//------------------------------------------------------------------------------
/// Remove 'length' characters at the given position from this string and return
/// the result as a new string. This string is not modified.

JILString* JILString_Remove(const JILString* _this, JILLong index, JILLong length)
{
	JILString* result;
	if( index < 0 )
		index = 0;
	if( (index < _this->length) && (length > 0) )
	{
		if( (index + length) > _this->length )
			length = _this->length - index;
		result = JILStringPreAlloc(_this->pState, _this->length - length);
		// copy left part
		memcpy(result->string, _this->string, index);
		// copy right part
		memcpy( result->string + index, _this->string + index + length, _this->length - (index + length) );
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_Copy(_this);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_ReplaceChar
//------------------------------------------------------------------------------
/// Replace all occurrences of the search char in this string with the given
/// replace char, and return the result as a new string. This string is not
/// modified.

JILString* JILString_ReplaceChar(const JILString* _this, JILLong schr, JILLong rchr)
{
	JILLong i;
	JILString* result = JILStringPreAlloc(_this->pState, _this->length);
	for( i = 0; i < result->length; i++ )
	{
		JILLong c = (unsigned char)_this->string[i];
		if( c == schr )
			c = rchr;
		result->string[i] = c;
	}
	result->string[result->length] = 0;
	return result;
}

//------------------------------------------------------------------------------
// JILString_Replace
//------------------------------------------------------------------------------
/// Replace all occurrences of the search string in this string with the given
/// replace string, and return the result as a new string. This string is not
/// modified.

JILString* JILString_Replace(const JILString* _this, const JILString* search, const JILString* replace)
{
	JILString* result = JILString_New(_this->pState);
	JILLong lastPos;
	JILLong lastLength;
	JILLong pos = 0;
	JILLong currentLength = 0;
	JILLong replaceLength = JILString_Length(replace);
	JILLong searchLength = JILString_Length(search);
	const JILChar* pReplace = JILString_String(replace);
	const JILChar* pSearch = JILString_String(search);
	char* pPos;
	// if search string is empty return
	if( searchLength )
	{
		while( pos < _this->length )
		{
			// using lowlevel strstr, coz it's faster...
			pPos = strstr( _this->string + pos, pSearch );
			if( pPos == NULL )
			{
				lastPos = pos;
				pos = _this->length;
				lastLength = currentLength;
				currentLength += (pos - lastPos);
				JILStringReAlloc(result, currentLength, JILTrue);
				// copy everything up to end of string
				memcpy(result->string + lastLength, _this->string + lastPos, pos - lastPos);
				break;
			}
			lastPos = pos;
			pos = (JILLong) (pPos - _this->string);
			lastLength = currentLength;
			currentLength += (pos - lastPos) + replaceLength;
			JILStringReAlloc(result, currentLength, JILTrue);
			// copy everything up to found position
			memcpy(result->string + lastLength, _this->string + lastPos, pos - lastPos);
			// copy replace string
			memcpy(result->string + lastLength + (pos - lastPos), pReplace, replaceLength);
			// skip search substring
			pos += searchLength;
		}
	}
	else
	{
		JILStringReAlloc(result, searchLength, JILFalse);
		memcpy(result->string, _this->string, searchLength);
	}
	if( result->length )
		result->string[result->length] = 0;
	return result;
}

//------------------------------------------------------------------------------
// JILString_ReplaceRange
//------------------------------------------------------------------------------
/// Replace the range of characters specified by start and length by the given
/// replace string, and return the result as a new string. This string is not
/// modified.

JILString* JILString_ReplaceRange(const JILString* _this, JILLong start, JILLong length, const JILString* replace)
{
	JILString* result;
	const JILChar* replaceString = JILString_String(replace);
	JILLong replaceLength = JILString_Length(replace);
	if( start >= 0 && start < _this->length && length >= 0 )
	{
		if( start + length > _this->length )
			length = _this->length - start;
		result = JILStringPreAlloc(_this->pState, _this->length - length + replaceLength);
		memcpy(result->string, _this->string, start);
		memcpy(result->string + start, replaceString, replaceLength);
		memcpy(result->string + start + replaceLength, _this->string + start + length, _this->length - start - length);
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_Copy(_this);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_Reverse
//------------------------------------------------------------------------------
/// Reverse the order of characters in this string and return the result as a
/// new string. This string is not modified.

JILString* JILString_Reverse(const JILString* _this)
{
	JILString* result = JILString_Copy(_this);
	_strrev(result->string);
	return result;
}

//------------------------------------------------------------------------------
// JILString_SubString
//------------------------------------------------------------------------------
/// Extract 'length' characters from this string, starting at position 'index'
/// and return the result as a new string. This string is not modified.

JILString* JILString_SubString(const JILString* _this, JILLong index, JILLong length)
{
	JILString* result;
	if( index < 0 )
		index = 0;
	if( (index < _this->length) && (length > 0) )
	{
		if( (index + length) > _this->length )
			length = _this->length - index;
		result = JILStringPreAlloc(_this->pState, length);
		memcpy(result->string, _this->string + index, length);
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_Trim
//------------------------------------------------------------------------------
/// Remove all white-space and control characters from the start and the end
/// of this string and return the result as a new string. This string is not
/// altered.

JILString* JILString_Trim(const JILString* _this)
{
	JILString* result;
	if( _this->length > 0 )
	{
		// trim start
		long start;
		long end;
		JILByte* pString = (JILByte*) _this->string;
		for( start = 0; start < _this->length; start++ )
		{
			if( pString[start] > 32 )
				break;
		}
		// trim end
		for( end = _this->length - 1; end >= 0; end-- )
		{
			if( pString[end] > 32 )
				break;
		}
		end++;
		if( end <= start )
		{
			result = JILString_New(_this->pState);
		}
		else
		{
			result = JILString_SubString(_this, start, end - start);
		}
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_ToUpper
//------------------------------------------------------------------------------
/// Convert all characters in this string to upper case and return the result as
/// a new string. This string is not modified.

JILString* JILString_ToUpper(const JILString* _this)
{
	JILString* result;
	if( _this->length > 0 )
	{
		JILLong l;
		char* dest;
		const JILChar* src;
		result = JILStringPreAlloc(_this->pState, _this->length);
		l = _this->length;
		dest = result->string;
		src = _this->string;
		while( l-- )
		{
			*dest++ = toupper(*src++);
		}
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_ToLower
//------------------------------------------------------------------------------
/// Convert all characters in this string to lower case and return the result as
/// a new string. This string is not modified.

JILString* JILString_ToLower(const JILString* _this)
{
	JILString* result;
	if( _this->length > 0 )
	{
		JILLong l;
		char* dest;
		const JILChar* src;
		result = JILStringPreAlloc(_this->pState, _this->length);
		l = _this->length;
		dest = result->string;
		src = _this->string;
		while( l-- )
		{
			*dest++ = tolower(*src++);
		}
		result->string[result->length] = 0;
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_EncodeURL
//------------------------------------------------------------------------------
/// Searches the given string for reserved characters and encodes them by
/// replacing them by the expression '%hh'.
/// If argument 'exceptChars' is not an empty string, it is regarded as a set
/// of characters that should _NOT_ be encoded.

JILString* JILString_EncodeURL(const JILString* _this, const JILString* exceptChars)
{
	JILString* result;
	if( _this->length > 0 )
	{
		static const JILChar* pEncodeChars = "$&+,/:;=?@ <>#%{}|\\^~[]`";
		JILLong c;
		JILLong l;
		const JILChar* src;
		char buf[4] = {0};
		result = JILString_New(_this->pState);
		l = _this->length;
		src = _this->string;
		while( l-- )
		{
			c = (JILByte)*src++;
			if(	(strchr(exceptChars->string, c) == NULL) && (c < 0x20 || c >= 0x80 || (strchr(pEncodeChars, c) != NULL)) )
			{
				JILSnprintf(buf, 4, "%%%2.2X", c);
				JILString_AppendCStr(result, buf);
			}
			else
			{
				buf[0] = c;
				buf[1] = 0;
				JILString_AppendCStr(result, buf);
			}
		}
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_DecodeURL
//------------------------------------------------------------------------------
/// Searches the given string for encoded characters in the format '%hh' and
/// replaces them with the ASCII character specified by the hexadecimal value
/// 'hh'.
/// If argument 'exceptChars' is not an empty string, it is regarded as a set
/// of characters that should _NOT_ be translated, i.e. remain escaped.

JILString* JILString_DecodeURL(const JILString* _this, const JILString* exceptChars)
{
	JILString* result;
	if( _this->length > 0 )
	{
		JILLong c;
		JILLong d;
		JILLong l;
		char* dummy;
		char* dest;
		const JILChar* src;
		char buf[4] = {0};
		result = JILStringPreAlloc(_this->pState, _this->length);
		l = _this->length;
		dest = result->string;
		src = _this->string;
		while( l-- )
		{
			c = (JILByte)*src++;
			if( c == '%' )
			{
				if( l >= 2 && isxdigit(src[0]) && isxdigit(src[1]) )
				{
					JILStrncpy(buf, 4, src, 2);
					d = strtol(buf, &dummy, 16);
					if( strchr(exceptChars->string, d) == NULL )
					{
						c = d;
						src += 2;
						l -= 2;
					}
				}
			}
			*dest++ = c;
		}
		*dest = 0;
		result->length = (JILLong) (dest - result->string);
	}
	else
	{
		result = JILString_New(_this->pState);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILString_Process
//------------------------------------------------------------------------------
/// Calls a delegate function for every character in this string. If the result
/// of the delegate is not 0, it is regarded as a character to be placed in the
/// destination string.
/// This can be used to convert or remove characters from the source string.

JILError JILString_Process(const JILString* _this, JILHandle* pDelegate, JILHandle* pArgs, JILString** ppNew)
{
	JILError result = JIL_No_Exception;
	JILString* newStr;
	JILLong i;
	JILChar chars[4];
	JILState* ps = _this->pState;
	JILHandle* pResult;

	memset(chars, 0, sizeof(JILChar) * 4);
	newStr = JILString_New(ps);
	for( i = 0; i < _this->length; i++ )
	{
		pResult = JILCallFunction(ps, pDelegate, 2, kArgInt, _this->string[i], kArgHandle, pArgs);
		result = NTLHandleToError(ps, pResult);
		if( result )
		{
			NTLFreeHandle(ps, pResult);
			goto exit;
		}
		chars[0] = NTLHandleToInt(ps, pResult);
		if( chars[0] )
		{
			JILString_AppendCStr(newStr, chars);
		}
		NTLFreeHandle(ps, pResult);
	}
	*ppNew = newStr;
	return result;

exit:
	JILString_Delete(newStr);
	*ppNew = NULL;
	return result;
}

//------------------------------------------------------------------------------
// JILString_Join
//------------------------------------------------------------------------------
/// Joins all string elements of the given array into one string.

JILString* JILString_Join(const JILArray* pArray, const JILString* pSeperator)
{
	JILLong i;
	JILState* ps = pSeperator->pState;
	JILString* pStr = JILString_New(ps);
	for( i = 0; i < pArray->size; i++ )
	{
		JILString* pSrc = (JILString*)NTLHandleToObject(ps, type_string, pArray->ppHandles[i]);
		if( pSrc != NULL )
		{
			JILString_Append(pStr, pSrc);
			if( i < (pArray->size - 1) )
				JILString_Append(pStr, pSeperator);
		}
	}
	return pStr;
}

//------------------------------------------------------------------------------
// JILString_Split
//------------------------------------------------------------------------------
/// Returns an array of substrings from this string. The string will be split
/// according to the seperator characters defined in 'seperators', which is
/// regarded as a character set. If 'bDiscard' is JILTrue, empty strings will
/// be discarded and not added to the array.

JILArray* JILString_Split(const JILString* _this, const JILString* seperators, JILBool bDiscard)
{
	JILState* ps = _this->pState;
	JILArray* pArray = JILArray_New(ps);
	JILString* pStr;
	JILHandle* hStr;
	JILLong len, i;
	for( i = 0; i < _this->length; )
	{
		len = strcspn( _this->string + i, JILString_String(seperators) );
		if( len > 0 || !bDiscard )
		{
			pStr = JILString_SubString(_this, i, len);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			JILArray_ArrMove(pArray, hStr);
			NTLFreeHandle(ps, hStr);
		}
		i = i + len + 1;
	}
	return pArray;
}

//------------------------------------------------------------------------------
// JILString_MatchString
//------------------------------------------------------------------------------
/// Treats the given array as a list of substrings and searches this string for
/// occurrences of these substrings. Any matches are returned as an array of
/// string::match instances. If no match is found, an array with zero length is
/// returned.

JILArray* JILString_MatchString(const JILString* _this, const JILArray* pArray)
{
	JILLong i;
	JILHandle* pH;
	JILState* ps = _this->pState;
	JILArray* pResArray = JILArray_New(ps);
	if( JILString_Length(_this) > 0 )
	{
		for( i = 0; i < pArray->size; i++ )
		{
			JILString* pStr = (JILString*)NTLHandleToObject(ps, type_string, pArray->ppHandles[i]);
			if( pStr != NULL && JILString_Length(pStr) > 0 )
			{
				const JILChar* pStart = JILString_String(_this);
				const JILChar* pPos = strstr(pStart, JILString_String(pStr));
				if( pPos != NULL )
				{
					pH = JILStringMatch_Create(ps, (JILLong)(pPos - pStart), JILString_Length(pStr), i);
					JILArray_ArrMove(pResArray, pH);
					NTLFreeHandle(ps, pH);
				}
			}
		}
	}
	return pResArray;
}

//------------------------------------------------------------------------------
// JILString_MatchArray
//------------------------------------------------------------------------------
/// Treats this string as a substring and searches the given array for
/// occurrences of the substring. Any matches are returned as an array of
/// string::match instances. If no match is found, an array with zero length is
/// returned.

JILArray* JILString_MatchArray(const JILString* _this, const JILArray* pArray)
{
	JILLong i;
	JILHandle* pH;
	JILState* ps = _this->pState;
	JILArray* pResArray = JILArray_New(ps);
	if( JILString_Length(_this) > 0 )
	{
		for( i = 0; i < pArray->size; i++ )
		{
			JILString* pStr = (JILString*)NTLHandleToObject(ps, type_string, pArray->ppHandles[i]);
			if( pStr != NULL && JILString_Length(pStr) > 0 )
			{
				const JILChar* pStart = JILString_String(pStr);
				const JILChar* pPos = strstr(pStart, JILString_String(_this));
				if( pPos != NULL )
				{
					pH = JILStringMatch_Create(ps, (JILLong)(pPos - pStart), JILString_Length(_this), i);
					JILArray_ArrMove(pResArray, pH);
					NTLFreeHandle(ps, pH);
				}
			}
		}
	}
	return pResArray;
}

//------------------------------------------------------------------------------
// JILStringPreAlloc
//------------------------------------------------------------------------------
/// Allocate a new string object and pre-allocate a string buffer big enough to
/// accommodate the given number of characters.

static JILString* JILStringPreAlloc(JILState* pState, JILLong length)
{
	JILString* _this = (JILString*) pState->vmMalloc(pState, sizeof(JILString));
	_this->pState = pState;
	_this->maxLength = (((length + 1) / kStringAllocGrain) + 1) * kStringAllocGrain;
	_this->length = length;
	_this->string = _this->pState->vmMalloc( _this->pState, _this->maxLength );
	return _this;
}

//------------------------------------------------------------------------------
// JILStringReAlloc
//------------------------------------------------------------------------------
/// Throw away old string buffer and allocate a new one.

static void JILStringReAlloc(JILString* _this, JILLong newSize, JILLong keepData)
{
	JILLong newMaxLength = (((newSize + 1) / kStringAllocGrain) + 1) * kStringAllocGrain;
	// first we check, if reallocation is necessary
	if( newMaxLength != _this->maxLength )
	{
		char* pNewBuffer = NULL;
		pNewBuffer = _this->pState->vmMalloc( _this->pState, newMaxLength );
		if( pNewBuffer )
		{
			if( keepData && _this->string )
			{
				JILLong numKeep = (newSize < _this->length) ? newSize : _this->length;
				JILStrncpy( pNewBuffer, newMaxLength, _this->string, numKeep );
			}
			// destroy old buffer
			if( _this->string )
				_this->pState->vmFree( _this->pState, _this->string );
			_this->string = pNewBuffer;
			_this->maxLength = newMaxLength;
		}
	}
	_this->length = newSize;
}

//------------------------------------------------------------------------------
// JILStringDeAlloc
//------------------------------------------------------------------------------
/// Deallocates all data contained by the string object, but not the string
/// object itself. The result will be an empty string object of zero length.

static void JILStringDeAlloc(JILString* _this)
{
	if( _this->string )
	{
		_this->pState->vmFree( _this->pState, _this->string );
		_this->string = NULL;
	}
	_this->length = 0;
	_this->maxLength = 0;
}
