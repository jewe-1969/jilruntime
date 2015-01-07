//------------------------------------------------------------------------------
// File: JILException.h                                   (c) 2005-2010 jewe.org
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
/// @file jilexception.h
/// Defines exception and error codes used throughout the library.
///
/// This file defines all exception numbers and error codes returned by the
/// JIL virtual machine, the JewelScript compiler, and the native type
/// interface.
//------------------------------------------------------------------------------

#ifndef JILEXCEPTION_H
#define JILEXCEPTION_H

//------------------------------------------------------------------------------
// JIL_Num_Exception_Strings
//------------------------------------------------------------------------------
// Don't forget to increment this, if you add another entry into the
// JILExceptionStrings or table! (see jilexception.c)

enum
{ 
	JIL_Num_Exception_Strings =	51,
	JCL_Num_Compiler_Codes =	122
};

//------------------------------------------------------------------------------
// JIL exceptions
//------------------------------------------------------------------------------

enum
{
	JIL_No_Exception = 0,

	// virtual machine exceptions
	JIL_Begin_Exceptions = 1000,

	JIL_VM_Illegal_Instruction = JIL_Begin_Exceptions,	// 1000
	JIL_VM_Stack_Overflow,				// 1001
	JIL_VM_Null_Reference,				// 1002
	JIL_VM_Unsupported_Type,			// 1003
	JIL_VM_Type_Mismatch,				// 1004
	JIL_VM_Call_To_NonFunction,			// 1005
	JIL_VM_Invalid_Operand,				// 1006
	JIL_VM_Division_By_Zero,			// 1007
	JIL_VM_Software_Exception,			// 1008
	JIL_VM_Trace_Exception,				// 1009
	JIL_VM_Break_Exception,				// 1010
	JIL_VM_Unhandled_Exception,			// 1011
	JIL_VM_Allocation_Failed,			// 1012
	JIL_VM_Invalid_Code_Address,		// 1013
	JIL_VM_Return_To_Native,			// 1014
	JIL_VM_Object_Copy_Exception,		// 1015
	JIL_VM_Abort_Exception,				// 1016
	JIL_VM_Native_CPP_Exception,		// 1017

	// Library error codes
	JIL_Begin_Errors = 1100,

	JIL_ERR_Generic_Error = JIL_Begin_Errors,	// 1100
	JIL_ERR_Illegal_Argument,			// 1101
	JIL_ERR_Out_Of_Code,				// 1102
	JIL_ERR_Illegal_Type_Name,			// 1103
	JIL_ERR_Register_Type_Failed,		// 1104
	JIL_ERR_Undefined_Type,				// 1105
	JIL_ERR_Unsupported_Native_Call,	// 1106
	JIL_ERR_Invalid_Vector,				// 1107
	JIL_ERR_Invalid_Handle_Index,		// 1108
	JIL_ERR_Invalid_Handle_Type,		// 1109
	JIL_ERR_Invalid_Member_Index,		// 1110
	JIL_ERR_Invalid_Function_Index,		// 1111
	JIL_ERR_Invalid_Register,			// 1112
	JIL_ERR_Call_To_NonFunction,		// 1113
	JIL_ERR_Runtime_Locked,				// 1114
	JIL_ERR_Save_Chunk_Failed,			// 1115
	JIL_ERR_Load_Chunk_Failed,			// 1116
	JIL_ERR_No_Symbol_Table,			// 1117
	JIL_ERR_Symbol_Exists,				// 1118
	JIL_ERR_Symbol_Not_Found,			// 1119
	JIL_ERR_Incompatible_NTL,			// 1120
	JIL_ERR_Detected_Memory_Leaks,		// 1121
	JIL_ERR_Trace_Not_Supported,		// 1122
	JIL_ERR_Runtime_Blocked,			// 1123
	JIL_ERR_Code_Not_Initialized,		// 1124
	JIL_ERR_Initialize_Failed,			// 1125
	JIL_ERR_No_Compiler,				// 1126
	JIL_ERR_File_Open,					// 1127
	JIL_ERR_File_End,					// 1128
	JIL_ERR_File_Generic,				// 1129
	JIL_ERR_Mark_Handle_Error,			// 1130

	// end of exception declarations
	JIL_Unknown_Exception				// 1131
};

