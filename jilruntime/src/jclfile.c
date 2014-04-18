//------------------------------------------------------------------------------
// File: JCLFile.c                                          (c) 2004 jewe.org
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
//	An object that represents a code snippet or whole source text file to be
//	compiled by the JewelScript compiler.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jclfile.h"

//------------------------------------------------------------------------------
// the keyword list
//------------------------------------------------------------------------------

const JCLToken kKeywordList[] =
{
	tk_accessor,	"accessor",
	tk_alias,		"alias",
	tk_and,			"and",
	tk_array,		"array",
	tk_break,		"break",
	tk_case,		"case",
	tk_class,		"class",
	tk_clause,		"clause",
	tk_cofunction,	"cofunction",
	tk_const,		"const",
	tk_continue,	"continue",
	tk_convertor,	"convertor",
	tk_default,		"default",
	tk_delegate,	"delegate",
	tk_do,			"do",
	tk_else,		"else",
	tk_explicit,	"explicit",
	tk_extern,		"extern",
	tk_false,		"false",
	tk_float,		"float",
	tk_for,			"for",
	tk_function,	"function",
	tk_goto,		"goto",
	tk_hybrid,		"hybrid",
	tk_if,			"if",
	tk_import,		"import",
	tk_int,			"int",
	tk_interface,	"interface",
	tk_method,		"method",
	tk_native,		"native",
	tk_new,			"new",
	tk_not,			"not",
	tk_null,		"null",
	tk_or,			"or",
	tk_option,		"option",
	tk_return,		"return",
	tk_sameref,		"sameref",
	tk_strict,		"strict",
	tk_string,		"string",
	tk_switch,		"switch",
	tk_this,		"this",
	tk_throw,		"throw",
	tk_true,		"true",
	tk_typeof,		"typeof",
	tk_using,		"using",
	tk_var,			"var",
	tk_weak,		"weak",
	tk_while,		"while",
	tk_yield,		"yield",
	tk__brk,		"__brk",
	tk__rtchk,		"__rtchk",
	tk__selftest,	"__selftest",
	0,				NULL
};

//------------------------------------------------------------------------------
// the operator list
//------------------------------------------------------------------------------

const JCLToken kOperatorList[] =
{
	// arithmetic
	tk_plus,		"+",
	tk_minus,		"-",
	tk_mul,			"*",
	tk_div,			"/",
	tk_mod,			"%",

	// boolean
	tk_not,			"!",
	tk_and,			"&&",
	tk_or,			"||",

	// binary
	tk_equ,			"==",
	tk_greater,		">",
	tk_greater_equ,	">=",
	tk_less,		"<",
	tk_less_equ,	"<=",
	tk_not_equ,		"!=",

	// bitwise
	tk_band,		"&",
	tk_bor,			"|",
	tk_xor,			"^",
	tk_bnot,		"~",
	tk_lshift,		"<<",
	tk_rshift,		">>",

	// assignment operators
	tk_assign,			"=",
	tk_plus_assign,		"+=",
	tk_minus_assign,	"-=",
	tk_mul_assign,		"*=",
	tk_div_assign,		"/=",
	tk_mod_assign,		"%=",
	tk_band_assign,		"&=",
	tk_bor_assign,		"|=",
	tk_xor_assign,		"^=",
	tk_lshift_assign,	"<<=",
	tk_rshift_assign,	">>=",

	// other operators
	tk_plusplus,		"++",
	tk_minusminus,		"--",

	0,					NULL
};

//------------------------------------------------------------------------------
// other characters
//------------------------------------------------------------------------------

const JCLToken kCharacterList[] =
{
	tk_colon,			":",
	tk_scope,			"::",
	tk_comma,			",",
	tk_semicolon,		";",
	tk_point,			".",
	tk_bullets,			"...",

	// brackets
	tk_round_open,		"(",
	tk_round_close,		")",
	tk_curly_open,		"{",
	tk_curly_close,		"}",
	tk_square_open,		"[",
	tk_square_close,	"]",

	0,					NULL
};

//------------------------------------------------------------------------------
// other global constants
//------------------------------------------------------------------------------

