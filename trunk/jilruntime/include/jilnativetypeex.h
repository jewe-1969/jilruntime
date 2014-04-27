//------------------------------------------------------------------------------
// File: JILNativeTypeEx.h                                (c) 2005-2010 jewe.org
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
/// @file jilnativetypeex.h
/// Advanced API for writing native types.
///
/// Only use the functions below if you know exactly what you are doing. These
/// functions allow you to directly retrieve, create, or free handles from the
/// virtual machine. If you are not very sure you need to use these functions,
/// and how to use them, you should not use them. In the best case of wrong use,
/// you might create a large memory leak. In the worst case you might crash your
/// machine (the real one, not only the virtual...)
///
/// The functions from JILNativeType.h offer some simple to use and rather safe
/// functionality to exchange ints, floats and strings as arguments and return
/// values. However, if you want to create more powerful native types, such as
/// lists or containers of objects, arrays, and other complex data, then you
/// will need to use the functions below.
///
/// A few rules for using handles:
///
/// - Never create more than 1 handle for each individual object
/// - You must store the handle for as long as you keep the object
/// - Always free (release) the handle if you do not need the object anymore
/// - <b>Never directly delete / free an object you created a handle for!</b>
///   Just free the handle, the VM will destroy it automatically when no more
///   references exist
/// - Never create a handle for an object which's life-time you do not control!
///   (e.g. stack or other temporary objects, draw contexts, objects passed to
///   your code from the system, a framework, or elsewhere...)
///
/// With the introduction of <i>weak references</i> you have the option to create
/// or pass handles as weak reference to your native type. Weak reference handles
/// are unsafe (see language manual), but have advantages especially for native
/// types and objects.
///
/// Using weak reference handles you can:
///
/// - Create more than 1 handle for each individual object
/// - Lazily create a new handle each time you pass the object to the VM
/// - Avoid that the runtime destroys your object
/// - Take care of destroying the object yourself (you even have to)
/// - Create handles for objects which's life-time you don't control
///
/// For an in-depth article on handling JILHandle pointers, read this post:
/// http://blog.jewe.org/?p=745
///
/// To pass a handle as a weak reference to a function of your native type,
/// simply declare the function argument using the 'weak' modifier keyword.
/// To create a new handle for an object as a weak reference, use the
/// NTLNewWeakRefForObject() function.
//------------------------------------------------------------------------------

#ifndef JILNATIVETYPEEX_H
#define JILNATIVETYPEEX_H

#include "jilnativetype.h"

//------------------------------------------------------------------------------
// NTLHandleToTypeID
//------------------------------------------------------------------------------
/// Get the TypeID of the given handle. The function returns type_null if an
/// error occurs. This lets you determine the type of an object or value that you
/// have obtained a handle for.

