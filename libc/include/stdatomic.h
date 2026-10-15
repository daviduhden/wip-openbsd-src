/*	$OpenBSD$	*/

/*-
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ISO/IEC 9899:2011 (C11) 7.17: Atomics <stdatomic.h>,
 * carried forward through C17 and C23.
 *
 * The interface is built on the compiler's atomic builtins.  Those are
 * language-level primitives: they are the only way to obtain real atomic
 * instructions, and using them is explicitly permitted.  The
 * compiler-supplied <stdatomic.h> is never included; this header is the
 * implementation.
 *
 * clang exposes the __c11_atomic_* builtins, which take a pointer to an
 * atomic object and (for loads and read-modify-write operations) return the
 * value directly, aggregates included.  GCC exposes the __atomic_* builtins,
 * which instead take the address of a result object; those are wrapped in
 * GNU statement expressions whose temporaries are ordinary, non-atomic
 * objects, computed with __typeof__((void)0, *p).  Each variant evaluates
 * every argument exactly once and every statement expression is wrapped in
 * __extension__ so that it does not trip -Wpedantic.
 *
 * A type whose size is not lock-free cannot be handled by a single
 * instruction.  The compiler would normally emit a call to its atomic
 * runtime (libatomic), which the base system does not provide.  Instead,
 * every operation first tests __atomic_always_lock_free() (a compile-time
 * constant) and, when it is false, calls a small lock-based fallback in
 * libc (__atomic_fallback_*); the constant test is folded away so the
 * compiler's runtime is never referenced.  The fallback acquires a
 * per-address spinlock with sequential-consistency ordering, which is
 * stronger than any memory order the standard asks for, so the requested
 * order is always satisfied.  This makes the whole interface, including
 * non-lock-free types, self-contained.
 *
 * atomic_is_lock_free() is answered with __atomic_always_lock_free(), which
 * is a compile-time constant.  The ATOMIC_*_LOCK_FREE constants come from
 * the compiler's own __GCC_ATOMIC_*_LOCK_FREE macros.
 *
 * This is a C header.  It also builds as C++ when the implementation of
 * _Atomic(T) is available (clang); the C++ front end of GCC has no _Atomic
 * and is diagnosed below.
 */

#ifndef _STDATOMIC_H_
#define _STDATOMIC_H_

#include <sys/cdefs.h>

#if defined(__cplusplus) && !defined(__clang__)
#error "<stdatomic.h> in C++ requires a compiler that implements _Atomic(T)"
#endif

#if defined(__STDC_NO_ATOMICS__)
#error "<stdatomic.h> is not available: __STDC_NO_ATOMICS__ is defined"
#endif

#if defined(__has_builtin)
#if !__has_builtin(__atomic_thread_fence) ||                                   \
    !__has_builtin(__atomic_signal_fence) ||                                   \
    !__has_builtin(__atomic_always_lock_free) ||                               \
    !__has_builtin(__atomic_test_and_set)
#error "<stdatomic.h> requires the compiler atomic builtins"
#endif
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define __STDC_VERSION_STDATOMIC_H__ 202311L
#endif

/*
 * The boolean type of the atomic_bool alias and of the compare-exchange
 * results: _Bool is a C keyword, bool is a C++ keyword.
 */
#ifdef __cplusplus
#define __stdc_atomic_bool bool
#else
#define __stdc_atomic_bool _Bool
#endif

/*
 * 7.17.1: lock-free property of the standard atomic types.
 */
#ifndef __GCC_ATOMIC_BOOL_LOCK_FREE
#error "<stdatomic.h> requires the compiler __GCC_ATOMIC_*_LOCK_FREE macros"
#endif
#define ATOMIC_BOOL_LOCK_FREE __GCC_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE __GCC_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __GCC_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __GCC_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE __GCC_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE __GCC_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE __GCC_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE __GCC_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE __GCC_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE __GCC_ATOMIC_POINTER_LOCK_FREE
#if defined(__GCC_ATOMIC_CHAR8_T_LOCK_FREE)
#define ATOMIC_CHAR8_T_LOCK_FREE __GCC_ATOMIC_CHAR8_T_LOCK_FREE
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/* char8_t is unsigned char; use its property if the compiler is silent. */
#define ATOMIC_CHAR8_T_LOCK_FREE ATOMIC_CHAR_LOCK_FREE
#endif

/*
 * 7.17.2: initialization.
 *
 * ATOMIC_VAR_INIT() was deprecated in C17 and removed in C23; it is kept
 * for C17 and earlier (and for C++, where it still exists).
 */