static const JILChar* kKeywordChars =		"ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
static const JILChar* kIdentifierChars =	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
static const JILChar* kFirstDigitChars =	"-.0123456789";
static const JILChar* kOperatorChars =		"+-*/%<=>!&|^~";
static const JILChar* kSingleChars =		"()[]{};";
static const JILChar* kCharacterChars =		":,.";
static const JILChar* kHexDigitChars =		"0123456789ABCDEFabcdef";
static const JILChar* kOctDigitChars =		"01234567";

//------------------------------------------------------------------------------
// internal functions
//------------------------------------------------------------------------------

static JILError		open_JCLFile		(JCLFile* _this, const JILChar* pName, const JILChar* pText, const JILChar* pPath);
static JILError		getToken_JCLFile	(JCLFile* _this, JCLString* pToken, JILLong* pTokenID);
static JILError		peekToken_JCLFile	(JCLFile* _this, JCLString* pToken, JILLong* pTokenID);
static JILLong		getLocator_JCLFile	(JCLFile* _this);
static void			setLocator_JCLFile	(JCLFile* _this, JILLong pos);

static JILLong		IsCharType			(JILChar chr, const JILChar* chrSet)	{ return (strchr(chrSet, chr) != NULL); }
static JILLong		IsDigit				(JILChar chr)							{ return (chr >= '0' && chr <= '9'); }
static JILLong		IsHexDigit			(JILChar chr)							{ return (chr >= '0' && chr <= '9') || (chr >='A' && chr <= 'F') || (chr >='a' && chr <= 'f'); }
static JILLong		IsOctDigit			(JILChar chr)							{ return (chr >= '0' && chr <= '7'); }
static JILLong		HexDigitValue		(JILChar chr)							{ return ((chr >= '0' && chr <= '9') ? chr - 48 : ((chr >= 'A' && chr <= 'Z') ? chr - 55 : chr - 87)); }
static JILError		Ignore				(JCLFile* _this);
static JILError		GetStrLiteral		(JCLFile* _this, JCLString* string);
static JILError		FindTokenAtPosition	(JCLFile* _this, JCLString* string, JILLong* pTokenID, const JCLToken* pTokenList);

//------------------------------------------------------------------------------
// JCLFile
//------------------------------------------------------------------------------
// constructor

void create_JCLFile( JCLFile* _this )
{
	// assign function pointers
	_this->Open = open_JCLFile;
	_this->GetToken = getToken_JCLFile;
	_this->PeekToken = peekToken_JCLFile;
	_this->GetLocator = getLocator_JCLFile;
	_this->SetLocator = setLocator_JCLFile;

	// init all our members
	_this->mipName = NULL;
	_this->mipText = NULL;
	_this->mipPath = NULL;
	_this->miPass = 0;
	_this->miNative = JILFalse;
}

//------------------------------------------------------------------------------
// JCLFile
//------------------------------------------------------------------------------
// destructor

void destroy_JCLFile( JCLFile* _this )
{
	DELETE( _this->mipName );
	DELETE( _this->mipText );
	DELETE( _this->mipPath );
}

//------------------------------------------------------------------------------
// JCLFile::Copy
//------------------------------------------------------------------------------
// Copying not supported for this class.

void copy_JCLFile( JCLFile* _this, const JCLFile* src )
{
}

//------------------------------------------------------------------------------
// JCLFile::Open
//------------------------------------------------------------------------------
//

static JILError open_JCLFile(JCLFile* _this, const JILChar* pName, const JILChar* pText, const JILChar* pPath)
{
	JILError err = JCL_No_Error;

	_this->mipName = NEW(JCLString);
	JCLSetString(_this->mipName, pName);
	_this->mipText = NEW(JCLString);
	JCLSetString(_this->mipText, pText);
	_this->mipPath = NEW(JCLString);
	JCLSetString(_this->mipPath, pPath);

	return err;
}

//------------------------------------------------------------------------------
// JCLFile::GetToken
//------------------------------------------------------------------------------
// Reads the next token from the source file and returns the token type ID as
// a positive integer value (see enum JCToken). For certain token types, the
// specific sub-string is return in [pToken].

