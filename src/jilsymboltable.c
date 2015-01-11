//------------------------------------------------------------------------------
// File: JILSymbolTable.c                                   (c) 2003 jewe.org
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
//	Provides functions to create, query and remove a symbol table for a JIL
//	program. The information in the symbol table is meaningless for the virtual
//	machine. It records it only for use with development tools such as
//  assemblers, compilers, debuggers or linkers.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilsymboltable.h"
#include "jiltools.h"

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static int JILMatchStrings(const char* expr, const char* string);

//------------------------------------------------------------------------------
// JILCreateSymbolTable
//------------------------------------------------------------------------------
// Create a symbol table.

JILError JILCreateSymbolTable(JILState* pState)
{
	JILError result = JIL_No_Exception;

	// if we already have a symbol table, discard it now
	JILRemoveSymbolTable(pState);

	return result;
}

//------------------------------------------------------------------------------
// JILAddSymbolTableEntry
//------------------------------------------------------------------------------
// After creating a symbol table, subsequently call this for every data item
// you want to add to the table. "pName" is expected to be an user-defined
// identifier string. The identifier string should only contain the characters
// 0-9 @ A-Z a-z _

JILError JILAddSymbolTableEntry(JILState* pState, const JILChar* pName, const JILUnknown* pData, JILLong size)
{
	JILError result = JIL_No_Exception;
	JILLong nLength;
	JILSymTabEntry* pEntry;

	// allocate a new entry
	nLength = strlen(pName);
	pEntry = (JILSymTabEntry*) malloc(sizeof(JILSymTabEntry));
	pEntry->pName = (JILChar*) malloc(nLength + 1);
	pEntry->pData = malloc(size);
	pEntry->sizeName = nLength;
	pEntry->sizeData = size;
	JILStrcpy(pEntry->pName, nLength + 1, pName);
	memcpy(pEntry->pData, pData, size);

	if( !pState->vmpSymTabFirst )
	{
		pState->vmpSymTabFirst = pState->vmpSymTabLast = pEntry;
		pEntry->pPrev = pEntry->pNext = NULL;
	}
	else
	{
		pState->vmpSymTabLast->pNext = pEntry;
		pEntry->pPrev = pState->vmpSymTabLast;
		pState->vmpSymTabLast = pEntry;
		pEntry->pNext = NULL;
	}
	return result;
}

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

