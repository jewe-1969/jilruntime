//------------------------------------------------------------------------------
// File: JCLOption.h                                           (c) 2005 jewe.org
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
/// @file jcloption.h
/// A class for maintaining compiler options, such as enabling warnings,
/// optimizations, and so forth.
//------------------------------------------------------------------------------

#ifndef JCLOPTION_H
#define JCLOPTION_H

#include "jclstring.h"
#include "jcltools.h"

/// Modes for MakeLocalVar()
enum
{
	kLocalAuto,					///< use registers if available, otherwise use stack (default)
	kLocalRegister,				///< force using registers, fail if none available (not recommended)
	kLocalStack					///< force using stack (never use registers)
};

/// Modes for Error JCLFormat option
enum
{
	kErrorFormatDefault = 1,	///< JewelScript error format
	kErrorFormatMS				///< Microsoft DeveloperStudio compatible error format
};

//------------------------------------------------------------------------------
// class JCLOption
//------------------------------------------------------------------------------

FORWARD_CLASS( JCLOption )
DECL_CLASS( JCLOption )

	JILError			(*ParseOption)		(JCLOption*, const JCLString*, JILOptionHandler, JILUnknown*);

	JILBool				miVerboseEnable;	///< output additional info
	JILBool				miWarningLevel;		///< output warnings
	JILLong				miLocalVarMode;		///< local variable code generation mode
	JILLong				miOptimizeLevel;	///< optimization level
	JILBool				miUseRTCHK;			///< use runtime type checking
	JILBool				miAllowFileImport;	///< allow import of additional scripts from local filesys
	JILBool				miDefaultFloat;		///< interpret all numeric literals as float
	JILLong				miErrorFormat;		///< error and warning output format
	JCLString*			mipFileExt;			///< script file extension to use for import
	Array_JILLong*		mipUsing;			///< list for using statement

END_CLASS( JCLOption )

#endif	// #ifndef JCLOPTION_H
