//------------------------------------------------------------------------------
// File: JILNativeType.h                                  (c) 2005-2010 jewe.org
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
/// @file jilnativetype.h
/// Definitions for native types implemented in C/C++.
///
/// This is the "simplified API" for writing native types implemented in C or
/// C++. The API allows only basic functionality, however, developers do not
/// need to worry about reference counting issues, thus this API is easier to
/// use. To implement more advanced native types, or to improve
/// performance, developers should use the advanced interface by including
/// jilnativetypeex.h instead of this file.
//------------------------------------------------------------------------------

#ifndef JILNATIVETYPE_H
#define JILNATIVETYPE_H

#include "jilapitypes.h"
#include "jilexception.h"

//------------------------------------------------------------------------------
// Messages for the native type library (NTL)
//------------------------------------------------------------------------------
/// Enumerates the messages sent to a native type library's callback function,
/// the <b>type proc</b>.

enum NTLMessage
{
	// runtime messages
	NTL_Register,				///< Sent when registering type to the runtime
	NTL_OnImport,				///< Sent when this native type is imported and before the package list or class declaration is compiled
	NTL_Initialize,				///< Sent when the VM is initialized and this type will be used
	NTL_NewObject,				///< Allocate a new object. This will be followed by the appropriate constructor call (NTL_CallMember) to initialize the object.
	NTL_MarkHandles,			///< Garbage collection: Call NTLMarkHandle() for all handles your native type owns
	NTL_CallStatic,				///< Call static function
	NTL_CallMember,				///< Call member function
	NTL_DestroyObject,			///< Destroy an object
	NTL_Terminate,				///< Sent when the VM is terminated
	NTL_Unregister,				///< Sent when unregistering type from the runtime

	// class information queries
	NTL_GetClassName,			///< Return this native type's class name
	NTL_GetBaseName,			///< (Optional) Return base class or interface name
	NTL_GetInterfaceVersion,	///< Return JILRevisionToLong(JIL_TYPE_INTERFACE_VERSION)
	NTL_GetAuthorVersion,		///< Return <b>your</b> version number of the type lib
	NTL_GetBuildTimeStamp,		///< Return a string containing your time stamp
	NTL_GetAuthorName,			///< Name of the author
	NTL_GetAuthorString,		///< Credits, copyright, etc, any length, any purpose allowed
	NTL_GetDeclString,			///< Return class declaration string
	NTL_GetPackageString		///< (Optional) Return comma seperated list of other classes to import before compiling the class declaration string
};

//------------------------------------------------------------------------------
// NTLRevisionToLong
//------------------------------------------------------------------------------
/// Use this to convert the revision string into an integer version number.

JILEXTERN JILLong		NTLRevisionToLong		(const JILChar* pRevision);

//------------------------------------------------------------------------------
// NTLIsValidTypeID
//------------------------------------------------------------------------------
/// Returns true if the given type-id is valid, otherwise false.

JILEXTERN JILBool		NTLIsValidTypeID		(JILState* pState, JILLong typeID);

//------------------------------------------------------------------------------
// NTLTypeNameToTypeID
//------------------------------------------------------------------------------
/// This returns the type-id number for a given type name. The type must be known
/// to the JewelScript compiler, which means, at least a forward declaration for
/// the type must have been made prior to calling this function.
/// <p>To programmatically "forward declare" a class, you can use the
/// JCLForwardClass() function. To programmatically import a class, you can use
/// the JCLImportClass() function.</p>
/// This function uses a hash table for very fast access to the type-id number
/// of a given type. Because this function is relatively fast, you can "lazily"
/// call it every time a type-id number is required.
/// The function returns 0 if the specified type is not found.

JILEXTERN JILLong		NTLTypeNameToTypeID		(JILState* pState, const JILChar* pClassName);

//------------------------------------------------------------------------------
// NTLGetTypeName
//------------------------------------------------------------------------------
/// Returns a character string containing the name of the type associated to the
/// specified type-id.

JILEXTERN const JILChar*	NTLGetTypeName		(JILState* pState, JILLong type);

//------------------------------------------------------------------------------
// NTLGetTypeFamily
//------------------------------------------------------------------------------
/// Returns an enum value that describes which type family the specified type is
/// a member of. Use this to determine if a type is a class, an interface, a
/// cofunction thread or a delegate, for example.
/// @see enum JILTypeFamily

JILEXTERN JILLong		NTLGetTypeFamily		(JILState* pState, JILLong type);

//------------------------------------------------------------------------------
// NTLGetArgTypeID
//------------------------------------------------------------------------------
/// Get the type-id of a function argument on the VM's stack. The first argument
/// to a native function has index 0, the second index 1, and so on.
/// The function returns 0 if an error occurs. This lets you determine the type
/// of an object passed to your function.

JILEXTERN JILLong		NTLGetArgTypeID			(JILState* pState, JILLong argNum);

