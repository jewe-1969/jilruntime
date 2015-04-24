//------------------------------------------------------------------------------
// File: JCLClause.h										   (c) 2014 jewe.org
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
/// @file jclclause.h
/// Holds data for the clause language construct.
//------------------------------------------------------------------------------

#ifndef JCLCLAUSE_H
#define JCLCLAUSE_H

#include "jcltools.h"

typedef struct JCLClauseBlock JCLClauseBlock;
struct JCLClauseBlock
{
	JCLClauseBlock*		mipNext;		///< next block
	JILLong				miCodePos;		///< code location of the block
	JCLString*			mipLabel;		///< branch label of the block
};

typedef struct JCLClauseGoto JCLClauseGoto;
struct JCLClauseGoto
{
	JCLClauseGoto*		mipNext;		///< next goto
	JILLong				miPopPos;		///< code location of the popm instruction to patch
	JILLong				miBranchPos;	///< code location of the branch instruction to patch
	JILLong				miStackPos;		///< stack counter when goto was encountered
	JILLong				miFilePos;		///< character position in file of the goto statement
	JCLString*			mipLabel;		///< branch label of the goto statement
};

FORWARD_CLASS( JCLClause )

//------------------------------------------------------------------------------
// class JCLClause
//------------------------------------------------------------------------------
/// Holds data for the clause language construct.

DECL_CLASS( JCLClause )

	JILLong				miStackPos;		///< stack position to unroll to in case of a goto
	JCLVar*				miParameter;	///< clause parameter variable
	JCLClauseBlock*		mipBlocks;		///< link list of all blocks
	JCLClauseGoto*		mipGotos;		///< link list of all gotos
	JCLClause*			mipParent;		///< pointer to parent clause statement

END_CLASS( JCLClause )

//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------

JCLClauseBlock*	JCLClause_GetBlock		(JCLClause* _this, const JCLString* pLabel);
JILBool			JCLClause_AddBlock		(JCLClause* _this, const JCLString* pLabel);
JILBool			JCLClause_SetBlock		(JCLClause* _this, const JCLString* pLabel, JILLong codePos);
JILBool			JCLClause_AddGoto		(JCLClause* _this, const JCLString* pLabel, JILLong popPos, JILLong branchPos, JILLong stackPos, JILLong filePos);
JILBool			JCLClause_FixBranches	(JCLClause* _this, Array_JILLong* pCode, JCLClauseGoto** ppOutFail);

#endif // #ifndef JCLCLAUSE_H
