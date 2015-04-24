//------------------------------------------------------------------------------
// File: JILArray.h                                            (c) 2015 jewe.org
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
/// @file jilarray.h
/// This is the built-in array class. The JewelScript array can dynamically grow
/// depending on the index used to access elements from it. In general, accessing
/// an array element with an index that is out of range will cause the array to
/// grow to the required number of elements. So one should be cautious to not use
/// invalid array indices. The array index is a signed 32-bit value.
/// Operator += can be used to add new elements to an array, as well as append an
/// array to an array.
//------------------------------------------------------------------------------

#ifndef JILARRAY_H
#define JILARRAY_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// struct JILArray
//------------------------------------------------------------------------------
/// This is the built-in dynamic array object used by the virtual machine.

struct JILArray
{
	JILLong		size;		///< Currently used size, in elements, of the array
	JILLong		maxSize;	///< Currently allocated size, in elements; if size reaches this value, the array is resized
	JILHandle**	ppHandles;	///< Pointer to the handles of the elements in this array
	JILState*	pState;		///< The virtual machine object this array 'belongs' to
};

//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------

BEGIN_JILEXTERN

JILArray*		JILArray_New(JILState* pState);
JILArray*		JILArray_NewNoInit(JILState* pState, JILLong size);
void			JILArray_Delete(JILArray* _this);
void			JILArray_SetSize(JILArray* _this, JILLong newSize);
JILArray*		JILArray_FillWithType(JILState* pState, JILLong type, JILLong size);
JILArray*		JILArray_Copy(const JILArray* pSource);
void			JILArray_ArrMove(JILArray* _this, JILHandle* pHandle);
void			JILArray_ArrCopy(JILArray* _this, JILHandle* pHandle);
void			JILArray_MoveTo(JILArray* _this, JILLong index, JILHandle* pHandle);
void			JILArray_CopyTo(JILArray* _this, JILLong index, JILHandle* pHandle);
JILHandle*		JILArray_GetFrom(JILArray* _this, JILLong index);
JILHandle**		JILArray_GetEA(JILArray* _this, JILLong index);

JILArray*		JILArray_DeepCopy(const JILArray* pSource);
JILArray*		JILArray_Insert(JILArray* _this, JILArray* src, JILLong index);
JILArray*		JILArray_InsertItem(JILArray* _this, JILHandle* src, JILLong index);
JILArray*		JILArray_Remove(JILArray* _this, JILLong index, JILLong length);
JILArray*		JILArray_SubArray(JILArray* _this, JILLong index, JILLong length);
void			JILArray_Swap(JILArray* _this, JILLong index1, JILLong index2);
JILError		JILArray_Sort(JILArray* _this, JILHandle* pDelegate, JILArray** ppNew);
JILLong			JILArray_IndexOf(JILArray* _this, JILHandle* hItem, JILLong index);

JILString*		JILArray_Format(JILArray* _this, JILString* pFormat);
JILString*		JILArray_ToString(JILArray* _this);
JILError		JILArray_Process(const JILArray* _this, JILHandle* pDelegate, JILHandle* pArgs, JILArray** ppNew);
JILError		JILArray_Enumerate(JILArray* _this, JILHandle* pDelegate, JILHandle* pArgs);

// These two routines might be helpful, too
void			JILArrayHandleToString(JILState*, JILString*, JILHandle*);
void			JILArrayHandleToStringF(JILState*, JILString*, const JILString*, JILHandle*);

//------------------------------------------------------------------------------
// JILArrayProc
//------------------------------------------------------------------------------
/// The main proc of the built-in array class.

JILError JILArrayProc(NTLInstance* pInst, JILLong msg, JILLong param, JILUnknown* pDataIn, JILUnknown** ppDataOut);

END_JILEXTERN

#endif	// #ifndef JILARRAY_H
