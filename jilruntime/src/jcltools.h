//------------------------------------------------------------------------------
// File: JCLTools.h                                         (c) 2004 jewe.org
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
/// @file jcltools.h
/// Pseudo class "templates" for rudimentary object oriented programming in C,
/// and a dynamically growing array template.
///	Seriously "messy" but extremely useful. ;-)
/// This file is included from JCLState.h.
//------------------------------------------------------------------------------

#ifndef JCLTOOLS_H
#define JCLTOOLS_H

extern JILLong g_NewCalls;
extern JILLong g_DeleteCalls;

//------------------------------------------------------------------------------
// NEW and DELETE macros
//------------------------------------------------------------------------------

// Use this to allocate objects dynamically and call constructor
#define NEW(T)		operator_new_##T (malloc (sizeof (T)))

// Use this to delete objects allocated with NEW
#define DELETE(P)	do{ operator_delete(P); if(P) free(P); } while(0)

//------------------------------------------------------------------------------
// macro FORWARD_CLASS( TYPE )
//------------------------------------------------------------------------------
// This can be used to forward declare a "class"

#define FORWARD_CLASS(T) \
	typedef struct T T;

//------------------------------------------------------------------------------
// macro DECL_CLASS( TYPE )
//------------------------------------------------------------------------------
// Begins a rudimentary class declaration with constructor, copy function and
// destructor.

#define DECL_CLASS(T) \
	struct T {\
		void (*Create) (T*);\
		void (*Destroy) (T*);\
		void (*Copy) (T*, const T*);

//------------------------------------------------------------------------------
// macro END_CLASS( TYPE )
//------------------------------------------------------------------------------
// Ends a rudimentary class declaration.

#define END_CLASS(T) \
	};\
	void create_##T (T*);\
	void destroy_##T (T*);\
	void copy_##T (T*, const T*);\
	static T* operator_new_##T (void* p)\
	{\
		T* q = (T*) p;\
		q->Create = create_##T;\
		q->Destroy = destroy_##T;\
		q->Copy = copy_##T;\
		q->Create (q);\
		g_NewCalls++;\
		return q;\
	}

//------------------------------------------------------------------------------
// macro DECL_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Declares all member functions and data members of our array object.
#define ARRAY_PREALLOC_SIZE 32