#if !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L) ||             \
    defined(__cplusplus)
#define ATOMIC_VAR_INIT(value) (value)
#endif

/*
 * 7.17.3: order and consistency.
 *
 * The values are the compiler's own memory order constants, so that they
 * can be passed straight to the builtins.
 */
typedef enum {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

/*
 * kill_dependency() is not required to perform any operation; it exists to
 * document that the returned value carries no dependency for
 * memory_order_consume.  clang and GCC do not track such dependencies, so
 * returning the argument unchanged is exact.
 */
#define kill_dependency(y) (y)

/*
 * 7.17.5: lock-free property.
 *
 * sizeof() does not evaluate its operand, and the pointer argument of
 * __atomic_always_lock_free() is only consulted for known alignment.
 */
#define atomic_is_lock_free(obj)                                               \
	__atomic_always_lock_free(sizeof(*(obj)), (obj))

/*
 * 7.17.6: atomic integer types.
 *
 * The element types are spelled with the compiler's predefined type macros
 * so that the header does not depend on <stdint.h>.
 */
__extension__ typedef _Atomic(__stdc_atomic_bool) atomic_bool;
__extension__ typedef _Atomic(char) atomic_char;
__extension__ typedef _Atomic(signed char) atomic_schar;
__extension__ typedef _Atomic(unsigned char) atomic_uchar;
__extension__ typedef _Atomic(short) atomic_short;
__extension__ typedef _Atomic(unsigned short) atomic_ushort;
__extension__ typedef _Atomic(int) atomic_int;
__extension__ typedef _Atomic(unsigned int) atomic_uint;
__extension__ typedef _Atomic(long) atomic_long;
__extension__ typedef _Atomic(unsigned long) atomic_ulong;
__extension__ typedef _Atomic(long long) atomic_llong;
__extension__ typedef _Atomic(unsigned long long) atomic_ullong;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
__extension__ typedef _Atomic(__CHAR8_TYPE__) atomic_char8_t;
#endif
__extension__ typedef _Atomic(__CHAR16_TYPE__) atomic_char16_t;
__extension__ typedef _Atomic(__CHAR32_TYPE__) atomic_char32_t;
__extension__ typedef _Atomic(__WCHAR_TYPE__) atomic_wchar_t;
__extension__ typedef _Atomic(__INT_LEAST8_TYPE__) atomic_int_least8_t;
__extension__ typedef _Atomic(__UINT_LEAST8_TYPE__) atomic_uint_least8_t;
__extension__ typedef _Atomic(__INT_LEAST16_TYPE__) atomic_int_least16_t;
__extension__ typedef _Atomic(__UINT_LEAST16_TYPE__) atomic_uint_least16_t;
__extension__ typedef _Atomic(__INT_LEAST32_TYPE__) atomic_int_least32_t;
__extension__ typedef _Atomic(__UINT_LEAST32_TYPE__) atomic_uint_least32_t;
__extension__ typedef _Atomic(__INT_LEAST64_TYPE__) atomic_int_least64_t;
__extension__ typedef _Atomic(__UINT_LEAST64_TYPE__) atomic_uint_least64_t;
__extension__ typedef _Atomic(__INT_FAST8_TYPE__) atomic_int_fast8_t;
__extension__ typedef _Atomic(__UINT_FAST8_TYPE__) atomic_uint_fast8_t;
__extension__ typedef _Atomic(__INT_FAST16_TYPE__) atomic_int_fast16_t;
__extension__ typedef _Atomic(__UINT_FAST16_TYPE__) atomic_uint_fast16_t;
__extension__ typedef _Atomic(__INT_FAST32_TYPE__) atomic_int_fast32_t;
__extension__ typedef _Atomic(__UINT_FAST32_TYPE__) atomic_uint_fast32_t;
__extension__ typedef _Atomic(__INT_FAST64_TYPE__) atomic_int_fast64_t;
__extension__ typedef _Atomic(__UINT_FAST64_TYPE__) atomic_uint_fast64_t;
__extension__ typedef _Atomic(__INTPTR_TYPE__) atomic_intptr_t;
__extension__ typedef _Atomic(__UINTPTR_TYPE__) atomic_uintptr_t;
__extension__ typedef _Atomic(__SIZE_TYPE__) atomic_size_t;
__extension__ typedef _Atomic(__PTRDIFF_TYPE__) atomic_ptrdiff_t;
__extension__ typedef _Atomic(__INTMAX_TYPE__) atomic_intmax_t;
__extension__ typedef _Atomic(__UINTMAX_TYPE__) atomic_uintmax_t;

/*
 * atomic_flag is a structure type whose single member is itself atomic, so
 * that the object is one byte wide and therefore always lock-free.  The
 * flag operations below address that byte through the builtins.
 */
struct __stdc_atomic_flag {
	atomic_bool __value;
};
typedef struct __stdc_atomic_flag atomic_flag;

#ifdef __cplusplus
#define ATOMIC_FLAG_INIT {false}
#else
#define ATOMIC_FLAG_INIT {0}
#endif

/*
 * 7.17.4: fences.  Prototypes are provided for source compatibility and so
 * that the address of the functions can be taken; the macros redirect the
 * ordinary calls to the builtins.  libc exports real definitions.  The
 * __atomic_fallback_* functions are the lock-based support for atomic
 * objects that are not lock-free; they are defined in libc and are part of
 * the implementation, not of the public interface.
 */
__BEGIN_DECLS
void atomic_thread_fence(memory_order);
void atomic_signal_fence(memory_order);
#ifndef __cplusplus
_Bool atomic_flag_test_and_set(volatile atomic_flag *);
_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *, memory_order);
#else
bool atomic_flag_test_and_set(volatile atomic_flag *);
bool atomic_flag_test_and_set_explicit(volatile atomic_flag *, memory_order);
#endif
void atomic_flag_clear(volatile atomic_flag *);
void atomic_flag_clear_explicit(volatile atomic_flag *, memory_order);

