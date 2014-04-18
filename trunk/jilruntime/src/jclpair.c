//------------------------------------------------------------------------------
// File: JCLPair.c                                             (c) 2012 jewe.org
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
/// @file jclpair.c
/// A general purpose class that holds two data members. The first is considered
/// the key of a collection item, the second it's data. There are also
/// predefined methods to add, remove and search for data by key.
/// The key is always a JCLString, the data can be any object instantiated by
/// the NEW() macro.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclpair.h"

//------------------------------------------------------------------------------
// JCLPair
//------------------------------------------------------------------------------
// constructor

void create_JCLPair( JCLPair* _this )
{
	_this->mipKey = NEW(JCLString);
	_this->mipData = NULL;
}

//------------------------------------------------------------------------------
// JCLPair
//------------------------------------------------------------------------------
// destructor

void destroy_JCLPair( JCLPair* _this )
{
	DELETE(_this->mipKey);
	DELETE(_this->mipData);
}

//------------------------------------------------------------------------------
// JCLPair::Copy
//------------------------------------------------------------------------------
// Copy all data from a JCLPair to this JCLPair

void copy_JCLPair(JCLPair* _this, const JCLPair* src)
{
	_this->mipKey->Copy(_this->mipKey, src->mipKey);
	((JCLString*) _this->mipData)->Copy(_this->mipData, src->mipData);
}

//------------------------------------------------------------------------------
// Implement array template for JCLPair
//------------------------------------------------------------------------------

IMPL_ARRAY( JCLPair )

//------------------------------------------------------------------------------
// Add()
//------------------------------------------------------------------------------

JCLPair* Add_JCLCollection(JCLCollection* _this, const JCLString* pKey, JILUnknown* pData)
{
	JCLPair* res = _this->New(_this);
	res->mipKey->Copy(res->mipKey, pKey);
	res->mipData = pData;
	return res;
}

//------------------------------------------------------------------------------
// Remove()
//------------------------------------------------------------------------------

void Remove_JCLCollection(JCLCollection* _this, const JCLString* pKey)
{
	JILLong index = IndexOf_JCLCollection(_this, pKey);
	if( index >= 0 )
	{
		JCLPair* pp = _this->Get(_this, index);
		DELETE(pp);
		if( (index + 1) < _this->count )
			memcpy(_this->array + index, _this->array + index + 1, (_this->count - index) * sizeof(JCLPair*));
		_this->count -= 1;
	}
}

//------------------------------------------------------------------------------
// Get()
//------------------------------------------------------------------------------

JCLPair* Get_JCLCollection(JCLCollection* _this, const JCLString* pKey)
{
	JILLong index;
	for( index = 0; index < _this->count; index++ )
	{
		JCLPair* pp = _this->Get(_this, index);
		if( strcmp(JCLGetString(pp->mipKey), JCLGetString(pKey)) == 0 )
		{
			return pp;
		}
	}
	return NULL;
}

//------------------------------------------------------------------------------
// GetAt()
//------------------------------------------------------------------------------

JCLPair* GetAt_JCLCollection(JCLCollection* _this, JILLong index)
{
	if( index >= 0 && index < _this->count )
		return _this->Get(_this, index);
	else
		return NULL;
}

//------------------------------------------------------------------------------
// GetData()
//------------------------------------------------------------------------------

JILUnknown* GetData_JCLCollection(JCLCollection* _this, JILLong index)
{
	JCLPair* pp = GetAt_JCLCollection(_this, index);
	if( pp )
		return pp->mipData;
	else
		return NULL;
}

//------------------------------------------------------------------------------
// GetKey()
//------------------------------------------------------------------------------

JCLString* GetKey_JCLCollection(JCLCollection* _this, JILLong index)
{
	JCLPair* pp = GetAt_JCLCollection(_this, index);
	if( pp )
		return pp->mipKey;
	else
		return NULL;
}

//------------------------------------------------------------------------------
// IndexOf()
//------------------------------------------------------------------------------

JILLong IndexOf_JCLCollection(JCLCollection* _this, const JCLString* pKey)
{
	JILLong index;
	for( index = 0; index < _this->count; index++ )
	{
		JCLPair* pp = _this->Get(_this, index);
		if( strcmp(JCLGetString(pp->mipKey), JCLGetString(pKey)) == 0 )
			return index;
	}
	return -1;
}

//------------------------------------------------------------------------------
// Count()
//------------------------------------------------------------------------------

JILLong Count_JCLCollection(JCLCollection* _this)
{
	return _this->count;
}
