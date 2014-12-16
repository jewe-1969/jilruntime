//------------------------------------------------------------------------------
// File: JILFixMem.c                                           (c) 2005 jewe.org
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
//	A memory manager class for allocating and freeing fixed-size blocks of
//  memory.
//------------------------------------------------------------------------------

#include "jilstdinc.h"
#include "jilfixmem.h"
#include "jildebug.h"

//------------------------------------------------------------------------------
// private structs
//------------------------------------------------------------------------------

typedef struct JILFixMemBucket	JILFixMemBucket;

typedef struct JILFixMemBlock	JILFixMemBlock;
struct JILFixMemBlock
{
	JILFixMemBucket*	bucket;			// ptr back to owner bucket
	JILLong				size;			// must keep track of size (large block support)
};

struct JILFixMemBucket
{
	JILChar*			pMemory;
	JILFixMemBucket*	pPrev;
	JILFixMemBucket*	pNext;
	JILFixMemBlock**	ppFreeBlocks;
	JILLong				numFreeBlocks;
};

struct JILFixMem
{
	JILFixMemBucket*	pFirst;
	JILMemStats*		pStats;
	JILLong				blockSize;
	JILLong				bucketSize;
	JILLong				currentBucket;
	JILLong				maxBuckets;
	JILLong				realBlockSize;
	JILLong				dynamicGrowth;
};

//------------------------------------------------------------------------------
// static constants
//------------------------------------------------------------------------------

const long kDynamicGrowthSize = 32;
const long kDefaultBucketSize = 32;

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static JILFixMemBucket* JILNewBucket(JILFixMem* _this);
static void JILLinkBucket(JILFixMem* _this, JILFixMemBucket* pBucket);
static void JILUnlinkBucket(JILFixMem* _this, JILFixMemBucket* pBucket);

//------------------------------------------------------------------------------
// New_FixMem
//------------------------------------------------------------------------------
// Allocates and initializes the fixed memory manager
//
// blockSize:  The size (bytes) of the memory blocks returned by FixMem_Alloc()
// maxBlocks:  The total number of memory blocks the manager should handle
//             Set this to 0 to use dynamic growth
// bucketSize: How many blocks are pre-allocated at once
//             Set this to 0 to use default (32)

JILFixMem* New_FixMem(JILLong blockSize, JILLong maxBlocks, JILLong bucketSize, JILMemStats* pStats)
{
	JILLong freeBuckets;

	JILFixMem* _this = malloc(sizeof(JILFixMem));
	memset(_this, 0, sizeof(JILFixMem));

	if( bucketSize <= 0 )
		bucketSize = kDefaultBucketSize;
	if( maxBlocks <= 0 )
	{
		maxBlocks = kDynamicGrowthSize * bucketSize;
		_this->dynamicGrowth = JILTrue;
	}
	if( maxBlocks < bucketSize )
		maxBlocks = bucketSize;

	freeBuckets = maxBlocks / bucketSize;

	_this->blockSize = blockSize;
	_this->bucketSize = bucketSize;
	_this->maxBuckets = freeBuckets;
	_this->realBlockSize = blockSize + sizeof(JILFixMemBlock);
	_this->pStats = pStats;

	return _this;
}

//------------------------------------------------------------------------------
// Delete_FixMem
//------------------------------------------------------------------------------
// Frees up all memory allocated by this memory manager.

void Delete_FixMem(JILFixMem* _this)
{
	if( _this )
	{
		JILLong missingBlocks = 0;
		JILFixMemBucket* bucket;
		JILFixMemBucket* next;
		for( bucket = _this->pFirst; bucket != NULL; bucket = next )
		{
			missingBlocks += (_this->bucketSize - bucket->numFreeBlocks);
			next = bucket->pNext;
			free( bucket );
			_this->currentBucket--;
		}
		#if !JIL_NO_FPRINTF
		JIL_INSERT_DEBUG_CODE(
			if( _this->currentBucket || missingBlocks )
			{
				JILLong bytesLeaked = (_this->currentBucket * _this->bucketSize + missingBlocks) * _this->realBlockSize;
				fprintf(stderr,
					"MEMORY LEAK DETECTED IN BLOCK MANAGER:\n"
					"Block size:          %d\n"
					"Bucket size:         %d\n"
					"Max buckets:         %d\n"
					"Dynamic growth:      %s\n"
					"Buckets leaked:      %d\n"
					"Blocks leaked:       %d\n"
					"Bytes leaked:        %d\n"
					"---------------------\n",
					_this->blockSize, _this->bucketSize, _this->maxBuckets, _this->dynamicGrowth ? "YES" : "NO",
					_this->currentBucket, missingBlocks, bytesLeaked
					);
			}
		);
		#endif
		free( _this );
	}
}