//------------------------------------------------------------------------------
// JCL Errors
//------------------------------------------------------------------------------

enum
{
	JCL_No_Error = 0,

	// Compiler errors
	JCL_Begin_Errors = 1200,

	JCL_ERR_End_Of_File = JCL_Begin_Errors,	// 1200
	JCL_ERR_Unexpected_Token,				// 1201
	JCL_ERR_Probe_Failed,					// 1202 RESERVED FOR PROBING. USER SHOULD NOT SEE THIS!
	JCL_ERR_Function_Already_Defined,		// 1203
	JCL_ERR_Typeless_Arg_Conflict,			// 1204
	JCL_ERR_Const_Arg_Conflict,				// 1205
	JCL_ERR_Identifier_Already_Defined,		// 1206
	JCL_ERR_Must_Return_Value,				// 1207
	JCL_ERR_Cannot_Return_Value,			// 1208
	JCL_ERR_Incompatible_Type,				// 1209
	JCL_ERR_Not_An_LValue,					// 1210
	JCL_ERR_Expression_Is_Const,			// 1211
	JCL_ERR_Var_Not_Initialized,			// 1212
	JCL_ERR_Undefined_Identifier,			// 1213
	JCL_ERR_Undefined_Function_Call,		// 1214
	JCL_ERR_No_Return_Value,				// 1215
	JCL_ERR_LValue_Is_Const,				// 1216
	JCL_ERR_No_Copy_Constructor,			// 1217
	JCL_ERR_No_Function_Body,				// 1218
	JCL_ERR_Constructor_Not_Void,			// 1219
	JCL_ERR_Method_Outside_Class,			// 1220
	JCL_ERR_Cannot_Reimplement_NTL,			// 1221
	JCL_ERR_Constructor_Is_Function,		// 1222
	JCL_ERR_Cannot_Call_Constructor,		// 1223
	JCL_ERR_Not_A_Constructor,				// 1224
	JCL_ERR_Cannot_Call_Foreign_Method,		// 1225
	JCL_ERR_Calling_Method_From_Static,		// 1226
	JCL_ERR_Import_Not_Supported,			// 1227
	JCL_ERR_Not_An_Object,					// 1228
	JCL_ERR_Illegal_NTL_Variable,			// 1229
	JCL_ERR_Incomplete_Arg_List,			// 1230
	JCL_ERR_Break_Without_Context,			// 1231
	JCL_ERR_Expression_Without_LValue,		// 1232
	JCL_ERR_Convertor_Is_Void,				// 1233
	JCL_ERR_Convertor_Is_Function,			// 1234
	JCL_ERR_Convertor_Has_Arguments,		// 1235
	JCL_ERR_Default_Not_At_End,				// 1236
	JCL_ERR_Case_Requires_Const_Expr,		// 1237
	JCL_ERR_Typeof_Var_Illegal,				// 1238
	JCL_ERR_Class_Only_Forwarded,			// 1239
	JCL_ERR_Invalid_Char_Literal,			// 1240
	JCL_ERR_Not_An_Array,					// 1241
	JCL_ERR_Missing_Semicolon,				// 1242
	JCL_ERR_No_Constructor_Defined,			// 1243
	JCL_ERR_Mixing_Class_And_Interface,		// 1244
	JCL_ERR_Interface_Not_Complete,			// 1245
	JCL_ERR_Method_Definition_Illegal,		// 1246
	JCL_ERR_Type_Not_Class,					// 1247
	JCL_ERR_Ref_Arg_Conflict,				// 1248 (maybe obsolete?)
	JCL_ERR_Arg_Type_Conflict,				// 1249
	JCL_ERR_Must_Init_All_Members,			// 1250
	JCL_ERR_Import_Not_Defined,				// 1251
	JCL_ERR_Function_Not_An_Accessor,		// 1252
	JCL_ERR_Member_Protected,				// 1253
	JCL_ERR_Function_Redefined,				// 1254
	JCL_ERR_Fatal_Error,					// 1255
	JCL_ERR_Global_In_Identifier,			// 1256
	JCL_ERR_Ambiguous_Function_Call,		// 1257
	JCL_ERR_Native_Code_Generator,			// 1258
	JCL_ERR_Return_In_Cofunction,			// 1259
	JCL_ERR_Yield_Outside_Cofunction,		// 1260
	JCL_ERR_Const_Thread_Error,				// 1261
	JCL_ERR_Throw_Not_Exception,			// 1262
	JCL_ERR_Weak_Without_Ref,				// 1263
	JCL_ERR_WRef_Arg_Conflict,				// 1264
	JCL_ERR_Syntax_Error,					// 1265
	JCL_ERR_Ctor_Is_Explicit,				// 1266
	JCL_ERR_No_Default_Ctor,				// 1267
	JCL_ERR_Const_Not_Initialized,			// 1268
	JCL_ERR_Interface_Native_Only,			// 1269
	JCL_ERR_Explicit_With_Method,			// 1270
	JCL_ERR_Conv_Requires_Cast,				// 1271
	JCL_ERR_Error_In_Func_Arg,				// 1272
	JCL_ERR_Array_Array,					// 1273
	JCL_ERR_Type_Not_Interface,				// 1274
	JCL_ERR_Function_Ref_Ambiguous,			// 1275
	JCL_ERR_Function_Ref_Illegal,			// 1276
	JCL_ERR_No_Suitable_Delegate,			// 1277
	JCL_ERR_Invalid_Variable_Call,			// 1278
	JCL_ERR_Hybrid_Expected,				// 1279
	JCL_ERR_Cofunction_In_NTL,				// 1280
	JCL_ERR_Unnamed_Delegate_Argument,		// 1281
	JCL_ERR_Anon_Func_In_Init_Block,		// 1282
	JCL_ERR_Native_With_Hybrid,				// 1283
	JCL_ERR_Native_Modifier_Illegal,		// 1284
	JCL_ERR_Class_Modifier_Conflict,		// 1285
	JCL_ERR_Character_Value_Too_Large,		// 1286
	JCL_ERR_Goto_Without_Context,			// 1287
	JCL_ERR_Function_In_Namespace,			// 1288
	JCL_ERR_Feature_Not_Available,			// 1289
	JCL_ERR_Cannot_Extend_Native_Class,		// 1290
	JCL_ERR_Function_No_Override,			// 1291
	JCL_ERR_Variable_Read_Only,				// 1292
	JCL_ERR_No_Access,						// 1293
	JCL_ERR_Class_Not_Relocatable,			// 1294

