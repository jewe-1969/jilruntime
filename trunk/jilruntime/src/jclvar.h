//------------------------------------------------------------------------------
// File: JCLVar.h                                           (c) 2004 jewe.org
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
/// @file jclvar.h
/// This is used by the compiler to describe a storage location. This can be a
/// register, or a location on the stack, for example. The JewelScript compiler
/// allocates registers and stack locations automatically and dynamically.
/// This file is included from JCLState.h.
//------------------------------------------------------------------------------

#ifndef JCLVAR_H
#define JCLVAR_H

#include "jiltypes.h"
#include "jcltools.h"

// miMode:
enum { kModeUnused, kModeRegister, kModeStack, kModeMember, kModeArray };

// miUsage:
enum { kUsageVar, kUsageTemp, kUsageResult };

// flags for JCLVar::ToString() and JCLFunc::ToString()
enum
{
	kIdentNames		= 1 << 0,	// include identifier names
	kFullDecl		= 1 << 1,	// full function declaration statement (includes method, function, etc keywords)
	kCompact		= 1 << 2,	// no extra spaces or other formatting
	kCurrentScope	= 1 << 3,	// add name of current class scope (JCLVar only)
	kClearFirst		= 1 << 4,	// clear the given string before writing into it
	kNoClassName	= 1 << 5	// do not include the class name and scope operator
};

//------------------------------------------------------------------------------
// class JCLVar
//------------------------------------------------------------------------------

FORWARD_CLASS( JCLVar )
DECL_CLASS( JCLVar )

	void				(*CopyType)		(JCLVar*, const JCLVar*);
	JCLString*			(*ToString)		(JCLVar*, JCLState*, JCLString*, JILLong);
	JCLString*			(*ToXml)		(JCLVar*, JCLState*, JCLString*);

	// TYPE related
	JILLong				miType;			// typeID this object currently represents (can change due to cast, access to array elements, etc)
	JILBool				miConst;		// is a constant
	JILBool				miRef;			// is a reference
	JILBool				miWeak;			// is weak (requires miRef == JILTrue)
	JILLong				miElemType;		// array element type (in the case of an array)
	JILBool				miElemRef;		// array elements are references

	// VARIABLE related
	JCLString*			mipName;		// variable name
	JCLVar*				mipArrIdx;		// array access: variable containing index
	JILLong				miMode;			// see enum
	JILLong				miUsage;		// see enum
	JILLong				miIndex;		// register # or stack address: index(sp)
	JILLong				miMember;		// member index if type is object
	JILLong				miIniType;		// typeID this object was initially created with, this should never change
	JILBool				miInited;		// has been initialized
	JILBool				miUnique;		// temp var already copied
	JILBool				miConstP;		// member access: object is const
	JILBool				miOnStack;		// var is currently on SimStack
	JILBool				miTypeCast;		// type-cast operator was encountered (for 'explicit')
	JILBool				miHidden;		// marked as hidden (can't be found when searching for a variable)

END_CLASS( JCLVar )

//------------------------------------------------------------------------------
// struct TypeInfo
//------------------------------------------------------------------------------
// Helper struct to describe the result type of an expression to caller.
// Keep this in sync with JCLVar type data.

typedef struct
{
	JILLong				miType;			// JIL type identifier
	JILBool				miConst;		// is a constant
	JILBool				miRef;			// is a reference
	JILBool				miWeak;			// is weak (requires miRef == JILTrue)
	JILLong				miElemType;		// array element type (in the case of an array)
	JILBool				miElemRef;		// array elements are references
} TypeInfo;

void JCLClrTypeInfo		(TypeInfo*);
void JCLSetTypeInfo		(TypeInfo*, JILLong type, JILBool bConst, JILBool bRef, JILBool bWeak, JILLong eType, JILBool eRef);
void JCLTypeInfoFromVar	(TypeInfo*, const JCLVar*);
void JCLTypeInfoSrcDst	(TypeInfo*, const JCLVar*, const JCLVar*);
void JCLTypeInfoToVar	(const TypeInfo*, JCLVar*);
void JCLTypeInfoCopy	(TypeInfo*, const TypeInfo*);

//------------------------------------------------------------------------------
// Declare array template for type JCLVar
//------------------------------------------------------------------------------

DECL_ARRAY( JCLVar )
DECL_DATA_ARRAY( JILLong )

#endif	// #ifndef JCLVAR_H
