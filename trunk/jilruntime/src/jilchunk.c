//------------------------------------------------------------------------------
// File: JILChunk.c                                         (c) 2003 jewe.org
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
//	Serialization functions for the JIL runtime. Provides functions to load
//	a JIL program from a binary chunk and save it to a binary chunk.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

//------------------------------------------------------------------------------
// JIL includes
//------------------------------------------------------------------------------

#include "jilchunk.h"
#include "jilcstrsegment.h"
#include "jilsymboltable.h"
#include "jiltypeinfo.h"
#include "jilmachine.h"
#include "jiltools.h"

//------------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------------

static const JILChar* kChunkID = "JILVM_";	// chunk ID is 'JILVM_' + revision number of VM

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILEXTERN JILError JILInitializeRuntime(JILState*, const JILChar*, JILBool);
JILEXTERN JILError JILTerminateRuntime(JILState*);

//------------------------------------------------------------------------------
// CreateChunkID
//------------------------------------------------------------------------------

static const JILChar* CreateChunkID(JILChar* buffer, JILLong bufferSize)
{
	JILStrncpy(buffer, bufferSize, kChunkID, strlen(kChunkID));
	JILStrcat(buffer, bufferSize, JIL_MACHINE_VERSION);
	return buffer;
}

//------------------------------------------------------------------------------
// JILLoadBinary
//------------------------------------------------------------------------------
// Loads bytecode from a binary chunk.

