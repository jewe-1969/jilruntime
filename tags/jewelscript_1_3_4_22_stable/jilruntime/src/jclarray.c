//------------------------------------------------------------------------------
// File: jclarray.c                                            (c) 2014 jewe.org
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
/// @file jclarray.c
/// Generic dynamic array used by the compiler. It can operate in "managed" and
/// "unmanaged" mode. In managed mode array elements are automatically created
/// using NEW() and destroyed with DELETE().
/// In unmanaged mode the array is just a simple container of pointers. It
/// does not create or destroy elements.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclarray.h"
#include "jclstring.h"

//------------------------------------------------------------------------------
// private opaque struct _object
//------------------------------------------------------------------------------

typedef struct {
	void (*Create)	(JILUnknown*);
	void (*Destroy)	(JILUnknown*);
	void (*Copy)	(JILUnknown*, const JILUnknown*);
} _object;

//------------------------------------------------------------------------------
// operator_delete
//------------------------------------------------------------------------------

void operator_delete (JILUnknown* p)
{
	if( p )
	{
		_object* ip = (_object*) p;
		ip->Destroy (p);
		g_DeleteCalls++;

#ifdef _DEBUG
		// this will cause an exception if we try to DELETE this instance again
		memset(ip, 0xDD, sizeof(_object));
#endif
	}
}

//------------------------------------------------------------------------------
// copy_element
//------------------------------------------------------------------------------

static JILUnknown* copy_element(JCLArray* _this, JILUnknown* obj)
{
	if( _this->new_element )
	{
		_object* d = _this->new_element(malloc(_this->size));
		_object* s = obj;
		s->Copy(d, s);
		return d;
	}
	else
	{
		return obj;
	}
}

//------------------------------------------------------------------------------
// operator_new_JCLArray
//------------------------------------------------------------------------------

JCLArray* operator_new_JCLArray(JILUnknown* p, JILLong size, operator_new c)
{
	JCLArray* q = (JCLArray*) p;
	memset(q, 0, sizeof(JCLArray));
	q->size = size;
	q->new_element = c;
	q->Create = create_JCLArray;
	q->Destroy = destroy_JCLArray;
	q->Copy = copy_JCLArray;
	q->Create (q);
	g_NewCalls++;
	return q;
}

//------------------------------------------------------------------------------
// create_JCLArray
//------------------------------------------------------------------------------

void create_JCLArray(JCLArray* _this)
{
	if( _this->size )
	{
		// only allow new in managed mode
		_this->New = new_JCLArray;
	}
	else
	{
		// disallow add / set in managed mode
		_this->Add = add_JCLArray;
		_this->Set = set_JCLArray;
	}
	_this->Get = get_JCLArray;
	_this->Trunc = trunc_JCLArray;
	_this->Count = count_JCLArray;
	_this->Grain = grain_JCLArray;
	_this->max = _this->count = 0;
	_this->grain = ARRAY_PREALLOC_SIZE;
	_this->array = NULL;
}

//------------------------------------------------------------------------------
// destroy_JCLArray
//------------------------------------------------------------------------------

void destroy_JCLArray(JCLArray* _this)
{
	if( _this->size )
	{
		JILLong i;
		for( i = 0; i < _this->count; i++ )
		{
			operator_delete(_this->array[i]);
			if( _this->array[i] )
				free(_this->array[i]);
		}
	}
	if( _this->array )
		free(_this->array);
	_this->max = _this->count = 0;
	_this->grain = ARRAY_PREALLOC_SIZE;
	_this->array = NULL;
}

//------------------------------------------------------------------------------
// copy_JCLArray
//------------------------------------------------------------------------------

void copy_JCLArray(JCLArray* _this, const JCLArray* src)
{
	JILLong i;
	destroy_JCLArray(_this);
	_this->grain = src->grain;
	for( i = 0; i < src->count; i++ )
		set_JCLArray(_this, i, copy_element(_this, get_JCLArray(src, i)));
}

//------------------------------------------------------------------------------
// add_JCLArray
//------------------------------------------------------------------------------

void add_JCLArray(JCLArray* _this, JILUnknown* data)
{
	set_JCLArray(_this, _this->count, data);
}

//------------------------------------------------------------------------------
// set_JCLArray
//------------------------------------------------------------------------------

void set_JCLArray(JCLArray* _this, JILLong i, JILUnknown* data)
{
	if( i >= _this->max )
	{
		JILUnknown** oldptr = _this->array;
		_this->max = i + _this->grain;
		_this->array = malloc (_this->max * sizeof(JILUnknown*));
		if( oldptr )
		{
			memcpy (_this->array, oldptr, _this->count * sizeof(JILUnknown*));
			free (oldptr);
		}
	}
	_this->array[i] = data;
	if( i >= _this->count )
		_this->count = i + 1;
}

//------------------------------------------------------------------------------
// get_JCLArray
//------------------------------------------------------------------------------

JILUnknown* get_JCLArray(const JCLArray* _this, JILLong i)
{
	JILUnknown* res = NULL;
	if( i >= 0 && i < _this->count )
		res = _this->array[i];
	return res;
}

//------------------------------------------------------------------------------
// new_JCLArray
//------------------------------------------------------------------------------

JILUnknown* new_JCLArray(JCLArray* _this)
{
	if( _this->new_element )
	{
		JILUnknown* ptr = _this->new_element(malloc(_this->size));
		set_JCLArray(_this, _this->count, ptr);
		return ptr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// trunc_JCLArray
//------------------------------------------------------------------------------

void trunc_JCLArray(JCLArray* _this, JILLong index)
{
	if( index > _this->count )
		index = _this->count;
	if( _this->size )
	{
		JILLong i;
		for( i = index; i < _this->count; i++ )
		{
			operator_delete(_this->array[i]);
			if( _this->array[i] )
				free(_this->array[i]);
		}
	}
	_this->count = index;
}

//------------------------------------------------------------------------------
// count_JCLArray
//------------------------------------------------------------------------------

JILLong count_JCLArray(const JCLArray* _this)
{
	return _this->count;
}

//------------------------------------------------------------------------------
// grain_JCLArray
//------------------------------------------------------------------------------

void grain_JCLArray(JCLArray* _this, JILLong grainSize)
{
	if (grainSize > 0)
		_this->grain = grainSize;
}
