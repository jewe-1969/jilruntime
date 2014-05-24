//------------------------------------------------------------------------------
// File: JILSymbolTable.h                                   (c) 2003 jewe.org
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
/// @file jilsymboltable.h
///	Provides functions to create, query and remove a symbol table for a JIL
///	program. The information in the symbol table is meaningless for the virtual
///	machine. It records it only for use with development tools such as
/// assemblers, compilers, debuggers or linkers.
//------------------------------------------------------------------------------

#ifndef JILSYMBOLTABLE_H
#define JILSYMBOLTABLE_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JILCreateSymbolTable
//------------------------------------------------------------------------------
// Create a symbol table.

JILError		JILCreateSymbolTable		(JILState* pState);

//------------------------------------------------------------------------------
// JILAddSymbolTableEntry
//------------------------------------------------------------------------------
// After creating a symbol table, subsequently call this for every data item you
// want to add to the table. "pName" is expected to be an user-defined identifier
// string. The identifier string should not contain the characters '?', '*' or
// '@' and isn't required to be unique in the symbol table.

JILError		JILAddSymbolTableEntry		(JILState* pState, const JILChar* pName, const JILUnknown* pData, JILLong size);

//------------------------------------------------------------------------------
// JILFindSymbolTableEntry
//------------------------------------------------------------------------------
// This finds and returns a symbol table entry by user-defined name. The search
// string can contain the wildcards '?' and '*'. The search is started at the
// entry with the index specified in "start". If a matching entry is found, a
// pointer to the data will be written to "ppData" and the size of the data
// will be written to "pSize". The index of the found entry will be returned.
// If no matching entry is found, the function returns -1 and does not alter
// ppData and pSize.

JILLong			JILFindSymbolTableEntry		(JILState* pState, const JILChar* pSearch, JILLong start, JILUnknown** ppData, JILLong* pSize, JILChar** ppName);

//------------------------------------------------------------------------------
// JILEnumSymbolTableEntries
//------------------------------------------------------------------------------
// This function enumerates (iterates through) all entries in the symbol table
// and calls the given callback "fn" for each entry, passing the JILState, the
// JILSymTabEntry and the user pointer as arguments to the callback. The
// enumeration is aborted, if the callback returns a value that is not
// JIL_No_Exception. The return value of the callback will also be the return
// value of this function. See "jiltypes.h" for the declaration of "fn".

JILError		JILEnumSymbolTableEntries	(JILState* pState, JILUnknown* user, JILSymTabEnumerator fn);

//------------------------------------------------------------------------------
// JILGetSymbolTableEntry
//------------------------------------------------------------------------------
// This returns the symbol table entry by index. A pointer to the data will be
// written to "ppData" and the size of the data will be written to "pSize".

JILError		JILGetSymbolTableEntry		(JILState* pState, JILLong index, JILUnknown** ppData, JILLong* pSize, JILChar** ppName);

//------------------------------------------------------------------------------
// JILGetNumSymbolTableEntries
//------------------------------------------------------------------------------
// Returns the number of entries in the symbol table. If no symbol table is
// present, returns 0.

JILLong			JILGetNumSymbolTableEntries	(JILState* pState);

//------------------------------------------------------------------------------
// JILGetSymbolTableChunkSize
//------------------------------------------------------------------------------
// Returns the size, in bytes, the symbol table will use when saved to a chunk.

JILLong			JILGetSymbolTableChunkSize	(JILState* pState);

//------------------------------------------------------------------------------
// JILWriteSymbolTableToChunk
//------------------------------------------------------------------------------
// Writes the symbol table to the buffer pointed to by pBuffer. Length specifies
// the maximum length the function is allowed to write.

JILError		JILWriteSymbolTableToChunk	(JILState* pState, JILChar* pBuffer, JILLong length);

//------------------------------------------------------------------------------
// JILReadSymbolTableFromChunk
//------------------------------------------------------------------------------
// Reads the symbol table from the buffer pointed to by pBuffer. Length specifies
// the number of bytes the function should read.

JILError		JILReadSymbolTableFromChunk	(JILState* pState, const JILChar* pBuffer, JILLong length);

//------------------------------------------------------------------------------
// JILRemoveSymbolTable
//------------------------------------------------------------------------------
// Removes the symbol table.

JILError		JILRemoveSymbolTable		(JILState* pState);

//------------------------------------------------------------------------------
// JILTruncateSymbolTable
//------------------------------------------------------------------------------
// Discards all entries that exceed the specified number of entries to keep.
// If "itemsToKeep" is for example 5, and there are 7 entries in the symbol
// table, the last two entries would be discarded.
// However, if there would be only 5 entries in the symbol table, then no
// entries would be discarded.

JILError		JILTruncateSymbolTable		(JILState* pState, JILLong itemsToKeep);

#endif	// #ifndef JILSYMBOLTABLE_H
