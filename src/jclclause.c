//------------------------------------------------------------------------------
// File: JCLClause.c                                           (c) 2014 jewe.org
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
/// @file jclclause.c
/// Holds data for the clause language construct.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jclclause.h"

//------------------------------------------------------------------------------
// JCLClause
//------------------------------------------------------------------------------
// constructor

void create_JCLClause( JCLClause* _this )
{
	_this->miStackPos = 0;
	_this->miParameter = NULL;
	_this->mipBlocks = NULL;
	_this->mipGotos = NULL;
	_this->mipParent = NULL;
}

//------------------------------------------------------------------------------
// JCLClause
//------------------------------------------------------------------------------
// destructor

void destroy_JCLClause( JCLClause* _this )
{
	JCLClauseBlock* pBlock = _this->mipBlocks;
	JCLClauseGoto* pGoto = _this->mipGotos;
	while (pBlock != NULL)
	{
		JCLClauseBlock* pNext = pBlock->mipNext;
		DELETE(pBlock->mipLabel);
		free(pBlock);
		pBlock = pNext;
	}
	while (pGoto != NULL)
	{
		JCLClauseGoto* pNext = pGoto->mipNext;
		DELETE(pGoto->mipLabel);
		free(pGoto);
		pGoto = pNext;
	}
	_this->mipBlocks = NULL;
	_this->mipGotos = NULL;
	_this->miParameter = NULL;
	_this->mipParent = NULL;
}

//------------------------------------------------------------------------------
// JCLClause::Copy
//------------------------------------------------------------------------------

void copy_JCLClause(JCLClause* _this, const JCLClause* src)
{
	// Copy not supported
}

//------------------------------------------------------------------------------
// GetBlock
//------------------------------------------------------------------------------

JCLClauseBlock* JCLClause_GetBlock(JCLClause* _this, const JCLString* pLabel)
{
	JCLClauseBlock* pBlock;
	for( pBlock = _this->mipBlocks; pBlock != NULL; pBlock = pBlock->mipNext )
	{
		if( JCLCompare(pBlock->mipLabel, pLabel) )
			break;
	}
	return pBlock;
}

//------------------------------------------------------------------------------
// AddBlock
//------------------------------------------------------------------------------

JILBool JCLClause_AddBlock(JCLClause* _this, const JCLString* pLabel)
{
	JILBool bSuccess = JILFalse;
	if( JCLClause_GetBlock(_this, pLabel) == NULL )
	{
		JCLClauseBlock* pBlock = (JCLClauseBlock*)malloc(sizeof(JCLClauseBlock));
		pBlock->mipNext = _this->mipBlocks;
		_this->mipBlocks = pBlock;

		pBlock->miCodePos = 0;
		pBlock->mipLabel = NEW(JCLString);
		pBlock->mipLabel->Copy(pBlock->mipLabel, pLabel);
		bSuccess = JILTrue;
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
// SetBlock
//------------------------------------------------------------------------------

JILBool JCLClause_SetBlock(JCLClause* _this, const JCLString* pLabel, JILLong codePos)
{
	JILBool bSuccess = JILFalse;
	JCLClauseBlock* pBlock = JCLClause_GetBlock(_this, pLabel);
	if( pBlock != NULL )
	{
		pBlock->miCodePos = codePos;
		bSuccess = JILTrue;
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
// AddGoto
//------------------------------------------------------------------------------

JILBool JCLClause_AddGoto(JCLClause* _this, const JCLString* pLabel, JILLong popPos, JILLong branchPos, JILLong stackPos, JILLong filePos)
{
	JILBool bSuccess = JILTrue;
	JCLClauseGoto* pGoto = (JCLClauseGoto*)malloc(sizeof(JCLClauseGoto));
	pGoto->mipNext = _this->mipGotos;
	_this->mipGotos = pGoto;

	pGoto->miPopPos = popPos;
	pGoto->miBranchPos = branchPos;
	pGoto->miStackPos = stackPos;
	pGoto->miFilePos = filePos;
	pGoto->mipLabel = NEW(JCLString);
	pGoto->mipLabel->Copy(pGoto->mipLabel, pLabel);
	return bSuccess;
}

//------------------------------------------------------------------------------
// FixBranches
//------------------------------------------------------------------------------

JILBool JCLClause_FixBranches(JCLClause* _this, Array_JILLong* pCode, JCLClauseGoto** ppOutFail)
{
	JILBool bSuccess = JILTrue;
	JCLClauseGoto* pGoto;
	JCLClauseBlock* pBlock;
	JILLong branchTarget;
	JILLong branchPos;
	JILLong popPos;
	JILLong numToPop;

	// fix all goto branch addresses
	*ppOutFail = NULL;
	for (pGoto = _this->mipGotos; pGoto != NULL; pGoto = pGoto->mipNext)
	{
		// get the block for this label
		pBlock = JCLClause_GetBlock(_this, pGoto->mipLabel);
		if( pBlock == NULL )
		{
			bSuccess = JILFalse;
			*ppOutFail = pGoto;
			break;
		}
		// patch the code
		branchTarget = pBlock->miCodePos;
		branchPos = pGoto->miBranchPos;
		popPos = pGoto->miPopPos;
		numToPop = _this->miStackPos - pGoto->miStackPos;
		pCode->Set(pCode, popPos + 1, numToPop);
		pCode->Set(pCode, branchPos + 1, branchTarget - branchPos);
	}
	return bSuccess;
}
