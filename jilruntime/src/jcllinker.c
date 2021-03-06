//------------------------------------------------------------------------------
// File: jcllinker.c                                           (c) 2015 jewe.org
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
//	Implements the linker and byte-code optimizer of the JIL runtime library.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclstate.h"

#include "jilcodelist.h"
#include "jilopcodes.h"
#include "jilprogramming.h"
#include "jiltools.h"

//------------------------------------------------------------------------------
// internal types
//------------------------------------------------------------------------------

typedef Array_JILLong	CodeBlock;

typedef struct
{
	JILLong		type;		// operand type, see enum in jiltypes.h
	JILLong		data[2];	// actual operand data (eg. register #, offset, etc)
} OperandInfo;

typedef struct
{
	JILLong		base_opcode;	// see GetBaseFromOpcode()
	OperandInfo	operand[4];		// up to four operands
} OpcodeInfo;

enum { src = 0, dst = 1 };

typedef struct
{
	JILLong		instr_removed;
	JILLong		instr_added;
	JILLong		count_before;
	JILLong		count_after;
	JILLong		numPasses;
	JILLong		totalPasses;
} OptimizeReport;

//------------------------------------------------------------------------------
// internal functions
//------------------------------------------------------------------------------

static JILError createLiterals_JCLFunc	(JCLFunc*, JCLState*);
static JILError optimizeCode_JCLFunc	(JCLFunc*, JCLState*);
static JILError DebugListFunction		(JCLFunc*, JCLState*);
static JILError InsertRegisterSaving	(JCLFunc*, JCLState*);
static JILError RelocateFunction		(JCLFunc*, JCLFunc*, JCLState*);

//------------------------------------------------------------------------------
// JCLLinkerMain
//------------------------------------------------------------------------------
// Main entry point for the linker and byte-code optimizer.

JILError JCLLinkerMain(JCLState* _this)
{
	JILError err = JIL_No_Exception;
	JILLong clas;
	JILLong fn;
	JILLong address = 0;
	JCLClass* pClass;
	JCLFunc* pFunc;
	Array_JILLong* pCode;
	JCLString* declString = NEW(JCLString);
	JILLong i;
	JILLong vtabSize;
	JILLong* pVtable;
	JILState* pVM = _this->mipMachine;

	// finish intro code
	err = cg_finish_intro(_this);
	if( err )
		goto exit;

	// reset statistics
	_this->miOptSavedInstr = 0;
	_this->miOptSizeBefore = 0;
	_this->miOptSizeAfter = 0;

	// iterate over all classes
	for( clas = 0; clas < NumClasses(_this); clas++ )
	{
		pClass = GetClass(_this, clas);
		if( (pClass->miFamily == tf_class || pClass->miFamily == tf_thread) && !(pClass->miModifier & kModeNativeBinding) )
		{
			// set class instance size and v-table
			if( !pClass->miHasVTable)
			{
				pClass->miHasVTable = JILTrue;
				if( pClass->miNative )
				{
					err = JILSetClassVTable(pVM, pClass->miType, NumFuncs(_this, clas), NULL);
					if( err )
						goto exit;
				}
				else if( pClass->miFamily == tf_class )
				{
					// Set class instance size
					err = JILSetClassInstanceSize(pVM, pClass->miType, pClass->mipVars->Count(pClass->mipVars));
					if( err )
						goto exit;
					// generate v-table
					vtabSize = NumFuncs(_this, clas);
					if( vtabSize )
					{
						pVtable = (JILLong*) malloc( vtabSize * sizeof(JILLong) );
						for( i = 0; i < vtabSize; i++ )
						{
							JCLFunc* pFunc = GetFunc(_this, pClass->miType, i);
							pVtable[i] = pFunc->miHandle;
						}
						err = JILSetClassVTable(pVM, pClass->miType, vtabSize, pVtable);
						free( pVtable );
						if( err )
							goto exit;
					}
				}
			}
			// iterate over all functions and link them
			for( fn = 0; fn < NumFuncs(_this, clas); fn++ )
			{
				pFunc = GetFunc(_this, clas, fn);
				pCode = pFunc->mipCode;
				// udpate function address
				if( !pClass->miNative )
				{
					err = pFunc->LinkCode(pFunc, _this);
					if( err )
						goto exit;
					// ensure that the function has a body
					pCode = pFunc->mipCode;
					if( !pCode->count )
					{
						// get function declaration
						pFunc->ToString(pFunc, _this, declString, kCompact);
						if( IsMethodInherited(_this, clas, fn) )
							err = EmitError(_this, declString, JCL_ERR_Interface_Not_Complete);
						else
							err = EmitError(_this, declString, JCL_ERR_No_Function_Body);
						goto exit;
					}
					// copy function code into JIL machine
					err = JILSetMemory(pVM, address, pCode->array, pCode->count);
					if( err )
						goto exit;
					err = JILSetFunctionAddress(pVM, pFunc->miHandle, address, pCode->count, pFunc->mipArgs->Count(pFunc->mipArgs));
					if( err )
						goto exit;
				}
				pFunc->miLnkAddr = address;
				address += pCode->count;
			}
			if( pClass->miFamily == tf_class )
			{
				err = JILSetClassMethodInfo(pVM, pClass->miType, &(pClass->miMethodInfo));
				if( err )
					goto exit;
			}
		}
	}

exit:
	DELETE( declString );
	return err;
}

//------------------------------------------------------------------------------
// JCLPostLink
//------------------------------------------------------------------------------
// Substitute all 'calls' instructions by cheaper 'jsr' instructions.
// Also substitutes function index by code address for 'jmp' instruction.

