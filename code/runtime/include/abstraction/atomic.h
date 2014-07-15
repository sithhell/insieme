/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#pragma once
#ifndef __GUARD_ABSTRACTION_ATOMIC_H
#define __GUARD_ABSTRACTION_ATOMIC_H

#ifdef _GEMS

	// direct mapping to compiler primitives/instrinsics
	 
	#define irt_atomic_fetch_and_add(__location, __value, __type)  __sync_fetch_and_add_##__type##(__location, __value)

	#define irt_atomic_fetch_and_add(__location, __value, __type)  __sync_fetch_and_add_##__type##(__location, __value)
	#define irt_atomic_fetch_and_sub(__location, __value, __type)  __sync_fetch_and_sub_##__type##(__location, __value)

	#define irt_atomic_add_and_fetch(__location, __value, __type)  __sync_add_and_fetch_##__type##(__location, __value)
	#define irt_atomic_sub_and_fetch(__location, __value, __type)  __sync_sub_and_fetch_##__type##(__location, __value)
	#define irt_atomic_or_and_fetch(__location, __value, __type)   __sync_or_and_fetch_##__type##(__location, __value)
	#define irt_atomic_and_and_fetch(__location, __value, __type)  __sync_and_and_fetch_##__type##(__location, __value)
	#define irt_atomic_xor_and_fetch(__location, __value, __type)  __sync_xor_and_fetch_##__type##(__location, __value)

	/**
	 * These builtins perform an atomic compare and swap. That is, if the current value of *__location is oldval, then write newval into *__location.
	 *
	 * irt_atomic_bool_compare_and_swap returns true if successful, false otherwise
	 * irt_atomic_val_compare_and_swap returns the value of *__location before the operation
	 */
	#define irt_atomic_bool_compare_and_swap(__location, __oldval, __newval, __type) __sync_bool_compare_and_swap_##__type##(__location, __oldval, __newval)
	#define irt_atomic_val_compare_and_swap(__location, __oldval, __newval, __type)  __sync_val_compare_and_swap_##__type##(__location, __oldval, __newval)

	// convenience
	// explicitly cast return value to void to supress warnings
	#define irt_atomic_inc(__location, __type) (void)irt_atomic_fetch_and_add(__location, 1, __type)
	#define irt_atomic_dec(__location, __type) (void)irt_atomic_fetch_and_sub(__location, 1, __type)

	int atomic_rmw_int(int *ptr, int value);

	#define IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(__type__, __op_string__) \
		__type__  __sync_fetch_and_##__op_string__##_##__type__ (__type__*  ptr, __type__  value); 

	#define IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(__type__, __op_string__) \
		__type__  __sync_##__op_string__##_and_fetch_##__type__ (__type__*  ptr, __type__  value); 

	#define IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(__type) \
		bool __sync_bool_compare_and_swap_##__type (__type  *ptr, __type  oldval, __type  newval); 

	#define IRT_DEFINE_SYNC_VAL_COMPARE_AND_SWAP_DECLARATION(__type) \
		__type  __sync_val_compare_and_swap_##__type (__type  *ptr, __type  oldval, __type  newval); 

	/* 32-bit signed versions */

	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(int32_t, add)
	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(int32_t, sub)

	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int32_t, add)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int32_t, sub)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int32_t, or)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int32_t, and)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int32_t, xor)
	
	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(int32_t)
	IRT_DEFINE_SYNC_VAL_COMPARE_AND_SWAP_DECLARATION(int32_t)

	/* 32-bit unsigned versions */

	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(uint32_t, sub)
	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(uint32_t, add)

	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint32_t, add)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint32_t, sub)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint32_t, or)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint32_t, and)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint32_t, xor)
	
	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(uint32_t)
	IRT_DEFINE_SYNC_VAL_COMPARE_AND_SWAP_DECLARATION(uint32_t)

	/* 64-bit signed versions */

	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(int64_t, sub)
	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(int64_t, add)

	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int64_t, add)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int64_t, sub)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int64_t, or)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int64_t, and)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(int64_t, xor)
	
	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(int64_t)
	IRT_DEFINE_SYNC_VAL_COMPARE_AND_SWAP_DECLARATION(int64_t)

	/* 64-bit unsigned versions */

	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(uint64_t, sub)
	IRT_DEFINE_SYNC_FETCH_AND_OP_DECLARATION(uint64_t, add)

	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint64_t, add)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint64_t, sub)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint64_t, or)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint64_t, and)
	IRT_DEFINE_SYNC_OP_AND_FETCH_DECLARATION(uint64_t, xor)
	
	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(uint64_t)
	IRT_DEFINE_SYNC_VAL_COMPARE_AND_SWAP_DECLARATION(uint64_t)

	/* additional versions */

	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(uintptr_t)
	IRT_DEFINE_SYNC_BOOL_COMPARE_AND_SWAP_DECLARATION(bool)

