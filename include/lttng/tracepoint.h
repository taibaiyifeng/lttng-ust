#ifndef _LTTNG_TRACEPOINT_H
#define _LTTNG_TRACEPOINT_H

/*
 * Copyright (c) 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include <lttng/tracepoint-types.h>
#include <lttng/tracepoint-rcu.h>
#include <urcu/compiler.h>
#include <dlfcn.h>	/* for dlopen */
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define tracepoint(provider, name, ...)					    \
	do {								    \
		if (caa_unlikely(__tracepoint_##provider##___##name.state)) \
			__tracepoint_cb_##provider##___##name(__VA_ARGS__); \
	} while (0)

#define TP_ARGS(...)       __VA_ARGS__

/*
 * TP_ARGS takes tuples of type, argument separated by a comma.
 * It can take up to 10 tuples (which means that less than 10 tuples is
 * fine too).
 * Each tuple is also separated by a comma.
 */
#define __TP_COMBINE_TOKENS(_tokena, _tokenb)				\
		_tokena##_tokenb
#define _TP_COMBINE_TOKENS(_tokena, _tokenb)				\
		__TP_COMBINE_TOKENS(_tokena, _tokenb)
#define __TP_COMBINE_TOKENS3(_tokena, _tokenb, _tokenc)			\
		_tokena##_tokenb##_tokenc
#define _TP_COMBINE_TOKENS3(_tokena, _tokenb, _tokenc)			\
		__TP_COMBINE_TOKENS3(_tokena, _tokenb, _tokenc)
#define __TP_COMBINE_TOKENS4(_tokena, _tokenb, _tokenc, _tokend)	\
		_tokena##_tokenb##_tokenc##_tokend
#define _TP_COMBINE_TOKENS4(_tokena, _tokenb, _tokenc, _tokend)		\
		__TP_COMBINE_TOKENS4(_tokena, _tokenb, _tokenc, _tokend)

/* _TP_EXVAR* extract the var names. */
#define _TP_EXVAR0()
#define _TP_EXVAR2(a,b)						b
#define _TP_EXVAR4(a,b,c,d)					b,d
#define _TP_EXVAR6(a,b,c,d,e,f)					b,d,f
#define _TP_EXVAR8(a,b,c,d,e,f,g,h)				b,d,f,h
#define _TP_EXVAR10(a,b,c,d,e,f,g,h,i,j)			b,d,f,h,j
#define _TP_EXVAR12(a,b,c,d,e,f,g,h,i,j,k,l)			b,d,f,h,j,l
#define _TP_EXVAR14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)		b,d,f,h,j,l,n
#define _TP_EXVAR16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)		b,d,f,h,j,l,n,p
#define _TP_EXVAR18(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r)	b,d,f,h,j,l,n,p,r
#define _TP_EXVAR20(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t)	b,d,f,h,j,l,n,p,r,t

#define _TP_EXDATA_VAR0()						__tp_data
#define _TP_EXDATA_VAR2(a,b)						__tp_data,b
#define _TP_EXDATA_VAR4(a,b,c,d)					__tp_data,b,d
#define _TP_EXDATA_VAR6(a,b,c,d,e,f)					__tp_data,b,d,f
#define _TP_EXDATA_VAR8(a,b,c,d,e,f,g,h)				__tp_data,b,d,f,h
#define _TP_EXDATA_VAR10(a,b,c,d,e,f,g,h,i,j)				__tp_data,b,d,f,h,j
#define _TP_EXDATA_VAR12(a,b,c,d,e,f,g,h,i,j,k,l)			__tp_data,b,d,f,h,j,l
#define _TP_EXDATA_VAR14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)			__tp_data,b,d,f,h,j,l,n
#define _TP_EXDATA_VAR16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)		__tp_data,b,d,f,h,j,l,n,p
#define _TP_EXDATA_VAR18(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r)		__tp_data,b,d,f,h,j,l,n,p,r
#define _TP_EXDATA_VAR20(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t)	__tp_data,b,d,f,h,j,l,n,p,r,t

/* _TP_EXPROTO* extract tuples of type, var */
#define _TP_EXPROTO0()
#define _TP_EXPROTO2(a,b)					a b
#define _TP_EXPROTO4(a,b,c,d)					a b,c d
#define _TP_EXPROTO6(a,b,c,d,e,f)				a b,c d,e f
#define _TP_EXPROTO8(a,b,c,d,e,f,g,h)				a b,c d,e f,g h
#define _TP_EXPROTO10(a,b,c,d,e,f,g,h,i,j)			a b,c d,e f,g h,i j
#define _TP_EXPROTO12(a,b,c,d,e,f,g,h,i,j,k,l)			a b,c d,e f,g h,i j,k l
#define _TP_EXPROTO14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)		a b,c d,e f,g h,i j,k l,m n
#define _TP_EXPROTO16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)		a b,c d,e f,g h,i j,k l,m n,o p
#define _TP_EXPROTO18(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r)	a b,c d,e f,g h,i j,k l,m n,o p,q r
#define _TP_EXPROTO20(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t)	a b,c d,e f,g h,i j,k l,m n,o p,q r,s t

#define _TP_EXDATA_PROTO0()						void *__tp_data
#define _TP_EXDATA_PROTO2(a,b)						void *__tp_data,a b
#define _TP_EXDATA_PROTO4(a,b,c,d)					void *__tp_data,a b,c d
#define _TP_EXDATA_PROTO6(a,b,c,d,e,f)					void *__tp_data,a b,c d,e f
#define _TP_EXDATA_PROTO8(a,b,c,d,e,f,g,h)				void *__tp_data,a b,c d,e f,g h
#define _TP_EXDATA_PROTO10(a,b,c,d,e,f,g,h,i,j)				void *__tp_data,a b,c d,e f,g h,i j
#define _TP_EXDATA_PROTO12(a,b,c,d,e,f,g,h,i,j,k,l)			void *__tp_data,a b,c d,e f,g h,i j,k l
#define _TP_EXDATA_PROTO14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)			void *__tp_data,a b,c d,e f,g h,i j,k l,m n
#define _TP_EXDATA_PROTO16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)		void *__tp_data,a b,c d,e f,g h,i j,k l,m n,o p
#define _TP_EXDATA_PROTO18(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r)		void *__tp_data,a b,c d,e f,g h,i j,k l,m n,o p,q r
#define _TP_EXDATA_PROTO20(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t)	void *__tp_data,a b,c d,e f,g h,i j,k l,m n,o p,q r,s t

/* Preprocessor trick to count arguments. Inspired from sdt.h. */
#define _TP_NARGS(...)			__TP_NARGS(__VA_ARGS__, 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define __TP_NARGS(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, N, ...)	N
#define _TP_PROTO_N(N, ...)		_TP_PARAMS(_TP_COMBINE_TOKENS(_TP_EXPROTO, N)(__VA_ARGS__))
#define _TP_VAR_N(N, ...)		_TP_PARAMS(_TP_COMBINE_TOKENS(_TP_EXVAR, N)(__VA_ARGS__))
#define _TP_DATA_PROTO_N(N, ...)	_TP_PARAMS(_TP_COMBINE_TOKENS(_TP_EXDATA_PROTO, N)(__VA_ARGS__))
#define _TP_DATA_VAR_N(N, ...)		_TP_PARAMS(_TP_COMBINE_TOKENS(_TP_EXDATA_VAR, N)(__VA_ARGS__))
#define _TP_ARGS_PROTO(...)		_TP_PROTO_N(_TP_NARGS(0, ##__VA_ARGS__), ##__VA_ARGS__)
#define _TP_ARGS_VAR(...)		_TP_VAR_N(_TP_NARGS(0, ##__VA_ARGS__), ##__VA_ARGS__)
#define _TP_ARGS_DATA_PROTO(...)	_TP_DATA_PROTO_N(_TP_NARGS(0, ##__VA_ARGS__), ##__VA_ARGS__)
#define _TP_ARGS_DATA_VAR(...)		_TP_DATA_VAR_N(_TP_NARGS(0, ##__VA_ARGS__), ##__VA_ARGS__)
#define _TP_PARAMS(...)			__VA_ARGS__

#define _DECLARE_TRACEPOINT(provider, name, ...)			 		\
extern struct tracepoint __tracepoint_##provider##___##name;				\
static inline void __tracepoint_cb_##provider##___##name(_TP_ARGS_PROTO(__VA_ARGS__))	\
{											\
	struct tracepoint_probe *__tp_probe;						\
											\
	if (!TP_RCU_LINK_TEST())							\
		return;									\
	tp_rcu_read_lock_bp();								\
	__tp_probe = tp_rcu_dereference_bp(__tracepoint_##provider##___##name.probes);	\
	if (caa_unlikely(!__tp_probe))							\
		goto end;								\
	do {										\
		void *__tp_cb =	__tp_probe->func;					\
		void *__tp_data = __tp_probe->data;					\
											\
		URCU_FORCE_CAST(void (*)(_TP_ARGS_DATA_PROTO(__VA_ARGS__)), __tp_cb)	\
				(_TP_ARGS_DATA_VAR(__VA_ARGS__));			\
	} while ((++__tp_probe)->func);							\
end:											\
	tp_rcu_read_unlock_bp();							\
}											\
static inline void __tracepoint_register_##provider##___##name(char *name,		\
		void *func, void *data)							\
{											\
	__tracepoint_probe_register(name, func, data);					\
}											\
static inline void __tracepoint_unregister_##provider##___##name(char *name,		\
		void *func, void *data)							\
{											\
	__tracepoint_probe_unregister(name, func, data);				\
}

extern int __tracepoint_probe_register(const char *name, void *func, void *data);
extern int __tracepoint_probe_unregister(const char *name, void *func, void *data);

#ifdef TRACEPOINT_DEFINE

/*
 * Note: to allow PIC code, we need to allow the linker to update the pointers
 * in the __tracepoints_ptrs section.
 * Therefore, this section is _not_ const (read-only).
 */
#define _DEFINE_TRACEPOINT(provider, name)					\
	static const char __tp_strtab_##provider##___##name[]			\
		__attribute__((section("__tracepoints_strings"))) =		\
			#provider ":" #name;					\
	struct tracepoint __tracepoint_##provider##___##name			\
		__attribute__((section("__tracepoints"))) =			\
			{ __tp_strtab_##provider##___##name, 0, NULL };		\
	static struct tracepoint * __tracepoint_ptr_##provider##___##name	\
		__attribute__((used, section("__tracepoints_ptrs"))) =		\
			&__tracepoint_##provider##___##name;

static int (*tracepoint_register_lib)(struct tracepoint * const *tracepoints_start,
		int tracepoints_count);
static int (*tracepoint_unregister_lib)(struct tracepoint * const *tracepoints_start);
static void *liblttngust_handle;

/*
 * These weak symbols, the constructor, and destructor take care of
 * registering only _one_ instance of the tracepoints per shared-ojbect
 * (or for the whole main program).
 */
extern struct tracepoint * const __start___tracepoints_ptrs[]
	__attribute__((weak, visibility("hidden")));
extern struct tracepoint * const __stop___tracepoints_ptrs[]
	__attribute__((weak, visibility("hidden")));
int __tracepoint_registered
	__attribute__((weak, visibility("hidden")));

static void __attribute__((constructor)) __tracepoints__init(void)
{
	if (__tracepoint_registered++)
		return;

	liblttngust_handle = dlopen("liblttng-ust.so", RTLD_NOW | RTLD_GLOBAL);
	if (!liblttngust_handle)
		return;
	tracepoint_register_lib =
		URCU_FORCE_CAST(int (*)(struct tracepoint * const *, int),
				dlsym(liblttngust_handle,
					"tracepoint_register_lib"));
	tracepoint_unregister_lib =
		URCU_FORCE_CAST(int (*)(struct tracepoint * const *),
				dlsym(liblttngust_handle,
					"tracepoint_unregister_lib"));
#ifndef _LGPL_SOURCE
	tp_rcu_read_lock_bp =
		URCU_FORCE_CAST(void (*)(void),
				dlsym(liblttngust_handle,
					"tp_rcu_read_lock_bp"));
	tp_rcu_read_unlock_bp =
		URCU_FORCE_CAST(void (*)(void),
				dlsym(liblttngust_handle,
					"tp_rcu_read_unlock_bp"));
	tp_rcu_dereference_sym_bp =
		URCU_FORCE_CAST(void *(*)(void *p),
				dlsym(liblttngust_handle,
					"tp_rcu_dereference_sym_bp"));
#endif
	tracepoint_register_lib(__start___tracepoints_ptrs,
				__stop___tracepoints_ptrs -
				__start___tracepoints_ptrs);
}

static void __attribute__((destructor)) __tracepoints__destroy(void)
{
	int ret;
	if (--__tracepoint_registered)
		return;
	if (tracepoint_unregister_lib)
		tracepoint_unregister_lib(__start___tracepoints_ptrs);
	if (liblttngust_handle) {
		tracepoint_unregister_lib = NULL;
		tracepoint_register_lib = NULL;
		ret = dlclose(liblttngust_handle);
		assert(!ret);
	}
}

#else /* TRACEPOINT_DEFINE */

#define _DEFINE_TRACEPOINT(provider, name)

#endif /* #else TRACEPOINT_DEFINE */

#ifdef __cplusplus
}
#endif

#endif /* _LTTNG_TRACEPOINT_H */

/* The following declarations must be outside re-inclusion protection. */

#ifndef TRACEPOINT_EVENT

/*
 * How to use the TRACEPOINT_EVENT macro:
 *
 * An example:
 * 
 * TRACEPOINT_EVENT(someproject_component, event_name,
 *
 *     * TP_ARGS takes from 0 to 10 "type, field_name" pairs *
 *
 *     TP_ARGS(int, arg0, void *, arg1, char *, string, size_t, strlen,
 *             long *, arg4, size_t, arg4_len),
 *
 *	* TP_FIELDS describes the event payload layout in the trace *
 *
 *     TP_FIELDS(
 *         * Integer, printed in base 10 * 
 *         ctf_integer(int, field_a, arg0)
 *
 *         * Integer, printed with 0x base 16 * 
 *         ctf_integer_hex(unsigned long, field_d, arg1)
 *
 *         * Array Sequence, printed as UTF8-encoded array of bytes * 
 *         ctf_array_text(char, field_b, string, FIXED_LEN)
 *         ctf_sequence_text(char, field_c, string, size_t, strlen)
 *
 *         * String, printed as UTF8-encoded string * 
 *         ctf_string(field_e, string)
 *
 *         * Array sequence of signed integer values * 
 *         ctf_array(long, field_f, arg4, FIXED_LEN4)
 *         ctf_sequence(long, field_g, arg4, size_t, arg4_len)
 *     )
 * )
 *
 * More detailed explanation:
 *
 * The name of the tracepoint is expressed as a tuple with the provider
 * and name arguments.
 *
 * The provider and name should be a proper C99 identifier.
 * The "provider" and "name" MUST follow these rules to ensure no
 * namespace clash occurs:
 *
 * For projects (applications and libraries) for which an entity
 * specific to the project controls the source code and thus its
 * tracepoints (typically with a scope larger than a single company):
 *
 * either:
 *   project_component, event
 * or:
 *   project, event
 *
 * Where "project" is the name of the project,
 *       "component" is the name of the project component (which may
 *       include several levels of sub-components, e.g.
 *       ...component_subcomponent_...) where the tracepoint is located
 *       (optional),
 *       "event" is the name of the tracepoint event.
 *
 * For projects issued from a single company wishing to advertise that
 * the company controls the source code and thus the tracepoints, the
 * "com_" prefix should be used:
 *
 * either:
 *   com_company_project_component, event
 * or:
 *   com_company_project, event
 *
 * Where "company" is the name of the company,
 *       "project" is the name of the project,
 *       "component" is the name of the project component (which may
 *       include several levels of sub-components, e.g.
 *       ...component_subcomponent_...) where the tracepoint is located
 *       (optional),
 *       "event" is the name of the tracepoint event.
 *
 * the provider:event identifier is limited to 127 characters.
 */

#define TRACEPOINT_EVENT(provider, name, args, fields)			\
	_DECLARE_TRACEPOINT(provider, name, _TP_PARAMS(args))		\
	_DEFINE_TRACEPOINT(provider, name)

#define TRACEPOINT_EVENT_CLASS(provider, name, args, fields)

#define TRACEPOINT_EVENT_INSTANCE(provider, _template, name, args)	\
	_DECLARE_TRACEPOINT(provider, name, _TP_PARAMS(args))		\
	_DEFINE_TRACEPOINT(provider, name)

#endif /* #ifndef TRACEPOINT_EVENT */

#ifndef TRACEPOINT_LOGLEVEL

/*
 * Tracepoint Loglevel Declaration Facility
 *
 * This is a place-holder the tracepoint loglevel declaration,
 * overridden by the tracer implementation.
 *
 * Typical use of these loglevels:
 *
 * 1) Declare the mapping between loglevel names and an integer values
 *    within TRACEPOINT_LOGLEVEL_ENUM(), using tp_loglevel() for each
 *    tuple. Do _NOT_ add comma (,) nor semicolon (;) between the
 *    tp_loglevel entries contained within TRACEPOINT_LOGLEVEL_ENUM().
 *    Do _NOT_ add comma (,) nor semicolon (;) after the
 *    TRACEPOINT_LOGLEVEL_ENUM() declaration.  The name should be a
 *    proper C99 identifier.
 *
 *      TRACEPOINT_LOGLEVEL_ENUM(
 *              tp_loglevel( < loglevel_name >, < value > )
 *              tp_loglevel( < loglevel_name >, < value > )
 *              ...
 *      )
 *
 *    e.g.:
 *
 *      TRACEPOINT_LOGLEVEL_ENUM(
 *              tp_loglevel(LOG_EMERG,   0)
 *              tp_loglevel(LOG_ALERT,   1)
 *              tp_loglevel(LOG_CRIT,    2)
 *              tp_loglevel(LOG_ERR,     3)
 *              tp_loglevel(LOG_WARNING, 4)
 *              tp_loglevel(LOG_NOTICE,  5)
 *              tp_loglevel(LOG_INFO,    6)
 *              tp_loglevel(LOG_DEBUG,   7)
 *      )
 *
 * 2) Then, declare tracepoint loglevels for tracepoints. A
 *    TRACEPOINT_EVENT should be declared prior to the the
 *    TRACEPOINT_LOGLEVEL for a given tracepoint name. The first field
 *    is the name of the tracepoint, the second field is the loglevel
 *    name.
 *
 *      TRACEPOINT_LOGLEVEL(< [com_company_]project[_component] >, < event >,
 *              < loglevel_name >)
 *
 * The TRACEPOINT_PROVIDER must be defined when declaring a
 * TRACEPOINT_LOGLEVEL_ENUM and TRACEPOINT_LOGLEVEL. The tracepoint
 * loglevel enumeration apply to the entire TRACEPOINT_PROVIDER. Only one
 * tracepoint loglevel enumeration should be declared per tracepoint
 * provider.
 */

#define TRACEPOINT_LOGLEVEL_ENUM(...)
#define TRACEPOINT_LOGLEVEL(provider, name, loglevel)

#endif /* #ifndef TRACEPOINT_LOGLEVEL */