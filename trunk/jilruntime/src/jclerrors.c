//------------------------------------------------------------------------------
// File: JILErrors.c                                           (c) 2005 jewe.org
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
/// @file jclerrors.c
/// Defines error and warning codes used by the JewelScript compiler.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JCLErrorStrings
//------------------------------------------------------------------------------
// This table defines all the error and warning strings.

const JCLErrorInfo JCLErrorStrings[JCL_Num_Compiler_Codes] =
{
	0,	JCL_No_Error,						NULL,

	// compiler error codes
	0,	JCL_ERR_End_Of_File,				"Unexpected End of file",
	0,	JCL_ERR_Unexpected_Token,			"Unexpected token",
	0,	JCL_ERR_General_Parser_Error,		"General Parser Error",
	0,	JCL_ERR_Function_Already_Defined,	"Function already defined",
	0,	JCL_ERR_Typeless_Arg_Conflict,		"Function redefined, typeless 'var' conflict",
	0,	JCL_ERR_Const_Arg_Conflict,			"Function redefined, inconsistent use of 'const' modifier",
	0,	JCL_ERR_Identifier_Already_Defined,	"Identifier already defined",
	0,	JCL_ERR_Must_Return_Value,			"Function must return a value",
	0,	JCL_ERR_Cannot_Return_Value,		"Function cannot return a value",
	0,	JCL_ERR_Incompatible_Type,			"Incompatible type and no convertor found",
	0,	JCL_ERR_Not_An_LValue,				"Expression is not a valid L-Value",
	0,	JCL_ERR_Expression_Is_Const,		"Expression is constant",
	0,	JCL_ERR_Var_Not_Initialized,		"Using variable without initialization",
	0,	JCL_ERR_Undefined_Identifier,		"Undefined identifier",
	0,	JCL_ERR_Undefined_Function_Call,	"Function undefined or no overload accepts the specified arguments",
	0,	JCL_ERR_No_Return_Value,			"Function does not return a value in all cases",
	0,	JCL_ERR_LValue_Is_Const,			"L-Value is a constant",
	0,	JCL_ERR_No_Copy_Constructor,		"Copy-constructor undefined, unable to copy object",
	0,	JCL_ERR_No_Function_Body,			"Function declared but not defined",
	0,	JCL_ERR_Constructor_Not_Void,		"Constructor cannot return a value",
	0,	JCL_ERR_Method_Outside_Class,		"Class member function needs to be declared in a class",
	0,	JCL_ERR_Cannot_Reimplement_NTL,		"Cannot reimplement function from native type",
	0,	JCL_ERR_Constructor_Is_Function,	"Constructors are methods, use 'method' keyword",
	0,	JCL_ERR_Cannot_Call_Constructor,	"Cannot call constructor",
	0,	JCL_ERR_Not_A_Constructor,			"Not a valid constructor",
	0,	JCL_ERR_Cannot_Call_Foreign_Method,	"Cannot call method without an object",
	0,	JCL_ERR_Calling_Method_From_Static, "Cannot call method from function",
	0,	JCL_ERR_Import_Not_Supported,		"Import not supported by native type",
	0,	JCL_ERR_Not_An_Object,				"Need an object left from '.'",
	0,	JCL_ERR_Illegal_NTL_Variable,		"Variable declaration not allowed for native type",
	0,	JCL_ERR_Incomplete_Arg_List,		"Incomplete argument list",
	0,	JCL_ERR_Break_Without_Context,		"Break / continue without for / while / switch",
	0,	JCL_ERR_Expression_Without_LValue,	"Expression without an L-Value",
	0,	JCL_ERR_Convertor_Is_Void,			"Convertor method requires return type",
	0,	JCL_ERR_Convertor_Is_Function,		"Convertors are methods, use 'method' keyword",
	0,	JCL_ERR_Convertor_Has_Arguments,	"Convertor cannot have arguments",
	0,	JCL_ERR_Default_Not_At_End,			"Default must appear last in switch statement",
	0,	JCL_ERR_Case_Requires_Const_Expr,	"Case requires constant expression",
	0,	JCL_ERR_Typeof_Var_Illegal,			"Operator typeof cannot evaluate typeless 'var'",
	0,	JCL_ERR_Class_Only_Forwarded,		"Class does not have a body",
	0,	JCL_ERR_Invalid_Char_Literal,		"Invalid character literal",
	0,	JCL_ERR_Not_An_Array,				"Identifier is not an array",
	0,	JCL_ERR_Missing_Semicolon,			"Missing ';' at end of statement",
	0,	JCL_ERR_No_Constructor_Defined,		"Class does not define any constructor",
	0,	JCL_ERR_Mixing_Class_And_Interface,	"Mixing usage of 'class' and 'interface' keywords",
	0,	JCL_ERR_Interface_Not_Complete,		"Inherited method not implemented",
	0,	JCL_ERR_Method_Definition_Illegal,	"Method definition not allowed for this type",
	0,	JCL_ERR_Type_Not_Class,				"Type is not a class",
	0,	JCL_ERR_Ref_Arg_Conflict,			"Function redefined, inconsistent modifiers",
	0,	JCL_ERR_Arg_Type_Conflict,			"Function redefined, inconsistent types",
	0,	JCL_ERR_Must_Init_All_Members,		"Constructor does not initialize member variable in all cases",
	0,	JCL_ERR_Import_Not_Defined,			"Unable to resolve specified import",
	0,	JCL_ERR_Function_Not_An_Accessor,	"Function signature not suitable for 'accessor'",
	0,	JCL_ERR_Member_Protected,			"No suitable accessor defined",
	0,	JCL_ERR_Function_Redefined,			"Function redefined, different function types",
	0,	JCL_ERR_Fatal_Error,				"Fatal error",
	0,	JCL_ERR_No_Type_Declaration,		"Not a type declaration",
	0,	JCL_ERR_Ambiguous_Function_Call,	"Ambiguous function call",
	0,	JCL_ERR_Native_Code_Generator,		"Error while running native binding code generator",
	0,	JCL_ERR_Return_In_Cofunction,		"Cannot use 'return' in cofunction, use 'yield'",
	0,	JCL_ERR_Yield_Outside_Cofunction,	"Cannot use 'yield' outside of cofunctions",
	0,	JCL_ERR_Const_Thread_Error,			"Type 'thread' cannot be const",
	0,	JCL_ERR_Throw_Not_Exception,		"Class does not implement interface 'exception'",
	0,	JCL_ERR_Weak_Without_Ref,			"Modifier 'weak' requires reference type",
	0,	JCL_ERR_WRef_Arg_Conflict,			"Function redefined, inconsistent use of 'weak' modifier",
	0,	JCL_ERR_Syntax_Error,				"Syntax error in statement",
	0,	JCL_ERR_Ctor_Is_Explicit,			"Class requires explicit initialization",
	0,	JCL_ERR_No_Default_Ctor,			"Class has no default constructor",
	0,	JCL_ERR_Const_Not_Initialized,		"Constant requires explicit initialization",
	0,	JCL_ERR_Interface_Native_Only,		"Interface is declared 'pure native' and cannot be implemented in script code",
	0,	JCL_ERR_Explicit_With_Method,		"Modifier 'explicit' can only be used with constructor and convertor methods",
	0,	JCL_ERR_Conv_Requires_Cast,			"Cannot convert, conversion requires type-cast operator",
	0,	JCL_ERR_Error_In_Func_Arg,			"Unable to compile one or more function arguments",
	0,	JCL_ERR_Array_Array,				"Element type of an array cannot be 'array'",
	0,	JCL_ERR_Type_Not_Interface,			"Type is not an interface",
	0,	JCL_ERR_Function_Ref_Ambiguous,		"Function reference is ambiguous",
	0,	JCL_ERR_Function_Ref_Illegal,		"Cannot take function reference from this type of function",
	0,	JCL_ERR_No_Suitable_Delegate,		"No delegate defined that matches function signature",
	0,	JCL_ERR_Invalid_Variable_Call,		"Variable is not a delegate or cofunction thread",
	0,	JCL_ERR_Hybrid_Expected,			"Expected 'hybrid' while defining class constructor",
	0,	JCL_ERR_Cofunction_In_NTL,			"Illegal 'cofunction' in native type",
	0,	JCL_ERR_Unnamed_Delegate_Argument,	"Delegate does not define a name for all arguments",
	0,	JCL_ERR_Anon_Func_In_Init_Block,	"Anonymous delegate not supported in initializer block",
	0,	JCL_ERR_Native_With_Hybrid,			"Hybrid inheritance not supported for native types",
	0,	JCL_ERR_Native_Modifier_Illegal,	"Illegal use of unimplemented native type",
	0,	JCL_ERR_Class_Modifier_Conflict,	"Type redefined; inconsistent use of modifiers",
	0,	JCL_ERR_Character_Value_Too_Large,	"Character value too large in string literal",
	0,	JCL_ERR_Goto_Without_Context,		"Goto without clause",

	// Compiler warnings
	2,	JCL_WARN_Imp_Conv_From_Var,			"Conversion from typeless to type",
	1,	JCL_WARN_Cast_To_Var,				"Dangerous cast to typeless, L-Value not typeless",
	2,	JCL_WARN_Auto_Convert_To_Ref,		"Implicit conversion to non-const reference",
	3,	JCL_WARN_Unknown_Option,			"Ignoring unknown compiler option",
	3,	JCL_WARN_Invalid_Option_Value,		"Ignoring invalid option value",
	1,	JCL_WARN_Assign_WRef_Temp_Value,	"Dangerous assignment of temporary value to weak reference",
	2,	JCL_WARN_Return_WRef_Local,			"Function returns weak reference to local variable",
	3,	JCL_WARN_Member_Hides_Global,		"Member variable hides global variable or function",
	3,	JCL_WARN_Local_Hides_Global,		"Local variable hides global variable or function",
	3,	JCL_WARN_Local_Hides_Member,		"Local variable hides member variable or function",
	3,	JCL_WARN_Unreachable_Code,			"Code is not reachable, function already returned",
	4,	JCL_WARN_Imp_Conv_Float_Int,		"Conversion from 'float' to 'int', value might get truncated",
	4,	JCL_WARN_Imp_Conv_Int_Float,		"Conversion from 'int' to 'float', value might get truncated",
	4,	JCL_WARN_Auto_Copy_Const,			"Assigning constant to reference, copying r-value",
	2,	JCL_WARN_Taking_Ref_From_Wref,		"Taking reference from weak reference, the reference to the value remains weak",
	3,	JCL_WARN_Operator_No_Effect,		"Operator has no effect",
	3,	JCL_WARN_Null_Assign_No_Ref,		"Assigning 'null' to variable that is not a reference",
	3,	JCL_WARN_Function_Auto_Complete,	"Function declared but not defined, generating default code",

	// leave this at the end of the table
	0,	JCL_Unknown_Error_Code,				"UNKNOWN COMPILER ERROR CODE"
};