void __atomic_fallback_load(__SIZE_TYPE__, const volatile void *, void *, int);
void __atomic_fallback_store(__SIZE_TYPE__, volatile void *, const void *, int);
void __atomic_fallback_exchange(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
int __atomic_fallback_compare_exchange(
    __SIZE_TYPE__, volatile void *, void *, const void *, int, int, int);
void __atomic_fallback_fetch_add(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
void __atomic_fallback_fetch_sub(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
void __atomic_fallback_fetch_and(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
void __atomic_fallback_fetch_or(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
void __atomic_fallback_fetch_xor(
    __SIZE_TYPE__, volatile void *, const void *, void *, int);
__END_DECLS

#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)

#define atomic_flag_test_and_set(object)                                       \
	__atomic_test_and_set((object), __ATOMIC_SEQ_CST)
#define atomic_flag_test_and_set_explicit(object, order)                       \
	__atomic_test_and_set((object), (order))
#define atomic_flag_clear(object) __atomic_clear((object), __ATOMIC_SEQ_CST)
#define atomic_flag_clear_explicit(object, order)                              \
	__atomic_clear((object), (order))

/*
 * 7.17.2/7.17.7: operations on atomic types.
 *
 * atomic_init() is an ordinary relaxed store; it may not be used
 * concurrently with any other access to the object.
 */
#define atomic_init(object, value)                                             \
	atomic_store_explicit((object), (value), memory_order_relaxed)

#if defined(__clang__)

/*
 * clang: type-generic builtins that operate on the atomic object directly.
 */
#define atomic_store_explicit(object, desired, order)                          \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__c11_atomic_store(__stdc_o, __stdc_v, (order));       \
		else                                                           \
			__atomic_fallback_store(                               \
			    sizeof(__stdc_v), __stdc_o, &__stdc_v, (order));   \
	})
#define atomic_load_explicit(object, order)                                    \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_v = __c11_atomic_load(__stdc_o, (order));       \
		else                                                           \
			__atomic_fallback_load(                                \
			    sizeof(__stdc_v), __stdc_o, &__stdc_v, (order));   \
		__stdc_v;                                                      \
	})
#define atomic_exchange_explicit(object, desired, order)                       \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		__typeof__((void)0, *__stdc_o) __stdc_r;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_r = __c11_atomic_exchange(                      \
			    __stdc_o, __stdc_v, (order));                      \
		else                                                           \
			__atomic_fallback_exchange(sizeof(__stdc_v), __stdc_o, \
			    &__stdc_v, &__stdc_r, (order));                    \
		__stdc_r;                                                      \
	})
#define atomic_compare_exchange_strong_explicit(                               \
    object, expected, desired, success, failure)                               \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		int __stdc_r;                                                  \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_r =                                             \
			    __c11_atomic_compare_exchange_strong(__stdc_o,     \
			        (expected), __stdc_v, (success), (failure));   \
		else                                                           \
			__stdc_r = __atomic_fallback_compare_exchange(         \
			    sizeof(__stdc_v), __stdc_o, (expected), &__stdc_v, \
			    0, (success), (failure));                          \
		(__stdc_atomic_bool) __stdc_r;                                 \
	})
