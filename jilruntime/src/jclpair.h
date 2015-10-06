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

FORWARD_CLASS(JCLPair)

//------------------------------------------------------------------------------
// class JCLPair
//------------------------------------------------------------------------------
/// A general purpose class that holds a key string and a data value.

DECL_CLASS( JCLPair )

	JCLString*	mipKey;		//!< The key string of this pair.
	JILUnknown*	mipData;	//!< The data value of this pair.

END_CLASS( JCLPair )

DECL_ARRAY( JCLPair )

typedef Array_JCLPair	JCLCollection;

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

JCLPair*	Add_JCLCollection		(JCLCollection* _this, const JCLString* pKey, JILUnknown* pData);
JCLPair*	Get_JCLCollection		(JCLCollection* _this, const JCLString* pKey);
JCLPair*	GetAt_JCLCollection		(JCLCollection* _this, JILLong index);
JILUnknown*	GetData_JCLCollection	(JCLCollection* _this, JILLong index);
JCLString*	GetKey_JCLCollection	(JCLCollection* _this, JILLong index);
JILLong		IndexOf_JCLCollection	(JCLCollection* _this, const JCLString* pKey);
JILLong		Count_JCLCollection		(JCLCollection* _this);

#endif	// #ifndef JCLPAIR_H