JILLong JILFindSymbolTableEntry(JILState* pState, const JILChar* pSearch, JILLong start, JILUnknown** ppData, JILLong* pSize, JILChar** ppName)
{
	JILLong result = -1;
	JILSymTabEntry* pEntry;
	JILLong index = 0;

	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		if( index >= start && JILMatchStrings(pSearch, pEntry->pName) )
		{
			if( ppData )
				*ppData = pEntry->pData;
			if( pSize )
				*pSize = pEntry->sizeData;
			if( ppName )
				*ppName = pEntry->pName;
			result = index;
			break;
		}
		index++;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILEnumSymbolTableEntries
//------------------------------------------------------------------------------
// This function enumerates (iterates through) all entries in the symbol table
// and calls the given callback "fn" for each entry, passing the JILState, the
// JILSymTabEntry and the user pointer as arguments to the callback. The
// enumeration is aborted, if the callback returns a value that is not
// JIL_No_Exception. The return value of the callback will also be the return
// value of this function. See "jiltypes.h" for the declaration of "fn".

JILError JILEnumSymbolTableEntries(JILState* pState, JILUnknown* user, JILSymTabEnumerator fn)
{
	JILError result = JIL_No_Exception;
	JILSymTabEntry* pEntry;
	JILLong index = 0;

	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		result = fn(pState, index++, pEntry, user);
		if( result )
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILGetSymbolTableEntry
//------------------------------------------------------------------------------
// This returns the symbol table entry by index. A pointer to the data will be
// written to "ppData" and the size of the data will be written to "pSize".

JILError JILGetSymbolTableEntry(JILState* pState, JILLong anIndex, JILUnknown** ppData, JILLong* pSize, JILChar** ppName)
{
	JILError result = JIL_ERR_Symbol_Not_Found;
	JILSymTabEntry* pEntry;
	JILLong index = 0;

	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		if( index == anIndex )
		{
			if( ppData )
				*ppData = pEntry->pData;
			if( pSize )
				*pSize = pEntry->sizeData;
			if( ppName )
				*ppName = pEntry->pName;
			result = JIL_No_Exception;
			break;
		}
		index++;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILGetNumSymbolTableEntries
//------------------------------------------------------------------------------
// Returns the number of entries in the symbol table. If no symbol table is
// present, returns 0.

JILLong JILGetNumSymbolTableEntries(JILState* pState)
{
	JILSymTabEntry* pEntry;
	JILLong result = 0;
	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		result++;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILGetSymbolTableChunkSize
//------------------------------------------------------------------------------
// Returns the size, in bytes, the symbol table will use when saved to a chunk.

JILLong JILGetSymbolTableChunkSize(JILState* pState)
{
	JILSymTabEntry* pEntry;
	JILLong result = 0;
	JILLong sizeDataAlg;
	JILLong sizeNameAlg;
	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		sizeDataAlg = (pEntry->sizeData + 3) & 0x7FFFFFFC;
		sizeNameAlg = (pEntry->sizeName + 3) & 0x7FFFFFFC;
		result += (sizeNameAlg + sizeDataAlg + 2 * sizeof(JILLong));
	}
	return result;
}

//------------------------------------------------------------------------------
// JILWriteSymbolTableToChunk
//------------------------------------------------------------------------------
// Writes the symbol table to the buffer pointed to by pBuffer. Length specifies
// the maximum length the function is allowed to write.

JILError JILWriteSymbolTableToChunk(JILState* pState, JILChar* pBuffer, JILLong length)
{
	JILError result = JIL_No_Exception;
	JILSymTabEntry* pEntry;
	JILLong check = 0;
	JILLong sizeDataAlg;
	JILLong sizeNameAlg;
	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pEntry->pNext )
	{
		sizeDataAlg = (pEntry->sizeData + 3) & 0x7FFFFFFC;
		sizeNameAlg = (pEntry->sizeName + 3) & 0x7FFFFFFC;
		check += (sizeNameAlg + sizeDataAlg + 2 * sizeof(JILLong));
		if( check > length )
			return JIL_ERR_Save_Chunk_Failed;
		// write length of name
		memcpy(pBuffer, &(pEntry->sizeName), sizeof(JILLong));
		pBuffer += sizeof(JILLong);
		// write name
		memcpy(pBuffer, pEntry->pName, pEntry->sizeName);
		pBuffer += sizeNameAlg;
		// write size of data
		memcpy(pBuffer, &(pEntry->sizeData), sizeof(JILLong));
		pBuffer += sizeof(JILLong);
		// write data
		memcpy(pBuffer, pEntry->pData, pEntry->sizeData);
		pBuffer += sizeDataAlg;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILReadSymbolTableFromChunk
//------------------------------------------------------------------------------
// Reads the symbol table from the buffer pointed to by pBuffer. Length specifies
// the number of bytes the function should read.

JILError JILReadSymbolTableFromChunk(JILState* pState, const JILChar* pBuffer, JILLong length)
{
	JILLong result = JIL_No_Exception;
	JILLong read;
	JILLong sizeNameAlg;
	JILLong sizeDataAlg;
	JILSymTabEntry* pEntry;
	// discard any existing symbol table
	JILRemoveSymbolTable(pState);
	// read
	for( read = 0; read < length; )
	{
		// allocate a new entry
		pEntry = (JILSymTabEntry*) malloc(sizeof(JILSymTabEntry));
		// read name length
		read += sizeof(JILLong);
		if( read > length )
			goto error;
		memcpy(&(pEntry->sizeName), pBuffer, sizeof(JILLong));
		pBuffer += sizeof(JILLong);
		sizeNameAlg = (pEntry->sizeName + 3) & 0x7FFFFFFC;
		// read name
		read += sizeNameAlg;
		if( read > length )
			goto error;
		pEntry->pName = (JILChar*) malloc(pEntry->sizeName + 1);
		memcpy(pEntry->pName, pBuffer, pEntry->sizeName);
		pEntry->pName[pEntry->sizeName] = 0;
		pBuffer += sizeNameAlg;
		// read data size
		read += sizeof(JILLong);
		if( read > length )
			goto error;
		memcpy(&(pEntry->sizeData), pBuffer, sizeof(JILLong));
		pBuffer += sizeof(JILLong);
		sizeDataAlg = (pEntry->sizeData + 3) & 0x7FFFFFFC;
		// read data
		read += sizeDataAlg;
		if( read > length )
			goto error;
		pEntry->pData = malloc(pEntry->sizeData);
		memcpy(pEntry->pData, pBuffer, pEntry->sizeData);
		pBuffer += sizeDataAlg;
		// add to link list
		if( !pState->vmpSymTabFirst )
		{
			pState->vmpSymTabFirst = pState->vmpSymTabLast = pEntry;
			pEntry->pPrev = pEntry->pNext = NULL;
		}
		else
		{
			pState->vmpSymTabLast->pNext = pEntry;
			pEntry->pPrev = pState->vmpSymTabLast;
			pState->vmpSymTabLast = pEntry;
			pEntry->pNext = NULL;
		}
	}
	return result;

error:
	free( pEntry );
	return JIL_ERR_Load_Chunk_Failed;
}

//------------------------------------------------------------------------------
// JILRemoveSymbolTable
//------------------------------------------------------------------------------
// Removes the symbol table.

JILError JILRemoveSymbolTable(JILState* pState)
{
	JILError result = JIL_No_Exception;

	JILSymTabEntry* pEntry;
	JILSymTabEntry* pNext;
	for( pEntry = pState->vmpSymTabFirst; pEntry != NULL; pEntry = pNext )
	{
		pNext = pEntry->pNext;
		free( pEntry->pName );
		free( pEntry->pData );
		free( pEntry );
	}
	pState->vmpSymTabFirst = NULL;
	pState->vmpSymTabLast = NULL;
	return result;
}

//------------------------------------------------------------------------------
// JILTruncateSymbolTable
//------------------------------------------------------------------------------
// Discards all entries that exceed the specified number of entries to keep.
// If "itemsToKeep" is for example 5, and there are 7 entries in the symbol
// table, the last two entries would be discarded.
// However, if there would be only 5 entries in the symbol table, then no
// entries would be discarded.

JILError JILTruncateSymbolTable(JILState* pState, JILLong itemsToKeep)
{
	JILError result = JIL_No_Exception;
	JILLong i = JILGetNumSymbolTableEntries(pState);
	JILSymTabEntry* pEntry;
	JILSymTabEntry* pPrev;
	for( pEntry = pState->vmpSymTabLast; pEntry != NULL; pEntry = pPrev )
	{
		if( i <= itemsToKeep )
			break;
		pPrev = pEntry->pPrev;
		if( pPrev )
		{
			pPrev->pNext = NULL;
			pState->vmpSymTabLast = pPrev;
		}
		else
		{
			pState->vmpSymTabLast = pState->vmpSymTabFirst = NULL;
		}
		free( pEntry->pName );
		free( pEntry->pData );
		free( pEntry );
		--i;
	}
	return result;
}

//------------------------------------------------------------------------------
// JILMatchStrings
//------------------------------------------------------------------------------
// Compares two strings allowing wildcards '?' and '*'. Returns 1 if "string"
// matches "expr", otherwise 0. Only "expr" can contain the wildcards.

static int JILMatchStrings(const char* expr, const char* string)
{
	unsigned char a;
	unsigned char b;
	for(;;)
	{
		a = (unsigned char) *expr++;
		b = (unsigned char) *string++;
		if( a == '?' )				// ? stands for 1 character of any kind
		{
			if( !b )
				return 0;	// if string at end, return !=
			// if expr is '?' skip this char
			continue;
		}
		else if( a == '*' )			// * stands for 0 or many characters of any kind
		{
			// get next character
			a = (unsigned char) *expr++;
			if( !a )
				return 1;	// if end of string, return ==
			// find character in string
			string = strchr(string - 1, a);
			if( !string )
				return 0;	// if character was not found, return !=
			// skip the char
			string++;
		}
		else if( a != b )
		{
			// if both not equal, return !=
			return 0;
		}
		else if( !a )
		{
			// if both are zero, return ==
			return 1;
		}
	}
	// we will never reach this line...
	return 0;
}