//------------------------------------------------------------------------------
// FixMem_Alloc
//------------------------------------------------------------------------------
// Allocate a memory buffer of the given JILFixMem memory manager's size. Make
// sure you never step over the buffer's size when writing into it, this will
// severely mess up the whole memory manager!

JILChar* FixMem_Alloc(JILFixMem* _this)
{
	JILChar* pBlock;
	JILFixMemBucket* pCurrentBucket;

	// get current bucket
	pCurrentBucket = _this->pFirst;

	// no more free buckets?
	if( pCurrentBucket == NULL )
	{
		// bucket limit reached?
		if( _this->currentBucket == (_this->maxBuckets - 1) )
		{
			if( !_this->dynamicGrowth )
				return NULL;
			_this->maxBuckets++;
		}
		// allocate a new bucket
		pCurrentBucket = JILNewBucket(_this);
		JILLinkBucket(_this, pCurrentBucket);
		_this->currentBucket++;
	}

	// pop a block from the free block stack
	pBlock = (JILChar*) pCurrentBucket->ppFreeBlocks[--pCurrentBucket->numFreeBlocks];
	pBlock += sizeof(JILFixMemBlock);

	// no more blocks in this bucket?
	if( pCurrentBucket->numFreeBlocks == 0 )
	{
		// unlink the bucket
		JILUnlinkBucket(_this, pCurrentBucket);
	}

	// do the statistics
	JIL_INSERT_DEBUG_CODE(
		_this->pStats->numAlloc++;
		_this->pStats->bytesUsed += _this->blockSize;
		if( _this->pStats->bytesUsed > _this->pStats->maxBytesUsed )
			_this->pStats->maxBytesUsed = _this->pStats->bytesUsed;
	)

	return pBlock;
}

//------------------------------------------------------------------------------
// FixMem_Free
//------------------------------------------------------------------------------
// Free a memory buffer allocated by FixMem_Alloc(). NEVER try to free a block
// with this function that has not been allocted by FixMem_Alloc(), but by any
// other method (f.ex. malloc). The function ASSUMES the given buffer is
// prepended by a valid JILFixMemBlock descriptor. If it isn't, an application
// crash is guaranteed!

void FixMem_Free(JILFixMem* _this, JILChar* pBuffer)
{
	JILFixMemBucket* pBucket;
	JILFixMemBlock* pBlock;
	
	// convert buffer pointer to a JILFixMemBlock pointer
	pBlock = (JILFixMemBlock*) (pBuffer - sizeof(JILFixMemBlock));

	// verify block size in block header
	if( pBlock->size == _this->blockSize )
	{
		// store block in free blocks list
		pBucket = pBlock->bucket;
		pBucket->ppFreeBlocks[pBucket->numFreeBlocks++] = pBlock;
		// relink the bucket
		if( pBucket->pNext == NULL &&
			pBucket->pPrev == NULL &&
			_this->pFirst != pBucket )
		{
			JILLinkBucket(_this, pBucket);
		}

		JIL_INSERT_DEBUG_CODE(
			/* pad memory with 0xDD to show it's deleted */
			memset(pBuffer, 0xDD, pBlock->size);
			/* do the statistics */
			_this->pStats->numFree++;
			_this->pStats->bytesUsed -= _this->blockSize;
		)
	}
	else
	{
		// this doesn't seem to be a valid buffer pointer
		#if !JIL_NO_FPRINTF
		fprintf(stderr, "ERROR: FixMem_Free() called with invalid buffer pointer\n");
		#endif
	}
}

//------------------------------------------------------------------------------
// FixMem_GetBlockLength
//------------------------------------------------------------------------------
// Gets the size of the given memory block. The function ASSUMES that the given
// buffer was allocated by the fixed memory manager. Calling this function with
// a pointer to a block that has NOT been allocated by a JILFixMem memory
// manager will return invalid results or might even crash!

JILLong FixMem_GetBlockLength(JILChar* pBuffer)
{
	JILFixMemBlock* pBlock = (JILFixMemBlock*) (pBuffer - sizeof(JILFixMemBlock));
	return pBlock->size;
}

//------------------------------------------------------------------------------
// FixMem_AllocLargeBlock
//------------------------------------------------------------------------------
// This will redirect the allocation to the C-Runtime 'malloc' function, but
// prepends the block with a valid JILFixMemBlock descriptor, so the block can
// be properly detected as a large block when trying to free it.

