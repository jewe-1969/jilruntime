//------------------------------------------------------------------------------
// File: JILHandle.h                                        (c) 2003 jewe.org
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
/// @file jilhandle.h
/// Provides functions for fast allocation, initialization and deallocation of
/// handles. Every datatype the virtual machine deals with is encapsulated by
/// a handle. The purpose of a handle is mainly providing the VM with
/// information about the data type of the encapsulated object and to do
/// reference counting for that object. If the reference count of a handle
/// becomes zero, it means the handle (and the encapsulated object) is no more
/// needed and can therefore safely be freed (destroyed).
//------------------------------------------------------------------------------

#ifndef JILHANDLE_H
#define JILHANDLE_H

#include "jiltools.h"
#include "jildebug.h"

//------------------------------------------------------------------------------
// forward declare structs
//------------------------------------------------------------------------------

typedef struct JILHandleInt			JILHandleInt;
typedef struct JILHandleFloat		JILHandleFloat;
typedef struct JILHandleString		JILHandleString;
typedef struct JILHandleArray		JILHandleArray;
typedef struct JILHandleObject		JILHandleObject;
typedef struct JILHandleNObject		JILHandleNObject;
typedef struct JILHandleContext		JILHandleContext;
typedef struct JILHandleDelegate	JILHandleDelegate;

//------------------------------------------------------------------------------
// struct JILHandleInt
//------------------------------------------------------------------------------
/// Opaque struct that describes an int value.

struct JILHandleInt
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILLong		l;
};

//------------------------------------------------------------------------------
// struct JILHandleFloat
//------------------------------------------------------------------------------
/// Opaque struct that describes a float value.

struct JILHandleFloat
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILFloat	f;
};

//------------------------------------------------------------------------------
// struct JILHandleString
//------------------------------------------------------------------------------
/// Opaque struct that describes a JILString object. This struct needs to
/// remain in sync with JILHandleNObject!

struct JILHandleString
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILString*	str;		// pointer to string, keep opaque with JILHandleNObject::ptr
};

//------------------------------------------------------------------------------
// struct JILHandleArray
//------------------------------------------------------------------------------
/// Opaque struct that describes a JILArray object. This struct needs to
/// remain in sync with JILHandleNObject!

struct JILHandleArray
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILArray*	arr;		// pointer to array, keep opaque with JILHandleNObject::ptr
};

//------------------------------------------------------------------------------
// struct JILHandleObject
//------------------------------------------------------------------------------
/// Opaque struct that describes an instance of a class written in virtual
/// machine code.

struct JILHandleObject
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILHandle**	ppHandles;	// pointer to object (which is an array of pointers to JILHandle objects)
};

//------------------------------------------------------------------------------
// struct JILHandleNObject
//------------------------------------------------------------------------------
/// Opaque struct that describes an instance of a class written in C/C++.

struct JILHandleNObject
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILUnknown*	ptr;		// pointer to native object
};

//------------------------------------------------------------------------------
// struct JILHandleContext
//------------------------------------------------------------------------------
/// Opaque struct that describes a thread context

struct JILHandleContext
{
	JILLong		type;
	JILLong		flags;
	JILLong		refCount;
	JILContext*	pContext;	// pointer to context
};

//------------------------------------------------------------------------------
// struct JILHandleDelegate
//------------------------------------------------------------------------------
/// Opaque struct that describes a delegate

struct JILHandleDelegate
{
	JILLong			type;
	JILLong			flags;
	JILLong			refCount;
	JILDelegate*	pDelegate;	// pointer to delegate struct
};

//------------------------------------------------------------------------------
// JILInitHandles
//------------------------------------------------------------------------------
/// Allocates and initializes a bucket of handles of size 'allocGrain'.
/// Handle management is dynamic, if all handles of the current bucket are in
/// use, the next GetNewHandle() call will allocate the next bucket of handles.

JILError			JILInitHandles		(JILState* pState, JILLong allocGrain);

