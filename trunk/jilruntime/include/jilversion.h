//------------------------------------------------------------------------------
// File: JILVersion.h                                     (c) 2005-2015 jewe.org
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
/// @file jilversion.h
/// Central include file for defining the version information of the runtime,
///	the compiler, the native type interface, and the overall version number of
///	the JILRuntime library.
///
/// In general, version numbers are composed out of four numbers in byte range.
/// For example "0.6.1.31" stands for product version 0.6, code revision 1.31.
/// You can use NTLRevisionToLong() to convert a string in this format into an
/// integer version number.
//------------------------------------------------------------------------------

#ifndef JILVERSION_H
#define JILVERSION_H

//------------------------------------------------------------------------------
// JIL_PRODUCT_VERSION
//------------------------------------------------------------------------------
/// Defines the <b>product version number</b> of the library.

#define	JIL_PRODUCT_VERSION			"1.3."	// keep the trailing dot!

//------------------------------------------------------------------------------
// JIL_LIBRARY_VERSION
//------------------------------------------------------------------------------
/// This is the version number of the whole library. This is the version number
/// any documentation or distributions would refer to. If any of the three
/// version numbers below is increased due to a change, this version should be
/// increased as well, in order to reflect this change.

#define JIL_LIBRARY_VERSION			JIL_PRODUCT_VERSION "4.11"

//------------------------------------------------------------------------------
// JIL_COMPILER_VERSION
//------------------------------------------------------------------------------
/// This is the version number of the JewelScript compiler.

#define JIL_COMPILER_VERSION		JIL_PRODUCT_VERSION "4.5"

//------------------------------------------------------------------------------
// JIL_MACHINE_VERSION
//------------------------------------------------------------------------------
/// This is the version number of the virtual machine.

#define JIL_MACHINE_VERSION			JIL_PRODUCT_VERSION "4.0"

//------------------------------------------------------------------------------
// JIL_TYPE_INTERFACE_VERSION
//------------------------------------------------------------------------------
/// This is the version number of the native type interface. In your native type,
/// upon receiving the NTL_GetInterfaceVersion message, convert this string into
/// an integer value and return it. @see NTLRevisionToLong ()

#define JIL_TYPE_INTERFACE_VERSION	JIL_PRODUCT_VERSION "4.1"

#endif // JILVERSION_H
