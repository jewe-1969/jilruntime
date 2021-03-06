//------------------------------------------------------------------------------
// File: JCLFunc.h                                          (c) 2004 jewe.org
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
/// @file jclfunc.h
/// Contains structures and functions that describe a JewelScript function or method.
/// JewelScript code can only be placed inside a function.
/// This file is included from JCLState.h.
//------------------------------------------------------------------------------

#ifndef JCLFUNC_H
#define JCLFUNC_H

#include "jiltypes.h"
#include "jcltools.h"

FORWARD_CLASS(JCLLiteral)
FORWARD_CLASS(JCLFunc)
FORWARD_CLASS(JCLFuncType)

//------------------------------------------------------------------------------
// class JCLLiteral
//------------------------------------------------------------------------------
/// Helper class that stores a literal constant.

DECL_CLASS(JCLLiteral)

	JILLong				miType;		//!< handle type (int, float, string, delegate)
	JILLong				miHandle;	//!< JIL data handle index (or 0)
	JILLong				miOffset;	//!< offset in byte code for patching
	JILLong				miLong;		//!< integer value in case of integer literals
	JILFloat			miFloat;	//!< float value in case of float literals
	JCLString*			miString;	//!< string constant in case of string literals
	JILLong				miLocator;	//!< character position of code block in source file (for anonymous delegates)
	JILBool				miMethod;	//!< anonymous delegate is method or closure
	struct JCLFile*		mipFile;	//!< source file (for anonymous delegates)
	Array_JCLVar*		mipStack;	//!< stack context in case of closure

END_CLASS( JCLLiteral )

DECL_ARRAY( JCLLiteral )

//------------------------------------------------------------------------------
// class JCLFunc
//------------------------------------------------------------------------------
/// Describes a JewelScript function.

DECL_CLASS( JCLFunc )

	JILError			(*LinkCode)			(JCLFunc*, JCLState*);
	JCLString*			(*ToString)			(JCLFunc*, JCLState*, JCLString*, JILLong);
	JCLString*			(*ToXml)			(JCLFunc*, JCLState*, JCLString*);
	void				(*CopyDeclaration)	(JCLFunc*, const JCLFunc*);

	JCLString*			mipName;		// function name
	JCLString*			mipTag;			// tag string
	JILLong				miHandle;		// JIL data handle index
	JILLong				miFuncIdx;		// index of function
	JILLong				miClassID;		// typeID of class
	JILLong				miLnkDelegate;	// link-to-delegate: index of member variable holding delegate
	JILLong				miLnkMethod;	// link-to-method: index of method / accessor to call
	JILLong				miLnkRelIdx;	// index of source function for code relocation
	JILLong				miLnkAddr;		// code address after linking
	JILLong				miLnkClass;		// link-to-method: class type ID for miLnkMethod
	JILLong				miLnkVarOffset;	// variable relocation offset
	JILBool				miRetFlag;		// encountered return statement
	JILBool				miYieldFlag;	// encountered yield statement
	JILBool				miMethod;		// is a member function
	JILBool				miCtor;			// is a constructor
	JILBool				miConvertor;	// is a convertor
	JILBool				miAccessor;		// is an accessor function
	JILBool				miCofunc;		// is a cofunction
	JILBool				miAnonymous;	// is a anonymous (local) function
	JILBool				miExplicit;		// constructor / convertor declared explicit
	JILBool				miStrict;		// fail if this function has no body during link stage
	JILBool				miVirtual;		// call this function as a virtual method
	JILBool				miNoOverride;	// the function is not overridable
	JILBool				miPrivate;		// the function is private
	JILBool				miLinked;		// the function has been linked
	JILBool				miNaked;		// do not save / restore registers for this function
	JILLong				miOptLevel;		// optimization level saved from compiler options
	JCLVar*				mipResult;		// result var / type
	Array_JCLVar*		mipArgs;		// function argument list
	Array_JILLong*		mipCode;		// buffer to compile code to
	Array_JCLLiteral*	mipLiterals;	// literals
	Array_JCLVar*		mipParentStack;	// parent stack in case of closure
	JILLong				miRegUsage[kNumRegisters];	// counts how often regs were allocated in function

END_CLASS( JCLFunc )

//------------------------------------------------------------------------------
// Declare array template for type JCLFunc
//------------------------------------------------------------------------------

DECL_ARRAY( JCLFunc )

//------------------------------------------------------------------------------
// class JCLFuncType
//------------------------------------------------------------------------------
/// Describes a JCL function type, also known as the "signature" of a
/// function or method. It contains only the result type and types of the
/// functions argument list.

DECL_CLASS( JCLFuncType )

	JCLString*			(*ToString)		(JCLFuncType*, JCLState*, JCLString*, JCLString*, JILLong, JILLong);
	JCLString*			(*ToXml)		(JCLFuncType*, JCLState*, JCLString*, JILLong);

	JCLVar*				mipResult;		// result type
	Array_JCLVar*		mipArgs;		// argument list

END_CLASS( JCLFuncType )

//------------------------------------------------------------------------------
// Declare array template for type JCLFuncType
//------------------------------------------------------------------------------

DECL_ARRAY( JCLFuncType )

#endif	// #ifndef JCLFUNC_H
