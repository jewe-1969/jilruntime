//------------------------------------------------------------------------------
// File: JCLClass.h                                         (c) 2004 jewe.org
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
/// @file jclclass.h
/// Contains structures and functions that describes a JewelScript class.
/// JewelScript functions can only be placed inside a class. Even class-less
/// global functions are compiled into a class called "__global". The compiler
/// will automatically generate this class.
/// This file is included from JCLState.h.
/// <p>
/// Starting with JewelScript version 0.9, a JCLClass instance is created for
/// every and any type, not only JewelScript classes. This means the JCLClass
/// is used rather like a "type info" than a "class info" object.
/// </p>
//------------------------------------------------------------------------------

#ifndef JCLCLASS_H
#define JCLCLASS_H

#include "jclpair.h"

/// class / function modifier flags
enum
{
	  kModeNativeBinding	= 1 << 0	//!< class, used as 'tag' for native binding generator
	, kModeNativeInterface	= 1 << 1	//!< interface
	, kModeFunction			= 1 << 2	//!< function
	, kModeMethod			= 1 << 3	//!< function
	, kModeAccessor			= 1 << 4	//!< function
	, kModeCofunction		= 1 << 5	//!< function
	, kModeExplicit			= 1 << 6	//!< function
	, kModeStrict			= 1 << 7	//!< class, interface, function
	, kModeVirtual			= 1 << 8	//!< class, function
	, kModePrivate			= 1 << 9	//!< function
};

//------------------------------------------------------------------------------
// Declare array templates
//------------------------------------------------------------------------------

FORWARD_CLASS(JCLClass)
DECL_UARRAY(JCLClass)
DECL_ARRAY(JCLString)

//------------------------------------------------------------------------------
// class JCLClass
//------------------------------------------------------------------------------
/// Describes a JewelScript type. (Not just a class!)

DECL_CLASS( JCLClass )

	JCLString*			(*ToXml)		(JCLClass*, JCLState*, JCLString*);

	JCLString*			mipName;		//!< class name
	JCLString*			mipTag;			//!< tag string
	JILLong				miType;			//!< type identifier
	JILLong				miBaseType;		//!< base typeID, if this class is inherited, otherwise 0
	JILLong				miHybridBase;	//!< typeID of base class if class is "hybrid", otherwise 0
	JILLong				miParentType;	//!< typeID of parent class of this type
	JILLong				miFamily;		//!< type family, see enum JILTypeFamily
	JILLong				miModifier;		//!< modifiers, such as "extern" or "native"
	JILBool				miNative;		//!< is a native type
	JILBool				miHasBody;		//!< is declared (not only forwarded)
	JILBool				miHasVTable;	//!< linker has generated v-table for this class
	JILBool				miHasCtor;		//!< class has at least one constructor
	JILBool				miHasMethod;	//!< class has at least one method
	Array_JCLFunc*		mipFuncs;		//!< member functions
	Array_JCLVar*		mipVars;		//!< member variables
	Array_JCLString*	mipAlias;		//!< aliases for this type
	Array_JILLong*		mipInherits;	//!< type IDs of inherited classes
	JCLFuncType*		mipFuncType;	//!< signature of a delegate or cofunction type
	JILMethodInfo		miMethodInfo;	//!< info about special methods like ctor, copy-ctor and dtor

END_CLASS( JCLClass )

//------------------------------------------------------------------------------
// Declare array template for type JCLClass
//------------------------------------------------------------------------------

DECL_ARRAY( JCLClass )

#endif	// #ifndef JCLCLASS_H
