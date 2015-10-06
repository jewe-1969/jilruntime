//------------------------------------------------------------------------------
// File: JILFragmentedArray.c                                  (c) 2015 jewe.org
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
/// @file jilfragmentedarray.c
/// A new array class for JewelScript. This implementation does not require
/// reallocation and does not copy large data blocks when the array size is
/// increased. Instead, a new fragment is appended to a logical list of
/// fragments that make up the entire array.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilfragmentedarray.h"
#include "jiltools.h"

//------------------------------------------------------------------------------
// private declarations
//------------------------------------------------------------------------------

typedef struct JILArrayFragment JILArrayFragment;
struct JILArrayFragment
{
    JILArrayFragment*   prev;
    JILUnknown**        data;
};

static JILArrayFragment* JILFragmentedArray_AddFragment(JILFragmentedArray*);
static void JILFragmentedArray_RemoveFragment(JILFragmentedArray*);

//------------------------------------------------------------------------------
// JILFragmentedArray_Create
//------------------------------------------------------------------------------

JILFragmentedArray* JILFragmentedArray_Create(JILLong granularity)
{
    JILFragmentedArray* result = malloc(sizeof(JILFragmentedArray));
    memset(result, 0, sizeof(JILFragmentedArray));
    result->fid = JILMAX(JILMIN(granularity, 16), 4);
    result->epf = 1 << result->fid;
    return result;
}

//------------------------------------------------------------------------------
// JILFragmentedArray_Destroy
//------------------------------------------------------------------------------

void JILFragmentedArray_Destroy(JILFragmentedArray* _this)
{
    // free all fragments
    while (_this->fragments)
        JILFragmentedArray_RemoveFragment(_this);
    _this->count = 0;
    free(_this);
}

//------------------------------------------------------------------------------
// JILFragmentedArray_Copy
//------------------------------------------------------------------------------
// ATTENTION: When using this with event callbacks that perform ref-counting on
// elements, this will add TWO refs to each element. Therefore, onGet should be
// set to NULL before calling this!

void JILFragmentedArray_Copy(JILFragmentedArray* _this, const JILFragmentedArray* src)
{
    JILLong i;
    JILLong n = JILFragmentedArray_GetLength(src);
    for (i = 0; i < n; i++)
        JILFragmentedArray_PushElement(_this, JILFragmentedArray_Get(src, i));
}

//------------------------------------------------------------------------------
// JILFragmentedArray_UpdateToc
//------------------------------------------------------------------------------

void JILFragmentedArray_UpdateToc(const JILFragmentedArray* _this)
{
    if (_this->fragments != _this->tocSize)
    {
        JILArrayFragment** ppI;
        JILArrayFragment* ptr;
        if (_this->ppToc != NULL)
            free(_this->ppToc);
        // TOC is mutable also for constant objects
        ((JILFragmentedArray*)_this)->ppToc = (JILArrayFragment**)malloc(_this->fragments * sizeof(JILArrayFragment*));
        ((JILFragmentedArray*)_this)->tocSize = _this->fragments;
        // fill TOC with fragments
        ppI = _this->ppToc + _this->fragments;
        for (ptr = _this->pLast; ptr != NULL; ptr = ptr->prev)
            *(--ppI) = ptr;
    }
}

//------------------------------------------------------------------------------
// JILFragmentedArray_SetLength
//------------------------------------------------------------------------------

void JILFragmentedArray_SetLength(JILFragmentedArray* _this, JILLong length)
{
    JILLong fragsNeeded = (length > 0) ? ((length - 1) >> _this->fid) + 1 : 0;
    if (_this->fragments < fragsNeeded)
    {
        // increase array size
        while (_this->fragments < fragsNeeded)
            JILFragmentedArray_AddFragment(_this);
        _this->count = length;
    }
    else
    {
        // decrease array size
        while (_this->fragments > fragsNeeded)
            JILFragmentedArray_RemoveFragment(_this);
        _this->count = JILMAX(length, 0);
    }
}

