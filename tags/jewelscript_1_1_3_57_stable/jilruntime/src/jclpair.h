//------------------------------------------------------------------------------
// File: JCLPair.h											   (c) 2012 jewe.org
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
/// @file jclpair.h
/// A general purpose class that holds two data members. The first is considered
/// the key of a collection item, the second it's data. There are also
/// predefined methods to add, remove and search for data by key.
/// The key is always a JCLString, the data can be any object instantiated by
/// the NEW() macro.
//------------------------------------------------------------------------------

#ifndef JCLPAIR_H
#define JCLPAIR_H

#include "jclstring.h"
#include "jcltools.h"

//------------------------------------------------------------------------------
// class JCLPair
//------------------------------------------------------------------------------

FORWARD_CLASS( JCLPair )
DECL_CLASS( JCLPair )

	JCLString*	mipKey;
	JILUnknown*	mipData;

END_CLASS( JCLPair )

DECL_ARRAY( JCLPair )

typedef Array_JCLPair	JCLCollection;

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

JCLPair*	Add_JCLCollection		(JCLCollection* _this, const JCLString* pKey, JILUnknown* pData);
void		Remove_JCLCollection	(JCLCollection* _this, const JCLString* pKey);
JCLPair*	Get_JCLCollection		(JCLCollection* _this, const JCLString* pKey);
JCLPair*	GetAt_JCLCollection		(JCLCollection* _this, JILLong index);
JILUnknown*	GetData_JCLCollection	(JCLCollection* _this, JILLong index);
JCLString*	GetKey_JCLCollection	(JCLCollection* _this, JILLong index);
JILLong		IndexOf_JCLCollection	(JCLCollection* _this, const JCLString* pKey);
JILLong		Count_JCLCollection		(JCLCollection* _this);

#endif	// #ifndef JCLPAIR_H
