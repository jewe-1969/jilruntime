//------------------------------------------------------------------------------
// File: JCLState.c                                            (c) 2004 jewe.org
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
//	The main object for the compiler. This is created when calling
//	JCLInitialize() and must be passed into all public API functions.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"
#include "jclfunc.h"
#include "jclclass.h"
#include "jclclause.h"
#include "jclstate.h"

#include "jiltools.h"
#include "jilopcodes.h"
#include "jilprogramming.h"
#include "jiltypelist.h"
#include "jilcallntl.h"

/*
--------------------------------------------------------------------------------
COMPILER TODO:
--------------------------------------------------------------------------------
	* Ensure that the SimStack is cleaned up properly during a compile-time error. All vars must be taken from stack!
	* Remove all the "special checks" for type_string and type_array and let them be handled generically by the type-info!

	Missing features:
	* enum
	* ternary operator <exp1> ? <exp2> : <exp3>

	Feature ideas:
	* Add macro JIL_ENABLE_COMPILER to allow building a compiler-free library
	* Indexer function: object[index] is compiled as call to method <type> indexer (int i);
	* Compiler option "defaultfloat": (interpret all numeric literals as float by default, useful for calculators)
	* Lambda expressions: (arg1, arg2) => <expr> (compiles <expr> into a function with a single return statement)
	* Native structures: struct MyStruct { int a; float b; string c; } (object that is binary compatible to a C-Struct and can be manipulated
		by script. self-contained memory-block, no pointers or references allowed, even when nesting structs. Limited to int, float, string,
		other structs and static arrays of those types.)
	* Generic classes ( class<T> )
	* Inheritance from multiple interfaces
	* try / catch
*/

//------------------------------------------------------------------------------
// FATALERROR, FATALERROREXIT
//------------------------------------------------------------------------------

#define	FATALERROR(FN,TEXT,S)		do {FatalError(_this,__FILE__,__LINE__,TEXT,FN); S;} while(0)
#define	FATALERROREXIT(FN,TEXT)		FATALERROR(FN, TEXT, err = JCL_ERR_Fatal_Error; goto exit)

//------------------------------------------------------------------------------
// Error handling macros
//------------------------------------------------------------------------------

#define ERROR_IF(COND,ERR,ARG,S)	do {if(COND){err = EmitError(_this,ARG,ERR); S;}} while(0)
#define ERROR(ERR,ARG,S)			do {err = EmitError(_this,ARG,ERR); S;} while(0)

//------------------------------------------------------------------------------
// Register allocation constants
//------------------------------------------------------------------------------
// Number of registers to reserve for local variables. Don't reserve too many
// registers for local variables, leave AT LEAST 16 registers free for temporary
// values. CAUTION: The first 3 regs (r0,r1,r2) MUST REMAIN reserved!
// Using a higher number of registers for local variables will only give a minor
// improvement in speed. If compiler option "stack-locals" is enabled, all
// local variables will be created on the stack and this constant will be
// disregarded.

static const JILLong kMaxVarRegisters	= 5;	// registers used for local variables
static const JILLong kFirstVarRegister	= 3;	// first three regs are reserved!

// This defines how many registers will be saved to the stack with single pushes
// rather than using the PUSHR / POPR instruction. If more registers have to be
// saved than this number, PUSHR and POPR will be used.
// Starting with library version 0.7.1.66, using PUSHR / POPR is reasonably faster
// than using single pushes, so this value should be kept low.
// Extensive performance tests on my P4 / 2.4 GHz proved the below settings to
// be the optimum.

static const JILLong kPushRegisterThreshold = 1;
static const JILLong kPushMultiThreshold = 1;

//------------------------------------------------------------------------------
// Global constants
//------------------------------------------------------------------------------

static const JILLong kSimStackSize = 1024;		// number of locations on simulated stack
static const JILLong kFileBufferSize = 1024;	// used by p_import()

const JILChar* kNameGlobalNameSpace = "__global";
const JILChar* kNameGlobalInitFunction = "__init";
const JILChar* kNameAnonymousFunction = "__anonymous_function_%x";

//------------------------------------------------------------------------------
// Debug variables
//------------------------------------------------------------------------------

JILLong g_NewCalls;
JILLong g_DeleteCalls;

//------------------------------------------------------------------------------
// private declarations
//------------------------------------------------------------------------------

// helper struct for SetMarker() / RestoreMarker()
typedef struct SMarker
{
	JCLFunc* mipFunc;
	JILLong	miCodePos;
	JILLong	miLiteralPos;
	JILLong miStackPos;
	JILLong miErrorPos;
	JILLong miNumErr;
	JILLong miNumWarn;
} SMarker;

typedef struct SInitState
{
	JILLong	miType;			// saves members of which class
	JILBool miRetFlag;		// return flag
	JILBool* mipInited;		// one flag per class member
	JCLState* mipCompiler;	// compiler state
} SInitState;

// enum for cg_move_xx
enum { op_move, op_copy, op_wref };

// flags for p_member_call
enum { kOnlyCtor = 1 << 0 };

// flags for p_expression
enum { kExpressionProbeMode = 1 << 0 };

// enum for p_function(), p_function_literal() and FindFuncRef()
enum
{
	kFunction	= 1 << 0,
	kMethod		= 1 << 1,
	kAccessor	= 1 << 2,
	kCofunction	= 1 << 3,
	kExplicit	= 1 << 4,
	kStrict		= 1 << 5
};

// enum for IsIdentifierUsed()
enum
{
	kGlobalVar = 0,
	kGlobalFunc,
	kGlobalClass,
	kGlobalCofunc,
	kGlobalDelegate,
	kGlobalAlias,
	kClassVar,
	kClassVarDelegate,
	kClassFunc,
	kClassMethod,
	kClassAccessor,
	kClassCtor,
	kFuncLocalVar,
	kMethodLocalVar,
};

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILEXTERN const JCLErrorInfo	JCLErrorStrings			[JCL_Num_Compiler_Codes];

JILEXTERN JILLong				JILGetInstructionSize	(JILLong opcode);
JILEXTERN JILError				JILHandleRuntimeOptions	(JILState*, const JILChar*, const JILChar*);
JILEXTERN JILLong				GetFuncInfoFlags		(JCLFunc* func);

//------------------------------------------------------------------------------
// various helper functions
//------------------------------------------------------------------------------

static JILError		FindPrototype		(JCLState*, const JCLFunc*, JCLFunc**);
static JILLong		FindBestPrototype	(JCLState*, JILLong, const JCLFunc*, JCLFunc**);
static JILError		FindConvertor		(JCLState*, JCLVar*, JCLVar*, JCLFunc**);
static JILError		FindConstructor		(JCLState*, JCLVar*, JCLVar*, JCLFunc**);
static JILError		FindDefaultCtor		(JCLState*, JCLVar*, JCLFunc**);
static JILError		FindCopyCtor		(JCLState*, JCLVar*, JCLFunc**);
static JILLong		FindAccessor		(JCLState*, JILLong, const JCLString*, JILLong, JCLFunc**);
static JILBool		FindSetAccessor		(JCLState*, JILLong, const JCLString*, JCLVar*, JCLFunc**);
static JILBool		FindGetAccessor		(JCLState*, JILLong, const JCLString*, JCLVar*, JCLFunc**);
static void			SimStackFixup		(JCLState*, JILLong);
static void			SimStackPush		(JCLState*, JCLVar*, JILBool);
static JILLong		SimStackReserve		(JCLState*, JILLong);
static void			SimStackPop			(JCLState*, JILLong);
static void			SimStackUnroll		(JCLState*, JILLong);
static JCLVar*		SimStackGet			(JCLState*, JILLong);
static void			SimRegisterSet		(JCLState*, JILLong, JCLVar*);
static JCLVar*		SimRegisterGet		(JCLState*, JILLong);
static JILError		MakeLocalVar		(JCLState*, Array_JCLVar*, JILLong, const JCLVar*);
static void			FreeLocalVars		(JCLState*, Array_JCLVar*);
static JCLVar*		FindLocalVar		(JCLState*, const JCLString*);
static JCLVar*		FindFuncArg			(JCLState*, const JCLString*);
static JILError		MakeTempVar			(JCLState*, JCLVar**, const JCLVar*);
static JILError		MakeTempArrayVar	(JCLState*, JCLVar**, JCLVar*);
static void			FreeTempVar			(JCLState*, JCLVar**);
static void			SetMarker			(JCLState*, SMarker*);
static void			RestoreMarker		(JCLState*, SMarker*);
static JCLClass*	CurrentClass		(JCLState*);
static JCLClass*	CurrentOutClass		(JCLState*);
static JCLFunc*		CurrentFunc			(JCLState*);
static JCLFunc*		CurrentOutFunc		(JCLState*);
static JILBool		IsFuncInGlobalScope	(JCLState*, const JCLString*);
static JILError		IsIdentifierUsed	(JCLState*, JILLong, JILLong, const JCLString*);
static JILError		IsAccessorUsed		(JCLState*, JILLong, JCLFunc*);
static JCLVar*		FindGlobalVar		(JCLState*, JILLong, const JCLString*);
static JCLVar*		FindMemberVar		(JCLState*, JILLong, const JCLString*);
static JCLVar*		FindAnyVar			(JCLState*, const JCLString*);
static JILError		FindFuncRef			(JCLState*, JCLString*, JILLong, JILLong, JCLVar*, JCLFunc**);
static JILError		FindAnyFuncRef		(JCLState*, JCLString*, JCLVar*, JCLFunc**);
static JILBool		IsTempVar			(const JCLVar*);
static JILBool		IsResultVar			(const JCLVar*);
static JILBool		IsArrayAccess		(const JCLVar*);
static void			InitMemberVars		(JCLState*, JILLong, JILBool);
static void			BreakBranchFixup	(JCLState*, Array_JILLong*, JILLong);
static JILBool		IsAssignOperator	(JILLong);
static JILBool		IsSrcInited			(const JCLVar*);
static JILBool		IsDstInited			(const JCLVar*);
static JILBool		IsDstConst			(const JCLVar*);
static JILBool		IsSrcConst			(const JCLVar*);
static JILBool		IsDstTakingRef		(const JCLVar*);
static JILBool		IsRef				(const JCLVar*);
static JILBool		IsWeakRef			(const JCLVar*);
static JILBool		IsRegisterAccess	(const JCLVar*, JILLong);
static JILBool		IsBasicType			(JILLong);
static JILBool		IsComparableType	(JILLong);
static JILBool		IsCalculatableType	(JILLong);
static void			DuplicateVar		(JCLVar**, const JCLVar*);
static void			FreeDuplicate		(JCLVar**);
static JILBool		IsOperatorToken		(JILLong);
static JILBool		ClassHasBody		(JCLState*, JILLong);
static JILError		AddGlobalVar		(JCLState*, JCLVar*);
static JILBool		EqualTypes			(const JCLVar*, const JCLVar*);
static JILBool		EqualRegisters		(const JCLVar*, const JCLVar*);
static JILBool		ImpConvertible		(JCLState*, JCLVar*, JCLVar*);
static JILBool		DynConvertible		(JCLState*, JCLVar*, JCLVar*);
static JILBool		AllMembersInited	(JCLState*, JILLong, JCLString*);
static JCLFile*		PushImport			(JCLState*, const JCLString*, const JCLString*, const JCLString*, JILBool);
static void			PopImport			(JCLState*);
static JCLFile*		FindImport			(JCLState*, const JCLString*);
static JILBool		IsArithmeticAssign	(JILLong);
static JILError		CheckTypeConflict	(const JCLVar*, const JCLVar*);
static JILBool		IsVarClassType		(JCLState*, const JCLVar*);
static JILBool		IsClassType			(JCLState*, JILLong);
static JILBool		IsInterfaceType		(JCLState*, JILLong);
static JILBool		IsValueType			(JCLState*, JILLong);
static JILBool		IsTypeCopyable		(JCLState*, JILLong);
static JILBool		IsGlobalScope		(JCLState*, JILLong);
static JILBool		IsClassToken		(JILLong token);
static JILBool		IsTypeName			(JCLState*, JILLong, const JCLString*, TypeInfo*);
static JILBool		IsSuperClass		(JCLState*, JILLong, JILLong);
static JILBool		IsSubClass			(JCLState*, JILLong, JILLong);
static JILBool		IsModifierNativeBinding(JCLClass*);
static JILBool		IsModifierNativeInterface(JCLClass*);
static JILBool		IsClassNative		(JCLClass*);
static void			PushOptions			(JCLState*);
static void			PopOptions			(JCLState*);
static JILLong		StringToType		(JCLState*, const JCLString*, JILLong);
static JILError		IsFullTypeDecl		(JCLState*, JCLString*, JCLVar*, JILBool);
static JILError		CreateCofunction	(JCLState*, JCLVar*, Array_JCLVar*, JILLong*);
static JILError		CreateDelegate		(JCLState*, JCLVar*, Array_JCLVar*, JILLong*);
static JILError		AddAlias			(JCLState*, JCLString*, JILLong);
static JILError		GetSignature		(JCLState*, const JILChar*, JCLVar*, Array_JCLVar*, JCLString*);
static void			CreateInitState		(SInitState*, JCLState*);
static void			SaveInitState		(SInitState*);
static void			RestoreInitState	(const SInitState*);
static void			AndInitState		(const SInitState*);
static void			SetInitState		(SInitState*, JILBool);
static void			FreeInitState		(SInitState*);

//------------------------------------------------------------------------------
// code generator functions
//------------------------------------------------------------------------------

static void			cg_opcode			(JCLState*, JILLong);
static void			cg_push_multi		(JCLState*, JILLong);
static void			cg_pop_multi		(JCLState*, JILLong);
static void			cg_return			(JCLState*);
static JILError		cg_get_literal		(JCLState*, JILLong, const JCLString*, JCLVar*, JCLVar**, JCLVar**, JILBool);
static JILError		cg_load_literal		(JCLState*, JILLong, const JILChar*, JCLVar*, JILBool, TypeInfo*);
static JILError		cg_load_null		(JCLState*, JCLVar*, TypeInfo*);
static JILError		cg_load_func_literal(JCLState*, JILLong, JCLVar*, JCLVar**, JCLVar**, JCLVar*);
static void			cg_call_static		(JCLState*, JILLong);
static void			cg_push_registers	(JCLState*, JILLong*, JILLong);
static void			cg_pop_registers	(JCLState*, JILLong*, JILLong);
static JILError		cg_move_var			(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_math_var			(JCLState*, JCLVar*, JCLVar*, JILLong);
static JILError		cg_compare_var		(JCLState*, JILLong, JCLVar*, JCLVar*, JCLVar*);
static JILError		cg_not_var			(JCLState*, JCLVar*);
static JILError		cg_modify_temp		(JCLState*, JCLVar*);
static JILError		cg_push_var			(JCLState*, JCLVar*);
static void			cg_call_member		(JCLState*, JILLong, JILLong);
static void			cg_call_factory		(JCLState*, JILLong, JILLong);
static JILError		cg_alloc_var		(JCLState*, JCLVar*, JCLVar*, JILBool);
static JILError		cg_alloci_var		(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_change_context	(JCLState*, JCLVar*);
static void			cg_call_native		(JCLState*, JILLong, JILLong);
static JILError		cg_incdec_var		(JCLState*, JCLVar*, JILBool);
static JILError		cg_neg_var			(JCLState*, JCLVar*);
static JILError		cg_auto_convert		(JCLState*, JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		cg_add_array_rule	(JCLState* _this, JCLVar* src, JCLVar* dst);
static JILError		cg_src_dst_rule		(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_src_src_rule		(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_dst_modify_rule	(JCLState*, JCLVar*);
static JILError		cg_dst_assign_rule	(JCLState*, JCLVar*);
static JILError		cg_copy_modify_rule	(JCLState*, JCLVar*);
static JILError		cg_testnull_var		(JCLState*, JILLong, JCLVar*, JCLVar*);
static void			cg_move_rr			(JCLState*, JILLong, JILLong, JILLong);
static void			cg_move_rd			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_rx			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_rs			(JCLState*, JILLong, JILLong, JILLong);
static void			cg_move_dr			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_dd			(JCLState*, JILLong, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_dx			(JCLState*, JILLong, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_ds			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_xr			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_xd			(JCLState*, JILLong, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_xx			(JCLState*, JILLong, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_xs			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_sr			(JCLState*, JILLong, JILLong, JILLong);
static void			cg_move_sd			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_sx			(JCLState*, JILLong, JILLong, JILLong, JILLong);
static void			cg_move_ss			(JCLState*, JILLong, JILLong, JILLong);
static JILError		cg_moveh_var		(JCLState*, JILLong, JCLVar*);
static JILError		cg_alloca_var		(JCLState*, JILLong, JILLong, JCLVar*);
static JILError		cg_cvf_var			(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_cvl_var			(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_dcvt_var			(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_and_or_xor_var	(JCLState*, JCLVar*, JCLVar*, JILLong);
static JILError		cg_bnot_var			(JCLState*, JCLVar*);
static JILError		cg_rtchk			(JCLState*, JCLVar*, JILLong);
static JILError		cg_cast_if_typeless	(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_newctx			(JCLState*, JCLVar*, JILLong, JILLong, JILLong);
static JILError		cg_resume			(JCLState*, JCLVar*);
static JILError		cg_init_var			(JCLState*, JCLVar*);
static JILError		cg_accessor_call	(JCLState*, JCLClass*, JCLFunc*, JCLVar*, const JCLString*);
static JILError		cg_convert_to_type	(JCLState*, JCLVar*, JILLong);
static JILError		cg_convert_compare	(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_convert_calc		(JCLState*, JCLVar*, JCLVar*);
static JILError		cg_new_delegate		(JCLState*, JILLong, JCLVar*, JCLVar*);
static JILError		cg_call_delegate	(JCLState*, JCLVar*);

//------------------------------------------------------------------------------
// parsing functions
//------------------------------------------------------------------------------

static JILError		p_root				(JCLState*);
static JILError		p_class				(JCLState*, JILLong);
static JILError		p_class_modifier	(JCLState*, JILLong);
static JILError		p_class_inherit		(JCLState*, JCLClass*);
static JILError		p_class_hybrid		(JCLState*, JCLClass*);
static JILError		p_function			(JCLState*, JILLong, JILBool);
static JILError		p_function_body		(JCLState*);
static JILError		p_function_pass		(JCLState*);
static JILError		p_sub_functions		(JCLState*);
static JILError		p_function_hybrid	(JCLState*, JCLFunc*);
static JILError		p_block				(JCLState*, JILBool*);
static JILError		p_statement			(JCLState*, Array_JCLVar*, JILBool*);
static JILError		p_local_decl		(JCLState*, Array_JCLVar*, JCLVar*);
static JILError		p_member_decl		(JCLState*, JILLong, JCLVar*);
static JILError		p_return			(JCLState*, Array_JCLVar*);
static JILError		p_throw				(JCLState*, Array_JCLVar*);
static JILError		p_expr_get_variable	(JCLState*, JCLString*, JCLVar*, JCLVar**, JCLVar**, JILBool);
static JILError		p_expr_atomic		(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**, JILLong);
static JILError		p_expr_get_array	(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_expr_get_member	(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_expr_call_variable(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_expr_primary		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_mul_div		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_add_sub		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_log_shift	(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_gt_lt		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_eq_ne		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_band			(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_xor			(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_bor			(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_and			(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expr_or			(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_expression		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_assignment		(JCLState*, Array_JCLVar*, JCLVar*, TypeInfo*);
static JILError		p_function_call		(JCLState*, Array_JCLVar*, const JCLString*, JCLVar*, TypeInfo*);
static JILError		p_member_call		(JCLState*, Array_JCLVar*, JILLong, const JCLString*, JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_match_function	(JCLState*, Array_JCLVar*, JILLong, const JCLString*, JCLVar*, TypeInfo*, JCLFunc*, JCLFunc**);
static JILError		p_if				(JCLState*, Array_JCLVar*);
static JILError		p_import			(JCLState*);
static JILError		p_import_class_list	(JCLState*, const char*);
static JILError		p_import_all		(JCLState*);
static JILError		p_new				(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_for				(JCLState*, Array_JCLVar*);
static JILError		p_while				(JCLState*, Array_JCLVar*);
static JILError		p_break				(JCLState*, JILBool);
static JILError		p_switch			(JCLState*, Array_JCLVar*);
static JILError		p_typeof			(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_sameref			(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_new_array			(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_array_init		(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**);
static JILError		p_new_basic_type	(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**, JILLong);
static JILError		p_new_copy_ctor		(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**, JILBool*);
static JILError		p_do_while			(JCLState*, Array_JCLVar*);
static JILError		p_global_decl		(JCLState*, JCLVar*, JCLString*);
static JILError		p_interface			(JCLState*, JILLong);
static JILError		p_accessor_call		(JCLState*, Array_JCLVar*, JCLFunc*, JCLVar*, JCLVar*, TypeInfo*);
static JILError		p_skip_braces		(JCLState*, JILLong, JILLong);
static JILError		p_skip_statement	(JCLState*);
static JILError		p_skip_block		(JCLState*);
static JILError		p_cast_operator		(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**, const TypeInfo*);
static JILError		p_option			(JCLState*);
static JILError		p_using				(JCLState*);
static JILError		p_delegate			(JCLState*);
static JILError		p_alias				(JCLState*);
static JILError		p_yield				(JCLState*, Array_JCLVar*);
static JILError		p_variable_call		(JCLState*, Array_JCLVar*, const JCLString*, JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_cofunction_resume	(JCLState*, JCLVar*, JCLVar*, JCLVar*, TypeInfo*);
static JILError		p_delegate_call		(JCLState*, Array_JCLVar*, JCLVar*, JCLVar*, JCLVar*, TypeInfo*, JILLong);
static JILError		p_strict			(JCLState*);
static JILError		p_function_literal	(JCLState*, Array_JCLVar*, JCLVar*, JCLVar**, JCLVar**, JILLong, JILLong);
static JILError		p_selftest			(JCLState*, Array_JCLVar*);
static JILError		p_tag				(JCLState*, JCLString*);
static JILError		p_clause			(JCLState*, Array_JCLVar*, JCLClause*);
static JILError		p_goto				(JCLState*, Array_JCLVar*);

//------------------------------------------------------------------------------
// Implement array template for JCLFile and JCLOption
//------------------------------------------------------------------------------

IMPL_ARRAY( JCLOption )
IMPL_ARRAY( JCLFile )

//------------------------------------------------------------------------------
// JCLState
//------------------------------------------------------------------------------
// constructor

void create_JCLState( JCLState* _this )
{
	int i;
	// init all our members
	_this->mipMachine = NULL;
	_this->mipFile = NULL;
	_this->miClass = 0;
	_this->miArgClass = 0;
	_this->miOutputClass = 0;
	_this->miFunc = 0;
	_this->miOutputFunc = 0;
	_this->miPass = 0;
	_this->mipClasses = NEW( Array_JCLClass );
	// alloc stack
	_this->mipStack = (JCLVar**) malloc(kSimStackSize * sizeof(JCLVar*));
	memset(_this->mipStack, 0, kSimStackSize * sizeof(JCLVar*));
	_this->miStackPos = kSimStackSize;
	// init register maps
	for( i = 0; i < kNumRegisters; i++ )
	{
		_this->mipRegs[i] = NULL;
		_this->miRegUsage[i] = 0;
	}
	_this->miNumRegsToSave = 0;
	_this->miNumVarRegisters = 0;
	_this->miBlockLevel = 0;
	// break / continue / clause statement information
	_this->miBreakUnrollSP = 0;
	_this->mipBreakFixup = NULL;
	_this->mipContFixup = NULL;
	_this->mipClause = NULL;
	// import stack
	_this->mipImportStack = NEW( Array_JCLFile );
	_this->mipImportPaths = NEW( Array_JCLPair );
	// error information
	_this->miLastError = 0;
	_this->miFlushedError = 0;
	_this->miFatalState = JILFalse;
	_this->miFatalErrorHandler = NULL;
	_this->miIntroFinished = JILFalse;
	// list of emitted errors and warnings
	_this->mipErrors = NEW( Array_JCLString );
	_this->miNumWarnings = 0;
	_this->miNumErrors = 0;
	_this->miNumCompiles = 0;
	_this->miTimestamp = 0.0;
	// compiler options
	_this->mipOptionStack = NEW( Array_JCLOption );
	_this->mipOptionStack->New(_this->mipOptionStack);
	// optimization
	_this->miOptSavedInstr = 0;
	_this->miOptSizeBefore = 0;
	_this->miOptSizeAfter = 0;
}

//------------------------------------------------------------------------------
// JCLState
//------------------------------------------------------------------------------
// destructor

void destroy_JCLState( JCLState* _this )
{
	// free objects in our struct
	DELETE( _this->mipClasses );
	// free stack
	free( _this->mipStack );
	// free break fixup list
	DELETE( _this->mipBreakFixup );
	// free import stack
	DELETE( _this->mipImportStack );
	DELETE( _this->mipImportPaths );
	// free errors and warnings
	DELETE( _this->mipErrors );
	// free options
	DELETE( _this->mipOptionStack );
}

//------------------------------------------------------------------------------
// JCLState::Copy
//------------------------------------------------------------------------------
// Copying not supported for this class.

void copy_JCLState( JCLState* _this, const JCLState* src )
{
}

//------------------------------------------------------------------------------
// JCLGetStringFromError
//------------------------------------------------------------------------------

static const JILChar* GetStringFromError(JILError err)
{
	// search the compiler error table...
	JILLong i;
	for( i = 0; JCLErrorStrings[i].e != JCL_Unknown_Error_Code; i++ )
	{
		if( JCLErrorStrings[i].e == err )
			return JCLErrorStrings[i].s;
	}
	return JCLErrorStrings[i].s;
}

//------------------------------------------------------------------------------
// EmitError
//------------------------------------------------------------------------------
// Record a compiler error.

JILError EmitError(JCLState* _this, const JCLString* pArg, JILError err)
{
	JCLString* pError;
	JCLString* str;
	JCLString* pMsg;
	const JILChar* pErrorSz;
	const JILChar* pName = NULL;
	JILLong line = 0;
	JILLong column = 0;

	pErrorSz = GetStringFromError(err);
	pError = NEW(JCLString);
	if( _this->mipFile )
	{
		if( JCLGetLength(_this->mipFile->mipPath) )
			pName = JCLGetString(_this->mipFile->mipPath);
		else
			pName = JCLGetString(_this->mipFile->mipName);
		GetCurrentPosition(_this->mipFile, &column, &line);
	}
	pMsg = NEW(JCLString);
	if( pArg )
	{
		JCLFormat(pMsg, "'%s' - ", JCLGetString(pArg));
		JCLAppend(pMsg, pErrorSz);
		pErrorSz = JCLGetString(pMsg);
	}
	switch( GetOptions(_this)->miErrorFormat )
	{
		case kErrorFormatDefault:
			if( pName )
				JCLFormat(pError, "Error %d: %s in %s (%d,%d)\n", err, pErrorSz, pName, line, column);
			else
				JCLFormat(pError, "Error %d: %s\n", err, pErrorSz);
			break;
		case kErrorFormatMS:
			if( pName )
				JCLFormat(pError, "%s(%d): Error %d: %s\n", pName, line, err, pErrorSz);
			else
				JCLFormat(pError, "Error %d: %s\n", err, pErrorSz);
			break;
	}
	str = _this->mipErrors->New(_this->mipErrors);
	str->Copy(str, pError);
	DELETE(pError);
	DELETE(pMsg);
	_this->miNumErrors++;
	return err;
}

//------------------------------------------------------------------------------
// EmitWarning
//------------------------------------------------------------------------------
// Record a compiler warning.

JILError EmitWarning(JCLState* _this, const JCLString* pArg, JILError err)
{
	JCLString* pWarning;
	JCLString* str;
	JCLString* pMsg;
	JILLong i;
	JILLong lev;
	const JILChar* pName = NULL;
	const JILChar* pWarningSz = NULL;
	JILLong line = 0;
	JILLong column = 0;

	// search for the warning in the table...
	for( i = 0; JCLErrorStrings[i].e != JCL_Unknown_Error_Code; i++ )
	{
		if( JCLErrorStrings[i].e == err )
		{
			// don't emit if warning level too high
			if( JCLErrorStrings[i].l > GetOptions(_this)->miWarningLevel )
				return err;
			break;
		}
	}
	pWarningSz = JCLErrorStrings[i].s;
	lev = JCLErrorStrings[i].l;
	pWarning = NEW(JCLString);
	if( _this->mipFile )
	{
		if( JCLGetLength(_this->mipFile->mipPath) )
			pName = JCLGetString(_this->mipFile->mipPath);
		else
			pName = JCLGetString(_this->mipFile->mipName);
		GetCurrentPosition(_this->mipFile, &column, &line);
	}
	pMsg = NEW(JCLString);
	if( pArg )
	{
		JCLFormat(pMsg, "'%s' - ", JCLGetString(pArg));
		JCLAppend(pMsg, pWarningSz);
		pWarningSz = JCLGetString(pMsg);
	}
	switch( GetOptions(_this)->miErrorFormat )
	{
		case kErrorFormatDefault:
			if( pName )
				JCLFormat(pWarning, "Warning %d(%d): %s in %s (%d,%d)\n", err, lev, pWarningSz, pName, line, column);
			else
				JCLFormat(pWarning, "Warning %d(%d): %s\n", err, lev, pWarningSz);
			break;
		case kErrorFormatMS:
			if( pName )
				JCLFormat(pWarning, "%s(%d): Warning %d(%d): %s\n", pName, line, err, lev, pWarningSz);
			else
				JCLFormat(pWarning, "Warning %d(%d): %s\n", err, lev, pWarningSz);
			break;
	}
	str = _this->mipErrors->New(_this->mipErrors);
	str->Copy(str, pWarning);
	DELETE(pWarning);
	DELETE(pMsg);
	_this->miNumWarnings++;
	return err;
}

//------------------------------------------------------------------------------
// FlushErrorsAndWarnings
//------------------------------------------------------------------------------
// Print out all collected compiler errors and warnings.

JILError FlushErrorsAndWarnings(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILLong i;
	JILState* ps = _this->mipMachine;
	Array_JCLString* pErrors = _this->mipErrors;
	for( i = _this->miFlushedError; i < pErrors->count; i++ )
		JILMessageLog(ps, "%s", JCLGetString(pErrors->Get(pErrors, i)));
	_this->miFlushedError = pErrors->count;
	return err;
}

//------------------------------------------------------------------------------
// FatalError
//------------------------------------------------------------------------------
// This should ONLY be called when detecting an internal programming error in
// the compiler, from which it cannot recover and which is likely to crash the
// Application using the compiler API.

void FatalError(JCLState* _this, const JILChar* pFile, JILLong line, const JILChar* pText, const JILChar* pFn)
{
	JCLString* str1;
	// Set fatal error state
	_this->miFatalState = JILTrue;
	// create error message
	str1 = NEW(JCLString);
	JCLFormat(str1, "\n\nFatal error in function %s():\n%s in file %s(%d)\n", pFn, pText, pFile, line);
	if( _this->mipFile )
	{
		JILLong scriptCol, scriptLine;
		const JILChar* pScriptName;
		JCLString* str2 = NEW(JCLString);
		pScriptName = JCLGetString(_this->mipFile->mipName);
		GetCurrentPosition(_this->mipFile, &scriptCol, &scriptLine);
		JCLFormat(str2, "While compiling script %s, line %d, column %d\n\n", pScriptName, scriptLine, scriptCol);
		JCLAppend(str1, JCLGetString(str2));
		DELETE( str2 );
	}
	// output log message
	JILMessageLog(_this->mipMachine, JCLGetString(str1));
	#ifdef _DEBUG
	if( _this->mipMachine->vmLogOutputProc == NULL )
		puts(JCLGetString(str1));
	#endif
	// call fatal error handler, if installed
	if( _this->miFatalErrorHandler )
		_this->miFatalErrorHandler( _this->mipMachine, JCLGetString(str1) );
	DELETE( str1 );
}

//------------------------------------------------------------------------------
// JCLVerbosePrint
//------------------------------------------------------------------------------
// Use the virtual machine's log message output callback to print additional
// information. The function will ignore all calls if the compiler option
// "verbose" is disabled.

void JCLVerbosePrint(JCLState* _this, const JILChar* pFormat, ...)
{
	#if !JIL_NO_FPRINTF
	JILState* pVM = _this->mipMachine;
	if( pVM->vmLogOutputProc && GetOptions(_this)->miVerboseEnable )
	{
		int len;
		char* pBuffer;
		va_list arguments;
		va_start( arguments, pFormat );

		pBuffer = (char*) malloc(4096);
		len = JIL_VSNPRINTF( pBuffer, 4090, pFormat, arguments );
		pBuffer[len] = 0;

		pVM->vmLogOutputProc(pVM, pBuffer);

		free(pBuffer);
		va_end( arguments );
	}
	#endif
}

//------------------------------------------------------------------------------
// JCLCreateType
//------------------------------------------------------------------------------
// This function creates a new type both in the compiler (in the form of a new
// object in the class list) AND in the runtime TypeInfo segment. This way it
// is ensured that type identifier numbers are synchronized between both of
// them. The function returns the new typeID by writing it to pType. If an
// error occurs (for example a native type cannot be found), the function
// returns an error code.

JILError JCLCreateType(JCLState* _this, const JILChar* pName, JILLong parentId, JILTypeFamily family, JILBool bNative, JILLong* pType)
{
	JILError err = JCL_No_Error;
	JILLong typeId = 0;
	JILLong classIndex;
	JCLClass* pClass;

	// create a TypeInfo entry in the runtime
	err = JILCreateType(_this->mipMachine, pName, family, bNative, &typeId);
	if( err )
		goto exit;

	// create a new class entry in the compiler
	classIndex = NumClasses(_this);
	pClass = _this->mipClasses->New(_this->mipClasses);
	JCLSetString(pClass->mipName, pName);
	pClass->miFamily = family;
	pClass->miNative = bNative;
	pClass->miType = typeId;
	pClass->miParentType = parentId;
	*pType = typeId;

	// do classIndex and typeId match?
	if( classIndex != typeId )
		FATALERROREXIT("JCLCreateType", "ClassIndex and TypeID are out of sync!");

exit:
	return err;
}

//------------------------------------------------------------------------------
// SetCompileContext
//------------------------------------------------------------------------------
// Sets the compiler context to the specified class and function. The compile
// context determines which function of which class is currently being parsed.
// Calling this also automatically sets the output context, which is the function
// and class to which bytecode is written.

void SetCompileContext( JCLState* _this, JILLong typeID, JILLong funcID )
{
	_this->miOutputClass = _this->miClass = typeID;
	_this->miOutputFunc = _this->miFunc = funcID;
}

//------------------------------------------------------------------------------
// SetCompileContextOnly
//------------------------------------------------------------------------------
// Sets the compiler context to the specified class and function, but leaves the
// output context alone.

void SetCompileContextOnly( JCLState* _this, JILLong typeID, JILLong funcID )
{
	_this->miClass = typeID;
	_this->miFunc = funcID;
}

//------------------------------------------------------------------------------
// SetOutputContext
//------------------------------------------------------------------------------
// Sets the output context to the specified class and function. The output
// context determines to which function of which class bytecode is written.

void SetOutputContext( JCLState* _this, JILLong typeID, JILLong funcID )
{
	_this->miOutputClass = typeID;
	_this->miOutputFunc = funcID;
}

//------------------------------------------------------------------------------
// GetCodeLocator
//------------------------------------------------------------------------------
// Get the current code locator.

static JILLong GetCodeLocator(JCLState* _this)
{
	JCLFunc* pFunc = CurrentOutFunc(_this);
	return pFunc->mipCode->count;
}

//------------------------------------------------------------------------------
// FindClass
//------------------------------------------------------------------------------
// Find and return a class by name. Returns the address of the found class and
// the index number. If the class was not found returns NULL and the number of
// known classes.

JILLong FindClass( JCLState* _this, const JCLString* pName, JCLClass** ppClass )
{
	JCLClass* pClass;
	JCLString* pAlias;
	JILLong i;
	JILLong j;
	for( i = 0; i < NumClasses(_this); i++ )
	{
		pClass = GetClass(_this, i);
		if( JCLCompare(pClass->mipName, pName) )
		{
			*ppClass = pClass;
			return i;
		}
		for( j = 0; j < pClass->mipAlias->count; j++ )
		{
			pAlias = pClass->mipAlias->Get(pClass->mipAlias, j);
			if( JCLCompare(pAlias, pName) )
			{
				*ppClass = pClass;
				return i;
			}
		}
	}
	*ppClass = NULL;
	return 0;
}

//------------------------------------------------------------------------------
// FindFunction
//------------------------------------------------------------------------------
// Find and return a function by name.

JILLong FindFunction( JCLState* _this, JILLong typeID, const JCLString* pName, JILLong start, JCLFunc** ppFunc )
{
	JCLFunc* pFunc = NULL;
	JILLong i;
	for( i = start; i < NumFuncs(_this, typeID); i++ )
	{
		pFunc = GetFunc(_this, typeID, i);
		if( JCLCompare(pFunc->mipName, pName) )
			break;
		pFunc = NULL;
	}
	*ppFunc = pFunc;
	return i;
}

//------------------------------------------------------------------------------
// FindDiscreteFunction
//------------------------------------------------------------------------------
// Find and return a function by name, arguments and result type.

JILLong FindDiscreteFunction( JCLState* _this, JILLong typeID, const JCLString* pName, const JCLVar* pResult, const Array_JCLVar* pArgs, JCLFunc** ppFunc )
{
	JCLFunc* pFunc = NULL;
	JILLong i, j;
	JCLVar* vsrc;
	JCLVar* vdst;
	for( i = 0; i < NumFuncs(_this, typeID); i++ )
	{
		pFunc = GetFunc(_this, typeID, i);
		if( JCLCompare(pFunc->mipName, pName)
		&&	pFunc->mipArgs->count == pArgs->count 
		&&	EqualTypes(pFunc->mipResult, pResult) )
		{
			for( j = 0; j < pFunc->mipArgs->count; j++ )
			{
				vsrc = pFunc->mipArgs->Get(pFunc->mipArgs, j);
				vdst = pArgs->Get(pArgs, j);
				if( !EqualTypes(vsrc, vdst) )
					break;
			}
			if( j == pFunc->mipArgs->count )
				break;
		}
		pFunc = NULL;
	}
	*ppFunc = pFunc;
	return i;
}

//------------------------------------------------------------------------------
// FindPrototype
//------------------------------------------------------------------------------
// Find a function by a given prototype. This function is used when parsing
// a function declaration statement and also reports conflict errors if a too
// similar function variant already exist.

static JILError FindPrototype( JCLState* _this, const JCLFunc* src, JCLFunc** ppFunc )
{
	int i, j;
	JCLFunc* dst;
	JCLVar* vsrc;
	JCLVar* vdst;
	JILLong thisType = _this->miClass;

	for( i = 0; i < NumFuncs(_this, thisType); i++ )
	{
		dst = GetFunc(_this, thisType, i);
		if( dst != src										// not same object
		&&	JCLCompare(dst->mipName, src->mipName)			// same name
		&&	dst->miClassID == src->miClassID				// same class membership
		&&	dst->mipArgs->count == src->mipArgs->count		// same number of arguments
		&&	ImpConvertible(_this, src->mipResult, dst->mipResult) )	// same result type, or one of them var
		{
			// check argument types
			for( j = 0; j < dst->mipArgs->count; j++ )
			{
				vsrc = src->mipArgs->Get(src->mipArgs, j);
				vdst = dst->mipArgs->Get(dst->mipArgs, j);
				if( !ImpConvertible(_this, vsrc, vdst) )
				{
					// not the same function
					break;
				}
			}
			// all seem to be equal
			if( j == dst->mipArgs->count )
			{
				JILError result;
				// check for function class mismatches
				if( dst->miMethod != src->miMethod
				||	dst->miCtor != src->miCtor				// unlikely to happen...
				||	dst->miConvertor != src->miConvertor	// also unlikely...
				||	dst->miAccessor != src->miAccessor
				||	dst->miCofunc != src->miCofunc
				||	dst->miExplicit != src->miExplicit )
				{
					return JCL_ERR_Function_Redefined;
				}
				// check result for type conflicts
				result = CheckTypeConflict(src->mipResult, dst->mipResult);
				if( result )
					return result;
				// check arguments for type conflicts
				for( j = 0; j < dst->mipArgs->count; j++ )
				{
					vsrc = src->mipArgs->Get(src->mipArgs, j);
					vdst = dst->mipArgs->Get(dst->mipArgs, j);
					result = CheckTypeConflict(vsrc, vdst);
					if( result )
						return result;
				}
				// all is fine, return function
				*ppFunc = dst;
				return JCL_No_Error;
			}
		}
	}
	return JCL_ERR_Undefined_Identifier;
}

//------------------------------------------------------------------------------
// FindBestPrototype
//------------------------------------------------------------------------------
// Find the best match when calling a function. This function is used to resolve
// calls to script functions and tries to find and return the function variant
// that requires the smallest amount of automatic type conversion.
// The function returns the number of matching function declarations that
// require the same amount of conversion (or none at all).

static JILLong FindBestPrototype( JCLState* _this, JILLong classIdx, const JCLFunc* src, JCLFunc** ppFunc )
{
	JILLong i, j;
	JCLFunc* dst;
	JCLVar* vsrc;
	JCLVar* vdst;
	JILLong score;
	JILLong candidates = 0;

	// the ideal candidate would have a "conversion score" of 0
	JILLong minScore = 0x7fffffff;
	JCLFunc* minFunc = NULL;

	for( i = 0; i < NumFuncs(_this, classIdx); )
	{
		dst = GetFunc(_this, classIdx, i);
		if( JCLCompare(dst->mipName, src->mipName)				// same name
		&&	dst->mipArgs->count == src->mipArgs->count )		// same number of arguments
		{
			score = 0;
			// if src result is not "void"
			if( src->mipResult->miMode != kModeUnused )
			{
				// but dst result is "void", we have no match at all
				if( dst->mipResult->miMode == kModeUnused )
					goto cont;
				// is there a way to convert result at all?
				if( !DynConvertible(_this, dst->mipResult, src->mipResult) )
					goto cont;
				// result type score
				if( !ImpConvertible(_this, src->mipResult, dst->mipResult) )
					score += 2;
			}
			else
			{
				// if src result is "void", favor functions that are "void"
				if( dst->mipResult->miMode != kModeUnused )
					score++;
			}
			// calculate score from arguments
			for( j = 0; j < dst->mipArgs->count; j++ )
			{
				vsrc = src->mipArgs->Get(src->mipArgs, j);
				vdst = dst->mipArgs->Get(dst->mipArgs, j);
				// is there a way to convert argument at all?
				if( !DynConvertible(_this, vsrc, vdst) )
					goto cont;
				if( !ImpConvertible(_this, vsrc, vdst) )
					score += 2;
			}
			// now check score against current champion
			if( score < minScore )
			{
				// new champion
				minScore = score;
				minFunc = dst;
				candidates = 1;
			}
			else if( score == minScore )
			{
				candidates++;
			}
		}
cont:
		i++;
	}
	*ppFunc = minFunc;
	return candidates;
}

//------------------------------------------------------------------------------
// FindConvertor
//------------------------------------------------------------------------------
// Find a convertor method by given source and destination types.

static JILError FindConvertor(JCLState* _this, JCLVar* src, JCLVar* dst, JCLFunc** ppFunc)
{
	JILError err = JCL_ERR_Incompatible_Type;
	JILLong i;
	JCLFunc* pFunc;

	*ppFunc = NULL;
	if( ImpConvertible(_this, src, dst) )
	{
		err = JCL_No_Error;
	}
	else if( IsVarClassType(_this, src) )
	{
		for( i = 0; i < NumFuncs(_this, src->miType); i++ )
		{
			pFunc = GetFunc(_this, src->miType, i);
			if( pFunc->miConvertor && ImpConvertible(_this, pFunc->mipResult, dst) )
			{
				// all is fine, return function
				*ppFunc = pFunc;
				err = JCL_No_Error;
				break;
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// FindConstructor
//------------------------------------------------------------------------------
// Find a constructor by a given source and destination type.

static JILError FindConstructor(JCLState* _this, JCLVar* src, JCLVar* dst, JCLFunc** ppFunc)
{
	JILError err = JCL_ERR_Incompatible_Type;
	JILLong i;
	JCLFunc* pFunc;
	JCLVar* pArg;

	*ppFunc = NULL;
	if( ImpConvertible(_this, src, dst) )
	{
		err = JCL_No_Error;
	}
	else if( IsVarClassType(_this, dst) )
	{
		for( i = 0; i < NumFuncs(_this, dst->miType); i++ )
		{
			pFunc = GetFunc(_this, dst->miType, i);
			if( pFunc->miCtor && pFunc->mipArgs->count == 1 )
			{
				pArg = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
				if( ImpConvertible(_this, src, pArg) )
				{
					// all is fine, return function
					*ppFunc = pFunc;
					err = JCL_No_Error;
					break;
				}
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// FindDefaultCtor
//------------------------------------------------------------------------------
// Find a default constructor by a given type.

static JILError FindDefaultCtor(JCLState* _this, JCLVar* pVar, JCLFunc** ppFunc)
{
	JILError err = JCL_ERR_Incompatible_Type;
	JILLong i;
	JCLFunc* pFunc;

	*ppFunc = NULL;
	if( IsVarClassType(_this, pVar) )
	{
		err = JCL_ERR_No_Default_Ctor;
		for( i = 0; i < NumFuncs(_this, pVar->miType); i++ )
		{
			pFunc = GetFunc(_this, pVar->miType, i);
			if( pFunc->miCtor && pFunc->mipArgs->count == 0 )
			{
				// all is fine, return function
				*ppFunc = pFunc;
				err = JCL_No_Error;
				break;
			}
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// FindAccessor
//------------------------------------------------------------------------------
// Find and return an accessor function by name.

static JILLong FindAccessor( JCLState* _this, JILLong classIdx, const JCLString* pName, JILLong start, JCLFunc** ppFunc )
{
	JCLFunc* pFunc = NULL;
	JILLong i;
	for( i = start; i < NumFuncs(_this, classIdx); i++ )
	{
		pFunc = GetFunc(_this, classIdx, i);
		if( pFunc->miAccessor && JCLCompare(pFunc->mipName, pName) )
			break;
		pFunc = NULL;
	}
	*ppFunc = pFunc;
	return i;
}

//------------------------------------------------------------------------------
// FindSetAccessor
//------------------------------------------------------------------------------
// Find the correct accessor Setter function.

static JILBool FindSetAccessor(JCLState* _this, JILLong classIdx, const JCLString* pName, JCLVar* src, JCLFunc** ppFunc)
{
	JILLong index;
	JCLVar* pVar;
	JCLFunc* pFunc;

	for( index = 0; ; index++ )
	{
		index = FindAccessor(_this, classIdx, pName, index, &pFunc);
		if( !pFunc )
			break;
		// check if we have the correct prototype
		if( pFunc->mipResult->miMode == kModeUnused )
		{
			pVar = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
			if( DynConvertible(_this, src, pVar) )	// Fix: take type conversion into account so accessor methods support automatic type conversion
			{
				// found.
				*ppFunc = pFunc;
				return JILTrue;
			}
			// accessors cannot be overloaded, so if we find the name, but the type doesn't match, we can quit
			return JILFalse;
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// FindGetAccessor
//------------------------------------------------------------------------------
// Find the correct accessor Getter function.

static JILBool FindGetAccessor(JCLState* _this, JILLong classIdx, const JCLString* pName, JCLVar* dst, JCLFunc** ppFunc)
{
	JILLong index;
	JCLFunc* pFunc;

	for( index = 0; ; index++ )
	{
		index = FindAccessor(_this, classIdx, pName, index, &pFunc);
		if( !pFunc )
			break;
		// check if we have the correct prototype
		if( pFunc->mipArgs->count == 0 )
		{
			if( DynConvertible(_this, pFunc->mipResult, dst) ) // Fix: take type conversion into account so accessor methods support automatic type conversion
			{
				// found.
				*ppFunc = pFunc;
				return JILTrue;
			}
			// accessors cannot be overloaded, so if we find the name, but the type doesn't match, we can quit
			return JILFalse;
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// SimStackFixup
//------------------------------------------------------------------------------
// If a new var is pushed onto the stack, the stack indexes of all variables on
// the stack increase. If a variable is popped from stack, the indexes of all
// remaining variables decrease. This is called to increase or decrease the
// stack indexes of all variables on the stack by the given offset.

static void SimStackFixup(JCLState* _this, JILLong offset)
{
	int i;
	for( i = _this->miStackPos; i < kSimStackSize; i++ )
	{
		if( _this->mipStack[i] )
		{
			_this->mipStack[i]->miIndex += offset;
			// safety test...
			if( _this->mipStack[i]->miIndex < 0
			||	(_this->mipStack[i]->miIndex + _this->miStackPos) > kSimStackSize
			|| _this->mipStack[i]->miMode != kModeStack )
			{
				FATALERROR("SimStackFixup", "Inconsistent stack variable detected", return);
			}
		}
	}
}

//------------------------------------------------------------------------------
// SimStackPush
//------------------------------------------------------------------------------
// Pushes a variable onto the simulated stack. The pointer will be stored, so
// the var object must exist until popped from the stack.

static void SimStackPush(JCLState* _this, JCLVar* pVar, JILBool bHidden)
{
	if( pVar && pVar->miMode != kModeUnused && pVar->miMode != kModeStack )
	{
		FATALERROR("SimStackPush", "Variable already in use", return);
	}
	if( _this->miStackPos )
	{
		SimStackFixup(_this, 1);
		if( pVar )
		{
			pVar->miIndex = 0;
			pVar->miMode = kModeStack;
			pVar->miOnStack = JILTrue;
			pVar->miHidden = bHidden;
		}
		_this->mipStack[-- _this->miStackPos] = pVar;
	}
	else
	{
		FATALERROR("SimStackPush", "Stack overflow", return);
	}
}

//------------------------------------------------------------------------------
// SimStackReserve
//------------------------------------------------------------------------------
// Reserves one or more stack locations on the simulated stack. This is used to
// simulate stack usage not caused by local variables.

static JILLong SimStackReserve(JCLState* _this, JILLong count)
{
	int i;
	for( i = 0; i < count; i++ )
		SimStackPush(_this, NULL, JILFalse);
	return _this->miStackPos;
}

//------------------------------------------------------------------------------
// SimStackPop
//------------------------------------------------------------------------------
// Pop one or more items from the simulated stack. The objects will NOT be
// destroyed (freed), the caller must ensure proper deallocation of variables.

static void SimStackPop(JCLState* _this, JILLong count)
{
	int i;
	JCLVar* pVar;
	if( count )
	{
		if( (_this->miStackPos + count) > kSimStackSize )
			count = kSimStackSize - _this->miStackPos;
		for( i = 0; i < count; i++ )
		{
			pVar = _this->mipStack[_this->miStackPos];
			_this->mipStack[_this->miStackPos] = NULL;
			if( pVar )
			{
				pVar->miOnStack = JILFalse;
				pVar->miHidden = JILFalse;
			}
			_this->miStackPos++;
		}
		SimStackFixup(_this, -count);
	}
}

//------------------------------------------------------------------------------
// SimStackUnroll
//------------------------------------------------------------------------------
// Pop as many items from the simulated stack, until the stack pointer has the
// given value. Given stack pos must be *higher* than current stack pos.

static void SimStackUnroll(JCLState* _this, JILLong stackPos)
{
	JILLong count = stackPos - _this->miStackPos;
	if( count > 0 )
	{
		SimStackPop(_this, count);
	}
}

//------------------------------------------------------------------------------
// SimStackGet
//------------------------------------------------------------------------------
// Get a variable from the simulated stack. Index is the position on the stack
// to return, 0 is the most recent item on the stack, 1 the second item, 2
// the third item, and so on. If index exceeds the number of items on the stack
// or is negative, NULL is returned.

static JCLVar* SimStackGet(JCLState* _this, JILLong index)
{
	JCLVar* pVar;
	if( (_this->miStackPos + index) > kSimStackSize || index < 0 )
		FATALERROR("SimStackGet", "Illegal access to stack", return NULL);
	pVar = _this->mipStack[_this->miStackPos + index];
	return pVar;
}

//------------------------------------------------------------------------------
// SimRegisterSet
//------------------------------------------------------------------------------
// "Assigns" a variable to a simulated machine register.

static void SimRegisterSet(JCLState* _this, JILLong regNum, JCLVar* pVar)
{
	if( pVar && pVar->miMode != kModeUnused )
	{
		FATALERROR("SimRegisterSet", "Variable already in use", return);
	}
	if( regNum >= 0 && regNum < kNumRegisters )
	{
		if( pVar )
		{
			pVar->miIndex = regNum;
			pVar->miMode = kModeRegister;
		}
		_this->mipRegs[regNum] = pVar;
	}
	else
	{
		FATALERROR("SimRegisterSet", "Invalid register number", return);
	}
}

//------------------------------------------------------------------------------
// SimRegisterUnset
//------------------------------------------------------------------------------
// "Unassigns" a simulated machine register.

static void SimRegisterUnset(JCLState* _this, JILLong regNum)
{
	JCLVar* pVar;
	if( regNum >= 0 && regNum < kNumRegisters )
	{
		pVar = _this->mipRegs[regNum];
		_this->mipRegs[regNum] = NULL;
		if( pVar )
		{
			pVar->miIndex = 0;
			pVar->miMode = kModeUnused;
		}
	}
	else
	{
		FATALERROR("SimRegisterUnset", "Invalid register number", return);
	}
}

//------------------------------------------------------------------------------
// SimRegisterGet
//------------------------------------------------------------------------------
// Gets a variable from a simulated machine register.

static JCLVar* SimRegisterGet(JCLState* _this, JILLong regNum)
{
	if( regNum >= 0 && regNum < kNumRegisters )
	{
		return _this->mipRegs[regNum];
	}
	else
	{
		FATALERROR("SimRegisterGet", "Invalid register number", return NULL);
	}
}

//------------------------------------------------------------------------------
// MakeLocalVar
//------------------------------------------------------------------------------
// Create a new local variable, either in a register, or on the stack. Caller
// must maintain an array of local variables. The new variable object will be
// created in this array, and a pointer to it will be then moved either to the
// stack or into a register.
// The given pVarDesc argument is used to specify the name, type and flags of
// the variable. Pass the result of the IsFullTypeDecl() call in pVarDesc.
// ATTENTION: This function generates code!

static JILError MakeLocalVar(JCLState* _this, Array_JCLVar* pLocals, JILLong where, const JCLVar* pVarDesc)
{
	int i;
	JILError err = JCL_No_Error;
	JCLVar* pVar;

	// check if a var with that name exists
	if( CurrentFunc(_this)->miMethod )
		err = IsIdentifierUsed(_this, kMethodLocalVar, _this->miClass, pVarDesc->mipName);
	else
		err = IsIdentifierUsed(_this, kFuncLocalVar, type_global, pVarDesc->mipName);
	if( err )
		goto exit;

	// create new var
	pVar = pLocals->New(pLocals);
	pVar->Copy(pVar, pVarDesc);

	// should we auto choose the location?
	if( where == kLocalAuto )
	{
		where = kLocalStack;
		// check if we can use a register
		if( _this->miNumVarRegisters < kMaxVarRegisters )
		{
			for( i = kFirstVarRegister; i < kNumRegisters; i++ )
			{
				if( SimRegisterGet(_this, i) == NULL )
				{
					where = kLocalRegister;
					break;
				}
			}
		}
	}
	if( where == kLocalRegister )
	{
		// make a register var
		if( _this->miNumVarRegisters >= kMaxVarRegisters )
			FATALERROREXIT("MakeLocalVar", "Unable to allocate var register");
		for( i = kFirstVarRegister; i < kNumRegisters; i++ )
		{
			if( SimRegisterGet(_this, i) == NULL )
			{
				SimRegisterSet(_this, i, pVar);
				_this->miRegUsage[i]++;
				CurrentFunc(_this)->miLocalRegs[i]++;
				break;
			}
		}
		if( i == kNumRegisters )
			FATALERROREXIT("MakeLocalVar", "Unable to allocate var register");
	}
	else if( where == kLocalStack )
	{
		// make a stack var
		cg_push_multi(_this, 1);	// push one null handle
		SimStackPush(_this, pVar, JILFalse);
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// FreeLocalVars
//------------------------------------------------------------------------------
// The local variables of a code block no longer exist and should be removed
// from the stack or registers. Caller must pass in the array of local variables.
// ATTENTION: This function generates code!

static void FreeLocalVars(JCLState* _this, Array_JCLVar* pLocals)
{
	int i;
	JCLVar* pVar;
	// find highest position on stack
	JILLong numStack = -1;
	for( i = 0; i < pLocals->count; i++ )
	{
		pVar = pLocals->Get(pLocals, i);
		if( pVar->miOnStack && pVar->miIndex > numStack )
			numStack = pVar->miIndex;
	}
	if( numStack != -1 )
	{
		// if stack var has index 0, we have 1 var on the stack, so:
		numStack++;
		cg_pop_multi(_this, numStack);
		SimStackPop(_this, numStack);
	}
	// now unset all register vars
	for( i = 0; i < pLocals->count; i++ )
	{
		pVar = pLocals->Get(pLocals, i);
		if( pVar->miMode == kModeRegister )
			SimRegisterUnset(_this, pVar->miIndex);
	}
	pLocals->Trunc(pLocals, 0);
}

//------------------------------------------------------------------------------
// FindLocalVar
//------------------------------------------------------------------------------
// Find a local variable with a specific name. This function searches ALL the
// variables currently on the stack or in registers and returns a pointer to
// the variable.

static JCLVar* FindLocalVar(JCLState* _this, const JCLString* pName)
{
	int i;
	JCLVar* pVar;

	// search r0, 'this'
	pVar = SimRegisterGet(_this, 0);
	if( pVar && !pVar->miHidden && JCLCompare(pVar->mipName, pName) )
		return pVar;

	// search var registers
	for( i = kFirstVarRegister; i < kNumRegisters; i++ )
	{
		pVar = SimRegisterGet(_this, i);
		if( pVar && (pVar->miUsage == kUsageVar) && !pVar->miHidden && JCLCompare(pVar->mipName, pName) )
			return pVar;
	}

	// search the stack from top down
	for( i = 0; i < (kSimStackSize - _this->miStackPos); i++ )
	{
		pVar = SimStackGet(_this, i);
		if( pVar && !pVar->miHidden && JCLCompare(pVar->mipName, pName) )
			return pVar;
	}

	return NULL;
}

//------------------------------------------------------------------------------
// FindFuncArg
//------------------------------------------------------------------------------
// Find a function argument by name.

static JCLVar* FindFuncArg(JCLState* _this, const JCLString* pName)
{
	int i;
	JCLVar* pVar;
	JCLFunc* pFunc = CurrentFunc(_this);
	for( i = 0; i < pFunc->mipArgs->count; i++ )
	{
		pVar = pFunc->mipArgs->Get(pFunc->mipArgs, i);
		if( pVar && JCLCompare(pVar->mipName, pName) )
			return pVar;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// MakeThisVar
//------------------------------------------------------------------------------
// Create a 'this' reference for the given class.

static JCLVar* MakeThisVar(JCLState* _this, JILLong typeID)
{
	JCLVar* pThis = NEW(JCLVar);
	pThis->miType = typeID;
	pThis->miConst = JILFalse;				// we could make functions const...
	pThis->miRef = JILTrue;					// 'this' is a reference
	pThis->miElemType = type_var;
	pThis->miElemRef = JILFalse;
	JCLSetString(pThis->mipName, "this");
	pThis->miMode = kModeUnused;			// set by SimRegisterSet()
	pThis->miUsage = kUsageVar;
	pThis->miIndex = 0;						// always r0
	pThis->miInited = JILTrue;
	pThis->miUnique = JILTrue;
	return pThis;
}

//------------------------------------------------------------------------------
// MakeTempVar
//------------------------------------------------------------------------------
// Allocate a register for temporarily holding a value. Caller needs to pass a
// pointer to a JCLVar pointer to this function. The pointer can be a stack
// variable. This function will automatically allocate a new JCLVar object and
// initialize it. The pointer to the JCLVar will be returned in ppVar and must
// be passed to FreeTempVar().

static JILError MakeTempVar(JCLState* _this, JCLVar** ppVar, const JCLVar* src)
{
	JILError err = JCL_No_Error;
	int i;
	JCLVar* pVar;
	if( ppVar )
	{
		*ppVar = NULL;
		// check if we have a free temp register
		for( i = kFirstVarRegister; i < kNumRegisters; i++ )
		{
			if( SimRegisterGet(_this, i) == NULL )
			{
				pVar = NEW(JCLVar);
				pVar->miType = type_var;
				pVar->miElemType = type_var;
				pVar->miElemRef = JILTrue;
				pVar->miIniType = type_var;
				pVar->miInited = JILTrue;
				if( src )
				{
					pVar->Copy(pVar, src);
					pVar->miMode = kModeUnused;
					pVar->miIniType = pVar->miType;
				}
				pVar->miUsage = kUsageTemp;
				pVar->miRef = JILTrue;
				pVar->miWeak = JILFalse;
				pVar->miUnique = JILTrue;
				*ppVar = pVar;
				SimRegisterSet(_this, i, pVar);
				_this->miRegUsage[i]++;
				break;
			}
		}
		if( i == kNumRegisters )
			FATALERROREXIT("MakeTempVar", "No free temporary register found");
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// MakeTempArrayVar
//------------------------------------------------------------------------------
// Allocate two temp registers for array access.

static JILError MakeTempArrayVar(JCLState* _this, JCLVar** ppVar, JCLVar* src)
{
	JILError err = JCL_No_Error;

	JCLVar* pArrVar = NULL;
	JCLVar* pIdxVar = NULL;

	// create a temp array var
	err = MakeTempVar(_this, &pArrVar, src);
	if( err )
		goto exit;
	*ppVar = pArrVar;
	pArrVar->miMode = kModeArray;
	pArrVar->miType = src->miElemType;
	pArrVar->miRef = src->miElemRef;
	pArrVar->miConst = src->miConst;
	err = MakeTempVar(_this, &pIdxVar, NULL);
	if( err )
		goto exit;
	pArrVar->mipArrIdx = pIdxVar;
	pIdxVar->miType = type_int;

exit:
	return err;
}

//------------------------------------------------------------------------------
// FreeTempVar
//------------------------------------------------------------------------------
// Free a previously allocated temp var. Pass in the JCLVar pointer obtained
// from MakeTempVar(). The given JCLVar object will be destroyed.

static void FreeTempVar(JCLState* _this, JCLVar** ppVar)
{
	if( ppVar && *ppVar )
	{
		JCLVar* pVar = *ppVar;
		if( pVar->miUsage == kUsageTemp )
		{
			if( pVar->mipArrIdx )
				FreeTempVar(_this, &(pVar->mipArrIdx));
			SimRegisterUnset(_this, pVar->miIndex);
			DELETE( pVar );
			*ppVar = NULL;
		}
		else
		{
			FATALERROR("FreeTempVar", "JCLVar is not a temp var", return);
		}
	}
}

//------------------------------------------------------------------------------
// SetMarker
//------------------------------------------------------------------------------
// Capture the current code position in a marker.

static void SetMarker(JCLState* _this, SMarker* pMarker)
{
	pMarker->mipFunc = CurrentOutFunc(_this);
	pMarker->miCodePos = pMarker->mipFunc->mipCode->count;
	pMarker->miLiteralPos = pMarker->mipFunc->mipLiterals->count;
	pMarker->miStackPos = _this->miStackPos;
	pMarker->miErrorPos = _this->mipErrors->count;
	pMarker->miNumErr = _this->miNumErrors;
	pMarker->miNumWarn = _this->miNumWarnings;
}

//------------------------------------------------------------------------------
// RestoreMarker
//------------------------------------------------------------------------------
// Undo all code changes up to the given marker.

static void RestoreMarker(JCLState* _this, SMarker* pMarker)
{
	Array_JILLong* pCode = pMarker->mipFunc->mipCode;
	Array_JCLLiteral* pLiterals = pMarker->mipFunc->mipLiterals;
	Array_JCLString* pErrors = _this->mipErrors;

	pCode->Trunc(pCode, pMarker->miCodePos);
	pLiterals->Trunc(pLiterals, pMarker->miLiteralPos);
	pErrors->Trunc(pErrors, pMarker->miErrorPos);
	SimStackUnroll(_this, pMarker->miStackPos);
	_this->miNumErrors = pMarker->miNumErr;
	_this->miNumWarnings = pMarker->miNumWarn;
}

//------------------------------------------------------------------------------
// NumClasses
//------------------------------------------------------------------------------
// Return current number of classes.

JILLong NumClasses(JCLState* _this)
{
	return _this->mipClasses->count;
}

//------------------------------------------------------------------------------
// NumFuncs
//------------------------------------------------------------------------------
// Return current number of functions in given class.

JILLong NumFuncs(JCLState* _this, JILLong typeID)
{
	return ClassDefined(_this, typeID) ? GetClass(_this, typeID)->mipFuncs->count : 0;
}

//------------------------------------------------------------------------------
// GetClass
//------------------------------------------------------------------------------
// Return a pointer to a specific class.

JCLClass* GetClass(JCLState* _this, JILLong typeID)
{
	JILError err;
	if( typeID < 0 || typeID >= _this->mipClasses->count )
		FATALERROREXIT("GetClass", "Access to invalid type id");
exit:
	return _this->mipClasses->Get(_this->mipClasses, typeID);
}

//------------------------------------------------------------------------------
// GetParentType
//------------------------------------------------------------------------------
// Returns the parent typeID of the given type, or 0.

JILLong GetParentType(JCLState* _this, JILLong typeID)
{
	return GetClass(_this, typeID)->miParentType;
}

//------------------------------------------------------------------------------
// HasParentType
//------------------------------------------------------------------------------
// Returns true if the given type has a parent type.

JILBool HasParentType(JCLState* _this, JILLong typeID)
{
	JILLong type = GetParentType(_this, typeID);
	return (type != type_null && type != type_global);
}

//------------------------------------------------------------------------------
// ClassDefined
//------------------------------------------------------------------------------
// Checks if a class with the given typeID exists.

JILBool ClassDefined(JCLState* _this, JILLong typeID)
{
	return (typeID >= 0 && typeID < _this->mipClasses->count);
}

//------------------------------------------------------------------------------
// GetFunc
//------------------------------------------------------------------------------
// Return a pointer to a specific function.

JCLFunc* GetFunc(JCLState* _this, JILLong typeID, JILLong funcIdx)
{
	JILError err;
	JCLClass* pClass = GetClass(_this, typeID);
	if( funcIdx < 0 || funcIdx >= pClass->mipFuncs->count )
		FATALERROREXIT("GetFunc", "Access to invalid function index");
exit:
	return pClass->mipFuncs->Get(pClass->mipFuncs, funcIdx);
}

//------------------------------------------------------------------------------
// CurrentClass
//------------------------------------------------------------------------------
// Return a pointer to the currently parsed class.

static JCLClass* CurrentClass(JCLState* _this)
{
	return GetClass(_this, _this->miClass);
}

//------------------------------------------------------------------------------
// CurrentOutClass
//------------------------------------------------------------------------------
// Return a pointer to the current output target class.

static JCLClass* CurrentOutClass(JCLState* _this)
{
	return GetClass(_this, _this->miOutputClass);
}

//------------------------------------------------------------------------------
// CurrentFunc
//------------------------------------------------------------------------------
// Return a pointer to the currently parsed function.

static JCLFunc* CurrentFunc(JCLState* _this)
{
	return GetFunc(_this, _this->miClass, _this->miFunc);
}

//------------------------------------------------------------------------------
// CurrentOutFunc
//------------------------------------------------------------------------------
// Return a pointer to the current output target function.

static JCLFunc* CurrentOutFunc(JCLState* _this)
{
	return GetFunc(_this, _this->miOutputClass, _this->miOutputFunc);
}

//------------------------------------------------------------------------------
// IsFuncInGlobalScope
//------------------------------------------------------------------------------
// Checks if the given identifier is a function that is globally accessible,
// either because it was directly declared in the global scope, or it has been
// mapped into the global scope by the "using" keyword.

static JILBool IsFuncInGlobalScope(JCLState* _this, const JCLString* pName)
{
	JCLFunc* pFunc = NULL;
	Array_JILLong* pUsing = GetOptions(_this)->mipUsing;
	JILLong i;

	FindFunction(_this, type_global, pName, 0, &pFunc);
	if( pFunc )
		return JILTrue;
	for( i = 0; i < pUsing->count; i++ )
	{
		FindFunction(_this, pUsing->array[i], pName, 0, &pFunc);
		if( pFunc )
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsIdentifierUsed
//------------------------------------------------------------------------------
// Checks if the given identifier name can be defined at the given scope, or if
// it would conflict with an already defined identifier.
// Argument 'whatIsDefined' describes for what kind of language element the
// identifier is going to be used (global function, member variable, etc.).

static JILError IsIdentifierUsed(JCLState* _this, JILLong whatIsDefined, JILLong classIdx, const JCLString* pName)
{
	JCLClass* pClass = NULL;
	JCLFunc* pFunc = NULL;
	JILLong i;

	switch( whatIsDefined )
	{
		case kGlobalVar:
		case kGlobalClass:
		case kGlobalCofunc:
		case kGlobalDelegate:
		case kGlobalAlias:
			// try global variables
			if( FindGlobalVar(_this, classIdx, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try global functions
			if( IsFuncInGlobalScope(_this, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try classes
			FindClass(_this, pName, &pClass);
			if( pClass )
				return JCL_ERR_Identifier_Already_Defined;
			break;
		case kGlobalFunc:
			// try global variables
			if( FindGlobalVar(_this, classIdx, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// multiple functions with same name allowed...
			break;
		case kClassVar:
		case kClassVarDelegate:
			// try global variables
			if( FindGlobalVar(_this, classIdx, pName) )
				EmitWarning(_this, pName, JCL_WARN_Member_Hides_Global);
			// try member variables
			if( FindMemberVar(_this, classIdx, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try global functions
			if( IsFuncInGlobalScope(_this, pName) )
				EmitWarning(_this, pName, JCL_WARN_Member_Hides_Global);
			// try member functions (allow delegate variables to hide functions)
			if( whatIsDefined == kClassVar )
			{
				// disallow all but accessor methods
				i = 0;
				while( i < NumFuncs(_this, classIdx) )
				{
					i = FindFunction(_this, classIdx, pName, i, &pFunc) + 1;
					if( pFunc && !pFunc->miAccessor )
						return JCL_ERR_Identifier_Already_Defined;
				}
			}
			// try classes
			FindClass(_this, pName, &pClass);
			if( pClass )
				return JCL_ERR_Identifier_Already_Defined;
			break;
		case kClassFunc:
		case kClassMethod:
		case kClassCtor:
			// try member variables
			if( FindMemberVar(_this, classIdx, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// multiple functions / methods with same name allowed...
			break;
		case kClassAccessor:
			// hiding member variables allowed
			// multiple functions / methods with same name allowed...
			break;
		case kFuncLocalVar:
			// try function args
			if( FindFuncArg(_this, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try local vars
			if( FindLocalVar(_this, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try global variables
			if( FindGlobalVar(_this, classIdx, pName) )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Global);
			// try global functions
			if( IsFuncInGlobalScope(_this, pName) )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Global);
			// try classes
			FindClass(_this, pName, &pClass);
			if( pClass )
				return JCL_ERR_Identifier_Already_Defined;
			break;
		case kMethodLocalVar:
			// try function args
			if( FindFuncArg(_this, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try local vars
			if( FindLocalVar(_this, pName) )
				return JCL_ERR_Identifier_Already_Defined;
			// try global variables
			if( FindGlobalVar(_this, classIdx, pName) )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Global);
			// try global functions
			if( IsFuncInGlobalScope(_this, pName) )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Global);
			// try member variables
			if( FindMemberVar(_this, classIdx, pName) )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Member);
			// try member functions
			FindFunction(_this, classIdx, pName, 0, &pFunc);
			if( pFunc )
				EmitWarning(_this, pName, JCL_WARN_Local_Hides_Member);
			// try classes
			FindClass(_this, pName, &pClass);
			if( pClass )
				return JCL_ERR_Identifier_Already_Defined;
			break;
	}
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// IsAccessorUsed
//------------------------------------------------------------------------------

static JILError IsAccessorUsed(JCLState* _this, JILLong typeID, JCLFunc* pFunc)
{
	JILError err = JCL_No_Error;
	JCLFunc* pFunc2;
	JCLVar* pVar;

	pVar = NEW(JCLVar);
	pVar->miType = type_var;
	if( pFunc->mipResult->miMode == kModeUnused )
	{
		if( FindSetAccessor(_this, typeID, pFunc->mipName, pVar, &pFunc2) )
		{
			JCLVar* src = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
			JCLVar* dst = pFunc2->mipArgs->Get(pFunc2->mipArgs, 0);
			if( !EqualTypes(src, dst) )
				err = JCL_ERR_Identifier_Already_Defined;
		}
	}
	else
	{
		if( FindGetAccessor(_this, typeID, pFunc->mipName, pVar, &pFunc2) )
		{
			JCLVar* src = pFunc->mipResult;
			JCLVar* dst = pFunc2->mipResult;
			if( !EqualTypes(src, dst) )
				err = JCL_ERR_Identifier_Already_Defined;
		}
	}
	DELETE(pVar);
	return err;
}

//------------------------------------------------------------------------------
// FindGlobalVar
//------------------------------------------------------------------------------
// Find a global variable, or a global class member variable.

static JCLVar* FindGlobalVar(JCLState* _this, JILLong typeID, const JCLString* pName)
{
	int i;
	JCLClass* pClass;
	JCLVar* pVar = NULL;
	JCLString* pMangledName = NEW(JCLString);

	if( ClassDefined(_this, typeID) )
	{
		pClass = GetClass(_this, typeID);
		JCLSetString(pMangledName, JCLGetString(pClass->mipName));
		JCLAppend(pMangledName, "::");
		JCLAppend(pMangledName, JCLGetString(pName));
		if( ClassDefined(_this, type_global) )
		{
			pClass = GetClass(_this, type_global);
			// first look for a global class member constant
			for( i = 0; i < pClass->mipVars->count; i++ )
			{
				pVar = pClass->mipVars->Get(pClass->mipVars, i);
				if( JCLCompare(pVar->mipName, pMangledName) )
					break;
				pVar = NULL;
			}
			if( pVar == NULL )
			{
				// no success - then look for the global variable
				for( i = 0; i < pClass->mipVars->count; i++ )
				{
					pVar = pClass->mipVars->Get(pClass->mipVars, i);
					if( JCLCompare(pVar->mipName, pName) )
						break;
					pVar = NULL;
				}
			}
		}
	}
	DELETE( pMangledName );
	return pVar;
}

//------------------------------------------------------------------------------
// FindMemberVar
//------------------------------------------------------------------------------
// Find a member variable of a class.

static JCLVar* FindMemberVar(JCLState* _this, JILLong typeID, const JCLString* pName)
{
	int i;
	JCLClass* pClass;
	JCLVar* pVar = NULL;

	pClass = GetClass(_this, typeID);
	if( pClass )
	{
		for( i = 0; i < pClass->mipVars->count; i++ )
		{
			pVar = pClass->mipVars->Get(pClass->mipVars, i);
			if( JCLCompare(pVar->mipName, pName) )
				break;
			pVar = NULL;
		}
	}
	return pVar;
}

//------------------------------------------------------------------------------
// FindAnyVar
//------------------------------------------------------------------------------
// Find a variable. Searches first local, then members, then globals.
// Does NOT search class member variables if the currently compiled function is
// a global function.

static JCLVar* FindAnyVar(JCLState* _this, const JCLString* pName)
{
	JCLVar* pVar;

	// try local
	pVar = FindLocalVar(_this, pName);
	if( pVar )
		return pVar;

	// try this classes members
	if( CurrentFunc(_this)->miMethod )
	{
		pVar = FindMemberVar(_this, _this->miClass, pName);
		if( pVar )
			return pVar;
	}

	// try global members
	pVar = FindGlobalVar(_this, _this->miClass, pName);
	if( pVar )
		return pVar;

	// parsing function argument: try global members of class being called
	if( _this->miArgClass )
	{
		pVar = FindGlobalVar(_this, _this->miArgClass, pName);
		if( pVar )
			return pVar;
	}

	return NULL;
}

//------------------------------------------------------------------------------
// FindFuncRef
//------------------------------------------------------------------------------
// Find a global or class member function by name. This is specifically used
// when taking a reference from a function or method. Specify kFunction and/or
// kMethod flags for 'flags'.

static JILError FindFuncRef(JCLState* _this, JCLString* pName, JILLong typeID, JILLong flags, JCLVar* pResult, JCLFunc** ppFunc)
{
	JILError err = JCL_No_Error;
	JILLong start;
	JILLong numMatches;
	JILLong funcIndex;
	JCLFunc* pFunc;
	JCLFunc* pFunc2;
	JCLString* pSignature = NULL;
	JCLClass* pClass;

	// find function (count number of matches)
	start = 0;
	numMatches = 0;
	funcIndex = -1;
	pFunc = NULL;
	do
	{
		funcIndex = FindFunction(_this, typeID, pName, start, &pFunc2);
		if( pFunc2 != NULL )
		{
			pFunc = pFunc2;
			numMatches++;
		}
		start = funcIndex + 1;
	}
	while( pFunc2 != NULL );

	// check result
	if( numMatches == 0 )
		return JCL_ERR_Undefined_Identifier;
	else if( numMatches > 1 )
		return JCL_ERR_Function_Ref_Ambiguous;
	if( pFunc->miCtor || pFunc->miConvertor || pFunc->miAccessor || pFunc->miCofunc )
		return JCL_ERR_Function_Ref_Illegal;
	if( !(flags & kMethod) && pFunc->miMethod )
		return JCL_ERR_Function_Ref_Illegal;
	if( !(flags & kFunction) && !pFunc->miMethod )
		return JCL_ERR_Function_Ref_Illegal;

	// create signature from function
	pSignature = NEW(JCLString);
	err = GetSignature(_this, "D", pFunc->mipResult, pFunc->mipArgs, pSignature);
	if( err )
		goto exit;

	// find a suitable delegate type
	FindClass(_this, pSignature, &pClass);
	if( !pClass || pClass->miFamily != tf_delegate )
	{
		err = JCL_ERR_No_Suitable_Delegate;
		goto exit;
	}

	pResult->miType = pClass->miType;
	pResult->miRef = JILTrue;
	pResult->miConst = JILFalse;
	pResult->miWeak = JILFalse;
	pResult->miInited = JILTrue;
	*ppFunc = pFunc;

exit:
	DELETE( pSignature );
	return err;
}

//------------------------------------------------------------------------------
// FindAnyFuncRef
//------------------------------------------------------------------------------
// Find a global or class member function by name. Searches first in the current
// class, then in the global space, then in the classes mapped to the global
// space by the using statement.

static JILError FindAnyFuncRef(JCLState* _this, JCLString* pName, JCLVar* pResult, JCLFunc** ppFunc)
{
	JILError err = JCL_No_Error;
	JILLong i;
	JILLong typeID;
	JILLong numFound = 0;
	Array_JILLong* pUsing = GetOptions(_this)->mipUsing;

	// if not in global scope, try current class first
	if( !IsGlobalScope(_this, _this->miClass) )
	{
		err = FindFuncRef( _this, pName, _this->miClass, kMethod | kFunction, pResult, ppFunc );
		if( err != JCL_ERR_Undefined_Identifier )
			return err;	// no need to check for ambiguous access in this case

		// also try parent class
		if( HasParentType(_this, _this->miClass) )
		{
			err = FindFuncRef( _this, pName, GetParentType(_this, _this->miClass), kMethod | kFunction, pResult, ppFunc );
			if( err != JCL_ERR_Undefined_Identifier )
				return err;	// no need to check for ambiguous access in this case
		}
	}

	// try global space
	err = FindFuncRef( _this, pName, type_global, kFunction, pResult, ppFunc );
	if( err && err != JCL_ERR_Undefined_Identifier )
		return err;
	if( err == JCL_No_Error )
		numFound++;

	// try using list for this file
	for( i = 0; i < pUsing->count; i++ )
	{
		typeID = pUsing->Get(pUsing, i);
		err = FindFuncRef( _this, pName, typeID, kFunction, pResult, ppFunc );
		if( err && err != JCL_ERR_Undefined_Identifier )
			return err;
		if( err == JCL_No_Error )
			numFound++;
	}

	err = JCL_No_Error;
	if( numFound == 0 )
		return JCL_ERR_Undefined_Identifier;
	else if( numFound > 1 )
		return JCL_ERR_Function_Ref_Ambiguous;

	return err;
}

//------------------------------------------------------------------------------
// AddMemberVarEx
//------------------------------------------------------------------------------
// Add a special member variable to a class.

static JILError AddMemberVarEx(JCLState* _this, JILLong whatIsDefined, JILLong classIdx, JCLVar* pVar)
{
	JILError err = JCL_No_Error;
	JCLVar* pNewVar;
	JCLClass* pClass;

	err = IsIdentifierUsed(_this, whatIsDefined, classIdx, pVar->mipName);
	if( err )
		goto exit;

	pClass = GetClass(_this, classIdx);
	// check that class is not a native type
	if( IsClassNative(pClass) && !pVar->miConst )
		return JCL_ERR_Illegal_NTL_Variable;
	pVar->miMode = kModeMember;
	pVar->miIndex = 0;
	pVar->miMember = pClass->mipVars->count;
	pVar->miInited = JILTrue;
	pNewVar = pClass->mipVars->New(pClass->mipVars);
	pNewVar->Copy(pNewVar, pVar);

exit:
	return err;
}

//------------------------------------------------------------------------------
// AddMemberVar
//------------------------------------------------------------------------------
// Add a normal member variable to a class.

static JILError AddMemberVar(JCLState* _this, JILLong classIdx, JCLVar* pVar)
{
	return AddMemberVarEx(_this, kClassVar, classIdx, pVar);
}

//------------------------------------------------------------------------------
// IsTempVar
//------------------------------------------------------------------------------
// Checks if a var object is a temp var.

static JILBool IsTempVar(const JCLVar* pVar)
{
	return (pVar->miUsage == kUsageTemp && pVar->miMode == kModeRegister);
}

//------------------------------------------------------------------------------
// IsResultVar
//------------------------------------------------------------------------------
// Checks if a var object is the return register r1.

static JILBool IsResultVar(const JCLVar* pVar)
{
	return (pVar->miUsage == kUsageResult && pVar->miMode == kModeRegister && pVar->miIndex == 1);
}

//------------------------------------------------------------------------------
// IsArrayAccess
//------------------------------------------------------------------------------
// Checks if a var object accesses an array member

static JILBool IsArrayAccess(const JCLVar* pVar)
{
	return (pVar->miUsage == kUsageTemp && pVar->miMode == kModeArray);
}

//------------------------------------------------------------------------------
// InitMemberVars
//------------------------------------------------------------------------------
// Marks all member variables of a class as inited or not inited.

static void InitMemberVars(JCLState* _this, JILLong typeID, JILBool value)
{
	int i;
	JCLClass* pClass = GetClass(_this, typeID);
	for( i = 0; i < pClass->mipVars->count; i++ )
		pClass->mipVars->Get(pClass->mipVars, i)->miInited = value;
}

//------------------------------------------------------------------------------
// BreakBranchFixup
//------------------------------------------------------------------------------
// Fixes the branches of all break statements, as soon as the code position of
// the block-end is known.

static void BreakBranchFixup(JCLState* _this, Array_JILLong* pFix, JILLong endBlockLoc)
{
	if( pFix )
	{
		int i;
		JILLong pos;
		Array_JILLong* pCode;
		pCode = CurrentOutFunc(_this)->mipCode;
		for( i = 0; i < pFix->count; i++ )
		{
			pos = pFix->Get(pFix, i);
			pCode->Set(pCode, pos + 1, endBlockLoc - pos);
		}
	}
}

//------------------------------------------------------------------------------
// IsAssignOperator
//------------------------------------------------------------------------------

static JILBool IsAssignOperator(JILLong t)
{
	return (t == tk_assign
		||	t == tk_plus_assign
		||	t == tk_minus_assign
		||	t == tk_mul_assign
		||	t == tk_div_assign
		||	t == tk_mod_assign
		||	t == tk_band_assign
		||	t == tk_bor_assign
		||	t == tk_xor_assign
		||	t == tk_lshift_assign
		||	t == tk_rshift_assign);
}

//------------------------------------------------------------------------------
// IsSrcInited
//------------------------------------------------------------------------------
// Checks if a source var (a var from which an operation is going to read) has
// been initialized. Certain var types require that no init-check is performed.
// For example, if a member variable of an object is read, we cannot really know
// if that variable was inited, because that depends on runtime-behaviour, such
// as if and what constructor was called and if it initialized that particular
// var.

static JILBool IsSrcInited(const JCLVar* pVar)
{
	if( pVar->miUsage == kUsageResult )	// function args and results always inited
		return JILTrue;
	if(	pVar->miMode == kModeMember )	// member vars always inited
		return JILTrue;
	if(	pVar->miMode == kModeArray )	// array elements always inited
		return JILTrue;
	return pVar->miInited;
}

//------------------------------------------------------------------------------
// IsDstInited
//------------------------------------------------------------------------------
// Checks if a destination var (a var to which an operation is going to write)
// has been initialized.

static JILBool IsDstInited(const JCLVar* pVar)
{
// TODO: Check all callers of this function why they check the init-state of the variable. It may be obsolete.
// The init state of a variable should no longer matter because there is no SET instruction anymore
// R-Values are always copied by reference now, regardless whether the variable is inited or not.
	if( pVar->miUsage == kUsageResult )	// function args and results never inited
		return JILFalse;
	return pVar->miInited;
}

//------------------------------------------------------------------------------
// IsDstConst
//------------------------------------------------------------------------------
// Checks if a DESTINATION is a constant. If the var is a member var of an
// object, also checks if the object is constant.

static JILBool IsDstConst(const JCLVar* pDst)
{
	if( pDst->miConst )
	{
		return JILTrue;
	}
	// member access: check if object is const
	else if( (pDst->miMode == kModeMember || pDst->miMode == kModeArray) && pDst->miConstP )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsSrcConst
//------------------------------------------------------------------------------
// Checks if a SOURCE is a constant. If the var is a member var of an object,
// also checks if the object is constant.

static JILBool IsSrcConst(const JCLVar* pSrc)
{
	if( pSrc->miConst )
	{
		return JILTrue;
	}
	// member access: check if object is const
	else if( (pSrc->miMode == kModeMember || pSrc->miMode == kModeArray) && pSrc->miConstP )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsDstTakingRef
//------------------------------------------------------------------------------
// Checks if the destination var would take a reference from a source.

static JILBool IsDstTakingRef(const JCLVar* pDst)
{
	if( pDst->miMode == kModeArray		// special case for array elements
	&&	pDst->miElemRef )
	{
		return JILTrue;
	}
	if(	pDst->miRef	)					// is destination a reference?
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsRef
//------------------------------------------------------------------------------
// Checks if a var is a reference. This just checks how the variable's type is
// declared, it does not check, if the variable actually *WOULD* take a
// reference, if it was the destination of a move operation.
// See also: IsDstTakingRef()

static JILBool IsRef(const JCLVar* pVar)
{
	if( pVar->miMode == kModeArray		// special case for array elements
	&&	pVar->miElemRef )
	{
		return JILTrue;
	}
	else if( pVar->miRef )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsWeakRef
//------------------------------------------------------------------------------
// Checks if a var is a weak reference.

static JILBool IsWeakRef(const JCLVar* pVar)
{
	if( pVar->miRef && pVar->miWeak )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsRegisterAccess
//------------------------------------------------------------------------------
// Checks if a var is direct register access with the given register number.

static JILBool IsRegisterAccess(const JCLVar* pVar, JILLong r)
{
	return (pVar->miMode == kModeRegister && pVar->miIndex == r);
}

//------------------------------------------------------------------------------
// IsBasicType
//------------------------------------------------------------------------------
// Checks if the given token is one of the basic types of the language.
// Basic types are the built-in types namely: long, float, string, array.

static JILBool IsBasicType(JILLong tokenID)
{
	switch( tokenID )
	{
		case tk_int:
		case tk_float:
		case tk_string:
		case tk_array:
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsComparableType
//------------------------------------------------------------------------------
// Checks if the given type can be compared by the VM. Comparable types are int,
// float and string.

static JILBool IsComparableType(JILLong typeID)
{
	switch( typeID )
	{
		case type_int:
		case type_float:
		case type_string:
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsCalculatableType
//------------------------------------------------------------------------------
// Checks if the VM can use the given type in arithmetic calculations.

static JILBool IsCalculatableType(JILLong typeID)
{
	switch( typeID )
	{
		case type_int:
		case type_float:
		case type_string:
		case type_array:
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// DuplicateVar
//------------------------------------------------------------------------------
// Allocate a new JCLVar as a duplicate of an existing one.

static void DuplicateVar(JCLVar** ppVar, const JCLVar* pSrc)
{
	*ppVar = NULL;
	if( pSrc )
	{
		JCLVar* pVar = NEW(JCLVar);
		*ppVar = pVar;
		pVar->Copy(pVar, pSrc);
		if( pSrc->miMode == kModeArray )
			DuplicateVar(&(pVar->mipArrIdx), pSrc->mipArrIdx);
	}
}

//------------------------------------------------------------------------------
// FreeDuplicate
//------------------------------------------------------------------------------
// Free a duplicated var. Do not use this for vars allocated by MakeTempVar()!

static void FreeDuplicate(JCLVar** ppVar)
{
	if( *ppVar )
	{
		JCLVar* pVar = *ppVar;
		if( pVar->mipArrIdx )
			DELETE( pVar->mipArrIdx );
		DELETE( pVar );
		*ppVar = NULL;
	}
}

//------------------------------------------------------------------------------
// IsOperatorToken
//------------------------------------------------------------------------------
// Returns JILTrue if the given token is an operator token.

static JILBool IsOperatorToken(JILLong tokenID)
{
	switch( tokenID )
	{
		case tk_point:
		case tk_and:
		case tk_or:
		case tk_not:
		case tk_equ:
		case tk_not_equ:
		case tk_greater:
		case tk_greater_equ:
		case tk_less:
		case tk_less_equ:
		case tk_plus:
		case tk_minus:
		case tk_mul:
		case tk_div:
		case tk_mod:
		case tk_band:
		case tk_bor:
		case tk_xor:
		case tk_bnot:
		case tk_lshift:
		case tk_rshift:
		case tk_assign:
		case tk_plus_assign:
		case tk_minus_assign:
		case tk_mul_assign:
		case tk_div_assign:
		case tk_mod_assign:
		case tk_band_assign:
		case tk_bor_assign:
		case tk_xor_assign:
		case tk_lshift_assign:
		case tk_rshift_assign:
			return JILTrue;
		default:
			return JILFalse;
	}
}

//------------------------------------------------------------------------------
// ClassHasBody
//------------------------------------------------------------------------------
// Checks if the class declaration has a body.

static JILBool ClassHasBody(JCLState* _this, JILLong typeID)
{
	return GetClass(_this, typeID)->miHasBody;
}

//------------------------------------------------------------------------------
// AddGlobalVar
//------------------------------------------------------------------------------
// Add a global variable to the global object.

static JILError AddGlobalVar(JCLState* _this, JCLVar* pVar)
{
	JILError err = JCL_No_Error;
	JCLVar* pNewVar;
	JCLClass* pClass;

	err = IsIdentifierUsed(_this, kGlobalVar, type_global, pVar->mipName);
	if( err )
		goto exit;

	pClass = GetClass(_this, type_global);
	if( !pClass )
		return JCL_ERR_Undefined_Identifier;	// TODO: not really a good error code
	// check that class is not a native type
	if( IsClassNative(pClass) )
		return JCL_ERR_Illegal_NTL_Variable;
	pVar->miMode = kModeMember;
	pVar->miIndex = 2;
	pVar->miMember = pClass->mipVars->count;
	pVar->miInited = JILFalse;
	pNewVar = pClass->mipVars->New(pClass->mipVars);
	pNewVar->Copy(pNewVar, pVar);

exit:
	return err;
}

//------------------------------------------------------------------------------
// EqualTypes
//------------------------------------------------------------------------------
// Checks if two vars have exactly the same types

static JILBool EqualTypes(const JCLVar* pSrc, const JCLVar* pDst)
{
	if( pSrc->miType != pDst->miType )	// same base type?
		return JILFalse;
	if( pSrc->miType == type_array )
	{
		if(pSrc->miElemType != pDst->miElemType)
			return JILFalse;
	}
	return JILTrue;
}

//------------------------------------------------------------------------------
// EqualRegisters
//------------------------------------------------------------------------------
// Checks if two vars refer to the same register, to the same location on the
// stack, the same member of the same object, or the same element of the same
// array. TODO: Unused function, remove?

static JILBool EqualRegisters(const JCLVar* src, const JCLVar* dst)
{
	if( src->miMode != dst->miMode )
		return JILFalse;
	if( src->miMode == kModeRegister || src->miMode == kModeStack )
	{
		return (src->miIndex == dst->miIndex);
	}
	else if( src->miMode == kModeMember )
	{
		return (src->miIndex == dst->miIndex && src->miMember == dst->miMember);
	}
	else if( src->miMode == kModeArray )
	{
		return (src->miIndex == dst->miIndex && src->mipArrIdx->miIndex == dst->mipArrIdx->miIndex);
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// ImpConvertible
//------------------------------------------------------------------------------
// Checks if two vars have the same type, or are IMPLICITLY CONVERTIBLE, which
// means they have types that are related in a way that no code needs to be
// generated to convert them from source to destination.
// Types are implicitly convertible if source or destination is type_var, or if
// the destination is the interface of the source class.

static JILBool ImpConvertible(JCLState* _this, JCLVar* pSrc, JCLVar* pDst)
{
	if( pSrc->miType == pDst->miType )
	{
		if( pSrc->miType != type_array )
			return JILTrue;
		return (pSrc->miElemType == pDst->miElemType
			|| pSrc->miElemType == type_var
			|| pDst->miElemType == type_var);
	}
	else if( pSrc->miType == type_var || pDst->miType == type_var )
	{
		return JILTrue;
	}
	else if( pSrc->miType == type_array )
	{
		// allow us to put a sub array into a typed array
		return (pDst->miMode == kModeArray && (pSrc->miElemType == pDst->miType || pSrc->miElemType == type_var));
	}
	else if( pDst->miType == type_array )
	{
		// allow us to get a sub array from a typed array
		return (pSrc->miMode == kModeArray && (pDst->miElemType == pSrc->miType || pDst->miElemType == type_var));
	}
	else if( pSrc->miType == type_delegate )
	{
		// allow us to convert a generic delegate into a typed delegate
		return (TypeFamily(_this, pDst->miType) == tf_delegate);
	}
	else if( IsVarClassType(_this, pSrc) && IsVarClassType(_this, pDst) )
	{
		return IsSubClass(_this, pSrc->miType, pDst->miType);
	}
	else
	{
		return JILFalse;
	}
}

//------------------------------------------------------------------------------
// DynConvertible
//------------------------------------------------------------------------------
// Checks if there is *ANY* way of converting from the source to the destination
// type. This includes the possibility of code generation to call constructor
// or convertor methods of a class.

static JILBool DynConvertible(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JCLFunc* dummy;
	if( ImpConvertible(_this, src, dst) )
	{
		return JILTrue;
	}
	else if( src->miType == type_int && dst->miType == type_float )
	{
		return JILTrue;
	}
	else if( src->miType == type_float && dst->miType == type_int )
	{
		return JILTrue;
	}
	else if( IsVarClassType(_this, src) )
	{
		if( IsSubClass(_this, dst->miType, src->miType) )
		{
			return JILTrue;
		}
		else
		{
			if( FindConvertor(_this, src, dst, &dummy) == JCL_No_Error )
				return JILTrue;
			if( FindConstructor(_this, src, dst, &dummy) == JCL_No_Error )
				return JILTrue;
		}
	}
	else if( IsVarClassType(_this, dst) )
	{
		if( FindConstructor(_this, src, dst, &dummy) == JCL_No_Error )
			return JILTrue;
		if( FindConvertor(_this, src, dst, &dummy) == JCL_No_Error )
			return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// AllMembersInited
//------------------------------------------------------------------------------
// Checks all member variables of a class whether they are inited. If an
// uninitialized member variable is found, it's name is copied to pArg.

static JILBool AllMembersInited(JCLState* _this, JILLong typeID, JCLString* pArg)
{
	int i;
	JCLVar* pVar;
	Array_JCLVar* pVars = GetClass(_this, typeID)->mipVars;
	for( i = 0; i < pVars->count; i++ )
	{
		if( !pVars->Get(pVars, i)->miInited )
		{
			pVar = pVars->Get(pVars, i);
			pVar->ToString(pVar, _this, pArg, kClearFirst|kIdentNames|kCurrentScope);
			return JILFalse;
		}
	}
	return JILTrue;
}

//------------------------------------------------------------------------------
// PushImport
//------------------------------------------------------------------------------
// Push a new import file onto the stack of open imports

static JCLFile* PushImport(JCLState* _this, const JCLString* pClassName, const JCLString* pText, const JCLString* pPath, JILBool native)
{
	JCLFile* pImport = _this->mipImportStack->New(_this->mipImportStack);
	pImport->Open(pImport, JCLGetString(pClassName), JCLGetString(pText), JCLGetString(pPath));
	pImport->miNative = native;
	return pImport;
}

//------------------------------------------------------------------------------
// PopImport
//------------------------------------------------------------------------------
// JCLRemove the topmost import file from the stack of open imports

static void PopImport(JCLState* _this)
{
	long num = _this->mipImportStack->count;
	if( num )
		_this->mipImportStack->Trunc(_this->mipImportStack, num - 1);
}

//------------------------------------------------------------------------------
// ClearImportStack
//------------------------------------------------------------------------------
// JCLRemove all class names of imports from the stack of open imports

void ClearImportStack(JCLState* _this)
{
	_this->mipImportStack->Trunc(_this->mipImportStack, 0);
}

//------------------------------------------------------------------------------
// FindImport
//------------------------------------------------------------------------------
// Check whether the given class name is already on the stack of open imports
// and return the address of the file object. If the file object was not found,
// returns NULL.

static JCLFile* FindImport(JCLState* _this, const JCLString* pString)
{
	int i;
	JCLFile* pImport;
	for( i = 0; i < _this->mipImportStack->count; i++ )
	{
		pImport = _this->mipImportStack->Get(_this->mipImportStack, i);
		if( JCLCompare(pImport->mipName, pString) )
			return pImport;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// IsArithmeticAssign
//------------------------------------------------------------------------------

static JILBool IsArithmeticAssign(JILLong t)
{
	return (t == tk_plus_assign
		||	t == tk_minus_assign
		||	t == tk_mul_assign
		||	t == tk_div_assign
		||	t == tk_mod_assign);
}

//------------------------------------------------------------------------------
// CheckTypeConflict
//------------------------------------------------------------------------------
// check for var / const / ref conflicts in function argument / result type
// declaration.

static JILError CheckTypeConflict(const JCLVar* src, const JCLVar* dst)
{
	if( (dst->miType == type_var || src->miType == type_var)
		&& dst->miType != src->miType )
	{
		return JCL_ERR_Typeless_Arg_Conflict;
	}
	else if( dst->miType == type_array && dst->miElemRef != src->miElemRef )
	{
		return JCL_ERR_Ref_Arg_Conflict;
	}
	else if( dst->miConst != src->miConst )
	{
		return JCL_ERR_Const_Arg_Conflict;
	}
	else if( dst->miRef != src->miRef )
	{
		return JCL_ERR_Ref_Arg_Conflict;
	}
	else if( dst->miWeak != src->miWeak )
	{
		return JCL_ERR_WRef_Arg_Conflict;
	}
	else if( !EqualTypes(src, dst) )
	{
		return JCL_ERR_Arg_Type_Conflict;	// can happen if one type inherits from the other
	}
	return JCL_No_Error;
}

//------------------------------------------------------------------------------
// IsVarClassType
//------------------------------------------------------------------------------
// Checks if the given var is of type 'class'.

static JILBool IsVarClassType(JCLState* _this, const JCLVar* pVar)
{
	return IsClassType(_this, pVar->miType);
}

//------------------------------------------------------------------------------
// IsClassType
//------------------------------------------------------------------------------
// Checks if a given typeID belongs to a class or interface.

static JILBool IsClassType(JCLState* _this, JILLong type)
{
	if( ClassDefined(_this, type) )
	{
		JILLong tf = GetClass(_this, type)->miFamily;
		return (tf == tf_class || tf == tf_interface);
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsInterfaceType
//------------------------------------------------------------------------------
// Checks if a given typeID belongs to an interface.

static JILBool IsInterfaceType(JCLState* _this, JILLong type)
{
	if( ClassDefined(_this, type) )
	{
		JILLong tf = GetClass(_this, type)->miFamily;
		return (tf == tf_interface);
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsValueType
//------------------------------------------------------------------------------
// Checks if a given typeID belongs to a value-type (not copied by reference).
// Value-types in JewelScript are currently int and float.

static JILBool IsValueType(JCLState* _this, JILLong type)
{
	return (type == type_int || type == type_float);
}

//------------------------------------------------------------------------------
// IsTypeCopyable
//------------------------------------------------------------------------------
// Checks if the given type can be copied by the runtime. An int, float or
// script object can always be copied, native types can only be copied if they
// define a copy-constructor.

static JILBool IsTypeCopyable(JCLState* _this, JILLong type)
{
	switch (type)
	{
		case type_null:
		case type_int:
		case type_float:
		case type_string:
		case type_array:
		case type_var:
			return JILTrue;
		default:
		{
			JCLClass* pClass = GetClass(_this, type);
			switch (pClass->miFamily)
			{
				case tf_integral:	return JILTrue;
				case tf_thread:		return JILFalse;
				case tf_interface:	return JILFalse;
				case tf_delegate:	return JILTrue;
				case tf_class:
					if( IsClassNative(pClass) )
						return (pClass->miMethodInfo.cctor != -1);
					else
						return JILTrue;
			}
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsGlobalScope
//------------------------------------------------------------------------------
// Checks if a given typeID represents the global scope

static JILBool IsGlobalScope(JCLState* pState, JILLong typeID)
{
	return (typeID == type_global);
}

//------------------------------------------------------------------------------
// IsClassToken
//------------------------------------------------------------------------------
// Checks if the given token is allowed as a class name.

static JILBool IsClassToken	(JILLong token)
{
	return (token == tk_identifier || token == tk_string || token == tk_array);
}

//------------------------------------------------------------------------------
// IsTypeName
//------------------------------------------------------------------------------
// Writes the type info from the given token into pOut and returns JILTrue.
// If the given token is not a known type name returns JILFalse.

static JILBool IsTypeName(JCLState* _this, JILLong token, const JCLString* pName, TypeInfo* pOut)
{
	JILBool success = JILTrue;
	switch( token )
	{
		case tk_null:
			JCLSetTypeInfo(pOut, type_null, JILFalse, JILFalse, JILFalse, type_null, JILFalse);
			break;
		case tk_int:
			JCLSetTypeInfo(pOut, type_int, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
			break;
		case tk_float:
			JCLSetTypeInfo(pOut, type_float, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
			break;
		case tk_string:
		case tk_array:
		case tk_identifier:
		{
			// check name
			JILLong type = StringToType(_this, pName, token);
			if( type != type_null )
				JCLSetTypeInfo(pOut, type, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
			else
				success = JILFalse;
			break;
		}
		case tk_var:
			JCLSetTypeInfo(pOut, type_var, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
			break;
		default:
			success = JILFalse;
			break;
	}
	return success;
}

//------------------------------------------------------------------------------
// IsSuperClass
//------------------------------------------------------------------------------
// Checks if type1 is a super-class of type2. In other words, checks if type2
// has inherited type1.

static JILBool IsSuperClass(JCLState* _this, JILLong type1, JILLong type2)
{
	return IsSubClass(_this, type2, type1);
}

//------------------------------------------------------------------------------
// IsSubClass
//------------------------------------------------------------------------------
// Checks if type1 is a sub-class of type2. In other words, checks if type1
// has inherited type2.

static JILBool IsSubClass(JCLState* _this, JILLong type1, JILLong type2)
{
	if( IsClassType(_this, type1) && IsClassType(_this, type2) )
	{
		JCLClass* pClass = GetClass(_this, type1);
		if( pClass )
		{
			if( pClass->miBaseType == type2 )
				return JILTrue;
		}
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// IsModifierNativeBinding
//------------------------------------------------------------------------------
// Returns true if the specified class has been declared using the 'native' modifier.

static JILBool IsModifierNativeBinding(JCLClass* pClass)
{
	return ((pClass->miModifier & kModiNativeBinding) == kModiNativeBinding);
}

//------------------------------------------------------------------------------
// IsModifierNativeInterface
//------------------------------------------------------------------------------
// Returns true if the specified interface has been declared using the 'native' modifier.

static JILBool IsModifierNativeInterface(JCLClass* pClass)
{
	return ((pClass->miModifier & kModiNativeInterface) == kModiNativeInterface);
}

//------------------------------------------------------------------------------
// IsClassNative
//------------------------------------------------------------------------------
// Returns true if the specified class is a native type library or has been
// declared using the 'native' modifier. If this returns false, the class is
// implemented, or can be implemented, in script code.

static JILBool IsClassNative(JCLClass* pClass)
{
	return (pClass->miNative || IsModifierNativeBinding(pClass));
}

//------------------------------------------------------------------------------
// TypeFamily
//------------------------------------------------------------------------------
// Return the type family of a given type.

JILLong TypeFamily(JCLState* _this, JILLong type)
{
	if( ClassDefined(_this, type) )
	{
		JCLClass* pClass = GetClass(_this, type);
		return pClass->miFamily;
	}
	return tf_undefined;
}

//------------------------------------------------------------------------------
// GetGlobalOptions
//------------------------------------------------------------------------------
// Get the GLOBAL compiler options.

JCLOption* GetGlobalOptions(JCLState* _this)
{
	return _this->mipOptionStack->Get(_this->mipOptionStack, 0);
}

//------------------------------------------------------------------------------
// GetOptions
//------------------------------------------------------------------------------
// Get the current compiler options

JCLOption* GetOptions(JCLState* _this)
{
	JILLong index = _this->mipOptionStack->count - 1;
	if( index < 0 )
		return NULL;
	return _this->mipOptionStack->Get(_this->mipOptionStack, index);
}

//------------------------------------------------------------------------------
// PushOptions
//------------------------------------------------------------------------------
// Push a new set of options onto the stack. The new set will be initialized
// from the GLOBAL compiler options.

static void PushOptions(JCLState* _this)
{
	JCLOption* opt = _this->mipOptionStack->New(_this->mipOptionStack);
	opt->Copy(opt, GetGlobalOptions(_this));
}

//------------------------------------------------------------------------------
// PopOptions
//------------------------------------------------------------------------------
// Pop a set of options from the stack.

static void PopOptions(JCLState* _this)
{
	JILLong count = _this->mipOptionStack->count;
	if( count > 1 )
	{
		_this->mipOptionStack->Trunc(_this->mipOptionStack, count - 1);
	}
}

//------------------------------------------------------------------------------
// StringToType
//------------------------------------------------------------------------------
// Helper function for IsFullTypeDecl

static JILLong StringToType(JCLState* _this, const JCLString* pToken, JILLong tokenID)
{
	JILLong type = type_null;
	switch( tokenID )
	{
		case tk_int:
			type = type_int;
			break;
		case tk_float:
			type = type_float;
			break;
		case tk_var:
			type = type_var;
			break;
		case tk_string:
			type = type_string;
			break;
		case tk_array:
			type = type_array;
			break;
		case tk_identifier:
		{
			JCLClass* pClass;
			// check for mangled type in current class first
			if( !IsGlobalScope(_this, _this->miClass) )
			{
				JCLClass* pCurClass = CurrentClass(_this);
				JCLString* pName = NEW(JCLString);
				JCLSetString(pName, JCLGetString(pCurClass->mipName));
				JCLAppend(pName, "::");
				JCLAppend(pName, JCLGetString(pToken));
				FindClass(_this, pName, &pClass);
				if( pClass )
				{
					type = pClass->miType;
				}
				else if( HasParentType(_this, _this->miClass) )
				{
					// check inside parent class too
					pClass = GetClass(_this, GetParentType(_this, _this->miClass));
					JCLSetString(pName, JCLGetString(pClass->mipName));
					JCLAppend(pName, "::");
					JCLAppend(pName, JCLGetString(pToken));
					FindClass(_this, pName, &pClass);
					if( pClass )
					{
						type = pClass->miType;
					}
				}
				DELETE(pName);
			}
			// if not found, check for unmangled type
			if( type == type_null )
			{
				FindClass(_this, pToken, &pClass);
				if( pClass )
					type = pClass->miType;
			}
			break;
		}
	}
	return type;
}

//------------------------------------------------------------------------------
// IsFullTypeDecl
//------------------------------------------------------------------------------
// Checks for a full type declaration and setup the given JCLVar.
// If 'bResult' is true, the caller does not expect the declaration to include
// an identifier name (a result type declaration is expected)
// NOTE: This function will return JCL_ERR_No_Type_Declaration if the tokens
// read do not seem to belong to a type declaration at all. A calling function
// that calls IsFullTypeDecl() to TEST whether there is a type declaration, should
// only proceed if this function returns JCL_No_Error (means yes, there is a
// type decl.) or JCL_ERR_No_Type_Declaration (means no, there is no type
// declaration). On all other error codes the caller should correctly handle the
// error returned!

static JILError IsFullTypeDecl(JCLState* _this, JCLString* pToken, JCLVar* pVar, JILBool bResult)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JILLong type1 = 0, type2 = 0, ref1 = 0, ref2 = 0;
	JILLong savePos, nextPos;

	// re-init given var
	pVar->Destroy(pVar);
	pVar->Create(pVar);

	pFile = _this->mipFile;

	err = pFile->GetToken(pFile, pToken, &tokenID);
	if( err )
		goto exit;
	// check for const
	if( tokenID == tk_const )
	{
		pVar->miConst = JILTrue;
		err = pFile->GetToken(pFile, pToken, &tokenID);
		if( err )
			goto exit;
	}
	// check for weak
	if( tokenID == tk_weak )
	{
		pVar->miWeak = JILTrue;
		err = pFile->GetToken(pFile, pToken, &tokenID);
		if( err )
			goto exit;
	}
	// check for type identifier
	type1 = StringToType(_this, pToken, tokenID);
	if( type1 == type_null )
	{
		err = JCL_ERR_No_Type_Declaration;
		goto exit;
	}
	// check if there is a "(" or "::" behind the identifier
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	if( err )
		goto exit;
	nextPos = pFile->GetLocator(pFile);
	pFile->SetLocator(pFile, savePos);
	if( tokenID == tk_round_open || tokenID == tk_scope )
	{
		// looks like we ran into a function call or something
		err = JCL_ERR_No_Type_Declaration;
		goto exit;
	}
	// check for "array"
	if( tokenID == tk_array )
	{
		type2 = type_array;
		// skip the "array" and read ahead
		pFile->SetLocator(pFile, nextPos);
		savePos = nextPos;
	}
	else if( tokenID == tk_square_open )
	{
		// skip the "[" and read ahead
		pFile->SetLocator(pFile, nextPos);
		savePos = nextPos;
		// check for "]"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		if( err )
			goto exit;
		nextPos = pFile->GetLocator(pFile);
		pFile->SetLocator(pFile, savePos);
		if( tokenID == tk_square_close )
		{
			type2 = type_array;
			// skip it!
			pFile->SetLocator(pFile, nextPos);
		}
		else	// not a "]"
		{
			err = JCL_ERR_No_Type_Declaration;
			goto exit;
		}
	}

	if( bResult )
	{
		// result is always r1
		pVar->miMode = kModeRegister;
		pVar->miIndex = 1;
	}
	else
	{
		// get identifier name
		err = pFile->GetToken(pFile, pToken, &tokenID);
		if( err )
			goto exit;
		if( tokenID != tk_identifier )
		{
			// this is really an error
			err = JCL_ERR_Unexpected_Token;
			goto exit;
		}
		pVar->mipName->Copy(pVar->mipName, pToken);
	}
	//
	// now look at what we have got and verify it makes sense
	//
	ref1 = !IsValueType(_this, type1);
	if( pVar->miConst && TypeFamily(_this, type1) == tf_thread )
	{
		err = JCL_ERR_Const_Thread_Error;
		goto exit;
	}
	if( type2 == type_array )
	{
		ref2 = JILTrue;
		pVar->miType = type2;
		pVar->miRef = ref2;
		pVar->miElemType = type1;
		pVar->miElemRef = ref1;
		if( type1 == type_array )
		{
			err = JCL_ERR_Array_Array;
			goto exit;
		}
	}
	else
	{
		pVar->miType = type1;
		pVar->miRef = ref1;
		if( type1 == type_array )
		{
			pVar->miElemType = type_var;
			pVar->miElemRef = ref1;
		}
		else
		{
			pVar->miElemType = type_var;
			pVar->miElemRef = (type1 == type_var);
		}
		if( !ref1 && pVar->miWeak )
		{
			err = JCL_ERR_Weak_Without_Ref;
			goto exit;
		}
	}
	return err;

exit:
	// clear the var
	pVar->Destroy(pVar);
	pVar->Create(pVar);
	return err;
}

//------------------------------------------------------------------------------
// CreateCofunction
//------------------------------------------------------------------------------
// Helper function that creates a new cofunction.

static JILError CreateCofunction(JCLState* _this, JCLVar* pResVar, Array_JCLVar* pArgs, JILLong* pType)
{
	JILError err = JCL_No_Error;
	JCLString* pSignature;
	JILLong typeID;
	JCLClass* pClass;
	JCLFuncType* pFuncType;

	*pType = type_null;
	pSignature = NEW(JCLString);
	GetSignature(_this, "C", pResVar, pArgs, pSignature);
	err = IsIdentifierUsed(_this, kGlobalCofunc, _this->miClass, pSignature);
	if( err == JCL_ERR_Identifier_Already_Defined )
	{
		FindClass(_this, pSignature, &pClass);
		if( pClass->miFamily != tf_thread )
			return err;
		err = JCL_No_Error;
		typeID = pClass->miType;
		pClass->miParentType = _this->miClass;	// HACK: Since all cofunctions share the same class, we must override the parent typeID here
	}
	else
	{
		err = JCLCreateType(_this, JCLGetString(pSignature), _this->miClass, tf_thread, JILFalse, &typeID);
		if( err )
			return err;
	}
	pClass = GetClass(_this, typeID);
	pFuncType = pClass->mipFuncType;
	pFuncType->mipResult->Copy(pFuncType->mipResult, pResVar);
	pFuncType->mipArgs->Copy(pFuncType->mipArgs, pArgs);
	*pType = typeID;

	DELETE( pSignature );
	return err;
}

//------------------------------------------------------------------------------
// CreateDelegate
//------------------------------------------------------------------------------
// Helper function that creates a new delegate.

static JILError CreateDelegate(JCLState* _this, JCLVar* pResVar, Array_JCLVar* pArgs, JILLong* pType)
{
	JILError err = JCL_No_Error;
	JCLString* pSignature;
	JILLong typeID;
	JCLClass* pClass;
	JCLFuncType* pFuncType;

	*pType = type_null;
	pSignature = NEW(JCLString);
	GetSignature(_this, "D", pResVar, pArgs, pSignature);
	err = IsIdentifierUsed(_this, kGlobalDelegate, _this->miClass, pSignature);
	if( err == JCL_ERR_Identifier_Already_Defined )
	{
		FindClass(_this, pSignature, &pClass);
		if( pClass->miFamily != tf_delegate )
			return err;
		err = JCL_No_Error;
		typeID = pClass->miType;
	}
	else
	{
		err = JCLCreateType(_this, JCLGetString(pSignature), _this->miClass, tf_delegate, JILFalse, &typeID);
		if( err )
			return err;
	}
	pClass = GetClass(_this, typeID);
	pFuncType = pClass->mipFuncType;
	pFuncType->mipResult->Copy(pFuncType->mipResult, pResVar);
	pFuncType->mipArgs->Copy(pFuncType->mipArgs, pArgs);
	*pType = typeID;

	DELETE( pSignature );
	return err;
}

//------------------------------------------------------------------------------
// AddAlias
//------------------------------------------------------------------------------
// Add an alias name to a class (type).

static JILError AddAlias(JCLState* _this, JCLString* pName, JILLong typeID)
{
	JILError err = JCL_No_Error;
	JCLClass* pClass;
	JCLString* pNew;

	// check if the name is already in use
	err = IsIdentifierUsed(_this, kGlobalAlias, type_global, pName);
	if( err )
		goto exit;

	// add the alias to the type
	pClass = GetClass(_this, typeID);
	pNew = pClass->mipAlias->New(pClass->mipAlias);
	JCLSetString(pNew, JCLGetString(pName));

exit:
	return err;
}

//------------------------------------------------------------------------------
// GetSignature
//------------------------------------------------------------------------------
// Create a string representation of the result type and argument types of a
// function.

static void GetSignatureFromVar(JCLVar* pVar, JCLString* pResult)
{
	JCLString* pStr = NEW(JCLString);
	JCLAppend(pResult, "_");
	if( pVar->miConst )
		JCLAppend(pResult, "C");
	if( pVar->miWeak )
		JCLAppend(pResult, "W");
	if( pVar->miRef )
		JCLAppend(pResult, "R");
	JCLFormat(pStr, "%d", pVar->miType);
	JCLAppend(pResult, JCLGetString(pStr));
	if( pVar->miType == type_array )
	{
		JCLAppend(pResult, "@");
		if( pVar->miElemRef )
			JCLAppend(pResult, "R");
		JCLFormat(pStr, "%d", pVar->miElemType);
		JCLAppend(pResult, JCLGetString(pStr));
	}
	DELETE(pStr);
}

static JILError GetSignature(JCLState* _this, const JILChar* prefix, JCLVar* pResVar, Array_JCLVar* pArgs, JCLString* pResult)
{
	JILError err = JCL_No_Error;
	JILLong i;

	JCLSetString(pResult, prefix);
	GetSignatureFromVar(pResVar, pResult);
	for( i = 0; i < pArgs->count; i++ )
	{
		GetSignatureFromVar(pArgs->Get(pArgs, i), pResult);
	}

	return err;
}

//------------------------------------------------------------------------------
// CreateInitState
//------------------------------------------------------------------------------
// Allocate an init state struct for all member variables of a given class.

static void CreateInitState(SInitState* _this, JCLState* pCompiler)
{
	JCLClass* pClass;
	JILLong typeID = pCompiler->miClass;
	if( IsClassType(pCompiler, typeID) )
	{
		_this->miType = typeID;
		pClass = GetClass(pCompiler, typeID);
		_this->mipInited = (JILBool*) malloc(pClass->mipVars->count * sizeof(JILBool));
		memset(_this->mipInited, 0, pClass->mipVars->count * sizeof(JILBool));
	}
	else
	{
		_this->miType = 0;
		_this->mipInited = NULL;
	}
	_this->mipCompiler = pCompiler;
	_this->miRetFlag = JILFalse;
}

//------------------------------------------------------------------------------
// SaveInitState
//------------------------------------------------------------------------------
// Save the init state flags of all member variables of a given class to an
// init state struct.

static void SaveInitState(SInitState* _this)
{
	JCLClass* pClass;
	JILLong i;
	if( _this->miType )
	{
		pClass = GetClass(_this->mipCompiler, _this->miType);
		for( i = 0; i < pClass->mipVars->count; i++ )
			_this->mipInited[i] = pClass->mipVars->Get(pClass->mipVars, i)->miInited;
	}
	_this->miRetFlag = CurrentFunc(_this->mipCompiler)->miRetFlag;
}

//------------------------------------------------------------------------------
// RestoreInitState
//------------------------------------------------------------------------------
// Write all init state flags from an init state struct back to their member
// variables.

static void RestoreInitState(const SInitState* _this)
{
	JCLClass* pClass;
	JILLong i;
	if( _this->miType )
	{
		pClass = GetClass(_this->mipCompiler, _this->miType);
		for( i = 0; i < pClass->mipVars->count; i++ )
			pClass->mipVars->Get(pClass->mipVars, i)->miInited = _this->mipInited[i];
	}
	CurrentFunc(_this->mipCompiler)->miRetFlag = _this->miRetFlag;
}

//------------------------------------------------------------------------------
// AndInitState
//------------------------------------------------------------------------------
// Combine an init state struct by the boolean AND operation with each member
// variable in the class.

static void AndInitState(const SInitState* _this)
{
	JCLClass* pClass;
	JILLong i;
	if( _this->miType )
	{
		pClass = GetClass(_this->mipCompiler, _this->miType);
		for( i = 0; i < pClass->mipVars->count; i++ )
			pClass->mipVars->Get(pClass->mipVars, i)->miInited &= _this->mipInited[i];
	}
	CurrentFunc(_this->mipCompiler)->miRetFlag &= _this->miRetFlag;
}

//------------------------------------------------------------------------------
// SetInitState
//------------------------------------------------------------------------------
// Set all init state flags in an init state struct to a discrete value.

static void SetInitState(SInitState* _this, JILBool flag)
{
	JCLClass* pClass;
	JILLong i;
	if( _this->miType )
	{
		pClass = GetClass(_this->mipCompiler, _this->miType);
		for( i = 0; i < pClass->mipVars->count; i++ )
			_this->mipInited[i] = flag;
	}
	_this->miRetFlag = flag;
}

//------------------------------------------------------------------------------
// FreeInitState
//------------------------------------------------------------------------------
// Free an init state struct.

static void FreeInitState(SInitState* _this)
{
	if( _this->miType )
	{
		free(_this->mipInited);
		_this->mipInited = NULL;
		_this->miType = 0;
	}
	_this->mipCompiler = NULL;
	_this->miRetFlag = JILFalse;
}

//------------------------------------------------------------------------------
// IsMethodInherited
//------------------------------------------------------------------------------
// Checks if the given method index of the given class belongs to a class or
// interface that was inherited.

JILBool IsMethodInherited(JCLState* _this, JILLong typeID, JILLong fn)
{
	JCLClass* pClass = GetClass(_this, typeID);
	if( pClass && fn < pClass->mipFuncs->count )
	{
		if( pClass->miBaseType )
		{
			pClass = GetClass(_this, pClass->miBaseType);
			if( pClass && fn < pClass->mipFuncs->count )
				return JILTrue;
		}
	}
	return JILFalse;
}

/******************************************************************************/
/************************** Parsing Functions *********************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// p_compile
//------------------------------------------------------------------------------
// This is called from external modules to begin compilation of a file or
// code snippet.

JILError p_compile(JCLState* _this, JILLong pass)
{
	JILError err = JCL_No_Error;
	_this->miPass = pass;
	if( pass == kPassCompile )
	{
		// mark global class as fully declared
		if( ClassDefined(_this, type_global) )
			GetClass(_this, type_global)->miHasBody = JILTrue;
	}
	err = p_root(_this);
	// flush any errors and warnings
	FlushErrorsAndWarnings(_this);
	return err;
}

//------------------------------------------------------------------------------
// p_root
//------------------------------------------------------------------------------
// parse root level of a file

static JILError p_root(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JCLVar* pVar;
	JILBool isCompound;

	// allocate work string
	pToken = NEW(JCLString);
	pVar = NEW(JCLVar);

	// Get the file from the state
	pFile = _this->mipFile;

	// check for a starting {
	isCompound = JILTrue;
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_curly_open )
	{
		pFile->SetLocator(pFile, savePos);
		isCompound = JILFalse;
	}

	while( err == JCL_No_Error )
	{
		// get a token
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		if( err == JCL_ERR_End_Of_File || (isCompound && tokenID == tk_curly_close) )
		{
			err = JCL_No_Error;
			break;
		}
		ERROR_IF(err, err, pToken, goto exit);
		// at root level, we expect to find a function or class keyword
		switch( tokenID )
		{
			case tk_class:
				err = p_class( _this, 0 );
				break;
			case tk_interface:
				err = p_interface( _this, 0 );
				break;
			case tk_function:
				err = p_function( _this, kFunction, JILFalse );
				break;
			case tk_cofunction:
				err = p_function( _this, kFunction | kCofunction, JILFalse );
				break;
			case tk_method:
				err = p_function( _this, kMethod, JILFalse );
				break;
			case tk_accessor:
				err = p_function( _this, kMethod | kAccessor, JILFalse );
				break;
			case tk_explicit:
				err = p_function( _this, kMethod | kExplicit, JILFalse );
				break;
			case tk_import:
				err = p_import( _this );
				break;
			case tk_option:
				err = p_option( _this );
				break;
			case tk_using:
				err = p_using( _this );
				break;
			case tk_extern:
				err = p_class_modifier( _this, kModiExtern );
				break;
			case tk_native:
				err = p_class_modifier( _this, kModiNativeBinding );
				break;
			case tk_delegate:
				err = p_delegate( _this );
				break;
			case tk_alias:
				err = p_alias( _this );
				break;
			case tk_strict:
				err = p_strict( _this );
				break;
			case tk__selftest:
				err = p_selftest( _this, NULL );
				break;
			default:
			{
				pFile->SetLocator(pFile, savePos);
				// check for type declaration
				err = IsFullTypeDecl(_this, pToken, pVar, JILFalse);
				if( err == JCL_ERR_No_Type_Declaration )
					err = JCL_ERR_Unexpected_Token;
				ERROR_IF(err, err, pToken, goto exit);
				if( _this->miPass == kPassPrecompile )
				{
					err = p_global_decl(_this, pVar, NULL);
				}
				else if( _this->miPass == kPassCompile )
				{
					err = p_skip_statement(_this);
				}
				break;
			}
		}
		if( err )
			goto exit;
		// compile any function literals in the global space
		err = p_sub_functions(_this);
		if( err )
			goto exit;
	}

exit:
	DELETE( pToken );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_class
//------------------------------------------------------------------------------
// parse a class declaration

static JILError p_class(JCLState* _this, JILLong modifier)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLClass* pClass;
	JCLVar* pVar;
	JCLString* pToken;
	JCLString* pClassName;
	JCLString* pIfaceName;
	JILLong tokenID;
	JILLong classToken;
	JILLong classIdx;
	JILLong savePos;
	JILLong strict;

	pVar = NEW(JCLVar);
	pClassName = NEW(JCLString);
	pIfaceName = NEW(JCLString);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	strict = (modifier & kModiStrict) ? kStrict : 0;

	// we expect to find a class name
	err = pFile->GetToken(pFile, pClassName, &classToken);
	ERROR_IF(err, err, pClassName, goto exit);
	ERROR_IF(!IsClassToken(classToken), JCL_ERR_Unexpected_Token, pClassName, goto exit);

	// peek for ';'
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// try to find the class
	FindClass(_this, pClassName, &pClass);
	// see if name is already taken
	if( _this->miPass == kPassPrecompile )
	{
		err = IsIdentifierUsed(_this, kGlobalClass, type_global, pClassName);
		if( err && pClass )
		{
			// if the class exists but has a body, this is an error
			if( pClass->miFamily == tf_class && (tokenID == tk_semicolon || !pClass->miHasBody) )
				err = JCL_No_Error;
			else if( pClass->miFamily == tf_interface )
				err = JCL_ERR_Mixing_Class_And_Interface;
			else if( pClass->miFamily != tf_class )
				err = JCL_ERR_Type_Not_Class;
		}
		ERROR_IF(err, err, pClassName, goto exit);
	}
	if( pClass )
	{
		classIdx = pClass->miType;
		ERROR_IF(pClass->miModifier != modifier, JCL_ERR_Class_Modifier_Conflict, pClassName, goto exit);
	}
	else
	{
		JILBool bNative;
		// check if class is a known NTL, or the native modifier was specified
		bNative = (JILGetNativeType(_this->mipMachine, JCLGetString(pClassName)) != NULL);
		// create a new type
		err = JCLCreateType(_this, JCLGetString(pClassName), _this->miClass, tf_class, bNative, &classIdx);
		ERROR_IF(err, err, pClassName, goto exit);
		// add a new class
		pClass = GetClass(_this, classIdx);
		pClass->miModifier = modifier;
	}

	// make this the "current class"
	SetCompileContext(_this, classIdx, 0);
	pClass = CurrentClass(_this);

	// check if inheriting from class or interface...
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_semicolon )
		goto exit;
	if( tokenID == tk_colon )
	{
		err = p_class_inherit(_this, pClass);
		if( err )
			goto exit;
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	if( tokenID == tk_hybrid )
	{
		err = p_class_hybrid(_this, pClass);
		if( err )
			goto exit;
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	// we expect to see a "{"
	if( tokenID != tk_curly_open )
		ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
	pClass->miHasBody = JILTrue;
	// check for tag
	err = p_tag(_this, CurrentClass(_this)->mipTag);
	if( err )
		goto exit;
	// get next token
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	while( tokenID != tk_curly_close )
	{
		switch( tokenID )
		{
			case tk_function:
				err = p_function( _this, kFunction | strict, JILFalse );
				break;
			case tk_cofunction:
				err = p_function( _this, kFunction | kCofunction | strict, JILFalse );
				break;
			case tk_method:
				err = p_function( _this, kMethod | strict, JILFalse );
				break;
			case tk_accessor:
				err = p_function( _this, kMethod | kAccessor | strict, JILFalse );
				break;
			case tk_explicit:
				err = p_function( _this, kMethod | kExplicit | strict, JILFalse );
				break;
			case tk_delegate:
				err = p_delegate( _this );
				break;
			case tk_alias:
				err = p_alias( _this );
				break;
			case tk_strict:
				err = p_strict( _this );
				break;
			default:
			{
				pFile->SetLocator(pFile, savePos);
				// check for type declaration
				err = IsFullTypeDecl(_this, pToken, pVar, JILFalse);
				if( err == JCL_ERR_No_Type_Declaration )
					err = JCL_ERR_Unexpected_Token;
				ERROR_IF(err, err, pToken, goto exit);
				if( _this->miPass == kPassPrecompile )
				{
					err = p_member_decl(_this, classIdx, pVar);
				}
				else if( _this->miPass == kPassCompile )
				{
					err = p_skip_statement(_this);
				}
				break;
			}
		}
		if( err )
			goto exit;
		// get next token
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	// done with class
	if (pClass->miHasMethod || pClass->mipVars->count > 0)
		ERROR_IF(!pClass->miHasCtor && !IsClassNative(pClass), JCL_ERR_No_Constructor_Defined, pClass->mipName, goto exit);

exit:
	// MUST! go back to "global" class
	SetCompileContext(_this, type_global, 0);
	DELETE( pToken );
	DELETE( pClassName );
	DELETE( pIfaceName );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_class_modifier
//------------------------------------------------------------------------------
// Parse a class or interface with a certain class modifier keyword.

static JILError p_class_modifier(JCLState* _this, JILLong modifier)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pToken;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);

	// get next token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// expect "class" keyword
	ERROR_IF(tokenID != tk_class && tokenID != tk_interface, JCL_ERR_Unexpected_Token, pToken, goto exit);

	if( tokenID == tk_class )
		err = p_class(_this, modifier);
	else if( modifier == kModiNativeBinding )
		err = p_interface(_this, kModiNativeInterface);
	else
		err = p_interface(_this, modifier);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_class_inherit
//------------------------------------------------------------------------------
// Parse interface inheritance of a class.

static JILError p_class_inherit(JCLState* _this, JCLClass* pClass)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pIfaceName;
	JCLString* pClassName;
	JCLClass* pSrcClass;
	JCLFunc* pFunc;
	JILState* pMachine;
	JILLong i;
	JILLong classIdx;
	JILBool bStrict;
	JILTypeInfo* pTI;

	pFile = _this->mipFile;
	pIfaceName = NEW(JCLString);
	classIdx = pClass->miType;
	pClassName = pClass->mipName;
	pMachine = _this->mipMachine;
	bStrict = (pClass->miModifier & kModiStrict);

	// we expect an identifier
	err = pFile->GetToken(pFile, pIfaceName, &tokenID);
	ERROR_IF(err, err, pIfaceName, goto exit);
	ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pIfaceName, goto exit);
	FindClass(_this, pIfaceName, &pSrcClass);
	ERROR_IF(!pSrcClass, JCL_ERR_Undefined_Identifier, pIfaceName, goto exit);
	ERROR_IF(pSrcClass->miFamily != tf_interface, JCL_ERR_Type_Not_Interface, pIfaceName, goto exit);
	ERROR_IF(!pSrcClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pIfaceName, goto exit);
	ERROR_IF(!IsClassNative(pClass) && IsModifierNativeInterface(pSrcClass), JCL_ERR_Interface_Native_Only, pIfaceName, goto exit);

	if( _this->miPass == kPassPrecompile )
	{
		// inherit from interface
		pClass->miBaseType = pSrcClass->miType;
		// copy method info
		memcpy(&pClass->miMethodInfo, &pSrcClass->miMethodInfo, sizeof(JILMethodInfo));
		// copy over all function declarations from source class
		pClass->mipFuncs->Copy(pClass->mipFuncs, pSrcClass->mipFuncs);
		// create function handles for each new function
		for( i = 0; i < NumFuncs(_this, classIdx); i++ )
		{
			pFunc = GetFunc(_this, classIdx, i);
			// set class index
			pFunc->miClassID = pClass->miType;
			// set strict modifier
			pFunc->miStrict |= bStrict;
			// if method is constructor, set correct name
			if( pFunc->miCtor )
			{
				pFunc->mipName->Copy(pFunc->mipName, pClassName);
				// if method is copy-constructor, set source type to inheriting class
				if (pFunc->mipArgs->count == 1)
				{
					JCLVar* pArg1 = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
					if (pArg1->miType == pClass->miBaseType)
						pArg1->miType = pClass->miType;
				}
			}
			// add to function segment
			err = JILCreateFunction(pMachine, pClass->miType, i, GetFuncInfoFlags(pFunc), JCLGetString(pFunc->mipName), &(pFunc->miHandle));
			ERROR_IF(err, err, NULL, goto exit);
		}
		pClass->mipTag->Copy(pClass->mipTag, pSrcClass->mipTag);
		// class inherits interface body, so...
		pClass->miHasBody = JILTrue;
		// update base-type identifier in JILTypeInfo
		pTI = JILTypeInfoFromType(pMachine, pClass->miType);
		pTI->base = pClass->miBaseType;
	}

exit:
	DELETE( pIfaceName );
	return err;
}

//------------------------------------------------------------------------------
// p_class_hybrid
//------------------------------------------------------------------------------
// Parse "hybrid inheritance" of a class.

static JILError p_class_hybrid(JCLState* _this, JCLClass* pClass)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pToken;
	JCLString* pBaseName;
	JCLClass* pSrcClass;
	JCLFunc* pFunc;
	JCLVar* pVar;
	JCLVar* pBaseVar;
	JILLong i;
	JILLong typeID;
	JILLong dstType;
	JILLong srcType;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);
	pBaseName = NEW(JCLString);
	pVar = NEW(JCLVar);
	pBaseVar = NEW(JCLVar);
	dstType = pClass->miType;

	// not allowed with 'native' class declaration
	ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_With_Hybrid, NULL, goto exit);
	// we expect an identifier
	err = pFile->GetToken(pFile, pBaseName, &tokenID);
	ERROR_IF(err, err, pBaseName, goto exit);
	ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pBaseName, goto exit);
	FindClass(_this, pBaseName, &pSrcClass);
	ERROR_IF(!pSrcClass, JCL_ERR_Undefined_Identifier, pBaseName, goto exit);
	srcType = pSrcClass->miType;
	ERROR_IF(!IsClassType(_this, srcType), JCL_ERR_Type_Not_Class, pBaseName, goto exit);
	ERROR_IF(!pSrcClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pBaseName, goto exit);
	if( srcType == dstType )
		ERROR(JCL_ERR_Unexpected_Token, pBaseName, goto exit);

	if( _this->miPass == kPassPrecompile )
	{
		pClass->miHybridBase = srcType;
		// create "base" variable in this class
		pBaseVar->miType = srcType;
		pBaseVar->miRef = JILTrue;
		JCLSetString(pBaseVar->mipName, "base");
		err = AddMemberVar(_this, dstType, pBaseVar);
		ERROR_IF(err, err, pBaseVar->mipName, goto exit);
		// iterate over all functions in source class
		for( i = 0; i < NumFuncs(_this, srcType); i++ )
		{
			pFunc = GetFunc(_this, srcType, i);
			// only global functions and methods are valid
			if( !pFunc->miCtor && !pFunc->miConvertor && !pFunc->miAccessor && !pFunc->miCofunc && !pFunc->miAnonymous )
			{
				// create delegate from function
				err = CreateDelegate(_this, pFunc->mipResult, pFunc->mipArgs, &typeID);
				ERROR_IF(err, err, pFunc->mipName, goto exit);
				// add a member variable for this type
				pVar->miType = typeID;
				pVar->miRef = JILTrue;
				pVar->miHidden = JILFalse;
				pVar->mipName->Copy(pVar->mipName, pFunc->mipName);
				err = AddMemberVar(_this, dstType, pVar);
				ERROR_IF(err != JCL_No_Error && err != JCL_ERR_Identifier_Already_Defined, err, pFunc->mipName, goto exit);
				// if we have a name conflict check if it's with an interface method
				if( err == JCL_ERR_Identifier_Already_Defined )
				{
					JCLFunc* pInFunc;
					// class must inherit an interface
					ERROR_IF(pClass->miBaseType == 0, err, pFunc->mipName, goto exit);
					// hybrid base must inherit same interface
					ERROR_IF(pSrcClass->miBaseType != pClass->miBaseType, err, pFunc->mipName, goto exit);
					// find function in interface
					FindDiscreteFunction(_this, pClass->miBaseType, pFunc->mipName, pFunc->mipResult, pFunc->mipArgs, &pInFunc);
					ERROR_IF(pInFunc == NULL, err, pFunc->mipName, goto exit);
					// find inherited function in this class
					FindDiscreteFunction(_this, dstType, pFunc->mipName, pFunc->mipResult, pFunc->mipArgs, &pInFunc);
					ERROR_IF(pInFunc == NULL, err, pFunc->mipName, goto exit);
					ERROR_IF(pInFunc->miAccessor || pInFunc->miCtor, err, pFunc->mipName, goto exit);
					// make delegate hidden so it cannot be called directly
					pVar->miHidden = JILTrue;
					// create the delegate variable to hide member function
					err = AddMemberVarEx(_this, kClassVarDelegate, dstType, pVar);
					ERROR_IF(err, err, pFunc->mipName, goto exit);
					// store the variable index in the function prototype for link process
					pInFunc->miLnkDelegate = pVar->miMember;
				}
			}
			else if( pFunc->miAccessor && pClass->miBaseType && pSrcClass->miBaseType == pClass->miBaseType )
			{
				JCLFunc* pInFunc;
				// find accessor in interface
				FindDiscreteFunction(_this, pClass->miBaseType, pFunc->mipName, pFunc->mipResult, pFunc->mipArgs, &pInFunc);
				if( pInFunc != NULL )
				{
					JILLong funcIdx = pInFunc->miFuncIdx;
					// find accessor in this class
					FindDiscreteFunction(_this, dstType, pFunc->mipName, pFunc->mipResult, pFunc->mipArgs, &pInFunc);
					if( pInFunc != NULL )
					{
						ERROR_IF(!pInFunc->miAccessor || pInFunc->miCtor, err, pFunc->mipName, goto exit);
						// store the function index in the function prototype for link process
						pInFunc->miLnkDelegate = funcIdx;
					}
				}
			}
		}
		pClass->miHasBody = JILTrue;
	}

exit:
	DELETE( pToken );
	DELETE( pBaseName );
	DELETE( pVar );
	DELETE( pBaseVar );
	return err;
}

//------------------------------------------------------------------------------
// p_function
//------------------------------------------------------------------------------
// Parse a function or method declaration or definition.

static JILError p_function(JCLState* _this, JILLong fnKind, JILBool isPure)
{
	JILError err = JCL_No_Error;
	JILState* pMachine = _this->mipMachine;
	JCLFile* pFile;
	JCLClass* pClass;
	JCLFunc* pFunc;
	JCLVar* pVar;
	JCLVar* pResVar;
	Array_JCLVar* pArgs;
	JCLString* pToken;
	JCLString* pName;
	JILLong tokenID;
	JILLong classToken;
	JILLong savePos;
	JILLong errorNamePos;
	JILLong funcIdx;
	JILLong argNum;
	JILLong initialScope = _this->miClass;
	JILBool forceDecl = JILFalse;
	JILBool removeFunc = JILFalse;
	JILLong i, j;

	pResVar = NEW(JCLVar);
	pToken = NEW(JCLString);
	pName = NEW(JCLString);
	pFile = _this->mipFile;

	// clear sim stack and sim regs
	if(_this->miStackPos != kSimStackSize)
		FATALERROREXIT("p_function", "Simulated stack not clean");
	for( i = 0; i < kNumRegisters; i++ )
	{
		if( _this->mipRegs[i] )
			FATALERROREXIT("p_function", "Simulated register not clean");
	}

	// is it a result type (eg. "const var")?
	savePos = pFile->GetLocator(pFile);
	err = IsFullTypeDecl(_this, pToken, pResVar, JILTrue);
	if( err == JCL_ERR_No_Type_Declaration )
	{
		// not a type decl, return to saved pos
		pFile->SetLocator(pFile, savePos);
	}
	else if( err )
	{
		ERROR(err, pToken, goto exit);
	}
	else
	{
		// result always r1
		pResVar->miMode = kModeRegister;
		pResVar->miUsage = kUsageResult;
		pResVar->miIndex = 1;
		pResVar->miInited = JILTrue;
	}

	// expecting an identifier, either function name, or class name
	err = pFile->GetToken(pFile, pName, &classToken);
	ERROR_IF(err, err, pName, goto exit);
	if( !IsClassToken(classToken) && classToken != tk_convertor )
		ERROR(JCL_ERR_Unexpected_Token, pName, goto exit);

	// expecting scope operator, qualifying identifier as class name,
	// or parenthese, qualifying identifier as function name
	errorNamePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( IsGlobalScope(_this, _this->miClass) && tokenID == tk_scope )
	{
		FindClass(_this, pName, &pClass);
		ERROR_IF(!pClass, JCL_ERR_Undefined_Identifier, pName, goto exit);
		ERROR_IF(pClass->miFamily != tf_class, JCL_ERR_Method_Definition_Illegal, pName, goto exit);
		ERROR_IF(!pClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pName, goto exit);
		ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_Modifier_Illegal, pName, goto exit);
		// make this the current class
		SetCompileContext(_this, pClass->miType, 0);
		// expecting an identifier, this time function name
		err = pFile->GetToken(pFile, pName, &classToken);
		ERROR_IF(err, err, pName, goto exit);
		if( !IsClassToken(classToken) && classToken != tk_convertor )
			ERROR(JCL_ERR_Unexpected_Token, pName, goto exit);
		errorNamePos = pFile->GetLocator(pFile);
		// get what comes next...
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	else if( IsGlobalScope(_this, _this->miClass) && (fnKind & kMethod) )
	{
		ERROR(JCL_ERR_Method_Outside_Class, pName, goto exit);
	}
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// add a new empty function
	pClass = CurrentClass(_this);
	funcIdx = NumFuncs(_this, pClass->miType);
	pFunc = pClass->mipFuncs->New(pClass->mipFuncs);
	pFunc->miFuncIdx = funcIdx;
	pFunc->miClassID = pClass->miType;
	pFunc->miMethod = (fnKind & kMethod);
	pFunc->miAccessor = (fnKind & kAccessor);
	pFunc->miCofunc = (fnKind & kCofunction);
	pFunc->miExplicit = (fnKind & kExplicit);
	pFunc->miStrict = (fnKind & kStrict);
	pClass->miHasMethod = (fnKind & (kAccessor | kMethod));
	removeFunc = JILTrue;

	// copy function name
	pFunc->mipName->Copy(pFunc->mipName, pName);
	// copy result to function
	pFunc->mipResult->Copy(pFunc->mipResult, pResVar);
	// if parsing a class
	if( !IsGlobalScope(_this, pClass->miType) )
	{
		// check for cofunction in native type
		ERROR_IF((fnKind & kCofunction) && IsClassNative(pClass), JCL_ERR_Cofunction_In_NTL, pName, goto exit);
		// compare function name with class name (constructor?)
		if( JCLCompare(pFunc->mipName, pClass->mipName) )
		{
			// this is a constructor, it must be declared "void"
			if( pFunc->mipResult->miMode != kModeUnused )
				ERROR(JCL_ERR_Constructor_Not_Void, pFunc->mipName, goto exit);
			// it also must be declared using the "method" keyword
			if( !(fnKind & kMethod) || (fnKind & kAccessor) )
				ERROR(JCL_ERR_Constructor_Is_Function, pFunc->mipName, goto exit);
			pFunc->miCtor = JILTrue;
			pFunc->miStrict |= !IsClassNative(pClass);	// constructors always strict if not a native type
			pClass->miHasCtor = JILTrue;
		}
		// check for the convertor keyword
		else if( classToken == tk_convertor )
		{
			// this is a convertor, it must have a return type
			if( pFunc->mipResult->miMode == kModeUnused )
				ERROR(JCL_ERR_Convertor_Is_Void, NULL, goto exit);
			// it also must be declared using the "method" keyword
			if( !(fnKind & kMethod) || (fnKind & kAccessor) )
				ERROR(JCL_ERR_Convertor_Is_Function, NULL, goto exit);
			pFunc->miConvertor = JILTrue;
		}
		else
		{
			// neither constructor nor convertor, forbid 'explicit'
			ERROR_IF(pFunc->miExplicit, JCL_ERR_Explicit_With_Method, NULL, goto exit);
		}
	}

	// make this the current function
	SetCompileContext(_this, _this->miClass, funcIdx);

	// parse argument list
	pArgs = pFunc->mipArgs;
	argNum = 0;
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	if( err )
		goto exit;
	if( tokenID != tk_round_close )
	{
		ERROR_IF(pFunc->miConvertor, JCL_ERR_Convertor_Has_Arguments, pToken, goto exit);
		// go back to saved pos and get arguments
		pFile->SetLocator(pFile, savePos);
		while( tokenID != tk_round_close )
		{
			pVar = pArgs->New(pArgs);
			err = IsFullTypeDecl(_this, pToken, pVar, JILTrue);
			if( err == JCL_ERR_No_Type_Declaration )
				err = JCL_ERR_Unexpected_Token;
			ERROR_IF(err, err, pToken, goto exit);
			// peek if there is a name
			err = pFile->PeekToken(pFile, pToken, &tokenID);
			if( err )
				goto exit;
			if( tokenID != tk_identifier )
				forceDecl = JILTrue;
			else
			{
				// get identifier name
				err = pFile->GetToken(pFile, pVar->mipName, &tokenID);
				ERROR_IF(err, err, pVar->mipName, goto exit);
			}
			// initialize
			pVar->miMode = kModeStack;
			pVar->miIndex = argNum++;
			pVar->miInited = JILTrue;
			// expecting now "," or ")"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_comma && tokenID != tk_round_close )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	// validate accessor function
	if( fnKind & kAccessor )
	{
		JCLFunc* pAcc;
		JILLong fn;
		JCLVar* pVar1, *pVar2;
		// if function has no result, it must have only 1 argument
		if( pFunc->mipResult->miMode == kModeUnused )
		{
			ERROR_IF(argNum != 1, JCL_ERR_Function_Not_An_Accessor, pFunc->mipName, goto exit);
		}
		// if function has result, it must have 0 arguments!
		else
		{
			ERROR_IF(argNum, JCL_ERR_Function_Not_An_Accessor, pFunc->mipName, goto exit);
		}
		// make sure the types of getter / setter match
		for(;;) // just so we can leave without using goto
		{
			fn = FindAccessor(_this, pClass->miType, pFunc->mipName, 0, &pAcc);
			while( pAcc != NULL && pFunc->mipArgs->count == pAcc->mipArgs->count )			// found setter but need getter?
				fn = FindAccessor(_this, pClass->miType, pFunc->mipName, fn + 1, &pAcc);	// search again
			if( pAcc == NULL )
				break;
			if( pFunc->mipArgs->count )
			{
				pVar1 = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
				pVar2 = pAcc->mipResult;
			}
			else
			{
				pVar1 = pFunc->mipResult;
				pVar2 = pAcc->mipArgs->Get(pAcc->mipArgs, 0);
			}
			ERROR_IF(!EqualTypes(pVar1, pVar2), JCL_ERR_Arg_Type_Conflict, pFunc->mipName, goto exit);
			break;
		}
	}
	// check for ctor, cctor
	if( pFunc->miCtor && !pFunc->miExplicit ) // TODO: disallow 'explicit' OK?
	{
		if (pFunc->mipArgs->count == 0 && pClass->miMethodInfo.ctor == -1)
		{
			pClass->miMethodInfo.ctor = pFunc->miFuncIdx;
		}
		else if (pFunc->mipArgs->count == 1 && pClass->miMethodInfo.cctor == -1)
		{
			pVar = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
			if (pVar->miType == pClass->miType)
				pClass->miMethodInfo.cctor = pFunc->miFuncIdx;
		}
	}
	// check for string convertor
	else if( pFunc->miConvertor && !pFunc->miExplicit ) // TODO: disallow 'explicit' OK?
	{
		if( pFunc->mipResult->miType == type_string && pClass->miMethodInfo.tostr == -1)
			pClass->miMethodInfo.tostr = pFunc->miFuncIdx;
	}
	// cofunction: create cofunction class
	else if( pFunc->miCofunc )
	{
		JILLong classIdx;
		JCLFunc* pFunc2;
		// create a new type, or use existing one
		err = CreateCofunction(_this, pFunc->mipResult, pFunc->mipArgs, &classIdx);
		ERROR_IF(err, err, pToken, goto exit);
		// now copy over the function to the delegate class
		pClass = GetClass(_this, classIdx);
		pClass->miHasBody = JILTrue;
		pFunc2 = pClass->mipFuncs->New(pClass->mipFuncs);
		pFunc2->Copy(pFunc2, pFunc);
		pFunc2->miFuncIdx = pClass->mipFuncs->count - 1;
		// destroy original function
		pClass = CurrentClass(_this);
		pClass->mipFuncs->Trunc(pClass->mipFuncs, pFunc->miFuncIdx);
		pFunc = pFunc2;
		if( _this->miPass == kPassPrecompile )
		{
			// if cofunction is class member, mangle cofunction type name
			JCLClear(pToken);
			if( !IsGlobalScope(_this, _this->miClass) )
			{
				JCLSetString(pToken, JCLGetString(GetClass(_this, _this->miClass)->mipName));
				JCLAppend(pToken, "::");
			}
			JCLAppend(pToken, JCLGetString(pName));
			// check if we already defined an alias for this
			FindClass(_this, pToken, &pClass);
			if( pClass && pClass->miType != classIdx )
			{
				ERROR(JCL_ERR_Identifier_Already_Defined, pToken, goto exit);
			}
			else if( !pClass )
			{
				// must clear function name or AddAlias will complain the name is taken...
				JCLClear(pFunc->mipName);
				// create an alias for this cofunction
				AddAlias(_this, pToken, classIdx);
				ERROR_IF(err, err, pToken, goto exit);
				// restore the name
				JCLSetString(pFunc->mipName, JCLGetString(pName));
			}
		}
		SetCompileContext(_this, classIdx, pFunc->miFuncIdx);
	}
	// check for ";" or "{" or "hybrid"
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( (tokenID == tk_curly_open || tokenID == tk_hybrid) && _this->miPass == kPassCompile )
	{
		JCLVar* src;
		JCLVar* dst;
		JCLFunc* pFunc2;
		ERROR_IF(forceDecl, JCL_ERR_Incomplete_Arg_List, pFunc->mipName, goto exit);
		ERROR_IF(isPure, JCL_ERR_Method_Definition_Illegal, pFunc->mipName, goto exit);
		pClass = CurrentClass(_this);
		// check if this is a native type lib declaration
		if( IsClassNative(pClass) )
			ERROR(JCL_ERR_Cannot_Reimplement_NTL, pClass->mipName, goto exit);
		// try to find the prototype (it MUST exist, cos pre-compile should have created it!)
		err = FindPrototype(_this, pFunc, &pFunc2);
		ERROR_IF(err, err, pFunc->mipName, goto exit);
		// function already has a body?
		ERROR_IF(pFunc2->mipCode->count, JCL_ERR_Function_Already_Defined, pFunc->mipName, goto exit);
		// update and check argument names
		for( i = 0; i < pFunc->mipArgs->count; i++ )
		{
			src = pFunc->mipArgs->Get(pFunc->mipArgs, i);
			dst = pFunc2->mipArgs->Get(pFunc2->mipArgs, i);
			dst->mipName->Copy(dst->mipName, src->mipName);
			dst->miInited = JILTrue;
			JCLSetString(src->mipName, "");
			// check if identifier can be used
			j = pFunc2->miMethod ? kMethodLocalVar : kFuncLocalVar;
			err = IsIdentifierUsed(_this, j, _this->miClass, dst->mipName);
			ERROR_IF(err, err, dst->mipName, goto exit);
		}
		// discard our prototype
		pClass = CurrentClass(_this);
		pClass->mipFuncs->Trunc(pClass->mipFuncs, pFunc->miFuncIdx);
		removeFunc = JILFalse;
		// compile to the found function instead
		SetCompileContext(_this, _this->miClass, pFunc2->miFuncIdx);
		// parse function body
		err = p_function_body(_this);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_semicolon || tokenID == tk_hybrid || _this->miPass == kPassPrecompile )
	{
		JCLFunc* pFunc2;
		if( tokenID == tk_semicolon )
		{
			// skip token
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			// check for tag
			err = p_tag(_this, pFunc->mipTag);
			if( err )
				goto exit;
		}
		else
		{
			if( tokenID == tk_hybrid )
			{
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				err = p_skip_braces(_this, tk_round_open, tk_round_close);
				ERROR_IF(err, err, NULL, goto exit);
			}
			// skip code block
			err = p_skip_block(_this);
			if( err )
				goto exit;
		}
		// try to find the prototype
		err = FindPrototype(_this, pFunc, &pFunc2);
		// if not yet defined
		if( err == JCL_ERR_Undefined_Identifier )
		{
			// check if the identifier name is OK
			JILLong kind = kGlobalFunc;
			if( pFunc->miAccessor )
				kind = kClassAccessor;
			else if( pFunc->miCtor )
				kind = kClassCtor;
			else if( pFunc->miMethod )
				kind = kClassMethod;
			else if( !IsGlobalScope(_this, _this->miClass) )
				kind = kClassFunc;
			if( kind == kClassAccessor )
				err = IsAccessorUsed(_this, _this->miClass, pFunc);
			else
				err = IsIdentifierUsed(_this, kind, _this->miClass, pFunc->mipName);
			if( err )
			{
				pFile->SetLocator(pFile, errorNamePos);
				ERROR(err, pFunc->mipName, goto exit);
			}
			// if we declare or define a class member function/method at file scope, which has not been declared INSIDE of the class declaration, this is an error!
			if( IsGlobalScope(_this, initialScope) && (kind != kGlobalFunc) && !pFunc->miCofunc )
				ERROR(JCL_ERR_Method_Outside_Class, pFunc->mipName, goto exit);
			if( !isPure )
			{
				// create a function handle
				pClass = CurrentClass(_this);
				err = JILCreateFunction(pMachine, pClass->miType, pFunc->miFuncIdx, GetFuncInfoFlags(pFunc), JCLGetString(pFunc->mipName), &(pFunc->miHandle));
				ERROR_IF(err, err, NULL, goto exit);
			}
		}
		else if( err == JCL_No_Error )
		{
			// take over strict attribute from new prototype
			pFunc2->miStrict |= pFunc->miStrict;
			// copy new tag if there is one
			if( JCLGetLength(pFunc->mipTag) > 0 )
				pFunc2->mipTag->Copy(pFunc2->mipTag, pFunc->mipTag);
			// discard our prototype
			pClass = CurrentClass(_this);
			pClass->mipFuncs->Trunc(pClass->mipFuncs, pFunc->miFuncIdx);
			removeFunc = JILFalse;
			SetCompileContext(_this, _this->miClass, 0);
		}
		else
		{
			ERROR(err, pFunc->mipName, goto exit);
		}
	}
	else
	{
		ERROR(JCL_ERR_Missing_Semicolon, NULL, goto exit);
	}

exit:
	// in case of an error, remove the function again
	if( err &&  removeFunc )
	{
		pClass = CurrentClass(_this);
		pClass->mipFuncs->Trunc(pClass->mipFuncs, funcIdx);
	}
	// make sure the sim stack is "clean"
	argNum = kSimStackSize - _this->miStackPos;	// number of items on stack
	SimStackPop(_this, argNum);
	SetCompileContext(_this, initialScope, 0);

	DELETE( pResVar );
	DELETE( pToken );
	DELETE( pName );
	return err;
}

//------------------------------------------------------------------------------
// p_function_body
//------------------------------------------------------------------------------
// Parse the body of a function, cofunction or method.

static JILError p_function_body(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLFunc* pFunc;
	JCLVar* pThis = NULL;
	JILLong savePos;
	SMarker marker;
	JILLong i;
	JILLong j;
	JILBool freeThis = JILFalse;

	pFile = _this->mipFile;
	pFunc = CurrentFunc(_this);

	// prepare "this"
	if( pFunc->miMethod )
	{
		pThis = MakeThisVar(_this, pFunc->miClassID);
		SimRegisterSet(_this, 0, pThis);
		freeThis = JILTrue;
	}
	// save current optimization level from compiler options (we don't have the option object anymore in linker stage!)
	pFunc->miOptLevel = GetOptions(_this)->miOptimizeLevel;
	// create restore point, so we can undo compilation later
	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	// clear register usage information
	for( i = 0; i < kNumRegisters; i++ )
		_this->miRegUsage[i] = 0;
	_this->miNumRegsToSave = 0;
	// *** PASS 1: Test-compile function body to get register usage information ***
	err = p_function_pass(_this);
	if( err )
		goto write_ret;
	// update register usage information
	for( j = 0; j < kNumRegisters; j++ )
	{
		if( _this->miRegUsage[j] )
			_this->miNumRegsToSave++;
	}
	// do we need to save registers at all?
	if( _this->miNumRegsToSave && !pFunc->miCofunc )
	{
		// then undo previous compilation and recompile with correct settings
		pFile->SetLocator(pFile, savePos);
		RestoreMarker(_this, &marker);
		// *** PASS 2: Compile function with correct register usage information ***
		err = p_function_pass(_this);
		if( err )
			goto write_ret;
	}
	// compile function literals
	err = p_sub_functions(_this);
	if( err )
		goto exit;

exit:
	// destroy 'this'
	if( freeThis )
	{
		SimRegisterUnset(_this, 0);
		DELETE( pThis );
	}
	return err;

write_ret:
	// we get here if there was an error compiling the function's body.
	// we remove the partial generated code and write a "moveh 0,r1"
	// and a "ret" instruction.
	pFunc = CurrentOutFunc(_this);
	pFunc->mipCode->Trunc(pFunc->mipCode, 0);
	if( pFunc->miCofunc )
	{
		cg_opcode(_this, op_moveh_r);
		cg_opcode(_this, 0);
		cg_opcode(_this, kReturnRegister);
		cg_opcode(_this, op_yield);
		cg_opcode(_this, op_bra);
		cg_opcode(_this, -1);
	}
	else
	{
		cg_opcode(_this, op_moveh_r);
		cg_opcode(_this, 0);
		cg_opcode(_this, kReturnRegister);
		cg_opcode(_this, op_ret);
	}
	goto exit;
}

//------------------------------------------------------------------------------
// p_function_pass
//------------------------------------------------------------------------------
// Functions need to be compiled in two passes to find out how many registers
// the function is going to use. In the second pass the correct number of
// registers will then be saved to stack at function begin and restored from
// stack before the function returns.

static JILError p_function_pass(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILLong i;
	JCLFunc* pFunc;
	JCLFile* pFile;
	Array_JCLVar* pArgs;
	JCLString* pToken;
	JCLString* pToken2;
	JILLong tokenID;
	JILBool isCompound;
	JCLClass* pClass;

	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pFile = _this->mipFile;

	pFunc = CurrentFunc(_this);
	pFunc->miRetFlag = JILFalse;
	pFunc->miYieldFlag = JILFalse;
	// push arguments in reverse order onto simulated stack
	pArgs = pFunc->mipArgs;
	for( i = pArgs->count - 1; i >= 0; i-- )
		SimStackPush(_this, pArgs->Get(pArgs, i), JILFalse);
	// generate register saving code
	if( _this->miNumRegsToSave )
	{
		cg_push_registers(_this, _this->miRegUsage, _this->miNumRegsToSave);
		SimStackReserve(_this, _this->miNumRegsToSave);
	}
	// un-init members in ctors
	if( pFunc->miCtor )
	{
		InitMemberVars(_this, pFunc->miClassID, JILFalse);
		// check if class is hybrid
		pClass = GetClass(_this, pFunc->miClassID);
		if( pClass->miHybridBase )
		{
			// expect "hybrid"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_hybrid, JCL_ERR_Hybrid_Expected, pFunc->mipName, goto exit);
			// expect "("
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
			// generate hybrid inheritance code
			err = p_function_hybrid(_this, pFunc);
			if( err)
				goto exit;
			// expect "}"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	// parse block (function body)
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_curly_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// clear block level
	_this->miBlockLevel = 0;
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	pFunc = CurrentFunc(_this);
	// re-init members
	if( pFunc->miCtor )
	{
		if( !AllMembersInited(_this, pFunc->miClassID, pToken) )
			ERROR(JCL_ERR_Must_Init_All_Members, pToken, goto exit);
	}
	if( pFunc->miCofunc )
	{
		// take function arguments from stack
		pArgs = pFunc->mipArgs;
		SimStackPop(_this, pArgs->count);
		// check if we have compiled a yield statement
		if( pFunc->miYieldFlag )
		{
			if( pArgs->count )
				cg_opcode(_this, op_yield);
		}
		else
		{
			// auto-generate yield
			if( pFunc->mipResult->miMode != kModeUnused )
			{
				cg_opcode(_this, op_moveh_r);
				cg_opcode(_this, 0);
				cg_opcode(_this, 1);
			}
			cg_opcode(_this, op_yield);
			pFunc->miYieldFlag = JILTrue;
		}
		// add branch back to yield if the cofunction is resumed after it has run through
		cg_opcode(_this, op_bra);
		cg_opcode(_this, -1);
	}
	else
	{
		// check if we had compiled a return statement
		if( pFunc->miRetFlag )
		{
			// take function arguments from stack
			pArgs = pFunc->mipArgs;
			SimStackPop(_this, pArgs->count);
		}
		else
		{
			// void function: auto-generate return
			if( pFunc->mipResult->miMode == kModeUnused )
			{
				// Unroll entire stack
				JILLong numStack = kSimStackSize - _this->miStackPos;	// number of items on stack
				numStack -= _this->miNumRegsToSave;						// minus saved registers
				numStack -= pFunc->mipArgs->count;						// minus arguments
				if( numStack < 0 )
					FATALERROREXIT("p_function_pass", "No. of items on stack is negative");
				if( numStack )
				{
					// pop all local variables from stack
					cg_pop_multi(_this, numStack);
					SimStackPop(_this, numStack);
				}
				// restore saved registers
				if( _this->miNumRegsToSave )
				{
					cg_pop_registers(_this, _this->miRegUsage, _this->miNumRegsToSave);
					SimStackPop(_this, _this->miNumRegsToSave);
				}
				// take function arguments from stack
				pArgs = pFunc->mipArgs;
				SimStackPop(_this, pArgs->count);
				cg_return(_this);
				pFunc->miRetFlag = JILTrue;
			}
			else
			{
				ERROR(JCL_ERR_No_Return_Value, pFunc->mipName, goto exit);
			}
		}
	}

exit:
	DELETE( pToken2 );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_sub_functions
//------------------------------------------------------------------------------
// Compile the code of the function literals of this function.

static JILError p_sub_functions(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILLong i;
	JILLong funcIdx;
	JILLong savePos;
	JCLFunc* pCurrentFunc;
	JCLFunc* pNewFunc;
	JCLLiteral* pLit;
	JCLString* pName;
	JCLClass* pClass;
	JCLFuncType* pDelegate;
	JCLFile* pFile;
	Array_JCLLiteral* pLiterals;
	JCLString* pToken;
	JILLong tokenID;

	// if we don't have an init function yet, bail
	if( _this->miPass == kPassPrecompile || NumFuncs(_this, type_global) == 0 )
		return err;

	pName = NEW(JCLString);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pClass = CurrentOutClass(_this);
	pCurrentFunc = CurrentOutFunc(_this);
	pLiterals = pCurrentFunc->mipLiterals;
	savePos = pFile->GetLocator(pFile);

	for( i = 0; i < pLiterals->count; i++ )
	{
		pLit = pLiterals->Get(pLiterals, i);
		if( TypeFamily(_this, pLit->miType) == tf_delegate && pLit->miHandle == 0 )
		{
			pDelegate = GetClass(_this, pLit->miType)->mipFuncType;
			funcIdx = NumFuncs(_this, pClass->miType);
			// generate function name
			JCLFormat(pName, kNameAnonymousFunction, funcIdx);
			// create function
			pNewFunc = pClass->mipFuncs->New(pClass->mipFuncs);
			pNewFunc->miFuncIdx = funcIdx;
			pNewFunc->miClassID = pClass->miType;
			pNewFunc->miMethod = pLit->miMethod;
			pNewFunc->miAnonymous = JILTrue;
			pNewFunc->mipName->Copy(pNewFunc->mipName, pName);
			// copy result and arguments from delegate
			pNewFunc->mipResult->Copy(pNewFunc->mipResult, pDelegate->mipResult);
			pNewFunc->mipArgs->Copy(pNewFunc->mipArgs, pDelegate->mipArgs);
			// create function handle
			err = JILCreateFunction(_this->mipMachine, pClass->miType, pNewFunc->miFuncIdx, GetFuncInfoFlags(pNewFunc), JCLGetString(pNewFunc->mipName), &(pNewFunc->miHandle));
			ERROR_IF(err, err, NULL, goto exit);
			// use function handle or method index?
			if( pLit->miMethod )
				pLit->miHandle = pNewFunc->miFuncIdx;
			else
				pLit->miHandle = pNewFunc->miHandle;
			// set code locator to start of sub function
			pFile->SetLocator(pFile, pLit->miLocator);
			// check for optional argument name list
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID == tk_round_open )
			{
				JILLong argc = 0;
				JCLVar* pVar;
				// read identifier names and update function argument names
				for(;;)
				{
					err = pFile->GetToken(pFile, pToken, &tokenID);
					ERROR_IF(err, err, pToken, goto exit);
					ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
					ERROR_IF(argc >= pNewFunc->mipArgs->count, JCL_ERR_Unexpected_Token, pToken, goto exit);
					pVar = pNewFunc->mipArgs->Get(pNewFunc->mipArgs, argc);
					pVar->mipName->Copy(pVar->mipName, pToken);
					argc++;
					err = pFile->GetToken(pFile, pToken, &tokenID);
					ERROR_IF(err, err, pToken, goto exit);
					if( tokenID == tk_round_close )
						break;
					else if( tokenID == tk_comma )
						continue;
					else
						ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
				}
				ERROR_IF(argc < pNewFunc->mipArgs->count, JCL_ERR_Unexpected_Token, pToken, goto exit);
			}
			else
			{
				pFile->SetLocator(pFile, pLit->miLocator);
			}
			// now compile to the new function
			SetCompileContext(_this, pClass->miType, funcIdx);
			err = p_function_body(_this);
			if( err )
				goto exit;
		}
	}

exit:
	SetCompileContext(_this, pClass->miType, pCurrentFunc->miFuncIdx);
	pFile->SetLocator(pFile, savePos);
	DELETE( pName );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_function_hybrid
//------------------------------------------------------------------------------
// Generate code for a hybrid class constructor.

static JILError p_function_hybrid(JCLState* _this, JCLFunc* pFunc)
{
	JILError err = JCL_No_Error;
	TypeInfo outType;
	JCLVar* pTempVar = NULL;
	JCLVar* pTempDel = NULL;
	Array_JCLVar* pLocals;
	JCLClass* pClass;
	JCLClass* pSrcClass;
	JCLFunc* pSFunc;
	JCLVar* pMember;
	JILLong i;
	JILLong memberIdx;

	pLocals = NEW(Array_JCLVar);
	pClass = GetClass(_this, pFunc->miClassID);
	pSrcClass = GetClass(_this, pClass->miHybridBase);

	// create a temp var
	err = MakeTempVar(_this, &pTempVar, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempVar->miType = pClass->miHybridBase;
	pTempVar->miRef = JILTrue;

	// parse hybrid expression
	JCLClrTypeInfo(&outType);
	err = p_expression(_this, pLocals, pTempVar, &outType, 0);
	if( err )
		goto exit;

	// create member variable initialization code
	memberIdx = 0;
	pMember = pClass->mipVars->Get(pClass->mipVars, memberIdx);
	err = cg_move_var(_this, pTempVar, pMember);
	ERROR_IF(err, err, NULL, goto exit);
	pMember->miInited = JILTrue;
	// HACK: Because OptimizeMoveOperations() would corrupt the code, we must reload pTempVar here
	err = cg_move_var(_this, pMember, pTempVar);
	ERROR_IF(err, err, NULL, goto exit);
	memberIdx++;
	for( i = 0; i < NumFuncs(_this, pSrcClass->miType); i++ )
	{
		pSFunc = GetFunc(_this, pSrcClass->miType, i);
		if( !pSFunc->miCtor && !pSFunc->miConvertor && !pSFunc->miAccessor && !pSFunc->miCofunc && !pSFunc->miAnonymous )
		{
			// create another temp var
			err = MakeTempVar(_this, &pTempDel, NULL);
			ERROR_IF(err, err, NULL, goto exit);
			if( pSFunc->miMethod )
			{
				err = FindFuncRef(_this, pSFunc->mipName, pSrcClass->miType, kMethod, pTempDel, &pSFunc);
				ERROR_IF(err, err, pSFunc->mipName, goto exit);
				err = cg_new_delegate(_this, pSFunc->miFuncIdx, pTempVar, pTempDel);
				ERROR_IF(err, err, NULL, goto exit);
			}
			else
			{
				err = FindFuncRef(_this, pSFunc->mipName, pSrcClass->miType, kFunction, pTempDel, &pSFunc);
				ERROR_IF(err, err, pSFunc->mipName, goto exit);
				err = cg_new_delegate(_this, pSFunc->miHandle, NULL, pTempDel);
				ERROR_IF(err, err, NULL, goto exit);
			}
			pMember = pClass->mipVars->Get(pClass->mipVars, memberIdx);
			err = cg_move_var(_this, pTempDel, pMember);
			ERROR_IF(err, err, NULL, goto exit);
			pMember->miInited = JILTrue;
			FreeTempVar(_this, &pTempDel);
			memberIdx++;
		}
	}

exit:
	FreeTempVar(_this, &pTempVar);
	FreeTempVar(_this, &pTempDel);
	FreeLocalVars(_this, pLocals);
	DELETE( pLocals );
	return err;
}

//------------------------------------------------------------------------------
// p_block
//------------------------------------------------------------------------------
// Parse a code block. Code blocks can be nested.

static JILError p_block(JCLState* _this, JILBool* pIsCompound)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	Array_JCLVar* pLocals;
	JILLong tokenID;

	// increase block level
	_this->miBlockLevel++;

	pLocals = NEW(Array_JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// check for "{"
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_curly_open )
	{
		// try single statement
		err = p_statement(_this, pLocals, pIsCompound);
		if( err )
			goto exit;
		// check for semicolon
		if( !(*pIsCompound) )
		{
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Missing_Semicolon, NULL, goto exit);
		}
		goto exit;
	}
	// skip "{"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// begin parsing statements
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_curly_close )
	{
		while( tokenID != tk_curly_close )
		{
			// Parse statement
			err = p_statement(_this, pLocals, pIsCompound);
			if( err )
				goto exit;

			// check for semicolon
			if( !(*pIsCompound) )
			{
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Missing_Semicolon, NULL, goto exit);
			}

			// check for "}"
			err = pFile->PeekToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
		}
	}
	*pIsCompound = JILTrue;
	// step over "}"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

exit:
	// free all locals in this block
	FreeLocalVars(_this, pLocals);
	_this->miBlockLevel--;

	DELETE( pToken );
	DELETE( pLocals );
	return err;
}

//------------------------------------------------------------------------------
// p_statement
//------------------------------------------------------------------------------
// Parse a single statement. A statement always ends with a ";" token, except it
// consists of a block statement.

static JILError p_statement(JCLState* _this, Array_JCLVar* pLocals, JILBool* pIsCompound)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pVar;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	if( CurrentFunc(_this)->miRetFlag )
	{
		EmitWarning(_this, NULL, JCL_WARN_Unreachable_Code);
		p_skip_statement(_this);
		*pIsCompound = JILTrue;
		goto exit;
	}
	CurrentFunc(_this)->miYieldFlag = JILFalse;
	*pIsCompound = JILFalse;
	// check for type declaration
	savePos = pFile->GetLocator(pFile);
	err = IsFullTypeDecl(_this, pToken, pVar, JILFalse);
	if( err == JCL_No_Error )
	{
		err = p_local_decl(_this, pLocals, pVar);
	}
	else if( err == JCL_ERR_No_Type_Declaration )
	{
		pFile->SetLocator(pFile, savePos);
		err = pFile->PeekToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// Parse statement
		switch( tokenID )
		{
			case tk_semicolon:				// empty statement
				break;
			case tk_curly_open:				// block statement
				*pIsCompound = JILTrue;
				err = p_block(_this, pIsCompound);
				break;
			case tk_return:
				err = p_return(_this, pLocals);
				CurrentFunc(_this)->miRetFlag = JILTrue;
				break;
			case tk_throw:
				err = p_throw(_this, pLocals);
				CurrentFunc(_this)->miRetFlag = JILTrue;
				break;
			case tk_if:
				*pIsCompound = JILTrue;
				err = p_if(_this, pLocals);
				break;
			case tk_for:
				*pIsCompound = JILTrue;
				err = p_for(_this, pLocals);
				break;
			case tk_while:
				*pIsCompound = JILTrue;
				err = p_while(_this, pLocals);
				break;
			case tk_break:
				err = p_break(_this, JILFalse);
				break;
			case tk_continue:
				err = p_break(_this, JILTrue);
				break;
			case tk_switch:
				*pIsCompound = JILTrue;
				err = p_switch(_this, pLocals);
				break;
			case tk_do:
				*pIsCompound = JILTrue;
				err = p_do_while(_this, pLocals);
				break;
			case tk_yield:
				err = p_yield(_this, pLocals);
				if( _this->miBlockLevel == 1 )
					CurrentFunc(_this)->miYieldFlag = JILTrue;
				break;
			case tk_clause:
				*pIsCompound = JILTrue;
				err = p_clause(_this, pLocals, NULL);
				break;
			case tk_goto:
				err = p_goto(_this, pLocals);
				break;
			case tk__brk:
				// place brk instruction
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				cg_opcode(_this, op_brk);
				break;
			case tk__selftest:
				*pIsCompound = JILTrue;
				err = p_selftest(_this, pLocals);
				break;
			default:
				// dispatch everything else to parse expression
				err = p_expression(_this, pLocals, NULL, &outType, 0);
				break;
		}
		if( err )
			goto exit;
	}
	else	// error from IsFullTypeDecl()
	{
		ERROR(err, pToken, goto exit);
	}

exit:
	DELETE( pToken );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_local_decl
//------------------------------------------------------------------------------
// Parse declaration / initialization of one or more local variables.

static JILError p_local_decl(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pAnyVar;
	JILLong tokenID;
	JILLong localVarMode;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );
	localVarMode = GetOptions(_this)->miLocalVarMode;

	for(;;)
	{
		err = MakeLocalVar(_this, pLocals, localVarMode, pVar);
		ERROR_IF(err, err, pVar->mipName, goto exit);
		// peek if we have a "=" token...
		err = pFile->PeekToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_assign )
		{
			pAnyVar = FindAnyVar(_this, pVar->mipName);
			ERROR_IF(!pAnyVar, JCL_ERR_Not_An_LValue, pVar->mipName, goto exit);
			// compile expression and init variable
			err = p_assignment(_this, pLocals, pAnyVar, &outType);
			if( err )
				goto exit;
		}
		else
		{
			// no assignment, default-initialize variable
			pAnyVar = FindAnyVar(_this, pVar->mipName);
			ERROR_IF(!pAnyVar, JCL_ERR_Not_An_LValue, pVar->mipName, goto exit);
			err = cg_init_var(_this, pAnyVar);
			ERROR_IF(err && err != JCL_ERR_Ctor_Is_Explicit, err, pAnyVar->mipName, goto exit);
		}
		// peek if we have a "," token...
		err = pFile->PeekToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID != tk_comma )
			break;
		// skip ","
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// get next identifier name
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
		JCLSetString(pVar->mipName, JCLGetString(pToken));
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_member_decl
//------------------------------------------------------------------------------
// Parse declaration of one or more member variables.

static JILError p_member_decl(JCLState* _this, JILLong classIdx, JCLVar* pVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pPrefix = NULL;
	JILLong tokenID;
	JCLClass* pClass;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pClass = GetClass(_this, classIdx);
	ERROR_IF(!pClass, JCL_ERR_Undefined_Identifier, NULL, goto exit);

	if( !pVar->miConst || IsModifierNativeBinding(pClass) )
	{
		for(;;)
		{
			err = AddMemberVar(_this, classIdx, pVar);
			ERROR_IF(err, err, pVar->mipName, goto exit);
			// peek if we have a "," or ";" token...
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID == tk_semicolon )
				break;
			ERROR_IF(tokenID != tk_comma, JCL_ERR_Unexpected_Token, pToken, goto exit);
			// get next identifier name
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
			JCLSetString(pVar->mipName, JCLGetString(pToken));
		}
	}
	else	// if the member is declared const, make it a global variable and mangle name
	{
		// TODO: Currently all global variables are added to the 'Global Object' regardless from which class they are.
		// To distinguish them, their name is prefixed with the class name. However, this creates countless problems.
		// We should make every class have it's own global object and add class member constants there.
		// Unfortunately this requires large parts of the compiler and runtime to be changed.
		JILLong curArgClass = _this->miArgClass;
		JILLong curClass = _this->miClass;
		JILLong curFunc = _this->miFunc;
		pPrefix = NEW(JCLString);
		JCLSetString(pPrefix, JCLGetString(pClass->mipName));
		JCLAppend(pPrefix, "::");
		SetCompileContext(_this, type_global, 0);
		_this->miArgClass = curClass; // HACK: Make sure FindAnyVar() also looks in current class
		err = p_global_decl(_this, pVar, pPrefix);
		SetCompileContext(_this, curClass, curFunc);
		_this->miArgClass = curArgClass;
	}

exit:
	DELETE( pToken );
	DELETE( pPrefix );
	return err;
}

//------------------------------------------------------------------------------
// p_return
//------------------------------------------------------------------------------
// Parse the return statement.

static JILError p_return(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pRetVar;
	JCLString* pToken;
	JCLFunc* pFunc;
	JILLong tokenID;
	JILLong numStack;
	TypeInfo outType;
	SMarker marker;

	pRetVar = NEW(JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pFunc = CurrentFunc(_this);
	JCLClrTypeInfo( &outType );
	SetMarker(_this, &marker);

	// return not possible in cofunctions
	if( pFunc->miCofunc )
		ERROR(JCL_ERR_Return_In_Cofunction, pFunc->mipName, goto exit);

	// in constructors, check if all members are inited
	if( !IsGlobalScope(_this, _this->miClass) && CurrentFunc(_this)->miCtor )
	{
		if( !AllMembersInited(_this, _this->miClass, pToken) )
			ERROR(JCL_ERR_Must_Init_All_Members, pToken, goto exit);
	}

	// skip "return" token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// see what's next
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_semicolon )
	{
		if( pFunc->mipResult->miMode != kModeUnused )
			ERROR(JCL_ERR_Must_Return_Value, pFunc->mipName, goto exit);
		goto no_expr;
	}
	else if( pFunc->mipResult->miMode == kModeUnused )
		ERROR(JCL_ERR_Cannot_Return_Value, pFunc->mipName, goto exit);

	// get expression
	pRetVar->Copy(pRetVar, pFunc->mipResult);
	pRetVar->miUsage = kUsageResult;
	pRetVar->miInited = JILFalse;
	err = p_expression(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;

	// check if returning local variable as weak ref
	if( !outType.miRef && IsWeakRef(pRetVar) )
		EmitWarning(_this, NULL, JCL_WARN_Return_WRef_Local);

no_expr:
	// get number of items on stack
	numStack = kSimStackSize - _this->miStackPos;
	numStack -= pFunc->mipArgs->count;		// minus arguments
	numStack -= _this->miNumRegsToSave;		// minus saved registers
	if( numStack < 0 )
		FATALERROREXIT("p_return", "Number of items on stack is negative");
	if( numStack )
	{
		// pop all local variables from stack
		cg_pop_multi(_this, numStack);
		if( _this->miBlockLevel == 1 )	// are we at function level?
			SimStackPop(_this, numStack);
	}
	// restore saved registers
	if( _this->miNumRegsToSave )
	{
		cg_pop_registers(_this, _this->miRegUsage, _this->miNumRegsToSave);
		if( _this->miBlockLevel == 1 )	// are we at function level?
			SimStackPop(_this, _this->miNumRegsToSave);
	}
	cg_return(_this);

exit:
	DELETE( pToken );
	DELETE( pRetVar );
	return err;
}

//------------------------------------------------------------------------------
// p_throw
//------------------------------------------------------------------------------
// Parse the throw statement.

static JILError p_throw(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pRetVar;
	JCLString* pToken;
	JCLFunc* pFunc;
	JCLClass* pClass;
	JILLong tokenID;
	JILLong numStack;
	TypeInfo outType;

	pRetVar = NEW(JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pFunc = CurrentFunc(_this);
	JCLClrTypeInfo( &outType );

	// skip "throw" token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// see what's next
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// get expression
	pRetVar->miType = type_var;
	pRetVar->miRef = JILTrue;
	pRetVar->miUsage = kUsageResult;
	pRetVar->miInited = JILFalse;
	pRetVar->miMode = kModeRegister;
	pRetVar->miIndex = 1;
	err = p_expression(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	// verify it's an exception object
	pClass = GetClass(_this, outType.miType);
	ERROR_IF(pClass->miBaseType != type_exception, JCL_ERR_Throw_Not_Exception, pClass->mipName, goto exit);

	// get number of items on stack
	numStack = kSimStackSize - _this->miStackPos;
	numStack -= pFunc->mipArgs->count;		// minus arguments
	numStack -= _this->miNumRegsToSave;		// minus saved registers
	if( numStack < 0 )
		FATALERROREXIT("p_throw", "Number of items on stack is negative");
	if( numStack )
	{
		// pop all local variables from stack
		cg_pop_multi(_this, numStack);
		if( _this->miBlockLevel == 1 )	// are we at function level?
			SimStackPop(_this, numStack);
	}
	// restore saved registers
	if( _this->miNumRegsToSave )
	{
		cg_pop_registers(_this, _this->miRegUsage, _this->miNumRegsToSave);
		if( _this->miBlockLevel == 1 )	// are we at function level?
			SimStackPop(_this, _this->miNumRegsToSave);
	}
	// generate throw
	cg_opcode(_this, op_throw);
	// return in case exception is cleared by exception handler
	cg_return(_this);

exit:
	DELETE( pToken );
	DELETE( pRetVar );
	return err;
}

//------------------------------------------------------------------------------
// help_force_temp
//------------------------------------------------------------------------------
// Helper function for p_expr_atomic().

static JILError help_force_temp(JCLState* _this, JCLVar** ppDest, JCLVar* pLVar, JCLVar** ppTempVar)
{
	if( !pLVar || !IsTempVar(pLVar) || IsRegisterAccess(pLVar, kReturnRegister) )
	{
		JILLong err = MakeTempVar(_this, ppTempVar, pLVar);
		if( err )
			return err;
		*ppDest = *ppTempVar;
	}
	else
	{
		*ppDest = pLVar;
	}
	return 0;
}

//------------------------------------------------------------------------------
// p_expr_get_variable
//------------------------------------------------------------------------------
// Helper function for p_expr_atomic(). Gets a global or member variable, or a
// reference to a global or member function.

static JILError p_expr_get_variable(JCLState* _this, JCLString* pName, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILBool bThis)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLFunc* pFunc;
	JCLVar* pVarOut;
	JCLString* pToken;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	if( bThis )
	{
		pVarOut = FindMemberVar(_this, _this->miClass, pName);
		if( pVarOut )
		{
			// return result var
			*ppVarOut = pVarOut;
		}
		else
		{
			JCLVar* pThis;
			JCLSetString(pToken, "this");
			pThis = FindLocalVar(_this, pToken);
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pVarOut = *ppVarOut;
			err = FindFuncRef(_this, pName, _this->miClass, kMethod, pVarOut, &pFunc);
			ERROR_IF(err, JCL_ERR_Undefined_Identifier, pName, goto exit);
			err = cg_new_delegate(_this, pFunc->miFuncIdx, pThis, pVarOut);
			ERROR_IF(err, err, pName, goto exit);
		}
	}
	else
	{
		pVarOut = FindAnyVar(_this, pName);
		if( pVarOut )
		{
			// return result var
			*ppVarOut = pVarOut;
		}
		else
		{
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pVarOut = *ppVarOut;
			err = FindAnyFuncRef(_this, pName, pVarOut, &pFunc);
			ERROR_IF(err, JCL_ERR_Undefined_Identifier, pName, goto exit);
			if( !pFunc->miMethod )
			{
				err = cg_new_delegate(_this, pFunc->miHandle, NULL, pVarOut);
				ERROR_IF(err, err, pName, goto exit);
			}
			else if( CurrentFunc(_this)->miMethod )
			{
				JCLVar* pThis;
				JCLSetString(pToken, "this");
				pThis = FindLocalVar(_this, pToken);
				err = FindFuncRef(_this, pName, _this->miClass, kMethod, pVarOut, &pFunc);
				ERROR_IF(err, err, pName, goto exit);
				err = cg_new_delegate(_this, pFunc->miFuncIdx, pThis, pVarOut);
				ERROR_IF(err, err, pName, goto exit);
			}
			else
			{
				ERROR(JCL_ERR_Calling_Method_From_Static, pName, goto exit);
			}
		}
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_atomic
//------------------------------------------------------------------------------
// Parse an atomic operand. An atomic operand can be a literal, a variable,
// a function call, or a full expression enclosed in parentheses.
// If the given pOut variable is not NULL, the given JCLVar will be filled with
// information describing the type of the expression.

static JILError p_expr_atomic(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLClass* pClass;
	JCLVar* pVarOut;
	JCLVar* pWorkVar;
	JCLString* pToken;
	JCLString* pToken2;
	JILLong tokenID;
	JILLong tokenID2;
	JILLong savePos;
	JILBool litIsNegative = JILFalse;
	TypeInfo outType;

	pVarOut = *ppVarOut;
	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// special case for negative literals...
	if( tokenID == tk_minus )
	{
		// check if next token is a long or float literal
		err = pFile->PeekToken(pFile, pToken2, &tokenID2);
		ERROR_IF(err, err, pToken2, goto exit);
		if( tokenID2 == tk_lit_int || tokenID2 == tk_lit_float )
		{
			// skip "-"
			savePos = pFile->GetLocator(pFile);
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			litIsNegative = JILTrue;
		}
	}

	switch( tokenID )
	{
		case tk_lit_int:
			tokenID = type_int;
			if( pLVar && (pLVar->miType == type_int || pLVar->miType == type_float) )
				tokenID = pLVar->miType;
			err = cg_get_literal(_this, tokenID, pToken, pLVar, ppVarOut, ppTempVar, litIsNegative);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		case tk_lit_float:
			err = cg_get_literal(_this, type_float, pToken, pLVar, ppVarOut, ppTempVar, litIsNegative);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		case tk_lit_string:
			err = cg_get_literal(_this, type_string, pToken, pLVar, ppVarOut, ppTempVar, litIsNegative);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		case tk_lit_char:
		{
			int i;
			int c;
			int l = 0;
			if( JCLGetLength(pToken) > sizeof(JILLong) || JCLGetLength(pToken) == 0 )
				ERROR(JCL_ERR_Invalid_Char_Literal, pToken, goto exit);
			for( i = 0; i < JCLGetLength(pToken); i++ )
			{
				c = JCLGetChar(pToken, i);
				l <<= 8;
				l |= (c & 255);
			}
			JCLFormat(pToken, "%d", l);
			err = cg_get_literal(_this, type_int, pToken, pLVar, ppVarOut, ppTempVar, JILFalse);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		}
		case tk_false:
			JCLSetString(pToken2, "0");
			err = cg_get_literal(_this, type_int, pToken2, pLVar, ppVarOut, ppTempVar, JILFalse);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		case tk_true:
			JCLSetString(pToken2, "1");
			err = cg_get_literal(_this, type_int, pToken2, pLVar, ppVarOut, ppTempVar, JILFalse);
			ERROR_IF(err, err, NULL, goto exit);
			break;
		case tk_round_open:
		{
			JILBool bFull = JILTrue;
			TypeInfo destType;
			JCLClrTypeInfo(&destType);
			// parentheses - first check for "cast operator"
			savePos = pFile->GetLocator(pFile);
			err = pFile->GetToken(pFile, pToken2, &tokenID2);
			ERROR_IF(err, err, pToken2, goto exit);
			if( IsTypeName(_this, tokenID2, pToken2, &destType) )
			{
				err = pFile->GetToken(pFile, pToken2, &tokenID2);
				ERROR_IF(err, err, pToken2, goto exit);
				// closing parenthese?
				if( tokenID2 == tk_round_close )
				{
					bFull = JILFalse;
					err = p_cast_operator(_this, pLocals, pLVar, ppVarOut, ppTempVar, &destType);
					if( err )
						goto exit;
				}
			}
			// do expression in parentheses
			if( bFull )
			{
				// return to saved position
				pFile->SetLocator(pFile, savePos);
				err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
				ERROR_IF(err, err, NULL, goto exit);
				pWorkVar = *ppVarOut;
				// parse full expression
				err = p_expression(_this, pLocals, pWorkVar, &outType, flags);
				if( err )
					goto exit;
				// we expect to see a ")"
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
				// copy expression result type
				JCLTypeInfoToVar(&outType, pWorkVar);
				pWorkVar->miInited = JILTrue;
			}
			break;
		}
		case tk_minus:
		{
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pWorkVar = *ppVarOut;
			// recurse
			err = p_expr_primary(_this, pLocals, pWorkVar, &outType, flags);
			if( err )
				goto exit;
			// update type of temp var
			JCLTypeInfoToVar(&outType, pWorkVar);
			// generate neg code
			err = cg_neg_var(_this, pWorkVar);
			ERROR_IF(err, err, pToken, goto exit);
			break;
		}
		case tk_not:
		{
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pWorkVar = *ppVarOut;
			// recurse
			err = p_expr_primary(_this, pLocals, pWorkVar, &outType, flags);
			if( err )
				goto exit;
			// update type of temp var
			JCLTypeInfoToVar(&outType, pWorkVar);
			// generate not code
			err = cg_not_var(_this, pWorkVar);
			ERROR_IF(err, err, pToken, goto exit);
			break;
		}
		case tk_bnot:
		{
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pWorkVar = *ppVarOut;
			// recurse
			err = p_expr_primary(_this, pLocals, pWorkVar, &outType, flags);
			if( err )
				goto exit;
			// update type of temp var
			JCLTypeInfoToVar(&outType, pWorkVar);
			// generate bnot code
			err = cg_bnot_var(_this, pWorkVar);
			ERROR_IF(err, err, pToken, goto exit);
			break;
		}
		case tk__rtchk:
		{
		    ERROR_IF(pLVar == NULL, JCL_ERR_Not_An_LValue, NULL, goto exit);
			err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
			ERROR_IF(err, err, NULL, goto exit);
			pWorkVar = *ppVarOut;
			// recurse
			err = p_expr_primary(_this, pLocals, pWorkVar, &outType, flags);
			if( err )
				goto exit;
			// update type of temp var
			JCLTypeInfoToVar(&outType, pWorkVar);
			// generate RTCHK instruction
			err = cg_rtchk(_this, pWorkVar, pLVar->miType);
			ERROR_IF(err, err, pToken, goto exit);
			break;
		}
		case tk_curly_open:
			err = p_array_init(_this, pLocals, pLVar, ppVarOut, ppTempVar);
			break;
		case tk_new:
			err = p_new(_this, pLocals, pLVar, ppVarOut, ppTempVar);
			break;
		case tk_typeof:
			err = p_typeof(_this, pLocals, pLVar, ppVarOut, ppTempVar);
			break;
		case tk_sameref:
			err = p_sameref(_this, pLocals, pLVar, ppVarOut, ppTempVar);
			break;
		case tk_function:
			err = p_function_literal(_this, pLocals, pLVar, ppVarOut, ppTempVar, flags, kFunction);
			break;
		case tk_method:
			err = p_function_literal(_this, pLocals, pLVar, ppVarOut, ppTempVar, flags, kMethod);
			break;
		case tk_string:
		case tk_array:
		case tk_identifier:
		case tk_scope:
		case tk_this:
		{
			JILBool bThis = JILFalse;
			if( tokenID == tk_scope )
			{
				pFile->SetLocator(pFile, savePos);
				JCLSetString(pToken, kNameGlobalNameSpace);
			}
			else if( tokenID == tk_this )
			{
				// is there a "."?
				err = pFile->PeekToken(pFile, pToken2, &tokenID2);
				ERROR_IF(err, err, pToken2, goto exit);
				if( tokenID2 == tk_point )
				{
					// skip "."
					err = pFile->GetToken(pFile, pToken2, &tokenID2);
					ERROR_IF(err, err, pToken2, goto exit);
					ERROR_IF(tokenID2 != tk_point, JCL_ERR_Unexpected_Token, pToken2, goto exit);
					// expect an identifier name
					err = pFile->GetToken(pFile, pToken, &tokenID2);
					ERROR_IF(err, err, pToken, goto exit);
					ERROR_IF(tokenID2 != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
					bThis = JILTrue;
				}
			}
			err = pFile->PeekToken(pFile, pToken2, &tokenID);
			ERROR_IF(err, err, pToken2, goto exit);
			switch( tokenID )
			{
				case tk_round_open:
				{
					// parse function call
					err = p_function_call(_this, pLocals, pToken, pLVar, &outType);
					if( err )
						goto exit;
					// fill out result type info
					JCLTypeInfoToVar(&outType, pVarOut);
					// fill out result var
					if( outType.miType != type_null )
					{
						pVarOut->miMode = kModeRegister;
						pVarOut->miUsage = kUsageResult;
						pVarOut->miIndex = 1;
						pVarOut->miInited = JILTrue;
					}
					break;
				}
				case tk_scope:
				{
					// static class function call
					ERROR_IF(bThis, JCL_ERR_Unexpected_Token, pToken2, goto exit);
					FindClass(_this, pToken, &pClass);
					ERROR_IF(pClass == NULL, JCL_ERR_Undefined_Identifier, pToken, goto exit);
					ERROR_IF(pClass->miFamily != tf_class, JCL_ERR_Type_Not_Class, pToken, goto exit);
					ERROR_IF(!pClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pToken, goto exit);
					ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_Modifier_Illegal, pToken, goto exit);
					// skip "::"
					err = pFile->GetToken(pFile, pToken, &tokenID);
					ERROR_IF(err, err, pToken, goto exit);
					// expecting function / variable name
					err = pFile->GetToken(pFile, pToken, &tokenID);
					ERROR_IF(err, err, pToken, goto exit);
					ERROR_IF(!IsClassToken(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);
					// check for parenthese
					err = pFile->PeekToken(pFile, pToken2, &tokenID);
					ERROR_IF(err, err, pToken2, goto exit);
					if( tokenID == tk_round_open )
					{
						// call static function
						err = p_member_call(_this, pLocals, pClass->miType, pToken, NULL, pLVar, &outType, 0);
						if( err )
							goto exit;
						// fill out result type info
						JCLTypeInfoToVar(&outType, pVarOut);
						if( outType.miType != type_null )
						{
							pVarOut->miMode = kModeRegister;
							pVarOut->miUsage = kUsageResult;
							pVarOut->miIndex = 1;
							pVarOut->miInited = JILTrue;
						}
					}
					else
					{
						// try to find it as a global var
						pWorkVar = FindGlobalVar(_this, pClass->miType, pToken);
						if( pWorkVar )
						{
							*ppVarOut = pWorkVar;
						}
						else
						{
							JCLFunc* pFunc;
							err = help_force_temp(_this, ppVarOut, pLVar, ppTempVar);
							ERROR_IF(err, err, NULL, goto exit);
							pVarOut = *ppVarOut;
							err = FindFuncRef(_this, pToken, pClass->miType, kFunction, pVarOut, &pFunc);
							ERROR_IF(err, err, NULL, goto exit);
							err = cg_new_delegate(_this, pFunc->miHandle, NULL, pVarOut);
							ERROR_IF(err, err, NULL, goto exit);
						}
					}
					break;
				}
				default:
				{
					err = p_expr_get_variable(_this, pToken, pLVar, ppVarOut, ppTempVar, bThis);
					if( err )
						goto exit;
					break;
				}
			}
			break;
		}
		default:
			pFile->SetLocator(pFile, savePos);
			ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
	}

exit:
	DELETE( pToken );
	DELETE( pToken2 );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_get_array
//------------------------------------------------------------------------------
// Parse operator [] (array element access) in an expression.

static JILError p_expr_get_array(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pTempIdx = NULL;
	JCLVar* pVar;
	JCLVar* pVarOut;
	JCLVar* pTempVar;
	JILLong tokenID;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	pVarOut = *ppVarOut;
	pTempVar = *ppTempVar;

	// check if we already have a temp var
	if( pTempVar == NULL )
	{
		err = MakeTempVar(_this, ppTempVar, pVarOut);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar = *ppTempVar;
		// MOVE from pVarOut to temp var
		pTempVar->miMode = kModeRegister;
		pTempVar->miRef = pVarOut->miRef;
		pTempVar->miUnique = pVarOut->miUnique;
		err = cg_move_var(_this, pVarOut, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		pVarOut = pTempVar;
	}
	else if( pTempVar->miMode != kModeRegister )
	{
		// must dereference first
		pVar->Copy(pVar, pTempVar);
		pVar->mipArrIdx = pTempVar->mipArrIdx;
		pTempVar->miMode = kModeRegister;
		pTempVar->mipArrIdx = NULL;
		err = cg_move_var(_this, pVar, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		FreeTempVar(_this, &(pVar->mipArrIdx));
		pVarOut = pTempVar;
	}
	// check that it is an array
	if( pVarOut->miType != type_array && pVarOut->miType != type_var )
		ERROR(JCL_ERR_Not_An_Array, NULL, goto exit);
	// create temp var for array access
	err = MakeTempVar(_this, &pTempIdx, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempIdx->miType = type_int;
	pVarOut->mipArrIdx = pTempIdx;
	pVarOut->miMode = kModeArray;
	pVarOut->miType = type_array; // expecting array to contain another array by default
	// skip "["
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	for(;;)
	{
		JCLClrTypeInfo( &outType );
		// parse expression
		err = p_expression(_this, pLocals, pTempIdx, &outType, 0);
		if( err )
			goto exit;
		// expect "]" or ","
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_square_close && tokenID != tk_comma, JCL_ERR_Unexpected_Token, pToken, goto exit);
		if( tokenID == tk_square_close )
			break;
		// move from array into temp var
		pVar->Copy(pVar, pVarOut);
		pVar->miMode = kModeRegister;
		err = cg_move_var(_this, pVarOut, pVar);
		ERROR_IF(err, err, NULL, goto exit);
	}

	*ppVarOut = pVarOut;
	*ppTempVar = pTempVar;

	// see what's next and recurse
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	if( tokenID == tk_square_open )
	{
		err = p_expr_get_array(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}

	// we have moved an array element into our var, so change our type accordingly
	pVarOut->miType = pVarOut->miElemType;
	pVarOut->miRef = pVarOut->miElemRef;

	if( tokenID == tk_round_open )
	{
		err = p_expr_call_variable(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_point )
	{
		err = p_expr_get_member(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}

exit:
	DELETE( pToken );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_get_member
//------------------------------------------------------------------------------
// Parse operator . (dot) in an expression.

static JILError p_expr_get_member(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pToken2;
	JCLVar* pVar;
	JCLVar* pVarOut;
	JCLVar* pTempVar;
	JILLong tokenID;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	pVarOut = *ppVarOut;
	pTempVar = *ppTempVar;

	// check if we already have a temp var
	if( pTempVar == NULL )
	{
		err = MakeTempVar(_this, ppTempVar, pVarOut);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar = *ppTempVar;
		// MOVE from pVarOut to temp var
		pTempVar->miMode = kModeRegister;
		pTempVar->miRef = pVarOut->miRef;
		pTempVar->miUnique = pVarOut->miUnique;
		err = cg_move_var(_this, pVarOut, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		pVarOut = pTempVar;
	}
	else if( pTempVar->miMode != kModeRegister )
	{
		// must dereference first
		pVar->Copy(pVar, pTempVar);
		pVar->mipArrIdx = pTempVar->mipArrIdx;
		pTempVar->miMode = kModeRegister;
		pTempVar->mipArrIdx = NULL;
		err = cg_move_var(_this, pVar, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		FreeTempVar(_this, &(pVar->mipArrIdx));
		pVarOut = pTempVar;
	}

	// skip the "."
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// we expect to see a member (or class) identifier
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(!IsClassToken(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);

	// check what comes next...
	err = pFile->PeekToken(pFile, pToken2, &tokenID);
	ERROR_IF(err, err, pToken2, goto exit);
	if( tokenID == tk_round_open )
	{
		// check if it's an object
		if( !IsVarClassType(_this, pVarOut) )
			ERROR(JCL_ERR_Not_An_Object, pVarOut->mipName, goto exit);
		// function call
		if( !ClassHasBody(_this, pVarOut->miType) )
			ERROR(JCL_ERR_Class_Only_Forwarded, pToken, goto exit);
		if( IsModifierNativeBinding(GetClass(_this, pVarOut->miType)) )
			ERROR(JCL_ERR_Native_Modifier_Illegal, pToken, goto exit);
		JCLClrTypeInfo( &outType );
		// first attempt first-class function or cofunction
		err = p_variable_call(_this, pLocals, pToken, pVarOut, pLVar, &outType, 0);
		if( err == JCL_ERR_Undefined_Identifier )
			err = p_member_call(_this, pLocals, pVarOut->miType, pToken, pVarOut, pLVar, &outType, 0);
		if( err )
			goto exit;
		// check for "." or "["
		err = pFile->PeekToken(pFile, pToken2, &tokenID);
		ERROR_IF(err, err, pToken2, goto exit);
		// if the function returns a result and we have an l-value, we move it to pVarOut
		if( outType.miType != type_null &&
			(tokenID == tk_point ||
			tokenID == tk_square_open ||
			tokenID == tk_round_open ||
			IsAssignOperator(tokenID) ||
			pLVar != NULL) )
		{
			// MOVE result from r1 to pVarOut
			JCLTypeInfoToVar(&outType, pVarOut);
			pVar->Copy(pVar, pVarOut);
			pVar->miMode = kModeRegister;
			pVar->miUsage = kUsageResult;
			pVar->miIndex = 1;
			err = cg_move_var(_this, pVar, pVarOut);
			ERROR_IF(err, err, NULL, goto exit);
			pVarOut->miInited = JILTrue;
			pVarOut->miConstP = pVarOut->miConst;
		}
		else
		{
			JCLTypeInfoToVar(&outType, pVarOut);
			pVarOut->miMode = kModeUnused;
		}
	}
	else if( tokenID == tk_scope )
	{
		// search for class
		JCLClass* pClass = NULL;
		FindClass(_this, pToken, &pClass);
		ERROR_IF(!pClass, JCL_ERR_Undefined_Identifier, pToken, goto exit);
		if( pVarOut->miIniType != type_array && pVarOut->miType != type_var )
		{
			if( !IsVarClassType(_this, pVarOut) )
				ERROR(JCL_ERR_Not_An_Object, pVarOut->mipName, goto exit);
			if( pVarOut->miType != pClass->miType &&
				!IsSubClass(_this, pVarOut->miType, pClass->miType) &&
				!IsSuperClass(_this, pVarOut->miType, pClass->miType) )
				ERROR(JCL_ERR_Incompatible_Type, pToken, goto exit);
		}
		else if( pVarOut->miIniType == type_array )
		{
			if( pVarOut->miType != type_var )
			{
				if( pClass->miType != type_array &&
					pVarOut->miType != pClass->miType &&
					!IsSubClass(_this, pVarOut->miType, pClass->miType) &&
					!IsSuperClass(_this, pVarOut->miType, pClass->miType) )
					ERROR(JCL_ERR_Incompatible_Type, pToken, goto exit);
			}
		}
		if( pVarOut->miType != pClass->miType )
		{
			pVarOut->miType = pClass->miType;
			if( GetOptions(_this)->miUseRTCHK )
			{
				err = cg_rtchk(_this, pVarOut, pClass->miType);
				ERROR_IF(err, err, pToken, goto exit);
			}
		}
		// recurse...
		*ppVarOut = pVarOut;
		*ppTempVar = pTempVar;
		err = p_expr_get_member(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else
	{
		JCLFunc* pFunc;
		// check if it's an object
		if( !IsVarClassType(_this, pVarOut) )
			ERROR(JCL_ERR_Not_An_Object, pVarOut->mipName, goto exit);
		if( !ClassHasBody(_this, pVarOut->miType) )
			ERROR(JCL_ERR_Class_Only_Forwarded, pToken, goto exit);
		if( IsModifierNativeBinding(GetClass(_this, pVarOut->miType)) )
			ERROR(JCL_ERR_Native_Modifier_Illegal, pToken, goto exit);
		// first try to find an accessor function
		FindAccessor(_this, pVarOut->miType, pToken, 0, &pFunc);
		if( pFunc )
		{
			JCLClrTypeInfo( &outType );
			err = p_accessor_call(_this, pLocals, pFunc, pVarOut, pLVar, &outType);
			if( err )
				goto exit;
			// check for "." or "["
			err = pFile->PeekToken(pFile, pToken2, &tokenID);
			ERROR_IF(err, err, pToken2, goto exit);
			// if the function returns a result and we have an l-value, we move it to pVarOut
			if( outType.miType != type_null &&
				(tokenID == tk_point ||
				tokenID == tk_square_open ||
				tokenID == tk_round_open ||
				IsAssignOperator(tokenID) ||
				pLVar != NULL) )
			{
				// MOVE result from r1 to pVarOut
				JCLTypeInfoToVar(&outType, pVarOut);
				pVar->Copy(pVar, pVarOut);
				pVar->miMode = kModeRegister;
				pVar->miUsage = kUsageResult;
				pVar->miIndex = 1;
				err = cg_move_var(_this, pVar, pVarOut);
				ERROR_IF(err, err, NULL, goto exit);
				pVarOut->miInited = JILTrue;
				pVarOut->miConstP = pVarOut->miConst;
			}
			else
			{
				JCLTypeInfoToVar(&outType, pVarOut);
				pVarOut->miMode = kModeUnused;
			}
		}
		else
		{
			// find member variable
			JCLVar* pWorkVar = FindMemberVar(_this, pVarOut->miType, pToken);
			if( pWorkVar )
			{
				// dereference next level
				pVarOut->miMode = kModeMember;
				pVarOut->miMember = pWorkVar->miMember;
				pVarOut->miConstP = pVarOut->miConst;
				pVarOut->CopyType(pVarOut, pWorkVar);
				pVarOut->miInited = pWorkVar->miInited;
			}
			else
			{
				pVar->Copy(pVar, pVarOut);
				err = FindFuncRef(_this, pToken, pVarOut->miType, kMethod, pVarOut, &pFunc);
				ERROR_IF(err, err, pToken, goto exit);
				err = cg_new_delegate(_this, pFunc->miFuncIdx, pVar, pVarOut);
				ERROR_IF(err, err, pToken, goto exit);
			}
		}
	}

	*ppVarOut = pVarOut;
	*ppTempVar = pTempVar;

	// check what comes next and recurse
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	if( tokenID == tk_square_open )
	{
		err = p_expr_get_array(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_round_open )
	{
		err = p_expr_call_variable(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_point )
	{
		err = p_expr_get_member(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}

exit:
	DELETE( pToken );
	DELETE( pToken2 );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_call_variable
//------------------------------------------------------------------------------
// Parse operator () (function call) in an expression.

static JILError p_expr_call_variable(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pVar;
	JCLVar* pVarOut;
	JCLVar* pTempVar;
	JILLong tokenID;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	pVarOut = *ppVarOut;
	pTempVar = *ppTempVar;

	// check if we already have a temp var
	if( pTempVar == NULL )
	{
		err = MakeTempVar(_this, ppTempVar, pVarOut);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar = *ppTempVar;
		// MOVE from pVarOut to temp var
		pTempVar->miMode = kModeRegister;
		pTempVar->miRef = pVarOut->miRef;
		pTempVar->miUnique = pVarOut->miUnique;
		err = cg_move_var(_this, pVarOut, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		pVarOut = pTempVar;
	}
	else if( pTempVar->miMode != kModeRegister )
	{
		// must dereference first
		pVar->Copy(pVar, pTempVar);
		pVar->mipArrIdx = pTempVar->mipArrIdx;
		pTempVar->miMode = kModeRegister;
		pTempVar->mipArrIdx = NULL;
		err = cg_move_var(_this, pVar, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		FreeTempVar(_this, &(pVar->mipArrIdx));
		pVarOut = pTempVar;
	}
	switch( TypeFamily(_this, pVarOut->miType) )
	{
		case tf_thread:
			err = p_cofunction_resume(_this, pVarOut, NULL, pLVar, &outType);
			break;
		case tf_delegate:
			err = p_delegate_call(_this, pLocals, pVarOut, NULL, pLVar, &outType, 0);
			break;
		default:
			ERROR(JCL_ERR_Invalid_Variable_Call, NULL, goto exit);
	}
	if( err )
		goto exit;

	// check for "." or "["
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// if the function returns a result and we have an l-value, we move it to pVarOut
	if( outType.miType != type_null &&
		(tokenID == tk_point ||
		tokenID == tk_square_open ||
		tokenID == tk_round_open ||
		IsAssignOperator(tokenID) ||
		pLVar != NULL) )
	{
		// MOVE result from r1 to pVarOut
		JCLTypeInfoToVar(&outType, pVarOut);
		pVar->Copy(pVar, pVarOut);
		pVar->miMode = kModeRegister;
		pVar->miUsage = kUsageResult;
		pVar->miIndex = 1;
		err = cg_move_var(_this, pVar, pVarOut);
		ERROR_IF(err, err, NULL, goto exit);
		pVarOut->miInited = JILTrue;
		pVarOut->miConstP = pVarOut->miConst;
	}
	else
	{
		JCLTypeInfoToVar(&outType, pVarOut);
		pVarOut->miMode = kModeUnused;
	}

	*ppVarOut = pVarOut;
	*ppTempVar = pTempVar;

	if( tokenID == tk_square_open )
	{
		err = p_expr_get_array(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_round_open )
	{
		err = p_expr_call_variable(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_point )
	{
		err = p_expr_get_member(_this, pLocals, pLVar, ppVarOut, ppTempVar);
		if( err )
			goto exit;
	}

exit:
	DELETE( pToken );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_primary
//------------------------------------------------------------------------------
// Parse an expression containing primary operators [] . = ++ --

static JILError p_expr_primary(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pVar;
	JCLVar* pVar2;
	JCLVar* pTempVar = NULL;
	JCLVar* pVarOut;
	JCLVar* pLVarDup = NULL;
	JCLVar* pLVarDup2 = NULL;
	JCLString* pToken;
	JCLString* pToken2;
	JILLong preIncDec = 0;
	JILLong tokenID;
	JILLong	savePos;
	TypeInfo outType;

	pVar = NEW(JCLVar);
	pVar2 = NEW(JCLVar);
	pVarOut = pVar2;
	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	if( pLVar )
	{
		DuplicateVar(&pLVarDup2, pLVar);
		pLVarDup = pLVarDup2;
	}

	//--------------------------------------------------------------------------
	// pre increment / decrement
	//--------------------------------------------------------------------------
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_plusplus || tokenID == tk_minusminus )
		preIncDec = tokenID;
	else
		pFile->SetLocator(pFile, savePos);

	//--------------------------------------------------------------------------
	// parse atomic expression
	//--------------------------------------------------------------------------
	err = p_expr_atomic(_this, pLocals, pLVarDup, &pVarOut, &pTempVar, flags);
	if( err )
		goto exit;
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	//--------------------------------------------------------------------------
	// parse [] () .
	//--------------------------------------------------------------------------
	if( tokenID == tk_square_open )
	{
		err = p_expr_get_array(_this, pLocals, pLVar, &pVarOut, &pTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_round_open )
	{
		err = p_expr_call_variable(_this, pLocals, pLVar, &pVarOut, &pTempVar);
		if( err )
			goto exit;
	}
	else if( tokenID == tk_point )
	{
		err = p_expr_get_member(_this, pLocals, pLVar, &pVarOut, &pTempVar);
		if( err )
			goto exit;
	}
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	//--------------------------------------------------------------------------
	// assignment
	//--------------------------------------------------------------------------
	if( IsAssignOperator(tokenID) )
	{
		if( preIncDec )
		{
			JCLSetString(pToken2, preIncDec == tk_plusplus ? "++" : "--");
			ERROR(JCL_ERR_Unexpected_Token, pToken2, goto exit);
		}
		ERROR_IF(pVarOut->miMode == kModeUnused, JCL_ERR_Not_An_LValue, pToken, goto exit);
		if( IsTempVar(pVarOut) || IsResultVar(pVarOut) )
			EmitWarning(_this, pToken, JCL_WARN_Operator_No_Effect);
		err = p_assignment(_this, pLocals, pVarOut, &outType);
		if( err )
			goto exit;
	}

	//--------------------------------------------------------------------------
	// pre increment / decrement
	//--------------------------------------------------------------------------
	if( preIncDec )
	{
		JCLSetString(pToken2, preIncDec == tk_plusplus ? "++" : "--");
		ERROR_IF(pVarOut->miMode == kModeUnused, JCL_ERR_Not_An_LValue, pToken2, goto exit);
		err = cg_incdec_var(_this, pVarOut, (preIncDec == tk_plusplus));
		ERROR_IF(err, err, pToken2, goto exit);
	}

	//--------------------------------------------------------------------------
	// post increment / decrement
	//--------------------------------------------------------------------------
	if( tokenID == tk_plusplus || tokenID == tk_minusminus )
	{
		ERROR_IF(pVarOut->miMode == kModeUnused, JCL_ERR_Not_An_LValue, pToken, goto exit);
		JCLTypeInfoSrcDst(pOut, pVarOut, pLVar);
		if( pLVar && IsTempVar(pLVar) )
		{
			// MOVE result to destination
			err = cg_move_var(_this, pVarOut, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
			// copy temp var
			err = cg_modify_temp(_this, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
		}
		else if( pLVar )
		{
			// copy result to destination
			JCLVar* pDup = NULL;
			DuplicateVar(&pDup, pLVar);
			pDup->miRef = JILFalse;	// force copying!
			err = cg_move_var(_this, pVarOut, pDup);
			FreeDuplicate(&pDup);
			ERROR_IF(err, err, NULL, goto exit);
		}
		if( IsRegisterAccess(pVarOut, kReturnRegister) )
		{
			// Fix: Post inc/dec on return register should have no effect
			JCLSetString(pToken2, tokenID == tk_plusplus ? "++" : "--");
			EmitWarning(_this, pToken2, JCL_WARN_Operator_No_Effect);
		}
		else
		{
			err = cg_incdec_var(_this, pVarOut, (tokenID == tk_plusplus));
			ERROR_IF(err, err, pToken, goto exit);
		}
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	else
	{
		// if result is in a temp var, MOVE it to destination
		JCLTypeInfoSrcDst(pOut, pVarOut, pLVar);
		if( pLVar )
		{
			err = cg_move_var(_this, pVarOut, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
		}
	}

exit:
	FreeTempVar(_this, &pTempVar);
	FreeDuplicate( &pLVarDup2 );
	DELETE( pToken );
	DELETE( pToken2 );
	DELETE( pVar );
	DELETE( pVar2 );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_mul_div
//------------------------------------------------------------------------------
// Parse an expression containing * and / operators.

static JILError p_expr_mul_div(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_primary(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator * / % ?
		if( tokenID == tk_mul || tokenID == tk_div || tokenID == tk_mod )
		{
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_primary(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change dst type to calculatable
			err = cg_convert_calc(_this, pRetVar, pTempVar);
			ERROR_IF(err, err, pToken, goto exit);
			// favor higher precision!
			if( pTempVar->miType == type_float &&
				pRetVar->miType == type_int )
			{
				// convert left operand to float
				err = cg_cvf_var(_this, pRetVar, pRetVar);
				ERROR_IF(err, err, pToken, goto exit);
				pRetVar->miType = type_float;
			}
			// create code
			err = cg_math_var(_this, pTempVar, pRetVar, tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			// free temp var
			FreeTempVar(_this, &pTempVar);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_add_sub
//------------------------------------------------------------------------------
// Parse an expression containing + and - operators.

static JILError p_expr_add_sub(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JCLString* pToken2;
	JILLong tokenID;
	JILLong tokenID2;
	JILLong tokenID3;
	JILLong savePos;
	JILLong savePos2;
	JILBool notDone = JILTrue;
	JILBool bOpt;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_mul_div(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator + or - ?
		if( tokenID == tk_plus || tokenID == tk_minus )
		{
			// optimization for the case of "x + 1" or "x - 1"
			savePos2 = pFile->GetLocator(pFile);
			err = pFile->GetToken(pFile, pToken, &tokenID2);
			ERROR_IF(err, err, pToken, goto exit);
			err = pFile->PeekToken(pFile, pToken2, &tokenID3);
			ERROR_IF(err, err, pToken, goto exit);
			bOpt = JILFalse;
			if( !IsOperatorToken(tokenID3) && tokenID2 == tk_lit_int )
			{
				if( IsTempVar(pRetVar) && !pRetVar->miUnique )	// don't do this if this would directly alter a variable!
				{
					pRetVar->miConst = JILFalse;
					if( strcmp(JCLGetString(pToken), "1") == 0 )
					{
						err = cg_incdec_var(_this, pRetVar, (tokenID == tk_plus));
						ERROR_IF(err, err, NULL, goto exit);
						bOpt = JILTrue;
					}
					else if( strcmp(JCLGetString(pToken), "2") == 0 )
					{
						err = cg_incdec_var(_this, pRetVar, (tokenID == tk_plus));
						ERROR_IF(err, err, NULL, goto exit);
						err = cg_incdec_var(_this, pRetVar, (tokenID == tk_plus));
						ERROR_IF(err, err, NULL, goto exit);
						bOpt = JILTrue;
					}
				}
			}
			if( !bOpt )
			{
				pFile->SetLocator(pFile, savePos2);
				// get temp var
				err = MakeTempVar(_this, &pTempVar, pRetVar);
				ERROR_IF(err, err, NULL, goto exit);
				pTempVar->miType = type_var;
				JCLClrTypeInfo( &outType );
				// get second operand
				err = p_expr_mul_div(_this, pLocals, pTempVar, &outType, 0);
				if( err )
					goto exit;
				JCLTypeInfoToVar(&outType, pTempVar);
				// change dst type to calculatable
				err = cg_convert_calc(_this, pRetVar, pTempVar);
				ERROR_IF(err, err, pToken, goto exit);
				// favor higher precision!
				if( pTempVar->miType == type_float &&
					pRetVar->miType == type_int )
				{
					// convert left operand to float
					err = cg_cvf_var(_this, pRetVar, pRetVar);
					ERROR_IF(err, err, pToken, goto exit);
					pRetVar->miType = type_float;
				}
				// create code
				err = cg_math_var(_this, pTempVar, pRetVar, tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				// free temp var
				FreeTempVar(_this, &pTempVar);
			}
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	DELETE( pToken2 );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_log_shift
//------------------------------------------------------------------------------
// Parse an expression containing logical shift operators << and >>

static JILError p_expr_log_shift(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_add_sub(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator << or >> ?
		if( tokenID == tk_lshift || tokenID == tk_rshift )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_add_sub(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change dst type if var
			err = cg_cast_if_typeless(_this, pTempVar, pRetVar);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_and_or_xor_var(_this, pTempVar, pRetVar, tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			// free temp var
			FreeTempVar(_this, &pTempVar);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_gt_lt
//------------------------------------------------------------------------------
// Parse an expression containing > and < operators.

static JILError p_expr_gt_lt(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_log_shift(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	// see what comes next
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// operator > >= < <= ?
	if( tokenID == tk_greater
	||	tokenID == tk_greater_equ
	||	tokenID == tk_less
	||	tokenID == tk_less_equ )
	{
		// get temp var
		err = MakeTempVar(_this, &pTempVar, pRetVar);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar->miType = type_var;
		JCLClrTypeInfo( &outType );
		// get second operand
		err = p_expr_log_shift(_this, pLocals, pTempVar, &outType, 0);
		if( err )
			goto exit;
		JCLTypeInfoToVar(&outType, pTempVar);
		// change type to comparable
		err = cg_convert_compare(_this, pRetVar, pTempVar);
		ERROR_IF(err, err, pToken, goto exit);
		// create code
		err = cg_compare_var(_this, tokenID, pTempVar, pRetVar, pLVar);
		ERROR_IF(err, err, pToken, goto exit);
		// result type of compare is always long!
		JCLSetTypeInfo(pOut, type_int, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
		// free temp var
		FreeTempVar(_this, &pTempVar);
	}
	else
	{
		pFile->SetLocator(pFile, savePos);
		JCLTypeInfoFromVar(pOut, pRetVar);
		// report unique status up
		pLVar->miUnique = pRetVar->miUnique;
	}

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_eq_ne
//------------------------------------------------------------------------------
// Parse an expression containing == and != operators.

static JILError p_expr_eq_ne(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong tokenID2;
	JILLong savePos;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_gt_lt(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	// see what comes next
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// operator == != ?
	if( tokenID == tk_equ || tokenID == tk_not_equ )
	{
		// peek for token 'null'
		err = pFile->PeekToken(pFile, pToken, &tokenID2);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID2 == tk_null )
		{
			// skip the token
			err = pFile->GetToken(pFile, pToken, &tokenID2);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_testnull_var(_this, tokenID, pRetVar, pLVar);
			ERROR_IF(err, err, pToken, goto exit);
			// result type of this test is always long
			JCLSetTypeInfo(pOut, type_int, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
		}
		else
		{
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_gt_lt(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change type to comparable
			err = cg_convert_compare(_this, pRetVar, pTempVar);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_compare_var(_this, tokenID, pTempVar, pRetVar, pLVar);
			ERROR_IF(err, err, pToken, goto exit);
			// result type of compare is always long!
			JCLSetTypeInfo(pOut, type_int, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
			// free temp var
			FreeTempVar(_this, &pTempVar);
		}
	}
	else
	{
		pFile->SetLocator(pFile, savePos);
		JCLTypeInfoFromVar(pOut, pRetVar);
		// report unique status up
		pLVar->miUnique = pRetVar->miUnique;
	}

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_band
//------------------------------------------------------------------------------
// Parse an expression containing bitwise "&" operator

static JILError p_expr_band(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_eq_ne(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator & ?
		if( tokenID == tk_band )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_eq_ne(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change dst type if var
			err = cg_cast_if_typeless(_this, pTempVar, pRetVar);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_and_or_xor_var(_this, pTempVar, pRetVar, tk_band);
			ERROR_IF(err, err, pToken, goto exit);
			// free temp var
			FreeTempVar(_this, &pTempVar);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_xor
//------------------------------------------------------------------------------
// Parse an expression containing bitwise "^" operator

static JILError p_expr_xor(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_band(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator ^ ?
		if( tokenID == tk_xor )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_band(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change dst type if var
			err = cg_cast_if_typeless(_this, pTempVar, pRetVar);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_and_or_xor_var(_this, pTempVar, pRetVar, tk_xor);
			ERROR_IF(err, err, pToken, goto exit);
			// free temp var
			FreeTempVar(_this, &pTempVar);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_bor
//------------------------------------------------------------------------------
// Parse an expression containing bitwise "|" operator

static JILError p_expr_bor(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_xor(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator | ?
		if( tokenID == tk_bor )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// get temp var
			err = MakeTempVar(_this, &pTempVar, pRetVar);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = type_var;
			JCLClrTypeInfo( &outType );
			// get second operand
			err = p_expr_xor(_this, pLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			// change dst type if var
			err = cg_cast_if_typeless(_this, pTempVar, pRetVar);
			ERROR_IF(err, err, pToken, goto exit);
			// create code
			err = cg_and_or_xor_var(_this, pTempVar, pRetVar, tk_bor);
			ERROR_IF(err, err, pToken, goto exit);
			// free temp var
			FreeTempVar(_this, &pTempVar);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_and
//------------------------------------------------------------------------------
// Parse an expression containing "&&", "and" operator

static JILError p_expr_and(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;
	JILLong codePos;
	Array_JILLong* pCode;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_bor(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator &&, and ?
		if( tokenID == tk_and )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// test if we already have 'false', then we can skip the next expression
			if (!IsTempVar(pRetVar))
				FATALERROREXIT("p_expr_and", "First operand is not a temp-var!");
			codePos = GetCodeLocator(_this);
			cg_opcode(_this, op_tsteq_r);
			cg_opcode(_this, pRetVar->miIndex);
			cg_opcode(_this, 0);
			// get second operand
			JCLClrTypeInfo( &outType );
			err = p_expr_bor(_this, pLocals, pRetVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pRetVar);
			// convert operand to int
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// create code
			pCode = CurrentOutFunc(_this)->mipCode;
			pCode->Set(pCode, codePos + 2, GetCodeLocator(_this) - codePos);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expr_or
//------------------------------------------------------------------------------
// Parse an expression containing "||", "or" operator

static JILError p_expr_or(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pRetVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILBool notDone = JILTrue;
	TypeInfo outType;
	JILLong codePos;
	Array_JILLong* pCode;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	DuplicateVar(&pRetVar, pLVar);
	pRetVar->miType = type_var;

	// get first operand
	err = p_expr_and(_this, pLocals, pRetVar, &outType, 0);
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pRetVar);

	while( notDone )
	{
		// see what comes next
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// operator ||, or ?
		if( tokenID == tk_or )
		{
			// convert operand to int (is only done once)
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// test if we already have 'true', then we can skip the next expression
			if (!IsTempVar(pRetVar))
				FATALERROREXIT("p_expr_or", "First operand is not a temp-var!");
			codePos = GetCodeLocator(_this);
			cg_opcode(_this, op_tstne_r);
			cg_opcode(_this, pRetVar->miIndex);
			cg_opcode(_this, 0);
			// get second operand
			JCLClrTypeInfo( &outType );
			err = p_expr_and(_this, pLocals, pRetVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pRetVar);
			// convert operand to int
			err = cg_convert_to_type(_this, pRetVar, type_int);
			ERROR_IF(err, err, NULL, goto exit);
			// create code
			pCode = CurrentOutFunc(_this)->mipCode;
			pCode->Set(pCode, codePos + 2, GetCodeLocator(_this) - codePos);
			// fix: operator result is per definition never const
			pRetVar->miConst = JILFalse;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			notDone = JILFalse;
		}
	}
	JCLTypeInfoFromVar(pOut, pRetVar);
	// report unique status up
	pLVar->miUnique = pRetVar->miUnique;

exit:
	FreeDuplicate(&pRetVar);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_expression
//------------------------------------------------------------------------------
// Parse a full expression.

static JILError p_expression(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JILError err2 = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pWorkVar;
	JCLVar* pTempVar = NULL;
	JCLVar* pDupLVar = NULL;
	JILLong savePos;
	JILLong tokenID;
	SMarker marker;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// if we don't have an L-Value, we ARE the L-Value, and operators are not allowed
	// left from the assignment, so we can assume an "atomic" expression
	if( !pLVar )
	{
		err = p_expr_primary(_this, pLocals, pLVar, &outType, flags);
		if( !err )
		{
			JCLTypeInfoCopy(pOut, &outType);
			// check if we stopped at an operator token
			err = pFile->PeekToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(IsOperatorToken(tokenID), JCL_ERR_Expression_Without_LValue, pToken, goto exit);
		}
		goto exit;
	}

	// check for special tokens
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	switch( tokenID )
	{
		case tk_null:
			err = cg_load_null(_this, pLVar, &outType);
			ERROR_IF(err, err, pToken, goto exit);
			JCLTypeInfoCopy(pOut, &outType);
			goto exit;
		default:
			pFile->SetLocator(pFile, savePos);
			break;
	}

	// OPTIMIZE: First we try a shortcut, to see if we have no operators. We do this because we
	// can avoid allocating a temporary register if the expression does not contain an operator.
	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	err2 = p_expr_primary(_this, pLocals, pLVar, &outType, flags);
	if( !err2 )
		JCLTypeInfoCopy(pOut, &outType);

	// now check if we stopped at an operator token
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( IsOperatorToken(tokenID) )
	{
		// didn't work - parse full expression
		pFile->SetLocator(pFile, savePos);
		RestoreMarker(_this, &marker);
		JCLClrTypeInfo( &outType );
		if( !IsTempVar(pLVar) )
		{
			err = MakeTempVar(_this, &pTempVar, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
			pWorkVar = pTempVar;
		}
		else
		{
			DuplicateVar(&pDupLVar, pLVar);
			pWorkVar = pDupLVar;
		}
		err = p_expr_or(_this, pLocals, pWorkVar, &outType, flags);
		if( err )
			goto exit;
		JCLTypeInfoToVar(&outType, pWorkVar);
		// if a math operator has converted to float for max precision...
		if( pWorkVar->miType == type_float &&
			pLVar->miType == type_int )
		{
			// convert back to long
			err = cg_cvl_var(_this, pWorkVar, pWorkVar);
			ERROR_IF(err, err, pToken, goto exit);
			pWorkVar->miType = type_int;
		}
		// move tempvar to lvar (and auto-convert type if needed)
		if( pTempVar )
		{
			JCLTypeInfoSrcDst(pOut, pTempVar, pLVar);
			err = cg_move_var(_this, pTempVar, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
		}
		else if( pDupLVar )
		{
			JCLTypeInfoSrcDst(pOut, pDupLVar, pLVar);
			err = cg_move_var(_this, pDupLVar, pLVar);
			ERROR_IF(err, err, NULL, goto exit);
		}
		// report unique status up
		pLVar->miUnique = pWorkVar->miUnique;
		FreeTempVar(_this, &pTempVar);
		FreeDuplicate( &pDupLVar );
	}
	else if( err2 )
	{
		err = err2;
	}

exit:
	FreeTempVar(_this, &pTempVar);
	FreeDuplicate( &pDupLVar );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_assignment
//------------------------------------------------------------------------------
// Parse an assignment. The function assumes the LValue token has already been
// read, the next token is expected to be an assignment operator token.
// Pass the LValue as a JCLVar to this function.

static JILError p_assignment(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, TypeInfo* pOut)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLString* pToken;
	JCLString* pThis;
	JILLong tokenID;
	TypeInfo outType;

	pThis = NEW(JCLString);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLSetString(pThis, "this");
	JCLClrTypeInfo( &outType );

	ERROR_IF(JCLCompare(pLVar->mipName, pThis), JCL_ERR_Not_An_LValue, pThis, goto exit);
	ERROR_IF(IsTempVar(pLVar), JCL_ERR_Not_An_LValue, NULL, goto exit);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(!IsAssignOperator(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);
	if( tokenID == tk_assign )
	{
		// normal assignment is a one liner...
		err = p_expression(_this, pLocals, pLVar, &outType, 0);
		if( err )
			goto exit;
		JCLTypeInfoCopy(pOut, &outType);
		// LValue is now inited
		pLVar->miInited = JILTrue;
	}
	else
	{
		err = MakeTempVar(_this, &pTempVar, pLVar);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar->miType = type_var;
		err = p_expression(_this, pLocals, pTempVar, &outType, 0);
		if( err )
			goto exit;
		JCLTypeInfoToVar(&outType, pTempVar);
		// arithmetic assignment?
		if( IsArithmeticAssign(tokenID) )
			err = cg_math_var(_this, pTempVar, pLVar, tokenID);
		else
			err = cg_and_or_xor_var(_this, pTempVar, pLVar, tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		JCLTypeInfoSrcDst(pOut, pTempVar, pLVar);
		FreeTempVar(_this, &pTempVar);
	}

exit:
	FreeTempVar(_this, &pTempVar);
	DELETE( pThis );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// IsMemberCallError
//------------------------------------------------------------------------------
// TODO: We need this because p_member_call() may return errors that must be tolerated as long as we test-compile other variations of the call. It's not a good solution though.

static JILBool IsMemberCallError(JILError err)
{
	if( err != JCL_No_Error && 
		err != JCL_ERR_Undefined_Function_Call &&
		err != JCL_ERR_Undefined_Identifier &&
		err != JCL_ERR_Error_In_Func_Arg
		)
	{
		return JILTrue;
	}
	else
	{
		return JILFalse;
	}
}

//------------------------------------------------------------------------------
// p_function_call
//------------------------------------------------------------------------------
// Parse a function call. The function assumes that the function name token has
// already been read. The name is passed as a string token to this function.
// The function expects to find a round bracket token in the stream.
// The passed in JCLVar object must describe the result type expected from the
// function call.
// This function will first look in the current class for the function, then
// in the global area.

static JILError p_function_call(JCLState* _this, Array_JCLVar* pLocals, const JCLString* pName, JCLVar* pLVar, TypeInfo* pOut)
{
	JILError err;
	JCLFile* pFile;
	TypeInfo outType;
	SMarker tryMarker;
	JILLong tryPos;
	JILLong i;
	JILLong numFound = 0;
	JILLong idxFound = 0;
	Array_JILLong* pUsing = GetOptions(_this)->mipUsing;

	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// first attempt to call first-class function or cofunction
	err = p_variable_call(_this, pLocals, pName, NULL, pLVar, &outType, 0);
	if( err && err != JCL_ERR_Undefined_Identifier )
		goto exit;
	if( err == JCL_No_Error )
		goto exit;	// we are done

	// if p_member_call fails, we need to return here...
	tryPos = pFile->GetLocator(pFile);
	SetMarker(_this, &tryMarker);

	// first try class member functions
	if( !IsGlobalScope(_this, _this->miClass) )
	{
		err = p_member_call(_this, pLocals, _this->miClass, pName, NULL, pLVar, &outType, 0);
		if( IsMemberCallError(err) )
			goto exit;
		if( err == JCL_No_Error )
			goto exit;	// no need to check for ambiguous access in this case, so we're done
		// return to saved position
		pFile->SetLocator(pFile, tryPos);
		RestoreMarker(_this, &tryMarker);

		// also try parent type
		if( HasParentType(_this, _this->miClass) )
		{
			err = p_member_call(_this, pLocals, GetParentType(_this, _this->miClass), pName, NULL, pLVar, &outType, 0);
			if( IsMemberCallError(err) )
				goto exit;
			if( err == JCL_No_Error )
				goto exit;	// no need to check for ambiguous access in this case, so we're done
			// return to saved position
			pFile->SetLocator(pFile, tryPos);
			RestoreMarker(_this, &tryMarker);
		}
	}

	// if parsing a function argument, also try class being called
	if( _this->miArgClass )
	{
		err = p_member_call(_this, pLocals, _this->miArgClass, pName, NULL, pLVar, &outType, 0);
		if( IsMemberCallError(err) )
			goto exit;
		if( err == JCL_No_Error )
			goto exit;	// no need to check for ambiguous access in this case, so we're done
		// return to saved position
		pFile->SetLocator(pFile, tryPos);
		RestoreMarker(_this, &tryMarker);
	}

	// then try functions in global namespace
	JCLClrTypeInfo( &outType );
	err = p_member_call(_this, pLocals, type_global, pName, NULL, pLVar, &outType, 0);
	if( IsMemberCallError(err) )
		goto exit;
	if( err == JCL_No_Error )
	{
		numFound++;
		idxFound = type_global;
	}
	// return to saved position
	pFile->SetLocator(pFile, tryPos);
	RestoreMarker(_this, &tryMarker);

	// check "using" list for function(s)...
	for( i = 0; i < pUsing->count; i++ )
	{
		// try find function in next class
		JCLClrTypeInfo( &outType );
		err = p_member_call(_this, pLocals, pUsing->array[i], pName, NULL, pLVar, &outType, 0);
		if( IsMemberCallError(err) )
			goto exit;
		if( err == JCL_No_Error )
		{
			numFound++;
			idxFound = pUsing->array[i];
		}
		// return to saved position
		pFile->SetLocator(pFile, tryPos);
		RestoreMarker(_this, &tryMarker);
	}
	if( numFound == 0 )
	{
		ERROR(JCL_ERR_Undefined_Function_Call, pName, goto exit);
	}
	else if( numFound > 1 )
	{
		ERROR(JCL_ERR_Ambiguous_Function_Call, pName, goto exit);
	}
	else
	{
		// now really compile found function
		JCLClrTypeInfo( &outType );
		err = p_member_call(_this, pLocals, idxFound, pName, NULL, pLVar, &outType, 0);
		if( err )
			goto exit;
	}

exit:
	// fill out result type
	JCLTypeInfoCopy(pOut, &outType);
	return err;
}

//------------------------------------------------------------------------------
// p_member_call
//------------------------------------------------------------------------------
// Parse a function call for a specific class and purpose.

static JILError p_member_call(JCLState* _this, Array_JCLVar* pLocals, JILLong classIdx, const JCLString* pName, JCLVar* pObj, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLFunc* pFunc;
	JCLFunc* pProto;
	JCLString* pToken;
	JCLClass* pClass;
	JCLVar* pDupVar = NULL;
	JILLong tokenID;
	TypeInfo outType;

	pClass = GetClass(_this, classIdx);
	pProto = NEW(JCLFunc);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	if( pObj && pObj->miType != classIdx && pObj->miType != type_array && pObj->miElemType != classIdx)
		FATALERROREXIT("p_member_call", "Parameter 'classIdx' != pObj->miType");

	// expecting "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// if we call a method from other class, save r0
	if( pObj && TypeFamily(_this, pObj->miType) != tf_thread )
	{
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
	}

	// Try to find the function
	err = p_match_function(_this, pLocals, classIdx, pName, pLVar, &outType, pProto, &pFunc);
	if( err )
		goto exit;
	// disallow to call constructors manually
	if( pFunc->miCtor && (flags & kOnlyCtor) == 0 )
		ERROR(JCL_ERR_Cannot_Call_Constructor, pName, goto exit);
	else if( !pFunc->miCtor && !pFunc->miCofunc && (flags & kOnlyCtor) )
		ERROR(JCL_ERR_Not_A_Constructor, pName, goto exit);
	// check if context change is allowed (calling member of another class)
	if( pFunc->miMethod && !pFunc->miCtor && _this->miClass != classIdx && !pObj )
		ERROR(JCL_ERR_Cannot_Call_Foreign_Method, pName, goto exit);
	// check if calling method from static function
	if( pFunc->miMethod && !pObj && !CurrentFunc(_this)->miMethod )
		ERROR(JCL_ERR_Calling_Method_From_Static, pName, goto exit);
	// check if calling method and object is not initialized
	if( pFunc->miMethod && pObj && !IsSrcInited(pObj) )
		ERROR(JCL_ERR_Var_Not_Initialized, pName, goto exit);
	// generate code for function call
	if( pClass->miNative )
	{
		if( pFunc->miMethod )
		{
			if( pObj )
			{
				err = cg_change_context(_this, pObj);
				ERROR_IF(err, err, pName, goto exit);
				cg_call_member(_this, pObj->miType, pFunc->miFuncIdx);
			}
			else
			{
				// the initializer block statement allows us to generate bytecode for a "native method" calling a native method
				// in which case we get here
				cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);
			}
		}
		else
		{
			cg_call_native(_this, pClass->miType, pFunc->miFuncIdx);
		}
	}
	else
	{
		if( pFunc->miMethod && pObj )
		{
			err = cg_change_context(_this, pObj);
			ERROR_IF(err, err, pName, goto exit);
			if( pFunc->miCtor && pObj->miType == type_array && IsInterfaceType(_this, pObj->miElemType) )
			{
				// factory construction
				cg_call_factory(_this, pObj->miElemType, pFunc->miFuncIdx);
			}
			else
			{
				// if calling through interface, we must use generic call method!
				// TODO: For now this is sufficient. To support true inheritance, we would need to use generic call also if callee inherits an interface.
				if( pClass->miFamily == tf_interface )
					cg_call_member(_this, pObj->miType, pFunc->miFuncIdx);
				else
					cg_call_static(_this, pFunc->miHandle);		// use cheaper call
			}
		}
		else if( pFunc->miCofunc )
		{
			err = cg_newctx(_this, pObj, classIdx, pFunc->miHandle, pFunc->mipArgs->count);
			ERROR_IF(err, err, pName, goto exit);
			JCLTypeInfoFromVar(&outType, pObj);
		}
		else
		{
			cg_call_static(_this, pFunc->miHandle);
		}
	}
	// pop arguments from stack
	if( pFunc->mipArgs->count )
	{
		cg_pop_multi(_this, pFunc->mipArgs->count);
		SimStackPop(_this, pFunc->mipArgs->count );
	}
	// restore r0
	if( pObj && TypeFamily(_this, pObj->miType) != tf_thread )
	{
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
	}
	JCLTypeInfoCopy(pOut, &outType);

exit:
	FreeDuplicate( &pDupVar );
	DELETE( pProto );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_match_function
//------------------------------------------------------------------------------
// Helper function for p_member_call().
// JUST SO I DO NOT FORGET AGAIN:
// I pass in an instance of JCLFunc in 'pProto' as a working buffer for this
// function, because the stack-arguments pushed on to the SimStack in this
// function can only be freed at the end of the calling function. So the buffer
// must stay alive until the caller is done with the arguments on the SimStack!

static JILError p_match_function(JCLState* _this, Array_JCLVar* pLocals, JILLong classIdx, const JCLString* pName, JCLVar* pLVar, TypeInfo* pOut, JCLFunc* pProto, JCLFunc** ppFunc)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pVar;
	JCLVar* pWorkVar;
	JCLFunc* pFunc;
	JCLString* pToken;
	Array_JCLVar* pArgs;
	JILLong tokenID;
	JILLong savePos;
	JILLong endPos;
	JILLong stModify;
	JILLong saveArgClass;
	int i, j;
	SMarker marker;
	TypeInfo outType;

	*ppFunc = NULL;
	pWorkVar = NEW(JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	saveArgClass = _this->miArgClass;
	_this->miArgClass = classIdx;

	// set up a workvar
	pWorkVar->miMode = kModeRegister;
	pWorkVar->miUsage = kUsageResult;
	pWorkVar->miIndex = 1;
	pWorkVar->miInited = JILTrue;
	pWorkVar->miType = type_var;
	pWorkVar->miRef = JILTrue;
	pWorkVar->miConst = JILTrue;	// Try to prevent "Class has no copy constructor" in 1st pass
	pWorkVar->miElemType = type_var;
	pWorkVar->miElemRef = JILFalse;

	// allocate and fill out our prototype function
	pProto->mipName->Copy(pProto->mipName, pName);
	if( pLVar )
	{
		pProto->mipResult->CopyType(pProto->mipResult, pLVar);
		pProto->mipResult->miMode = kModeRegister;
	}
	else
		pProto->mipResult->miMode = kModeUnused;
	pArgs = pProto->mipArgs;

	//
	// STEP 1: We test-compile to see how many arguments of which type we have
	//
	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_round_close )
	{
		pFile->SetLocator(pFile, savePos);
		while( tokenID != tk_round_close )
		{
			JCLClrTypeInfo( &outType );
			pWorkVar->miInited = JILFalse;
			err = p_expression(_this, pLocals, pWorkVar, &outType, kExpressionProbeMode);
			// TODO: "Remapping" errors really not a good solution. We should explicitly define which error codes must be tolerated when probing.
			ERROR_IF(err == JCL_ERR_Undefined_Function_Call, JCL_ERR_Error_In_Func_Arg, pName, goto exit);
			ERROR_IF(err == JCL_ERR_Undefined_Identifier, JCL_ERR_Error_In_Func_Arg, pName, goto exit);
			if( err )
				goto exit;
			// now we know the type of the argument, add it to the prototype
			pVar = pArgs->New(pArgs);
			JCLTypeInfoToVar(&outType, pVar);
			// expecting now "," or ")"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_comma && tokenID != tk_round_close )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	// we now have a prototype function object we can use to search the function
	i = FindBestPrototype(_this, classIdx, pProto, &pFunc);
	// any candidate found at all?
	ERROR_IF(i == 0, JCL_ERR_Undefined_Function_Call, pName, goto exit);
	// more than one candidate?
	ERROR_IF(i > 1, JCL_ERR_Ambiguous_Function_Call, pName, goto exit);
	// code created is useless, remove it
	endPos = pFile->GetLocator(pFile);
	pFile->SetLocator(pFile, savePos);
	RestoreMarker(_this, &marker);

	//
	// STEP 2: Now really compile the function call, using the found function declaration
	//
	stModify = pFunc->mipArgs->count;
	cg_push_multi(_this, stModify);
	// copy argument vars, so we can modify them but get no problems in recursive calls
	pArgs->Copy(pArgs, pFunc->mipArgs);
	// The arguments must be pushed onto the SimStack, because their stack indexes must be
	// corrected when more data is pushed on the stack while the function executes!
	for( i = stModify - 1; i >= 0; i-- )
	{
		pVar = pArgs->Get(pArgs, i);
		pVar->miUsage = kUsageResult;
		pVar->miInited = JILFalse;
		SimStackPush(_this, pVar, JILTrue);
	}
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit2);
	if( tokenID != tk_round_close )
	{
		j = 0;
		pFile->SetLocator(pFile, savePos);
		while( tokenID != tk_round_close )
		{
			JCLClrTypeInfo( &outType );
			pVar = pArgs->Get(pArgs, j++);
			err = p_expression(_this, pLocals, pVar, &outType, 0);
			if( err )
				goto exit2;
			// expecting now "," or ")"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit2);
			if( tokenID != tk_comma && tokenID != tk_round_close )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit2);
		}
	}
	JCLTypeInfoFromVar(pOut, pFunc->mipResult);
	*ppFunc = pFunc;
	// done
	goto exit;

exit2:
	SimStackPop(_this, stModify);
	pFile->SetLocator(pFile, endPos);

exit:
	_this->miArgClass = saveArgClass;
	DELETE( pWorkVar );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_if
//------------------------------------------------------------------------------
// Parse an if statement.

static JILError p_if(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLString* pToken;
	Array_JILLong* pCode;
	JILLong tokenID;
	JILLong posBranchIf;
	JILLong posBranchEndIf = 0;
	JILBool isCompound;
	TypeInfo outType;
	SInitState orig;
	SInitState cond;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );
	CreateInitState(&orig, _this);
	CreateInitState(&cond, _this);

	// skip the "if"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// expecting "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// use temp var for expression
	err = MakeTempVar(_this, &pTempVar, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempVar->miType = type_int;
	pTempVar->miRef = JILTrue;		// we prefer a reference
	// parse expression
	err = p_expression(_this, pLocals, pTempVar, &outType, 0);
	if( err )
		goto exit;
	// expecting ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// save current code address for later
	posBranchIf = GetCodeLocator(_this);
	cg_opcode(_this, op_tsteq_r);
	cg_opcode(_this, pTempVar->miIndex);
	cg_opcode(_this, 0);
	FreeTempVar(_this, &pTempVar);
	// save init-states of member variables before conditional block
	SaveInitState(&orig);
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	// is there an else?
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_else )
	{
		// fix branch offset
		pCode = CurrentOutFunc(_this)->mipCode;
		pCode->Set(pCode, posBranchIf + 2, GetCodeLocator(_this) - posBranchIf);
		// restore original init states
		RestoreInitState(&orig);
	}
	else
	{
		// skip the "else"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// if the first block did not return, we must branch to end of if now
		if( !CurrentFunc(_this)->miRetFlag )
		{
			posBranchEndIf = GetCodeLocator(_this);
			cg_opcode(_this, op_bra);
			cg_opcode(_this, 0);
		}
		// save current conditional state
		SaveInitState(&cond);
		// restore original state
		RestoreInitState(&orig);
		// fix branch offset
		pCode = CurrentOutFunc(_this)->mipCode;
		pCode->Set(pCode, posBranchIf + 2, GetCodeLocator(_this) - posBranchIf);
		// is there an "if"?
		err = pFile->PeekToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_if )
		{
			// then recurse
			err = p_if(_this, pLocals);
			if( err )
				goto exit;
		}
		else
		{
			// otherwise parse else block
			err = p_block(_this, &isCompound);
			if( err )
				goto exit;
		}
		// fix branch offset
		if( posBranchEndIf )
		{
			pCode = CurrentOutFunc(_this)->mipCode;
			pCode->Set(pCode, posBranchEndIf + 1, GetCodeLocator(_this) - posBranchEndIf);
		}
		// combine conditional and current init states
		AndInitState(&cond);
	}

exit:
	FreeInitState(&orig);
	FreeInitState(&cond);
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_import
//------------------------------------------------------------------------------
// Parse an import statement.

static JILError p_import(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pClassName;
	JCLString* pToken;
	JILLong tokenID;

	pToken = NEW(JCLString);
	pClassName = NEW(JCLString);
	pFile = _this->mipFile;

	// expecting class name
	err = pFile->GetToken(pFile, pClassName, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(!IsClassToken(tokenID), JCL_ERR_Unexpected_Token, pClassName, goto exit);

	for(;;)
	{
		// expecting to see a ";" or "."
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_semicolon )
			break;
		ERROR_IF(tokenID != tk_point, JCL_ERR_Missing_Semicolon, pToken, goto exit);
		JCLAppend(pClassName, ".");
		// expecting to see an identifier
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
		JCLAppend(pClassName, JCLGetString(pToken));
	}

	if( strcmp(JCLGetString(pClassName), "all") == 0 )
		err = p_import_all(_this);
	else
		err = p_import_class(_this, pClassName);

exit:
	DELETE( pClassName );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_import_class
//------------------------------------------------------------------------------
// Import a class.

JILError p_import_class(JCLState* _this, JCLString* pClassName)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLFile* pNewFile = NULL;
	JCLString* pToken = NULL;
	JCLString* pWorkstr = NULL;
	JCLString* pFilePath = NULL;
	JILTypeListItem* pItem;
	JILTypeProc proc;
	const char* pDecl = NULL;
	const char* pBase = NULL;
	const char* pPackage = NULL;
	JCLDeclStruct declStruct = {0};

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pFilePath = NEW(JCLString);

	// check if class is already imported
	pNewFile = FindImport(_this, pClassName);
	if( pNewFile && pNewFile->miPass == _this->miPass )
		goto exit;

	// in 2nd compile pass, we can ignore native types
	if( _this->miPass == kPassCompile && pNewFile->miNative )
		goto exit;

	// are we in PreCompile pass?
	if( _this->miPass == kPassPrecompile )
	{
		JILBool bNative;
		// see if a typelib with that name exists...
		pItem = JILGetNativeType( _this->mipMachine, JCLGetString(pClassName) );
		if( pItem )
		{
			// get the type proc
			proc = pItem->typeProc;
			// try to get package string
			CallNTLGetPackageString(proc, &pPackage);
			// try to get class declaration
			declStruct.pString = NEW(JCLString);
			declStruct.pState = _this->mipMachine;
			err = CallNTLGetDeclString(proc, &declStruct, &pDecl);
			ERROR_IF(err, JCL_ERR_Import_Not_Supported, pClassName, goto exit);
			// try to get base class / interface name
			CallNTLGetBaseName(proc, &pBase);
			// assemble string
			JCLSetString(pToken, "class ");
			JCLAppend(pToken, JCLGetString(pClassName));
			if( pBase )
			{
				JCLAppend(pToken, " : ");
				JCLAppend(pToken, pBase);
			}
			JCLAppend(pToken, " { ");
			JCLAppend(pToken, pDecl ? pDecl : JCLGetString(declStruct.pString));
			JCLAppend(pToken, " } ");
			DELETE( declStruct.pString );
			declStruct.pString = NULL;
			bNative = JILTrue;
		}
		else
		{
			#if JIL_USE_LOCAL_FILESYS
			if( GetOptions(_this)->miAllowFileImport )
			{
				JCLPair* pPair;
				// see if a file with the class name exists...
				pWorkstr = NEW(JCLString);
				pWorkstr->Copy(pWorkstr, pClassName);
				JCLReplace(pWorkstr, ".", JIL_PATHSEPARATORSTR);
				// check if there is an import path defined for the first identifier in the specifier list
				if( JCLSpanExcluding(pClassName, ".", pToken)
					&& (JCLGetLength(pToken) < JCLGetLength(pClassName)) )	// if there is nothing else, assume this is a file
				{
					pPair = Get_JCLCollection(_this->mipImportPaths, pToken);
					if( pPair != NULL )
					{
						JCLReplace(pWorkstr, JCLGetString(pToken), JCLGetString(pPair->mipData));
					}
				}
				JCLAppend(pWorkstr, ".");
				JCLAppend(pWorkstr, JCLGetString(GetOptions(_this)->mipFileExt));
				JCLGetAbsolutePath(_this, pFilePath, pWorkstr);
				// try to open
				if( JCLReadTextFile(pToken, JCLGetString(pFilePath), _this->mipMachine) < 0 )
				{
					ERROR(JCL_ERR_Import_Not_Defined, pClassName, goto exit);
				}
				bNative = JILFalse;
			}
			else
			#endif
			{
				ERROR(JCL_ERR_Import_Not_Defined, pClassName, goto exit);
			}
		}
		// create a new file object
		pNewFile = PushImport(_this, pClassName, pToken, pFilePath, bNative);
		// if we have a package string, import those classes first
		if( pPackage && (*pPackage) )
		{
			JCLClass* test;
			JILLong classIdx;
			// this will act as a "forward declaration" of the class, in case classes from the package string refer to this class
			FindClass(_this, pClassName, &test);
			if( test == NULL )
			{
				err = JCLCreateType(_this, JCLGetString(pClassName), _this->miClass, tf_class, bNative, &classIdx);
				ERROR_IF(err, err, pClassName, goto exit);
			}
			// now import the classes from the package string
			err = p_import_class_list(_this, pPackage);
			if( err )
				goto exit;
		}
	}

	// set current compile pass
	pNewFile->miPass = _this->miPass;
	// reset locator to beginning of file
	pNewFile->SetLocator(pNewFile, 0);
	// set new file scope
	_this->mipFile = pNewFile;
	// push new compiler options
	PushOptions(_this);
	// begin parsing
	err = p_root(_this);
	// pop compiler options from stack
	PopOptions(_this);
	// return to old file scope
	_this->mipFile = pFile;
	if( err )
		goto exit;

	// to save some memory, we can free the file's text contents after the 2nd pass
	if( pNewFile->miPass == kPassCompile )
		pNewFile->Close(pNewFile);

exit:
	DELETE( pToken );
	DELETE( pWorkstr );
	DELETE( pFilePath );
	DELETE( declStruct.pString );
	return err;
}

//------------------------------------------------------------------------------
// p_import_class_list
//------------------------------------------------------------------------------
// Import classes from a comma seperated list.

static JILError p_import_class_list(JCLState* _this, const char* pList)
{
	JILError err = JCL_No_Error;
	JCLString* pToken;
	JCLString* pClassName;

	pToken = NEW(JCLString);
	pClassName = NEW(JCLString);

	JCLSetString(pToken, pList);
	while( !JCLAtEnd(pToken) )
	{
		JCLSpanExcluding(pToken, ",", pClassName);
		JCLSeekForward(pToken, 1);
		JCLTrim(pClassName);
		if( JCLGetLength(pClassName) )
		{
			err = p_import_class(_this, pClassName);
			if( err )
				goto exit;
		}
	}

exit:
	DELETE( pToken );
	DELETE( pClassName );
	return err;
}

//------------------------------------------------------------------------------
// p_import_all
//------------------------------------------------------------------------------
// Parse an import all statement.

static JILError p_import_all(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLString* pClassName;
	JILLong numTypes;
	JILLong t;

	pClassName = NEW(JCLString);

	if( _this->miPass == kPassPrecompile )
	{
		numTypes = _this->mipMachine->vmUsedNativeTypes;
		for( t = 0; t < numTypes; t++ )
		{
			JILTypeListItem* pItem = _this->mipMachine->vmpTypeList + t;
			JCLSetString(pClassName, pItem->pClassName);
			err = p_import_class(_this, pClassName);
			if( err )
				goto exit;
		}
	}

exit:
	DELETE( pClassName );
	return err;
}

//------------------------------------------------------------------------------
// p_new_init_block
//------------------------------------------------------------------------------
// parse an initialization block after a new statement.

static JILError p_new_init_block(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pObject)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLClass* pClass;
	JCLFunc* pCtor = NULL;
	JCLVar* pThis = NULL;
	JCLVar* pSaveThis = NULL;
	JILLong saveClass;
	JILLong saveFunc;
	JILLong i;
	JILBool isCompound;
	JILBool retFlag;

	// save current class + function
	saveClass = _this->miClass;
	saveFunc = _this->miFunc;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// only class types allowed
	ERROR_IF(!IsClassType(_this, pObject->miType), JCL_ERR_Unexpected_Token, NULL, goto exit);
	// special case for interface array returned by factorization
	ERROR_IF(pObject->miType == type_array && IsInterfaceType(_this, pObject->miElemType), JCL_ERR_Unexpected_Token, NULL, goto exit);
	pClass = GetClass(_this, pObject->miType);

	// set compile context to any constructor in the class we can find (doesn't matter which)
	for( i = 0; i < NumFuncs(_this, pObject->miType); i++ )
	{
		JCLFunc* pFunc = GetFunc(_this, pObject->miType, i);
		if( pFunc->miCtor )
		{
			pCtor = pFunc;
			break;
		}
	}
	ERROR_IF(pCtor == NULL, JCL_ERR_Not_A_Constructor, NULL, goto exit);
	SetCompileContextOnly(_this, pObject->miType, pCtor->miFuncIdx);

	// create 'this'
	pSaveThis = SimRegisterGet(_this, 0);
	SimRegisterUnset(_this, 0);
	pThis = MakeThisVar(_this, pObject->miType);
	SimRegisterSet(_this, 0, pThis);

	// save current r0 to stack and load new class to r0
	cg_opcode(_this, op_push_r);
	cg_opcode(_this, 0);
	SimStackReserve(_this, 1);
	err = cg_move_var(_this, pObject, pThis);
	ERROR_IF( err, err, NULL, goto exit2 );

	// compile block
	retFlag = pCtor->miRetFlag;
	pCtor->miRetFlag = JILFalse;
	err = p_block(_this, &isCompound);
	pCtor->miRetFlag = retFlag;
	if( err )
		goto exit2;

exit2:
	// pop r0 from the stack
	cg_opcode(_this, op_pop_r);
	cg_opcode(_this, 0);
	SimStackPop(_this, 1);
	SimRegisterUnset(_this, 0);
	SimRegisterSet(_this, 0, pSaveThis);

exit:
	// free 'this'
	if( pThis )
	{
		DELETE(pThis);
	}
	// restore context
	SetCompileContextOnly(_this, saveClass, saveFunc);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_new
//------------------------------------------------------------------------------
// Parse a new operator.

static JILError p_new(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pToken2;
	JILLong tokenID;
	JILLong typeID;
	JCLClass* pClass;
	JCLVar* pVar;
	JCLVar* pWorkVar;
	JILBool bContinue;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// test compile to see if this is a copy-constructor call
	err = p_new_copy_ctor(_this, pLocals, pLVar, ppVarOut, ppTempVar, &bContinue);
	if( !bContinue )
	{
		if( err == JCL_No_Error )
			goto initblock;
		else
			goto exit;
	}

	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_array )
	{
		err = p_new_array(_this, pLocals, pLVar, ppVarOut, ppTempVar);
	}
	else if( IsBasicType(tokenID) )
	{
		err = p_new_basic_type(_this, pLocals, pLVar, ppVarOut, ppTempVar, tokenID);
	}
	else if( tokenID == tk_identifier )
	{
		// try to find class
		typeID = StringToType(_this, pToken, tokenID);
		ERROR_IF(!typeID, JCL_ERR_Undefined_Identifier, pToken, goto exit);
		pClass = GetClass(_this, typeID);
		ERROR_IF(pClass->miFamily != tf_class && pClass->miFamily != tf_interface && pClass->miFamily != tf_thread, JCL_ERR_Type_Not_Class, pToken, goto exit);
		ERROR_IF(!pClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pToken, goto exit);
		ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_Modifier_Illegal, pToken, goto exit);
		// fill out source var
		pVar->miType = pClass->miType;
		pVar->miElemType = type_var;
		pVar->miElemRef = JILFalse;
		pVar->miInited = JILTrue;
		pVar->miUnique = JILTrue;
		// must alloc into temp var in case destination var is passed into constructor!
		err = MakeTempVar(_this, ppTempVar, pLVar);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
		pWorkVar->miType = pClass->miType;
		pWorkVar->miConst = JILFalse;
		pWorkVar->miRef = JILTrue;
		// generate allocation code
		if( pClass->miFamily == tf_class )
		{
			err = cg_alloc_var(_this, pVar, pWorkVar, pClass->miNative);
			ERROR_IF(err, err, pToken, goto exit);
		}
		else if( pClass->miFamily == tf_interface )
		{
			pWorkVar->miType = type_array;
			pWorkVar->miElemRef = JILTrue;
			pWorkVar->miElemType = pClass->miType;
			err = cg_alloci_var(_this, pVar, pWorkVar);
			ERROR_IF(err, err, pToken, goto exit);
		}
		// must mark temp register as inited
		pWorkVar->miInited = JILTrue;
		// is a constructor specified?
		err = pFile->PeekToken(pFile, pToken2, &tokenID);
		ERROR_IF(err, err, pToken2, goto exit);
		if( tokenID == tk_round_open )
		{
			// call constructor
			err = p_member_call(_this, pLocals, pClass->miType, pToken, pWorkVar, NULL, &outType, kOnlyCtor);
			if( err )
				goto exit;
		}
		else
		{
			ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	else
	{
		ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
	}

initblock:
	// check for block statement following new
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if (tokenID == tk_curly_open)
	{
		err = p_new_init_block(_this, pLocals, *ppVarOut);
		if( err )
			goto exit;
	}

exit:
	DELETE( pToken );
	DELETE( pToken2 );
	DELETE( pVar );
	return err;
}

//------------------------------------------------------------------------------
// p_for
//------------------------------------------------------------------------------
// Parse a for statement.

static JILError p_for(JCLState* _this, Array_JCLVar* pParentLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pTempVar = NULL;
	Array_JILLong* pCode;
	Array_JILLong* pSaveFixup;
	Array_JILLong* pSaveCFixup;
	Array_JCLVar* pLocals;
	JILLong pSaveUnrollSP;
	JILLong tokenID;
	JILLong branchFix;
	JILLong branchBack;
	JILLong thirdStLoc;
	JILLong endBlockLoc;
	JILLong endBlockCode;
	JILLong bracketID = 0;
	SMarker thirdStCode;
	JILBool noTest = JILFalse;
	JILBool isCompound = JILFalse;
	TypeInfo outType;
	SInitState orig;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );
	CreateInitState(&orig, _this);

	// increase block level and allocate a new stack context
	_this->miBlockLevel++;
	pLocals = NEW(Array_JCLVar);

	// exchange break fixup list
	pSaveFixup = _this->mipBreakFixup;
	pSaveCFixup = _this->mipContFixup;
	_this->mipBreakFixup = NEW(Array_JILLong);
	_this->mipContFixup = NEW(Array_JILLong);
	pSaveUnrollSP = _this->miBreakUnrollSP;

	// skip the "for"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// parse the first statement
	err = p_statement(_this, pLocals, &isCompound);
	if( err )
		goto exit;
	// skip semicolon
	if( isCompound )
	{
		ERROR(JCL_ERR_Syntax_Error, NULL, goto exit);
	}
	else
	{
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);
	}
	// save the stack pointer for break stack unroll
	pSaveUnrollSP = _this->miBreakUnrollSP;
	_this->miBreakUnrollSP = _this->miStackPos;
	// save code position for branch back
	branchBack = GetCodeLocator(_this);

	// check if there is a second statement
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	noTest = (tokenID == tk_semicolon);
	if( !noTest )
	{
		// parse the second "statement", which must be an expression
		err = MakeTempVar(_this, &pTempVar, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar->miType = type_int;
		pTempVar->miRef = JILTrue;		// we prefer a reference
		// parse expression
		err = p_expression(_this, pLocals, pTempVar, &outType, 0);
		if( err )
			goto exit;
	}
	// expecting ";"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);
	branchFix = GetCodeLocator(_this);
	if( !noTest )
	{
		cg_opcode(_this, op_tsteq_r);
		cg_opcode(_this, pTempVar->miIndex);
		cg_opcode(_this, 0);
	}
	FreeTempVar(_this, &pTempVar);

	// now it gets a little dirty, we need to skip the third expression to
	// be able to parse the block, then return to the third expression and parse it

	// save current position
	thirdStLoc = pFile->GetLocator(pFile);
	SetMarker(_this, &thirdStCode);
	err = pFile->PeekToken(pFile, pToken, &bracketID);
	ERROR_IF(err, err, pToken, goto exit);
	if( bracketID != tk_round_close )
	{
		// "dummy parse" third expression to skip it
		err = p_expression(_this, pLocals, NULL, &outType, 0);
		if( err )
			goto exit;
		// discard code from third statement
		RestoreMarker(_this, &thirdStCode);
	}

	// expecting ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// save init states of member vars
	SaveInitState(&orig);
	// parse block
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	// and restore them again
	RestoreInitState(&orig);
	// save end position
	endBlockLoc = pFile->GetLocator(pFile);
	endBlockCode = GetCodeLocator(_this);

	// now it's time to parse third statement
	if( bracketID != tk_round_close )
	{
		pFile->SetLocator(pFile, thirdStLoc);
		err = p_expression(_this, pLocals, NULL, &outType, 0);
		if( err )
			goto exit;
		// skip ")"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}

	// add code to jump back
	cg_opcode(_this, op_bra);
	cg_opcode(_this, branchBack - GetCodeLocator(_this) + 1);
	// fix jump to end of loop
	if( !noTest )
	{
		pCode = CurrentOutFunc(_this)->mipCode;
		pCode->Set(pCode, branchFix + 2, GetCodeLocator(_this) - branchFix);
	}
	// fix any break statements in this loop
	BreakBranchFixup(_this, _this->mipBreakFixup, GetCodeLocator(_this));
	BreakBranchFixup(_this, _this->mipContFixup, endBlockCode);

	// goto end position
	pFile->SetLocator(pFile, endBlockLoc);

exit:
	// free all locals in this block
	FreeLocalVars(_this, pLocals);
	_this->miBlockLevel--;
	DELETE( pLocals );

	// restore break fixup lists
	DELETE( _this->mipBreakFixup );
	_this->mipBreakFixup = pSaveFixup;
	DELETE( _this->mipContFixup );
	_this->mipContFixup = pSaveCFixup;
	_this->miBreakUnrollSP = pSaveUnrollSP;

	FreeTempVar(_this, &pTempVar);
	FreeInitState(&orig);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_while
//------------------------------------------------------------------------------
// Parse a while statement.

static JILError p_while(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pTempVar = NULL;
	Array_JILLong* pCode;
	Array_JILLong* pSaveFixup;
	Array_JILLong* pSaveCFixup;
	JILLong pSaveUnrollSP;
	JILLong tokenID;
	JILLong branchFix;
	JILLong branchBack;
	JILBool isCompound;
	TypeInfo outType;
	SInitState orig;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );
	CreateInitState(&orig, _this);

	// exchange break fixup list
	pSaveFixup = _this->mipBreakFixup;
	pSaveCFixup = _this->mipContFixup;
	_this->mipBreakFixup = NEW(Array_JILLong);
	_this->mipContFixup = NEW(Array_JILLong);
	// save the stack pointer for break stack unroll
	pSaveUnrollSP = _this->miBreakUnrollSP;
	_this->miBreakUnrollSP = _this->miStackPos;
	// skip the "while"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// save code position for branch back
	branchBack = GetCodeLocator(_this);
	// parse the expression
	err = MakeTempVar(_this, &pTempVar, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempVar->miType = type_int;
	pTempVar->miRef = JILTrue;		// we prefer a reference
	// parse expression
	err = p_expression(_this, pLocals, pTempVar, &outType, 0);
	if( err )
		goto exit;
	branchFix = GetCodeLocator(_this);
	cg_opcode(_this, op_tsteq_r);
	cg_opcode(_this, pTempVar->miIndex);
	cg_opcode(_this, 0);
	FreeTempVar(_this, &pTempVar);
	// expecting ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// save init states
	SaveInitState(&orig);
	// parse block
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	// restore original state
	RestoreInitState(&orig);
	// add code to jump back
	cg_opcode(_this, op_bra);
	cg_opcode(_this, branchBack - GetCodeLocator(_this) + 1);
	// fix jump to end of loop
	pCode = CurrentOutFunc(_this)->mipCode;
	pCode->Set(pCode, branchFix + 2, GetCodeLocator(_this) - branchFix);
	// fix any break statements in this loop
	BreakBranchFixup(_this, _this->mipBreakFixup, GetCodeLocator(_this));
	BreakBranchFixup(_this, _this->mipContFixup, branchBack);

exit:
	// restore break fixup lists
	DELETE( _this->mipBreakFixup );
	_this->mipBreakFixup = pSaveFixup;
	DELETE( _this->mipContFixup );
	_this->mipContFixup = pSaveCFixup;
	_this->miBreakUnrollSP = pSaveUnrollSP;

	FreeTempVar(_this, &pTempVar);
	FreeInitState(&orig);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_break
//------------------------------------------------------------------------------
// Parse a break statement

static JILError p_break(JCLState* _this, JILBool bContinue)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JILLong tokenID;
	JILLong numStackItems;
	JILLong fixupPos;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// skip the token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// check if we have a context (for, while, switch)
	if( (!bContinue && !_this->mipBreakFixup) || (bContinue && !_this->mipContFixup) )
		ERROR(JCL_ERR_Break_Without_Context, NULL, goto exit);

	// generate code to unroll loop
	numStackItems = _this->miBreakUnrollSP - _this->miStackPos;
	if( numStackItems )
		cg_pop_multi(_this, numStackItems);
	// generate branch
	fixupPos = GetCodeLocator(_this);
	cg_opcode(_this, op_bra);
	cg_opcode(_this, 0);

	// add fixup position to the list
	if( bContinue )
		_this->mipContFixup->Set(_this->mipContFixup, _this->mipContFixup->count, fixupPos);
	else
		_this->mipBreakFixup->Set(_this->mipBreakFixup, _this->mipBreakFixup->count, fixupPos);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_switch
//------------------------------------------------------------------------------
// Parse a switch statement.

static JILError p_switch(JCLState* _this, Array_JCLVar* pParentLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pSwitchVar = NULL;
	JCLVar* pTempVar = NULL;
	JCLVar* pDupVar = NULL;
	JCLVar* pDummyVar;
	Array_JILLong* pCode;
	Array_JILLong* pSaveFixup;
	Array_JILLong* pSaveCFixup;
	Array_JILLong* pCaseFixup;
	Array_JILLong* pBranchFixup;
	Array_JCLVar* pLocals;
	Array_JCLVar* pTagLocals;
	JILLong pSaveUnrollSP;
	JILLong tokenID;
	JILLong savePos;
	JILLong branchFix = 0;
	JILBool isCompound;
	JILBool haveDefault = JILFalse;
	JILBool haveBreak = JILTrue;
	JILLong i;
	JILLong j;
	JILLong casenum = 0;
	TypeInfo outType;
	SInitState orig;
	SInitState prev;
	SMarker marker;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pCaseFixup = NEW(Array_JILLong);
	pBranchFixup = NEW(Array_JILLong);
	CreateInitState(&orig, _this);
	CreateInitState(&prev, _this);

	// create a new stack context
	pLocals = NEW(Array_JCLVar);
	_this->miBlockLevel++;
	pTagLocals = NEW(Array_JCLVar);
	_this->miBlockLevel++;

	// exchange break fixup list
	pSaveFixup = _this->mipBreakFixup;
	pSaveCFixup = _this->mipContFixup;
	_this->mipBreakFixup = NEW(Array_JILLong);
	_this->mipContFixup = NULL;
	pSaveUnrollSP = _this->miBreakUnrollSP;

	// skip the "switch"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// we MUST use a local stack variable to hold the expression result!
	// otherwise optimization will break the code generated by switch
	pDummyVar = NEW(JCLVar);
	pDummyVar->miType = type_int;
	pDummyVar->miConst = JILTrue;
	pDummyVar->miRef = JILTrue;
	pDummyVar->miInited = JILFalse;
	pDummyVar->miElemType = type_var;
	pDummyVar->miElemRef = JILFalse;
	JCLRandomIdentifier(pDummyVar->mipName, 16);
	err = MakeLocalVar(_this, pLocals, kLocalStack, pDummyVar);
	DELETE(pDummyVar);
	ERROR_IF(err, err, NULL, goto exit);
	pSwitchVar = SimStackGet(_this, 0);

	// save the stack pointer for break stack unroll
	pSaveUnrollSP = _this->miBreakUnrollSP;
	_this->miBreakUnrollSP = _this->miStackPos;

	// parse expression trying with int
	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	JCLClrTypeInfo( &outType );
	err = p_expression(_this, pLocals, pSwitchVar, &outType, 0);
	if( err == JCL_ERR_Incompatible_Type || err == JCL_ERR_Conv_Requires_Cast )
	{
		// retry with string
		pSwitchVar->miType = type_string;
		RestoreMarker(_this, &marker);
		pFile->SetLocator(pFile, savePos);
		JCLClrTypeInfo( &outType );
		err = p_expression(_this, pLocals, pSwitchVar, &outType, 0);
	}
	if( err )
		goto exit;
	JCLTypeInfoToVar(&outType, pSwitchVar);
	pSwitchVar->miInited = JILTrue;

	// we only allow switch with int and string
	if( outType.miType != type_int && outType.miType != type_string )
		ERROR(JCL_ERR_Incompatible_Type, NULL, goto exit);

	// expecting ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// expecting "{"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_curly_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// save member variable init states
	SaveInitState(&orig);
	SetInitState(&prev, JILTrue);

	// get next token
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	while( tokenID != tk_curly_close )
	{
		// what follows is either "case" or a statement
		if( tokenID == tk_case )
		{
			ERROR_IF(haveDefault, JCL_ERR_Default_Not_At_End, pToken, goto exit);
			// free locals from previous block
			FreeLocalVars(_this, pTagLocals);
			// insert branch to next block
			if( !haveBreak )
			{
				pBranchFixup->Set(pBranchFixup, pBranchFixup->count, GetCodeLocator(_this));
				cg_opcode(_this, op_bra);
				cg_opcode(_this, 0);
				haveBreak = JILTrue;
			}
			if( branchFix )
			{
				pCode = CurrentOutFunc(_this)->mipCode;
				pCode->Set(pCode, branchFix + 2, GetCodeLocator(_this) - branchFix);
				branchFix = 0;
			}
			if( casenum )
			{
				AndInitState(&prev);		// and current init state with previous
				SaveInitState(&prev);		// store current init states
				RestoreInitState(&orig);	// restore original states
			}
			err = MakeTempVar(_this, &pTempVar, NULL);
			ERROR_IF(err, err, NULL, goto exit);
			pTempVar->miType = pSwitchVar->miType;
			pTempVar->miRef = JILTrue;
			JCLClrTypeInfo( &outType );
			err = p_expression(_this, pTagLocals, pTempVar, &outType, 0);
			if( err )
				goto exit;
			JCLTypeInfoToVar(&outType, pTempVar);
			ERROR_IF(!outType.miConst, JCL_ERR_Case_Requires_Const_Expr, pToken, goto exit);
			// expecting ":"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_colon, JCL_ERR_Unexpected_Token, pToken, goto exit);
			// duplicate temp var to set result type to 'int'
			DuplicateVar(&pDupVar, pTempVar);
			pDupVar->miType = type_int;
			err = cg_compare_var(_this, tk_equ, pTempVar, pSwitchVar, pDupVar);
			ERROR_IF(err, err, NULL, goto exit);
			// insert test and branch
			pCaseFixup->Set(pCaseFixup, pCaseFixup->count, GetCodeLocator(_this));
			branchFix = GetCodeLocator(_this);
			cg_opcode(_this, op_tsteq_r);
			cg_opcode(_this, pTempVar->miIndex);
			cg_opcode(_this, 0);
			FreeTempVar(_this, &pTempVar);
			FreeDuplicate(&pDupVar);
			casenum++;
		}
		else if( tokenID == tk_default )
		{
			// expecting ":"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_colon, JCL_ERR_Unexpected_Token, pToken, goto exit);
			// free locals from previous block
			FreeLocalVars(_this, pTagLocals);
			if( !haveBreak )
			{
				// insert branch to next block
				pBranchFixup->Set(pBranchFixup, pBranchFixup->count, GetCodeLocator(_this));
				cg_opcode(_this, op_bra);
				cg_opcode(_this, 0);
				haveBreak = JILTrue;
			}
			if( branchFix )
			{
				pCode = CurrentOutFunc(_this)->mipCode;
				pCode->Set(pCode, branchFix + 2, GetCodeLocator(_this) - branchFix);
				branchFix = 0;
			}
			if( casenum )
			{
				AndInitState(&prev);		// and current init state with previous
				SaveInitState(&prev);		// store current init states
				RestoreInitState(&orig);	// restore original states
			}
			haveDefault = JILTrue;
			casenum++;
		}
		else
		{
			haveBreak = (tokenID == tk_break || tokenID == tk_return);
			if( pCaseFixup->count )
			{
				// fix jump to here from previous case(s)
				pCode = CurrentOutFunc(_this)->mipCode;
				for( i = 0; i < pCaseFixup->count - 1; i++ )
				{
					j = pCaseFixup->Get(pCaseFixup, i);
					pCode->Set(pCode, j, op_tstne_r);
					pCode->Set(pCode, j + 2, GetCodeLocator(_this) - j);
				}
				pCaseFixup->Trunc(pCaseFixup, 0);
			}
			if( pBranchFixup->count )
			{
				// fix jump to here from previous code blocks
				pCode = CurrentOutFunc(_this)->mipCode;
				for( i = 0; i < pBranchFixup->count; i++ )
				{
					j = pBranchFixup->Get(pBranchFixup, i);
					pCode->Set(pCode, j + 1, GetCodeLocator(_this) - j);
				}
				pBranchFixup->Trunc(pBranchFixup, 0);
			}
			// not case, go back and parse statement
			pFile->SetLocator(pFile, savePos);
			err = p_statement(_this, pTagLocals, &isCompound);
			if( err )
				goto exit;
			if( !isCompound )
			{
				// skip semicolon
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Missing_Semicolon, pToken, goto exit);
			}
		}
		// get next token
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	// free locals from previous block
	FreeLocalVars(_this, pTagLocals);
	// if no default tag was given, init states are always UNDEFINED
	if( haveDefault )
		AndInitState(&prev);
	else
		RestoreInitState(&orig);
	// fix jump to end of switch
	if( pCaseFixup->count )
	{
		pCode = CurrentOutFunc(_this)->mipCode;
		for( i = 0; i < pCaseFixup->count - 1; i++ )
		{
			j = pCaseFixup->Get(pCaseFixup, i);
			pCode->Set(pCode, j, op_tstne_r);
			pCode->Set(pCode, j + 2, GetCodeLocator(_this) - j);
		}
	}
	if( pBranchFixup->count )
	{
		pCode = CurrentOutFunc(_this)->mipCode;
		for( i = 0; i < pBranchFixup->count; i++ )
		{
			j = pBranchFixup->Get(pBranchFixup, i);
			pCode->Set(pCode, j + 1, GetCodeLocator(_this) - j);
		}
		pBranchFixup->Trunc(pBranchFixup, 0);
	}
	if( branchFix )
	{
		pCode = CurrentOutFunc(_this)->mipCode;
		pCode->Set(pCode, branchFix + 2, GetCodeLocator(_this) - branchFix);
	}
	// fix any break statements
	BreakBranchFixup(_this, _this->mipBreakFixup, GetCodeLocator(_this));

exit:
	// restore break fixup list
	DELETE( _this->mipBreakFixup );
	_this->mipBreakFixup = pSaveFixup;
	_this->mipContFixup = pSaveCFixup;
	_this->miBreakUnrollSP = pSaveUnrollSP;

	FreeLocalVars(_this, pTagLocals);
	DELETE( pTagLocals );
	_this->miBlockLevel--;
	FreeLocalVars(_this, pLocals);
	_this->miBlockLevel--;
	DELETE( pLocals );

	FreeTempVar(_this, &pTempVar);
	FreeDuplicate(&pDupVar);
	FreeInitState(&orig);
	FreeInitState(&prev);
	DELETE( pToken );
	DELETE( pCaseFixup );
	DELETE( pBranchFixup );
	return err;
}

//------------------------------------------------------------------------------
// p_typeof
//------------------------------------------------------------------------------
// Parse the typeof operator

static JILError p_typeof(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLClass* pClass;
	JCLVar* pWorkVar;
	JCLString* pToken;
	JILLong tokenID;
	JILLong typeID;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// now we have either a type, or an expression
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	typeID = -1;
	switch( tokenID )
	{
		case tk_null:
			typeID = type_null;
			break;
		case tk_int:
			typeID = type_int;
			break;
		case tk_float:
			typeID = type_float;
			break;
		case tk_string:
		case tk_array:
		case tk_identifier:
			FindClass(_this, pToken, &pClass);
			if( pClass )
			{
				ERROR_IF(pClass->miFamily != tf_class, JCL_ERR_Type_Not_Class, pToken, goto exit);
				ERROR_IF(!pClass->miHasBody, JCL_ERR_Class_Only_Forwarded, pToken, goto exit);
				ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_Modifier_Illegal, pToken, goto exit);
				typeID = pClass->miType;
			}
			break;
		case tk_this:
			// don't error on 'this'
			break;
		case tk_var:
			ERROR(JCL_ERR_Typeof_Var_Illegal, pToken, goto exit);
			break;
		default:
			ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
			break;
	}
	// did we find a type?
	if( typeID != -1 )
	{
		// make a literal constant out of it...
		JCLFormat(pToken, "%d", typeID);
		err = cg_get_literal(_this, type_int, pToken, pLVar, ppVarOut, ppTempVar, JILFalse);
		ERROR_IF(err, err, NULL, goto exit);
		// skip token
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	else
	{
		// have to evaluate expression and use runtime features
		if( !pLVar || !IsTempVar(pLVar) )
		{
			err = MakeTempVar(_this, ppTempVar, NULL);
			ERROR_IF(err, err, NULL, goto exit);
			*ppVarOut = *ppTempVar;
			pWorkVar = *ppTempVar;
		}
		else
		{
			*ppVarOut = pLVar;
			pWorkVar = pLVar;
		}
		pWorkVar->miType = type_var;
		err = p_expression(_this, pLocals, pWorkVar, &outType, 0);
		if( err )
			goto exit;
		// perform type instruction on work var
		cg_opcode(_this, op_type);
		cg_opcode(_this, pWorkVar->miIndex);
		cg_opcode(_this, pWorkVar->miIndex);
		// now work var is a new int...
		pWorkVar->miType = type_int;
		pWorkVar->miUnique = JILTrue;
	}
	// ensure there is a ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_sameref
//------------------------------------------------------------------------------
// Parse the sameref operator

static JILError p_sameref(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pWorkVar;
	JCLVar* pTempVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// get first expression
	if( !pLVar || !IsTempVar(pLVar) )
	{
		err = MakeTempVar(_this, ppTempVar, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	else
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	pWorkVar->miType = type_var;
	pWorkVar->miRef = JILTrue;
	pWorkVar->miConst = JILTrue;
	err = p_expression(_this, pLocals, pWorkVar, &outType, 0);
	if( err )
		goto exit;
	// ensure there is a ","
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_comma, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// get second expression
	err = MakeTempVar(_this, &pTempVar, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempVar->miType = type_var;
	pTempVar->miRef = JILTrue;
	pTempVar->miConst = JILTrue;
	JCLClrTypeInfo( &outType );
	err = p_expression(_this, pLocals, pTempVar, &outType, 0);
	if( err )
		goto exit;

	// perform cmpref instruction on work var
	cg_opcode(_this, op_cmpref_rr);
	cg_opcode(_this, pWorkVar->miIndex);
	cg_opcode(_this, pTempVar->miIndex);
	cg_opcode(_this, pWorkVar->miIndex);
	// now work var is a new int...
	pWorkVar->miType = type_int;
	pWorkVar->miRef = JILFalse;
	pWorkVar->miConst = JILFalse;
	pWorkVar->miUnique = JILTrue;

	// ensure there is a ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

exit:
	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_new_array
//------------------------------------------------------------------------------
// Parse array allocation: new array(x)

static JILError p_new_array(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pWorkVar;
	JILLong tokenID;
	TypeInfo outType;
	JILLong numDim;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );
	numDim = 0;

	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, pLVar);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
		if( !pLVar )
		{
			pWorkVar->miRef = JILFalse;
			pWorkVar->miConst = JILFalse;
		}
	}
	// we rely on being allowed to MODIFY L-VAR
	pWorkVar->miType = type_int;
	pWorkVar->miRef = JILTrue;
	pWorkVar->miInited = JILFalse;
	// test if there are parentheses
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// check for ")"
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_round_close )
	{
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	while( tokenID != tk_round_close )
	{
		// parse expression
		err = p_expression(_this, pLocals, pWorkVar, &outType, 0);
		if( err )
			goto exit;
		// push onto stack
		err = cg_push_var(_this, pWorkVar);
		if( err )
			goto exit;
		SimStackReserve(_this, 1);
		numDim++;
		// we expect to see a "," or ")"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_comma && tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	}
	// generate code
	pWorkVar->miType = type_array;
	pWorkVar->miRef = JILTrue;
	pWorkVar->miElemType = pLVar ? pLVar->miElemType : type_var;
	pWorkVar->miElemRef = pLVar ? pLVar->miElemRef : JILTrue;
	pWorkVar->miUnique = JILTrue;
	pWorkVar->miInited = JILTrue;
	err = cg_alloca_var(_this, pWorkVar->miElemType, numDim, pWorkVar);
	cg_pop_multi(_this, numDim);
	SimStackPop(_this, numDim);
	ERROR_IF(err, err, NULL, goto exit);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_array_init
//------------------------------------------------------------------------------
// Parse array initialization expression: { x, y, z }

static JILError p_array_init(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pExpVar = NULL;
	JCLVar* pArrVar = NULL;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	TypeInfo outType;
	SMarker marker;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// create temp array var
	err = MakeTempArrayVar(_this, ppTempVar, pLVar);
	ERROR_IF(err, err, NULL, goto exit);
	*ppVarOut = *ppTempVar;
	pArrVar = *ppTempVar;
	pArrVar->miConst = pLVar->miConst;
	// generate code to allocate empty array
	pArrVar->miMode = kModeRegister;
	pArrVar->miType = type_array;
	pArrVar->miRef = pLVar->miRef;
	err = cg_alloca_var(_this, pLVar->miElemType, 0, pArrVar);
	ERROR_IF(err, err, NULL, goto exit);
	pArrVar->miMode = kModeArray;
	pArrVar->miType = pArrVar->miElemType;
	pArrVar->miRef = pArrVar->miElemRef;
	// check if we have empty brackets...
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_curly_close )
	{
		// skip the "}"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}
	else
	{
		// clear index variable
		cg_opcode(_this, op_ldz_r);
		cg_opcode(_this, pArrVar->mipArrIdx->miIndex);
		// make a third temp var
		err = MakeTempVar(_this, &pExpVar, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		while( tokenID != tk_curly_close )
		{
			// first try sub-array
			savePos = pFile->GetLocator(pFile);
			SetMarker(_this, &marker);
			pExpVar->miType = pArrVar->miType;	// pArrVar already set to miElemType
			pExpVar->miRef = pArrVar->miRef;	// pArrVar already set to miElemRef
			pExpVar->miElemType = type_var;
			pExpVar->miElemRef = JILFalse;
			pExpVar->miConst = pArrVar->miConst;
			pExpVar->miInited = JILFalse;
			// get expression
			err = p_expression(_this, pLocals, pExpVar, &outType, 0);
			if( err && err != JCL_ERR_Incompatible_Type && err != JCL_ERR_Conv_Requires_Cast )
				goto exit;
			if( err != JCL_No_Error )
			{
				RestoreMarker(_this, &marker);
				pFile->SetLocator(pFile, savePos);
				JCLClrTypeInfo( &outType );
				pExpVar->miType = type_var;
				pExpVar->miElemType = pArrVar->miType;	// pArrVar already set to miElemType
				pExpVar->miElemRef = pArrVar->miRef;	// pArrVar already set to miElemRef
				err = p_expression(_this, pLocals, pExpVar, &outType, 0);
				if( err )
					goto exit;
			}
			JCLTypeInfoToVar(&outType, pExpVar);
			// generate code to add result to array
			err = cg_move_var(_this, pExpVar, pArrVar);
			ERROR_IF(err, err, NULL, goto exit);
			// we expect to see a "," or "}"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_curly_close && tokenID != tk_comma )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
			// are we at end?
			if( tokenID != tk_curly_close )
			{
				// no, increase index
				cg_opcode(_this, op_incl_r);
				cg_opcode(_this, pArrVar->mipArrIdx->miIndex);
			}
		}
		FreeTempVar(_this, &pExpVar);
	}
	// transform into register mode var
	pArrVar->miMode = kModeRegister;
	pArrVar->miType = type_array;
	pArrVar->miRef = pLVar->miRef;
	pArrVar->miUnique = JILTrue;	// newly constructed array is unique
	pArrVar->miInited = JILTrue;	// and initialized
	FreeTempVar(_this, &(pArrVar->mipArrIdx));

exit:
	FreeTempVar(_this, &pExpVar);

	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_new_basic_type
//------------------------------------------------------------------------------
// Parse a new expression where the given type is a basic type.
// For example: string& b = new string("hello");

static JILError p_new_basic_type(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILLong typeToken)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pWorkVar;
	JCLString* pToken;
	JILLong tokenID;
	JILBool hasArg;
	JILLong theType = type_null;
	TypeInfo outType;
	const JILChar* pLiteral;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	hasArg = JILFalse;
	JCLClrTypeInfo( &outType );

	// check if the expression has a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	hasArg = JILTrue;
	// check for empty brackets
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_round_close )
	{
		// skip the ")"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		hasArg = JILFalse;
	}
	// do we need a new temp var?
	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	switch( typeToken )
	{
		case tk_int:
			theType = type_int;
			pLiteral = "0";
			break;
		case tk_float:
			theType = type_float;
			pLiteral = "0.0";
			break;
		case tk_string:
			theType = type_string;
			pLiteral = "";
			break;
		default:
			ERROR(JCL_ERR_Unexpected_Token, NULL, goto exit);
	}
	pWorkVar->miType = theType;
	pWorkVar->miRef = JILFalse;
	pWorkVar->miInited = JILFalse;
	// do we have an expression to compile?
	if( hasArg )
	{
		// parse expression
		err = p_expression(_this, pLocals, pWorkVar, &outType, 0);
		if( err )
			goto exit;
		// we expect a ")"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	}
	else
	{
		// replace this internally by a long, float or string literal
		err = cg_load_literal(_this, theType, pLiteral, pWorkVar, JILFalse, &outType);
		ERROR_IF(err, err, NULL, goto exit);
	}
	// this will work in most cases, but...
	pWorkVar->miUnique = JILFalse;
	// ...if the result type is reference we must explicitly copy
	if( IsDstTakingRef(pLVar) && !IsDstConst(pLVar) )
	{
		ERROR_IF(!IsTypeCopyable(_this, pWorkVar->miType), JCL_ERR_No_Copy_Constructor, NULL, goto exit);
		// create a copy of the value
		cg_opcode(_this, op_copy_rr);
		cg_opcode(_this, pWorkVar->miIndex);
		cg_opcode(_this, pWorkVar->miIndex);
		pWorkVar->miUnique = JILTrue;
		pWorkVar->miConst = JILFalse;
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_new_copy_ctor
//------------------------------------------------------------------------------
// Parse a new expression that results in the invocation of the copy-constructor.
// For example: string b = new string(a);

static JILError p_new_copy_ctor(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILBool* pContinue)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pWorkVar;
	JCLString* pToken;
	JILLong tokenID;
	JILLong savePos;
	JILLong theType;
	TypeInfo outType;
	SMarker marker;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	*pContinue = JILFalse;

	// get type name
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( !IsClassToken(tokenID) )
		goto _continue;
	theType = StringToType(_this, pToken, tokenID);
	if (theType == type_null || theType == type_int || theType == type_float)
		goto _continue;

	// check if the expression has a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_round_open )
		goto _continue;
	// check for ")"
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_round_close )
		goto _continue;
	// do we need a new temp var?
	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	pWorkVar->miType = type_var;
	pWorkVar->miRef = JILTrue;
	pWorkVar->miInited = JILFalse;
	// parse expression
	err = p_expression(_this, pLocals, pWorkVar, &outType, 0);
	if( err )
		goto _continue;
	// we expect a ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_round_close )
		goto _continue;
	if( outType.miType != theType )
		goto _continue;
	JCLTypeInfoToVar(&outType, pWorkVar);
	ERROR_IF(!IsTypeCopyable(_this, pWorkVar->miType), JCL_ERR_No_Copy_Constructor, NULL, goto exit);
	// create a copy of the value
	cg_opcode(_this, op_copy_rr);
	cg_opcode(_this, pWorkVar->miIndex);
	cg_opcode(_this, pWorkVar->miIndex);
	pWorkVar->miConst = JILFalse;
	pWorkVar->miUnique = JILTrue;
	goto exit;

_continue:
	*pContinue = JILTrue;
	RestoreMarker(_this, &marker);
	pFile->SetLocator(pFile, savePos);
	FreeTempVar(_this, ppTempVar);
	*ppVarOut = NULL;

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_do_while
//------------------------------------------------------------------------------
// Parse a do - while statement.

static JILError p_do_while(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pTempVar = NULL;
	Array_JILLong* pSaveFixup;
	Array_JILLong* pSaveCFixup;
	JILLong pSaveUnrollSP;
	JILLong tokenID;
	JILLong branchFix;
	JILLong branchBack;
	JILLong endBlockCode;
	JILBool isCompound;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// exchange break fixup list
	pSaveFixup = _this->mipBreakFixup;
	pSaveCFixup = _this->mipContFixup;
	_this->mipBreakFixup = NEW(Array_JILLong);
	_this->mipContFixup = NEW(Array_JILLong);

	// save the stack pointer for break stack unroll
	pSaveUnrollSP = _this->miBreakUnrollSP;
	_this->miBreakUnrollSP = _this->miStackPos;
	// save code position for branch back
	branchBack = GetCodeLocator(_this);

	// skip the "do"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// parse the block
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	endBlockCode = GetCodeLocator(_this);

	// we expect to see a "while"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_while, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// we expect to see a "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// parse the expression
	err = MakeTempVar(_this, &pTempVar, NULL);
	ERROR_IF(err, err, NULL, goto exit);
	pTempVar->miType = type_int;
	pTempVar->miRef = JILTrue;		// we prefer a reference
	// parse expression
	err = p_expression(_this, pLocals, pTempVar, &outType, 0);
	if( err )
		goto exit;
	branchFix = GetCodeLocator(_this);
	cg_opcode(_this, op_tstne_r);
	cg_opcode(_this, pTempVar->miIndex);
	cg_opcode(_this, branchBack - branchFix);
	FreeTempVar(_this, &pTempVar);

	// expecting ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// expecting ";"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// fix any break statements in this loop
	BreakBranchFixup(_this, _this->mipBreakFixup, GetCodeLocator(_this));
	BreakBranchFixup(_this, _this->mipContFixup, endBlockCode);

exit:
	// restore break fixup list
	DELETE( _this->mipBreakFixup );
	_this->mipBreakFixup = pSaveFixup;
	DELETE( _this->mipContFixup );
	_this->mipContFixup = pSaveCFixup;
	_this->miBreakUnrollSP = pSaveUnrollSP;

	FreeTempVar(_this, &pTempVar);
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_global_decl
//------------------------------------------------------------------------------
// Parse declaration of one or more global variables.

static JILError p_global_decl(JCLState* _this, JCLVar* pVar, JCLString* pPrefix)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	Array_JCLVar* pLocals = NULL;
	JCLVar* pAnyVar;
	JILLong tokenID;
	TypeInfo outType;

	pLocals = NEW(Array_JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	for(;;)
	{
		// mangle var name if we have a prefix
		if( pPrefix )
			JCLInsert(pVar->mipName, pPrefix, 0);
		err = AddGlobalVar(_this, pVar);
		ERROR_IF(err, err, pVar->mipName, goto exit);
		// peek if we have an assignment
		err = pFile->PeekToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_assign )
		{
			pAnyVar = FindAnyVar(_this, pVar->mipName);
			ERROR_IF(!pAnyVar, JCL_ERR_Not_An_LValue, pVar->mipName, goto exit);
			// compile expression and init variable
			err = p_assignment(_this, pLocals, pAnyVar, &outType);
			if( err )
				goto exit;
		}
		else
		{
			// no assignment, default initialize variable
			pAnyVar = FindAnyVar(_this, pVar->mipName);
			ERROR_IF(!pAnyVar, JCL_ERR_Not_An_LValue, pVar->mipName, goto exit);
			err = cg_init_var(_this, pAnyVar);
			ERROR_IF(err && err != JCL_ERR_Ctor_Is_Explicit, err, pAnyVar->mipName, goto exit);
		}
		// check if we have a comma or semicolon
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_semicolon )
			break;
		ERROR_IF(tokenID != tk_comma, JCL_ERR_Missing_Semicolon, NULL, goto exit);
		// get next identifier name
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
		JCLSetString(pVar->mipName, JCLGetString(pToken));
	}

exit:
	DELETE( pToken );
	DELETE( pLocals );
	return err;
}

//------------------------------------------------------------------------------
// p_interface
//------------------------------------------------------------------------------
// parse an interface declaration

static JILError p_interface(JCLState* _this, JILLong modifier)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLClass* pClass;
	JCLString* pToken;
	JCLString* pClassName;
	JILLong tokenID;
	JILLong classToken;
	JILLong classIdx;
	JILLong savePos;
	JILLong strict;

	pClassName = NEW(JCLString);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	strict = (modifier & kModiStrict) ? kStrict : 0;

	// we expect to find a class name
	err = pFile->GetToken(pFile, pClassName, &classToken);
	ERROR_IF(err, err, pClassName, goto exit);
	ERROR_IF(classToken != tk_identifier, err = JCL_ERR_Unexpected_Token, pClassName, goto exit);

	// peek for ';'
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// try to find class
	FindClass(_this, pClassName, &pClass);
	// see if name is already taken
	if( _this->miPass == kPassPrecompile )
	{
		err = IsIdentifierUsed(_this, kGlobalClass, type_global, pClassName);
		if( err && pClass )
		{
			// if interface exists but has a body, this is an error
			if( pClass->miFamily == tf_interface && (tokenID == tk_semicolon || !pClass->miHasBody) )
				err = JCL_No_Error;
			else if( pClass->miFamily == tf_class )
				err = JCL_ERR_Mixing_Class_And_Interface;
			else if( pClass->miFamily != tf_interface )
				err = JCL_ERR_Identifier_Already_Defined;
		}
		ERROR_IF(err, err, pClassName, goto exit);
	}
	if( pClass )
	{
		classIdx = pClass->miType;
		ERROR_IF(pClass->miModifier != modifier, JCL_ERR_Class_Modifier_Conflict, pClassName, goto exit);
	}
	else
	{
		// check if class is a known NTL...
		if( JILGetNativeType(_this->mipMachine, JCLGetString(pClassName)) != NULL )
		{
			ERROR(JCL_ERR_Identifier_Already_Defined, pClassName, goto exit);
		}

		// add a new type
		err = JCLCreateType(_this, JCLGetString(pClassName), _this->miClass, tf_interface, JILFalse, &classIdx);
		ERROR_IF(err, err, pToken, goto exit);
	}

	// make this the "current class"
	SetCompileContext(_this, classIdx, 0);
	pClass = CurrentClass(_this);
	pClass->miModifier = modifier;

	// we expect to find a "{" or ";"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_semicolon )
		goto exit;
	if( tokenID != tk_curly_open )
		ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
	pClass->miHasBody = JILTrue;
	// check for tag
	err = p_tag(_this, CurrentClass(_this)->mipTag);
	if( err )
		goto exit;
	// get next token
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	while( tokenID != tk_curly_close )
	{
		switch( tokenID )
		{
			case tk_method:
				err = p_function( _this, kMethod | strict, JILTrue );
				break;
			case tk_accessor:
				err = p_function( _this, kMethod | kAccessor | strict, JILTrue );
				break;
			default:
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
		if( err )
			goto exit;
		// get next token
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}

exit:
	// go back to "global" class
	SetCompileContext(_this, type_global, 0);
	DELETE( pToken );
	DELETE( pClassName );
	return err;
}

//------------------------------------------------------------------------------
// p_accessor_call
//------------------------------------------------------------------------------
// parse a call to an accessor function

static JILError p_accessor_call(JCLState* _this, Array_JCLVar* pLocals, JCLFunc* pFunc, JCLVar* pObj, JCLVar* pLVar, TypeInfo* pOut)
{
	JILError err = JCL_No_Error;
	JILLong tokenID;
	JILLong classIdx = pFunc->miClassID;
	JCLString* pName = pFunc->mipName;
	JCLString* pToken;
	JCLFile* pFile;
	JCLVar* pTempVar = NULL;
	JCLVar* pTempVar2 = NULL;
	JCLVar* pVar;
	JCLVar* pArg;
	JCLClass* pClass;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	pVar = NEW(JCLVar);
	JCLClrTypeInfo(&outType);
	pClass = GetClass(_this, classIdx);

	// get the next token
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// if next token is assignment, then this is a set accessor operation
	if( tokenID == tk_assign )
	{
		// find setter variant
		if( pFunc->mipArgs->count == 0 )
		{
			FindAccessor(_this, classIdx, pName, pFunc->miFuncIdx + 1, &pFunc);
			if( pFunc == NULL )
				ERROR(JCL_ERR_Member_Protected, pName, goto exit);
		}
		// save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		// skip the assign token
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		pArg = pFunc->mipArgs->Get(pFunc->mipArgs, 0);
		err = MakeTempVar(_this, &pTempVar, pArg);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar->miConst = pTempVar->miConstP = JILFalse; // TODO: If pArg is const, array-constructor expression fails - why???
		// parse the expression
		err = p_expression(_this, pLocals, pTempVar, &outType, 0);
		if( err )
			goto exit;
		JCLTypeInfoToVar(&outType, pTempVar);
		// ensure type safety
		if( !DynConvertible(_this, pTempVar, pArg) )
			ERROR(JCL_ERR_Incompatible_Type, pArg->mipName, goto exit);
		pVar->Copy(pVar, pFunc->mipArgs->Get(pFunc->mipArgs, 0));
		pVar->miUsage = kUsageResult;
		pVar->miInited = JILFalse;
		cg_push_multi(_this, 1);
		SimStackPush(_this, pVar, JILFalse);
		err = cg_move_var(_this, pTempVar, pVar);
		ERROR_IF(err, err, NULL, goto exit);
		FreeTempVar(_this, &pTempVar);
		JCLSetTypeInfo(&outType, type_null, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
		err = cg_accessor_call(_this, pClass, pFunc, pObj, pName);
		ERROR_IF(err, err, NULL, goto exit);
		// restore r0
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
	}
	else if( IsAssignOperator(tokenID) )
	{
		// save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		// call get accessor
		pVar->miType = type_var;
		pVar->miInited = JILFalse;
		pVar->miUsage = kUsageResult;
		pVar->miElemType = type_var;
		pVar->miElemRef = JILFalse;
		if( !FindGetAccessor(_this, classIdx, pName, pLVar ? pLVar : pVar, &pFunc) )
		{
			if( !pLVar || !FindGetAccessor(_this, classIdx, pName, pVar, &pFunc) )
				ERROR(JCL_ERR_Member_Protected, pName, goto exit);
		}
		err = cg_accessor_call(_this, pClass, pFunc, pObj, pName);
		ERROR_IF(err, err, NULL, goto exit);
		pVar->Copy(pVar, pFunc->mipResult);
		err = MakeTempVar(_this, &pTempVar, pVar);
		ERROR_IF(err, err, NULL, goto exit);
		err = cg_move_var(_this, pVar, pTempVar);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar->miUnique = JILTrue;
		// must restore r0 for p_expression()
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
		// get the operand
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		err = MakeTempVar(_this, &pTempVar2, NULL);
		ERROR_IF(err, err, NULL, goto exit);
		pTempVar2->miType = type_var;
		err = p_expression(_this, pLocals, pTempVar2, &outType, 0);
		if( err )
			goto exit;
		JCLTypeInfoToVar(&outType, pTempVar2);
		// execute arithmetic / logic operation
		if( IsArithmeticAssign(tokenID) )
		{
			err = cg_math_var(_this, pTempVar2, pTempVar, tokenID);
			ERROR_IF(err, err, NULL, goto exit);
		}
		else
		{
			err = cg_and_or_xor_var(_this, pTempVar2, pTempVar, tokenID);
			ERROR_IF(err, err, NULL, goto exit);
		}
		FreeTempVar(_this, &pTempVar2);
		// save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		// call set accessor
		if( !FindSetAccessor(_this, classIdx, pName, pTempVar, &pFunc) )
			ERROR(JCL_ERR_Member_Protected, pName, goto exit);
		pVar->Copy(pVar, pFunc->mipArgs->Get(pFunc->mipArgs, 0));
		pVar->miUsage = kUsageResult;
		pVar->miInited = JILFalse;
		cg_push_multi(_this, 1);
		SimStackPush(_this, pVar, JILFalse);
		err = cg_move_var(_this, pTempVar, pVar);
		ERROR_IF(err, err, NULL, goto exit);
		FreeTempVar(_this, &pTempVar);
		err = cg_accessor_call(_this, pClass, pFunc, pObj, pName);
		ERROR_IF(err, err, NULL, goto exit);
		JCLSetTypeInfo(&outType, type_null, JILFalse, JILFalse, JILFalse, type_var, JILFalse);
		// restore r0
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
	}
	else
	{
		// if there is no L-Value, we fake a 'var'
		pVar->miType = type_var;
		pVar->miInited = JILFalse;
		pVar->miUsage = kUsageResult;
		pVar->miElemType = type_var;
		pVar->miElemRef = JILFalse;
		// find the accessor getter function
		if( !FindGetAccessor(_this, classIdx, pName, pLVar ? pLVar : pVar, &pFunc) )
		{
			// if not found with L-Value, try any result type
			if( !pLVar || !FindGetAccessor(_this, classIdx, pName, pVar, &pFunc) )
				ERROR(JCL_ERR_Member_Protected, pName, goto exit);
		}
		JCLTypeInfoFromVar(&outType, pFunc->mipResult);
		// save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		err = cg_accessor_call(_this, pClass, pFunc, pObj, pName);
		ERROR_IF(err, err, NULL, goto exit);
		// restore r0
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
	}
	JCLTypeInfoCopy(pOut, &outType);

exit:
	FreeTempVar(_this, &pTempVar);
	FreeTempVar(_this, &pTempVar2);
	DELETE( pVar );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_skip_braces
//------------------------------------------------------------------------------
// Skip two matching brace tokens. Can be used to skip code enclosed in round,
// square or curly braces.

static JILError p_skip_braces(JCLState* _this, JILLong token1, JILLong token2)
{
	JILError err = JCL_No_Error;
	JILLong tokenID;
	JCLString* pToken;
	JCLFile* pFile;
	JILLong braceLevel;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	braceLevel = 0;
	do
	{
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == token1 )
			braceLevel++;
		else if( tokenID == token2 )
			braceLevel--;
	}
	while( braceLevel > 0 );

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_skip_if
//------------------------------------------------------------------------------

static JILError p_skip_if(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILLong tokenID;
	JCLString* pToken;
	JCLFile* pFile;
	JILLong savePos;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// skip expression in round braces
	err = p_skip_braces(_this, tk_round_open, tk_round_close);
	if( err )
		goto exit;

	// skip 'true' statement
	err = p_skip_statement(_this);
	if( err )
		goto exit;

	// check for "else"
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_else )
	{
		// check for 'if'
		savePos = pFile->GetLocator(pFile);
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		if( tokenID == tk_if )
		{
			err = p_skip_if(_this);		// recurse
			if( err )
				goto exit;
		}
		else
		{
			pFile->SetLocator(pFile, savePos);
			err = p_skip_statement(_this);		// skip 'false' statement
			if( err )
				goto exit;
		}
	}
	else
		pFile->SetLocator(pFile, savePos);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_skip_statement
//------------------------------------------------------------------------------
// Advance in the text stream until the end of a complete statement. This
// includes block-statements as well as statements that consist of
// sub-statements (like for example 'if').

static JILError p_skip_statement(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILLong tokenID;
	JCLString* pToken;
	JCLFile* pFile;
	JILLong savePos;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	switch( tokenID )
	{
		case tk_semicolon:						// empty statement
			// done
			break;
		case tk_curly_open:						// block statement
			pFile->SetLocator(pFile, savePos);
			err = p_skip_block(_this);
			if( err )
				goto exit;
			break;
		case tk_if:
			err = p_skip_if(_this);
			if( err )
				goto exit;
			break;
		case tk_switch:
		case tk_for:
		case tk_while:
			// skip expression in round braces
			err = p_skip_braces(_this, tk_round_open, tk_round_close);
			if( err )
				goto exit;
			err = p_skip_statement(_this);	// recurse
			if( err )
				goto exit;
			break;
		case tk_do:
			err = p_skip_statement(_this);
			if( err )
				goto exit;
			// skip 'while'
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			// skip expression in round braces
			err = p_skip_braces(_this, tk_round_open, tk_round_close);
			if( err )
				goto exit;
			// skip ';'
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			break;
		default:
			// advance until we reach a semicolon in this block-level
			do
			{
				savePos = pFile->GetLocator(pFile);
				err = pFile->GetToken(pFile, pToken, &tokenID);
				ERROR_IF(err, err, pToken, goto exit);
				if( tokenID == tk_curly_open )
				{
					pFile->SetLocator(pFile, savePos);
					err = p_skip_block(_this);
					ERROR_IF(err, err, NULL, goto exit);
				}
			}
			while( tokenID != tk_semicolon );
			// check for TAGS
			err = pFile->PeekToken(pFile, pToken, &tokenID);
			if( err == JCL_ERR_End_Of_File )
				err = 0;
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID == tk_square_open )
			{
				// skip everything inside
				err = p_skip_braces(_this, tk_square_open, tk_square_close);
				if( err )
					goto exit;
			}
			break;
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_skip_block
//------------------------------------------------------------------------------
// Advance in the text steram until an entire code block has been skipped.
// The first token this function expects to read is an open curly brace.
// The function will then advance until the *matching* closing curly brace has
// been read.

static JILError p_skip_block(JCLState* _this)
{
	return p_skip_braces(_this, tk_curly_open, tk_curly_close);
}

//------------------------------------------------------------------------------
// p_cast_operator
//------------------------------------------------------------------------------
// Parse a cast operator in the form: (typename) expr
// The function assumes that the opening parenthese has already been read from
// the input stream.

static JILError p_cast_operator(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, const TypeInfo* pDestType)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pWorkVar;
	TypeInfo outType;

	pFile = _this->mipFile;
	JCLClrTypeInfo(&outType);

	// if we have no L-Value, fail
	ERROR_IF(!pLVar, JCL_ERR_Expression_Without_LValue, NULL, goto exit);

	if( pDestType->miType == type_var && pLVar->miType != type_var )
		EmitWarning(_this, NULL, JCL_WARN_Cast_To_Var);

	// do we need a temp var?
	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, pLVar);
		ERROR_IF(err, err, NULL, goto exit);
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	pWorkVar->miType = pDestType->miType;
	pWorkVar->miElemType = pDestType->miElemType;
	pWorkVar->miConst = pLVar->miConst;
	pWorkVar->miRef = pLVar->miRef;
	pWorkVar->miElemRef = pLVar->miElemRef;
	pWorkVar->miTypeCast = JILTrue;
	// recurse
	err = p_expr_primary(_this, pLocals, pWorkVar, &outType, 0);
	if( err )
		goto exit;

exit:
	return err;
}

//------------------------------------------------------------------------------
// p_option
//------------------------------------------------------------------------------
// Parse an option statement.

static JILError p_option(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pToken;
	JCLString* pStr = NULL;
	JCLOption* pOptions;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);

	// get next token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// expect a string literal
	ERROR_IF(tokenID != tk_lit_string, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// slice option string into single options and parse them
	pOptions = GetOptions(_this);
	pStr = NEW(JCLString);
	while( !JCLAtEnd(pToken) )
	{
		// copy up to separator into pStr
		JCLSpanExcluding(pToken, ",;", pStr);
		// trim any spaces
		JCLTrim(pStr);
		// something left?
		if( JCLGetLength(pStr) )
		{
			// have option object parse it
			err = pOptions->ParseOption(pOptions, pStr, JILHandleRuntimeOptions, _this->mipMachine);
			// handle warnings and errors
			if( err == JCL_WARN_Unknown_Option )
			{
				JCLSpanExcluding(pStr, "=", pStr);
				EmitWarning(_this, pStr, err);
				JCLSetLocator(pStr, 0);
			}
			else if( err == JCL_WARN_Invalid_Option_Value )
			{
				EmitWarning(_this, pStr, err);
			}
			else if( err )
			{
				ERROR(err, pStr, goto exit);
			}
		}
		// skip the separator(s)
		JCLSpanIncluding(pToken, ",;", pStr);
	}

	// expecting to see a ";"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);

exit:
	DELETE( pToken );
	DELETE( pStr );
	return err;
}

//------------------------------------------------------------------------------
// p_using
//------------------------------------------------------------------------------
// Parse an using statement.

static JILError p_using(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JILLong i;
	JCLString* pToken;
	JCLClass* pClass;
	Array_JILLong* pUsing = GetOptions(_this)->mipUsing;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);

	do
	{
		// get next token
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		// expect an identifier
		ERROR_IF(!IsClassToken(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);
		// find the given class
		FindClass(_this, pToken, &pClass);
		ERROR_IF(pClass == NULL, JCL_ERR_Undefined_Identifier, pToken, goto exit);
		ERROR_IF(pClass->miFamily != tf_class, JCL_ERR_Type_Not_Class, pToken, goto exit);
		// if the given class is not already there, add it to the list
		for( i = 0; i < pUsing->count; i++ )
		{
			if( pUsing->array[i] == pClass->miType )
				break;
		}
		if( i == pUsing->count )
			pUsing->Set(pUsing, i, pClass->miType);
		// expecting to see "," or ";"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_semicolon && tokenID != tk_comma, JCL_ERR_Unexpected_Token, pToken, goto exit);
	}
	while( tokenID == tk_comma );

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_delegate
//------------------------------------------------------------------------------
// Parse a delegate statement.

static JILError p_delegate(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pToken;
	JCLString* pName;
	JCLVar* pResVar;
	JCLVar* pVar;
	JCLClass* pClass;
	Array_JCLVar* pArgs;
	int savePos;
	int argNum;
	JILLong typeID;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);
	pName = NEW(JCLString);
	pResVar = NEW(JCLVar);
	pArgs = NEW(Array_JCLVar);

	// skip in compile mode (only do this in precompile)
	if( _this->miPass == kPassCompile )
	{
		err = p_skip_statement(_this);
		goto exit;
	}

	// result type?
	savePos = pFile->GetLocator(pFile);
	err = IsFullTypeDecl(_this, pToken, pResVar, JILTrue);
	if( err == JCL_ERR_No_Type_Declaration )
	{
		// not a type decl, return to saved pos
		pFile->SetLocator(pFile, savePos);
	}
	else if( err )
	{
		ERROR(err, pToken, goto exit);
	}
	else
	{
		// result always r1
		pResVar->miMode = kModeRegister;
		pResVar->miUsage = kUsageResult;
		pResVar->miIndex = 1;
		pResVar->miInited = JILTrue;
	}

	// expecting an identifier
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
	// mangle delegate name
	if( IsGlobalScope(_this, _this->miClass) )
	{
		JCLSetString(pName, JCLGetString(pToken));
	}
	else
	{
		pClass = CurrentClass(_this);
		JCLSetString(pName, JCLGetString(pClass->mipName));
		JCLAppend(pName, "::");
		JCLAppend(pName, JCLGetString(pToken));
	}

	// expecting parenthese
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// parse argument list
	argNum = 0;
	savePos = pFile->GetLocator(pFile);
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID != tk_round_close )
	{
		// go back to saved pos and get arguments
		pFile->SetLocator(pFile, savePos);
		while( tokenID != tk_round_close )
		{
			pVar = pArgs->New(pArgs);
			err = IsFullTypeDecl(_this, pToken, pVar, JILTrue);
			if( err == JCL_ERR_No_Type_Declaration )
				err = JCL_ERR_Unexpected_Token;
			ERROR_IF(err, err, pToken, goto exit);
			// peek if there is a name
			err = pFile->PeekToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID == tk_identifier )
			{
				// get identifier name
				err = pFile->GetToken(pFile, pVar->mipName, &tokenID);
				ERROR_IF(err, err, pVar->mipName, goto exit);
			}
			// initialize
			pVar->miMode = kModeStack;
			pVar->miIndex = argNum++;
			pVar->miInited = JILTrue;
			// expecting now "," or ")"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_comma && tokenID != tk_round_close )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	// expecting semicolon
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// done with parsing, now create the type
	err = CreateDelegate(_this, pResVar, pArgs, &typeID);
	ERROR_IF(err, err, pName, goto exit);

	// handle tags
	// TODO: We only have 1 tag for all aliases of a delegate type :( AddAlias() should accept an additional tag parameter.
	pClass = GetClass(_this, typeID);
	err = p_tag(_this, pClass->mipTag);
	if( err )
		goto exit;

	// add an alias to the type
	err = AddAlias(_this, pName, typeID);
	ERROR_IF(err, err, pName, goto exit);

exit:
	DELETE( pToken );
	DELETE( pName );
	DELETE( pResVar );
	DELETE( pArgs );
	return err;
}

//------------------------------------------------------------------------------
// p_alias
//------------------------------------------------------------------------------
// Parse an alias statement.

static JILError p_alias(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pToken2;
	JCLString* pName;
	JCLClass* pClass;
	JILLong type;
	JILLong tokenID;
	JILLong tokenID2;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);
	pToken2 = NEW(JCLString);
	pName = NEW(JCLString);

	// only do this during precompile
	if( _this->miPass == kPassCompile )
	{
		err = p_skip_statement(_this);
		goto exit;
	}

	// get next token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(!IsClassToken(tokenID) && !IsBasicType(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);
	// peek for scope operator
	err = pFile->PeekToken(pFile, pToken2, &tokenID2);
	ERROR_IF(err, err, pToken2, goto exit);
	if( tokenID2 == tk_scope )
	{
		// skip "::"
		err = pFile->GetToken(pFile, pToken2, &tokenID2);
		ERROR_IF(err, err, pToken2, goto exit);
		// expecting identifier name
		err = pFile->GetToken(pFile, pToken2, &tokenID2);
		ERROR_IF(err, err, pToken2, goto exit);
		ERROR_IF(!IsClassToken(tokenID2), JCL_ERR_Unexpected_Token, pToken2, goto exit);
		JCLAppend(pToken, "::");
		JCLAppend(pToken, JCLGetString(pToken2));
		tokenID = tk_identifier;
	}

	// check for existing type name
	type = StringToType(_this, pToken, tokenID);
	ERROR_IF(type == type_null, JCL_ERR_Undefined_Identifier, pToken, goto exit);
	// get next token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	// expect an identifer
	ERROR_IF(!IsClassToken(tokenID), JCL_ERR_Unexpected_Token, pToken, goto exit);

	// mangle alias name
	if( IsGlobalScope(_this, _this->miClass) )
	{
		JCLSetString(pName, JCLGetString(pToken));
	}
	else
	{
		pClass = CurrentClass(_this);
		JCLSetString(pName, JCLGetString(pClass->mipName));
		JCLAppend(pName, "::");
		JCLAppend(pName, JCLGetString(pToken));
	}

	// add the alias
	err = AddAlias(_this, pName, type);
	ERROR_IF(err, err, pName, goto exit);

	// expecting semicolon
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_semicolon, JCL_ERR_Unexpected_Token, pToken, goto exit);

exit:
	DELETE( pToken );
	DELETE( pToken2 );
	DELETE( pName );
	return err;
}

//------------------------------------------------------------------------------
// p_yield
//------------------------------------------------------------------------------
// Parse the yield statement.

static JILError p_yield(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLVar* pRetVar;
	JCLString* pToken;
	JILLong tokenID;
	TypeInfo outType;

	pRetVar = NEW(JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// yield only possible in cofunctions
	if( !CurrentFunc(_this)->miCofunc )
		ERROR(JCL_ERR_Yield_Outside_Cofunction, NULL, goto exit);

	// skip "yield" token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// see what's next
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( CurrentFunc(_this)->mipResult->miMode == kModeUnused )
	{
		if( tokenID == tk_semicolon )
		{
			cg_opcode(_this, op_yield);
		}
		else
		{
			ERROR(JCL_ERR_Cannot_Return_Value, NULL, goto exit);
		}
	}
	else
	{
		if( tokenID == tk_semicolon )
		{
			ERROR(JCL_ERR_Must_Return_Value, NULL, goto exit);
		}
		else
		{
			// get expression
			pRetVar->Copy(pRetVar, CurrentFunc(_this)->mipResult);
			pRetVar->miInited = JILFalse;
			err = p_expression(_this, pLocals, pRetVar, &outType, 0);
			if( err )
				goto exit;
			cg_opcode(_this, op_yield);
		}
	}

exit:
	DELETE( pToken );
	DELETE( pRetVar );
	return err;
}

//------------------------------------------------------------------------------
// p_variable_call
//------------------------------------------------------------------------------
// Parse a call to a first-class value (a function or thread context in a
// variable).

static JILError p_variable_call(JCLState* _this, Array_JCLVar* pLocals, const JCLString* pName, JCLVar* pObj, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLVar* pAnyVar;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	// try to find the variable	
	if( pObj )
	{
		pAnyVar = FindMemberVar(_this, pObj->miType, pName);	// call a member from another object
	}
	else
	{
		pAnyVar = FindAnyVar(_this, pName);	// look-up variable depending on context
	}
	if( !pAnyVar || pAnyVar->miHidden )
	{
		err = JCL_ERR_Undefined_Identifier;
		goto exit;
	}
	// parse call expression depending on type family
	switch( TypeFamily(_this, pAnyVar->miType) )
	{
		case tf_thread:
		{
			err = p_cofunction_resume(_this, pAnyVar, pObj, pLVar, &outType);
			if( err )
				goto exit;
			break;
		}
		case tf_delegate:
		{
			err = p_delegate_call(_this, pLocals, pAnyVar, pObj, pLVar, &outType, 0);
			if( err )
				goto exit;
			break;
		}
		default:
		{
			// fix: if the variable is wrong type, it can still be 'hidden' by an accessor with the same name
			// by returning this error code, the caller will continue to look for a function or method call with this name
			err = JCL_ERR_Undefined_Identifier;
			goto exit;
		}
	}
	JCLTypeInfoCopy(pOut, &outType);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_cofunction_resume
//------------------------------------------------------------------------------
// Parse a call to a confunction thread variable.

static JILError p_cofunction_resume(JCLState* _this, JCLVar* pThreadVar, JCLVar* pObj, JCLVar* pLVar, TypeInfo* pOut)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JILLong tokenID;
	JCLVar* pDupVar = NULL;
	JCLFunc* pFunc;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// expecting "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// read closing ")" from input stream
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// call a member from another object?
	if( pObj )
	{
		if( pObj->miMode != kModeRegister )
			FATALERROREXIT("p_cofunction_resume", "'pObj' was assumed to be 'kModeRegister' but is not!");
		DuplicateVar( &pDupVar, pThreadVar );
		pDupVar->miMode = kModeMember;
		pDupVar->miIndex = pObj->miIndex;
		err = cg_resume(_this, pDupVar);
		ERROR_IF(err, err, NULL, goto exit);
	}
	else	// this depends on the context we are in...
	{
		err = cg_resume(_this, pThreadVar);
		ERROR_IF(err, err, NULL, goto exit);
	}

	// get result type from cofunction
	pFunc = GetFunc(_this, pThreadVar->miType, 0);
	if( !pFunc )
		FATALERROREXIT("p_cofunction_resume", "The cofunction class does not have any functions!");
	ERROR_IF(pFunc->mipResult->miMode == kModeUnused && pLVar != NULL, JCL_ERR_Cannot_Return_Value, NULL, goto exit);
	JCLTypeInfoFromVar(pOut, pFunc->mipResult);

exit:
	FreeDuplicate( &pDupVar );
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_delegate_call
//------------------------------------------------------------------------------
// Parse a call to a delegate.

static JILError p_delegate_call(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pDelegateVar, JCLVar* pObj, JCLVar* pLVar, TypeInfo* pOut, JILLong flags)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	TypeInfo outType;
	JCLVar* pDupVar = NULL;
	JCLFuncType* pFunc;
	Array_JCLVar* pArgs;
	JCLVar* pVar;
	JILLong stModify;
	JILLong tokenID;
	JILLong savePos;
	JILLong saveStack = 0;
	JILLong i;
	JILLong j;

	pArgs = NEW(Array_JCLVar);
	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	JCLClrTypeInfo( &outType );

	if( pObj )
	{
		if( pObj->miMode != kModeRegister )
			FATALERROREXIT("p_delegate_call", "'pObj' was assumed to be 'kModeRegister' but is not!");
		// we can't just use the member variable as it is stored since the actual register number is variable, thus
		// we need to duplicate it and update the register number now
		DuplicateVar( &pDupVar, pDelegateVar );
		pDupVar->miMode = kModeMember;
		pDupVar->miIndex = pObj->miIndex;
		pDelegateVar = pDupVar;
	}

	// expect "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	savePos = pFile->GetLocator(pFile);

	// save R0
	saveStack = _this->miStackPos;
	cg_opcode(_this, op_push_r);
	cg_opcode(_this, 0);
	SimStackReserve(_this, 1);

	// compile argument list
	pFunc = GetClass(_this, pDelegateVar->miType)->mipFuncType;
	stModify = pFunc->mipArgs->count;
	cg_push_multi(_this, stModify);
	// copy argument vars, so we can modify them but get no problems in recursive calls
	pArgs->Copy(pArgs, pFunc->mipArgs);
	// The arguments must be pushed onto the SimStack, because their stack indexes must be
	// corrected when more data is pushed on the stack while the function executes!
	for( i = stModify - 1; i >= 0; i-- )
	{
		pVar = pArgs->Get(pArgs, i);
		pVar->miUsage = kUsageResult;
		pVar->miInited = JILFalse;
		SimStackPush(_this, pVar, JILTrue);
	}
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	j = 0;
	if( tokenID != tk_round_close )
	{
		pFile->SetLocator(pFile, savePos);
		while( tokenID != tk_round_close )
		{
			JCLClrTypeInfo( &outType );
			pVar = pArgs->Get(pArgs, j++);
			err = p_expression(_this, pLocals, pVar, &outType, 0);
			if( err )
				goto exit;
			// expecting now "," or ")"
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_comma && tokenID != tk_round_close )
				ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
		}
	}
	// check for correct number of arguments
	ERROR_IF(j != stModify, JCL_ERR_Undefined_Function_Call, pDelegateVar->mipName, goto exit);
	JCLTypeInfoFromVar(pOut, pFunc->mipResult);

	// generate code for delegate call
	err = cg_call_delegate(_this, pDelegateVar);
	if( err )
		goto exit;

	// clean up stack
	SimStackPop(_this, stModify);
	cg_pop_multi(_this, stModify);

	// restore R0
	cg_opcode(_this, op_pop_r);
	cg_opcode(_this, 0);
	SimStackPop(_this, 1);
	saveStack = 0;

exit:
	if( saveStack )
		SimStackUnroll(_this, saveStack);
	FreeDuplicate( &pDupVar );
	DELETE( pToken );
	DELETE( pArgs );
	return err;
}

//------------------------------------------------------------------------------
// p_strict
//------------------------------------------------------------------------------
// Parse the strict keyword.

static JILError p_strict(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JILLong tokenID;
	JCLString* pToken;

	pFile = _this->mipFile;
	pToken = NEW(JCLString);

	// get next token
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	switch( tokenID )
	{
		case tk_class:
			ERROR_IF(!IsGlobalScope(_this, _this->miClass), JCL_ERR_Unexpected_Token, pToken, goto exit);
			err = p_class( _this, kModiStrict );
			break;
		case tk_interface:
			ERROR_IF(!IsGlobalScope(_this, _this->miClass), JCL_ERR_Unexpected_Token, pToken, goto exit);
			err = p_interface( _this, kModiStrict );
			break;
		case tk_function:
			err = p_function( _this, kFunction | kStrict, JILFalse );
			break;
		case tk_cofunction:
			err = p_function( _this, kFunction | kCofunction | kStrict, JILFalse );
			break;
		case tk_method:
			err = p_function( _this, kMethod | kStrict, JILFalse );
			break;
		case tk_accessor:
			err = p_function( _this, kMethod | kAccessor | kStrict, JILFalse );
			break;
		case tk_explicit:
			err = p_function( _this, kMethod | kExplicit | kStrict, JILFalse );
			break;
		default:
			ERROR(JCL_ERR_Unexpected_Token, pToken, goto exit);
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_function_literal
//------------------------------------------------------------------------------
// Parse an anonymous function literal

static JILError p_function_literal(JCLState* _this, Array_JCLVar* pLocals, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILLong flags, JILLong fnKind)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLClass* pClass;
	Array_JCLVar* pArgs;
	JCLVar* pVar;
	JCLVar* pObj = NULL;
	JILLong tokenID;
	JILLong locatorPos;
	JILLong i;
	JILBool hasArgs = JILFalse;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;
	*ppVarOut = pLVar;

	// are we inside operator 'new' initializer block?
	if( _this->miOutputClass != _this->miClass || _this->miOutputFunc != _this->miFunc )
	{
		ERROR(JCL_ERR_Anon_Func_In_Init_Block, NULL, goto exit);
	}

	// method delegates can only be used from within methods
	if( (fnKind & kMethod) && !CurrentFunc(_this)->miMethod )
	{
		ERROR(JCL_ERR_Calling_Method_From_Static, NULL, goto exit);
	}

	// memorize locator for later compilation
	locatorPos = pFile->GetLocator(pFile);

	// check for optional list of argument names and skip
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_round_open )
	{
		hasArgs = JILTrue;
		err = p_skip_braces(_this, tk_round_open, tk_round_close);
		if( err )
			goto exit;
	}

	// expect "{" and skip
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_curly_open, JCL_ERR_Unexpected_Token, pToken, goto exit);
	err = p_skip_block(_this);
	if( err )
		goto exit;

	// if no argument list: ensure the delegate defines names for all arguments
	if( !hasArgs )
	{
		pClass = GetClass(_this, pLVar->miType);
		pArgs = pClass->mipFuncType->mipArgs;
		for( i = 0; i < pArgs->count; i++ )
		{
			pVar = pArgs->Get(pArgs, i);
			if( JCLGetLength(pVar->mipName) == 0 )
				ERROR(JCL_ERR_Unnamed_Delegate_Argument, pClass->mipName, goto exit);
		}
	}

	// in probe mode, fake a generic delegate result
	if( pLVar->miType == type_var && (flags & kExpressionProbeMode) )
	{
		err = MakeTempVar(_this, ppTempVar, pLVar);
		if( err )
			goto exit;
		*ppVarOut = *ppTempVar;
		(*ppTempVar)->miType = type_delegate;
		goto exit;
	}

	// ensure the type left from the assignment is a delegate
	if( TypeFamily(_this, pLVar->miType) != tf_delegate )
	{
		JCLSetString(pToken, "Anonymous delegate");
		ERROR(JCL_ERR_Incompatible_Type, pToken, goto exit);
	}

	// for method delegates, get 'this'
	if( fnKind & kMethod )
	{
		JCLSetString(pToken, "this");
		pObj = FindLocalVar(_this, pToken);
		if( pObj == NULL )
			FATALERROREXIT("p_function_literal", "Local variable 'this' not found.");
	}

	// create function literal
	err = cg_load_func_literal(_this, locatorPos, pLVar, ppVarOut, ppTempVar, pObj);
	ERROR_IF(err, err, NULL, goto exit);

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_selftest
//------------------------------------------------------------------------------
// Parse compiler self-test statement

static JILError p_selftest(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JILError skipErr = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* str;
	JILLong tokenID;
	JILLong expectedResult;
	const JILChar* passFail;
	JILBool isCompound;
	JILLong column;
	JILLong line;
	JILLong savePos;
	SMarker marker;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	if( pLocals )
	{
		// skip "__selftest" token
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
	}

	// get expected result code (integer literal)
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_lit_int, JCL_ERR_Unexpected_Token, pToken, goto exit);
	expectedResult = atol(JCLGetString(pToken));

	// expect {
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_curly_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// remember current position and parse block statement
	savePos = pFile->GetLocator(pFile);
	SetMarker(_this, &marker);
	GetCurrentPosition(_this->mipFile, &column, &line);

	// if we have no locals, then we are at root level
	if( pLocals == NULL )
	{
		err = p_root(_this);
	}
	else
	{
		err = p_statement(_this, pLocals, &isCompound);
		CurrentFunc(_this)->miRetFlag = JILFalse;
		CurrentFunc(_this)->miYieldFlag = JILFalse;
	}

	// if the block statement did not compile, go back and skip the whole block
	if( err )
	{
		RestoreMarker(_this, &marker);
		pFile->SetLocator(pFile, savePos);
		skipErr = p_skip_block(_this);
	}
	// now output PASS or FAIL message
	if( err == expectedResult )
	{
		passFail = "PASS";
	}
	else
	{
		passFail = "FAIL";
	}
	str = _this->mipErrors->New(_this->mipErrors);
	JCLFormat(pToken, "COMPILER SELF TEST %d(%d): %s in %s (%d,%d)\n", err, expectedResult, passFail, JCLGetString(pFile->mipName), line, column);
	str->Copy(str, pToken);
	err = skipErr;

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_tag
//------------------------------------------------------------------------------
// Parse compiler tag. We don't fail if there isn't a tag at this position.

static JILError p_tag(JCLState* _this, JCLString* pOutTag)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JILLong tokenID;

	pToken = NEW(JCLString);
	pFile = _this->mipFile;

	// test for "["
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	if( err == JCL_ERR_End_Of_File )
	{
		err = JCL_No_Error;
		goto exit;
	}
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_square_open )
	{
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);

		// for now, we only allow string literals in tags
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_lit_string, JCL_ERR_Unexpected_Token, pToken, goto exit);
		// Set output string
		JCLSetString(pOutTag, JCLGetString(pToken));

		// get "]"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_square_close, JCL_ERR_Unexpected_Token, pToken, goto exit);
	}

exit:
	DELETE( pToken );
	return err;
}

//------------------------------------------------------------------------------
// p_clause
//------------------------------------------------------------------------------
// Parse a clause statement

static JILError p_clause(JCLState* _this, Array_JCLVar* pLocals, JCLClause* pClause)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pLabel;
	JCLVar* pVar;
	JCLVar* pParamVar;
	JCLClause* pSaveClause;
	Array_JILLong* pCode;
	JCLClauseGoto* pFail;
	JILLong fixBranchAddr;
	JILLong newClauseAddr;
	JILLong tokenID;
	JILLong savePos;
	JILBool isCompound;
	JILBool isFirst;
	SMarker marker;
	SInitState orig;

	CreateInitState(&orig, _this);
	pToken = NEW(JCLString);
	pLabel = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;
	pSaveClause = _this->mipClause;

	if(_this->miPass == kPassPrecompile)
		FATALERROREXIT("p_clause", "_this->miPass == kPassPrecompile");

	// get "clause"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// first clause?
	if( pClause == NULL )
	{
		// increase block level and allocate a new stack context
		_this->miBlockLevel++;
		pLocals = NEW(Array_JCLVar);

		// test for "("
		isFirst = JILTrue;
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

		// variable declaration
		err = IsFullTypeDecl(_this, pToken, pVar, JILFalse);
		ERROR_IF(err, err, pToken, goto exit);
		err = p_local_decl(_this, pLocals, pVar); // TODO: tell function to only allow 1 declaration
		ERROR_IF(err, err, pToken, goto exit);
		pParamVar = FindLocalVar(_this, pVar->mipName);
		if( pParamVar == NULL )
			FATALERROREXIT("p_clause", "Created local variable not found!");

		// test for ")"
		err = pFile->GetToken(pFile, pToken, &tokenID);
		ERROR_IF(err, err, pToken, goto exit);
		ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

		// create clause data
		pClause = NEW(JCLClause);
		pClause->miStackPos = _this->miStackPos;
		pClause->miParameter = pParamVar;
		pClause->mipParent = pSaveClause;
		_this->mipClause = pClause;

		// pre-read all labels
		savePos = pFile->GetLocator(pFile);
		for(;;)
		{
			err = p_skip_block(_this);
			ERROR_IF(err, err, NULL, goto exit);
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			if( tokenID != tk_clause )
				break;
			err = pFile->GetToken(pFile, pToken, &tokenID);
			ERROR_IF(err, err, pToken, goto exit);
			ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pToken, goto exit);
			ERROR_IF(!JCLClause_AddBlock(pClause, pToken), JCL_ERR_Identifier_Already_Defined, pToken, goto exit);
		}
		pFile->SetLocator(pFile, savePos);
	}
	else
	{
		// test for label
		isFirst = JILFalse;
		err = pFile->GetToken(pFile, pLabel, &tokenID);
		ERROR_IF(err, err, pLabel, goto exit);
		ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pLabel, goto exit);
		// set block to code location
		ERROR_IF(!JCLClause_SetBlock(pClause, pLabel, GetCodeLocator(_this)), JCL_ERR_Undefined_Identifier, pLabel, goto exit);
	}

	// save init state
	SaveInitState(&orig);
	// parse block statement
	err = p_block(_this, &isCompound);
	if( err )
		goto exit;
	// restore init state
	RestoreInitState(&orig);
	// add code to jump to end
	fixBranchAddr = GetCodeLocator(_this);
	SetMarker(_this, &marker);
	cg_opcode(_this, op_bra);
	cg_opcode(_this, 2);
	newClauseAddr = GetCodeLocator(_this);
	// test for another clause
	err = pFile->PeekToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	if( tokenID == tk_clause )
	{
		// go into recursion
		err = p_clause(_this, pLocals, pClause);
		if( err )
			goto exit;
		// fix jump to end of clause
		pCode = CurrentOutFunc(_this)->mipCode;
		pCode->Set(pCode, fixBranchAddr + 1, GetCodeLocator(_this) - fixBranchAddr);
	}
	else
	{
		// remove last branch
		RestoreMarker(_this, &marker);
	}
	if( isFirst )
	{
		if( !JCLClause_FixBranches(pClause, CurrentOutFunc(_this)->mipCode, &pFail) )
		{
			pFile->SetLocator(pFile, pFail->miFilePos);
			ERROR(JCL_ERR_Undefined_Identifier, pFail->mipLabel, goto exit);
		}
		goto exit;
	}
	DELETE( pToken );
	DELETE( pLabel );
	DELETE( pVar );
	FreeInitState(&orig);
	return err;

exit:
	// destroy clause
	if( isFirst )
	{
		FreeLocalVars(_this, pLocals);
		DELETE( pLocals );
		_this->miBlockLevel--;
		_this->mipClause = pSaveClause;
		DELETE( pClause );
	}
	DELETE( pToken );
	DELETE( pLabel );
	DELETE( pVar );
	FreeInitState(&orig);
	return err;
}

//------------------------------------------------------------------------------
// p_goto
//------------------------------------------------------------------------------
// Parse a goto statement

static JILError p_goto(JCLState* _this, Array_JCLVar* pLocals)
{
	JILError err = JCL_No_Error;
	JCLFile* pFile;
	JCLString* pToken;
	JCLString* pLabel;
	JCLVar* pVar;
	JCLClause* pClause;
	JILLong tokenID;
	JILLong labelPos;
	JILLong popPos;
	TypeInfo outType;

	pToken = NEW(JCLString);
	pLabel = NEW(JCLString);
	pVar = NEW(JCLVar);
	pFile = _this->mipFile;

	// get "goto"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);

	// get label
	err = pFile->GetToken(pFile, pLabel, &tokenID);
	ERROR_IF(err, err, pLabel, goto exit);
	ERROR_IF(tokenID != tk_identifier, JCL_ERR_Unexpected_Token, pLabel, goto exit);
	labelPos = pFile->GetLocator(pFile);

	// get clause for this label
	pClause = _this->mipClause;
	while( pClause != NULL )
	{
		if( JCLClause_GetBlock(pClause, pLabel) != NULL )
			break;
		pClause = pClause->mipParent;
	}
	if( pClause == NULL )
		ERROR(JCL_ERR_Goto_Without_Context, pLabel, goto exit);

	// get "("
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_open, JCL_ERR_Unexpected_Token, pToken, goto exit);

	// get goto parameter
	err = p_expression(_this, pLocals, pClause->miParameter, &outType, 0);
	if( err )
		goto exit;

	// generate code to unroll stack
	popPos = GetCodeLocator(_this);
	cg_opcode(_this, op_popm);
	cg_opcode(_this, 0);

	// generate branch
	JCLClause_AddGoto(pClause, pLabel, popPos, GetCodeLocator(_this), _this->miStackPos, labelPos);
	cg_opcode(_this, op_bra);
	cg_opcode(_this, 0);

	// get ")"
	err = pFile->GetToken(pFile, pToken, &tokenID);
	ERROR_IF(err, err, pToken, goto exit);
	ERROR_IF(tokenID != tk_round_close, JCL_ERR_Unexpected_Token, pToken, goto exit);

exit:
	DELETE( pToken );
	DELETE( pLabel );
	DELETE( pVar );
	return err;
}

/******************************************************************************/
/*********************** Code Generator Functions *****************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// cg_opcode
//------------------------------------------------------------------------------
// write an opcode.

static void cg_opcode(JCLState* _this, JILLong opcode)
{
	Array_JILLong* pCode = CurrentOutFunc(_this)->mipCode;
	pCode->Set(pCode, pCode->count, opcode);
}

//------------------------------------------------------------------------------
// cg_push_multi
//------------------------------------------------------------------------------
// push one or more null handles onto the stack: pushm "count"

static void cg_push_multi(JCLState* _this, JILLong count)
{
	// OPTIMIZATION: If there are only 3 or less null handles to push, use single pushes
	if( count <= kPushMultiThreshold )
	{
		int i;
		for( i = 0; i < count; i++ )
		{
			cg_opcode(_this, op_push);
		}
	}
	else
	{
		cg_opcode(_this, op_pushm);
		cg_opcode(_this, count);
	}
}

//------------------------------------------------------------------------------
// cg_pop_multi
//------------------------------------------------------------------------------
// pop one or more handles from the stack: popm "count"

static void cg_pop_multi(JCLState* _this, JILLong count)
{
	// OPTIMIZATION: If there are only 3 or less handles to pop, use single pops
	if( count <= kPushMultiThreshold )
	{
		int i;
		for( i = 0; i < count; i++ )
		{
			cg_opcode(_this, op_pop);
		}
	}
	else
	{
		cg_opcode(_this, op_popm);
		cg_opcode(_this, count);
	}
}

//------------------------------------------------------------------------------
// cg_return
//------------------------------------------------------------------------------
// return from function call: ret

static void cg_return(JCLState* _this)
{
	cg_opcode(_this, op_ret);
}

//------------------------------------------------------------------------------
// cg_get_literal
//------------------------------------------------------------------------------
// get a constant into a variable, used from p_expr_atomic().

static JILError cg_get_literal(JCLState* _this, JILLong type, const JCLString* pData, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JILBool bNegative)
{
	JILError err = JCL_No_Error;
	JILLong codePos;
	JCLLiteral* pLit;
	JCLVar* pWorkVar;
	JILChar* pDummy;
	JCLFunc* pFunc;

	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, pLVar);
		if( err )
			goto exit;
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	pWorkVar->miType = type;
	pWorkVar->miUnique = JILFalse;
	pWorkVar->miConst = JILTrue;
	pWorkVar->miRef = JILFalse;
	pWorkVar->miInited = JILFalse;
	codePos = GetCodeLocator(_this) + 1;
	err = cg_moveh_var(_this, 0, pWorkVar);
	if( err )
		goto exit;
	pWorkVar->miInited = JILTrue;

	// store literal for later code fixup
	pFunc = CurrentOutFunc(_this);
	pLit = pFunc->mipLiterals->New(pFunc->mipLiterals);
	pLit->miType = type;
	switch( type )
	{
		case type_int:
			pLit->miLong = strtol(JCLGetString(pData), &pDummy, 0);
			if( bNegative )
				pLit->miLong = -pLit->miLong;
			break;
		case type_float:
			pLit->miFloat = strtod(JCLGetString(pData), &pDummy);
			if( bNegative )
				pLit->miFloat = -pLit->miFloat;
			break;
		case type_string:
			JCLSetString(pLit->miString, JCLGetString(pData));
			break;
	}
	pLit->miOffset = codePos;

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_load_literal
//------------------------------------------------------------------------------
// load a constant into a variable

static JILError cg_load_literal(JCLState* _this, JILLong type, const JILChar* pData, JCLVar* dst, JILBool bNegative, TypeInfo* pOut)
{
	JILError err = JCL_No_Error;
	JILLong codePos;
	JCLLiteral* pLit;
	JCLVar* pTempVar = NULL;
	JCLVar* pDupVar = NULL;
	JCLVar* pWorkVar;
	JCLFunc* pFunc;
	char* pDummy;

	// old school way of doing it...
	pDupVar = NEW(JCLVar);
	if( IsTempVar(dst) )
	{
		// we must not modify the given 'dst' object!
		pDupVar->Copy(pDupVar, dst);
		pWorkVar = pDupVar;
	}
	else
	{
		err = MakeTempVar(_this, &pTempVar, dst);
		if( err )
			goto exit;
		pWorkVar = pTempVar;
	}
	pWorkVar->miType = type;
	pWorkVar->miUnique = JILFalse;
	pWorkVar->miConst = JILTrue;
	pWorkVar->miRef = JILFalse;
	pWorkVar->miInited = JILFalse;
	codePos = GetCodeLocator(_this) + 1;
	cg_opcode(_this, op_moveh_r);
	cg_opcode(_this, 0);
	cg_opcode(_this, pWorkVar->miIndex);
	err = cg_move_var(_this, pWorkVar, dst);
	if( err )
		goto exit;
	pWorkVar->miInited = JILTrue;
	JCLTypeInfoSrcDst(pOut, pWorkVar, dst);
	FreeTempVar(_this, &pTempVar);

	// store literal for later code fixup
	pFunc = CurrentOutFunc(_this);
	pLit = CurrentFunc(_this)->mipLiterals->New(CurrentFunc(_this)->mipLiterals);
	pLit->miType = type;
	switch( type )
	{
		case type_int:
			pLit->miLong = strtol(pData, &pDummy, 0);
			if( bNegative )
				pLit->miLong = -pLit->miLong;
			break;
		case type_float:
			pLit->miFloat = strtod(pData, &pDummy);
			if( bNegative )
				pLit->miFloat = -pLit->miFloat;
			break;
		case type_string:
			JCLSetString(pLit->miString, pData);
			break;
	}
	pLit->miOffset = codePos;

exit:
	FreeTempVar(_this, &pTempVar);
	DELETE( pDupVar );
	return err;
}

//------------------------------------------------------------------------------
// cg_load_null
//------------------------------------------------------------------------------
// Load the null handle into a variable.
// We define null is NOT a constant, so we can pass null to functions that
// take a non-const reference. This would be wrong for all other data types
// of course. However, it is safe for 'null' because it has no data we could
// modify through the reference.
// We also make the type of 'null' compatible to the expected result type to
// make sure assignments will not fail.

static JILError cg_load_null(JCLState* _this, JCLVar* dst, TypeInfo* pOut)
{
	JILError err = JCL_No_Error;
	JCLVar* pTempVar = NULL;
	JCLVar* pDupVar;
	JCLVar* pWorkVar;

	pDupVar = NEW(JCLVar);
	if( !IsDstTakingRef(dst) )
	{
		JCLString* str = NEW(JCLString);
		JCLSetString(str, "null");
		EmitWarning(_this, str, JCL_WARN_Null_Assign_No_Ref);
		DELETE(str);
	}
	if( IsTempVar(dst) )
	{
		// we must not modify the given 'dst' object!
		pDupVar->Copy(pDupVar, dst);
		pWorkVar = pDupVar;
	}
	else
	{
		err = MakeTempVar(_this, &pTempVar, dst);
		if( err )
			goto exit;
		pWorkVar = pTempVar;
	}
	pWorkVar->miType = dst->miType;
	pWorkVar->miUnique = JILFalse;
	pWorkVar->miConst = JILFalse;
	pWorkVar->miRef = JILFalse;
	pWorkVar->miInited = JILTrue;
	cg_opcode(_this, op_moveh_r);
	cg_opcode(_this, 0);
	cg_opcode(_this, pWorkVar->miIndex);
	err = cg_move_var(_this, pWorkVar, dst);
	if( err )
		goto exit;
	JCLTypeInfoSrcDst(pOut, pWorkVar, dst);
	FreeTempVar(_this, &pTempVar);

exit:
	FreeTempVar(_this, &pTempVar);
	DELETE( pDupVar );
	return err;
}

//------------------------------------------------------------------------------
// cg_load_func_literal
//------------------------------------------------------------------------------
// Load a function literal into a delegate variable. If pObj is NULL, creates a
// function delegate, else creates a method delegate for pObj's type.

static JILError cg_load_func_literal(JCLState* _this, JILLong codeLocator, JCLVar* pLVar, JCLVar** ppVarOut, JCLVar** ppTempVar, JCLVar* pObj)
{
	JILError err = JCL_No_Error;
	JILLong codePos;
	JCLLiteral* pLit;
	JCLVar* pWorkVar;
	JCLFunc* pFunc;

	if( TypeFamily(_this, pLVar->miType) != tf_delegate )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	if( pLVar && IsTempVar(pLVar) )
	{
		*ppVarOut = pLVar;
		pWorkVar = pLVar;
	}
	else
	{
		err = MakeTempVar(_this, ppTempVar, pLVar);
		if( err )
			goto exit;
		*ppVarOut = *ppTempVar;
		pWorkVar = *ppTempVar;
	}
	codePos = GetCodeLocator(_this) + 2;

	// create function delegate
	err = cg_new_delegate(_this, pWorkVar->miType, pObj, pWorkVar);
	if( err )
		goto exit;

	// store literal for later processing
	pFunc = CurrentOutFunc(_this);
	pLit = pFunc->mipLiterals->New(pFunc->mipLiterals);
	pLit->miType = pLVar->miType;
	pLit->miOffset = codePos;
	pLit->miLocator = codeLocator;
	pLit->miMethod = (pObj != NULL);

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_call_static
//------------------------------------------------------------------------------
// Call a static function: calls "handle"

static void cg_call_static(JCLState* _this, JILLong handle)
{
	cg_opcode(_this, op_calls);
	cg_opcode(_this, handle);
}

//------------------------------------------------------------------------------
// cg_push_registers
//------------------------------------------------------------------------------
//

static void cg_push_registers(JCLState* _this, JILLong* regUsage, JILLong num)
{
	int i;
	// OPTIMIZATION: If there are only kPushRegisterThreshold or less registers to save, use single push
	if( num <= kPushRegisterThreshold )
	{
		for( i = 0; i < kNumRegisters; i++ )
		{
			if( regUsage[i] )
			{
				cg_opcode(_this, op_push_r);
				cg_opcode(_this, i);
			}
		}
	}
	else
	{
		for( i = 0; i < kNumRegisters; i++ )
		{
			if( regUsage[i] )
				break;
		}
		cg_opcode(_this, op_pushr);
		cg_opcode(_this, i);
		cg_opcode(_this, num);
	}
}

//------------------------------------------------------------------------------
// cg_pop_registers
//------------------------------------------------------------------------------
//

static void cg_pop_registers(JCLState* _this, JILLong* regUsage, JILLong num)
{
	int i;
	// OPTIMIZATION: If there are only kPushRegisterThreshold or less registers to restore, use single pop
	if( num <= kPushRegisterThreshold )
	{
		for( i = kNumRegisters - 1; i >= 0; i-- )
		{
			if( regUsage[i] )
			{
				cg_opcode(_this, op_pop_r);
				cg_opcode(_this, i);
			}
		}
	}
	else
	{
		for( i = 0; i < kNumRegisters; i++ )
		{
			if( regUsage[i] )
				break;
		}
		cg_opcode(_this, op_popr);
		cg_opcode(_this, i);
		cg_opcode(_this, num);
	}
}

//------------------------------------------------------------------------------
// cg_use_wref
//------------------------------------------------------------------------------
// Checks if we need to use a weak reference.

static JILBool cg_use_wref(JCLVar* src, JCLVar* dst)
{
	return (IsWeakRef(dst) && !IsTempVar(dst) && !IsWeakRef(src));
}

//------------------------------------------------------------------------------
// cg_use_move
//------------------------------------------------------------------------------
// Checks if we are allowed to use a move.

static JILBool cg_use_move(JCLVar* src, JCLVar* dst)
{
	return (IsRef(dst) || IsTempVar(dst) || /*TODO: TEST*/ IsDstConst(dst) || (IsTempVar(src) && src->miUnique));
}

//------------------------------------------------------------------------------
// cg_move_var
//------------------------------------------------------------------------------
// Helper function that generates code to move, copy or set a variable from any
// place to any place.

static JILError cg_move_var(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JCLVar* newsrc;
	JCLVar* tmpvar = NULL;
	JILBool bCopy = JILFalse;
	JILLong opMode;

	// check type and auto-convert
	err = cg_auto_convert(_this, src, dst, &newsrc, &tmpvar);
	if( err )
		goto exit;
	// check if a weak reference is assigned a temporary, unique value
	if( !IsDstInited(dst) && IsWeakRef(dst) && IsTempVar(newsrc) && newsrc->miUnique )
		EmitWarning(_this, NULL, JCL_WARN_Assign_WRef_Temp_Value);
	// check if a reference is taken from a weak reference
	if( !IsDstInited(dst) && !IsTempVar(dst) && !IsWeakRef(dst) && IsWeakRef(newsrc) )
		EmitWarning(_this, NULL, JCL_WARN_Taking_Ref_From_Wref);
	// check access rules
	err = cg_src_dst_rule(_this, newsrc, dst);
	// taking ref from const: auto-copy
	if(	err == JCL_ERR_Expression_Is_Const && !IsWeakRef(dst) )
	{
		EmitWarning(_this, NULL, JCL_WARN_Auto_Copy_Const);
		bCopy = JILTrue;
		err = JCL_No_Error;
	}
	if( err )
		goto exit;
	// if dest is temp var and ref and src is const, copy constancy to dest // TODO: Test if this new check has any negative results!
	if( IsTempVar(dst) && IsDstTakingRef(dst) && IsSrcConst(src) )
	{
		dst->miConst = src->miConst;
		dst->miConstP = src->miConstP;
	}
	// if source is temp var and unique, dest is also unique
	if( IsTempVar(dst) && IsTempVar(newsrc) )
		dst->miUnique = newsrc->miUnique;
	else if( dst->miRef || IsTempVar(dst) )
		dst->miUnique = JILFalse;
	else
		dst->miUnique = JILTrue;
	// check if we will use copy, move, or weak-ref
	if( cg_use_wref(newsrc, dst) && !bCopy )		// if dest is weak reference, use wref
		opMode = op_wref;
	else if( cg_use_move(newsrc, dst) && !bCopy )	// if dest is reference, or newsrc is temp and unique copy, use move
		opMode = op_move;
	else											// in all other cases we must copy
		opMode = op_copy;
	// if we are gonna copy the source, check if it's possible
	if ((opMode == op_copy) && !IsTypeCopyable(_this, newsrc->miType))
	{
		ERROR_IF((opMode == op_copy) && !IsTypeCopyable(_this, newsrc->miType), JCL_ERR_No_Copy_Constructor, NULL, goto exit);
	}
	// now generate the code according to addressing mode
	switch( newsrc->miMode )
	{
		case kModeRegister:
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_move_rr(_this, opMode, newsrc->miIndex, dst->miIndex);
					break;
				case kModeStack:
					cg_move_rs(_this, opMode, newsrc->miIndex, dst->miIndex);
					break;
				case kModeMember:
					cg_move_rd(_this, opMode, newsrc->miIndex, dst->miIndex, dst->miMember);
					break;
				case kModeArray:
					cg_move_rx(_this, opMode, newsrc->miIndex, dst->miIndex, dst->mipArrIdx->miIndex);
					break;
				default:
					FATALERROREXIT("cg_move_var", "Var mode not implemented");
					break;
			}
			break;
		case kModeStack:
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_move_sr(_this, opMode, newsrc->miIndex, dst->miIndex);
					break;
				case kModeStack:
					cg_move_ss(_this, opMode, newsrc->miIndex, dst->miIndex);
					break;
				case kModeMember:
					cg_move_sd(_this, opMode, newsrc->miIndex, dst->miIndex, dst->miMember);
					break;
				case kModeArray:
					cg_move_sx(_this, opMode, newsrc->miIndex, dst->miIndex, dst->mipArrIdx->miIndex);
					break;
				default:
					FATALERROREXIT("cg_move_var", "Var mode not implemented");
					break;
			}
			break;
		case kModeMember:
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_move_dr(_this, opMode, newsrc->miIndex, newsrc->miMember, dst->miIndex);
					break;
				case kModeStack:
					cg_move_ds(_this, opMode, newsrc->miIndex, newsrc->miMember, dst->miIndex);
					break;
				case kModeMember:
					cg_move_dd(_this, opMode, newsrc->miIndex, newsrc->miMember, dst->miIndex, dst->miMember);
					break;
				case kModeArray:
					cg_move_dx(_this, opMode, newsrc->miIndex, newsrc->miMember, dst->miIndex, dst->mipArrIdx->miIndex);
					break;
				default:
					FATALERROREXIT("cg_move_var", "Var mode not implemented");
					break;
			}
			break;
		case kModeArray:
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_move_xr(_this, opMode, newsrc->miIndex, newsrc->mipArrIdx->miIndex, dst->miIndex);
					break;
				case kModeStack:
					cg_move_xs(_this, opMode, newsrc->miIndex, newsrc->mipArrIdx->miIndex, dst->miIndex);
					break;
				case kModeMember:
					cg_move_xd(_this, opMode, newsrc->miIndex, newsrc->mipArrIdx->miIndex, dst->miIndex, dst->miMember);
					break;
				case kModeArray:
					cg_move_xx(_this, opMode, newsrc->miIndex, newsrc->mipArrIdx->miIndex, dst->miIndex, dst->mipArrIdx->miIndex);
					break;
				default:
					FATALERROREXIT("cg_move_var", "Var mode not implemented");
					break;
			}
			break;
		default:
			FATALERROREXIT("cg_move_var", "Var mode not implemented");
			break;
	}

exit:
	FreeTempVar(_this, &tmpvar);
	return err;
}

//------------------------------------------------------------------------------
// CanAddToArray
//------------------------------------------------------------------------------

static JILBool CanAddToArray(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	if( src->miType == type_array )
	{
		// check element types - equal or one of them var?
		if( src->miElemType == dst->miElemType || IsSubClass(_this, src->miElemType, dst->miElemType) || src->miElemType == type_var || dst->miElemType == type_var )
			return JILTrue;
	}
	else if( src->miType == dst->miElemType || IsSubClass(_this, src->miType, dst->miElemType) || src->miType == type_var || dst->miElemType == type_var )
	{
		return JILTrue;
	}
	return JILFalse;
}

//------------------------------------------------------------------------------
// cg_math_var
//------------------------------------------------------------------------------
// Helper function that generates code to add/sub/mul/div/mod two variables.

static JILError cg_math_var(JCLState* _this, JCLVar* src, JCLVar* dst, JILLong op)
{
	JILError err = JCL_No_Error;
	JILLong op1, op2, op3, op4, op5, op6, op7;
	JCLVar* newsrc;
	JCLVar* tmpvar = NULL;
	op1 = op2 = op3 = op4 = op5 = op6 = op7 = 0;

	// special case when adding to an array
	if( dst->miType == type_array && (op == tk_plus || op == tk_plus_assign) )
	{
		if( !CanAddToArray(_this, src, dst) )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
		err = JCL_No_Error;
		newsrc = src;
	}
	else
	{
		// check type and auto-convert
		err = cg_auto_convert(_this, src, dst, &newsrc, &tmpvar);
		if( err )
			goto exit;
	}
	// check access rules
	if( dst->miType != type_array && IsAssignOperator(op) )
	{
		err = cg_dst_modify_rule(_this, dst);
		if( err )
			goto exit;
	}
	else if( dst->miType == type_array && (op == tk_plus || op == tk_plus_assign) )
	{
		err = cg_add_array_rule(_this, newsrc, dst);
		if( err )
			goto exit;
	}
	else
	{
		err = cg_src_dst_rule(_this, newsrc, dst);
		if( err )
			goto exit;
	}
	if( op == tk_plus || op == tk_plus_assign )
	{
		if( dst->miType != type_array )
		{
			if( (dst->miType != type_string
			&&	dst->miType != type_int
			&&	dst->miType != type_float
			&&	dst->miType != type_var)
			||	(newsrc->miType != type_array
			&&	newsrc->miType != type_string
			&&	newsrc->miType != type_int
			&&	newsrc->miType != type_float
			&&	newsrc->miType != type_var) )
			{
				err = JCL_ERR_Incompatible_Type;
				goto exit;
			}
		}
	}
	else
	{
		if( (dst->miType != type_int
		&&	dst->miType != type_float
		&&	dst->miType != type_var)
		||	(newsrc->miType != type_int
		&&	newsrc->miType != type_float
		&&	newsrc->miType != type_var) )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
	}
	switch( op )
	{
		case tk_plus:
		case tk_plus_assign:
			if( dst->miType == type_array || newsrc->miType == type_array )
			{
				if (dst->miElemRef && !IsSrcConst(newsrc))
				{
					op1 = op_arrmv_rr;
					op2 = op_arrmv_rs;
					op3 = op_arrmv_sr;
					op4 = op_arrmv_rd;
					op5 = op_arrmv_dr;
					op6 = op_arrmv_rx;
					op7 = op_arrmv_xr;
				}
				else
				{
					op1 = op_arrcp_rr;
					op2 = op_arrcp_rs;
					op3 = op_arrcp_sr;
					op4 = op_arrcp_rd;
					op5 = op_arrcp_dr;
					op6 = op_arrcp_rx;
					op7 = op_arrcp_xr;
				}
			}
			else if( dst->miType == type_string || newsrc->miType == type_string )
			{
				op1 = op_stradd_rr;
				op2 = op_stradd_rs;
				op3 = op_stradd_sr;
				op4 = op_stradd_rd;
				op5 = op_stradd_dr;
				op6 = op_stradd_rx;
				op7 = op_stradd_xr;
			}
			else if( dst->miType == type_float || newsrc->miType == type_float )
			{
				op1 = op_addf_rr;
				op2 = op_addf_rs;
				op3 = op_addf_sr;
				op4 = op_addf_rd;
				op5 = op_addf_dr;
				op6 = op_addf_rx;
				op7 = op_addf_xr;
			}
			else if( dst->miType == type_int || newsrc->miType == type_int )
			{
				op1 = op_addl_rr;
				op2 = op_addl_rs;
				op3 = op_addl_sr;
				op4 = op_addl_rd;
				op5 = op_addl_dr;
				op6 = op_addl_rx;
				op7 = op_addl_xr;
			}
			else
			{
				op1 = op_add_rr;
				op2 = op_add_rs;
				op3 = op_add_sr;
				op4 = op_add_rd;
				op5 = op_add_dr;
				op6 = op_add_rx;
				op7 = op_add_xr;
			}
			break;
		case tk_minus:
		case tk_minus_assign:
			if( dst->miType == type_float || newsrc->miType == type_float )
			{
				op1 = op_subf_rr;
				op2 = op_subf_rs;
				op3 = op_subf_sr;
				op4 = op_subf_rd;
				op5 = op_subf_dr;
				op6 = op_subf_rx;
				op7 = op_subf_xr;
			}
			else if( dst->miType == type_int || newsrc->miType == type_int )
			{
				op1 = op_subl_rr;
				op2 = op_subl_rs;
				op3 = op_subl_sr;
				op4 = op_subl_rd;
				op5 = op_subl_dr;
				op6 = op_subl_rx;
				op7 = op_subl_xr;
			}
			else
			{
				op1 = op_sub_rr;
				op2 = op_sub_rs;
				op3 = op_sub_sr;
				op4 = op_sub_rd;
				op5 = op_sub_dr;
				op6 = op_sub_rx;
				op7 = op_sub_xr;
			}
			break;
		case tk_mul:
		case tk_mul_assign:
			if( dst->miType == type_float || newsrc->miType == type_float )
			{
				op1 = op_mulf_rr;
				op2 = op_mulf_rs;
				op3 = op_mulf_sr;
				op4 = op_mulf_rd;
				op5 = op_mulf_dr;
				op6 = op_mulf_rx;
				op7 = op_mulf_xr;
			}
			else if( dst->miType == type_int || newsrc->miType == type_int )
			{
				op1 = op_mull_rr;
				op2 = op_mull_rs;
				op3 = op_mull_sr;
				op4 = op_mull_rd;
				op5 = op_mull_dr;
				op6 = op_mull_rx;
				op7 = op_mull_xr;
			}
			else
			{
				op1 = op_mul_rr;
				op2 = op_mul_rs;
				op3 = op_mul_sr;
				op4 = op_mul_rd;
				op5 = op_mul_dr;
				op6 = op_mul_rx;
				op7 = op_mul_xr;
			}
			break;
		case tk_div:
		case tk_div_assign:
			if( dst->miType == type_float || newsrc->miType == type_float )
			{
				op1 = op_divf_rr;
				op2 = op_divf_rs;
				op3 = op_divf_sr;
				op4 = op_divf_rd;
				op5 = op_divf_dr;
				op6 = op_divf_rx;
				op7 = op_divf_xr;
			}
			else if( dst->miType == type_int || newsrc->miType == type_int )
			{
				op1 = op_divl_rr;
				op2 = op_divl_rs;
				op3 = op_divl_sr;
				op4 = op_divl_rd;
				op5 = op_divl_dr;
				op6 = op_divl_rx;
				op7 = op_divl_xr;
			}
			else
			{
				op1 = op_div_rr;
				op2 = op_div_rs;
				op3 = op_div_sr;
				op4 = op_div_rd;
				op5 = op_div_dr;
				op6 = op_div_rx;
				op7 = op_div_xr;
			}
			break;
		case tk_mod:
		case tk_mod_assign:
			if( dst->miType == type_float || newsrc->miType == type_float )
			{
				op1 = op_modf_rr;
				op2 = op_modf_rs;
				op3 = op_modf_sr;
				op4 = op_modf_rd;
				op5 = op_modf_dr;
				op6 = op_modf_rx;
				op7 = op_modf_xr;
			}
			else if( dst->miType == type_int || newsrc->miType == type_int )
			{
				op1 = op_modl_rr;
				op2 = op_modl_rs;
				op3 = op_modl_sr;
				op4 = op_modl_rd;
				op5 = op_modl_dr;
				op6 = op_modl_rx;
				op7 = op_modl_xr;
			}
			else
			{
				op1 = op_mod_rr;
				op2 = op_mod_rs;
				op3 = op_mod_sr;
				op4 = op_mod_rd;
				op5 = op_mod_dr;
				op6 = op_mod_rx;
				op7 = op_mod_xr;
			}
			break;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( newsrc->miMode )
	{
		case kModeRegister:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					op2 = op1;
					// fall thru
				case kModeStack:
					cg_opcode(_this, op2);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				case kModeMember:
					cg_opcode(_this, op4);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					cg_opcode(_this, dst->miMember);
					break;
				case kModeArray:
					cg_opcode(_this, op6);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					cg_opcode(_this, dst->mipArrIdx->miIndex);
					break;
				default:
					FATALERROREXIT("cg_math_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeStack:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op3);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_math_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeMember:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op5);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, newsrc->miMember);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_math_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeArray:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op7);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, newsrc->mipArrIdx->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_math_var", "Var mode not implemented");
					break;
			}
			break;
		}
		default:
			FATALERROREXIT("cg_math_var", "Var mode not implemented");
			break;
	}

exit:
	FreeTempVar(_this, &tmpvar);
	return err;
}

//------------------------------------------------------------------------------
// cg_compare_var
//------------------------------------------------------------------------------
// Generate code that compares two long or float values.

static JILError cg_compare_var(JCLState* _this, JILLong op, JCLVar* src1, JCLVar* src2, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JILLong op1, op2, op3;
	JCLVar* newsrc;
	JCLVar* tmpvar = NULL;
	op1 = op2 = op3 = 0;

	// check type and auto-convert
	err = cg_auto_convert(_this, src1, src2, &newsrc, &tmpvar);
	if( err )
		goto exit;
	// check access rules
	err = cg_src_src_rule(_this, newsrc, src2);
	if( err )
		goto exit;
	if( op == tk_equ || op == tk_not_equ )
	{
		if( (src2->miType != type_string
		&&	src2->miType != type_int
		&&	src2->miType != type_float
		&&	src2->miType != type_var)
		||	(newsrc->miType != type_string
		&&	newsrc->miType != type_int
		&&	newsrc->miType != type_float
		&&	newsrc->miType != type_var)
		||	(dst->miType != type_int
		&&	dst->miType != type_var) )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
	}
	else
	{
		if( (src2->miType != type_int
		&&	src2->miType != type_float
		&&	src2->miType != type_var)
		||	(newsrc->miType != type_int
		&&	newsrc->miType != type_float
		&&	newsrc->miType != type_var)
		||	(dst->miType != type_int
		&&	dst->miType != type_var) )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
	}
	switch( op )
	{
		case tk_equ:
			if( newsrc->miType == type_string || src2->miType == type_string )
			{
				op1 = op_streq_rr;
				op2 = op_streq_rs;
				op3 = op_streq_sr;
			}
			else if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_cseqf_rr;
				op2 = op_cseqf_rs;
				op3 = op_cseqf_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_cseql_rr;
				op2 = op_cseql_rs;
				op3 = op_cseql_sr;
			}
			else
			{
				op1 = op_cseq_rr;
				op2 = op_cseq_rs;
				op3 = op_cseq_sr;
			}
			break;
		case tk_not_equ:
			if( newsrc->miType == type_string || src2->miType == type_string )
			{
				op1 = op_strne_rr;
				op2 = op_strne_rs;
				op3 = op_strne_sr;
			}
			else if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_csnef_rr;
				op2 = op_csnef_rs;
				op3 = op_csnef_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_csnel_rr;
				op2 = op_csnel_rs;
				op3 = op_csnel_sr;
			}
			else
			{
				op1 = op_csne_rr;
				op2 = op_csne_rs;
				op3 = op_csne_sr;
			}
			break;
		case tk_greater:
			if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_csgtf_rr;
				op2 = op_csgtf_rs;
				op3 = op_csgtf_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_csgtl_rr;
				op2 = op_csgtl_rs;
				op3 = op_csgtl_sr;
			}
			else
			{
				op1 = op_csgt_rr;
				op2 = op_csgt_rs;
				op3 = op_csgt_sr;
			}
			break;
		case tk_greater_equ:
			if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_csgef_rr;
				op2 = op_csgef_rs;
				op3 = op_csgef_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_csgel_rr;
				op2 = op_csgel_rs;
				op3 = op_csgel_sr;
			}
			else
			{
				op1 = op_csge_rr;
				op2 = op_csge_rs;
				op3 = op_csge_sr;
			}
			break;
		case tk_less:
			if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_csltf_rr;
				op2 = op_csltf_rs;
				op3 = op_csltf_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_csltl_rr;
				op2 = op_csltl_rs;
				op3 = op_csltl_sr;
			}
			else
			{
				op1 = op_cslt_rr;
				op2 = op_cslt_rs;
				op3 = op_cslt_sr;
			}
			break;
		case tk_less_equ:
			if( newsrc->miType == type_float || src2->miType == type_float )
			{
				op1 = op_cslef_rr;
				op2 = op_cslef_rs;
				op3 = op_cslef_sr;
			}
			else if( newsrc->miType == type_int || src2->miType == type_int )
			{
				op1 = op_cslel_rr;
				op2 = op_cslel_rs;
				op3 = op_cslel_sr;
			}
			else
			{
				op1 = op_csle_rr;
				op2 = op_csle_rs;
				op3 = op_csle_sr;
			}
			break;
	}
	// temp var value is unique
	if( IsTempVar(dst) )
		dst->miUnique = JILTrue;
	switch( newsrc->miMode )
	{
		case kModeRegister:
		{
			switch( src2->miMode )
			{
				case kModeRegister:
					op2 = op1;
					// fall thru
				case kModeStack:
					cg_opcode(_this, op2);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, src2->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_compare_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeStack:
		{
			switch( src2->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op3);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, src2->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_compare_var", "Var mode not implemented");
					break;
			}
			break;
		}
		default:
			FATALERROREXIT("cg_compare_var", "Var mode not implemented");
			break;
	}

exit:
	FreeTempVar(_this, &tmpvar);
	return err;
}

//------------------------------------------------------------------------------
// cg_not_var
//------------------------------------------------------------------------------
// Helper function that generates code for the boolean not operator.

static JILError cg_not_var(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	err = cg_copy_modify_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_int && dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_unot_r);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_unot_s);
			cg_opcode(_this, dst->miIndex);
			break;
		default:
			FATALERROREXIT("cg_not_var", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_modify_temp
//------------------------------------------------------------------------------
// This must be called from all code generator functions that would cause the
// destination value to be modified. For example a math operation or an
// assignment to a reference.
// This function must be called BEFORE actually generating the code. It will
// create a unique copy, if the destination is a temp var. For performance
// reasons, temp vars are always treated as references unless their values
// get modified.

static JILError cg_modify_temp(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	if( IsTempVar(dst) && !dst->miUnique )
	{
		if( !IsTypeCopyable(_this, dst->miType) )
		{
			err = JCL_ERR_No_Copy_Constructor;
			goto exit;
		}
		cg_opcode(_this, op_copy_rr);
		cg_opcode(_this, dst->miIndex);
		cg_opcode(_this, dst->miIndex);
		dst->miUnique = JILTrue;
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_push_var
//------------------------------------------------------------------------------
// Push a variable onto the stack.

static JILError cg_push_var(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_push_r);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_push_s);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op_push_d);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op_push_x);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->mipArrIdx->miIndex);
			break;
		default:
			FATALERROREXIT("cg_push_var", "Var mode not implemented");
			break;
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_call_member
//------------------------------------------------------------------------------
// Call a member function: callm "type", "index"

static void cg_call_member(JCLState* _this, JILLong type, JILLong index)
{
	cg_opcode(_this, op_callm);
	cg_opcode(_this, type);
	cg_opcode(_this, index);
}

//------------------------------------------------------------------------------
// cg_call_factory
//------------------------------------------------------------------------------
// Call a member function: calli "type", "index"

static void cg_call_factory(JCLState* _this, JILLong type, JILLong index)
{
	cg_opcode(_this, op_calli);
	cg_opcode(_this, type);
	cg_opcode(_this, index);
}

//------------------------------------------------------------------------------
// cg_alloc_var
//------------------------------------------------------------------------------
// Allocate a JIL object or native object.

static JILError cg_alloc_var(JCLState* _this, JCLVar* src, JCLVar* dst, JILBool isNTL)
{
	JILError err = JCL_No_Error;
	JILLong op1;
	// check access rules
	err = cg_dst_assign_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( !ImpConvertible(_this, src, dst) )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	op1 = isNTL ? op_allocn : op_alloc;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op1);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op1);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeMember:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op1);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeArray:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op1);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		default:
			FATALERROREXIT("cg_alloc_var", "Var mode not supported");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_alloci_var
//------------------------------------------------------------------------------
// Allocate an array of JIL objects or native objects.

static JILError cg_alloci_var(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// check access rules
	err = cg_dst_assign_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( !IsInterfaceType(_this, src->miType) || dst->miType != type_array || dst->miElemType != src->miType )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_alloci);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloci);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeMember:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloci);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeArray:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloci);
			cg_opcode(_this, src->miType);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		default:
			FATALERROREXIT("cg_alloci_var", "Var mode not supported");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_change_context
//------------------------------------------------------------------------------
// Change the context to an other object. Moves var to r0, which is 'this'.

static JILError cg_change_context(JCLState* _this, JCLVar* src)
{
	JILError err = JCL_No_Error;
	if( !IsVarClassType(_this, src)
	&&	src->miType != type_var )
		FATALERROREXIT("cg_change_context", "Trying to change context to non-object variable");
	switch( src->miMode )
	{
		case kModeRegister:
			if( src->miIndex )
			{
				// only if source is not r0
				cg_opcode( _this, op_move_rr );
				cg_opcode( _this, src->miIndex );
				cg_opcode( _this, 0 );				// r0
			}
			break;
		case kModeStack:
		{
			cg_opcode( _this, op_move_sr );
			cg_opcode( _this, src->miIndex );
			cg_opcode( _this, 0 );					// r0
			break;
		}
		case kModeMember:
		{
			cg_opcode( _this, op_move_dr );
			cg_opcode( _this, src->miIndex );
			cg_opcode( _this, src->miMember );
			cg_opcode( _this, 0 );					// r0
			break;
		}
		case kModeArray:
		{
			cg_opcode( _this, op_move_xr );
			cg_opcode( _this, src->miIndex );
			cg_opcode( _this, src->mipArrIdx->miIndex );
			cg_opcode( _this, 0 );					// r0
			break;
		}
		default:
			FATALERROREXIT("cg_change_context", "Var mode not implemented");
			break;
	}
exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_call_native
//------------------------------------------------------------------------------
// Call a native typelib's static function: calln "type", "index"

static void cg_call_native(JCLState* _this, JILLong type, JILLong index)
{
	cg_opcode(_this, op_calln);
	cg_opcode(_this, type);
	cg_opcode(_this, index);
}

//------------------------------------------------------------------------------
// cg_incdec_var
//------------------------------------------------------------------------------
// Generate code for ++/-- operator

static JILError cg_incdec_var(JCLState* _this, JCLVar* dst, JILBool bInc)
{
	JILLong op1, op2, op3, op4;
	JILError err = JCL_No_Error;
	// check access rules
	err = cg_dst_modify_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_int && dst->miType != type_float && dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	switch( dst->miType )
	{
		case type_float:
			op1 = bInc ? op_incf_r : op_decf_r;
			op2 = bInc ? op_incf_s : op_decf_s;
			op3 = bInc ? op_incf_d : op_decf_d;
			op4 = bInc ? op_incf_x : op_decf_x;
			break;
		case type_int:
			op1 = bInc ? op_incl_r : op_decl_r;
			op2 = bInc ? op_incl_s : op_decl_s;
			op3 = bInc ? op_incl_d : op_decl_d;
			op4 = bInc ? op_incl_x : op_decl_x;
			break;
		default:
			op1 = bInc ? op_inc_r : op_dec_r;
			op2 = bInc ? op_inc_s : op_dec_s;
			op3 = bInc ? op_inc_d : op_dec_d;
			op4 = bInc ? op_inc_x : op_dec_x;
			break;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op1);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op2);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op3);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op4);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->mipArrIdx->miIndex);
			break;
		default:
			FATALERROREXIT("cg_incdec_var", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_neg_var
//------------------------------------------------------------------------------
// Generates code to negate a variable.

static JILError cg_neg_var(JCLState* _this, JCLVar* dst)
{
	JILLong op1, op2;
	JILError err = JCL_No_Error;
	// check access rules
	err = cg_copy_modify_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_int && dst->miType != type_float && dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	switch( dst->miType )
	{
		case type_float:
			op1 = op_negf_r;
			op2 = op_negf_s;
			break;
		case type_int:
			op1 = op_negl_r;
			op2 = op_negl_s;
			break;
		default:
			op1 = op_neg_r;
			op2 = op_neg_s;
			break;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op1);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op2);
			cg_opcode(_this, dst->miIndex);
			break;
		default:
			FATALERROREXIT("cg_neg_var", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_convertor_convert
//------------------------------------------------------------------------------
// Tries to convert a source type to a destination type by calling a suitable
// convertor function from the source. Helper function for cg_auto_convert().

static JILError cg_convertor_convert(JCLState* _this, JCLVar* src, JCLVar* dst, JCLVar** ppSrcOut, JCLVar** ppTmpOut)
{
	JILError err;
	JCLFunc* pFunc;
	JCLVar* r0 = NULL;
	// try to find a convertor for the source type
	err = FindConvertor(_this, src, dst, &pFunc);
	if( !err )
	{
		JCLClass* pClass;
		JCLVar* r1;
		pClass = GetClass(_this, pFunc->miClassID);
		r0 = NEW(JCLVar);
		r0->miMode = kModeRegister;
		r0->miIndex = 0;
		r0->CopyType(r0, src);
		r0->miRef = JILTrue;
		r0->miWeak = JILFalse;
		r1 = pFunc->mipResult;
		// check if we are allowed to auto-convert
		if( pFunc->miExplicit && !dst->miTypeCast )
		{
			err = JCL_ERR_Conv_Requires_Cast;
			goto exit;
		}
		err = MakeTempVar(_this, ppTmpOut, r1);
		if( err )
			goto exit;
		*ppSrcOut = *ppTmpOut;
		// we must save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		// move src into r0
		err = cg_move_var(_this, src, r0);
		if( err )
			goto error;
		// native type?
		if( pClass->miNative )
		{
			cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);	// call method
		}
		else
		{
			if( pClass->miFamily == tf_interface )
				cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);	// call pure method
			else
				cg_call_static(_this, pFunc->miHandle);		// call method
		}
		// restore r0
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
		// move result to temp var
		err = cg_move_var(_this, r1, *ppTmpOut);
		if( err )
			goto error;
	}
exit:
	DELETE(r0);
	return err;
error:
	FreeTempVar(_this, ppTmpOut);
	DELETE(r0);
	return err;
}

//------------------------------------------------------------------------------
// cg_ctor_convert
//------------------------------------------------------------------------------
// Tries to convert a source type to a destination type by calling a suitable
// constructor from the destination. Helper function for cg_auto_convert().

static JILError cg_ctor_convert(JCLState* _this, JCLVar* src, JCLVar* dst, JCLVar** ppSrcOut, JCLVar** ppTmpOut)
{
	JILError err;
	JCLFunc* pFunc;
	JCLVar* r0 = NULL;
	JCLVar* r1 = NULL;
	// try to call class constructor
	err = FindConstructor(_this, src, dst, &pFunc);
	if( !err )
	{
		JCLClass* pClass;
		pClass = GetClass(_this, pFunc->miClassID);
		r0 = NEW(JCLVar);
		r1 = NEW(JCLVar);
		r0->miMode = kModeRegister;
		r0->miIndex = 0;
		r0->CopyType(r0, dst);
		r0->miRef = JILTrue;
		r0->miWeak = JILFalse;
		r0->miInited = JILTrue;
		r1->Copy(r1, r0);
		r1->miIndex = 1;
		r1->miUsage = kUsageResult;
		r1->miInited = JILFalse;
		// check if we are allowed to auto-convert
		if( pFunc->miExplicit && !dst->miTypeCast )
		{
			err = JCL_ERR_Conv_Requires_Cast;
			goto exit;
		}
		err = MakeTempVar(_this, ppTmpOut, dst);
		if( err )
			goto exit;
		*ppSrcOut = *ppTmpOut;
		// we must save r0
		cg_opcode(_this, op_push_r);
		cg_opcode(_this, 0);
		SimStackReserve(_this, 1);
		// fatal if interface!
		if( pClass->miFamily != tf_class )
			FATALERROR("cg_ctor_convert", "Destination type is not class", err = JCL_ERR_Fatal_Error; goto error);
		// native typelib?
		if( pClass->miNative )
		{
			err = cg_push_var(_this, src);		// push src as argument
			if( err )
				goto error;
			SimStackReserve(_this, 1);
			cg_opcode(_this, op_allocn);		// alloc new object
			cg_opcode(_this, pClass->miType);
			cg_opcode(_this, 0);
			cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);	// call constructor
			cg_pop_multi(_this, 1);
			SimStackPop(_this, 1);
			err = cg_move_var(_this, r0, r1);	// move result to r1
			if( err )
				goto error;
		}
		else
		{
			err = cg_push_var(_this, src);		// push src as argument
			if( err )
				goto error;
			SimStackReserve(_this, 1);
			cg_opcode(_this, op_alloc);			// alloc new object
			cg_opcode(_this, pClass->miType);
			cg_opcode(_this, 0);
			cg_call_static(_this, pFunc->miHandle);	// call constructor (this can never be an interface method)
			cg_pop_multi(_this, 1);
			SimStackPop(_this, 1);
			err = cg_move_var(_this, r0, r1);	// move result to r1
			if( err )
				goto error;
		}
		// restore r0
		cg_opcode(_this, op_pop_r);
		cg_opcode(_this, 0);
		SimStackPop(_this, 1);
		// move result to temp var
		err = cg_move_var(_this, r1, *ppTmpOut);
		if( err )
			goto error;
		// Theoretically, constructor result should always be unique
		(*ppTmpOut)->miUnique = JILTrue;
	}
exit:
	DELETE(r0);
	DELETE(r1);
	return err;
error:
	FreeTempVar(_this, ppTmpOut);
	DELETE(r0);
	DELETE(r1);
	return err;
}

//------------------------------------------------------------------------------
// cg_auto_convert
//------------------------------------------------------------------------------
// Generates code to convert one type to another.
// ATTENTION: This function allocates a temp var that the caller must free when
// he is finished with it.

static JILError cg_auto_convert(JCLState* _this, JCLVar* src, JCLVar* dst, JCLVar** ppSrcOut, JCLVar** ppTmpOut)
{
	JILError err = JCL_ERR_Incompatible_Type;
	if( ImpConvertible(_this, src, dst) )
	{
		if( (src->miType == type_var && dst->miType != type_var)
			|| (src->miMode == kModeArray && src->miType == type_var && dst->miType != type_var) )
		{
			if( dst->miType == type_string )
			{
				EmitWarning(_this, NULL, JCL_WARN_Dynamic_Conversion);
				err = MakeTempVar(_this, ppTmpOut, dst);
				if( err )
					goto exit;
				*ppSrcOut = *ppTmpOut;
				err = cg_dcvt_var(_this, src, *ppTmpOut);
				goto exit;
			}
			else if( GetOptions(_this)->miUseRTCHK )
			{
				EmitWarning(_this, NULL, JCL_WARN_Imp_Conv_From_Var);
				// place RTCHK instruction
				err = cg_rtchk(_this, src, dst->miType);
				if( err )
					goto exit;
			}
		}
		*ppSrcOut = src;
		err = JCL_No_Error;
		goto exit;
	}
	if( !IsTempVar(dst) && dst->miRef && !dst->miConst && !IsSubClass(_this, dst->miType, src->miType) )
	{
		// IF both types are NOT implicitly convertible AND
		// IF destination is NOT a temporary variable AND
		// IF destination is non-const reference THEN
		// emit warning and continue
		EmitWarning(_this, NULL, JCL_WARN_Auto_Convert_To_Ref);
	}
	if( src->miType == type_int && dst->miType == type_float )
	{
		err = MakeTempVar(_this, ppTmpOut, dst);
		if( err )
			goto exit;
		*ppSrcOut = *ppTmpOut;
		err = cg_cvf_var(_this, src, *ppTmpOut);
		if( err )
			goto exit;
	}
	else if( src->miType == type_float && dst->miType == type_int )
	{
		err = MakeTempVar(_this, ppTmpOut, dst);
		if( err )
			goto exit;
		*ppSrcOut = *ppTmpOut;
		err = cg_cvl_var(_this, src, *ppTmpOut);
		if( err )
			goto exit;
	}
	else if( IsVarClassType(_this, src) )
	{
		if( IsSubClass(_this, dst->miType, src->miType) )
		{
			// casting from interface to sub-class
			if( dst->miTypeCast )
			{
				if( GetOptions(_this)->miUseRTCHK )
				{
					// place RTCHK instruction
					err = cg_rtchk(_this, src, dst->miType);
					if( err )
						goto exit;
				}
				*ppSrcOut = src;
				err = JCL_No_Error;
				goto exit;
			}
			else
			{
				err = JCL_ERR_Conv_Requires_Cast;
				goto exit;
			}
		}
		else
		{
			// try to call a convertor method from the source class
			err = cg_convertor_convert(_this, src, dst, ppSrcOut, ppTmpOut);
			// if this failed, try to use a constructor from the destination class
			if( err && err != JCL_ERR_Conv_Requires_Cast )
				err = cg_ctor_convert(_this, src, dst, ppSrcOut, ppTmpOut);
		}
	}
	else if( IsVarClassType(_this, dst) )
	{
		// try to use a constructor from the destination class
		err = cg_ctor_convert(_this, src, dst, ppSrcOut, ppTmpOut);
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_add_array_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where a source value is moved or copied into an array.

static JILError cg_add_array_rule(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// is source inited?
	if( !IsSrcInited(src)
		&& !IsTempVar(src) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}
	// check if destination is const
	if( IsDstConst(dst)
		&& IsDstInited(dst)
		&& !IsTempVar(dst) )
	{
		err = JCL_ERR_LValue_Is_Const;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_src_dst_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where a destination var is modified using
// a source var as operand.
// 2005/11/02: There is an issue with checking if we are taking a reference of
// a constant in this function. Because in a expression using an arithmetical
// operator the "desired" dst operand inherits the result type of the src
// operand, this "desired" dst operand can be non-const reference (as a result
// of the src operand beeing a function call that returns a non-const ref).
// However, if the actual src operand is then a constant, we would get
// an error because this function would detect that we are taking a non-const
// ref from a constant.
// We work-around this issue by checking whether the dst operand is a temp-var,
// and only check for this error when the destination is really a (user defined)
// variable.

static JILError cg_src_dst_rule(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// is source inited?
	if( !IsSrcInited(src)
		&& !IsTempVar(src) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}
	// taking reference from const value?
	if(	!IsTempVar(dst)				// we MUST exclude operations on temp-vars!
		&& !IsDstConst(dst)			// destination not const?
		&& IsDstTakingRef(dst)		// destination taking a reference?
		&& IsSrcConst(src) )		// is source a constant?
	{
		err = JCL_ERR_Expression_Is_Const;
		goto exit;
	}
	// check if destination is const
	if( IsDstConst(dst)
		&& IsDstInited(dst)
		&& !IsTempVar(dst) )
	{
		err = JCL_ERR_LValue_Is_Const;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_src_src_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where two source vars are only read, but
// not modified.

static JILError cg_src_src_rule(JCLState* _this, JCLVar* src1, JCLVar* src2)
{
	JILError err = JCL_No_Error;
	// is variable inited?
	if( !IsSrcInited(src1)
		&& !IsTempVar(src1) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}
	if( !IsSrcInited(src2)
		&& !IsTempVar(src2) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_dst_modify_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where a single var gets directly modified.
// The destination will be read, modified and written back to the same var.

static JILError cg_dst_modify_rule(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// is variable inited?
	if( !IsSrcInited(dst)	// source cos we wanna check if reading the var is OK
		&& !IsTempVar(dst) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}
	// check if destination is const
	if( IsDstConst(dst)
		&& IsDstInited(dst)
		/* && !IsTempVar(dst) Temp-Vars are references, modifying them modifies the original, so this check is wrong! */
		)
	{
		err = JCL_ERR_LValue_Is_Const;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_dst_assign_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where a single var gets assigned a new or
// initial value. The destination will never be read, it's current value
// doesn't matter.

static JILError cg_dst_assign_rule(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// check if destination is const
	if( IsDstConst(dst)
		&& IsDstInited(dst)
		&& !IsTempVar(dst) )
	{
		err = JCL_ERR_LValue_Is_Const;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_copy_modify_rule
//------------------------------------------------------------------------------
// Tests access rules for operations where a single var gets copied and the
// copy is modified. The destination will keep it's value, instead a copy of it
// will be taken and modified.

static JILError cg_copy_modify_rule(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// in this case dst is really a source
	if( !IsSrcInited(dst)
		&& !IsTempVar(dst) )
	{
		err = JCL_ERR_Var_Not_Initialized;
		goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_testnull_var
//------------------------------------------------------------------------------
// Generate code that tests if a variable is null

static JILError cg_testnull_var(JCLState* _this, JILLong op, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JILLong opcode;
	// check access rules
	err = cg_src_dst_rule(_this, src, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_int
	&&	dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	if( op == tk_not_equ )
		opcode = op_snnul_rr;
	else
		opcode = op_snul_rr;
	if( IsTempVar(dst) )
	{
		switch( src->miMode )
		{
			case kModeRegister:
				cg_opcode(_this, opcode);
				cg_opcode(_this, src->miIndex);
				cg_opcode(_this, dst->miIndex);
				break;
			default:
				FATALERROREXIT("cg_testnull_var", "Var mode not implemented");
				break;
		}
	}
	else
	{
		switch( src->miMode )
		{
			case kModeRegister:
				cg_opcode(_this, opcode);
				cg_opcode(_this, src->miIndex);
				cg_opcode(_this, src->miIndex);
				break;
			default:
				FATALERROREXIT("cg_testnull_var", "Var mode not implemented");
				break;
		}
		err = cg_move_var(_this, src, dst);
		if( err )
			goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_move_rr
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_rr(JCLState* _this, JILLong mode, JILLong sReg, JILLong dReg)
{
	if( sReg == dReg && mode == op_move )
		return;
	if( mode == op_move )
		mode = op_move_rr;
	else if( mode == op_copy )
		mode = op_copy_rr;
	else
		mode = op_wref_rr;
	cg_opcode(_this, mode);
	cg_opcode(_this, sReg);
	cg_opcode(_this, dReg);
}

//------------------------------------------------------------------------------
// cg_move_rd
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_rd(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dIndex, JILLong dMember)
{
	if( mode == op_move )
		mode = op_move_rd;
	else if( mode == op_copy )
		mode = op_copy_rd;
	else
		mode = op_wref_rd;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dIndex);
	cg_opcode(_this, dMember);
}

//------------------------------------------------------------------------------
// cg_move_rx
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_rx(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dArray, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_rx;
	else if( mode == op_copy )
		mode = op_copy_rx;
	else
		mode = op_wref_rx;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dArray);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_rs
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_rs(JCLState* _this, JILLong mode, JILLong sReg, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_rs;
	else if( mode == op_copy )
		mode = op_copy_rs;
	else
		mode = op_wref_rs;
	cg_opcode(_this, mode);
	cg_opcode(_this, sReg);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_dr
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_dr(JCLState* _this, JILLong mode, JILLong sIndex, JILLong sMember, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_dr;
	else if( mode == op_copy )
		mode = op_copy_dr;
	else
		mode = op_wref_dr;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, sMember);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_dd
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_dd(JCLState* _this, JILLong mode, JILLong sIndex, JILLong sMember, JILLong dIndex, JILLong dMember)
{
	if( sIndex == dIndex && sMember == dMember && mode == op_move )
		return;
	if( mode == op_move )
		mode = op_move_dd;
	else if( mode == op_copy )
		mode = op_copy_dd;
	else
		mode = op_wref_dd;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, sMember);
	cg_opcode(_this, dIndex);
	cg_opcode(_this, dMember);
}

//------------------------------------------------------------------------------
// cg_move_dx
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_dx(JCLState* _this, JILLong mode, JILLong sIndex, JILLong sMember, JILLong dArray, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_dx;
	else if( mode == op_copy )
		mode = op_copy_dx;
	else
		mode = op_wref_dx;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, sMember);
	cg_opcode(_this, dArray);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_ds
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_ds(JCLState* _this, JILLong mode, JILLong sIndex, JILLong sMember, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_ds;
	else if( mode == op_copy )
		mode = op_copy_ds;
	else
		mode = op_wref_ds;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, sMember);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_xr
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_xr(JCLState* _this, JILLong mode, JILLong sArray, JILLong sIndex, JILLong dReg)
{
	if( mode == op_move )
		mode = op_move_xr;
	else if( mode == op_copy )
		mode = op_copy_xr;
	else
		mode = op_wref_xr;
	cg_opcode(_this, mode);
	cg_opcode(_this, sArray);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dReg);
}

//------------------------------------------------------------------------------
// cg_move_xd
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_xd(JCLState* _this, JILLong mode, JILLong sArray, JILLong sIndex, JILLong dIndex, JILLong dMember)
{
	if( mode == op_move )
		mode = op_move_xd;
	else if( mode == op_copy )
		mode = op_copy_xd;
	else
		mode = op_wref_xd;
	cg_opcode(_this, mode);
	cg_opcode(_this, sArray);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dIndex);
	cg_opcode(_this, dMember);
}

//------------------------------------------------------------------------------
// cg_move_xx
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_xx(JCLState* _this, JILLong mode, JILLong sArray, JILLong sIndex, JILLong dArray, JILLong dIndex)
{
	if( sArray == dArray && sIndex == dIndex && mode == op_move )
		return;
	if( mode == op_move )
		mode = op_move_xx;
	else if( mode == op_copy )
		mode = op_copy_xx;
	else
		mode = op_wref_xx;
	cg_opcode(_this, mode);
	cg_opcode(_this, sArray);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dArray);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_xs
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_xs(JCLState* _this, JILLong mode, JILLong sArray, JILLong sIndex, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_xs;
	else if( mode == op_copy )
		mode = op_copy_xs;
	else
		mode = op_wref_xs;
	cg_opcode(_this, mode);
	cg_opcode(_this, sArray);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_sr
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_sr(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dReg)
{
	if( mode == op_move )
		mode = op_move_sr;
	else if( mode == op_copy )
		mode = op_copy_sr;
	else
		mode = op_wref_sr;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dReg);
}

//------------------------------------------------------------------------------
// cg_move_sd
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_sd(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dIndex, JILLong dMember)
{
	if( mode == op_move )
		mode = op_move_sd;
	else if( mode == op_copy )
		mode = op_copy_sd;
	else
		mode = op_wref_sd;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dIndex);
	cg_opcode(_this, dMember);
}

//------------------------------------------------------------------------------
// cg_move_sx
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_sx(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dArray, JILLong dIndex)
{
	if( mode == op_move )
		mode = op_move_sx;
	else if( mode == op_copy )
		mode = op_copy_sx;
	else
		mode = op_wref_sx;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dArray);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_move_ss
//------------------------------------------------------------------------------
// Move, copy or set from source to dest

static void cg_move_ss(JCLState* _this, JILLong mode, JILLong sIndex, JILLong dIndex)
{
	if( sIndex == dIndex && mode == op_move )
		return;
	if( mode == op_move )
		mode = op_move_ss;
	else if( mode == op_copy )
		mode = op_copy_ss;
	else
		mode = op_wref_ss;
	cg_opcode(_this, mode);
	cg_opcode(_this, sIndex);
	cg_opcode(_this, dIndex);
}

//------------------------------------------------------------------------------
// cg_moveh_var
//------------------------------------------------------------------------------
// Generate a moveh instruction

static JILError cg_moveh_var(JCLState* _this, JILLong index, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JILLong op1, op2, op3, op4;
	if( dst->miInited )
			FATALERROREXIT("cg_moveh_var", "Initializing dst var that is already initialized");
	if( dst->miRef || IsTempVar(dst) )
	{
		op1 = op_moveh_r;
		op2 = op_moveh_d;
		op3 = op_moveh_x;
		op4 = op_moveh_s;
		dst->miUnique = JILFalse;
	}
	else
	{
		op1 = op_copyh_r;
		op2 = op_copyh_d;
		op3 = op_copyh_x;
		op4 = op_copyh_s;
	}
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op1);
			cg_opcode(_this, index);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op2);
			cg_opcode(_this, index);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op3);
			cg_opcode(_this, index);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->mipArrIdx->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op4);
			cg_opcode(_this, index);
			cg_opcode(_this, dst->miIndex);
			break;
		default:
			FATALERROREXIT("cg_moveh_var", "Var mode not supported");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_alloca_var
//------------------------------------------------------------------------------
// Allocate an array.

static JILError cg_alloca_var(JCLState* _this, JILLong type, JILLong dim, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// check access rules
	err = cg_dst_assign_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_var && dst->miType != type_array )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	if( dim < 0 )
		dim = 0;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_alloca);
			cg_opcode(_this, type);
			cg_opcode(_this, dim);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloca);
			cg_opcode(_this, type);
			cg_opcode(_this, dim);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeMember:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloca);
			cg_opcode(_this, type);
			cg_opcode(_this, dim);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		case kModeArray:
		{
			JCLVar* pTempVar = NULL;
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_alloca);
			cg_opcode(_this, type);
			cg_opcode(_this, dim);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			FreeTempVar(_this, &pTempVar);
			break;
		}
		default:
			FATALERROREXIT("cg_alloca_var", "Var mode not supported");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_begin_intro
//------------------------------------------------------------------------------
// create global "intro" code, allocate global space

JILError cg_begin_intro(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JCLClass* pClass;
	JCLFunc* pFunc;

	// create init function in global class
	pClass = GetClass(_this, type_global);
	pClass->miHasVTable = JILTrue;
	JCLSetString(pClass->mipTag, "This class is maintained by the runtime and represents the global space.");
	pFunc = pClass->mipFuncs->New(pClass->mipFuncs);
	pFunc->miHandle = 0;
	pFunc->miClassID = type_global;
	JCLSetString(pFunc->mipName, kNameGlobalInitFunction);
	JCLSetString(pFunc->mipTag, "The runtime automatically creates and calls this function to intialize all global variables.");
	err = JILCreateFunction(_this->mipMachine, type_global, 0, 0, kNameGlobalInitFunction, &(pFunc->miHandle));
	if( err )
		goto exit;

	// create allocation code for global space in init function
	_this->miIntroFinished = JILFalse;
	SetCompileContext(_this, type_global, 0);
	cg_opcode(_this, op_alloc);
	cg_opcode(_this, type_global);
	cg_opcode(_this, 2);	// register 2 reserved for global space!

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_finish_intro
//------------------------------------------------------------------------------
// finish global "intro" code, allocate global space

JILError cg_finish_intro(JCLState* _this)
{
	JILError err = JCL_No_Error;
	JILState* pMachine = _this->mipMachine;
	JCLClass* pClass;

	// restore __global::__init context
	SetCompileContext(_this, type_global, 0);

	// get global class
	pClass = GetClass(_this, type_global);

	// set / update global object size
	err = JILSetGlobalObjectSize(pMachine, type_global, pClass->mipVars->count);
	if( err )
		goto exit;

	// check if we need to add a RET instruction
	if( !_this->miIntroFinished )
	{
		_this->miIntroFinished = JILTrue;
		cg_opcode(_this, op_ret);
	}

	// set __init optimize level
	GetFunc(_this, type_global, 0)->miOptLevel = GetOptions(_this)->miOptimizeLevel;

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_resume_intro
//------------------------------------------------------------------------------
// takes care if the intro code has already been finalized after calling
// JCLLink(), but another piece of code is compiled by JCLCompile() after that.

JILError cg_resume_intro(JCLState* _this)
{
	JILError err = JCL_No_Error;
	Array_JILLong* pCode;
	// check if we must delete the RET instruction
	if( _this->miIntroFinished )
	{
		_this->miIntroFinished = JILFalse;
		pCode = GetFunc(_this, type_global, 0)->mipCode;				// get __init function
		pCode->Trunc(pCode, pCode->count - 1);							// remove last instruction word
		GetFunc(_this, type_global, 0)->miLinked = JILFalse;			// link again
	}
	return err;
}

//------------------------------------------------------------------------------
// cg_cvf_var
//------------------------------------------------------------------------------
// Create code for long to float conversion. Called from cg_auto_convert.
// Destination is always a temp register.

static JILError cg_cvf_var(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JCLVar* pTempVar = NULL;
	if( dst->miMode != kModeRegister )
		FATALERROREXIT("cg_cvl_var", "Var mode not implemented");
	// if using 32bit float, emit truncation warning
	if( sizeof(JILFloat) == sizeof(float) )
		EmitWarning(_this, NULL, JCL_WARN_Imp_Conv_Int_Float);
	switch( src->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_cvf);
			cg_opcode(_this, src->miIndex);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		case kModeMember:
		case kModeArray:
			err = MakeTempVar(_this, &pTempVar, src);
			if( err )
				goto exit;
			cg_move_var(_this, src, pTempVar);
			cg_opcode(_this, op_cvf);
			cg_opcode(_this, pTempVar->miIndex);
			cg_opcode(_this, dst->miIndex);
			FreeTempVar(_this, &pTempVar);
			break;
		default:
			FATALERROREXIT("cg_cvf_var", "Var mode not implemented");
			break;
	}
	// conversion creates unique value
	dst->miUnique = JILTrue;

exit:
	FreeTempVar(_this, &pTempVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_cvl_var
//------------------------------------------------------------------------------
// Create code for float to long conversion. Called from cg_auto_convert.
// Destination is always a temp register.

static JILError cg_cvl_var(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JCLVar* pTempVar = NULL;
	if( dst->miMode != kModeRegister )
		FATALERROREXIT("cg_cvl_var", "Var mode not implemented");
	// if using 64bit float, emit truncation warning
	if( sizeof(JILFloat) == sizeof(double) )
		EmitWarning(_this, NULL, JCL_WARN_Imp_Conv_Float_Int);
	switch( src->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_cvl);
			cg_opcode(_this, src->miIndex);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		case kModeMember:
		case kModeArray:
			err = MakeTempVar(_this, &pTempVar, src);
			if( err )
				goto exit;
			cg_move_var(_this, src, pTempVar);
			cg_opcode(_this, op_cvl);
			cg_opcode(_this, pTempVar->miIndex);
			cg_opcode(_this, dst->miIndex);
			FreeTempVar(_this, &pTempVar);
			break;
		default:
			FATALERROREXIT("cg_cvl_var", "Var mode not implemented");
			break;
	}
	// conversion creates unique value
	dst->miUnique = JILTrue;

exit:
	FreeTempVar(_this, &pTempVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_dcvt_var
//------------------------------------------------------------------------------
// Create code for dynamic runtime conversion. Called from cg_auto_convert.
// Destination is always a temp register.

static JILError cg_dcvt_var(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JCLVar* pTempVar = NULL;
	if( dst->miMode != kModeRegister )
		FATALERROREXIT("cg_dcvt_var", "Var mode not implemented");
	switch( src->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_dcvt);
			cg_opcode(_this, dst->miType);
			cg_opcode(_this, src->miIndex);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		case kModeMember:
		case kModeArray:
			err = MakeTempVar(_this, &pTempVar, src);
			if( err )
				goto exit;
			cg_move_var(_this, src, pTempVar);
			cg_opcode(_this, op_dcvt);
			cg_opcode(_this, dst->miType);
			cg_opcode(_this, pTempVar->miIndex);
			cg_opcode(_this, dst->miIndex);
			FreeTempVar(_this, &pTempVar);
			break;
		default:
			FATALERROREXIT("cg_dcvt_var", "Var mode not implemented");
			break;
	}
	// conversion creates unique value
	dst->miUnique = JILTrue;

exit:
	FreeTempVar(_this, &pTempVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_and_or_xor_var
//------------------------------------------------------------------------------
// Helper function that generates code for the bitwise & | ^ << >> operators.

static JILError cg_and_or_xor_var(JCLState* _this, JCLVar* src, JCLVar* dst, JILLong op)
{
	JILLong op1, op2, op3, op4, op5;
	JILError err = JCL_No_Error;
	JCLVar* newsrc;
	JCLVar* tmpvar = NULL;
	op1 = op2 = op3 = op4 = op5 = 0;

	// check type and auto-convert
	err = cg_auto_convert(_this, src, dst, &newsrc, &tmpvar);
	if( err )
		goto exit;
	// check access rules
	if( IsAssignOperator(op) )
	{
		err = cg_dst_modify_rule(_this, dst);
		if( err )
			goto exit;
	}
	else
	{
		err = cg_src_dst_rule(_this, newsrc, dst);
		if( err )
			goto exit;
	}
	if( (dst->miType != type_int && dst->miType != type_var) ||
		(newsrc->miType != type_int &&	newsrc->miType != type_var) )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	// check token
	switch( op )
	{
		case tk_band:
		case tk_band_assign:
			op1 = op_and_rr;
			op2 = op_and_rs;
			op3 = op_and_sr;
			op4 = op_and_rd;
			op5 = op_and_dr;
			break;
		case tk_bor:
		case tk_bor_assign:
			op1 = op_or_rr;
			op2 = op_or_rs;
			op3 = op_or_sr;
			op4 = op_or_rd;
			op5 = op_or_dr;
			break;
		case tk_xor:
		case tk_xor_assign:
			op1 = op_xor_rr;
			op2 = op_xor_rs;
			op3 = op_xor_sr;
			op4 = op_xor_rd;
			op5 = op_xor_dr;
			break;
		case tk_lshift:
		case tk_lshift_assign:
			op1 = op_lsl_rr;
			op2 = op_lsl_rs;
			op3 = op_lsl_sr;
			op4 = op_lsl_rd;
			op5 = op_lsl_dr;
			break;
		case tk_rshift:
		case tk_rshift_assign:
			op1 = op_lsr_rr;
			op2 = op_lsr_rs;
			op3 = op_lsr_sr;
			op4 = op_lsr_rd;
			op5 = op_lsr_dr;
			break;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( newsrc->miMode )
	{
		case kModeRegister:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					op2 = op1;
					// fall thru
				case kModeStack:
					cg_opcode(_this, op2);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				case kModeMember:
					cg_opcode(_this, op4);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					cg_opcode(_this, dst->miMember);
					break;
				default:
					FATALERROREXIT("cg_and_or_xor_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeStack:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op3);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_and_or_xor_var", "Var mode not implemented");
					break;
			}
			break;
		}
		case kModeMember:
		{
			switch( dst->miMode )
			{
				case kModeRegister:
					cg_opcode(_this, op5);
					cg_opcode(_this, newsrc->miIndex);
					cg_opcode(_this, newsrc->miMember);
					cg_opcode(_this, dst->miIndex);
					break;
				default:
					FATALERROREXIT("cg_and_or_xor_var", "Var mode not implemented");
					break;
			}
			break;
		}
		default:
			FATALERROREXIT("cg_and_or_xor_var", "Var mode not implemented");
			break;
	}

exit:
	FreeTempVar(_this, &tmpvar);
	return err;
}

//------------------------------------------------------------------------------
// cg_bnot_var
//------------------------------------------------------------------------------
// Helper function that generates code for the bitwise "~" operator.

static JILError cg_bnot_var(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	err = cg_copy_modify_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_int && dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	// handle temp var copying
	err = cg_modify_temp(_this, dst);
	if( err )
		goto exit;
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_not_r);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_not_s);
			cg_opcode(_this, dst->miIndex);
			break;
		default:
			FATALERROREXIT("cg_bnot_var", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_rtchk
//------------------------------------------------------------------------------
// Place a rtchk instruction.
// There is a problem if the destination class is an interface, cos interfaces
// only exists "in thought" of the compiler, there is no physical equivalent to
// an interface in the virtual machine runtime, and runtime-type-information is
// recorded for interfaces (that would be useless anyway). So the correct
// implementation here would be to check the type of the value in 'src' against
// the TypeID's of ALL POSSIBLE CLASSES derived from that interface, but only
// throw an exception, if the value is NEITHER ONE of those TypeID's.
// However, for that we would need a much more powerful rtchk implementation.
// So for now, no rtchk instruction is placed if the destination type is an
// interface.

static JILError cg_rtchk(JCLState* _this, JCLVar* src, JILLong dstType)
{
	JILError err = JCL_No_Error;
	JCLClass* pClass;

	if( dstType == type_var )
		FATALERROREXIT("cg_rtchk", "dstType is 'var'");
	pClass = GetClass(_this, dstType);
	if( !pClass )
		FATALERROREXIT("cg_rtchk", "dstType is not valid");
	// check if destination class is an interface
	if( pClass->miFamily == tf_interface )
		goto exit;	// silently leave...
	// generate instruction
	switch( src->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_rtchk_r);
			cg_opcode(_this, dstType);
			cg_opcode(_this, src->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_rtchk_s);
			cg_opcode(_this, dstType);
			cg_opcode(_this, src->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op_rtchk_d);
			cg_opcode(_this, dstType);
			cg_opcode(_this, src->miIndex);
			cg_opcode(_this, src->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op_rtchk_x);
			cg_opcode(_this, dstType);
			cg_opcode(_this, src->miIndex);
			cg_opcode(_this, src->mipArrIdx->miIndex);
			break;
		default:
			FATALERROREXIT("cg_rtchk", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_cast_if_typeless
//------------------------------------------------------------------------------
// This changes the destination var's type to that of the source var, if the
// destination's type is typeless and the source's type is not typeless.
// Additionally it writes a rtchk instruction, if this option is enabled.

static JILError cg_cast_if_typeless(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;

	if( (dst->miType == type_var && src->miType != type_var) )
	{
		EmitWarning(_this, NULL, JCL_WARN_Imp_Conv_From_Var);
		dst->miType = src->miType;
		dst->miElemType = src->miElemType;
		if( GetOptions(_this)->miUseRTCHK )
		{
			err = cg_rtchk(_this, dst, src->miType);
			if( err )
				goto exit;
		}
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_newctx
//------------------------------------------------------------------------------
// Place instructions that allocate a context and move it into the destination
// var.

static JILError cg_newctx(JCLState* _this, JCLVar* dst, JILLong type, JILLong funcIndex, JILLong numArgs)
{
	JILError err = JCL_No_Error;
	JCLVar* pTempVar = NULL;
	err = cg_dst_assign_rule(_this, dst);
	if( err )
		goto exit;
	// check type
	if( dst->miType != type_var )
	{
		if( dst->miType != type || TypeFamily(_this, dst->miType) != tf_thread )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
	}
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_newctx);
			cg_opcode(_this, type);
			cg_opcode(_this, funcIndex);
			cg_opcode(_this, numArgs);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
		case kModeMember:
		case kModeArray:
			err = MakeTempVar(_this, &pTempVar, dst);
			if( err )
				goto exit;
			cg_opcode(_this, op_newctx);
			cg_opcode(_this, type);
			cg_opcode(_this, funcIndex);
			cg_opcode(_this, numArgs);
			cg_opcode(_this, pTempVar->miIndex);
			err = cg_move_var(_this, pTempVar, dst);
			if( err )
				goto exit;
			break;
		default:
			FATALERROREXIT("cg_newctx", "Var mode not implemented");
			break;
	}
	dst->miUnique = JILTrue;

exit:
	FreeTempVar(_this, &pTempVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_resume
//------------------------------------------------------------------------------
// Place a resume instruction.

static JILError cg_resume(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// check type
	if( TypeFamily(_this, dst->miType) != tf_thread && dst->miType != type_var )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_resume_r);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_resume_s);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op_resume_d);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op_resume_x);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->mipArrIdx->miIndex);
			break;
		default:
			FATALERROREXIT("cg_resume", "Var mode not implemented");
			break;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_init_var
//------------------------------------------------------------------------------
// Initialize a global or local variable with a default value. This is called
// when compiling a variable declaration statement that contains no immediate
// explicit initialization value, for example:
//		long n;		// initialize with "0"
//		float f;	// initialize with "0.0"
//		string s;	// initialize with "" (empty string object)
//		array a;	// initialize with {} (empty array object)
//		CFoo foo;	// initialize with default constructed CFoo object
//      var v;      // initialize with null
// If the class to be constructed has no default constructor, this function
// produces a compile-time error.
// If the variable to be initialized is typeless (type var), a compile-time
// error is produced.
// Note that it is only possible to default initialize regular variables this
// way, reference variables must be initialized explicitly by immediately
// assigning a right-hand value to the variable.

static JILError cg_init_var(JCLState* _this, JCLVar* pLVar)
{
	JILError err = JCL_No_Error;
	TypeInfo outType;
	JCLVar* pTempVar = NULL;

	if( !pLVar )
		FATALERROREXIT("cg_init_var", "pLVar is NULL");
	if( pLVar->miConst )
	{
		err = JCL_ERR_Const_Not_Initialized;
		goto exit;
	}
	JCLClrTypeInfo(&outType);
	switch( pLVar->miType )
	{
		case type_var:
			err = cg_load_null(_this, pLVar, &outType);
			goto exit;
		case type_int:
			err = cg_load_literal(_this, type_int, "0", pLVar, JILFalse, &outType);
			if( err )
				goto exit;
			break;
		case type_float:
			err = cg_load_literal(_this, type_float, "0.0", pLVar, JILFalse, &outType);
			if( err )
				goto exit;
			break;
		case type_string:
			err = cg_load_literal(_this, type_string, "", pLVar, JILFalse, &outType);
			if( err )
				goto exit;
			break;
		case type_array:
		{
			err = cg_alloca_var(_this, pLVar->miElemType, 0, pLVar);
			if( err )
				goto exit;
			break;
		}
		default:
		{
			// default construct a class
			JCLFunc* pFunc;
			JCLClass* pClass;
			pClass = GetClass(_this, pLVar->miType);
			ERROR_IF(pClass->miFamily != tf_class, JCL_ERR_Type_Not_Class, NULL, goto exit);
			ERROR_IF(!pClass->miHasBody, JCL_ERR_Class_Only_Forwarded, NULL, goto exit);
			ERROR_IF(IsModifierNativeBinding(pClass), JCL_ERR_Native_Modifier_Illegal, NULL, goto exit);
			err = FindDefaultCtor(_this, pLVar, &pFunc);
			if( err )
				goto exit;
			ERROR_IF(pFunc->miExplicit,JCL_ERR_Ctor_Is_Explicit, pClass->mipName, goto exit);
			// creating temp var here produces "cheaper" code
			err = MakeTempVar(_this, &pTempVar, pLVar);
			if( err )
				goto exit;
			// generate allocation code
			err = cg_alloc_var(_this, pLVar, pTempVar, pClass->miNative);
			if( err )
				goto exit;
			// generate constructor call
			cg_opcode(_this, op_push_r);	// save r0
			cg_opcode(_this, 0);
			SimStackReserve(_this, 1);
			if( pClass->miNative )
			{
				err = cg_change_context(_this, pTempVar);
				if( err )
					goto exit;
				cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);
			}
			else
			{
				err = cg_change_context(_this, pTempVar);
				if( err )
					goto exit;
				cg_call_static(_this, pFunc->miHandle);
			}
			cg_opcode(_this, op_pop_r);		// restore r0
			cg_opcode(_this, 0);
			SimStackPop(_this, 1);
			err = cg_move_var(_this, pTempVar, pLVar);
			if( err )
				goto exit;
			FreeTempVar(_this, &pTempVar);
			break;
		}
	}
	// variable is initialized
	pLVar->miInited = JILTrue;

exit:
	FreeTempVar(_this, &pTempVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_accessor_call
//------------------------------------------------------------------------------
// Helper function to p_accessor_call, generates the raw byte code for the call
// to the set or get accessor method.

static JILError cg_accessor_call(JCLState* _this, JCLClass* pClass, JCLFunc* pFunc, JCLVar* pObj, const JCLString* pName)
{
	JILError err = JCL_No_Error;

	if( !pFunc || !pFunc->miMethod || !pFunc->miAccessor || !pObj || !pClass )
		FATALERROREXIT( "cg_accessor_call", "One or more function arguments are invalid");
	// check if context change is allowed (calling member of another class)
	if( pFunc->miMethod && !pFunc->miCtor && _this->miClass != pClass->miType && !pObj )
		ERROR(JCL_ERR_Cannot_Call_Foreign_Method, pName, goto exit);
	// check if calling method from static function
	if( !CurrentFunc(_this)->miMethod && pFunc->miMethod && !pObj )
		ERROR(JCL_ERR_Calling_Method_From_Static, pName, goto exit);
	// check if calling method and object is not initialized
	if( pFunc->miMethod && pObj && !IsSrcInited(pObj) )
		ERROR(JCL_ERR_Var_Not_Initialized, pName, goto exit);
	// check if calling set-accessor and object is const
	if( pFunc->mipArgs->count && IsDstConst(pObj) )
		ERROR(JCL_ERR_LValue_Is_Const, pName, goto exit);
	// generate code for function call
	if( pClass->miNative )
	{
		err = cg_change_context(_this, pObj);
		ERROR_IF(err, err, pName, goto exit);
		cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);
	}
	else
	{
		err = cg_change_context(_this, pObj);
		ERROR_IF(err, err, NULL, goto exit);
		if( pClass->miFamily == tf_interface )
			cg_call_member(_this, pClass->miType, pFunc->miFuncIdx);	// use call via v-table
		else
			cg_call_static(_this, pFunc->miHandle);		// use cheaper call
	}

exit:
	// pop arguments from stack
	if( pFunc->mipArgs->count )
	{
		cg_pop_multi(_this, pFunc->mipArgs->count);
		SimStackPop(_this, pFunc->mipArgs->count );
	}
	return err;
}

//------------------------------------------------------------------------------
// cg_convert_to_type
//------------------------------------------------------------------------------
// Generates code to convert a var to a specific type. ATTENTION: This function
// changes the typeID of the given var.

static JILError cg_convert_to_type(JCLState* _this, JCLVar* pSrcVar, JILLong destType)
{
	JILError err = JCL_No_Error;
	JCLVar* pDstVar = NULL;
	JCLVar* pSrcOut = NULL;
	JCLVar* pTmpOut = NULL;

	if( pSrcVar->miType == type_var && destType != type_var )
	{
		EmitWarning(_this, NULL, JCL_WARN_Imp_Conv_From_Var);
		pSrcVar->miType = destType;
		if( GetOptions(_this)->miUseRTCHK )
		{
			err = cg_rtchk(_this, pSrcVar, destType);
			if( err )
				goto exit;
		}
	}
	else if( pSrcVar->miType != destType )
	{
		DuplicateVar(&pDstVar, pSrcVar);
		pDstVar->miType = destType;
		err = cg_auto_convert(_this, pSrcVar, pDstVar, &pSrcOut, &pTmpOut);
		if( err )
			goto exit;
		if( pSrcOut != pSrcVar )
		{
			pSrcVar->miType = destType;
			err = cg_move_var(_this, pSrcOut, pSrcVar);
			if( err )
				goto exit;
			pSrcVar->miUnique = JILTrue;
		}
		FreeTempVar(_this, &pTmpOut);
	}

exit:
	FreeDuplicate(&pDstVar);
	return err;
}

//------------------------------------------------------------------------------
// cg_convert_compare
//------------------------------------------------------------------------------
// Converts the SOURCE var to the DEST var's type, if the DEST var's type can be
// compared by the VM. Comparable types are int, float and string. If the DEST
// var's type is not comparable, defaults to int.
// ATTENTION: This function changes the typeID of the given SOURCE var.

static JILError cg_convert_compare(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JILLong typeToUse;

	if( !IsComparableType(src->miType) )
	{
		if( IsComparableType(dst->miType) )
		{
			typeToUse = dst->miType;
		}
		else
		{
			typeToUse = type_int;
		}
		err = cg_convert_to_type(_this, src, typeToUse);
		if( err )
			goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_convert_calc
//------------------------------------------------------------------------------
// Converts the SOURCE var to the DEST var's type, if the DEST var's type can be
// used in arithmetic calculations by the VM. Calculatable types are int, float
// string and array. If the DEST var's type is not calculatable, defaults
// to int.
// ATTENTION: This function changes the typeID of the given SOURCE var.

static JILError cg_convert_calc(JCLState* _this, JCLVar* src, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	JILLong typeToUse;

	if( !IsCalculatableType(src->miType) )
	{
		if( IsCalculatableType(dst->miType) )
		{
			typeToUse = dst->miType;
		}
		else
		{
			typeToUse = type_int;
		}
		err = cg_convert_to_type(_this, src, typeToUse);
		if( err )
			goto exit;
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// cg_new_delegate
//------------------------------------------------------------------------------
// Generate code for allocating a new delegate for a global or member function.
// If the function is a member function, 'pObj' must contain the variable of the
// class instance, and 'funcIdx' is the method index of the function. If the
// function is global, 'pObj' must be NULL and 'funcIdx' is the function index
// (function handle of the runtime) of the function.

static JILError cg_new_delegate(JCLState* _this, JILLong funcIdx, JCLVar* pObj, JCLVar* pDst)
{
	JILError err = JCL_No_Error;
	JCLVar* pTmpObj = NULL;
	JCLVar* pTmpDst = NULL;
	JCLVar* pNewDst = pDst;

	err = cg_dst_assign_rule(_this, pDst);
	if( err )
		goto exit;
	if( TypeFamily(_this, pDst->miType) != tf_delegate )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	if( pObj && pObj->miMode != kModeRegister )
	{
		if( TypeFamily(_this, pObj->miType) != tf_class )
		{
			err = JCL_ERR_Incompatible_Type;
			goto exit;
		}
		err = MakeTempVar(_this, &pTmpObj, pObj);
		if( err )
			goto exit;
		err = cg_move_var(_this, pObj, pTmpObj);
		if( err )
			goto exit;
		pObj = pTmpObj;
	}
	if( pDst->miMode != kModeRegister )
	{
		err = MakeTempVar(_this, &pTmpDst, pDst);
		if( err )
			goto exit;
		pNewDst = pTmpDst;
	}
	if( pObj )
	{
		cg_opcode(_this, op_newdgm);
		cg_opcode(_this, pNewDst->miType);
		cg_opcode(_this, funcIdx);
		cg_opcode(_this, pObj->miIndex);
		cg_opcode(_this, pNewDst->miIndex);
	}
	else
	{
		cg_opcode(_this, op_newdg);
		cg_opcode(_this, pNewDst->miType);
		cg_opcode(_this, funcIdx);
		cg_opcode(_this, pNewDst->miIndex);
	}
	if( pNewDst != pDst )
	{
		err = cg_move_var(_this, pNewDst, pDst);
		if( err )
			goto exit;
	}
	pDst->miUnique = JILTrue;

exit:
	FreeTempVar(_this, &pTmpObj);
	FreeTempVar(_this, &pTmpDst);
	return err;
}

//------------------------------------------------------------------------------
// cg_call_delegate
//------------------------------------------------------------------------------
// Place a call delegate instruction.

static JILError cg_call_delegate(JCLState* _this, JCLVar* dst)
{
	JILError err = JCL_No_Error;
	// check type
	if( TypeFamily(_this, dst->miType) != tf_delegate )
	{
		err = JCL_ERR_Incompatible_Type;
		goto exit;
	}
	switch( dst->miMode )
	{
		case kModeRegister:
			cg_opcode(_this, op_calldg_r);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeStack:
			cg_opcode(_this, op_calldg_s);
			cg_opcode(_this, dst->miIndex);
			break;
		case kModeMember:
			cg_opcode(_this, op_calldg_d);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->miMember);
			break;
		case kModeArray:
			cg_opcode(_this, op_calldg_x);
			cg_opcode(_this, dst->miIndex);
			cg_opcode(_this, dst->mipArrIdx->miIndex);
			break;
		default:
			FATALERROREXIT("cg_call_delegate", "Var mode not implemented");
			break;
	}

exit:
	return err;
}
