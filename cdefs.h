// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#ifndef __cplusplus
#error "C++ ahead"
#endif

#if __cplusplus < 201103L
#error "need c++11 or later"
#endif

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "standard_types.h"
#include "settings.h"


// optimization:
#define LIKELY(x)  	__builtin_expect((x),true)
#define UNLIKELY(x)	__builtin_expect((x),false)

#ifndef __printflike
#define __printflike(A,B)  __attribute__((format(printf, A, B)))
#endif

// Assertions:
// STATIC_ASSERT(COND)  -->  checked by compiler
// ASSERT(COND)         -->  checked at runtime, target Debug only!
// RUNTIME_ASSERT(COND) -->  checked at runtime, target Final too! (will reset the CPU or do s.th. similar evil)

#if defined(DEBUG)
#ifndef assert
#define assert(COND) (LIKELY(COND) ? (void)0 : handle_assert(__FILE__,__LINE__))
#endif
#else
#undef assert
#define assert(COND) ((void)0)
#endif

#define TOSTRING(X) #X
#define RUNTIME_ASSERT(COND) do { if(UNLIKELY(!(COND))) throw FatalError("ASSERT FAILED: " __FILE__ " line " TOSTRING(__LINE__) ": "  #COND); } while(0)

#define KITTY(X,Y) X ## Y
#define CAT(X,Y) KITTY(X,Y)
#define STATIC_ASSERT(COND) typedef char CAT(line_,__LINE__)[(COND)?1:-1] __attribute__((unused))

#define NELEM(feld) (sizeof(feld)/sizeof((feld)[0]))    // UNSIGNED !!


#ifndef debug_break

   #ifdef DEBUG
   #if defined(__arm__)
   #define debug_break() __asm__ volatile("bkpt")
   #else
   #include "3rdparty/debugbreak.h"
   #endif
   #else
   #define debug_break __debugbreak
   static inline void debug_break(){}
   #endif

   // must be provided by application:
   __attribute__((noreturn)) extern void handle_assert(cstr file, uint line) noexcept;

   #define IERR() handle_assert(__FILE__,__LINE__)
   #define TODO() handle_assert(__FILE__,__LINE__)
   #define OMEM() handle_assert(__FILE__,__LINE__)

#endif // debug_break


/**
 * Template: Determine the minimum of two values.
 * @param a  the one value
 * @param b  the other value
 * @return   the smaller one
 */
template<typename T>
static inline T min(T a,T b)
{
	return a<b?a:b;
}

/**
 * Template: Determine the maximum of two values.
 * @param a  the one value
 * @param b  the other value
 * @return   the greater one
 */
template<typename T>
static inline T max(T a,T b)
{
	return a>b?a:b;
}

/**
 * Template: Limit a value to a lower and upper limit.
 * @param a  the lower limit
 * @param x  the value
 * @param b  the upper limit
 * @return   the limited value
 */
template<typename S, typename T, typename U>
static inline T minmax(S a, T x, U b)
{
   return x >= a ? x <= b ? x : b : a;
}

/**
 * Wait for Interrupt.
 *
 * Whenever waiting this function should be used to consume time.
 * On an ARM it uses the assembler instruction @ref wfi which waits for the next interrupt.
 * It is not required that interrupts are enabled, but they should.
 * This function is expected to reduce the power consumption of this core (slightly).
 */
static inline void wfi() noexcept
{
#ifdef __arm__
__asm__ __volatile__("wfi");
#else
for (uint i=0; i<999; i++) { __asm__ __volatile__(""); }
#endif
}


static constexpr bool on = true;
static constexpr bool off = false;