//------------------------------------------------------------------------------
// JILFragmentedArray_Set
//------------------------------------------------------------------------------

void JILFragmentedArray_Set(JILFragmentedArray* _this, JILLong index, JILUnknown* pElement)
{
    if (JILFragmentedArray_InBounds(_this, index))
    {
        JILArrayFragment* ptr;
        JILUnknown** ppElem;
        if (_this->ppToc == NULL)
            JILFragmentedArray_UpdateToc(_this);
        ptr = _this->ppToc[index >> _this->fid];
        ppElem = ptr->data + (index & (_this->epf - 1));
        if (_this->onSet != NULL)
            _this->onSet(_this, *ppElem, pElement);
        *ppElem = pElement;
    }
}

//------------------------------------------------------------------------------
// JILFragmentedArray_Get
//------------------------------------------------------------------------------

JILUnknown* JILFragmentedArray_Get(const JILFragmentedArray* _this, JILLong index)
{
    JILUnknown* pElem = NULL;
    if (JILFragmentedArray_InBounds(_this, index))
    {
        JILArrayFragment* ptr;
        if (_this->ppToc == NULL)
            JILFragmentedArray_UpdateToc(_this);
        ptr = _this->ppToc[index >> _this->fid];
        pElem = ptr->data[index & (_this->epf - 1)];
        if (_this->onGet != NULL)
            pElem = _this->onGet(_this, pElem);
    }
    return pElem;
}

//------------------------------------------------------------------------------
// JILFragmentedArray_PushElement
//------------------------------------------------------------------------------

void JILFragmentedArray_PushElement(JILFragmentedArray* _this, JILUnknown* pElement)
{
    JILLong index = _this->count;
    JILFragmentedArray_SetLength(_this, index + 1);
    JILFragmentedArray_Set(_this, index, pElement);
}

//------------------------------------------------------------------------------
// JILFragmentedArray_PopElement
//------------------------------------------------------------------------------

JILUnknown* JILFragmentedArray_PopElement(JILFragmentedArray* _this)
{
    JILUnknown* result = NULL;
    if (_this->count > 0)
    {
        JILLong index = _this->count - 1;
        result = JILFragmentedArray_Get(_this, index);
        JILFragmentedArray_SetLength(_this, index);
    }
    return result;
}

//------------------------------------------------------------------------------
// JILArrayFragment_AddFragment
//------------------------------------------------------------------------------

static JILArrayFragment* JILFragmentedArray_AddFragment(JILFragmentedArray* _this)
{
    JILLong n = _this->epf;
    JILArrayFragment* frag = malloc(sizeof(JILArrayFragment) + sizeof(JILUnknown*) * n);
    JILUnknown** ppE = (JILUnknown**)(frag + 1);
    JILElementCtor fn = _this->onCreate;
    frag->prev = _this->pLast;
    frag->data = ppE;
    _this->pLast = frag;
    _this->fragments++;
    // initialize all elements
    if (fn != NULL)
    {
        while (n--)
            *ppE++ = fn(_this);
    }
    else
    {
        while (n--)
            *ppE++ = NULL;
    }
    return frag;
}

//------------------------------------------------------------------------------
// JILArrayFragment_RemoveFragment
//------------------------------------------------------------------------------

static void JILFragmentedArray_RemoveFragment(JILFragmentedArray* _this)
{
    if (_this->fragments > 0)
    {
        JILArrayFragment* frag = _this->pLast;
        JILLong n = _this->epf;
        JILUnknown** ppE = frag->data;
        JILElementDtor fn = _this->onDestroy;
        _this->pLast = frag->prev;
        _this->fragments--;
        // destroy all elements
        if (fn != NULL)
        {
            while (n--)
                fn(_this, *ppE++);
        }
        frag->data = NULL;
        frag->prev = NULL;
        free(frag);
    }
}