JILError JCLPostLink(JCLState* _this)
{
	JILLong c;
	JILLong f;
	JILLong i;
	JILLong l;
	JILLong o;
	JILLong addr;
	JCLFunc* pFunc;
	Array_JILLong* pCode;
	JILFuncInfo* pFuncInfo;
	JILState* ps = _this->mipMachine;
	JILLong data[2];

	addr = 0;
	for (c = 0; c < NumClasses(_this); c++)
	{
		for (f = 0; f < NumFuncs(_this, c); f++)
		{
			pFunc = GetFunc(_this, c, f);
			pCode = pFunc->mipCode;
			for (i = 0; i < pCode->count; i += l)
			{
				o = pCode->Get(pCode, i);
				l = JILGetInstructionSize(o);
				if (l == 0)
					break;
				if (o == op_calls)
				{
					pFuncInfo = JILGetFunctionInfo(ps, pCode->Get(pCode, i + 1));
					data[0] = op_jsr;
					data[1] = pFuncInfo->codeAddr;
					JILSetMemory(ps, addr + i, data, 2);
				}
				else if (o == op_jmp)
				{
					pFuncInfo = JILGetFunctionInfo(ps, pCode->Get(pCode, i + 1));
					data[0] = op_jmp;
					data[1] = pFuncInfo->codeAddr;
					JILSetMemory(ps, addr + i, data, 2);
				}
			}
			addr += pCode->count;
		}
	}
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// JCLLinkFunction
//------------------------------------------------------------------------------

JILError JCLLinkFunction(JCLFunc* _this, JCLState* pCompiler)
{
	JILError err = JCL_No_Error;
	// not already linked?
	if (!_this->miLinked)
	{
		// generate "stub" if function has no body
		Array_JILLong* pCode = _this->mipCode;
		if (!pCode->count && (!_this->miStrict || _this->miLnkDelegate >= 0 || _this->miLnkMethod >= 0 || _this->miLnkRelIdx >= 0))
		{
			if (_this->miLnkDelegate < 0 && _this->miLnkMethod < 0 && _this->miLnkRelIdx < 0)
			{
				JCLString* declString = NEW(JCLString);
				_this->ToString(_this, pCompiler, declString, kCompact);
				EmitWarning(pCompiler, JCL_WARN_Function_Auto_Complete, 1, declString);
				DELETE(declString);
			}
			if (_this->miCofunc)
			{
				pCode->Set(pCode, 0, op_moveh_r);
				pCode->Set(pCode, 1, 0);
				pCode->Set(pCode, 2, kReturnRegister);
				pCode->Set(pCode, 3, op_yield);
				pCode->Set(pCode, 4, op_bra);
				pCode->Set(pCode, 5, -1);
			}
			else if (_this->miLnkRelIdx >= 0)
			{
				return RelocateFunction(_this, GetFunc(pCompiler, _this->miLnkClass, _this->miLnkRelIdx), pCompiler);
			}
			else if (_this->miLnkMethod >= 0)
			{
				// directly jump into base class method
				JCLFunc* baseFunc = GetFunc(pCompiler, _this->miLnkClass, _this->miLnkMethod);
				pCode->Set(pCode, 0, op_jmp);
				pCode->Set(pCode, 1, baseFunc->miHandle);
			}
			else if (_this->miLnkDelegate >= 0)
			{
				// call base class delegate
				int i;
				int n = 0;
				int j = 0;
				if (_this->miMethod)
				{
					pCode->Set(pCode, n++, op_push_r);
					pCode->Set(pCode, n++, 0);
					j++;
					if (_this->mipArgs->Count(_this->mipArgs))
					{
						if (_this->mipArgs->Count(_this->mipArgs) > 1)
						{
							pCode->Set(pCode, n++, op_pushm);
							pCode->Set(pCode, n++, _this->mipArgs->Count(_this->mipArgs));
						}
						else
						{
							pCode->Set(pCode, n++, op_push);
						}
						for (i = 0; i < _this->mipArgs->Count(_this->mipArgs); i++)
						{
							pCode->Set(pCode, n++, op_move_ss);
							pCode->Set(pCode, n++, _this->mipArgs->Count(_this->mipArgs) + j + i);
							pCode->Set(pCode, n++, i);
						}
					}
				}
				pCode->Set(pCode, n++, op_calldg_d);
				pCode->Set(pCode, n++, 0);
				pCode->Set(pCode, n++, _this->miLnkDelegate);
				if (_this->miMethod)
				{
					if (_this->mipArgs->Count(_this->mipArgs))
					{
						if (_this->mipArgs->Count(_this->mipArgs) > 1)
						{
							pCode->Set(pCode, n++, op_popm);
							pCode->Set(pCode, n++, _this->mipArgs->Count(_this->mipArgs));
						}
						else
						{
							pCode->Set(pCode, n++, op_pop);
						}
					}
					pCode->Set(pCode, n++, op_pop_r);
					pCode->Set(pCode, n++, 0);
				}
				pCode->Set(pCode, n++, op_ret);
			}
			else
			{
				pCode->Set(pCode, 0, op_moveh_r);
				pCode->Set(pCode, 1, 0);
				pCode->Set(pCode, 2, kReturnRegister);
				pCode->Set(pCode, 3, op_ret);
			}
		}
		// generate data handles for literals and patch code
		err = createLiterals_JCLFunc(_this, pCompiler);
		if (err)
			goto exit;
		// insert register saving code
		err = InsertRegisterSaving(_this, pCompiler);
		if (err)
			goto exit;
		// do optimization
		err = optimizeCode_JCLFunc(_this, pCompiler);
		if (err)
			goto exit;
		// make sure we are not linked again
		_this->miLinked = JILTrue;
	}
exit:
	return err;
}

/******************************************************************************/
/*********************** Code optimization functions **************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// GetNumRegsToSave
//------------------------------------------------------------------------------
// Returns the number of registers that need to be saved to the stack.

JILEXTERN JILLong GetNumRegsToSave(JCLFunc* pFunc)
{
	JILLong j;
	JILLong numRegs = 0;
	if( !pFunc->miCofunc && !pFunc->miNaked ) // exclude co-functions and __init() function
	{
		for( j = 3; j < kNumRegisters; j++ )
		{
			if( pFunc->miRegUsage[j] )
				numRegs++;
		}
	}
	return numRegs;
}

//------------------------------------------------------------------------------
// CopyOperand
//------------------------------------------------------------------------------
// Copies the OperandInfo from pSrc->operand[dIndex] to pDest->operand[dIndex]

static void CopyOperand(OpcodeInfo* pDest, JILLong dIndex, const OpcodeInfo* pSrc, const JILLong sIndex)
{
	if( pDest && pSrc && dIndex >= 0 && dIndex < 4 && sIndex >= 0 && sIndex < 4 )
	{
		memcpy(pDest->operand + dIndex, pSrc->operand + sIndex, sizeof(OperandInfo));
	}
}

//------------------------------------------------------------------------------
// CompareOperands
//------------------------------------------------------------------------------
// compares the given operands

static JILBool CompareOperands(const OpcodeInfo* pSrc1, JILLong index1, const OpcodeInfo* pSrc2, JILLong index2)
{
	if( pSrc1 && pSrc2 && index1 >= 0 && index1 < 4 && index2 >= 0 && index2 < 4 )
	{
		if( pSrc1->operand[index1].type == pSrc2->operand[index2].type )
		{
			switch( pSrc1->operand[index1].type )
			{
				case ot_ear:
				case ot_eas:
					return (pSrc1->operand[index1].data[0] == pSrc2->operand[index2].data[0]);
				case ot_ead:
				case ot_eax:
					return (pSrc1->operand[index1].data[0] == pSrc2->operand[index2].data[0] &&
							pSrc1->operand[index1].data[1] == pSrc2->operand[index2].data[1]);
			}
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IndependentOperands
//------------------------------------------------------------------------------
// Checks if operand 1 is indenpendent from operand 2 and vice-versa.
// That means, if operand 1 uses a register that is also used by operand 2, or
// operand 2 uses a register that is also used by operand 1, then they are NOT
// independent.

static JILBool IndependentOperands(const OpcodeInfo* pInfo1, const JILLong op1, const OpcodeInfo* pInfo2, const JILLong op2)
{
	JILBool bResult = JILTrue;
	const OperandInfo* pop1 = pInfo1->operand + op1;
	const OperandInfo* pop2 = pInfo2->operand + op2;
	switch( pop1->type )
	{
		case ot_ear:
		case ot_ead:
			switch( pop2->type )
			{
				case ot_ear:
				case ot_ead:
					if( pop1->data[0] == pop2->data[0] )
						bResult = JILFalse;
					break;
				case ot_eax:
					if( pop1->data[0] == pop2->data[0] || pop1->data[0] == pop2->data[1] )
						bResult = JILFalse;
					break;
			}
			break;
		case ot_eax:
			switch( pop2->type )
			{
				case ot_ear:
				case ot_ead:
					if( pop1->data[0] == pop2->data[0] || pop1->data[1] == pop2->data[0] )
						bResult = JILFalse;
					break;
				case ot_eax:
					if( pop1->data[0] == pop2->data[0] ||
						pop1->data[1] == pop2->data[0] ||
						pop1->data[0] == pop2->data[1] ||
						pop1->data[1] == pop2->data[1] )
						bResult = JILFalse;
					break;
			}
			break;
	}
	return bResult;
}

//------------------------------------------------------------------------------
// GetBaseFromOpcode
//------------------------------------------------------------------------------
// Uses the instruction info table built into the JIL Runtime to look up the
// "base opcode" for a given opcode. The base opcode is the opcode with the
// simplest addressing mode. For example, if passing a op_move_xd, which is a
// "move (rx+ry), (rz+d)" in clear text, the function will return op_move_rr,
// which would be a "move rx, ry" in clear text. The base opcode is used in
// some of the optimization algorithm to identify the *kind* of instruction to
// generate, while not regarding the actual addressing mode.
// In the event of an error, the function will return JILFalse, so it is
// important to check for success!

static JILBool GetBaseFromOpcode(JILLong opcode, JILLong* pResult)
{
	const JILInstrInfo* pInfo = JILGetInfoFromOpcode(opcode);
	if( pInfo )
	{
		// now find the first instruction in the table with the same name :)
		JILLong index = JILGetInstructionIndex(pInfo->name, 0);
		if( index != -1 )
		{
			// get the info from that index...
			pInfo = JILGetInstructionInfo(index);
			if( pInfo )
			{
				// voil�, return the opcode
				*pResult = pInfo->opCode;
				return JILTrue;
			}
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// GetOpcodeFromBase
//------------------------------------------------------------------------------
// This is the counterpart to GetBaseOpcode. Given a base opcode and the
// desired addressing mode for all operands, this will return the correct
// opcode to use. The operand enums to pass are defined in jiltypes.h
// In the event of an error, the function will return JILFalse, so it is
// important to check for success!

static JILBool GetOpcodeFromBase(JILLong base, JILLong operand[4], JILLong* pResult)
{
	const JILInstrInfo* pInfo = JILGetInfoFromOpcode(base);
	if( pInfo )
	{
		const JILInstrInfo* pInfo2;
		JILLong index = 0;
		JILLong opr;
		for( index = JILGetInstructionIndex(pInfo->name, 0); index != -1; index++ )
		{
			// get info for this index
			pInfo2 = JILGetInstructionInfo(index);
			if( !pInfo2 )
				break;
			// verify same name
			if( strcmp(pInfo->name, pInfo2->name) != 0 )
				break;
			// compare operand types
			for( opr = 0; opr < pInfo2->numOperands; opr++ )
			{
				if( pInfo2->opType[opr] != operand[opr] )
					break;
			}
			if( opr == pInfo2->numOperands )
			{
				*pResult = pInfo2->opCode;
				return JILTrue;
			}
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// GetOpcodeInfo
//------------------------------------------------------------------------------
// Read in an instruction at the given address and fill a OpcodeInfo struct
// with information about the instruction.

static JILBool GetOpcodeInfo(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool bResult = JILFalse;
	const JILInstrInfo* pInfo;
	JILLong op;
	JILLong opr;
	JILLong oprsize;
	JILLong oprtype;
	JILLong i;

	memset(outInfo, 0, sizeof(OpcodeInfo));
	op = _this->array[addr];
	pInfo = JILGetInfoFromOpcode(op);
	if( pInfo )
	{
		if( GetBaseFromOpcode(op, &outInfo->base_opcode) )
		{
			addr++;
			for( opr = 0; opr < pInfo->numOperands; opr++ )
			{
				oprtype = pInfo->opType[opr];
				outInfo->operand[opr].type = oprtype;
				oprsize = JILGetOperandSize(oprtype);
				for( i = 0; i < oprsize; i++ )
					outInfo->operand[opr].data[i] = _this->array[addr++];
			}
			bResult = JILTrue;
		}
	}
	return bResult;
}

//------------------------------------------------------------------------------
// CreateInstruction
//------------------------------------------------------------------------------
// Creates and returns a full instruction (opcode including all operands) from
// an OpcodeInfo struct passed in. The function writes the instruction into the
// given buffer, which should be not smaller than 8 JILLong words. The size of
// the generated instruction is written to pSize.

static JILBool CreateInstruction(const OpcodeInfo* pInfo, JILLong* pBuffer, JILLong* pSize)
{
	JILBool bResult = JILFalse;
	JILLong operands[4];
	JILLong opr;
	JILLong numOpr;
	JILLong oprsize;
	JILLong i;

	// the number of operands is the same for all addressing mode variants of a
	// instruction, so we can just use the base opcode's number of operands!
	numOpr = JILGetInfoFromOpcode(pInfo->base_opcode)->numOperands;

	// fill operand type array
	for( opr = 0; opr < numOpr; opr++ )
		operands[opr] = pInfo->operand[opr].type;

	if( GetOpcodeFromBase(pInfo->base_opcode, operands, pBuffer++) )
	{
		*pSize = 1;
		// write operand data
		for( opr = 0; opr < numOpr; opr++ )
		{
			oprsize = JILGetOperandSize( pInfo->operand[opr].type );
			for( i = 0; i < oprsize; i++ )
				*pBuffer++ = pInfo->operand[opr].data[i];
			*pSize += oprsize;
		}
		bResult = JILTrue;
	}
	return bResult;
}

//------------------------------------------------------------------------------
// GetBranchAddr
//------------------------------------------------------------------------------

static JILBool GetBranchAddr(CodeBlock* _this, JILLong addr, JILLong* outAddr)
{
	JILLong offs;
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_bra:
			offs = _this->array[addr + 1];
			res = JILTrue;
			break;
		case op_tsteq_r:
		case op_tsteq_s:
		case op_tstne_r:
		case op_tstne_s:
			offs = _this->array[addr + 2];
			res = JILTrue;
			break;
		case op_tsteq_d:
		case op_tsteq_x:
		case op_tstne_d:
		case op_tstne_x:
			offs = _this->array[addr + 3];
			res = JILTrue;
			break;
		default:
			offs = 0;
			res = JILFalse;
			break;
	}
	*outAddr = addr + offs;
	return res;
}

//------------------------------------------------------------------------------
// SetBranchAddr
//------------------------------------------------------------------------------

static JILBool SetBranchAddr(CodeBlock* _this, JILLong addr, JILLong newAddr)
{
	JILBool res;
	JILLong offs = newAddr - addr;
	switch( _this->array[addr] )
	{
		case op_bra:
			_this->array[addr + 1] = offs;
			res = JILTrue;
			break;
		case op_tsteq_r:
		case op_tsteq_s:
		case op_tstne_r:
		case op_tstne_s:
			_this->array[addr + 2] = offs;
			res = JILTrue;
			break;
		case op_tsteq_d:
		case op_tsteq_x:
		case op_tstne_d:
		case op_tstne_x:
			_this->array[addr + 3] = offs;
			res = JILTrue;
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// IsAddrBranchTarget
//------------------------------------------------------------------------------
// Checks if the given address is the target of a un/conditional branch
// instruction in the code.

static JILBool IsAddrBranchTarget(CodeBlock* _this, JILLong addr)
{
	JILLong opaddr;
	JILLong opsize;
	JILLong branchAddr;
	// arg check
	if( addr < 0 || addr > _this->count )
		return JILFalse;
	// search whole code for branches
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize(_this->array[opaddr]);
		if( GetBranchAddr(_this, opaddr, &branchAddr) )
		{
			if( branchAddr == addr )
				return JILTrue;
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// InsertCode
//------------------------------------------------------------------------------
// JCLInsert instruction words (ints) into the function code and automatically fix
// branch addresses in the code. If fixInsPoint is false, branches jumping to
// insPoint are NOT altered, instead they will branch to the inserted code.

static void InsertCode(CodeBlock* _this, JILLong insPoint, JILLong numInts, JILBool fixInsPoint)
{
	JILLong opaddr;
	JILLong opcode;
	JILLong opsize;
	JILLong branchAddr;
	// arg check
	if( insPoint < 0 || insPoint > _this->count || numInts <= 0 )
		return;
	// fix branches first
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opcode = _this->array[opaddr];
		opsize = JILGetInstructionSize(opcode);
		if( GetBranchAddr(_this, opaddr, &branchAddr) )
		{
			if( branchAddr == insPoint && !fixInsPoint )
				continue;
			if( opaddr < insPoint && branchAddr >= insPoint )
			{
				branchAddr += numInts;
				SetBranchAddr(_this, opaddr, branchAddr);
			}
			else if( opaddr >= insPoint && branchAddr < insPoint )
			{
				branchAddr -= numInts;
				SetBranchAddr(_this, opaddr, branchAddr);
			}
		}
	}
	// still space in array?
	if( _this->count + numInts < _this->max )
	{
		memmove(_this->array + insPoint + numInts, _this->array + insPoint, (_this->count - insPoint) * sizeof(JILLong));
		memset(_this->array + insPoint, 0, numInts * sizeof(JILLong));
		_this->count += numInts;
	}
	else
	{
		// no, must reallocate
		JILLong oldMax = _this->max;
		JILLong oldCount = _this->count;
		JILLong* oldArray = _this->array;
		JILLong newMax = oldMax + numInts + 32;
		JILLong newCount = oldCount + numInts;
		JILLong* newArray = malloc(newMax * sizeof(JILLong));
		memcpy(newArray, oldArray, insPoint * sizeof(JILLong));
		memset(newArray + insPoint, 0, numInts * sizeof(JILLong));
		memcpy(newArray + insPoint + numInts, oldArray + insPoint, (oldCount - insPoint) * sizeof(JILLong));
		_this->max = newMax;
		_this->count = newCount;
		_this->array = newArray;
		free( oldArray );
	}
}

//------------------------------------------------------------------------------
// DeleteCode
//------------------------------------------------------------------------------
// Remove (delete) instruction words from the function code and automatically
// fix branch addresses in the code.

static void DeleteCode(CodeBlock* _this, JILLong delPoint, JILLong numInts)
{
	JILLong opaddr;
	JILLong opcode;
	JILLong opsize;
	JILLong branchAddr;
	// arg check
	if( delPoint < 0 || delPoint > _this->count || numInts <= 0 )
		return;
	if( numInts > _this->count - delPoint )
		numInts = _this->count - delPoint;
	// fix branches first
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opcode = _this->array[opaddr];
		opsize = JILGetInstructionSize(opcode);
		if( GetBranchAddr(_this, opaddr, &branchAddr) )
		{
			if( opaddr < delPoint && branchAddr >= (delPoint + numInts) )
			{
				branchAddr -= numInts;
				SetBranchAddr(_this, opaddr, branchAddr);
			}
			else if( opaddr >= (delPoint + numInts) && branchAddr < delPoint )
			{
				branchAddr += numInts;
				SetBranchAddr(_this, opaddr, branchAddr);
			}
			else if( branchAddr >= delPoint && branchAddr < (delPoint + numInts) )
			{
				branchAddr = delPoint;
				SetBranchAddr(_this, opaddr, branchAddr);
			}
		}
	}
	// move memory
	memmove(_this->array + delPoint, _this->array + delPoint + numInts, (_this->count - delPoint - numInts) * sizeof(JILLong));
	_this->count -= numInts;
}

//------------------------------------------------------------------------------
// ReplaceCode
//------------------------------------------------------------------------------
// Replaces instruction(s) at the given address by NOP instructions and fixes
// branch addresses automatically. The new number of instruction words can be
// smaller or greater than the old number of instruction words.

static void ReplaceCode(CodeBlock* _this, JILLong addr, JILLong oldNumInts, JILLong newNumInts)
{
	// check args
	if( addr < 0 || addr > _this->count || oldNumInts < 0 || newNumInts < 0 )
		return;
	if( oldNumInts > _this->count - addr )
		oldNumInts = _this->count - addr;
	// pad old area with NOP instructions
	memset(_this->array + addr, 0, oldNumInts * sizeof(JILLong));
	// do we have to shrink or expand the code?
	if( oldNumInts < newNumInts )
	{
		// insert NOP instructions
		JILLong intsToAdd = newNumInts - oldNumInts;
		JILLong insPoint = addr + oldNumInts;
		InsertCode(_this, insPoint, intsToAdd, JILTrue);
	}
	else if( oldNumInts > newNumInts )
	{
		// remove NOP instructions
		JILLong intsToDel = oldNumInts - newNumInts;
		JILLong delPoint = addr + newNumInts;
		DeleteCode(_this, delPoint, intsToDel);
	}
}

//------------------------------------------------------------------------------
// GetMoveToRegister
//------------------------------------------------------------------------------

static JILBool GetMoveToRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_move_rr:
		case op_move_dr:
		case op_move_xr:
		case op_move_sr:
		case op_moveh_r:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// GetCopyToRegister
//------------------------------------------------------------------------------

static JILBool GetCopyToRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_copy_rr:
		case op_copy_dr:
		case op_copy_xr:
		case op_copy_sr:
		case op_copyh_r:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// GetWrefToRegister
//------------------------------------------------------------------------------

static JILBool GetWrefToRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_wref_rr:
		case op_wref_dr:
		case op_wref_xr:
		case op_wref_sr:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// GetMoveFromRegister
//------------------------------------------------------------------------------

static JILBool GetMoveFromRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_move_rr:
		case op_copy_rr:
		case op_wref_rr:
		case op_move_rd:
		case op_copy_rd:
		case op_wref_rd:
		case op_move_rx:
		case op_copy_rx:
		case op_wref_rx:
		case op_move_rs:
		case op_copy_rs:
		case op_wref_rs:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// GetMathFromRegister
//------------------------------------------------------------------------------

static JILBool GetMathFromRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	switch( _this->array[addr] )
	{
		case op_add_rr:
		case op_add_rd:
		case op_add_rx:
		case op_add_rs:
		case op_addl_rr:
		case op_addl_rd:
		case op_addl_rx:
		case op_addl_rs:
		case op_addf_rr:
		case op_addf_rd:
		case op_addf_rx:
		case op_addf_rs:
		case op_sub_rr:
		case op_sub_rd:
		case op_sub_rx:
		case op_sub_rs:
		case op_subl_rr:
		case op_subl_rd:
		case op_subl_rx:
		case op_subl_rs:
		case op_subf_rr:
		case op_subf_rd:
		case op_subf_rx:
		case op_subf_rs:
		case op_mul_rr:
		case op_mul_rd:
		case op_mul_rx:
		case op_mul_rs:
		case op_mull_rr:
		case op_mull_rd:
		case op_mull_rx:
		case op_mull_rs:
		case op_mulf_rr:
		case op_mulf_rd:
		case op_mulf_rx:
		case op_mulf_rs:
		case op_div_rr:
		case op_div_rd:
		case op_div_rx:
		case op_div_rs:
		case op_divl_rr:
		case op_divl_rd:
		case op_divl_rx:
		case op_divl_rs:
		case op_divf_rr:
		case op_divf_rd:
		case op_divf_rx:
		case op_divf_rs:
		case op_mod_rr:
		case op_mod_rd:
		case op_mod_rx:
		case op_mod_rs:
		case op_modl_rr:
		case op_modl_rd:
		case op_modl_rx:
		case op_modl_rs:
		case op_modf_rr:
		case op_modf_rd:
		case op_modf_rx:
		case op_modf_rs:
		// some rather exotic "+" operations
		case op_stradd_rr:
		case op_stradd_dr:
		case op_stradd_xr:
		case op_stradd_sr:
		case op_arrcp_rr:
		case op_arrcp_dr:
		case op_arrcp_xr:
		case op_arrcp_sr:
		case op_arrmv_rr:
		case op_arrmv_dr:
		case op_arrmv_xr:
		case op_arrmv_sr:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// GetCompareRegister
//------------------------------------------------------------------------------

static JILBool GetCompareRegister(CodeBlock* _this, JILLong addr, OpcodeInfo* outInfo)
{
	JILBool res;
	JILLong op = _this->array[addr];
	switch( op )
	{
		case op_cseq_rr:
		case op_csne_rr:
		case op_csgt_rr:
		case op_csge_rr:
		case op_cslt_rr:
		case op_csle_rr:
		case op_cseql_rr:
		case op_csnel_rr:
		case op_csgtl_rr:
		case op_csgel_rr:
		case op_csltl_rr:
		case op_cslel_rr:
		case op_cseqf_rr:
		case op_csnef_rr:
		case op_csgtf_rr:
		case op_csgef_rr:
		case op_csltf_rr:
		case op_cslef_rr:
		// some rather exotic compare operations
		case op_streq_rr:
		case op_strne_rr:
			res = GetOpcodeInfo(_this, addr, outInfo);
			break;
		default:
			res = JILFalse;
			break;
	}
	return res;
}

//------------------------------------------------------------------------------
// CreateCombinedMove
//------------------------------------------------------------------------------

static JILBool CreateCombinedMove(CodeBlock* _this, const OpcodeInfo* srcInfo, const OpcodeInfo* dstInfo, JILLong* pBuffer, JILLong* pSize)
{
	JILBool res = JILFalse;
	// is the destination register of the 1st instruction the same as the source register of the 2nd instruction?
	if( srcInfo->operand[dst].type == ot_ear &&
		dstInfo->operand[src].type == ot_ear &&
		srcInfo->operand[dst].data[0] == dstInfo->operand[src].data[0] )
	{
		OpcodeInfo mergedInfo;
		// choose which new instruction to make
		if( srcInfo->base_opcode == op_move_rr )		// src used a move
			mergedInfo.base_opcode = dstInfo->base_opcode;
		else if( srcInfo->base_opcode == op_wref_rr )	// src used a wref
			mergedInfo.base_opcode = srcInfo->base_opcode;
		else if( srcInfo->base_opcode == op_moveh_r )	// src used a moveh
		{
			// if destination used move, make a moveh
			if( dstInfo->base_opcode == op_move_rr )
				mergedInfo.base_opcode = op_moveh_r;
			// if destination used copy, make a copyh
			else if( dstInfo->base_opcode == op_copy_rr )
				mergedInfo.base_opcode = op_copyh_r;
			// if destination used set, fail - there is no seth instruction!
			else
				return JILFalse;
		}
		else if( srcInfo->base_opcode == op_copy_rr )	// src used a copy
		{
			// if destination used move, make a copy
			if( dstInfo->base_opcode == op_move_rr )
				mergedInfo.base_opcode = op_copy_rr;
			// if destination used copy, fail
			else if( dstInfo->base_opcode == op_copy_rr )
				return JILFalse;
			// if destination used set, fail
			else
				return JILFalse;
		}
		else if( srcInfo->base_opcode == op_copyh_r )	// src used a copyh
		{
			// if destination used move, make a copyh
			if( dstInfo->base_opcode == op_move_rr )
				mergedInfo.base_opcode = op_copyh_r;
			// if destination used copy, fail
			else if( dstInfo->base_opcode == op_copy_rr )
				return JILFalse;
			// if destination used set, fail - there is no seth instruction!
			else
				return JILFalse;
		}
		// will use the first instruction's source operand
		CopyOperand( &mergedInfo, src, srcInfo, src );
		// and the second instruction's destination operand
		CopyOperand( &mergedInfo, dst, dstInfo, dst );
		// did we end up with something like "move r1,r1" ?
		if( mergedInfo.base_opcode == op_move_rr &&
			mergedInfo.operand[src].type == ot_ear &&
			mergedInfo.operand[dst].type == ot_ear &&
			mergedInfo.operand[src].data[0] == mergedInfo.operand[dst].data[0] )
		{
			// remove code that has no effect!
			*pSize = 0;
			res = JILTrue;
		}
		else
		{
			// create the instruction!
			res = CreateInstruction(&mergedInfo, pBuffer, pSize);
		}
	}
	return res;
}

//------------------------------------------------------------------------------
// CreateCombinedMath
//------------------------------------------------------------------------------

static JILBool CreateCombinedMath(CodeBlock* _this, const OpcodeInfo* moveInfo, const OpcodeInfo* mathInfo, JILLong* pBuffer, JILLong* pSize)
{
	JILBool res = JILFalse;
	// destination register of the move instruction same as source register of the math instruction?
	if( moveInfo->operand[dst].type == ot_ear &&
		mathInfo->operand[src].type == ot_ear &&
		moveInfo->operand[dst].data[0] == mathInfo->operand[src].data[0] )
	{
		if( moveInfo->base_opcode != op_moveh_r )	// cannot deal with moveh!
		{
			OpcodeInfo mergedInfo;
			// choose which new instruction to make
			mergedInfo.base_opcode = mathInfo->base_opcode;
			// will use the first instruction's source operand
			CopyOperand( &mergedInfo, src, moveInfo, src );
			// and the second instruction's destination operand
			CopyOperand( &mergedInfo, dst, mathInfo, dst );
			// create the instruction!
			res = CreateInstruction(&mergedInfo, pBuffer, pSize);
		}
	}
	return res;
}

//------------------------------------------------------------------------------
// CreateCombinedCompare
//------------------------------------------------------------------------------

static JILBool CreateCombinedCompare(CodeBlock* _this, OpcodeInfo* move1Info, OpcodeInfo* move2Info, OpcodeInfo* cmpInfo, JILLong* pBuffer, JILLong* pSize)
{
	JILBool res = JILFalse;
	if( move1Info->operand[dst].type == ot_ear &&
		cmpInfo->operand[1].type == ot_ear &&
		move1Info->operand[dst].data[0] == cmpInfo->operand[1].data[0] &&
		move2Info->operand[dst].type == ot_ear &&
		cmpInfo->operand[0].type == ot_ear &&
		move2Info->operand[dst].data[0] == cmpInfo->operand[0].data[0] )
	{
		// we can only optimize either operand[0] or operand[1]...
		if( move2Info->base_opcode != op_moveh_r )	// we favor operand[0]!
		{
			// make sure move 1 does NOT modify a register used in move 2!
			if( IndependentOperands(move1Info, dst, move2Info, src) )
			{
				JILLong newSize;
				// cmp will use the second move's source operand
				CopyOperand( cmpInfo, 0, move2Info, src );
				// create the instructions
				res = CreateInstruction(move1Info, pBuffer, pSize);
				if( !res ) return res;
				newSize = *pSize;
				pBuffer += newSize;
				res = CreateInstruction(cmpInfo, pBuffer, pSize);
				if( !res ) return res;
				newSize += *pSize;
				*pSize = newSize;
			}
		}
		else if( move1Info->base_opcode != op_moveh_r )
		{
			// make sure move 2 does NOT modify a register used in move 1!
			if( IndependentOperands(move2Info, dst, move1Info, src) )
			{
				JILLong newSize;
				// cmp will use the first move's source operand
				CopyOperand( cmpInfo, 1, move1Info, src );
				// create the instructions
				res = CreateInstruction(move2Info, pBuffer, pSize);
				if( !res ) return res;
				newSize = *pSize;
				pBuffer += newSize;
				res = CreateInstruction(cmpInfo, pBuffer, pSize);
				if( !res ) return res;
				newSize += *pSize;
				*pSize = newSize;
			}
		}
	}
	return res;
}

//------------------------------------------------------------------------------
// GetInstructionInitRegister
//------------------------------------------------------------------------------
// This function checks whether the instruction at 'addr' initializes a register
// and returns the register number if true.
// NOTE: This function assumes that the compiler will not use the "pop", "popr"
// or "popm" instructions to initialize a register.

static JILBool GetInstructionInitRegister(CodeBlock* _this, JILLong addr, JILLong *regNum)
{
	switch( _this->array[addr] )
	{
		case op_ldz_r:
			*regNum = _this->array[addr + 1];
			return JILTrue;
		case op_moveh_r:
		case op_copyh_r:
		case op_move_rr:
		case op_move_sr:
		case op_copy_rr:
		case op_copy_sr:
		case op_wref_rr:
		case op_wref_sr:
		case op_alloc:
		case op_allocn:
		case op_alloci:
		case op_cvf:
		case op_cvl:
		case op_snul_rr:
		case op_type:
			*regNum = _this->array[addr + 2];
			return JILTrue;
		case op_move_dr:
		case op_move_xr:
		case op_copy_dr:
		case op_copy_xr:
		case op_wref_dr:
		case op_wref_xr:
		case op_alloca:
		case op_newdg:
		case op_newctx:
			*regNum = _this->array[addr + 3];
			return JILTrue;
		case op_cseq_rr:
		case op_cseql_rr:
		case op_cseqf_rr:
		case op_csne_rr:
		case op_csnel_rr:
		case op_csnef_rr:
		case op_csgt_rr:
		case op_csgtl_rr:
		case op_csgtf_rr:
		case op_csge_rr:
		case op_csgel_rr:
		case op_csgef_rr:
		case op_cslt_rr:
		case op_csltl_rr:
		case op_csltf_rr:
		case op_csle_rr:
		case op_cslel_rr:
		case op_cslef_rr:
		case op_cseq_rs:
		case op_cseql_rs:
		case op_cseqf_rs:
		case op_csne_rs:
		case op_csnel_rs:
		case op_csnef_rs:
		case op_csgt_rs:
		case op_csgtl_rs:
		case op_csgtf_rs:
		case op_csge_rs:
		case op_csgel_rs:
		case op_csgef_rs:
		case op_cslt_rs:
		case op_csltl_rs:
		case op_csltf_rs:
		case op_csle_rs:
		case op_cslel_rs:
		case op_cslef_rs:
		case op_cseq_sr:
		case op_cseql_sr:
		case op_cseqf_sr:
		case op_csne_sr:
		case op_csnel_sr:
		case op_csnef_sr:
		case op_csgt_sr:
		case op_csgtl_sr:
		case op_csgtf_sr:
		case op_csge_sr:
		case op_csgel_sr:
		case op_csgef_sr:
		case op_cslt_sr:
		case op_csltl_sr:
		case op_csltf_sr:
		case op_csle_sr:
		case op_cslel_sr:
		case op_cslef_sr:
		case op_streq_rr:
		case op_streq_sr:
		case op_streq_rs:
		case op_strne_rr:
		case op_strne_sr:
		case op_strne_rs:
		case op_cmpref_rr:
		case op_dcvt:
			*regNum = _this->array[addr + 3];
			return JILTrue;
		case op_cseq_rd:
		case op_cseql_rd:
		case op_cseqf_rd:
		case op_csne_rd:
		case op_csnel_rd:
		case op_csnef_rd:
		case op_csgt_rd:
		case op_csgtl_rd:
		case op_csgtf_rd:
		case op_csge_rd:
		case op_csgel_rd:
		case op_csgef_rd:
		case op_cslt_rd:
		case op_csltl_rd:
		case op_csltf_rd:
		case op_csle_rd:
		case op_cslel_rd:
		case op_cslef_rd:
		case op_cseq_dr:
		case op_cseql_dr:
		case op_cseqf_dr:
		case op_csne_dr:
		case op_csnel_dr:
		case op_csnef_dr:
		case op_csgt_dr:
		case op_csgtl_dr:
		case op_csgtf_dr:
		case op_csge_dr:
		case op_csgel_dr:
		case op_csgef_dr:
		case op_cslt_dr:
		case op_csltl_dr:
		case op_csltf_dr:
		case op_csle_dr:
		case op_cslel_dr:
		case op_cslef_dr:
		case op_cseq_xr:
		case op_cseql_xr:
		case op_cseqf_xr:
		case op_csne_xr:
		case op_csnel_xr:
		case op_csnef_xr:
		case op_csgt_xr:
		case op_csgtl_xr:
		case op_csgtf_xr:
		case op_csge_xr:
		case op_csgel_xr:
		case op_csgef_xr:
		case op_cslt_xr:
		case op_csltl_xr:
		case op_csltf_xr:
		case op_csle_xr:
		case op_cslel_xr:
		case op_cslef_xr:
		case op_cseq_rx:
		case op_cseql_rx:
		case op_cseqf_rx:
		case op_csne_rx:
		case op_csnel_rx:
		case op_csnef_rx:
		case op_csgt_rx:
		case op_csgtl_rx:
		case op_csgtf_rx:
		case op_csge_rx:
		case op_csgel_rx:
		case op_csgef_rx:
		case op_cslt_rx:
		case op_csltl_rx:
		case op_csltf_rx:
		case op_csle_rx:
		case op_cslel_rx:
		case op_cslef_rx:
		case op_streq_rd:
		case op_streq_dr:
		case op_streq_rx:
		case op_streq_xr:
		case op_strne_rd:
		case op_strne_dr:
		case op_strne_rx:
		case op_strne_xr:
		case op_newdgm:
		case op_newdgc:
			*regNum = _this->array[addr + 4];
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsRegisterInitialized
//------------------------------------------------------------------------------
// This function scans the function code in order to find out whether the
// specified register is used. A register is detected as beeing used, if an
// instruction is found that moves or copies a value into that register.
// In other words, if the register is initialized as a result of a specific
// instruction, this register is 'used'.
// The function starts scanning at the given address and stops at the end of
// the whole code.
// See also NOTES for GetInstructionInitRegister().

static JILBool IsRegisterInitialized(CodeBlock* _this, JILLong addr, JILLong regNum)
{
	JILLong opaddr;
	JILLong opsize;
	JILLong opregnum;

	for( opaddr = addr; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		if( GetInstructionInitRegister(_this, opaddr, &opregnum) && opregnum == regNum )
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsPushRegister
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "push rn" instruction and returns
// the register number if true.

static JILBool IsPushRegister(CodeBlock* _this, JILLong addr, JILLong* regNum)
{
	if( _this->array[addr] == op_push_r )
	{
		*regNum = _this->array[addr + 1];
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsPushMulti
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "pushr rn-rm" instruction and
// returns a map of the registers pushed. Returns the number of pushed registers
// as a result value or 0, if the instruction is not a pushr instruction.
// The given 'regMap' array will contain the register numbers saved by the
// instruction and should not be less than kNumRegisters in size.

static JILLong IsPushMulti(CodeBlock* _this, JILLong addr, JILLong* regMap)
{
	if( _this->array[addr] == op_pushr )
	{
		JILLong i;
		JILLong reg = _this->array[addr + 1];
		JILLong cnt = _this->array[addr + 2];
		for( i = 0; i < cnt; i++ )
			regMap[i] = reg++;
		return cnt;
	}
	return 0;
}

//------------------------------------------------------------------------------
// IsPopRegister
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "pop rn" instruction with the given
// register number as operand.

static JILBool IsPopRegister(CodeBlock* _this, JILLong addr, JILLong regNum)
{
	if( _this->array[addr] == op_pop_r &&
		_this->array[addr + 1] == regNum )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsPopMulti
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "popr rn-rm" instruction, and
// whether it pops the given register number from the stack.

static JILLong IsPopMulti(CodeBlock* _this, JILLong addr, JILLong regNum)
{
	if( _this->array[addr] == op_popr )
	{
		JILLong reg = _this->array[addr + 1];
		JILLong cnt = _this->array[addr + 2];
		return (regNum >= reg && regNum < (reg + cnt));
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// PushMultiDecrement
//------------------------------------------------------------------------------
// Decreases the count operand from a pushr or popr instruction if the given
// register number is the HIGHEST register number that this instruction saves /
// restores. Thus, the register is "removed" from the instruction.
// Returns -1 if the instruction at addr is not pushr / popr, or if regNum
// cannot be removed from the operand (is not the highest register), otherwise
// returns the new count operand. If 0 is returned, the instruction can be
// (SHOULD BE!) completely removed from the code.

static JILLong PushMultiDecrement(CodeBlock* _this, JILLong addr, JILLong regNum)
{
	if( _this->array[addr] == op_pushr || _this->array[addr] == op_popr )
	{
		JILLong reg = _this->array[addr + 1];
		JILLong cnt = _this->array[addr + 2];
		if( regNum == (reg + cnt - 1) )
		{
			_this->array[addr + 2]--;
			return _this->array[addr + 2];
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// IsPushNullHandle
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "push" or "pushm" instruction and
// returns the amount of null handles pushed, if true.

static JILBool IsPushNullHandle(CodeBlock* _this, JILLong addr, JILLong* count)
{
	if( _this->array[addr] == op_push )
	{
		*count = 1;
		return JILTrue;
	}
	else if( _this->array[addr] == op_pushm )
	{
		*count = _this->array[addr + 1];
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsPopAndForget
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is a "pop" or "popm" instruction and
// returns the amount of handles pop'd from stack, if true.

static JILBool IsPopAndForget(CodeBlock* _this, JILLong addr, JILLong* count)
{
	if( _this->array[addr] == op_pop )
	{
		*count = 1;
		return JILTrue;
	}
	else if( _this->array[addr] == op_popm )
	{
		*count = _this->array[addr + 1];
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// GetStackModifier
//------------------------------------------------------------------------------
// Checks if the instruction at 'addr' is modifying the stack pointer and
// returns the amount (positive or negative) if true.
// Unlike the two functions above this function recognizes ALL push and pop
// instructions.

static JILBool GetStackModifier(CodeBlock* _this, JILLong addr, JILLong* count)
{
	switch( _this->array[addr] )
	{
		case op_push:
		case op_push_r:
		case op_push_d:
		case op_push_x:
		case op_push_s:
			*count = 1;
			return JILTrue;
		case op_pop:
		case op_pop_r:
		case op_pop_d:
		case op_pop_x:
		case op_pop_s:
			*count = -1;
			return JILTrue;
		case op_pushm:
			*count = _this->array[addr + 1];
			return JILTrue;
		case op_popm:
			*count = - _this->array[addr + 1];
			return JILTrue;
		case op_pushr:
			*count = _this->array[addr + 2];
			return JILTrue;
		case op_popr:
			*count = - _this->array[addr + 2];
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// InstructionUsesRegister
//------------------------------------------------------------------------------
// This function uses the instruction table built into the JIL Runtime to check
// whether a source or destination operand of the instruction at 'addr' uses
// a specific register.

static JILBool InstructionUsesRegister(CodeBlock* _this, JILLong addr, JILLong regNum)
{
	const JILInstrInfo* pInfo = JILGetInfoFromOpcode(_this->array[addr]);
	if( pInfo->numOperands )
	{
		JILLong i;
		JILLong opaddr = addr + 1;
		for( i = 0; i < pInfo->numOperands; i++ )
		{
			switch( pInfo->opType[i] )
			{
				case ot_ear:
				case ot_ead:
					if( _this->array[opaddr] == regNum )
						return JILTrue;
					break;
				case ot_eax:
					if( _this->array[opaddr] == regNum ||
						_this->array[opaddr + 1] == regNum )
						return JILTrue;
					break;
			}
			opaddr += JILGetOperandSize( pInfo->opType[i] );
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// InstructionReplaceRegister
//------------------------------------------------------------------------------
// This function replaces references to the 'findReg' register by references to
// the 'replReg' register. If the instruction at the current position does not
// refer 'findReg' nothing is changed and JILFalse is returned.

static JILBool InstructionReplaceRegister(CodeBlock* _this, JILLong addr, JILLong findReg, JILLong replReg)
{
	JILBool bSuccess = JILFalse;
	const JILInstrInfo* pInfo = JILGetInfoFromOpcode(_this->array[addr]);
	if( pInfo->numOperands )
	{
		JILLong i;
		JILLong opaddr = addr + 1;
		// first do a "dry" attempt to see if replacing is possible
		for( i = 0; i < pInfo->numOperands; i++ )
		{
			switch( pInfo->opType[i] )
			{
				case ot_ear:
				case ot_ead:
					if( _this->array[opaddr] == findReg )
						bSuccess = JILTrue;
					break;
				case ot_eax:
					if( _this->array[opaddr] == findReg )
					{
						if( _this->array[opaddr + 1] == replReg )
							return JILFalse;
						bSuccess = JILTrue;
					}
					else if( _this->array[opaddr + 1] == findReg )
					{
						if( _this->array[opaddr] == replReg )
							return JILFalse;
						bSuccess = JILTrue;
					}
					break;
			}
			opaddr += JILGetOperandSize( pInfo->opType[i] );
		}
		if( bSuccess )
		{
			// now really replace
			opaddr = addr + 1;
			for( i = 0; i < pInfo->numOperands; i++ )
			{
				switch( pInfo->opType[i] )
				{
					case ot_ear:
					case ot_ead:
						if( _this->array[opaddr] == findReg )
							_this->array[opaddr] = replReg;
						break;
					case ot_eax:
						if( _this->array[opaddr] == findReg )
							_this->array[opaddr] = replReg;
						else if( _this->array[opaddr + 1] == findReg )
							_this->array[opaddr + 1] = replReg;
						break;
				}
				opaddr += JILGetOperandSize( pInfo->opType[i] );
			}
		}
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
// MoveToCopyInstr
//------------------------------------------------------------------------------
// Convert a move instruction with any addressing mode into it's copy
// counterpart. This includes the moveh type of instructions.

static JILLong MoveToCopyInstr(JILLong opcode)
{
	switch( opcode )
	{
		case op_move_rr:	return op_copy_rr;
		case op_move_rd:	return op_copy_rd;
		case op_move_rx:	return op_copy_rx;
		case op_move_rs:	return op_copy_rs;
		case op_move_dr:	return op_copy_dr;
		case op_move_dd:	return op_copy_dd;
		case op_move_dx:	return op_copy_dx;
		case op_move_ds:	return op_copy_ds;
		case op_move_xr:	return op_copy_xr;
		case op_move_xd:	return op_copy_xd;
		case op_move_xx:	return op_copy_xx;
		case op_move_xs:	return op_copy_xs;
		case op_move_sr:	return op_copy_sr;
		case op_move_sd:	return op_copy_sd;
		case op_move_sx:	return op_copy_sx;
		case op_move_ss:	return op_copy_ss;
		case op_moveh_r:	return op_copyh_r;
		case op_moveh_d:	return op_copyh_d;
		case op_moveh_x:	return op_copyh_x;
		case op_moveh_s:	return op_copyh_s;
	}
	return 0;
}

//------------------------------------------------------------------------------
// IsBranchInstruction
//------------------------------------------------------------------------------
// Checks if an instruction at the given address is a branch and returns the
// branch offset and whether or not the branch is conditional.

static JILBool IsBranchInstruction(CodeBlock* _this, JILLong addr, JILLong* offset, JILBool* isCond)
{
	switch( _this->array[addr] )
	{
		case op_bra:
			*isCond = JILFalse;
			*offset = _this->array[addr + 1];
			return JILTrue;
		case op_tsteq_r:
		case op_tsteq_s:
		case op_tstne_r:
		case op_tstne_s:
			*isCond = JILTrue;
			*offset = _this->array[addr + 2];
			return JILTrue;
		case op_tsteq_d:
		case op_tsteq_x:
		case op_tstne_d:
		case op_tstne_x:
			*isCond = JILTrue;
			*offset = _this->array[addr + 3];
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// FixStackOffsetsInBranch
//------------------------------------------------------------------------------
// Fixes all accesses to arguments on the stack [addressing mode (sp+d)] by the
// given fixup amount. The initial "stackpointer" is given. This function will
// recurse if a conditional branch is detected.
// The function will also follow unconditional branches and will advance from
// the given address until either a 'ret' instruction is found or the given stop
// address is detected.

static void FixStackOffsetsInBranch(CodeBlock* _this, JILLong addr, JILLong stopAddr, JILLong fixup, JILLong stackPointer, JILChar* tbl)
{
	JILLong opaddr;
	JILLong opsize;
	JILLong opcode;
	JILLong i;
	JILLong subAddr;
	JILLong modiAmount;
	JILLong branchOffset;
	JILBool isConditional;
	JILBool freeTbl = JILFalse;
	const JILInstrInfo* instrInfo;
	// if 'tbl' is NULL, create a new table
	if( tbl == NULL )
	{
		freeTbl = JILTrue;
		tbl = malloc(_this->count);
		memset(tbl, 0, _this->count);
	}
	for( opaddr = addr; opaddr < stopAddr; opaddr += opsize )
	{
		opcode = _this->array[opaddr];
		opsize = JILGetInstructionSize(opcode);
		// stop if ret || this code path has already been traced
		if( opcode == op_ret || tbl[opaddr] )
			break;	// done
		tbl[opaddr] = JILTrue;
		// check if instruction accesses stack and fix up
		instrInfo = JILGetInfoFromOpcode(opcode);
		subAddr = opaddr + 1;
		for( i = 0; i < instrInfo->numOperands; i++ )
		{
			if( instrInfo->opType[i] == ot_eas )
			{
				if( _this->array[subAddr] >= stackPointer )
					_this->array[subAddr] += fixup;
			}
			subAddr += JILGetOperandSize(instrInfo->opType[i]);
		}
		// special case closure
		if( opcode == op_newdgc )
		{
			_this->array[opaddr + 2] += fixup;
		}
		// take into account push/pop and branches
		if( GetStackModifier(_this, opaddr, &modiAmount) )
		{
			stackPointer += modiAmount;
		}
		else if( IsBranchInstruction(_this, opaddr, &branchOffset, &isConditional) )
		{
			// we only need to care about forward branches!
			if( branchOffset > 0 )
			{
				JILLong targetAddr = opaddr + branchOffset;
				// if branch is conditional, recurse to fix conditional "body"
				if( isConditional )
					FixStackOffsetsInBranch(_this, opaddr + opsize, _this->count, fixup, stackPointer, tbl);
				// continue from branch target
				opaddr = targetAddr;
				opsize = 0;
			}
		}
	}
	if( freeTbl )
	{
		free(tbl);
	}
}

//------------------------------------------------------------------------------
// IsOpcodeSwappable
//------------------------------------------------------------------------------
// Checks if the source and destination operands of the given opcode can be
// swapped without affecting the result. Example: 1+2 and 2+1 yields the same
// result, while 1-2 and 2-1 are not swappable.

static JILBool IsOpcodeSwappable(JILLong opcode)
{
	switch( opcode )
	{
		case op_add_rr:
		case op_mul_rr:
		case op_addl_rr:
		case op_mull_rr:
		case op_addf_rr:
		case op_mulf_rr:
		case op_and_rr:
		case op_or_rr:
		case op_xor_rr:
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsTestEqual
//------------------------------------------------------------------------------
// Checks if an instruction at the given address is a tsteq instruction and
// returns the appropriate counter part.

static JILBool IsTestEqual(CodeBlock* _this, JILLong addr, OpcodeInfo* pInfo)
{
	if( GetOpcodeInfo(_this, addr, pInfo) )
	{
		if( pInfo->base_opcode == op_tsteq_r )
		{
			pInfo->base_opcode = op_tstne_r;
			return JILTrue;
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsMovingThisRef
//------------------------------------------------------------------------------
// Checks if an instruction moves r0 into any variable or register.

static JILBool IsMovingThisRef(CodeBlock* _this, JILLong addr, OpcodeInfo* pInfo)
{
	if( GetMoveFromRegister(_this, addr, pInfo) )
	{
		if( pInfo->operand[0].type == ot_ear && pInfo->operand[0].data[0] == 0 ) // r0?
		{
			return (pInfo->operand[1].type != ot_ear || pInfo->operand[1].data[0] != 1); // allow r1
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// InsertRegisterSaving
//------------------------------------------------------------------------------
// Inserts code that saves all modified registers at the start of the function
// and restores all saved registers at the end.

static JILError InsertRegisterSaving(JCLFunc* pFunc, JCLState* pCompiler)
{
	JILError err = JCL_No_Error;
	JILLong numRegsToSave;
	JILLong opaddr;
	JILLong opsize;
	JILLong opcode;
	CodeBlock* _this = pFunc->mipCode;

	// calculate numbers of registers to save
	numRegsToSave = GetNumRegsToSave(pFunc);
	// if zero, nothing to do
	if( numRegsToSave == 0 )
		return err;
	// fix all stack offsets accordingly
	FixStackOffsetsInBranch(_this, 0, _this->count, numRegsToSave, 0, NULL);
	// insert push code at start of function
	if( numRegsToSave == 1 )
	{
		InsertCode(_this, 0, 2, JILFalse);
		_this->array[0] = op_push_r;
		_this->array[1] = 3;
	}
	else if (numRegsToSave > 1)
	{
		InsertCode(_this, 0, 3, JILFalse);
		_this->array[0] = op_pushr;
		_this->array[1] = 3;
		_this->array[2] = numRegsToSave;
	}
	// insert pop code at all exits of function
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opcode = _this->array[opaddr];
		opsize = JILGetInstructionSize(opcode);
		if( opcode == op_ret )
		{
			if( numRegsToSave == 1 )
			{
				InsertCode(_this, opaddr, 2, JILFalse);
				_this->array[opaddr] = op_pop_r;
				_this->array[opaddr + 1] = 3;
				opsize += 2;
			}
			else if (numRegsToSave > 1)
			{
				InsertCode(_this, opaddr, 3, JILFalse);
				_this->array[opaddr] = op_popr;
				_this->array[opaddr + 1] = 3;
				_this->array[opaddr + 2] = numRegsToSave;
				opsize += 3;
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// OptimizeMoveOperations
//------------------------------------------------------------------------------
// Optimizes two move/copy instructions into a single one, if possible.
// Example:
//		move [any source], r3
//		copy r3, [any destination]
// Is optimized into:
//		copy [any source], [any destination]

static JILError OptimizeMoveOperations(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILBool bCont;
	OpcodeInfo mtrInfo;
	OpcodeInfo mfrInfo;
	CodeBlock* _this = pFunc->mipCode;
	do
	{
		pReport->totalPasses++;
		bCont = JILFalse;
		for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
		{
			opsize = JILGetInstructionSize( _this->array[opaddr] );
			if( GetMoveToRegister(_this, opaddr, &mtrInfo) ||
				GetCopyToRegister(_this, opaddr, &mtrInfo) ||
				GetWrefToRegister(_this, opaddr, &mtrInfo) )
			{
				JILLong opsize2;
				JILLong opaddr2 = opaddr + opsize;
				if( opaddr2 < _this->count )
				{
					opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
					if( GetMoveFromRegister(_this, opaddr2, &mfrInfo) &&
						!IsAddrBranchTarget(_this, opaddr2) )
					{
						JILLong buffer[8];
						JILLong newSize = 0;
						if( CreateCombinedMove(_this, &mtrInfo, &mfrInfo, buffer, &newSize) )
						{
							ReplaceCode(_this, opaddr, opsize + opsize2, newSize);
							memcpy(_this->array + opaddr, buffer, newSize * sizeof(JILLong));
							opsize = newSize;
							if( newSize )
								pReport->instr_added++;
							pReport->instr_removed += 2;
							bCont = JILTrue;
						}
					}
				}
			}
		}
		if( bCont )
			pReport->numPasses++;
	} while( bCont );
	pReport->count_after = _this->count;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeOperationAndMove
//------------------------------------------------------------------------------
// Optimization for the case where a certain operation is performed on a
// register, where the result is then moved back to the source of the operation.
// In certain cases, the move can be omitted, if the operation source and
// destination operands are reversed.
// - The source operand of the first instructon must be the destination operand
//   of the following move instruction
// - The destination operand of the first instruction must be the same as the
//   source operand of the following move instruction
// - The destination operand of the first instruction must have register
//   addressing mode and must not be used as a local variable (= must be temp var)
// Example:
//		add [source], r3
//		move r3, [source]
// Is optimized into:
//		add r3, [source]

static JILError OptimizeOperationAndMove(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILBool bCont;
	OpcodeInfo info;
	OpcodeInfo info2;
	CodeBlock* _this = pFunc->mipCode;
	do
	{
		pReport->totalPasses++;
		bCont = JILFalse;
		for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
		{
			opsize = JILGetInstructionSize( _this->array[opaddr] );
			if( GetOpcodeInfo(_this, opaddr, &info) && info.operand[dst].type == ot_ear )
			{
				if( IsOpcodeSwappable(info.base_opcode) )
				{
					JILLong opsize2;
					JILLong opaddr2 = opaddr + opsize;
					opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
					if( GetOpcodeInfo(_this, opaddr2, &info2) &&
						info2.base_opcode == op_move_rr &&
						CompareOperands(&info, src, &info2, dst) &&
						CompareOperands(&info, dst, &info2, src) )
					{
						JILLong buffer[8];
						JILLong newSize;
						OpcodeInfo dummy;
						// swap operands
						CopyOperand(&dummy, src, &info, src);
						CopyOperand(&info, src, &info, dst);
						CopyOperand(&info, dst, &dummy, src);
						if( CreateInstruction(&info, buffer, &newSize) )
						{
							ReplaceCode(_this, opaddr, opsize + opsize2, newSize);
							memcpy(_this->array + opaddr, buffer, newSize * sizeof(JILLong));
							bCont = JILTrue;
							pReport->instr_removed++;
							opsize = newSize;
						}
					}
				}
			}
		}
		if( bCont )
			pReport->numPasses++;
	} while( bCont );
	pReport->count_after = _this->count;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeMathOperations
//------------------------------------------------------------------------------
// Optimizes a move instruction into a temp register, followed by a arithmetical
// instruction from that temp register to another register into a single
// instruction.
// Example:
//		move [any source], r3
//		addl r3, r4
// Is optimized into:
//		addl [any source], r3
// We can also use the same algorithm for the uand and uor instructions.

static JILError OptimizeMathOperations(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILBool bCont;
	OpcodeInfo ins1Info;
	OpcodeInfo ins2Info;
	CodeBlock* _this = pFunc->mipCode;
	pReport->totalPasses++;
	bCont = JILFalse;
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		if( GetMoveToRegister(_this, opaddr, &ins1Info) )
		{
			JILLong opsize2;
			JILLong opaddr2 = opaddr + opsize;
			if( opaddr2 < _this->count )
			{
				opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
				if( GetMathFromRegister(_this, opaddr2, &ins2Info) )
				{
					JILLong buffer[8];
					JILLong newSize = 0;
					if( CreateCombinedMath(_this, &ins1Info, &ins2Info, buffer, &newSize) )
					{
						ReplaceCode(_this, opaddr, opsize + opsize2, newSize);
						memcpy(_this->array + opaddr, buffer, newSize * sizeof(JILLong));
						opsize = newSize;
						pReport->instr_added++;
						pReport->instr_removed += 2;
						bCont = JILTrue;
					}
				}
			}
		}
	}
	if( bCont )
		pReport->numPasses++;
	pReport->count_after = _this->count;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeCompareOperations
//------------------------------------------------------------------------------
// Optimizes two move instructions into temp registers, followed by a compare
// instruction into one move instruction and one compare instruction.
// Example:
//		move [any source1], r3
//		move [any source2], r4
//		csge r4, r3, r3
// Is optimized into:
//		move [any source1], r3
//		csge [any source2], r3, r3

static JILError OptimizeCompareOperations(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILBool bCont;
	OpcodeInfo move1Info;
	OpcodeInfo move2Info;
	OpcodeInfo cmpInfo;
	CodeBlock* _this = pFunc->mipCode;
	pReport->totalPasses++;
	bCont = JILFalse;
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		if( GetMoveToRegister(_this, opaddr, &move1Info) )
		{
			JILLong opsize2;
			JILLong opaddr2 = opaddr + opsize;
			if( opaddr2 < _this->count )
			{
				opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
				if( GetMoveToRegister(_this, opaddr2, &move2Info) )
				{
					JILLong opsize3;
					JILLong opaddr3 = opaddr2 + opsize2;
					if( opaddr3 < _this->count )
					{
						opsize3 = JILGetInstructionSize( _this->array[opaddr3] );
						if( GetCompareRegister(_this, opaddr3, &cmpInfo) )
						{
							JILLong buffer[16];
							JILLong newSize = 0;
							if( CreateCombinedCompare(_this, &move1Info, &move2Info, &cmpInfo, buffer, &newSize) )
							{
								ReplaceCode(_this, opaddr, opsize + opsize2 + opsize3, newSize);
								memcpy(_this->array + opaddr, buffer, newSize * sizeof(JILLong));
								opsize = newSize;
								pReport->instr_added += 2;
								pReport->instr_removed += 3;
								bCont = JILTrue;
							}
						}
					}
				}
			}
		}
	}
	if( bCont )
		pReport->numPasses++;
	pReport->count_after = _this->count;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeRegisterSaving
//------------------------------------------------------------------------------
// Checks if there are registers saved at the beginning of a function code,
// and restored at the end of a function code, which are no longer in use.
// After calling OptimizeRegisterUsage() there might be code left like this:
//		push	r3
//		copyh	72, 0(r0)
//		pop		r3
//		ret
// This function tries to remove the push and pop instructions that have become
// obsolete.

static JILError OptimizeRegisterSaving(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr, opaddr2;
	JILLong opsize, opsize2;
	JILLong regNum, newReg;
	JILLong numMap;
	JILLong regMap[kNumRegisters];
	JILLong fixupCount = 0;
	JILLong i, cnt;
	CodeBlock* _this = pFunc->mipCode;

	pReport->totalPasses++;
	numMap = IsPushMulti(_this, 0, regMap);
	if( numMap )
	{
		// using pushr (multiple push instruction)
		opsize = JILGetInstructionSize( _this->array[0] );
		opaddr = 0;
		for( i = numMap - 1; i >= 0; i-- )
		{
			regNum = regMap[i];
			if( !IsRegisterInitialized(_this, opaddr + opsize, regNum) )
			{
				// if register is not the last in list, replace with last in list
				if( numMap && regNum != regMap[numMap - 1] )
				{
					newReg = regNum;
					regNum = regMap[numMap - 1];
					regMap[i] = regNum;
					regMap[numMap - 1] = newReg;
					for( opaddr2 = opaddr; opaddr2 < _this->count; opaddr2 += opsize2 )
					{
						opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
						if( !IsPopMulti(_this, opaddr2, regNum) )
							InstructionReplaceRegister(_this, opaddr2, regNum, newReg);
					}
				}
				// remove register from pushr
				cnt = PushMultiDecrement(_this, opaddr, regNum);
				if( cnt < 0 )
					break;	// should not happen!
				fixupCount--;
				numMap--;
				if( cnt == 0 )
				{
					// we can remove the whole instruction
					DeleteCode(_this, opaddr, opsize);
					pReport->instr_removed++;
					// find and delete all popr instructions
					for( opaddr2 = 0; opaddr2 < _this->count; opaddr2 += opsize2 )
					{
						opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
						if( IsPopMulti(_this, opaddr2, regNum) )
						{
							DeleteCode(_this, opaddr2, opsize2);
							pReport->instr_removed++;
							opsize2 = 0;
						}
					}
					// we are done
					break;
				}
				else
				{
					// find and decrement all popr instructions
					for( opaddr2 = 0; opaddr2 < _this->count; opaddr2 += opsize2 )
					{
						opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
						if( IsPopMulti(_this, opaddr2, regNum) )
						{
							PushMultiDecrement(_this, opaddr2, regNum);
						}
					}
				}
				// If there was only 1 register used in the function, we are now left with something
				// like "pushr r3-r3". Since we are perfectionists, replace that by single push / pop.
				numMap = IsPushMulti(_this, opaddr, regMap);
				if( numMap == 1 )
				{
					OpcodeInfo pushInfo;
					JILLong buffer[8];
					JILLong newSize;

					pushInfo.base_opcode = op_push_r;
					pushInfo.operand[0].type = ot_ear;
					pushInfo.operand[0].data[0] = regMap[0];
					if( CreateInstruction(&pushInfo, buffer, &newSize) )
					{
						ReplaceCode(_this, opaddr, opsize, newSize);
						memcpy(_this->array + opaddr, buffer, newSize * sizeof(JILLong));
						pReport->instr_removed++;
						pReport->instr_added++;
						// find and replace all popr instructions
						opaddr += newSize;
						pushInfo.base_opcode = op_pop_r;
						pushInfo.operand[0].type = ot_ear;
						pushInfo.operand[0].data[0] = regMap[0];
						if( CreateInstruction(&pushInfo, buffer, &newSize) )
						{
							for( opaddr2 = opaddr; opaddr2 < _this->count; opaddr2 += opsize2 )
							{
								opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
								if( IsPopMulti(_this, opaddr2, regMap[0]) )
								{
									ReplaceCode(_this, opaddr2, opsize2, newSize);
									memcpy(_this->array + opaddr2, buffer, newSize * sizeof(JILLong));
									pReport->instr_removed++;
									pReport->instr_added++;
									opsize2 = newSize;
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// using single pushs
		for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
		{
			opsize = JILGetInstructionSize( _this->array[opaddr] );
			if( !IsPushRegister(_this, opaddr, &regNum) || regNum == 0 )
				break;
			if( !IsRegisterInitialized(_this, opaddr + opsize, regNum) )
			{
				JILLong opaddr2;
				JILLong opsize2;
				// delete this push instruction
				DeleteCode(_this, opaddr, opsize);
				pReport->instr_removed++;
				fixupCount--;
				// find and delete all pop instructions with this register
				for( opaddr2 = 0; opaddr2 < _this->count; opaddr2 += opsize2 )
				{
					opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
					if( IsPopRegister(_this, opaddr2, regNum) )
					{
						DeleteCode(_this, opaddr2, opsize2);
						pReport->instr_removed++;
						opsize2 = 0;
					}
				}
				// scan this addr again!
				opsize = 0;
			}
		}
	}
	// now that we have eliminated pushes, we must correct all accesses to stack arguments!
	if( fixupCount )
	{
		pReport->numPasses++;
		FixStackOffsetsInBranch(_this, 0, _this->count, fixupCount, 0, NULL);
	}
	return err;
}

//------------------------------------------------------------------------------
// OptimizeTempRegCopying
//------------------------------------------------------------------------------
// The compiler will try to move values into temporary registers as often as
// possible, because moving by reference is faster than copying.
// However, if the compiler later detects that the value in the temporary
// register must be modified, it will create a "real" copy from the reference.
// Thus, you will find code like this:
//		move	[any source], r4
//		move	[any source], r5
//		(probably more instructions here)
//		copy	r4, r4
//		addl	r5, r4
// This optimization routine tries to simply the code like this:
//		copy	[any source], r4
//		move	[any source], r5
//		(probably more instructions here)
//		addl	r5, r4
// TODO:
// For now, this algorithm seems to be "safe" enough to ensure no unwanted code
// changes happen. Especially the instruction "copy r, r" where source and
// destination registers are the same is very unique output from the compiler
// and sufficient to mark "temp register copying". However, to make this algorithm
// more safe, one could check for un/conditional branches occurring between the
// move instruction and the copy instruction.

static JILError OptimizeTempRegCopying(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILLong regNum;
	JILBool bSuccess = JILFalse;
	OpcodeInfo mtrInfo;
	CodeBlock* _this = pFunc->mipCode;

	pReport->totalPasses++;
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		if( GetMoveToRegister(_this, opaddr, &mtrInfo) )
		{
			JILLong opaddr2;
			JILLong opsize2;
			regNum = mtrInfo.operand[dst].data[0];
			// search for "copy [regNum], [regNum]"
			for( opaddr2 = opaddr + opsize; opaddr2 < _this->count; opaddr2 += opsize2 )
			{
				opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
				if( InstructionUsesRegister(_this, opaddr2, regNum) )
				{
					// if stopped at "copy r, r" optimize, otherwise give up!
					if( _this->array[opaddr2] == op_copy_rr &&
						_this->array[opaddr2 + 1] == regNum &&
						_this->array[opaddr2 + 2] == regNum )
					{
						// remove the copy instruction
						DeleteCode(_this, opaddr2, opsize2);
						pReport->instr_removed++;
						// turn the move instruction into a copy instruction
						_this->array[opaddr] = MoveToCopyInstr(_this->array[opaddr]);
						bSuccess = JILTrue;
					}
					break;
				}
			}
		}
	}
	if( bSuccess )
		pReport->numPasses++;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeCombinePushPop
//------------------------------------------------------------------------------
// Combines a consecutive sequence of push or pop instructions to a single
// instruction.

static JILError OptimizeCombinePushPop(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILLong count = 0;
	JILBool bSuccess = JILFalse;
	CodeBlock* _this = pFunc->mipCode;

	pReport->totalPasses++;
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		if( IsPushNullHandle(_this, opaddr, &count) )
		{
			JILLong opaddr2;
			JILLong opsize2;
			JILLong count2;
			JILLong countSum = count;
			JILLong countIns = 1;
			// count all consecutive pushes
			for( opaddr2 = opaddr + opsize; opaddr2 < _this->count; opaddr2 += opsize2 )
			{
				opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
				if( IsPushNullHandle(_this, opaddr2, &count2) &&
					!IsAddrBranchTarget(_this, opaddr2) )
				{
					countSum += count2;
					countIns++;
				}
				else
				{
					// something to combine?
					if( countSum > count )
					{
						ReplaceCode(_this, opaddr, opaddr2 - opaddr, 2);
						_this->array[opaddr] = op_pushm;
						_this->array[opaddr + 1] = countSum;
						opsize = 2;
						pReport->instr_added++;
						pReport->instr_removed += countIns;
						bSuccess = JILTrue;
					}
					break;
				}
			}
		}
		else if( IsPopAndForget(_this, opaddr, &count) )
		{
			JILLong opaddr2;
			JILLong opsize2;
			JILLong count2;
			JILLong countSum = count;
			JILLong countIns = 1;
			// count all consecutive pops
			for( opaddr2 = opaddr + opsize; opaddr2 < _this->count; opaddr2 += opsize2 )
			{
				opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
				if( IsPopAndForget(_this, opaddr2, &count2) &&
					!IsAddrBranchTarget(_this, opaddr2) )
				{
					countSum += count2;
					countIns++;
				}
				else
				{
					// something to combine?
					if( countSum > count )
					{
						ReplaceCode(_this, opaddr, opaddr2 - opaddr, 2);
						_this->array[opaddr] = op_popm;
						_this->array[opaddr + 1] = countSum;
						opsize = 2;
						pReport->instr_added++;
						pReport->instr_removed += countIns;
						bSuccess = JILTrue;
					}
					break;
				}
			}
		}
	}
	if( bSuccess )
		pReport->numPasses++;
	return err;
}

//------------------------------------------------------------------------------
// OptimizeRegisterReplacing
//------------------------------------------------------------------------------
// This function checks whether a register beeing used in the code can be
// replaced by a register used earlier in the code. The function assumes, if the
// register used earlier is no longer referenced in the whole code, then it can
// be reused instead of the new register.
// Ideally this algorithm should be executed *before* OptimizeRegisterSaving().

static JILError OptimizeRegisterReplacing(JCLFunc* pFunc, OptimizeReport* pReport)
{
	JILError err = JCL_No_Error;
	JILLong opaddr;
	JILLong opsize;
	JILLong currentRegister;
	JILLong newRegister;
	JILBool bSuccess;
	JILLong regmap[kNumRegisters];
	JILBool initial[kNumRegisters];
	JILLong nummap;
	JILLong regNum;
	JILLong i;
	CodeBlock* _this = pFunc->mipCode;

	bSuccess = JILFalse;
	pReport->totalPasses++;
	for( i = 0; i < kNumRegisters; i++ )
		initial[i] = JILTrue;

	// first, create a map of used registers from push instructions at function start
	nummap = IsPushMulti(_this, 0, regmap);
	if( nummap )
	{
		opaddr = 0;
		opsize = JILGetInstructionSize( _this->array[0] );
	}
	else
	{
		for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
		{
			opsize = JILGetInstructionSize( _this->array[opaddr] );
			if( IsPushRegister(_this, opaddr, &regNum) )
				regmap[nummap++] = regNum;
			else
				break;
		}
	}

	// do main job
	for( ; opaddr < _this->count; opaddr += opsize )
	{
		opsize = JILGetInstructionSize( _this->array[opaddr] );
		// does instruction init a register?
		if( GetInstructionInitRegister(_this, opaddr, &newRegister) )
		{
			if( newRegister >= 0 && newRegister < 3 )
			{
				// ignore r0 - r2!
			}
			else if( initial[newRegister] )
			{
				initial[newRegister] = JILFalse;
				// check if the register can be replaced by one that is no longer used
				for( i = 0; i < nummap; i++ )
				{
					currentRegister = regmap[i];
					if( currentRegister != newRegister &&
						!initial[currentRegister] )
					{
						// scan from here on whether currentRegister is still referenced
						JILBool stillUsed = JILFalse;
						{
							JILLong opaddr2;
							JILLong opsize2;
							for( opaddr2 = opaddr; opaddr2 < _this->count; opaddr2 += opsize2 )
							{
								opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
								if( InstructionUsesRegister(_this, opaddr2, currentRegister) &&
									!IsPopRegister(_this, opaddr2, currentRegister) )
								{
									stillUsed = JILTrue;
									break;
								}
							}
						}
						// if current no longer used, replace new by current
						if( !stillUsed )
						{
							JILLong opaddr2;
							JILLong opsize2;
							for( opaddr2 = opaddr; opaddr2 < _this->count; opaddr2 += opsize2 )
							{
								opsize2 = JILGetInstructionSize( _this->array[opaddr2] );
								if( !IsPopRegister(_this, opaddr2, newRegister) )
									InstructionReplaceRegister(_this, opaddr2, newRegister, currentRegister);
							}
							bSuccess = JILTrue;
							initial[newRegister] = JILTrue;
							break;
						}
					}
				}
			}
		}
	}
	if( bSuccess )
		pReport->numPasses++;
	return err;
}

//------------------------------------------------------------------------------
// DebugListFunction
//------------------------------------------------------------------------------
// This allows us to get a clear-text listing of the code currently stored in
// the function object's mipCode array for debugging optimization functions.
// Since this overwrites the code-segment of the virtual machine, you should NOT
// call this after you have linked your program with JCLLink().
// Consider this a "hack" for debug purposes only.

static JILError DebugListFunction(JCLFunc* _this, JCLState* pCompiler)
{
	JILError err = JCL_No_Error;
	JILLong* pSaveBuffer = NULL;
	JILLong length, saveLen;
	JILState* pMachine = pCompiler->mipMachine;

	JILMessageLog(pMachine, "\n----- Debug printing function %s -----\n", JCLGetString(_this->mipName));
	length = _this->mipCode->count;
	saveLen = JILGetCodeLength(pMachine);
	saveLen = (length < saveLen) ? length : saveLen;
	pSaveBuffer = (JILLong*) malloc( sizeof(JILLong) * saveLen );
	err = JILGetMemory(pMachine, 0, pSaveBuffer, saveLen);
	if( err ) 
		goto cleanup;
	err = JILSetMemory(pMachine, 0, _this->mipCode->array, length);
	if( err )
		goto restore;
	JILListCode(pMachine, 0, length, 1);

restore:
	err = JILSetMemory(pMachine, 0, pSaveBuffer, saveLen);
	if( err )
		goto cleanup;

cleanup:
	free( pSaveBuffer );
	JILMessageLog(pMachine, "----------------------------------------\n");
	return err;
}

//------------------------------------------------------------------------------
// JCLFunc::CreateLiterals
//------------------------------------------------------------------------------

static JILError createLiterals_JCLFunc(JCLFunc* _this, JCLState* pCompiler)
{
	JILError err = JCL_No_Error;
	Array_JCLLiteral* pLiterals;
	JCLLiteral* pLit;
	Array_JILLong* pCode;
	JILLong hObj;
	JILLong j;

	pCode = _this->mipCode;
	pLiterals = _this->mipLiterals;
	for( j = 0; j < pLiterals->Count(pLiterals); j++ )
	{
		pLit = pLiterals->Get(pLiterals, j);
		hObj = pLit->miHandle;
		if( !hObj )
		{
			switch( pLit->miType )
			{
				case type_int:
					err = JILCreateLong(pCompiler->mipMachine, pLit->miLong, &hObj);
					break;
				case type_float:
					err = JILCreateFloat(pCompiler->mipMachine, pLit->miFloat, &hObj);
					break;
				case type_string:
					err = JILCreateString(pCompiler->mipMachine, JCLGetString(pLit->miString), &hObj);
					break;
			}
			if( err )
				break;
			pCode->Set(pCode, pLit->miOffset, hObj);
			pLit->miHandle = hObj;
		}
		else
		{
			if( TypeFamily(pCompiler, pLit->miType) == tf_delegate )
			{
				pCode->Set(pCode, pLit->miOffset, pLit->miHandle);
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// JCLFunc::OptimizeCode
//------------------------------------------------------------------------------

static JILError optimizeCode_JCLFunc(JCLFunc* _this, JCLState* pCompiler)
{
	JILError err = JCL_No_Error;
	OptimizeReport report = {0};
	JILLong optLevel = _this->miOptLevel;
	JCLString* pFuncName = NEW(JCLString);

	pCompiler->miOptSizeBefore += _this->mipCode->count * sizeof(JILLong);
	if( optLevel && _this->mipCode->count )
	{
		_this->ToString(_this, pCompiler, pFuncName, kFullDecl | kCompact);
		JCLVerbosePrint(pCompiler, "Optimizing %s ...\n", JCLGetString(pFuncName));
		report.count_before = _this->mipCode->count;

		// optimize consecutive pushes and pops
		err = OptimizeCombinePushPop(_this, &report);
		if( err )
			goto exit;

		// optimize move operations
		err = OptimizeMoveOperations(_this, &report);
		if( err )
			goto exit;

		if( optLevel > 1 )
		{
			// optimize temp register copying
			err = OptimizeTempRegCopying(_this, &report);
			if( err )
				goto exit;

			// optimize arithmetical operations (MUST follow temp reg copying!)
			err = OptimizeMathOperations(_this, &report);
			if( err )
				goto exit;

			// optimize compare operations
			err = OptimizeCompareOperations(_this, &report);
			if( err )
				goto exit;

			// optimize move following add/mul/and/or etc
			err = OptimizeOperationAndMove(_this, &report);
			if( err )
				goto exit;

			if( optLevel > 2 )
			{
				// optimize register replacing
				err = OptimizeRegisterReplacing(_this, &report);
				if( err )
					goto exit;

				// optimize register saving code
				err = OptimizeRegisterSaving(_this, &report);
				if( err )
					goto exit;
			}
		}

		report.count_after = _this->mipCode->count;
		if( (report.instr_removed - report.instr_added) ||
			(report.count_before != report.count_after) )
		{
			JCLVerbosePrint(pCompiler,
				"Saved %d instructions in %d of %d passes.\nCode size reduced from %d to %d bytes.\n",
				report.instr_removed - report.instr_added,
				report.numPasses,
				report.totalPasses,
				report.count_before * sizeof(JILLong),
				report.count_after * sizeof(JILLong));
			pCompiler->miOptSavedInstr += report.instr_removed - report.instr_added;
			pCompiler->miOptSizeAfter += report.count_after * sizeof(JILLong);
		}
	}

exit:
	DELETE( pFuncName );
	return err;
}

//------------------------------------------------------------------------------
// JCLFunc::SearchFunction
//------------------------------------------------------------------------------

static JCLFunc* SearchFunction(JCLClass* pClass, JILLong srcType, JILLong srcFuncIdx)
{
	// Search a function in the given class
	JCLFunc* pFunk;
	JILLong i;
	JILLong n = pClass->mipFuncs->Count(pClass->mipFuncs);
	for( i = 0; i < n; i++ )
	{
		pFunk = pClass->mipFuncs->Get(pClass->mipFuncs, i);
		if( pFunk->miLnkClass == srcType && pFunk->miLnkRelIdx == srcFuncIdx )
			break;
		pFunk = NULL;
	}
	return pFunk;
}

//------------------------------------------------------------------------------
// JCLFunc::RelocateFunction
//------------------------------------------------------------------------------

static JILError RelocateFunction(JCLFunc* pDstFunc, JCLFunc* pSrcFunc, JCLState* pCompiler)
{
	JILLong opaddr, opsize, opcode, i;
	JILLong srcType = pSrcFunc->miClassID;
	JILLong dstType = pDstFunc->miClassID;
	JILLong varOffset = pDstFunc->miLnkVarOffset;
	JILLong dstFuncIdx = pDstFunc->miFuncIdx;
	OpcodeInfo info;
	CodeBlock* _this = pDstFunc->mipCode;
	JILState* pState = pCompiler->mipMachine;
	JCLClass* pClass = GetClass(pCompiler, dstType);
	const JILInstrInfo* pInfo;
	JILFuncInfo* pfi;
	JCLFunc* pFunc;
	JILBool bUpdate;

	// copy entire code from source function
	pDstFunc->mipCode->Copy(pDstFunc->mipCode, pSrcFunc->mipCode);

	// go through code and relocate variable offsets, function indexes and type IDs
	for( opaddr = 0; opaddr < _this->count; opaddr += opsize )
	{
		opcode = _this->array[opaddr];
		opsize = JILGetInstructionSize(opcode);
		if( IsMovingThisRef(_this, opaddr, &info) )
		{
			JCLString* pName = NEW(JCLString);
			pDstFunc->ToString(pDstFunc, pCompiler, pName, kCompact);
			EmitWarning(pCompiler, JCL_WARN_Unsafe_This_Operation, 1, pName);
			DELETE(pName);
		}
		if( GetOpcodeInfo(_this, opaddr, &info) )
		{
			bUpdate = JILFalse;
			if( opcode == op_callm )
			{
				if( info.operand[0].data[0] == srcType )
				{
					pFunc = SearchFunction(pClass, srcType, info.operand[1].data[0]);
					if( pFunc == NULL )
						return JIL_ERR_Generic_Error;
					info.operand[0].data[0] = dstType;
					info.operand[1].data[0] = pFunc->miFuncIdx;
					bUpdate = JILTrue;
				}
			}
			else if( opcode == op_calls )
			{
				pfi = JILGetFunctionInfo(pState, info.operand[0].data[0]);
				if( pfi->type == srcType )
				{
					pFunc = SearchFunction(pClass, srcType, pfi->memberIdx);
					if( pFunc == NULL )
						return JIL_ERR_Generic_Error;
					info.operand[0].data[0] = pFunc->miHandle;
					bUpdate = JILTrue;
				}
			}
			else if( opcode == op_newdgm )
			{
				if( info.operand[2].data[0] == 0 ) // R0?
				{
					pFunc = SearchFunction(pClass, srcType, info.operand[1].data[0]);
					if( pFunc == NULL )
						return JIL_ERR_Generic_Error;
					info.operand[1].data[0] = pFunc->miFuncIdx;
					bUpdate = JILTrue;
				}
			}
			else if( opcode == op_newdg )
			{
				pfi = JILGetFunctionInfo(pState, info.operand[1].data[0]);
				if( pfi->type == srcType )
				{
					pFunc = SearchFunction(pClass, srcType, pfi->memberIdx);
					if( pFunc == NULL )
						return JIL_ERR_Generic_Error;
					info.operand[1].data[0] = pFunc->miHandle;
					bUpdate = JILTrue;
				}
			}
			else
			{
				pInfo = JILGetInfoFromOpcode(opcode);
				for( i = 0; i < pInfo->numOperands; i++ )
				{
					if( info.operand[i].type == ot_type )
					{
						if( info.operand[i].data[0] == srcType )
						{
							// replace type
							info.operand[i].data[0] = dstType;
							bUpdate = JILTrue;
						}
					}
					else if( info.operand[i].type == ot_ead )
					{
						if( info.operand[i].data[0] == 0 ) // R0?
						{
							// relocate member variable access
							info.operand[i].data[1] += varOffset;
							bUpdate = JILTrue;
						}
					}
				}
			}
			if( bUpdate )
			{
				JILLong buf[8];
				JILLong bsize = 0;
				if( CreateInstruction(&info, buf, &bsize) )
				{
					if( bsize != opsize )
						return JIL_ERR_Generic_Error;
					memcpy(_this->array + opaddr, buf, bsize * sizeof(JILLong));
				}
			}
		}
	}
	pDstFunc->miLinked = JILTrue;
	return 0;
}
