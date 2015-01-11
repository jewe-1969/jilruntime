//------------------------------------------------------------------------------
// File: JILDataSegment.h                                   (c) 2003 jewe.org
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
/// @file jilsegment.h
///	A segment is a consecutive block of memory of a distinct type or structure,
/// that can dynamically grow during compile-time, but is static afterwards.
/// Segments can be stored in binary chunks (@see JILSaveBinary). They are used
/// to store information about global constants, functions, and other data
/// needed by the virtual machine runtime.
/// This file uses preprocessor macros to define a generic "template" that can
/// be used to implement any type specific segment.
//------------------------------------------------------------------------------

#ifndef JILSEGMENT_H
#define JILSEGMENT_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// DECL_SEGMENT
//------------------------------------------------------------------------------

#define DECL_SEGMENT(T) \
	struct Seg_##T { \
		JILLong	usedSize; \
		JILLong	maxSize; \
		T*		pData; \
		}; \
	JILError InitSegment_##T (Seg_##T* _this, JILLong initialSize); \
	JILLong NewElement_##T (Seg_##T* _this, T** ppOut); \
	JILError DestroySegment_##T (Seg_##T* _this);

//------------------------------------------------------------------------------
// IMPL_SEGMENT
//------------------------------------------------------------------------------

#define IMPL_SEGMENT(T) \
	JILError InitSegment_##T (Seg_##T* _this, JILLong initialSize) \
	{ \
		_this->usedSize = 0; \
		_this->maxSize = initialSize; \
		_this->pData = (T*) malloc( initialSize * sizeof(T) ); \
		return JIL_No_Exception; \
	} \
	JILLong NewElement_##T (Seg_##T* _this, T** ppOut) \
	{ \
		T* pElement; \
		long result = 0; \
		if( _this->usedSize >= _this->maxSize ) \
		{ \
			long oldMax = _this->maxSize; \
			long newMax = oldMax + kSegmentAllocGrain; \
			T* pOldData = _this->pData; \
			_this->pData = (T*) malloc( newMax * sizeof(T) ); \
			_this->maxSize = newMax; \
			memcpy( _this->pData, pOldData, oldMax * sizeof(T) ); \
			free( pOldData ); \
		} \
		result = _this->usedSize++; \
		pElement = _this->pData + result; \
		memset( pElement, 0, sizeof(T) ); \
		*ppOut = pElement; \
		return result; \
	} \
	JILError DestroySegment_##T (Seg_##T* _this) \
	{ \
		if( _this->pData ) \
		{ \
			free( _this->pData ); \
			_this->pData = NULL; \
		} \
		_this->usedSize = 0; \
		_this->maxSize = 0; \
		return JIL_No_Exception; \
	}

//------------------------------------------------------------------------------
// Declare Segments
//------------------------------------------------------------------------------

DECL_SEGMENT( JILDataHandle )
DECL_SEGMENT( JILLong )
DECL_SEGMENT( JILFuncInfo )

#endif	// #ifndef JILSEGMENT_H
