//------------------------------------------------------------------------------
// File: JCLState.h                                         (c) 2004 jewe.org
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
/// @file jclstate.h
/// The main object for the JewelScript compiler. This is created when calling
/// JCLInitialize() and must be passed into all public API functions.
//------------------------------------------------------------------------------

#ifndef JCLSTATE_H
#define JCLSTATE_H

#include "jiltypes.h"
#include "jcltools.h"
#include "jclpair.h"

//------------------------------------------------------------------------------
// enum constants
//------------------------------------------------------------------------------

/// Compile pass identifier
enum
{
	kPassPrecompile	= 0,		///< pass 1, compile only declarations
	kPassCompile	= 1			///< pass 2, compile code
};

//------------------------------------------------------------------------------
// declare array template for JCLFile and JCLOption
//------------------------------------------------------------------------------

DECL_ARRAY( JCLOption )
DECL_ARRAY( JCLFile )

FORWARD_CLASS( JCLClause )

//------------------------------------------------------------------------------
// class JCLState
//------------------------------------------------------------------------------
/// This is the main compiler object.

DECL_CLASS( JCLState )

	JILState*			mipMachine;					///< The JIL virtual machine instance to program
	JCLFile*			mipFile;					///< Points to current file context
	JILLong				miClass;					///< Currently parsed class index
	JILLong				miArgClass;					///< For parsing function arguments: Class index of class being called, else 0
	JILLong				miOutputClass;				///< Class to which bytecode is output (usually same as miClass)
	JILLong				miFunc;						///< Currently parsed function index
	JILLong				miOutputFunc;				///< Function to which bytecode is output (usually same as miFunc)
	JILLong				miPass;						///< Current compilation pass
	Array_JCLClass*		mipClasses;					///< Array of compiled classes (or interfaces and other types)
	JCLVar**			mipStack;					///< Simulated data stack when compiling function body
	JILLong				miStackPos;					///< Current simulated data stack pointer
	JCLVar*				mipRegs[kNumRegisters];		///< Simulated register contents when compiling function body
	JILLong				miRegUsage[kNumRegisters];	///< Count how often regs were allocated in function
	JILLong				miNumRegsToSave;			///< No. of regs function must save / restore
	JILLong				miNumVarRegisters;			///< No. of regs currently used for local variables
	JILLong				miBlockLevel;				///< Nested block level counter
	JILLong				miBreakUnrollSP;			///< Saved stack pointer for unrolling the stack in case of a break or continue statement
	JCLClause*			mipClause;					///< Current clause data or NULL
	Array_JILLong*		mipBreakFixup;				///< List of code-offsets to patch in case of a break
	Array_JILLong*		mipContFixup;				///< List of code-offsets to patch in case of a continue
	Array_JCLFile*		mipImportStack;				///< Stack of imported files
	JILLong				miLastError;				///< Last reported error or warning
	JILLong				miFlushedError;				///< Last flushed error or warning
	JILBool				miFatalState;				///< Compiler is in fatal error state
	JILBool				miIntroFinished;			///< cg_finish_intro() has generated 'RET' instruction
	Array_JCLString*	mipErrors;					///< Storage of emitted errors and warnings
	JILLong				miNumErrors;				///< Total number of errors
	JILLong				miNumWarnings;				///< Total number of warnings
	JILLong				miNumCompiles;				///< Total number of files compiled
	JILFloat			miTimestamp;				///< Compile start time stamp
	Array_JCLOption*	mipOptionStack;				///< Stack of options, first element in array is global options
	JCLCollection*		mipImportPaths;				///< Collection of pathes to import directories
	JILLong				miOptSavedInstr;			///< Optimization: Number of saved instructions
	JILLong				miOptSizeBefore;			///< Optimization: Total code size before optimization (bytes)
	JILLong				miOptSizeAfter;				///< Optimization: Total code size after optimization (bytes)
	JCLFatalErrorHandler miFatalErrorHandler;		///< Fatal Error callback

END_CLASS( JCLState )

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

JILError	p_compile			(JCLState*, JILLong);
JILLong		FindClass			(JCLState*, const JCLString*, JCLClass**);
JILLong		FindFunction		(JCLState*, JILLong, const JCLString*, JILLong, JCLFunc**);
JILLong		NumClasses			(JCLState*);
JILLong		NumFuncs			(JCLState*, JILLong);
JCLClass*	GetClass			(JCLState*, JILLong);
JILBool		ClassDefined		(JCLState*, JILLong);
JCLFunc*	GetFunc				(JCLState*, JILLong, JILLong);
JILError	cg_begin_intro		(JCLState*);
JILError	cg_finish_intro		(JCLState*);
JILError	cg_resume_intro		(JCLState*);
JILError	EmitWarning			(JCLState*, const JCLString*, JILError);
JILError	EmitError			(JCLState*, const JCLString*, JILError);
void		FatalError			(JCLState*, const JILChar*, JILLong, const JILChar*, const JILChar*);
JCLOption*	GetOptions			(JCLState*);
JCLOption*	GetGlobalOptions	(JCLState*);
void		JCLVerbosePrint		(JCLState*, const JILChar*, ...);
JILError	FlushErrorsAndWarnings	(JCLState* _this);
JILError	JCLCreateType		(JCLState*, const JILChar*, JILLong, JILTypeFamily, JILBool, JILLong*);
JILBool		IsMethodInherited	(JCLState*, JILLong, JILLong);
JILLong		TypeFamily			(JCLState*, JILLong);
void		JCLGetAbsolutePath	(JCLState* _this, JCLString* pOut, const JCLString* pIn);
JILError	p_import_class		(JCLState* _this, JCLString* pClassName);

#endif	// #ifndef JCLSTATE_H