#elif !defined(_MSC_VER)

	// direct mapping to compiler primitives/instrinsics

	#define irt_atomic_fetch_and_add(__location, __value, __type, ...)  __sync_fetch_and_add(__location, __value, ##__VA_ARGS__)
	#define irt_atomic_fetch_and_sub(__location, __value, __type, ...)  __sync_fetch_and_sub(__location, __value, ##__VA_ARGS__)

	#define irt_atomic_add_and_fetch(__location, __value, __type, ...)  __sync_add_and_fetch(__location, __value, ##__VA_ARGS__)
	#define irt_atomic_sub_and_fetch(__location, __value, __type, ...)  __sync_sub_and_fetch(__location, __value, ##__VA_ARGS__)
	#define irt_atomic_or_and_fetch(__location, __value, __type, ...)   __sync_or_and_fetch(__location, __value, ##__VA_ARGS__)
	#define irt_atomic_and_and_fetch(__location, __value, __type, ...)  __sync_and_and_fetch(__location, __value, ##__VA_ARGS__)
	#define irt_atomic_xor_and_fetch(__location, __value, __type, ...)  __sync_xor_and_fetch(__location, __value, ##__VA_ARGS__)

	/**
	 * These builtins perform an atomic compare and swap. That is, if the current value of *__location is oldval, then write newval into *__location.
	 *
	 * irt_atomic_bool_compare_and_swap returns true if successful, false otherwise
	 * irt_atomic_val_compare_and_swap returns the value of *__location before the operation
	 */
	#define irt_atomic_bool_compare_and_swap(__location, __oldval, __newval, __type, ...) __sync_bool_compare_and_swap(__location, __oldval, __newval, ##__VA_ARGS__)
	#define irt_atomic_val_compare_and_swap(__location, __oldval, __newval, __type, ...)  __sync_val_compare_and_swap(__location, __oldval, __newval, ##__VA_ARGS__)

	//#define irt_atomic_lock_test_and_set(__location,  __value, ...) __sync_lock_test_and_set(__location, __value, ##__VA_ARGS__)
	//#define irt_atomic_lock_release(__location, ...)                __sync_lock_release(__location, ##__VA_ARGS__)

	// convenience
	// explicitly cast return value to void to supress warnings
	#define irt_atomic_inc(__location, __type) (void)irt_atomic_fetch_and_add(__location, 1, __type)
	#define irt_atomic_dec(__location, __type) (void)irt_atomic_fetch_and_sub(__location, 1, __type)