//------------------------------------------------------------------------------
// NTLGetArgInt
//------------------------------------------------------------------------------
/// Get the int value of a function argument. If the specified argument does not
/// contain an int handle, returns 0.

JILEXTERN JILLong		NTLGetArgInt			(JILState* pState, JILLong argNum);

//------------------------------------------------------------------------------
// NTLGetArgFloat
//------------------------------------------------------------------------------
/// Get the float value of a function argument. If the specified argument does
/// not contain a float handle, returns 0.

JILEXTERN JILFloat		NTLGetArgFloat			(JILState* pState, JILLong argNum);

//------------------------------------------------------------------------------
// NTLGetArgString
//------------------------------------------------------------------------------
/// Get the string value of a function argument. If the specified argument does
/// not contain a string handle, returns a pointer to an empty string.

JILEXTERN const JILChar*	NTLGetArgString		(JILState* pState, JILLong argNum);

//------------------------------------------------------------------------------
// NTLGetArgObject
//------------------------------------------------------------------------------
/// Returns a pointer to a JIL object or native object. If the given argument
/// does not contain an object with the given type-id, or not an object at all,
/// returns NULL.

JILEXTERN JILUnknown*	NTLGetArgObject			(JILState* pState, JILLong argNum, JILLong type);

//------------------------------------------------------------------------------
// NTLReturnInt
//------------------------------------------------------------------------------
/// Call this if your native function returns an int value.
/// This function will create a new handle for the value and return it to the
/// virtual machine.

JILEXTERN void			NTLReturnInt			(JILState* pState, JILLong value);

//------------------------------------------------------------------------------
// NTLReturnFloat
//------------------------------------------------------------------------------
/// Call this if your native function returns a float value.
/// This function will create a new handle for the value and return it to the
/// virtual machine.

JILEXTERN void			NTLReturnFloat			(JILState* pState, JILFloat value);

//------------------------------------------------------------------------------
// NTLReturnString
//------------------------------------------------------------------------------
/// Call this if your native function returns a string value.
/// This function will create a new handle for the value and return it to the
/// virtual machine.
/// The string will be copied, so the pointer can be volatile.

JILEXTERN void			NTLReturnString			(JILState* pState, const JILChar* value);

//------------------------------------------------------------------------------
// NTLInstanceSetUser
//------------------------------------------------------------------------------
/// This allows you to attach any pointer to the instance of your native type.
/// If your native type needs to allocate some private block of memory, you can
/// allocate the memory when the native type gets sent the NTL_Initialize and
/// attach it to the instance by calling this function.
/// If you dynamically allocate memory using malloc() or operator new, do not
/// forget to free this memory when receiving NTL_Terminate. The virtual
/// machine will <b>not</b> automatically free this memory.
/// The function returns the value that was previously stored in the instance's
/// user data memory.
/// @see NTLInstance, NTLSetTypeUserData

JILEXTERN JILUnknown*	NTLInstanceSetUser		(NTLInstance* pInstance, JILUnknown* pData);

//------------------------------------------------------------------------------
// NTLInstanceGetUser
//------------------------------------------------------------------------------
/// Returns the pointer to the user memory if set by NTLInstanceSetUser().
/// If no pointer was previously set, returns NULL.
/// @see NTLInstance

JILEXTERN JILUnknown*	NTLInstanceGetUser		(NTLInstance* pInstance);

//------------------------------------------------------------------------------
// NTLInstanceTypeID
//------------------------------------------------------------------------------
/// Returns the type-id for the specified native type instance.
/// @see NTLInstance

JILEXTERN JILLong		NTLInstanceTypeID		(NTLInstance* pInstance);

//------------------------------------------------------------------------------
// NTLInstanceGetVM
//------------------------------------------------------------------------------
/// Returns a pointer to the virtual machine state that is owning this native
/// type instance.
/// @see NTLInstance

JILEXTERN JILState*		NTLInstanceGetVM		(NTLInstance* pInstance);

//------------------------------------------------------------------------------
// NTLSetTypeUserData
//------------------------------------------------------------------------------
/// This function allows you to attach an arbitrary pointer to any type.
/// This will only work if the type is already known by the runtime, which means
/// the type has already been imported (by script or programmatically).
/// The function will store the pointer in the type's user data field, the same
/// field that is written by the NTLInstanceSetUser() function. Be sure not to
/// overwrite important data when calling this.
/// <p>The purpose of this function is to allow developers to create native
/// functions that appear as global functions in JewelScript, but call an
/// instance of a C++ class natively. The binding code can retrieve the user
/// data pointer by calling NTLInstanceGetUser() and call a method from that
/// pointer.</p>
/// If the specified type-id is invalid, the function returns an error.
/// @see NTLInstanceGetUser, NTLInstanceSetUser

JILEXTERN JILError		NTLSetTypeUserData		(JILState* pVM, JILLong typeID, JILUnknown* pUser);

#endif	// #ifndef JILNATIVETYPE_H
