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
#include "jcloption.h"

//------------------------------------------------------------------------------
// the keyword list
//------------------------------------------------------------------------------

const JCLToken kKeywordList[] =
{
	tk_accessor,	"accessor",
	tk_alias,		"alias",
	tk_and,			"and",
	tk_break,		"break",
	tk_case,		"case",
	tk_class,		"class",
	tk_clause,		"clause",
	tk_cofunction,	"cofunction",
	tk_const,		"const",
	tk_continue,	"continue",
	tk_default,		"default",
	tk_delegate,	"delegate",
	tk_do,			"do",
	tk_else,		"else",
	tk_explicit,	"explicit",
	tk_extends,		"extends",
	tk_false,		"false",
	tk_for,			"for",
	tk_function,	"function",
	tk_goto,		"goto",
	tk_hybrid,		"hybrid",
	tk_if,			"if",
	tk_implements,	"implements",
	tk_import,		"import",
	tk_inherits,	"inherits",
	tk_interface,	"interface",
	tk_method,		"method",
	tk_namespace,	"namespace",
	tk_native,		"native",
	tk_new,			"new",
	tk_not,			"not",
	tk_null,		"null",
	tk_option,		"option",
	tk_or,			"or",
	tk_private,		"private",
	tk_return,		"return",
	tk_sameref,		"sameref",
	tk_strict,		"strict",
	tk_switch,		"switch",
	tk_this,		"this",
	tk_throw,		"throw",
	tk_true,		"true",
	tk_typeof,		"typeof",
	tk_using,		"using",
	tk_var,			"var",
	tk_virtual,		"virtual",
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
	tk_ternary,			"?",
	tk_lambda,			"=>",

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
static const JILChar* kOperatorChars =		"+-*/%<=>!&|^~?";
static const JILChar* kSingleChars =		"()[]{};";
static const JILChar* kCharacterChars =		":,.";
static const JILChar* kHexDigitChars =		"0123456789ABCDEFabcdef";
static const JILChar* kOctDigitChars =		"01234567";

//------------------------------------------------------------------------------
// internal functions
//------------------------------------------------------------------------------

static JILError		open_JCLFile		(JCLFile* _this, const JILChar* pName, const JILChar* pText, const JILChar* pPath, JCLOption* pOptions);
static JILError		getToken_JCLFile	(JCLFile* _this, JCLString* pToken, JILLong* pTokenID);
static JILError		peekToken_JCLFile	(JCLFile* _this, JCLString* pToken, JILLong* pTokenID);
static JILLong		getLocator_JCLFile	(JCLFile* _this);
static void			setLocator_JCLFile	(JCLFile* _this, JILLong pos);
static JILError		close_JCLFile		(JCLFile* _this);
static JILError		scanBlock_JCLFile	(JCLFile* _this, JCLString* outStr);
static JILError		scanStatement_JCLFile(JCLFile* _this, JCLString* outStr);
static JILError		scanExpression_JCLFile(JCLFile* _this, JCLString* outStr);

static JILLong		IsCharType			(JILChar chr, const JILChar* chrSet)	{ return (strchr(chrSet, chr) != NULL); }
static JILLong		IsDigit				(JILChar chr)							{ return (chr >= '0' && chr <= '9'); }
static JILLong		IsHexDigit			(JILChar chr)							{ return (chr >= '0' && chr <= '9') || (chr >='A' && chr <= 'F') || (chr >='a' && chr <= 'f'); }
static JILLong		IsOctDigit			(JILChar chr)							{ return (chr >= '0' && chr <= '7'); }
static JILLong		HexDigitValue		(JILChar chr)							{ return ((chr >= '0' && chr <= '9') ? chr - 48 : ((chr >= 'A' && chr <= 'Z') ? chr - 55 : chr - 87)); }
static JILError		GetToken			(JCLFile* _this, JCLString* pToken, JILLong* pTokenID);
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
	_this->Close = close_JCLFile;
	_this->ScanBlock = scanBlock_JCLFile;
	_this->ScanStatement = scanStatement_JCLFile;
	_this->ScanExpression = scanExpression_JCLFile;

	// init all our members
	_this->mipName = NULL;
	_this->mipText = NULL;
	_this->mipPath = NULL;
	_this->mipTokens = NULL;
	_this->mipPackage = NULL;
	_this->mipOptions = NULL;
	_this->miLocator = 0;
	_this->miPass = 0;
	_this->miLine = 0;
	_this->miColumn = 0;
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
	DELETE( _this->mipPackage );
	DELETE( _this->mipTokens );
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
// Initializes this file object and pre-compiles the given source code into an
// array of token objects.

static JILError open_JCLFile(JCLFile* _this, const JILChar* pName, const JILChar* pText, const JILChar* pPath, JCLOption* pOptions)
{
	JILError err = JCL_No_Error;
	JCLString* pToken = NEW(JCLString);
	JILLong tokenID;
	JILLong loc;
	JCLFileToken* pft;
	// allocate members
	_this->mipName = NEW(JCLString);
	_this->mipText = NEW(JCLString);
	_this->mipPath = NEW(JCLString);
	_this->mipPackage = NEW(JCLString);
	_this->mipTokens = NEW(Array_JCLFileToken);
	_this->mipOptions = pOptions;
	_this->miLocator = 0;
	_this->miPass = 0;
	_this->miLine = 1;
	_this->miColumn = 0;
	_this->mipTokens->Grain(_this->mipTokens, 1024);
	// copy arguments
	JCLSetString(_this->mipName, pName);
	JCLSetString(_this->mipText, pText);
	JCLSetString(_this->mipPath, pPath);
	// pre-parse text
	for(;;)
	{
		if( JCLAtEnd(_this->mipText) )
			break;
		err = Ignore(_this);
		if( err || JCLAtEnd(_this->mipText) )
			break;
		err = GetToken(_this, pToken, &tokenID);
		if( err )
			break;
		loc = JCLGetLocator(_this->mipText);
		pft = _this->mipTokens->New(_this->mipTokens);
		pft->miLocation = loc;
		pft->miLine = _this->miLine;
		pft->miColumn = loc - _this->miColumn + 1;
		pft->miTokenID = tokenID;
		if( JCLGetLength(pToken) )
		{
			pft->mipToken = NEW(JCLString);
			pft->mipToken->Copy(pft->mipToken, pToken);
		}
	}
	if( err == JCL_ERR_End_Of_File )
		err = JCL_No_Error;
	DELETE(pToken);
	DELETE(_this->mipText);
	_this->mipText = NULL;
	_this->mipOptions = NULL;
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::PeekToken
//------------------------------------------------------------------------------
// Reads a token from the token array and returns the token ID as a
// positive integer value (see enum TokenID). For certain token types, the
// string representation of the token is returned in pToken.

static JILError peekToken_JCLFile(JCLFile* _this, JCLString* pToken, JILLong* pTokenID)
{
	JILError err = JCL_ERR_End_Of_File;
	*pTokenID = tk_unknown;
	JCLClear(pToken);
	if( _this->miLocator < _this->mipTokens->Count(_this->mipTokens) )
	{
		JCLFileToken* pft = _this->mipTokens->Get(_this->mipTokens, _this->miLocator);
		*pTokenID = pft->miTokenID;
		if( pft->mipToken )
			pToken->Copy(pToken, pft->mipToken);
		err = JCL_No_Error;
	}
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::GetToken
//------------------------------------------------------------------------------
// Reads the next token from the token array and advances the read position.

static JILError getToken_JCLFile(JCLFile* _this, JCLString* pToken, JILLong* pTokenID)
{
	JILError err = peekToken_JCLFile(_this, pToken, pTokenID);
	if( err == JCL_No_Error )
		_this->miLocator++;
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::JCLGetLocator
//------------------------------------------------------------------------------

static JILLong getLocator_JCLFile(JCLFile* _this)
{
	return _this->miLocator;
}

//------------------------------------------------------------------------------
// JCLFile::JCLSetLocator
//------------------------------------------------------------------------------

static void setLocator_JCLFile(JCLFile* _this, JILLong pos)
{
	if( pos >= 0 )
		_this->miLocator = pos;
}

//------------------------------------------------------------------------------
// JCLFile::Close
//------------------------------------------------------------------------------

static JILError close_JCLFile(JCLFile* _this)
{
	DELETE(_this->mipText);
	_this->mipText = NULL;
	DELETE(_this->mipTokens);
	_this->mipTokens = NULL;
	DELETE(_this->mipPackage);
	_this->mipPackage = NULL;
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// TokenToString
//------------------------------------------------------------------------------

static void TokenToString(JCLFile* _this, JILLong tokenID, const JCLString* pToken, JCLString* out)
{
	if( tokenID == tk_lit_string )
	{
		JCLAppend(out, "/\"");
		JCLAppend(out, JCLGetString(pToken));
		JCLAppend(out, "\"/");
	}
	else
	{
		JCLAppend(out, JCLGetString(pToken));
	}
	JCLAppend(out, " ");
}

//------------------------------------------------------------------------------
// JCLFile::ScanBlock
//------------------------------------------------------------------------------
// Reads all text between {}()[] into a string.

static JILError scanBlock_JCLFile(JCLFile* _this, JCLString* outStr)
{
	JILError err;
	JILLong tokenID;
	JILLong startToken;
	JILLong endToken;
	JILLong hier;
	JCLString* pToken = NEW(JCLString);
	JCLClear(outStr);
	err = getToken_JCLFile(_this, pToken, &startToken);
	if( !err && startToken == tk_curly_open || startToken == tk_round_open || startToken == tk_square_open )
	{
		if( startToken == tk_curly_open )
			endToken = tk_curly_close;
		else if( startToken == tk_round_open )
			endToken = tk_round_close;
		else if( startToken == tk_square_open )
			endToken = tk_square_close;
		TokenToString(_this, startToken, pToken, outStr);
		hier = 1;
		do 
		{
			err = getToken_JCLFile(_this, pToken, &tokenID);
			if( err )
				break;
			if( tokenID == startToken )
				hier++;
			else if( tokenID == endToken )
				hier--;
			TokenToString(_this, tokenID, pToken, outStr);
		}
		while( tokenID != endToken && hier > 0 );
	}
	DELETE(pToken);
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::ScanStatement
//------------------------------------------------------------------------------
// Reads a statement into a string. Stops at ;}

static JILError scanStatement_JCLFile(JCLFile* _this, JCLString* outStr)
{
	JILError err;
	JILLong tokenID;
	JILLong h1, h2, h3;
	JILLong savePos;
	JCLString* pToken = NEW(JCLString);
	JCLClear(outStr);
	h1 = h2 = h3 = 0;
	for(;;) 
	{
		savePos = _this->miLocator;
		err = getToken_JCLFile(_this, pToken, &tokenID);
		if( err )
			break;
		if( !h1 && !h2 && !h3 )
		{
			if( tokenID == tk_semicolon || tokenID == tk_curly_close )
				break;
		}
		TokenToString(_this, tokenID, pToken, outStr);
		switch( tokenID )
		{
			case tk_curly_open: h1++; break;
			case tk_round_open: h2++; break;
			case tk_square_open: h3++; break;
			case tk_curly_close: h1--; break;
			case tk_round_close: h2--; break;
			case tk_square_close: h3--; break;
		}
	}
	_this->miLocator = savePos;
	DELETE(pToken);
	return err;
}

//------------------------------------------------------------------------------
// JCLFile::ScanExpression
//------------------------------------------------------------------------------
// Reads an expression into a string. Stops at :,);}

static JILError scanExpression_JCLFile(JCLFile* _this, JCLString* outStr)
{
	JILError err;
	JILLong tokenID;
	JILLong h1, h2, h3;
	JILLong savePos;
	JCLString* pToken = NEW(JCLString);
	JCLClear(outStr);
	h1 = h2 = h3 = 0;
	for(;;) 
	{
		savePos = _this->miLocator;
		err = getToken_JCLFile(_this, pToken, &tokenID);
		if( err )
			break;
		if( !h1 && !h2 && !h3 )
		{
			if( tokenID == tk_colon
			||	tokenID == tk_ternary
			||	tokenID == tk_comma
			||	tokenID == tk_round_close
			||	tokenID == tk_semicolon
			||	tokenID == tk_curly_close )
				break;
		}
		TokenToString(_this, tokenID, pToken, outStr);
		switch( tokenID )
		{
			case tk_curly_open: h1++; break;
			case tk_round_open: h2++; break;
			case tk_square_open: h3++; break;
			case tk_curly_close: h1--; break;
			case tk_round_close: h2--; break;
			case tk_square_close: h3--; break;
		}
	}
	_this->miLocator = savePos;
	DELETE(pToken);
	return err;
}

//------------------------------------------------------------------------------
// GetToken
//------------------------------------------------------------------------------
// Internal function to get the next token from the text stream.

static JILError GetToken(JCLFile* _this, JCLString* pToken, JILLong* pTokenID)
{
	JILError err = JCL_No_Error;
	JILChar c;
	JILChar d;

	// clear output string
	*pTokenID = tk_unknown;
	JCLClear(pToken);

	// check what we have found
	c = JCLGetCurrentChar(_this->mipText);
	d = JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1);
	// verbatim string literal?
	if( (c == '/' || c == '@') && d == '\"' )
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
		*pTokenID = (type || _this->mipOptions->miDefaultFloat) ? tk_lit_float : tk_lit_int;
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
	return err;
}

//------------------------------------------------------------------------------
// Ignore
//------------------------------------------------------------------------------
// Ignore white space (including comments).

static JILError Ignore(JCLFile* _this)
{
	JILError err = JCL_No_Error;
	JILLong c = 0;
	JILLong nextChar = 0;
	JILBool comment = JILFalse;

	for(;;)
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
			if( comment )
			{
				if( c == '*' && nextChar == '/' )
				{
					comment = JILFalse;
					JCLSeekForward(_this->mipText, 2);
				}
				else
				{
					if( c == '\n' )
					{
						_this->miColumn = JCLGetLocator(_this->mipText) + 1;
						_this->miLine++;
					}
					JCLSeekForward(_this->mipText, 1);
				}
			}
			else
			{
				if( c <= 32 )
				{
					if( c == '\n' )
					{
						_this->miColumn = JCLGetLocator(_this->mipText) + 1;
						_this->miLine++;
					}
					JCLSeekForward(_this->mipText, 1);
				}
				else if( c == '#' || (c == '/' && nextChar == '/') )
				{
					// skip line up to line feed
					JCLSeekUntil(_this->mipText, "\n");
				}
				else if( c == '/' && nextChar == '*' )
				{
					comment = JILTrue;
					JCLSeekForward(_this->mipText, 2);
				}
				else
				{
					// found something non-white spacey
					break;
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
	const JCLToken* pi;
	for (pi = pTokenList; pi->name != NULL; pi++)
	{
		if (strcmp(string, pi->name) == 0)
			return pi->id;
	}
	return tk_unknown;
}

//------------------------------------------------------------------------------
// GetCurrentPosition
//------------------------------------------------------------------------------
// Gets the current position as line and column number.

void GetCurrentPosition(JCLFile* _this, JILLong* pColumn, JILLong* pLine)
{
	JILLong loc = _this->miLocator - 1; // because GetToken() advances BEFORE we can examine the token
	if( loc >= 0 && loc < _this->mipTokens->Count(_this->mipTokens) )
	{
		JCLFileToken* pft = _this->mipTokens->Get(_this->mipTokens, loc);
		*pColumn = pft->miColumn;
		*pLine = pft->miLine;
	}
	else
	{
		*pColumn = 0;
		*pLine = 0;
	}
}

//------------------------------------------------------------------------------
// GetStrLiteral
//------------------------------------------------------------------------------
// Reads a string literal from the input text stream and parses escape
// sequences like "\n", "\t", etc.

static JILError GetStrLiteral(JCLFile* _this, JCLString* string)
{
	JILError err = JCL_ERR_End_Of_File;
	JILLong c, q, p;
	char* pDummy;
	JCLString* pTemp = NULL;
	JILBool bEscape = JILTrue;

	JCLClear(string);
	// skip the start quote
	p = q = JCLGetCurrentChar(_this->mipText);
	if( p == '/' || p == '@' )
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
				case 'e':	// escape
					JCLAppend(string, "\x1B");
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
				else if( (c == '/' || c == '@') && JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == q )
				{
					p = c;
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
				if( p == '@' || JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == '/' )
				{
					// skip quote
					JCLSeekForward(_this->mipText, (p == '@') ? 1 : 2);
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
					else if( (c == '/' || c == '@') && JCLGetChar(_this->mipText, JCLGetLocator(_this->mipText) + 1) == q )
					{
						p = c;
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

//------------------------------------------------------------------------------
// JCLFileToken::Create
//------------------------------------------------------------------------------

void create_JCLFileToken(JCLFileToken* _this)
{
	_this->miLocation = 0;
	_this->miLine = 0;
	_this->miColumn = 0;
	_this->miTokenID = 0;
	_this->mipToken = NULL;
}

//------------------------------------------------------------------------------
// JCLFileToken::Copy
//------------------------------------------------------------------------------

void copy_JCLFileToken(JCLFileToken* _this, const JCLFileToken* src)
{
	_this->miLocation = src->miLocation;
	_this->miLine = src->miLine;
	_this->miColumn = src->miColumn;
	_this->miTokenID = src->miTokenID;
	if( src->mipToken )
	{
		_this->mipToken = NEW(JCLString);
		_this->mipToken->Copy(_this->mipToken, src->mipToken);
	}
}

//------------------------------------------------------------------------------
// JCLFileToken::Destroy
//------------------------------------------------------------------------------

void destroy_JCLFileToken(JCLFileToken* _this)
{
	DELETE(_this->mipToken);
}