#define	DECL_ARRAY(T) \
	FORWARD_CLASS(Array_##T)\
	DECL_CLASS(Array_##T)\
		T* (*New) (Array_##T*);\
		T* (*Get) (const Array_##T*, long);\
		void (*Trunc) (Array_##T*, long);\
		long count; long max; T** array;\
	END_CLASS(Array_##T)\
	T* new_Array_##T (Array_##T*);\
	T* get_Array_##T (const Array_##T*, long);\
	void trunc_Array_##T (Array_##T*, long);

//------------------------------------------------------------------------------
// macro IMPL_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Defines / implements all member functions of our array object.

#define IMPL_ARRAY(T) \
	void create_Array_##T (Array_##T* _this)\
	{\
		_this->New = new_Array_##T;\
		_this->Get = get_Array_##T;\
		_this->Trunc = trunc_Array_##T;\
		_this->max = _this->count = 0;\
		_this->array = 0;\
	}\
	void destroy_Array_##T (Array_##T* _this)\
	{\
		int i;\
		for( i = 0; i < _this->count; i++ )\
			DELETE(_this->array[i]);\
		if(_this->array) free (_this->array);\
		_this->max = _this->count = 0;\
		_this->array = 0;\
	}\
	void copy_Array_##T (Array_##T* _this, const Array_##T* src)\
	{\
		int i; T* d;\
		destroy_Array_##T (_this);\
		for( i = 0; i < src->count; i++ )\
		{\
			d = new_Array_##T (_this);\
			d->Copy (d, src->Get (src, i));\
		}\
	}\
	T* new_Array_##T (Array_##T* _this)\
	{\
		T* res;\
		if( _this->count == _this->max )\
		{\
			T** oldptr = _this->array;\
			_this->max += ARRAY_PREALLOC_SIZE;\
			_this->array = malloc (_this->max * sizeof(T*));\
			if( oldptr )\
			{\
				memcpy (_this->array, oldptr, _this->count * sizeof(T*));\
				free (oldptr);\
			}\
		}\
		res = NEW(T);\
		_this->array[_this->count] = res;\
		_this->count++;\
		return res;\
	}\
	T* get_Array_##T (const Array_##T* _this, long index)\
	{\
		T* res = 0;\
		if( index >= 0 && index < _this->count )\
			res = _this->array[index];\
		return res;\
	}\
	void trunc_Array_##T (Array_##T* _this, long index)\
	{\
		int i;\
		if( index > _this->count )\
			index = _this->count;\
		for( i = index; i < _this->count; i++ )\
			DELETE(_this->array[i]);\
		_this->count = index;\
	}\

//------------------------------------------------------------------------------
// macro DECL_DATA_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Declares an array for storing simple data types (no ctor / dtor calls).

#define	DECL_DATA_ARRAY(T) \
	FORWARD_CLASS(Array_##T)\
	DECL_CLASS(Array_##T)\
		void (*Set) (Array_##T*, long, T);\
		T (*Get) (const Array_##T*, long);\
		void (*Trunc) (Array_##T*, long);\
		long count; long max; T* array;\
	END_CLASS(Array_##T)\
	void set_Array_##T (Array_##T*, long, T);\
	T get_Array_##T (const Array_##T*, long);\
	void trunc_Array_##T (Array_##T*, long);

//------------------------------------------------------------------------------
// macro IMPL_DATA_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Implements an array for storing simple data types (no ctor / dtor calls).

#define IMPL_DATA_ARRAY(T) \
	void create_Array_##T (Array_##T* _this)\
	{\
		_this->Set = set_Array_##T;\
		_this->Get = get_Array_##T;\
		_this->Trunc = trunc_Array_##T;\
		_this->max = _this->count = 0;\
		_this->array = 0;\
	}\
	void destroy_Array_##T (Array_##T* _this)\
	{\
		free (_this->array);\
		_this->max = _this->count = 0;\
		_this->array = 0;\
	}\
	void copy_Array_##T (Array_##T* _this, const Array_##T* src)\
	{\
		long i;\
		destroy_Array_##T (_this);\
		for( i = 0; i < src->count; i++ )\
			set_Array_##T (_this, i, src->Get (src, i));\
	}\
	void set_Array_##T (Array_##T* _this, long i, T data)\
	{\
		if( i >= _this->max )\
		{\
			T* oldptr = _this->array;\
			_this->max = i + ARRAY_PREALLOC_SIZE;\
			_this->array = malloc (_this->max * sizeof(T));\
			if( oldptr )\
			{\
				memcpy (_this->array, oldptr, _this->count * sizeof(T));\
				free (oldptr);\
			}\
		}\
		_this->array[i] = data;\
		if( i >= _this->count )\
			_this->count = i + 1;\
		return;\
	}\
	T get_Array_##T (const Array_##T* _this, long i)\
	{\
		T res = 0;\
		if( i >= 0 && i < _this->count )\
			res = _this->array[i];\
		return res;\
	}\
	void trunc_Array_##T (Array_##T* _this, long index)\
	{\
		if( index > _this->count )\
			index = _this->count;\
		_this->count = index;\
	}\

//------------------------------------------------------------------------------
// operator_delete
//------------------------------------------------------------------------------

static void operator_delete (void* p)
{
	if( p )
	{
		typedef struct {
			void (*Create) (void*);
			void (*Destroy) (void*);
			void (*Copy) (void*, const void*);
		} _inst;

		_inst* ip = (_inst*) p;
		ip->Destroy (p);
		g_DeleteCalls++;

#ifdef _DEBUG
		// this will cause an exception if we try to DELETE this instance again
		memset(ip, 0xDD, sizeof(_inst));
#endif
	}
}

#endif	// #ifndef JCLTOOLS_H