JILEXTERN JILLong		NTLHandleToTypeID		(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToBaseID
//------------------------------------------------------------------------------
/// Get the TypeID of the base class or interface this handle implements. If the
/// specified handle is not an instance of a class, or the class does not
/// implement any interface, the result is type_null.
/// This lets you determine whether the handle you obtained is an instance of a
/// class that inherits from a specific class or interface.

JILEXTERN JILLong		NTLHandleToBaseID		(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToInt
//------------------------------------------------------------------------------
/// Convert an int handle directly to an int. If the specified handle is not an
/// int handle, returns 0.

JILEXTERN JILLong		NTLHandleToInt			(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToFloat
//------------------------------------------------------------------------------
/// Convert a float handle directly to an int. If the specified handle is not a
/// float handle, returns 0.

JILEXTERN JILFloat		NTLHandleToFloat		(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToString
//------------------------------------------------------------------------------
/// Convert a string handle directly to a const char pointer. If the specified
/// handle is not a string handle, returns a pointer to an empty string.

JILEXTERN const JILChar*	NTLHandleToString	(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToError
//------------------------------------------------------------------------------
/// Returns the error code from an exception object. If the specified handle is
/// not a type related to the exception interface, returns JIL_No_Exception.
/// <p>You can use this to test whether the result of JILCallFunction() produced
/// an error (and get the error code), or if the returned handle is actually
/// the result of the call to the script-function, in which case this function
/// returns JIL_No_Exception.</p>
/// <p>If you call delegates from inside a function of your native type, you
/// should use this function to check if the call produced an error, and if so,
/// abort any operation and return the error code up to the caller of your
/// native type function.</p>

JILEXTERN JILError		NTLHandleToError		(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToErrorMessage
//------------------------------------------------------------------------------
/// Returns the error message from an exception object. If the specified handle
/// is not a type related to the exception interface, returns NULL.
/// <p>You can use this to obtain the message string of an object that implements
/// the exception interface. The string object is returned as a JILHandle pointer.
/// Use NTLHandleToString() to obtain a C string from the handle.</p>
/// <p>This will actually call the getMessage() method of the exception object,
/// regardless whether the exception object is implemented natively or in script
/// code.</p>
/// You must call NTLFreeHandle() on the result of this function when you no
/// longer need it.

JILEXTERN JILHandle*	NTLHandleToErrorMessage	(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLHandleToObject
//------------------------------------------------------------------------------
/// Returns a pointer to the object from a given handle. If the given handle is
/// not compatible to the specified type, returns NULL.
/// You need to cast the returned void pointer to it's real type.
/// See NTLNewHandleForObject() documentation for information which TypeID is
/// associated with which pointer type.

JILEXTERN JILUnknown*	NTLHandleToObject		(JILState* pState, JILLong type, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLGetArgHandle
//------------------------------------------------------------------------------
/// Get the handle address of an object passed to the typelib as an argument.
/// A reference is added to the handle, so it cannot be destroyed by the virtual
/// machine unless the typelib frees it. Call NTLFreeHandle() to free a handle
/// if you don't need it anymore. Call this if you want to store 'pointers' to
/// objects passed to your typelib.

JILEXTERN JILHandle*	NTLGetArgHandle			(JILState* pState, JILLong argNum);

//------------------------------------------------------------------------------
// NTLGetNullHandle
//------------------------------------------------------------------------------
/// This will return the handle that represents 'null' in the JILRuntime
/// environment. This is useful for initializing handle pointers in your class
/// with 'null'. A reference is added to the null handle. If you replace your
/// handle pointer later with a pointer to another handle, you must free the
/// null handle first.

JILEXTERN JILHandle*	NTLGetNullHandle		(JILState* pState);

//------------------------------------------------------------------------------
// NTLReturnHandle
//------------------------------------------------------------------------------
/// Call this if your member function or static function returns a value.
/// Pass a pointer to the return value's handle. If you pass a NULL pointer, the
/// function automatically returns the 'null handle'.

JILEXTERN void			NTLReturnHandle			(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLReferHandle
//------------------------------------------------------------------------------
/// Add a reference to a handle obtained from another object. This function needs
/// to be called in <b>very rare cases</b>, normally the other API functions take care
/// of proper addref'ing automatically. The only case, where you need to add a
/// reference to a handle <b>explicitly</b>, is when you get passed an object or array
/// to one of your functions and you move a <b>member</b> handle from that object or
/// <b>element</b> handle from that array into your object in order to keep it.
/// In other words, for all data that is directly passed to your NTL, the
/// reference counting is handled automatically. However, if this data contains other
/// handles, because it is an object or array, and you want to keep one of the
/// contained handles in your object, you need to addref it. Of course, you also
/// need to release this handle again, when your object is destroyed.

JILEXTERN void			NTLReferHandle			(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLFreeHandle
//------------------------------------------------------------------------------
/// Free (release) a handle your native type owns and doesn't need anymore.

JILEXTERN void			NTLFreeHandle			(JILState* pState, JILHandle* handle);

//------------------------------------------------------------------------------
// NTLMarkHandle
//------------------------------------------------------------------------------
/// Call this for all handles your native class owns if your native type
/// receives the NTL_MarkHandles message, which is sent by the garbage collector
/// to find and free leaked objects due to reference cycles.

JILEXTERN JILError		NTLMarkHandle			(JILState* pState, JILHandle* pHandle);

//------------------------------------------------------------------------------
// NTLCopyHandle
//------------------------------------------------------------------------------
/// Create a copy of the source object and return a new handle for it.
/// All objects will be copied, regardless of whether they are value-types or
/// reference-types. For reference-types, the object's copy-constructor method
/// will be invoked, if the class has defined one.
/// <p>Note that this is a recursive operation and can potentially fail, if for
/// example a native class does not implement a copy-constructor.
/// In case of an error, this function returns NULL.</p>
/// <p>This function should only be used if you want to deep-copy an object. For
/// normal copies, you should use JILCopyValueType() instead.</p>

JILEXTERN JILHandle*	NTLCopyHandle			(JILState* pState, JILHandle* hSource);

//------------------------------------------------------------------------------
// JILCopyValueType
//------------------------------------------------------------------------------
/// Copies the source object and returns a new handle for it, if it is a value-type.
/// If the source object is a reference-type, adds a reference to it and returns
/// the source handle. This is not a recursive operation.

JILEXTERN JILHandle*	NTLCopyValueType		(JILState* pState, JILHandle* hSource);

//------------------------------------------------------------------------------
// NTLNewHandleForObject
//------------------------------------------------------------------------------
/// Create a handle for an existing object. Make sure you never create more than
/// one handle for each individual object. Pass in a pointer to the object and
/// the type identifier of the object. For objects and native objects, you need
/// to pass in the TypeID of the object's class.
/// The return value will be the new handle, or 0 if the function fails.
/// Call NTLFreeHandle() if you do not need the object and handle anymore.
/// <p>Use this if you are certain that the object you pass in can be a reference
/// counted resource, and that your application is safe, if the object will be
/// automatically destroyed by the runtime once the reference count reaches 0.
/// If you cannot risk that the object is automatically destroyed by the runtime
/// or you cannot guarantee that only one handle is created for the object, use
/// NTLNewWeakRefForObject() instead of this function.</p>
/// <p>If you pass a NULL pointer for 'pObject' the function will return a
/// null-handle, which respresents the 'null' value in JewelScript.</p>
/// Valid TypeID and expected type of given pointer:<p>
///	type_int --> JILLong*<br>
///	type_float --> JILFloat*<br>
///	type_string --> JILString*<br>
///	type_array --> JILArray*<br>
///	(TypeID of your native class) --> (your native class)*</p>
/// @see NTLTypeNameToTypeID ()

JILEXTERN JILHandle*	NTLNewHandleForObject	(JILState* pState, JILLong TypeID, JILUnknown* pObject);

//------------------------------------------------------------------------------
// NTLNewWeakRefForObject
//------------------------------------------------------------------------------
/// Like NTLNewHandleForObject(), but creates a <b>weak reference</b> instead of
/// a normal reference to the object. Use this in cases where you <b>do not</b>
/// want the runtime to automatically destroy your object when there are no more
/// references to it in the VM. Use this only for objects to which you keep a
/// pointer somewhere in your application, because you will have to take care of
/// freeing the object yourself when it is no longer needed.
/// <p>Note that there is always the risk that weak references to your object
/// still exist somewhere in the virtual machine when you destroy it, which can
/// lead to a crash. If possible, only create one weak reference handle for each
/// individual object. In this case you can examine the handle's reference count
/// as an indicator whether the object is still in use by the VM or not. Another
/// possibility is to keep the object alive until you terminate the VM.</p>
/// <p>Of course you can also define your own mechanisms to make using weak
/// references to objects safer, like defining a specific script function
/// (i.e. "dispose" method) that you call right before you destroy your object,
/// which is supposed to free any existing references to the object your script
/// may hold.</p>
/// <p>Even though your object will not be destroyed by the handle returned by this
/// function, you do have to take care of proper reference counting of the
/// handle, to ensure that the handle itself is being freed once it is no longer
/// in use. Not freeing the handle will create a handle leak, making the VM's
/// handle segment grow larger and larger, which eventually will decrease
/// performance and spoil system memory.</p>

JILEXTERN JILHandle*	NTLNewWeakRefForObject	(JILState* pState, JILLong TypeID, JILUnknown* pObject);

//------------------------------------------------------------------------------
// NTLNewObject
//------------------------------------------------------------------------------
/// This allocates a new object from a JIL class or native type and returns
/// a new handle for it. You need to pass in the TypeID of the object's class.
/// Make sure to call NTLFreeHandle() when you do not need the object anymore.
/// Note that this does not automatically call the object's constructor method.
/// To ensure the JIL object or native object is properly initialized, you need
/// to call JILCallFunction() for the returned object handle.
/// If the given TypeID is invalid, the result is 0.
/// @see NTLTypeNameToTypeID ()

JILEXTERN JILHandle*	NTLNewObject			(JILState* pState, JILLong TypeID);

//------------------------------------------------------------------------------
// NTLLoadResource
//------------------------------------------------------------------------------
/// A native type library can use this to load any resources needed into memory.
/// The function will use the virtual machine's file input callback function to
/// load the data, so this will also work when the application has overridden
/// the librarys default loading behaviour.
/// The function will load the whole file into memory and return a pointer to
/// the data and the file size in 'ppData' and 'pSize'.
/// The caller must free the returned memory block by calling NTLFreeResource()
/// when he is done with it.
/// @see NTLFileOpen, NTLFileClose, NTLFileRead, NTLFileSeek, NTLFileLength

JILEXTERN JILError		NTLLoadResource			(JILState* pState, const JILChar* pName, JILUnknown** ppData, JILLong* pSize);

//------------------------------------------------------------------------------
// NTLFreeResource
//------------------------------------------------------------------------------
/// Free a memory block returned by NTLLoadResource().
/// Call this if you have loaded any resources using NTLLoadResource().
/// Pass in the 'ppData' pointer returned by NTLLoadResource()
/// in order to free the memory block.

JILEXTERN JILError		NTLFreeResource			(JILState* pState, JILUnknown* pData);

//------------------------------------------------------------------------------
// NTLFileOpen
//------------------------------------------------------------------------------
/// This will open any file using the VM's file input callback function, so this
/// will also work when the application has overridden the library's default
/// loading behaviour.
/// If successful, the function will write a pointer to a JILFileHandle object to 'ppFileOut'.
/// Pass the object to subsequent calls to NTLFileRead, NTLFileSeek, NTLFileLength and NTLFileClose.

JILEXTERN JILError		NTLFileOpen				(JILState* pState, const JILChar* pName, JILFileHandle** ppFileOut);

//------------------------------------------------------------------------------
// NTLFileRead
//------------------------------------------------------------------------------
/// This will read the given amount of bytes to the specified data buffer.
/// Unlike NTLLoadResource, this function does NOT manage the data buffer for the
/// caller. Instead, the caller needs to allocate a buffer of sufficient size and
/// pass it to this function.
/// If the remainder of the file is smaller than the number of bytes requested,
/// the function will return the number of bytes actually read.

JILEXTERN JILLong		NTLFileRead				(JILFileHandle* pFile, JILUnknown* pData, JILLong length);

//------------------------------------------------------------------------------
// NTLFileSeek
//------------------------------------------------------------------------------
/// This will move the file pointer to the specified byte position in the file.
/// Since the file pointer is a signed 32-bit value, this will only work for
/// files up to 2 GB in size.

JILEXTERN JILError		NTLFileSeek				(JILFileHandle* pFile, JILLong position);

//------------------------------------------------------------------------------
// NTLFileLength
//------------------------------------------------------------------------------
/// Returns the total size, in bytes, of the file to 'pLengthOut'.
/// Depending on the actual implementation of the VM's file input proc, calling
/// this may move the file pointer to the end of the file.

JILEXTERN JILLong		NTLFileLength			(JILFileHandle* pFile);

//------------------------------------------------------------------------------
// NTLFileClose
//------------------------------------------------------------------------------
/// Closes the given file. After this call the specified JILFileHandle object
/// will be destroyed and the point should no longer be used.

JILEXTERN JILError		NTLFileClose			(JILFileHandle* pFile);

//------------------------------------------------------------------------------
// NTLDeclareConstantInt
//------------------------------------------------------------------------------
/// Adds a constant declaration of type 'typeID' and initialized by the integer
/// 'value' to a dynamic type declaration string.
/// <p>This and the following NTLDeclare...() functions can be used if you do
/// not want to "hardcode" the declaration of your native type library.
/// It allows you, for example to add global constant definitions to your class
/// based on values defined at runtime.</p>
/// <p>Call this function when your native type receives the NTL_GetDeclString
/// message and pass it the pDataIn pointer received by your type proc.
/// <p>Calling this function with these values:<p>
/// <pre>NTLDeclareConstantInt(pDataIn, type_int, "kFoo", 27);</pre>
/// <p>Will add the following string to the type declaration:</p>
/// <pre>const int kFoo = 27;</pre>

JILEXTERN JILError		NTLDeclareConstantInt		(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, JILLong value);

//------------------------------------------------------------------------------
// NTLDeclareConstantFloat
//------------------------------------------------------------------------------
/// Adds a constant declaration of type 'typeID' and initialized by the float
/// 'value' to the dynamic type declaration. See NTLDeclareConstantInt() for
/// more information.

JILEXTERN JILError		NTLDeclareConstantFloat		(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, JILFloat value);

//------------------------------------------------------------------------------
// NTLDeclareConstantString
//------------------------------------------------------------------------------
/// Adds a constant declaration of type 'typeID' and initialized by the string
/// 'pValue' to the dynamic type declaration. The given C-string will be
/// automatically enclosed in double quotes before adding it to the class
/// declaration string.
/// See NTLDeclareConstantInt() for more information.

JILEXTERN JILError		NTLDeclareConstantString	(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, const JILChar* pValue);

//------------------------------------------------------------------------------
// NTLDeclareVerbatim
//------------------------------------------------------------------------------
/// Adds the given string verbatim (literally) to the class declaration string.
/// You can use this function to add any static parts of the class declaration to
/// the class declaration string (usually the function and method declarations).
/// <p>Call this function when your native type receives the NTL_GetDeclString
/// message and pass it the pDataIn pointer received by your type proc.
/// See NTLDeclareConstantInt() for more information.

JILEXTERN JILError		NTLDeclareVerbatim			(JILUnknown* pDataIn, const JILChar* pText);

#endif	// #ifndef JILNATIVETYPEEX_H