JILChar* FixMem_AllocLargeBlock(JILLong size, JILMemStats* pStats)
{
	JILChar* pBuffer = (JILChar*) malloc(size + sizeof(JILFixMemBlock));
	JILFixMemBlock* pBlock = (JILFixMemBlock*) pBuffer;
	pBuffer += sizeof(JILFixMemBlock);
	pBlock->bucket = NULL;
	pBlock->size = size;

	// do the statistics
	JIL_INSERT_DEBUG_CODE(
		pStats->numAlloc++;
		pStats->bytesUsed += size;
		if( pStats->bytesUsed > pStats->maxBytesUsed )
			pStats->maxBytesUsed = pStats->bytesUsed;
	)
	return pBuffer;
}

//------------------------------------------------------------------------------
// FixMem_FreeLargeBlock
//------------------------------------------------------------------------------
// Free a large memory block by calling the C-Runtime 'free' function. This
// function ASSUMES that the given buffer pointer is prepended by a
// JILFixMemBlock descriptor!

void FixMem_FreeLargeBlock(JILChar* pBuffer, JILMemStats* pStats)
{
	// do the statistics
	JIL_INSERT_DEBUG_CODE(
		pStats->numFree++;
		pStats->bytesUsed -= ((JILFixMemBlock*)(pBuffer - sizeof(JILFixMemBlock)))->size;
	)

	free( pBuffer - sizeof(JILFixMemBlock) );
}

//------------------------------------------------------------------------------
// JILNewBucket
//------------------------------------------------------------------------------
// Allocate a new bucket. A bucket is a fixed-sized set of pre-allocated
// memory blocks.

static JILFixMemBucket* JILNewBucket(JILFixMem* _this)
{
	int i;
	JILLong size;
	JILChar* pBlock;
	JILFixMemBucket* pBucket;
	// allocate memory in one big chunk
	size = sizeof(JILFixMemBucket) + _this->bucketSize * _this->realBlockSize + _this->bucketSize * sizeof(JILFixMemBlock*);
	pBlock = (JILChar*) malloc(size);
	pBucket = (JILFixMemBucket*) pBlock;
	pBucket->numFreeBlocks = _this->bucketSize;
	pBucket->pNext = NULL;
	pBucket->pPrev = NULL;
	pBlock += sizeof(JILFixMemBucket);
	// allocate block memory as one big chunk
	pBucket->pMemory = pBlock;
	pBlock += _this->bucketSize * _this->realBlockSize;
	// allocate stack for pointers to free blocks
	pBucket->ppFreeBlocks = (JILFixMemBlock**) pBlock;
	pBlock = pBucket->pMemory;
	// fill free block stack
	for( i = 0; i < _this->bucketSize; i++ )
	{
		pBucket->ppFreeBlocks[i] = (JILFixMemBlock*) pBlock;
		pBucket->ppFreeBlocks[i]->bucket = pBucket;
		pBucket->ppFreeBlocks[i]->size = _this->blockSize;
		pBlock += _this->realBlockSize;
	}
	// do the statistics
	JIL_INSERT_DEBUG_CODE(
		_this->pStats->bucketBytes += size;
		_this->pStats->numBuckets++;
	)
	return pBucket;
}

//------------------------------------------------------------------------------
// JILLinkBucket
//------------------------------------------------------------------------------
// Add a bucket to the link list

static void JILLinkBucket(JILFixMem* _this, JILFixMemBucket* pBucket)
{
	JILFixMemBucket* oldFirst = _this->pFirst;
	if( oldFirst )
	{
		_this->pFirst = pBucket;
		oldFirst->pPrev = pBucket;
		pBucket->pPrev = NULL;
		pBucket->pNext = oldFirst;
	}
	else
	{
		_this->pFirst = pBucket;
		pBucket->pPrev = NULL;
		pBucket->pNext = NULL;
	}
}

//------------------------------------------------------------------------------
// JILUnlinkBucket
//------------------------------------------------------------------------------
// JCLRemove a bucket from the link list

static void JILUnlinkBucket(JILFixMem* _this, JILFixMemBucket* pBucket)
{
	JILFixMemBucket* pPrev = pBucket->pPrev;
	JILFixMemBucket* pNext = pBucket->pNext;
	if( pPrev )
		pPrev->pNext = pNext;
	if( pNext )
		pNext->pPrev = pPrev;
	if( _this->pFirst == pBucket )
		_this->pFirst = pNext;
	pBucket->pPrev = NULL;
	pBucket->pNext = NULL;
}
