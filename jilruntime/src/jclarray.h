//------------------------------------------------------------------------------
// File: jclarray.h                                            (c) 2014 jewe.org
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
/// @file jclarray.h
/// Generic dynamic array used by the compiler. It can operate in "managed" and
/// "unmanaged" mode. In managed mode array elements are automatically created
/// using NEW() and destroyed with DELETE().
/// In unmanaged mode the array is just a simple container of pointers. It
/// does not create or destroy elements.
//------------------------------------------------------------------------------

#ifndef JCLARRAY_H
#define JCLARRAY_H

#include "jiltypes.h"
#include "jcltools.h"

//------------------------------------------------------------------------------
// struct JCLArray
//------------------------------------------------------------------------------
/// This is an OPAQUE STRUCT for DECL_PTR_ARRAY AND DECL_ARRAY from
/// "jcltools.h". If this struct is changed, those structs must be changed
/// accordingly to avoid crashes!

typedef struct JCLArray JCLArray;
struct JCLArray {
	void (*Create)		(JCLArray*);
	void (*Destroy)		(JCLArray*);
	void (*Copy)		(JCLArray*, const JCLArray*);
	void (*Add)			(JCLArray*, JILUnknown*);
	void (*Set)			(JCLArray*, JILLong, JILUnknown*);
	JILUnknown* (*Get)	(const JCLArray*, JILLong);
	JILUnknown* (*New)	(JCLArray*);
	void (*Trunc)		(JCLArray*, JILLong);
	JILLong (*Count)	(const JCLArray*);
	void (*Grain)		(JCLArray*, JILLong);

	JILLong count;
	JILLong max;
	JILLong size;				// element size; if > 0 this array is managed, if 0 it is unmanaged
	JILLong grain;				// number of elements to allocate per bucket
	JILUnknown** array;
	operator_new new_element;
};

//------------------------------------------------------------------------------
// methods
//------------------------------------------------------------------------------

JCLArray*	operator_new_JCLArray	(JILUnknown*, JILLong, operator_new);
void		create_JCLArray			(JCLArray*);
void		destroy_JCLArray		(JCLArray*);
void		copy_JCLArray			(JCLArray*, const JCLArray*);
void		add_JCLArray			(JCLArray*, JILUnknown*);
void		set_JCLArray			(JCLArray*, JILLong, JILUnknown*);
JILUnknown*	get_JCLArray			(const JCLArray*, JILLong);
JILUnknown*	new_JCLArray			(JCLArray*);
void		trunc_JCLArray			(JCLArray*, JILLong);
JILLong		count_JCLArray			(const JCLArray*);
void		grain_JCLArray			(JCLArray*, JILLong);

#endif // #ifndef JCLARRAY_H
