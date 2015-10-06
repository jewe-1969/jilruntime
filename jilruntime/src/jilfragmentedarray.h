//------------------------------------------------------------------------------
// File: JILFragmentedArray.h                                  (c) 2015 jewe.org
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
/// @file jilfragmentedarray.h
/// A new array class for JewelScript. This implementation does not require
/// reallocation and does not copy large data blocks when the array size is
/// increased. Instead, a new fragment is appended to a logical list of
/// fragments that make up the entire array.
//------------------------------------------------------------------------------

#ifndef JILFRAGMENTEDARRAY_H
#define JILFRAGMENTEDARRAY_H

#include "jiltypes.h"

typedef struct JILFragmentedArray JILFragmentedArray;

typedef JILUnknown* (*JILElementCtor)(JILFragmentedArray*);
typedef void(*JILElementDtor)(JILFragmentedArray*, JILUnknown*);
typedef void(*JILElementSet)(JILFragmentedArray*, JILUnknown* _old, JILUnknown* _new);
typedef JILUnknown* (*JILElementGet)(const JILFragmentedArray*, JILUnknown*);

//------------------------------------------------------------------------------
// JILFragmentedArray
//------------------------------------------------------------------------------

struct JILFragmentedArray
{
	JILLong                     count;      //!< the logical number of elements in this array
	JILLong                     fragments;  //!< the current number of allocated fragments
	JILLong                     epf;        //!< number of elements per fragment
	JILLong                     fid;        //!< fragment index divisor (right shift)
    JILLong                     tocSize;    //!< number of fragments in TOC
	JILUnknown*			        pUser;      //!< general purpose user data pointer
	struct JILArrayFragment*    pLast;      //!< points to last array fragment
	struct JILArrayFragment**   ppToc;      //!< table of contents (array of all fragments)

	// events
	JILElementCtor              onCreate;   //!< sent for every element that is created
	JILElementDtor              onDestroy;  //!< sent for every element that is destroyed
	JILElementSet               onSet;      //!< sent when an element is set to a new value
	JILElementGet               onGet;      //!< sent when an element is retrieved from the array
};

//------------------------------------------------------------------------------
// methods
//------------------------------------------------------------------------------

BEGIN_JILEXTERN

JILFragmentedArray* JILFragmentedArray_Create           (JILLong granularity);
void                JILFragmentedArray_Destroy          (JILFragmentedArray* _this);
void                JILFragmentedArray_Copy             (JILFragmentedArray* _this, const JILFragmentedArray* src);
void                JILFragmentedArray_UpdateToc        (const JILFragmentedArray* _this);
void                JILFragmentedArray_SetLength        (JILFragmentedArray* _this, JILLong length);
void                JILFragmentedArray_Set              (JILFragmentedArray* _this, JILLong index, JILUnknown* pElement);
JILUnknown*         JILFragmentedArray_Get              (const JILFragmentedArray* _this, JILLong index);
void                JILFragmentedArray_PushElement      (JILFragmentedArray* _this, JILUnknown* pElement);
JILUnknown*         JILFragmentedArray_PopElement       (JILFragmentedArray* _this);

JILINLINE JILLong   JILFragmentedArray_GetLength        (const JILFragmentedArray* _this) { return _this->count; }
JILINLINE JILBool   JILFragmentedArray_InBounds         (const JILFragmentedArray* _this, JILLong index) { return (index >= 0 && index < _this->count); }

END_JILEXTERN

#endif // JILFRAGMENTEDARRAY_H
