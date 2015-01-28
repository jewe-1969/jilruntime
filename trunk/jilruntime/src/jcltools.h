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
// Pseudo class "templates" for rudimentary object oriented programming in C,
// and a dynamically growing array template.
// Seriously messy but extremely useful.
//------------------------------------------------------------------------------

#ifndef JCLTOOLS_H
#define JCLTOOLS_H

extern JILLong g_NewCalls;
extern JILLong g_DeleteCalls;

//------------------------------------------------------------------------------
// NEW and DELETE macros
//------------------------------------------------------------------------------

typedef JILUnknown* (*operator_new)(JILUnknown*);

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
	static T* operator_new_##T (JILUnknown* p)\
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
// Declares a managed array. Elements in this array are automatically destroyed.

#define ARRAY_PREALLOC_SIZE 32

#define	DECL_ARRAY(T) \
	typedef struct Array_##T Array_##T;\
	struct Array_##T {\
		void (*Create) (Array_##T*);\
		void (*Destroy) (Array_##T*);\
		void (*Copy) (Array_##T*, const Array_##T*);\
		void (*Add) (Array_##T*, T*);\
		void (*Set) (Array_##T*, JILLong, T*);\
		T* (*Get) (const Array_##T*, JILLong);\
		T* (*New) (Array_##T*);\
		void (*Trunc) (Array_##T*, JILLong);\
		JILLong (*Count) (const Array_##T*);\
		JILLong _i[3];\
		JILUnknown* _p[2];\
	};\
	struct JCLArray* operator_new_JCLArray(JILUnknown*, JILLong, operator_new);\
	static Array_##T* operator_new_Array_##T (JILUnknown* p)\
	{\
		return (Array_##T*) operator_new_JCLArray(p, sizeof(T), (operator_new)operator_new_##T);\
	}

//------------------------------------------------------------------------------
// macro DECL_UARRAY( TYPE )
//------------------------------------------------------------------------------
// Declares an unmanaged array. Elements in this array are not destroyed.

#define	DECL_UARRAY(T) \
	typedef struct UArray_##T UArray_##T;\
	struct UArray_##T {\
		void (*Create) (UArray_##T*);\
		void (*Destroy) (UArray_##T*);\
		void (*Copy) (UArray_##T*, const UArray_##T*);\
		void (*Add) (UArray_##T*, T*);\
		void (*Set) (UArray_##T*, JILLong, T*);\
		T* (*Get) (const UArray_##T*, JILLong);\
		T* (*New) (UArray_##T*);\
		void (*Trunc) (UArray_##T*, JILLong);\
		JILLong (*Count) (const UArray_##T*);\
		JILLong _i[3];\
		JILUnknown* _p[2];\
	};\
	struct JCLArray* operator_new_JCLArray(JILUnknown*, JILLong, operator_new);\
	static UArray_##T* operator_new_UArray_##T (JILUnknown* p)\
	{\
		return (UArray_##T*) operator_new_JCLArray(p, 0, NULL);\
	}

//------------------------------------------------------------------------------
// macro DECL_DATA_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Declares an array for storing simple data types (non-pointer types)

#define	DECL_DATA_ARRAY(T) \
	FORWARD_CLASS(Array_##T)\
	DECL_CLASS(Array_##T)\
		void (*Add) (Array_##T*, T);\
		void (*Set) (Array_##T*, JILLong, T);\
		T (*Get) (const Array_##T*, JILLong);\
		void (*Trunc) (Array_##T*, JILLong);\
		JILLong count; JILLong max; T* array;\
	END_CLASS(Array_##T)\
	void add_Array_##T (Array_##T*, T);\
	void set_Array_##T (Array_##T*, JILLong, T);\
	T get_Array_##T (const Array_##T*, JILLong);\
	void trunc_Array_##T (Array_##T*, JILLong);

//------------------------------------------------------------------------------
// macro IMPL_DATA_ARRAY( TYPE )
//------------------------------------------------------------------------------
// Implements an array for storing simple data types (no ctor / dtor calls).

#define IMPL_DATA_ARRAY(T) \
	void create_Array_##T (Array_##T* _this)\
	{\
		_this->Add = add_Array_##T;\
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
		JILLong i;\
		destroy_Array_##T (_this);\
		for( i = 0; i < src->count; i++ )\
			set_Array_##T (_this, i, src->Get (src, i));\
	}\
	void add_Array_##T (Array_##T* _this, T data)\
	{\
		set_Array_##T (_this, _this->count, data);\
	}\
	void set_Array_##T (Array_##T* _this, JILLong i, T data)\
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
	T get_Array_##T (const Array_##T* _this, JILLong i)\
	{\
		T res = 0;\
		if( i >= 0 && i < _this->count )\
			res = _this->array[i];\
		return res;\
	}\
	void trunc_Array_##T (Array_##T* _this, JILLong index)\
	{\
		if( index > _this->count )\
			index = _this->count;\
		_this->count = index;\
	}\

//------------------------------------------------------------------------------
// operator_delete
//------------------------------------------------------------------------------

void operator_delete (JILUnknown* p);

#endif	// #ifndef JCLTOOLS_H
