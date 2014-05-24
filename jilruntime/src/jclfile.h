//------------------------------------------------------------------------------
// File: JCLFile.h                                          (c) 2004 jewe.org
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
/// @file jclfile.h
/// A pseudo-class that represents a code snippet or whole source text file to be
/// compiled by the JewelScript compiler.
/// This file is included from JCLState.h.
//------------------------------------------------------------------------------

#ifndef JCLFILE_H
#define JCLFILE_H

#include "jiltypes.h"
#include "jcltools.h"

FORWARD_CLASS(JCLOption);

//------------------------------------------------------------------------------
// class JCLFileToken
//------------------------------------------------------------------------------

FORWARD_CLASS( JCLFileToken )
DECL_CLASS( JCLFileToken )

	JILLong				miTokenID;		///< The ID number of the token
	JILLong				miLocation;		///< The character position in the file of the token
	JCLString*			mipToken;		///< The token string or NULL

END_CLASS( JCLFileToken )

//------------------------------------------------------------------------------
// Array_JCLFileToken
//------------------------------------------------------------------------------

DECL_ARRAY( JCLFileToken )

//------------------------------------------------------------------------------
// class JCLFile
//------------------------------------------------------------------------------
// Represents a source code "file" or code-snippet in form of a zero-terminated
// C-string

FORWARD_CLASS( JCLFile )
DECL_CLASS( JCLFile )

	JILError			(*Open)			(JCLFile*, const JILChar*, const JILChar*, const JILChar*, JCLOption*);
	JILError			(*GetToken)		(JCLFile*, JCLString*, JILLong*);
	JILError			(*PeekToken)	(JCLFile*, JCLString*, JILLong*);
	JILLong				(*GetLocator)	(JCLFile*);
	void				(*SetLocator)	(JCLFile*, JILLong);
	JILError			(*Close)		(JCLFile*);

	JCLString*			mipName;		///< Name of file or code-snippet
	JCLString*			mipText;		///< The source code
	JCLString*			mipPath;		///< Filename and path of the file
	Array_JCLFileToken*	mipTokens;		///< Array of tokens
	JCLOption*			mipOtions;		///< Current compiler options
	JILLong				miLocator;		///< Current parsing position
	JILLong				miPass;			///< Current compile pass
	JILBool				miNative;		///< File is a native type declaration

END_CLASS( JCLFile )

//------------------------------------------------------------------------------
// struct JCLToken
//------------------------------------------------------------------------------

typedef struct
{
	JILLong			id;
	const JILChar*	name;

} JCLToken;

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

void		GetCurrentPosition	(JCLFile* _this, JILLong* pColumn, JILLong* pLine);
JILLong		GetTokenID			(const JILChar* string, const JCLToken* pTokenList);

//------------------------------------------------------------------------------
// the token ID's
//------------------------------------------------------------------------------

typedef enum
{
	tk_unknown = 0,

	// keywords
	tk_keywords,

	tk_accessor = tk_keywords,
	tk_alias,
	tk_and,
	tk_array,
	tk_break,
	tk_case,
	tk_class,
	tk_clause,
	tk_cofunction,
	tk_const,
	tk_continue,
	tk_default,
	tk_delegate,
	tk_do,
	tk_else,
	tk_explicit,
	tk_extern,
	tk_false,
	tk_float,
	tk_for,
	tk_function,
	tk_goto,
	tk_hybrid,
	tk_if,
	tk_import,
	tk_interface,
	tk_int,
	tk_method,
	tk_namespace,
	tk_native,
	tk_new,
	tk_not,
	tk_null,
	tk_option,
	tk_or,
	tk_return,
	tk_sameref,
	tk_strict,
	tk_string,
	tk_switch,
	tk_this,
	tk_throw,
	tk_true,
	tk_typeof,
	tk_using,
	tk_var,
	tk_weak,
	tk_while,
	tk_yield,
	tk__brk,		// place brk instruction
	tk__rtchk,		// place rtchk instruction
	tk__selftest,	// compiler self-test directive

	tk_num_keywords,

	// operators
	tk_operators = tk_num_keywords,

	// arithmetic
	tk_plus = tk_operators,
	tk_minus,
	tk_mul,
	tk_div,
	tk_mod,

	// binary
	tk_equ,
	tk_greater,
	tk_greater_equ,
	tk_less,
	tk_less_equ,
	tk_not_equ,

	// bitwise
	tk_band,
	tk_bor,
	tk_xor,
	tk_bnot,
	tk_lshift,
	tk_rshift,

	// assignment operators
	tk_assign,
	tk_plus_assign,
	tk_minus_assign,
	tk_mul_assign,
	tk_div_assign,
	tk_mod_assign,
	tk_band_assign,
	tk_bor_assign,
	tk_xor_assign,
	tk_lshift_assign,
	tk_rshift_assign,

	// other operators
	tk_plusplus,
	tk_minusminus,

	tk_num_operators,

	// other characters
	tk_characters = tk_num_operators,

	tk_colon = tk_characters,
	tk_scope,
	tk_comma,
	tk_semicolon,
	tk_point,
	tk_bullets,

	// brackets
	tk_round_open,
	tk_round_close,
	tk_curly_open,
	tk_curly_close,
	tk_square_open,
	tk_square_close,

	tk_num_characters,

	// identifier (any sequence of letters that is not a keyword)
	tk_identifier = tk_num_characters,

	// literals
	tk_lit_int,
	tk_lit_float,
	tk_lit_string,
	tk_lit_char,	// single quoted character literal

	tk_num_tokens

} TokenID;

#endif	// #ifndef JCLFILE_H