#define atomic_compare_exchange_weak_explicit(                                 \
    object, expected, desired, success, failure)                               \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		int __stdc_r;                                                  \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_r =                                             \
			    __c11_atomic_compare_exchange_weak(__stdc_o,       \
			        (expected), __stdc_v, (success), (failure));   \
		else                                                           \
			__stdc_r = __atomic_fallback_compare_exchange(         \
			    sizeof(__stdc_v), __stdc_o, (expected), &__stdc_v, \
			    1, (success), (failure));                          \
		(__stdc_atomic_bool) __stdc_r;                                 \
	})
#define __stdc_clang_fetch(name, fallback, object, operand, order)             \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_r;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_r), 0))            \
			__stdc_r = name(__stdc_o, (operand), (order));         \
		else {                                                         \
			__typeof__((void)0, *__stdc_o) __stdc_v =              \
			    (__typeof__((void)0, *__stdc_o))(operand);         \
			fallback(sizeof(__stdc_r), __stdc_o, &__stdc_v,        \
			    &__stdc_r, (order));                               \
		}                                                              \
		__stdc_r;                                                      \
	})
#define atomic_fetch_add_explicit(object, operand, order)                      \
	__stdc_clang_fetch(__c11_atomic_fetch_add,                             \
	    __atomic_fallback_fetch_add, object, operand, order)
#define atomic_fetch_sub_explicit(object, operand, order)                      \
	__stdc_clang_fetch(__c11_atomic_fetch_sub,                             \
	    __atomic_fallback_fetch_sub, object, operand, order)
#define atomic_fetch_or_explicit(object, operand, order)                       \
	__stdc_clang_fetch(__c11_atomic_fetch_or, __atomic_fallback_fetch_or,  \
	    object, operand, order)
#define atomic_fetch_xor_explicit(object, operand, order)                      \
	__stdc_clang_fetch(__c11_atomic_fetch_xor,                             \
	    __atomic_fallback_fetch_xor, object, operand, order)
#define atomic_fetch_and_explicit(object, operand, order)                      \
	__stdc_clang_fetch(__c11_atomic_fetch_and,                             \
	    __atomic_fallback_fetch_and, object, operand, order)

#else /* GCC and other compilers with the GCC __atomic_* interface */

/*
 * The GCC __atomic_* builtins take the address of the result instead of
 * returning it.  The temporaries are ordinary (non-atomic) objects, so the
 * comma operator in __typeof__((void)0, *p) is used to obtain the plain
 * value type of the atomic object.
 *
 * __atomic_fetch_add/sub on a pointer object: GCC's builtin adds the operand
 * as a raw value ("as an integer of the width of the pointer"), while ISO C
 * requires pointer arithmetic; a pointer object (always lock-free) is
 * detected with __builtin_classify_type and served by a compare-exchange
 * loop.  Integer objects keep the single-instruction builtin.
 */
#define atomic_store_explicit(object, desired, order)                          \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__atomic_store(__stdc_o, &__stdc_v, (order));          \
		else                                                           \
			__atomic_fallback_store(                               \
			    sizeof(__stdc_v), __stdc_o, &__stdc_v, (order));   \
	})
#define atomic_load_explicit(object, order)                                    \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__atomic_load(__stdc_o, &__stdc_v, (order));           \
		else                                                           \
			__atomic_fallback_load(                                \
			    sizeof(__stdc_v), __stdc_o, &__stdc_v, (order));   \
		__stdc_v;                                                      \
	})
#define atomic_exchange_explicit(object, desired, order)                       \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		__typeof__((void)0, *__stdc_o) __stdc_r;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__atomic_exchange(                                     \
			    __stdc_o, &__stdc_v, &__stdc_r, (order));          \
		else                                                           \
			__atomic_fallback_exchange(sizeof(__stdc_v), __stdc_o, \
			    &__stdc_v, &__stdc_r, (order));                    \
		__stdc_r;                                                      \
	})
#define atomic_compare_exchange_strong_explicit(                               \
    object, expected, desired, success, failure)                               \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		int __stdc_r;                                                  \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_r = __atomic_compare_exchange(__stdc_o,         \
			    (expected), &__stdc_v, 0, (success), (failure));   \
		else                                                           \
			__stdc_r = __atomic_fallback_compare_exchange(         \
			    sizeof(__stdc_v), __stdc_o, (expected), &__stdc_v, \
			    0, (success), (failure));                          \
		(__stdc_atomic_bool) __stdc_r;                                 \
	})
