//------------------------------------------------------------------------------
// File: JILChunk.h                                         (c) 2003 jewe.org
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
/// @file jilchunk.h
/// Serialization functions for the JILRuntime library. Provides functions to
/// load a binary program from a chunk, or save a binary program to a chunk.
//------------------------------------------------------------------------------

#ifndef JILCHUNK_H
#define JILCHUNK_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// LoadFromChunk
//------------------------------------------------------------------------------
// Loads bytecode from a binary chunk.

JILError			LoadFromChunk			(JILState* pState, const JILUnknown* pData, JILLong dataSize);

//------------------------------------------------------------------------------
// SaveToChunk
//------------------------------------------------------------------------------
// Save bytecode to a binary chunk.

JILError			SaveToChunk				(JILState* pState, JILUnknown** ppData, JILLong* pDataSize);

#endif	// #ifndef JILCHUNK_H
