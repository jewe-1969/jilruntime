//------------------------------------------------------------------------------
// File: ntl_trex.h                                            (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description: 
// ------------
//	A regular expression object for JewelScript.
//
//------------------------------------------------------------------------------

#ifndef NTL_TREX_H
#define NTL_TREX_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "jilnativetype.h"
#include "jilstring.h"
#include "trex.h"

//------------------------------------------------------------------------------
// class NTrex
//------------------------------------------------------------------------------

typedef struct NTrex NTrex;

struct NTrex
{
	JILState*	pState;
	TRex*		pTrex;
	JILString*	pRegEx;
	JILString*	pSubMatch[10];
	JILLong		matchStart;
	JILLong		matchEnd;
};

//------------------------------------------------------------------------------
// trex functions
//------------------------------------------------------------------------------

// constructor, destructor, copy, assign
void				Trex_Create			(NTrex* _this, const JILChar* pRegEx);
void				Trex_Destroy		(NTrex* _this);
NTrex*				Trex_Clone			(NTrex* _this);
void				Trex_Set			(NTrex* _this, const NTrex* src);

JILLong				Trex_Match			(NTrex* _this, const JILString* pText);
JILLong				Trex_Search			(NTrex* _this, const JILString* pText);
JILLong				Trex_SearchRange	(NTrex* _this, const JILString* pText, JILLong start, JILLong length);
JILHandle*			Trex_MultiSearch	(NTrex* _this, const JILString* pText, const JILChar* pFormat);
JILError			Trex_DelegateSearch (NTrex* _this, const JILString* pText, JILHandle* hThis, JILHandle* hDelegate);
JILHandle*			Trex_Slice			(NTrex* _this, const JILString* pText);
void				Trex_Replace		(NTrex* _this, const JILString* pText, const JILChar* pReplace, JILString* pResult);
void				Trex_SubstSubMatch	(NTrex* _this, const JILChar* pFormat, JILString* pResult);
JILLong				Trex_SubMatch		(NTrex* _this, JILLong index, JILString* pOut);

NTrex*				New_Trex			(JILState* pState);
void				Delete_Trex			(NTrex* _this);

//------------------------------------------------------------------------------
// the type proc
//------------------------------------------------------------------------------

JILEXTERN int TrexProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut);

#endif	// #ifndef NTL_TREX_H