JILError JILLoadBinary(JILState* pState, const JILUnknown* pData, JILLong size)
{
	JILError err = JIL_ERR_Load_Chunk_Failed;
	JILChunkHeader* pChunk = (JILChunkHeader*) pData;
	JILChar buffer[20];

	if( (size >= sizeof(JILChunkHeader)) 
	&&	(strcmp(pChunk->cnkMagic, CreateChunkID(buffer, sizeof(buffer))) == 0))
	{
		JILLong codeSize = pChunk->cnkCodeSegSize;
		JILLong funcSize = pChunk->cnkFuncSegSize;
		JILLong cstrSize = pChunk->cnkCStrSegSize;
		JILLong typeSize = pChunk->cnkTypeSegSize;
		JILLong dataSize = pChunk->cnkDataSegSize;
		JILLong symTSize = pChunk->cnkSymTabSize;
		JILLong checkSize = (sizeof(JILChunkHeader)
			+ (codeSize * sizeof(JILLong))
			+ (funcSize * sizeof(JILFuncInfo))
			+ (typeSize * sizeof(JILTypeInfo))
			+ (dataSize * sizeof(JILDataHandle))
			+ cstrSize + symTSize);
		if( (checkSize == size) && (checkSize == pChunk->cnkSize) )
		{
			JILChar* ptr = (JILChar*) pData;
			ptr += sizeof(JILChunkHeader);
			// terminate runtime
			err = JILTerminateRuntime(pState);
			if( err )
				goto exit;
			// initialize runtime without segments
			err = JILInitializeRuntime(pState, NULL, JILFalse);
			if( err )
				goto exit;
			// initialize all segments
			err = InitSegment_JILLong(pState->vmpCodeSegment, codeSize);
			if( err )
				goto exit;
			err = InitSegment_JILFuncInfo(pState->vmpFuncSegment, funcSize);
			if( err )
				goto exit;
			err = JILInitCStrSegment(pState, cstrSize);
			if( err )
				goto exit;
			err = JILInitTypeInfoSegment(pState, typeSize);
			if( err )
				goto exit;
			err = InitSegment_JILDataHandle(pState->vmpDataSegment, dataSize);
			if( err )
				goto exit;
			// copy code segment
			memcpy( pState->vmpCodeSegment->pData, ptr, codeSize * sizeof(JILLong) );
			ptr += codeSize * sizeof(JILLong);
			// copy function segment
			memcpy( pState->vmpFuncSegment->pData, ptr, funcSize * sizeof(JILFuncInfo) );
			ptr += funcSize * sizeof(JILFuncInfo);
			// copy TypeInfo segment
			memcpy( pState->vmpTypeInfoSegment, ptr, typeSize * sizeof(JILTypeInfo) );
			ptr += typeSize * sizeof(JILTypeInfo);
			// copy data segment
			memcpy( pState->vmpDataSegment->pData, ptr, dataSize * sizeof(JILDataHandle) );
			ptr += dataSize * sizeof(JILDataHandle);
			// copy cstr segment
			memcpy( pState->vmpCStrSegment, ptr, cstrSize );
			ptr += cstrSize;
			// read symbol table if available
			if( symTSize )
			{
				err = JILReadSymbolTableFromChunk(pState, ptr, symTSize);
				if( err )
					return err;
				ptr += symTSize;
			}
			// done.
			pState->vmpCodeSegment->usedSize = codeSize;
			pState->vmpFuncSegment->usedSize = funcSize;
			pState->vmpDataSegment->usedSize = dataSize;
			pState->vmUsedCStrSegSize = cstrSize;
			pState->vmUsedTypeInfoSegSize = typeSize;
			pState->vmInitialized = JILFalse;
			pState->errException = JIL_No_Exception;
			err = JIL_No_Exception;
		}
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// JILSaveBinary
//------------------------------------------------------------------------------
// Save bytecode to a binary chunk.

JILError JILSaveBinary(JILState* pState, JILUnknown** ppData, JILLong* pDataSize)
{
	JILError err = JIL_ERR_Save_Chunk_Failed;
	JILChunkHeader header;

	// clear given variables
	*ppData = NULL;
	*pDataSize = 0;

	// prepare chunk header
	memset( &header, 0, sizeof(JILChunkHeader) );
	CreateChunkID(header.cnkMagic, sizeof(header.cnkMagic));
	header.cnkCodeSegSize = pState->vmpCodeSegment->usedSize;
	header.cnkFuncSegSize = pState->vmpFuncSegment->usedSize;
	header.cnkDataSegSize = pState->vmpDataSegment->usedSize;
	header.cnkCStrSegSize = pState->vmUsedCStrSegSize;
	header.cnkTypeSegSize = pState->vmUsedTypeInfoSegSize;
	header.cnkSymTabSize =  JILGetSymbolTableChunkSize(pState);
	header.cnkSize =
		sizeof(JILChunkHeader)
		+ (pState->vmpCodeSegment->usedSize * sizeof(JILLong))
		+ (pState->vmpFuncSegment->usedSize * sizeof(JILFuncInfo))
		+ (pState->vmUsedTypeInfoSegSize * sizeof(JILTypeInfo))
		+ (pState->vmpDataSegment->usedSize * sizeof(JILDataHandle))
		+ pState->vmUsedCStrSegSize
		+ header.cnkSymTabSize;

	// release old chunk buffer
	if( pState->vmpChunkBuffer )
	{
		free( pState->vmpChunkBuffer );
		pState->vmpChunkBuffer = NULL;
	}
	// allocate new chunk buffer
	pState->vmpChunkBuffer = malloc( header.cnkSize );
	if( pState->vmpChunkBuffer )
	{
		JILChar* ptr = (JILChar*) pState->vmpChunkBuffer;
		// copy header
		memcpy( ptr, &header, sizeof(JILChunkHeader) );
		ptr += sizeof(JILChunkHeader);
		// copy code segment
		memcpy( ptr, pState->vmpCodeSegment->pData, pState->vmpCodeSegment->usedSize * sizeof(JILLong) );
		ptr += pState->vmpCodeSegment->usedSize * sizeof(JILLong);
		// copy function segment
		memcpy( ptr, pState->vmpFuncSegment->pData, pState->vmpFuncSegment->usedSize * sizeof(JILFuncInfo) );
		ptr += pState->vmpFuncSegment->usedSize * sizeof(JILFuncInfo);
		// copy TypeInfo segment
		memcpy( ptr, pState->vmpTypeInfoSegment, pState->vmUsedTypeInfoSegSize * sizeof(JILTypeInfo) );
		ptr += pState->vmUsedTypeInfoSegSize * sizeof(JILTypeInfo);
		// copy data segment
		memcpy( ptr, pState->vmpDataSegment->pData, pState->vmpDataSegment->usedSize * sizeof(JILDataHandle) );
		ptr += pState->vmpDataSegment->usedSize * sizeof(JILDataHandle);
		// copy CStr segment
		memcpy( ptr, pState->vmpCStrSegment, pState->vmUsedCStrSegSize );
		ptr += pState->vmUsedCStrSegSize;
		// optionally save symbol table
		if( header.cnkSymTabSize )
		{
			err = JILWriteSymbolTableToChunk(pState, ptr, header.cnkSymTabSize);
			ptr += header.cnkSymTabSize;
		}
		// done.
		*ppData = pState->vmpChunkBuffer;
		*pDataSize = header.cnkSize;
		err = JIL_No_Exception;
	}
	return err;
}