//------------------------------------------------------------------------------
// JILGetNewHandle
//------------------------------------------------------------------------------
/// Gets a new handle. If the preallocated "bucket" of handles is completely in
/// use, a new bucket will be allocated. Return value is the address of the
/// new handle, the handle will be initialized as type 'null' and with an initial
/// reference count of 1. If an error occurred, the return value is NULL.

JILHandle*			JILGetNewHandle		(JILState* pState);

//------------------------------------------------------------------------------
// JILFindHandleIndex
//------------------------------------------------------------------------------
/// Searches through all allocated runtime handles for the given handle
/// address and returns it's index number.

JILLong				JILFindHandleIndex	(JILState* pState, JILHandle* pHandle);

//------------------------------------------------------------------------------
// JILDestroyHandles
//------------------------------------------------------------------------------
/// This is called when the VM terminates to destroy the handle buffers.
/// Do not call this function as a user of the library.

JILError			JILDestroyHandles	(JILState* pState);

//------------------------------------------------------------------------------
// JILCopyHandle
//------------------------------------------------------------------------------
/// Copies the object encapsulated by the source handle and returns the copy with
/// a new handle. All objects are copied, regardless if they are value-types or
/// reference-types.

JILError			JILCopyHandle		(JILState* pState, JILHandle* srcHandle, JILHandle** ppOut);

//------------------------------------------------------------------------------
// JILCopyValueType
//------------------------------------------------------------------------------
/// Copies the object encapsulated by the source handle and returns the copy with
/// a new handle. The object is only physically copied, if it is a value-type.
/// If the object is a reference type, a new reference is added to the source
/// handle and it is returned as result.

JILError			JILCopyValueType	(JILState* pState, JILHandle* srcHandle, JILHandle** ppOut);

//------------------------------------------------------------------------------
// JILCreateWeakRef
//------------------------------------------------------------------------------
/// Creates a "weak reference" from the given source handle. The weak reference
/// will point to the same object than the source handle, but it will _NOT_
/// destroy the referenced object when it's reference counter reaches 0.
/// Only use this if you can guarantee that the source object will have a LONGER
/// life-time than your weak reference will.

JILHandle*			JILCreateWeakRef	(JILState* pState, JILHandle* srcHandle);

//------------------------------------------------------------------------------
// JILMarkHandle
//------------------------------------------------------------------------------
/// Called by the garbage collector to mark all handles still in use.

JILError			JILMarkHandle		(JILState* pState, JILHandle* pHandle);

//------------------------------------------------------------------------------
// JILDestroyObject
//------------------------------------------------------------------------------
/// Called to destroy the object encapsulated by the given handle.

void				JILDestroyObject	(JILState* pState, JILHandle* pHandle);

//------------------------------------------------------------------------------
// JILCreateException
//------------------------------------------------------------------------------
/// Create and return an exception object and return it's handle.

JILHandle*			JILCreateException	(JILState* pState, JILError err);

//------------------------------------------------------------------------------
// JILGetNullHandle
//------------------------------------------------------------------------------
/// Return the 'null handle', which is the only single handle that has the type
/// type_null.

JILINLINE JILHandle* JILGetNullHandle(JILState* pState)
{
	return *pState->vmppHandles;
}

//------------------------------------------------------------------------------
// JILAddRef
//------------------------------------------------------------------------------
/// Add a reference to a handle.

#define JILAddRef(HANDLE)	(HANDLE)->refCount++;

//------------------------------------------------------------------------------
// JILRelease
//------------------------------------------------------------------------------
/// Release a reference to a handle. If the reference count reaches 0, the handle
/// will be freed and the encapsulated object will be destroyed.

#define JILRelease(STATE, HANDLE) \
{\
	JILState* _S = (STATE);\
	JILHandle* _H = (HANDLE);\
	if( --_H->refCount == 0 )\
	{\
		JILDestroyObject( _S, _H );\
		_S->vmppFreeHandles[--_S->vmUsedHandles] = _H;\
	}\
}

#endif	// #ifndef JILHANDLE_H