	// Compiler warnings
	JCL_Begin_Warnings = 1500,

	JCL_WARN_Imp_Conv_From_Var = JCL_Begin_Warnings,	// 1500
	JCL_WARN_Cast_To_Var,					// 1501
	JCL_WARN_Auto_Convert_To_Ref,			// 1502
	JCL_WARN_Unknown_Option,				// 1503
	JCL_WARN_Invalid_Option_Value,			// 1504
	JCL_WARN_Assign_WRef_Temp_Value,		// 1505
	JCL_WARN_Return_WRef_Local,				// 1506
	JCL_WARN_Member_Hides_Global,			// 1507
	JCL_WARN_Local_Hides_Global,			// 1508
	JCL_WARN_Local_Hides_Member,			// 1509
	JCL_WARN_Unreachable_Code,				// 1510
	JCL_WARN_Imp_Conv_Float_Int,			// 1511
	JCL_WARN_Imp_Conv_Int_Float,			// 1512
	JCL_WARN_Auto_Copy_Const,				// 1513
	JCL_WARN_Taking_Ref_From_Wref,			// 1514
	JCL_WARN_Operator_No_Effect,			// 1515
	JCL_WARN_Null_Assign_No_Ref,			// 1516
	JCL_WARN_Function_Auto_Complete,		// 1517
	JCL_WARN_Dynamic_Conversion,			// 1518
	JCL_WARN_Ambiguous_Type_Name,			// 1519
	JCL_WARN_Taking_Wref_From_Wref,			// 1520
	JCL_WARN_Wref_Variable,					// 1521
	JCL_WARN_Imp_Conv_Var_Array,			// 1522
	JCL_WARN_Interface_In_Inherit,			// 1523
	JCL_WARN_Unsafe_This_Operation,			// 1524

	JCL_Unknown_Error_Code
};

#endif	// #ifndef JILEXCEPTION_H