#else

	#include <Windows.h>

	// working common mappings for all windows versions
	#define irt_atomic_or_and_fetch(__location, __value, __type, ...)		(_InterlockedOr(__location, __value))
	#define irt_atomic_and_and_fetch(__location, __value, __type, ...)		(_InterlockedAnd(__location, __value))
	#define irt_atomic_xor_and_fetch(__location, __value, __type, ...)		(_InterlockedXor(__location, __value))


	// Windows 7 and up -> InterlockedExchangeAdd and others are overloaded (such that there is a function with matching types)
	#if (WINVER >= 0x0601)

		// fetch_and_op: original value before op shall be returned
		#define irt_atomic_fetch_and_add(__location, __value, __type, ...)		(InterlockedExchangeAdd(__location, __value))
		#define irt_atomic_fetch_and_sub(__location, __value, __type, ...)		(InterlockedExchangeAdd(__location, - (__value)))

		#define irt_atomic_add_and_fetch(__location, __value, __type, ...)		(InterlockedExchangeAdd(__location, __value) + (__value))
		#define irt_atomic_sub_and_fetch(__location, __value, __type, ...)		(InterlockedExchangeAdd(__location, - (__value)) - (__value))

		/**
		 * These builtins perform an atomic compare and swap. That is, if the current value of *__location is oldval, then write newval into *__location.
		 *
		 * irt_atomic_bool_compare_and_swap returns true if successful, false otherwise
		 * irt_atomic_val_compare_and_swap returns the value of *__location before the operation
		 */
		#define irt_atomic_bool_compare_and_swap(__location, __oldval, __newval, __type, ...)	((__oldval) == InterlockedCompareExchange(__location, __newval, __oldval)) /* somehow equiv to __oldval == *__location which is the condition for swapping values */
		#define irt_atomic_val_compare_and_swap(__location, __oldval, __newval, __type, ...)	InterlockedCompareExchange(__location, __newval, __oldval)

		#define irt_atomic_inc(__location, __type) InterlockedIncrement(__location)
		#define irt_atomic_dec(__location, __type) InterlockedDecrement(__location)

	// Windows XP, Windows Vista (and below?) -> there exist no overloaded functions but 32bit and 64bit versions of interlocked functions
	// hence we do the overloading
	#else // WINVER < 0x0601

		#include <intrin.h>

		#pragma intrinsic(_InterlockedCompareExchange64)
		
		// function overloads which will either call the 32bit or the 64bit interlocked operation
		uint64 _irt_interlocked_exchange_add(volatile uint64 *loc, uint64 val){

			LONGLONG old;

			do {
				old = *loc;
			} while (_InterlockedCompareExchange64((LONGLONG*)loc,
												  (LONGLONG)(old + val),
												  old) != old);

			return (uint64)old;
			//return (uint64)_InterlockedExchangeAdd64((LONGLONG*)loc, (LONGLONG)val);
		}

		uint32 _irt_interlocked_exchange_add(volatile uint32 *loc, uint32 val){
			return (uint32)InterlockedExchangeAdd((long*)loc, (long)val);
		}

		uint64 _irt_interlocked_compare_exchange(volatile uint64 *loc, uint64 newval, uint64 oldval){
			// directly call the compiler intrinsic
			return (uint64)_InterlockedCompareExchange64((LONGLONG*)loc, (LONGLONG)newval, (LONGLONG)oldval);
		}

		uint32 _irt_interlocked_compare_exchange(volatile uint32 *loc, uint32 newval, uint32 oldval){
			// directly call the compiler intrinsic
			return (uint32)InterlockedCompareExchange((long*)loc, (long)newval, (long)oldval);
		}

		uint64 _irt_interlocked_increment(uint64 *loc){
			LONGLONG old;
			do {
				old = *loc;
			} while (_InterlockedCompareExchange64((LONGLONG*)loc,
												  (LONGLONG)(old + 1),
												  old) != old);
			return (uint64)old+1;
		}

		uint32 _irt_interlocked_increment(uint32 *loc){
			return (uint32)InterlockedIncrement((long*)loc);
		}

		uint64 _irt_interlocked_decrement(uint64 *loc){
			LONGLONG old;
			do {
				old = *loc;
			} while (_InterlockedCompareExchange64((LONGLONG*)loc,
												  (LONGLONG)(old - 1),
												  old) != old);
			return (uint64)old-1;
		}

		uint32 _irt_interlocked_decrement(uint32 *loc){
			return (uint32)InterlockedDecrement((long*)loc);
		}


		#define irt_atomic_fetch_and_add(__location, __value, __type, ...)		(_irt_interlocked_exchange_add(__location, __value))
		#define irt_atomic_fetch_and_sub(__location, __value, __type, ...)		(_irt_interlocked_exchange_add(__location, - (__value)))

		#define irt_atomic_add_and_fetch(__location, __value, __type, ...)		(_irt_interlocked_exchange_add(__location, __value) + (__value))
		#define irt_atomic_sub_and_fetch(__location, __value, __type, ...)		(_irt_interlocked_exchange_add(__location, - (__value)) - (__value))


		#define irt_atomic_bool_compare_and_swap(__location, __oldval, __newval, __type, ...)	((__oldval) == _irt_interlocked_compare_exchange(__location, __newval, __oldval)) /* somehow equiv to __oldval == *__location which is the condition for swapping values */
		#define irt_atomic_val_compare_and_swap(__location, __oldval, __newval, __type, ...)	_irt_interlocked_compare_exchange(__location, __newval, __oldval)

		#define irt_atomic_inc(__location, __type) _irt_interlocked_increment(__location)
		#define irt_atomic_dec(__location, __type) _irt_interlocked_decrement(__location)

	#endif // WINVER >= 0x0600

#endif


#endif // ifndef __GUARD_ABSTRACTION_ATOMIC_H