#define atomic_compare_exchange_weak_explicit(                                 \
    object, expected, desired, success, failure)                               \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_v = (desired);           \
		int __stdc_r;                                                  \
		if (__atomic_always_lock_free(sizeof(__stdc_v), 0))            \
			__stdc_r = __atomic_compare_exchange(__stdc_o,         \
			    (expected), &__stdc_v, 1, (success), (failure));   \
		else                                                           \
			__stdc_r = __atomic_fallback_compare_exchange(         \
			    sizeof(__stdc_v), __stdc_o, (expected), &__stdc_v, \
			    1, (success), (failure));                          \
		(__stdc_atomic_bool) __stdc_r;                                 \
	})
#define __stdc_ptr_fetch(object, operand, order, op)                           \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_old =                    \
		    atomic_load_explicit(__stdc_o, memory_order_relaxed);      \
		__typeof__((void)0, *__stdc_o) __stdc_new;                     \
		do {                                                           \
			__stdc_new = __stdc_old op(operand);                   \
		} while (!atomic_compare_exchange_weak_explicit(__stdc_o,      \
		    &__stdc_old, __stdc_new, (order), memory_order_relaxed));  \
		__stdc_old;                                                    \
	})
#define __stdc_gcc_fetch(name, fallback, pointer_expr, object, operand, order) \
	__extension__({                                                        \
		__auto_type __stdc_o = (object);                               \
		__typeof__((void)0, *__stdc_o) __stdc_r;                       \
		if (__atomic_always_lock_free(sizeof(__stdc_r), 0))            \
			__stdc_r = __builtin_choose_expr(                      \
			    __builtin_classify_type(*__stdc_o) == 5,           \
			    (pointer_expr),                                    \
			    name(__stdc_o, (operand), (order)));               \
		else {                                                         \
			__typeof__((void)0, *__stdc_o) __stdc_v =              \
			    (__typeof__((void)0, *__stdc_o))(operand);         \
			fallback(sizeof(__stdc_r), __stdc_o, &__stdc_v,        \
			    &__stdc_r, (order));                               \
		}                                                              \
		__stdc_r;                                                      \
	})
#define atomic_fetch_add_explicit(object, operand, order)                      \
	__stdc_gcc_fetch(__atomic_fetch_add, __atomic_fallback_fetch_add,      \
	    __stdc_ptr_fetch(object, operand, order, +), object, operand,      \
	    order)
#define atomic_fetch_sub_explicit(object, operand, order)                      \
	__stdc_gcc_fetch(__atomic_fetch_sub, __atomic_fallback_fetch_sub,      \
	    __stdc_ptr_fetch(object, operand, order, -), object, operand,      \
	    order)
#define atomic_fetch_or_explicit(object, operand, order)                       \
	__stdc_gcc_fetch(__atomic_fetch_or, __atomic_fallback_fetch_or,        \
	    __atomic_fetch_or((object), (operand), (order)), object, operand,  \
	    order)
#define atomic_fetch_xor_explicit(object, operand, order)                      \
	__stdc_gcc_fetch(__atomic_fetch_xor, __atomic_fallback_fetch_xor,      \
	    __atomic_fetch_xor((object), (operand), (order)), object, operand, \
	    order)
#define atomic_fetch_and_explicit(object, operand, order)                      \
	__stdc_gcc_fetch(__atomic_fetch_and, __atomic_fallback_fetch_and,      \
	    __atomic_fetch_and((object), (operand), (order)), object, operand, \
	    order)

#endif /* __clang__ */

#define atomic_store(object, desired)                                          \
	atomic_store_explicit((object), (desired), memory_order_seq_cst)
#define atomic_load(object) atomic_load_explicit((object), memory_order_seq_cst)
#define atomic_exchange(object, desired)                                       \
	atomic_exchange_explicit((object), (desired), memory_order_seq_cst)
#define atomic_compare_exchange_strong(object, expected, desired)              \
	atomic_compare_exchange_strong_explicit((object), (expected),          \
	    (desired), memory_order_seq_cst, memory_order_seq_cst)
#define atomic_compare_exchange_weak(object, expected, desired)                \
	atomic_compare_exchange_weak_explicit((object), (expected), (desired), \
	    memory_order_seq_cst, memory_order_seq_cst)
#define atomic_fetch_add(object, operand)                                      \
	atomic_fetch_add_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_sub(object, operand)                                      \
	atomic_fetch_sub_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_or(object, operand)                                       \
	atomic_fetch_or_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_xor(object, operand)                                      \
	atomic_fetch_xor_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_and(object, operand)                                      \
	atomic_fetch_and_explicit((object), (operand), memory_order_seq_cst)

#endif /* !_STDATOMIC_H_ */