static JILError getToken_JCLFile(JCLFile* _this, JCLString* pToken, JILLong* pTokenID)
{
	JILError err = JCL_No_Error;
	*pTokenID = tk_unknown;

	// clear output string
	JCLClear(pToken);

	// ignore whitespace first
	err = Ignore(_this);
	if( !err )
	{
		// check what we have found
		JILChar c = JCLGetCurrentChar(_this->mipText);
		JILChar d = JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1);
		// un-escaped string literal?
		if( c == '/' && d == '\"' )
		{
			// read the entire string literal
			err = GetStrLiteral(_this, pToken);
			*pTokenID = tk_lit_string;
		}
		// part of keyword or identifier characters?
		else if( IsCharType(c, kKeywordChars) )
		{
			// try to read as many characters of the keyword or identifier
			JCLSpanIncluding(_this->mipText, kIdentifierChars, pToken);
			// check if it is a keyword
			*pTokenID = GetTokenID(JCLGetString(pToken), kKeywordList);
			// check err
			if( *pTokenID == tk_unknown )
				*pTokenID = tk_identifier;
		}
		// part of operator characters?
		else if( IsCharType(c, kOperatorChars) && (c != '.' || !IsDigit(d)) )
		{
			// try to find a matching operator token
			err = FindTokenAtPosition(_this, pToken, pTokenID, kOperatorList);
		}
		// part of number characters?
		else if( IsCharType(c, kFirstDigitChars) && (c != '.' || IsDigit(d)) )
		{
			JILLong type;
			// check if long or float number, scan in the token and return tk_lit_int or tk_lit_float!
			JCLSpanNumber(_this->mipText, pToken, &type);
			*pTokenID = (type == 0) ? tk_lit_int : tk_lit_float;
		}
		else if( IsCharType(c, kCharacterChars) )
		{
			// try to read as many characters as possible
			JCLSpanIncluding(_this->mipText, kCharacterChars, pToken);
			// try to find the character in the list
			*pTokenID = GetTokenID(JCLGetString(pToken), kCharacterList);
			// check err
			if( *pTokenID == tk_unknown )
				err = JCL_ERR_Unexpected_Token;
		}
		else if( IsCharType(c, kSingleChars) )
		{
			// must read only a single character
			JCLFill(pToken, JCLGetCurrentChar(_this->mipText), 1);
			JCLSeekForward(_this->mipText, 1);
			// try to find the character in the list
			*pTokenID = GetTokenID(JCLGetString(pToken), kCharacterList);
			// check err
			if( *pTokenID == tk_unknown )
				err = JCL_ERR_Unexpected_Token;
		}
		else
		{
			// single characters
			switch( c )
			{
				case '\"':
					// read the entire string literal
					err = GetStrLiteral(_this, pToken);
					*pTokenID = tk_lit_string;
					break;
				case '\'':
					// read the entire character literal
					err = GetStrLiteral(_this, pToken);
					*pTokenID = tk_lit_char;
					break;
				default:
					err = JCL_ERR_Unexpected_Token;
					break;
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::PeekToken
//------------------------------------------------------------------------------
// Like getToken, but does not advance in the stream. The locator stays at
// the beginning of the token.

static JILError peekToken_JCLFile(JCLFile* _this, JCLString* pToken, JILLong* pTokenID)
{
	JILError err = JCL_No_Error;

	// clear output string
	JCLClear(pToken);

	// ignore whitespace first
	err = Ignore(_this);
	if( !err )
	{
		// save current locator position
		JILLong oldPos = JCLGetLocator(_this->mipText);
		// get token
		err = _this->GetToken(_this, pToken, pTokenID);
		// restore locator position
		JCLSetLocator(_this->mipText, oldPos);
	}
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::JCLGetLocator
//------------------------------------------------------------------------------
//

static JILLong getLocator_JCLFile(JCLFile* _this)
{
	return JCLGetLocator(_this->mipText);
}

//------------------------------------------------------------------------------
// JCLFile::JCLSetLocator
//------------------------------------------------------------------------------
//

static void setLocator_JCLFile(JCLFile* _this, JILLong pos)
{
	JCLSetLocator(_this->mipText, pos);
}

//------------------------------------------------------------------------------
// Ignore
//------------------------------------------------------------------------------
// Ignore white space (including comments).

static JILError Ignore(JCLFile* _this)
{
	JILError err = JCL_No_Error;
	JILLong nextChar = 0;
	JILLong comment = 0;
	JILLong c = 0;

	while( !err )
	{
		if( JCLAtEnd(_this->mipText) )
		{
			err = JCL_ERR_End_Of_File;
			break;
		}
		else
		{
			c = JCLGetCurrentChar(_this->mipText);
			nextChar = JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1);
			if( !comment )
			{
				if( c <= 32 )
				{
					JCLSeekForward(_this->mipText, 1);
				}
				else if( c == '#' || (c == '/' && nextChar == '/') )
				{
					// skip line up to line feed
					JCLSeekUntil(_this->mipText, "\n");
				}
				else if( c == '/' && nextChar == '*' )
				{
					comment = 1;
					JCLSeekForward(_this->mipText, 2);
				}
				else
				{
					// found something non-white spacey
					break;
				}
			}
			else
			{
				if( c == '*' && nextChar == '/' )
				{
					comment = 0;
					JCLSeekForward(_this->mipText, 2);
				}
				else
				{
					JCLSeekForward(_this->mipText, 1);
				}
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// GetTokenID
//------------------------------------------------------------------------------
// Checks if given string is a token and returns the ID of the token. If the
// given string is not found in the list, returns tk_unknown.

JILLong GetTokenID(const JILChar* string, const JCLToken* pTokenList)
{
	JILLong i;
	for( i = 0; pTokenList[i].name != NULL; i++ )
	{
		if( strcmp(string, pTokenList[i].name) == 0 )
			return pTokenList[i].id;
	}
	return tk_unknown;
}

//------------------------------------------------------------------------------
// GetCurrentPosition
//------------------------------------------------------------------------------
// Gets the current position as line and column number.

void GetCurrentPosition(JCLFile* _this, JILLong* pColumn, JILLong* pLine)
{
	JCLString* str;
	JILLong length;
	JILLong i;
	JILLong line = 1;
	JILLong column = 1;
	char c;
	str = _this->mipText;
	length = JCLGetLocator(str);
	if( length > JCLGetLength(str) )
		length = JCLGetLength(str);
	for( i = 0; i < length; i++ )
	{
		c = JCLGetChar(str, i);
		if( c == 13 )
		{
			if( JCLGetChar(str, i+1) == 10 )
				i++;
			line++;
			column = 1;
		}
		else if( c == 10 )
		{
			if( JCLGetChar(str, i+1) == 13 )
				i++;
			line++;
			column = 1;
		}
		else if( c == 9 )
		{
			column += 4 - ((column - 1) % 4);
		}
		else
		{
			column++;
		}
	}
	*pColumn = column;
	*pLine = line;
}

//------------------------------------------------------------------------------
// GetStrLiteral
//------------------------------------------------------------------------------
// Reads a string literal from the input text stream and parses escape
// sequences like "\n", "\t", etc.

static JILError GetStrLiteral(JCLFile* _this, JCLString* string)
{
	JILError err = JCL_ERR_End_Of_File;
	JILLong c, q;
	char* pDummy;
	JCLString* pTemp = NULL;
	JILBool bEscape = JILTrue;

	JCLClear(string);
	// skip the start quote
	q = JCLGetCurrentChar(_this->mipText);
	if( q == '/' )
	{
		// un-escaped string literal
		bEscape = JILFalse;
		JCLSeekForward(_this->mipText, 1);
		q = JCLGetCurrentChar(_this->mipText);
	}
	JCLSeekForward(_this->mipText, 1);
	// seek toward end quote
	while( !JCLAtEnd(_this->mipText) )
	{
		c = JCLGetCurrentChar(_this->mipText);
		if( bEscape && c == '\\' )			// escape character?
		{
			// get next character
			JCLSeekForward(_this->mipText, 1);
			if( JCLAtEnd(_this->mipText) )
				goto error;
			c = JCLGetCurrentChar(_this->mipText);
			switch( c )
			{
				case 'a':	// alert (bell)
					JCLAppend(string, "\a");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'b':	// backspace
					JCLAppend(string, "\b");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'f':	// formfeed
					JCLAppend(string, "\f");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'n':	// newline
					JCLAppend(string, "\n");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'r':	// return
					JCLAppend(string, "\r");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 't':	// tab
					JCLAppend(string, "\t");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'v':	// vertical tab
					JCLAppend(string, "\v");
					JCLSeekForward(_this->mipText, 1);
					break;
				case '\'':	// single quotation mark
					JCLAppend(string, "\'");
					JCLSeekForward(_this->mipText, 1);
					break;
				case '\"':	// double quotation mark
					JCLAppend(string, "\"");
					JCLSeekForward(_this->mipText, 1);
					break;
				case '\\':	// backslash
					JCLAppend(string, "\\");
					JCLSeekForward(_this->mipText, 1);
					break;
				case 'x':	// ASCII character in hexadecimal
					// skip the x
					JCLSeekForward(_this->mipText, 1);
					if( JCLAtEnd(_this->mipText) )
						goto error;
					pTemp = NEW(JCLString);
					JCLSpanIncluding(_this->mipText, kHexDigitChars, pTemp);
					c = strtol(JCLGetString(pTemp), &pDummy, 16);
					DELETE( pTemp );
					pTemp = NULL;
					if( sizeof(JILChar) == 1 && c > 255 )
					{
						err = JCL_ERR_Character_Value_Too_Large;
						goto error;
					}
					JCLAppendChar(string, c);
					break;
				case '0':	// ASCII character in octal
					pTemp = NEW(JCLString);
					JCLSpanIncluding(_this->mipText, kOctDigitChars, pTemp);
					c = strtol(JCLGetString(pTemp), &pDummy, 8);
					DELETE( pTemp );
					pTemp = NULL;
					if( sizeof(JILChar) == 1 && c > 255 )
					{
						err = JCL_ERR_Character_Value_Too_Large;
						goto error;
					}
					JCLAppendChar(string, c);
					break;
			}
		}
		else if( c == q )		// end quote?
		{
			if( bEscape )
			{
				// skip end quote
				JCLSeekForward(_this->mipText, 1);
				// ignore whitespace
				err = Ignore(_this);
				if( err )
					goto error;
				// check what's next
				c = JCLGetCurrentChar(_this->mipText);
				// another quote?
				if( c == q )
				{
					bEscape = JILTrue;
					// skip quote and continue
					JCLSeekForward(_this->mipText, 1);
				}
				else if( c == '/' && JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == q )
				{
					bEscape = JILFalse;
					// skip quote and continue
					JCLSeekForward(_this->mipText, 2);
				}
				else
				{
					// we're done
					err = JCL_No_Error;
					break;
				}
			}
			else
			{
				if( JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == '/' )
				{
					// skip quote
					JCLSeekForward(_this->mipText, 2);
					// ignore whitespace
					err = Ignore(_this);
					if( err )
						goto error;
					// check what's next
					c = JCLGetCurrentChar(_this->mipText);
					// another quote?
					if( c == q )
					{
						bEscape = JILTrue;
						// skip quote and continue
						JCLSeekForward(_this->mipText, 1);
					}
					else if( c == '/' && JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == q )
					{
						bEscape = JILFalse;
						// skip quote and continue
						JCLSeekForward(_this->mipText, 2);
					}
					else
					{
						// we're done
						err = JCL_No_Error;
						break;
					}
				}
				else
				{
					JCLAppendChar(string, c);
					JCLSeekForward(_this->mipText, 1);
				}
			}
		}
		else					// any other character
		{
			JCLAppendChar(string, c);
			JCLSeekForward(_this->mipText, 1);
		}
	}
error:
	DELETE( pTemp );
	return err;
}

//------------------------------------------------------------------------------
// FindTokenAtPosition
//------------------------------------------------------------------------------
// Searches at the current locator position for a token. If multiple tokens
// match, the longest token is returned.

static JILError FindTokenAtPosition(JCLFile* _this, JCLString* string, JILLong* pTokenID, const JCLToken* pTokenList)
{
	JILLong i;
	size_t l;
	JILLong i_max = -1;
	size_t l_max = 0;
	for( i = 0; pTokenList[i].name != NULL; i++ )
	{
		if( JCLBeginsWith(_this->mipText, pTokenList[i].name) )
		{
			l = strlen(pTokenList[i].name);
			if( l >= l_max )
			{
				l_max = l;
				i_max = i;
			}
		}
	}
	if( i_max == -1 )
		return JCL_ERR_Unexpected_Token;
	JCLSetString(string, pTokenList[i_max].name);
	*pTokenID = pTokenList[i_max].id;
	JCLSeekForward(_this->mipText, l_max);
	return JCL_No_Error;
}
