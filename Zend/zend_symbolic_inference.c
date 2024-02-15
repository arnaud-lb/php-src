/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Arnaud Le Blanc <arnaud.lb@gmail.com>                       |
   +----------------------------------------------------------------------+
*/

/* This implements type inference for constructor arguments.
 *
 * The algorithm is unsound by design. Most notably, it ignores references.
 *
 * This is implemented as a forward data flow analysis based on zend_inference.c
 *
 * Types that can not be resolved at compile time, such as the return type of
 * function calls (for functions declared in other compilation units) are
 * represented as symbols. E.g. the type of the expression `f(1)` is represented
 * as `fcall<f,1>`. Such types can then be resolved at runtime.
 *
 * Symbolic types may represent an infinity of types and never converge. E.g.
 * the variable $a in the following code will successively have the types
 * fcall<f,int>, fcall<f,fcall<f,int>|int>, and so on:
 *
 * $a = 1;
 * while ($i--) { $a = f($a); }
 *
 * We handle these by using a variation of the algorithm decribed in:
 * Weiss G, Schonberg E. Typefinding recursive structures: A data-flow analysis
 * in the presence of infinite type sets. New York University. Courant Institute
 * of Mathematical Sciences. Computer Science Department; 1986.
 *
 * This can be summarized as follows: Uses appearing in the same SCC as an
 * opcode defs are considered recursive, and represented with a placeholder.
 *
 * After a fixpoint is reached we resolve the placeholders. Based on the
 * assumption that operations are indempotent (type wise), we can eliminate
 * recursivity: fcall<f,$v1> with $v1=fcall<f,$v1>|int is equivalent to
 * fcall<f,fcall<int>|int>.
 */

#include "zend_arena.h"
#include "zend_closures.h"
#include "zend_compile.h"
#include "zend_operators.h"
#include "zend_portability.h"
#include "zend_string.h"
#include "zend_types.h"
#include "zend_type_tools.h"
#include "zend_type_info.h"
#include "zend_bitset.h"
#include "zend_vm_opcodes.h"
#include "zend_execute.h"
#include "zend_API.h"
#include "zend_constants.h"
#include <stdint.h>
#include "zend_symbolic_inference.h"
#include "Optimizer/zend_dump.h"
#include "Optimizer/zend_optimizer.h"
#include "Optimizer/zend_cfg.h"
#include "Optimizer/zend_ssa.h"
#include "Optimizer/zend_scc.h"

static const zend_op *zend_find_fcall_init_op(const zend_op_array *op_array, const zend_op *opline)
{
	int depth = 1;

	for (opline--; opline >= op_array->opcodes; opline--) {
		switch (opline->opcode) {
			case ZEND_INIT_FCALL:
			case ZEND_INIT_FCALL_BY_NAME:
			case ZEND_INIT_NS_FCALL_BY_NAME:
			case ZEND_INIT_DYNAMIC_CALL:
			case ZEND_INIT_USER_CALL:
			case ZEND_INIT_METHOD_CALL:
			case ZEND_INIT_STATIC_METHOD_CALL:
			case ZEND_NEW:
				depth--;
				if (depth == 0) {
					return opline;
				}
				break;
			case ZEND_DO_FCALL:
			case ZEND_DO_ICALL:
			case ZEND_DO_UCALL:
			case ZEND_DO_FCALL_BY_NAME:
				depth++;
				break;
		}
	}

	ZEND_UNREACHABLE();
}

static zend_always_inline zend_type _const_op_type(const zval *zv) {
	uint32_t mask;
	if (Z_TYPE_P(zv) == IS_CONSTANT_AST) {
		// TODO
		mask = MAY_BE_ANY;
	} else if (Z_TYPE_P(zv) == IS_PNR) {
		mask = MAY_BE_STRING;
	} else if (Z_TYPE_P(zv) == IS_ARRAY) {
		mask = MAY_BE_ARRAY;
	} else {
		mask = 1 << Z_TYPE_P(zv);
	}

	return (zend_type) ZEND_TYPE_INIT_MASK(mask);
}

#define _ZEND_TYPE_SSA_VAR_BIT	(1 << _ZEND_TYPE_EXTRA_FLAGS_SHIFT)
#define _ZEND_TYPE_SYMBOLIC_BIT (1 << (_ZEND_TYPE_EXTRA_FLAGS_SHIFT+1))

#define ZEND_TYPE_IS_SSA_VAR(t) (((t).type_mask & _ZEND_TYPE_SSA_VAR_BIT) != 0)
#define ZEND_TYPE_SSA_VAR(t) ((int)(uintptr_t)(t).ptr)

#define ZEND_TYPE_IS_SYMBOLIC(t) (((t).type_mask & _ZEND_TYPE_SYMBOLIC_BIT) != 0)

#define ZEND_TYPE_INIT_SYMBOLIC(list, extra_flags) \
	ZEND_TYPE_INIT_PTR(list, _ZEND_TYPE_SYMBOLIC_BIT | _ZEND_TYPE_LIST_BIT, 0, extra_flags)

#define SYMTYPE_BINOP               1
#define SYMTYPE_FCALL               2
#define SYMTYPE_NS_FCALL            3
#define SYMTYPE_METHOD_CALL         4
#define SYMTYPE_STATIC_METHOD_CALL  5
#define SYMTYPE_NEW                 6
#define SYMTYPE_NOT_NULL            7
#define SYMTYPE_PROP                8
#define SYMTYPE_CLASS_CONSTANT      9
#define SYMTYPE_CONSTANT			10
#define SYMTYPE_CLASS				11

#define BINOP_NUMBER_ARRAY  1
#define BINOP_NUMBER        2
#define BINOP_LONG_STRING   3
#define BINOP_STRING        4
#define BINOP_LONG          5

static zend_type_list *zend_symbolic_alloc(zend_arena **arena, int symtype, uint32_t num_types)
{
	zend_type_list *list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(num_types));
	list->num_types = num_types;
	ZEND_TYPE_FULL_MASK(list->types[0]) = symtype;
	return list;
}

static zend_type zend_symbolic_binop(zend_arena **arena, zend_type op1, zend_type op2, uint32_t type_mask)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_BINOP, 4);
	ZEND_TYPE_FULL_MASK(list->types[1]) = type_mask;
	list->types[2] = op1;
	list->types[3] = op2;
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

static zend_type zend_symbolic_binop_auto(zend_arena **arena, size_t opcode, zend_type op1, zend_type op2)
{
	switch (opcode) {
		case ZEND_ADD:
			return zend_symbolic_binop(arena, op1, op2, BINOP_NUMBER_ARRAY);
		case ZEND_MUL:
		case ZEND_DIV:
		case ZEND_SUB:
		case ZEND_POW:
			return zend_symbolic_binop(arena, op1, op2, BINOP_NUMBER);
		case ZEND_BW_AND:
		case ZEND_BW_OR:
		case ZEND_BW_XOR:
		case ZEND_BW_NOT:
			return zend_symbolic_binop(arena, op1, op2, BINOP_LONG_STRING);
		case ZEND_MOD:
		case ZEND_SL:
		case ZEND_SR:
			return zend_symbolic_binop(arena, op1, op2, BINOP_LONG);
		case ZEND_CONCAT:
			return zend_symbolic_binop(arena, op1, op2, BINOP_STRING);
			break;
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}

#define SYMBOLIC_BINOP_TYPE_MASK(list) ZEND_TYPE_FULL_MASK((list)->types[1])
#define SYMBOLIC_BINOP_OP1(list) (list)->types[2]
#define SYMBOLIC_BINOP_OP2(list) (list)->types[3]

static zend_type zend_symbolic_fcall(zend_arena **arena, zend_string *fname, size_t nargs)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_FCALL, nargs + 2);
	zend_string_addref(fname);
	list->types[1] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(fname), 0, 0);
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_FCALL_FNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[1])
#define SYMBOLIC_FCALL_ARGS(list) &(list)->types[2]
#define SYMBOLIC_FCALL_NARGS(list) ((list)->num_types-2)

static zend_type zend_symbolic_ns_fcall(zend_arena **arena, zend_string *fname, zend_string *ns_fname, size_t nargs)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_NS_FCALL, nargs + 3);
	zend_string_addref(fname);
	zend_string_addref(ns_fname);
	list->types[1] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(fname), 0, 0);
	list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(ns_fname), 0, 0);
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_NS_FCALL_FNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[1])
#define SYMBOLIC_NS_FCALL_NS_FNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[2])
#define SYMBOLIC_NS_FCALL_ARGS(list) &(list)->types[3]
#define SYMBOLIC_NS_FCALL_NARGS(list) ((list)->num_types-3)

static zend_type zend_symbolic_mcall(zend_arena **arena, zend_type object, zend_string *fname, size_t nargs)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_METHOD_CALL, nargs + 3);
	ZEND_TYPE_SET_PTR(list->types[0], 0);
	zend_string_addref(fname);
	list->types[1] = object;
	list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(fname), 0, 0);
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_MCALL_FETCH_TYPE(list) ((uintptr_t)(list)->types[0].ptr)
#define SYMBOLIC_MCALL_OBJECT(list) (list)->types[1]
#define SYMBOLIC_MCALL_FNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[2])
#define SYMBOLIC_MCALL_ARGS(list) &(list)->types[3]
#define SYMBOLIC_MCALL_NARGS(list) ((list)->num_types-3)

static zend_type zend_symbolic_scall(zend_arena **arena, uint32_t fetch_type, zend_type object, zend_string *lcname, zend_string *fname, size_t nargs)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_STATIC_METHOD_CALL, nargs + 4);
	ZEND_TYPE_SET_PTR(list->types[0], (void*)(uintptr_t)fetch_type);
	zend_string_addref(fname);
	list->types[1] = object;
	list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(fname), 0, 0);
	if (lcname) {
		zend_string_addref(lcname);
		list->types[4] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(lcname), 0, 0);
	}
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_SCALL_LCNAME(list) (list)->types[4]
#define SYMBOLIC_SCALL_NARGS(list) ((list)->num_types-4)

static zend_type zend_symbolic_new(zend_arena **arena, zend_type class, zend_type *args, size_t nargs)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_NEW, nargs + 2);
	list->types[1] = class;
	for (size_t i = 0; i < nargs; i++) {
		list->types[i+2] = args[i];
	}
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_NEW_OBJECT(list) (list)->types[1]
#define SYMBOLIC_NEW_ARGS(list) &(list)->types[2]
#define SYMBOLIC_NEW_NARGS(list) ((list)->num_types-2)

static zend_type zend_symbolic_coalesce(zend_arena **arena, zend_type type)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_NOT_NULL, 2);
	list->types[1] = type;
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_NOT_NULL_OP(list) (list)->types[1]

static zend_type zend_symbolic_prop(zend_arena **arena, uint32_t fetch_type, zend_type object, zend_string *lcname, zend_string *pname)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_PROP, 4);
	ZEND_TYPE_SET_PTR(list->types[0], (void*)(uintptr_t)fetch_type);
	zend_string_addref(pname);
	list->types[1] = object;
	if (lcname) {
		zend_string_addref(lcname);
		list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(lcname), 0, 0);
	}
	list->types[3] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(pname), 0, 0);
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_PROP_OBJECT(list) (list)->types[1]
#define SYMBOLIC_PROP_LCNAME(list) (list)->types[2]
#define SYMBOLIC_PROP_PNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[3])

static zend_type zend_symbolic_class_const(zend_arena **arena, uint32_t fetch_type, zend_type object, zend_string *lcname, zend_string *cname)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_CLASS_CONSTANT, 3);
	zend_string_addref(cname);
	ZEND_TYPE_SET_PTR(list->types[0], (void*)(uintptr_t)fetch_type);
	list->types[1] = object;
	if (lcname) {
		zend_string_addref(lcname);
		list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(lcname), 0, 0);
	}
	list->types[3] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_CCONST_FETCH_TYPE(list) ((uintptr_t)(list)->types[0].ptr)
#define SYMBOLIC_CCONST_OBJECT(list) (list)->types[1]
#define SYMBOLIC_CCONST_LCNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[2])
#define SYMBOLIC_CCONST_CNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[3])

static zend_type zend_symbolic_const(zend_arena **arena, zend_string *key1, zend_string *key2)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_CONSTANT, 3);
	zend_string_addref(key1);
	list->types[1] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(key1), 0, 0);
	if (key2) {
		zend_string_addref(key2);
		list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(key2), 0, 0);
	} else {
		list->types[2] = (zend_type) ZEND_TYPE_INIT_MASK(0);
	}
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_CONST_KEY1(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[1])
#define SYMBOLIC_CONST_KEY2(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[2])

static zend_type zend_symbolic_class(zend_arena **arena, uint32_t fetch_type, zend_type object, zend_string *lcname)
{
	zend_type_list *list = zend_symbolic_alloc(arena, SYMTYPE_CLASS, 3);
	ZEND_TYPE_SET_PTR(list->types[0], (void*)(uintptr_t)fetch_type);
	list->types[1] = object;
	if (lcname) {
		zend_string_addref(lcname);
		list->types[2] = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(lcname), 0, 0);
	} else {
		list->types[2] = (zend_type) ZEND_TYPE_INIT_MASK(0);
	}
	return (zend_type) ZEND_TYPE_INIT_SYMBOLIC(list, _ZEND_TYPE_ARENA_BIT);
}

#define SYMBOLIC_CLASS_FETCH_TYPE(list) ((uintptr_t)(list)->types[0].ptr)
#define SYMBOLIC_CLASS_OBJECT(list) (list)->types[1]
#define SYMBOLIC_CLASS_LCNAME(list) ZEND_TYPE_PNR_SIMPLE_NAME((list)->types[2])

static zend_always_inline zend_type get_ssa_var_info(zend_type *ssa_var_info, int ssa_var_num)
{
	if (ssa_var_num >= 0) {
		return ssa_var_info[ssa_var_num];
	} else {
		return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
	}
}

static zend_always_inline zend_type get_ssa_recursive_var_info(zend_ssa *ssa, zend_type *ssa_var_info, int embedding_var, int embedded_var)
{
	ZEND_ASSERT(embedding_var >= 0);

	if (embedded_var >= 0) {
		zend_ssa_var *var = &ssa->vars[embedded_var];
		if (var->scc && ssa->vars[embedding_var].scc == var->scc) {
			return (zend_type) ZEND_TYPE_INIT_PTR((void*)(uintptr_t)embedded_var, _ZEND_TYPE_SSA_VAR_BIT, 0, 0);
		} else {
			return ssa_var_info[embedded_var];
		}
	} else {
		return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
	}
}

#define DEFINE_SSA_OP_INFO(opN) \
	static ZEND_ATTRIBUTE_UNUSED zend_always_inline zend_type _ssa_##opN##_info(const zend_op_array *op_array, zend_type *ssa_var_info, const zend_op *opline, const zend_ssa_op *ssa_op) \
	{																		\
		if (opline->opN##_type == IS_CONST) {							\
			return _const_op_type(CRT_CONSTANT(opline->opN)); \
		} else { \
			return get_ssa_var_info(ssa_var_info, ssa_op->opN##_use); \
		} \
	} \

#define DEFINE_SSA_OP_REC_INFO(opN) \
	static ZEND_ATTRIBUTE_UNUSED zend_always_inline zend_type _ssa_##opN##_rec_info(const zend_op_array *op_array, zend_ssa *ssa, zend_type *ssa_var_info, int var, const zend_op *opline, const zend_ssa_op *ssa_op) \
	{																		\
		if (opline->opN##_type == IS_CONST) {							\
			return _const_op_type(CRT_CONSTANT(opline->opN)); \
		} else { \
			return get_ssa_recursive_var_info(ssa, ssa_var_info, var, ssa_op->opN##_use); \
		} \
	} \

DEFINE_SSA_OP_INFO(op1)
DEFINE_SSA_OP_INFO(op2)
DEFINE_SSA_OP_INFO(result)

DEFINE_SSA_OP_REC_INFO(op1)
DEFINE_SSA_OP_REC_INFO(op2)
DEFINE_SSA_OP_REC_INFO(result)

#define OP1_INFO_EX(opline, ssa_op)  (_ssa_op1_info(op_array, ssa_var_info, opline, ssa_op))
#define OP2_INFO_EX(opline, ssa_op)  (_ssa_op2_info(op_array, ssa_var_info, opline, ssa_op))
#define OP1_INFO()           (_ssa_op1_info(op_array, ssa_var_info, opline, ssa_op))
#define OP2_INFO()           (_ssa_op2_info(op_array, ssa_var_info, opline, ssa_op))
#define OP1_DATA_INFO()      (_ssa_op1_info(op_array, ssa_var_info, (opline+1), (ssa_op+1)))
#define OP2_DATA_INFO()      (_ssa_op2_info(op_array, ssa_var_info, (opline+1), (ssa_op+1)))
#define RES_USE_INFO()       (_ssa_result_info(op_array, ssa_var_info, opline, ssa_op))

/* Recursive-aware variants: Return a placeholder if the variable is in the same
 * SCC as result_var */
#define OP1_REC_INFO_EX(opline, ssa_op, result_var) (_ssa_op1_rec_info(op_array, ssa, ssa_var_info, result_var, opline, ssa_op))
#define OP2_REC_INFO_EX(opline, ssa_op, result_var) (_ssa_op2_rec_info(op_array, ssa, ssa_var_info, result_var, opline, ssa_op))
#define OP1_DATA_REC_INFO_EX(opline, ssa_op, result_var) (_ssa_op1_rec_info(op_array, ssa, ssa_var_info, result_var, opline, ssa_op))
#define OP1_REC_INFO()		 (_ssa_op1_rec_info(op_array, ssa, ssa_var_info, ssa_op->result_def, opline, ssa_op))
#define OP2_REC_INFO()	     (_ssa_op2_rec_info(op_array, ssa, ssa_var_info, ssa_op->result_def, opline, ssa_op))
#define OP1_DATA_REC_INFO()  (_ssa_op1_rec_info(op_array, ssa, ssa_var_info, ssa_op->result_def, (opline+1), (ssa_op+1)))

#define UPDATE_SSA_TYPE(_type, _var)									\
	do {																\
		uint32_t __type = (_type);										\
		int __var = (_var);												\
		if (__var >= 0) {												\
			if (ZEND_TYPE_FULL_MASK(ssa_var_info[__var]) != __type) {	\
				ZEND_TYPE_FULL_MASK(ssa_var_info[__var]) = __type;							\
				add_usages(op_array, ssa, worklist, __var);				\
			}															\
			/*zend_bitset_excl(worklist, var);*/						\
		}																\
	} while (0)

#define UPDATE_SSA_TYPE_EX(_type, _var)									\
	do {																\
		zend_type __type = (_type);										\
		int __var = (_var);												\
		if (__var >= 0) {												\
			if (!zend_type_equals(ssa_var_info[__var], __type)) {		\
				ssa_var_info[__var] = __type;							\
				add_usages(op_array, ssa, worklist, __var);				\
			}															\
			/*zend_bitset_excl(worklist, var);*/						\
		}																\
	} while (0)

static void add_usages(const zend_op_array *op_array, zend_ssa *ssa, zend_bitset worklist, int var)
{
	if (ssa->vars[var].phi_use_chain) {
		zend_ssa_phi *p = ssa->vars[var].phi_use_chain;
		do {
			zend_bitset_incl(worklist, p->ssa_var);
			p = zend_ssa_next_use_phi(ssa, var, p);
		} while (p);
	}
	if (ssa->vars[var].use_chain >= 0) {
		int use = ssa->vars[var].use_chain;
		zend_ssa_op *op;

		do {
			op = ssa->ops + use;
			if (op->result_def >= 0) {
				zend_bitset_incl(worklist, op->result_def);
			}
			if (op->op1_def >= 0) {
				zend_bitset_incl(worklist, op->op1_def);
			}
			if (op->op2_def >= 0) {
				zend_bitset_incl(worklist, op->op2_def);
			}
			if (op_array->opcodes[use].opcode == ZEND_OP_DATA) {
				op--;
				if (op->result_def >= 0) {
					zend_bitset_incl(worklist, op->result_def);
				}
				if (op->op1_def >= 0) {
					zend_bitset_incl(worklist, op->op1_def);
				}
				if (op->op2_def >= 0) {
					zend_bitset_incl(worklist, op->op2_def);
				}
			} else if (use + 1 < op_array->last
			 && op_array->opcodes[use + 1].opcode == ZEND_OP_DATA) {
				op++;
				if (op->result_def >= 0) {
					zend_bitset_incl(worklist, op->result_def);
				}
				if (op->op1_def >= 0) {
					zend_bitset_incl(worklist, op->op1_def);
				}
				if (op->op2_def >= 0) {
					zend_bitset_incl(worklist, op->op2_def);
				}
			}
			use = zend_ssa_next_use(ssa->ops, var, use);
		} while (use >= 0);
	}
}

static void zend_get_fcall_arg_types(zend_arena **arena, zend_ssa *ssa, zend_type *ssa_var_info, const zend_op_array *op_array, const zend_op *opline, int result_var, zend_type *args, int nargs)
{
	int depth = 1;
	int arg = 0;

	ZEND_ASSERT(result_var >= 0);

	for (int i = opline - op_array->opcodes + 1; i < op_array->last; i++) {
		zend_op *opline = &op_array->opcodes[i];
		switch (opline->opcode) {
			case ZEND_INIT_FCALL:
			case ZEND_INIT_FCALL_BY_NAME:
			case ZEND_INIT_NS_FCALL_BY_NAME:
			case ZEND_INIT_DYNAMIC_CALL:
			case ZEND_INIT_USER_CALL:
			case ZEND_INIT_METHOD_CALL:
			case ZEND_INIT_STATIC_METHOD_CALL:
			case ZEND_NEW:
				depth++;
				break;
			case ZEND_DO_FCALL:
			case ZEND_DO_ICALL:
			case ZEND_DO_UCALL:
			case ZEND_DO_FCALL_BY_NAME:
				depth--;
				break;
			case ZEND_SEND_VAL:
			case ZEND_SEND_VAL_EX:
			case ZEND_SEND_VAR:
			case ZEND_SEND_VAR_EX:
			case ZEND_SEND_FUNC_ARG:
			case ZEND_SEND_REF:
			case ZEND_SEND_VAR_NO_REF:
			case ZEND_SEND_VAR_NO_REF_EX:
			case ZEND_SEND_USER:
				if (depth == 1) {
					zend_ssa_op *ssa_op = &ssa->ops[opline - op_array->opcodes];
					args[arg] = OP1_REC_INFO_EX(opline, ssa_op, result_var);
					arg++;
				}
				break;
		}
		if (depth == 0) {
			break;
		}
	}

	ZEND_ASSERT(arg == nargs);
}

static bool zend_type_equals(zend_type a, zend_type b)
{
	if (ZEND_TYPE_FULL_MASK(a) != ZEND_TYPE_FULL_MASK(b)) {
		return false;
	}

	if (!ZEND_TYPE_IS_COMPLEX(a) && !ZEND_TYPE_IS_SSA_VAR(a)) {
		return true;
	}

	if (a.ptr == b.ptr) {
		return true;
	}

	if (ZEND_TYPE_IS_SSA_VAR(a)) {
		return false;
	}

	if (ZEND_TYPE_HAS_PNR(a)) {
		zend_packed_name_reference pnr_a = ZEND_TYPE_PNR(a);
		zend_packed_name_reference pnr_b = ZEND_TYPE_PNR(b);
		if (ZEND_PNR_IS_COMPLEX(pnr_a)) {
			if (!ZEND_PNR_IS_COMPLEX(pnr_b)) {
				return false;
			}
			zend_name_reference *ref_a = ZEND_PNR_COMPLEX_GET_REF(pnr_a);
			zend_name_reference *ref_b = ZEND_PNR_COMPLEX_GET_REF(pnr_b);
			if (!zend_string_equals(ref_a->name, ref_b->name)) {
				return false;
			}
			if (ref_a->args.num_types != ref_b->args.num_types) {
				return false;
			}
			for (uint32_t i = 0; i < ref_a->args.num_types; i++) {
				if (!zend_type_equals(ref_a->args.types[i], ref_b->args.types[i])) {
					return false;
				}
			}
			return true;
		} else {
			if (ZEND_PNR_IS_COMPLEX(pnr_b)) {
				return false;
			}
			zend_string *name_a = ZEND_PNR_SIMPLE_GET_NAME(pnr_a);
			zend_string *name_b = ZEND_PNR_SIMPLE_GET_NAME(pnr_b);
			return zend_string_equals(name_a, name_b);
		}
	}

	ZEND_ASSERT(ZEND_TYPE_HAS_LIST(a));

	zend_type_list *a_list = ZEND_TYPE_LIST(a);
	zend_type_list *b_list = ZEND_TYPE_LIST(b);
	if (a_list->num_types != b_list->num_types) {
		return false;
	}
	for (uint32_t i = 0; i < a_list->num_types; i++) {
		if (!zend_type_equals(a_list->types[i], b_list->types[i])) {
			return false;
		}
	}

	return true;
}

static bool zend_union_type_contains(zend_type container, zend_type t)
{
	if (ZEND_TYPE_IS_UNION(container)) {
		zend_type *single_type;
		ZEND_TYPE_LIST_FOREACH(ZEND_TYPE_LIST(container), single_type) {
			if (zend_type_equals(*single_type, t)) {
				return true;
			}
		} ZEND_TYPE_LIST_FOREACH_END();
		return false;
	} else {
		return zend_type_equals(container, t);
	}
}

static zend_result zend_resolve_relative_class_base(zend_arena **arena,
		uint32_t *fetch_type, zend_type *object, zend_string **lcname,
		const zend_op_array *op_array, const znode_op *op) {
	if (!op_array->scope
			|| (op_array->scope->ce_flags & ZEND_ACC_TRAIT)) {
		return FAILURE;
	}
	zend_packed_name_reference pnr;
	if (op_array->scope->num_generic_params) {
		zend_name_reference *name_ref = zend_arena_alloc(
				arena, ZEND_CLASS_REF_SIZE(op_array->scope->num_generic_params));
		name_ref->name = op_array->scope->name;
		zend_string_addref(name_ref->name);
		for (uint32_t i = 0; i < op_array->scope->num_generic_params; i++) {
			name_ref->args.types[i] = (zend_type) ZEND_TYPE_INIT_GENERIC_PARAM(i, 0);
		}
		name_ref->args.num_types = op_array->scope->num_generic_params;
		zend_compile_name_reference_key(name_ref, NULL);
		pnr = ZEND_PNR_ENCODE_REF(name_ref);
	} else {
		zend_string *cname = op_array->scope->name;
		zend_string_addref(cname);
		pnr = ZEND_PNR_ENCODE_NAME(cname);
	}
	*lcname = ZEND_CE_TO_REF(op_array->scope->name)->key;
	*object = (zend_type) ZEND_TYPE_INIT_PNR(pnr, 0, 0);
	*fetch_type = op->num & ZEND_FETCH_CLASS_MASK;
	return SUCCESS;
}

static zend_result zend_update_symbolic_var_type(zend_arena **arena, zend_ssa *ssa, zend_bitset worklist, zend_type *ssa_var_info, const zend_op_array *op_array, const zend_op *opline)
{
	zend_ssa_op *ssa_op = &ssa->ops[opline - op_array->opcodes];

	if (opline->opcode == ZEND_OP_DATA) {
		opline--;
		ssa_op--;
	}

	zend_type t1 = OP1_INFO();
	zend_type t2 = OP2_INFO();

label:
	if (0) goto label;

	/* If one of the operands cannot have any type, this means the operand derives from
	 * unreachable code. Propagate the empty result early, so that that the following
	 * code may assume that operands have at least one type. */
	if (!t1.type_mask
	 || !t2.type_mask
	 || (ssa_op->result_use >= 0 && !RES_USE_INFO().type_mask)
	 || ((opline->opcode == ZEND_ASSIGN_DIM_OP
	   || opline->opcode == ZEND_ASSIGN_OBJ_OP
	   || opline->opcode == ZEND_ASSIGN_STATIC_PROP_OP
	   || opline->opcode == ZEND_ASSIGN_DIM
	   || opline->opcode == ZEND_ASSIGN_OBJ)
	    && !OP1_DATA_INFO().type_mask)) {
		if (ssa_op->result_def >= 0) {
			UPDATE_SSA_TYPE(0, ssa_op->result_def);
		}
		if (ssa_op->op1_def >= 0) {
			UPDATE_SSA_TYPE(0, ssa_op->op1_def);
		}
		if (ssa_op->op2_def >= 0) {
			UPDATE_SSA_TYPE(0, ssa_op->op2_def);
		}
		if (opline->opcode == ZEND_ASSIGN_DIM_OP
		 || opline->opcode == ZEND_ASSIGN_OBJ_OP
		 || opline->opcode == ZEND_ASSIGN_STATIC_PROP_OP
		 || opline->opcode == ZEND_ASSIGN_DIM
		 || opline->opcode == ZEND_ASSIGN_OBJ) {
			if ((ssa_op+1)->op1_def >= 0) {
				UPDATE_SSA_TYPE(0, (ssa_op+1)->op1_def);
			}
		}

		return SUCCESS;
	}

	switch (opline->opcode) {
		case ZEND_ASSIGN:
			UPDATE_SSA_TYPE_EX(t2, ssa_op->op1_def);
			UPDATE_SSA_TYPE_EX(t2, ssa_op->result_def);
			break;
		case ZEND_QM_ASSIGN:
		case ZEND_COPY_TMP:
			UPDATE_SSA_TYPE_EX(t1, ssa_op->result_def);
			break;
		case ZEND_ASSIGN_DIM: {
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def); // TODO: vivification
			zend_type d1 = OP1_DATA_INFO();
			UPDATE_SSA_TYPE_EX(d1, ssa_op->result_def);
			break;
		}
		case ZEND_UNSET_CV:
			UPDATE_SSA_TYPE(MAY_BE_NULL, ssa_op->op1_def);
			break;
		case ZEND_UNSET_DIM:
		case ZEND_UNSET_OBJ:
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def);
			break;
		case ZEND_INIT_ARRAY:
		case ZEND_ADD_ARRAY_ELEMENT:
		case ZEND_ADD_ARRAY_UNPACK:
			UPDATE_SSA_TYPE(MAY_BE_ARRAY, ssa_op->result_def);
			break;
		case ZEND_ASSIGN_DIM_OP: {
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def); // TODO: vivification
			// TODO: ArrayAccess (if op1 is an object, we can do better)
			zend_type t = zend_symbolic_binop_auto(arena,
				opline->extended_value,
				(zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY),
				OP1_DATA_REC_INFO_EX(opline+1, ssa_op+1, ssa_op->op1_def));
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_R:
		case ZEND_FETCH_W:
		case ZEND_FETCH_RW:
		case ZEND_FETCH_IS:
		case ZEND_FETCH_FUNC_ARG:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		case ZEND_FETCH_GLOBALS:
			UPDATE_SSA_TYPE(MAY_BE_ARRAY, ssa_op->result_def);
			break;
		case ZEND_INCLUDE_OR_EVAL:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		case ZEND_FETCH_DIM_R:
		case ZEND_FETCH_DIM_W:
		case ZEND_FETCH_DIM_RW:
		case ZEND_FETCH_DIM_IS:
		case ZEND_FETCH_DIM_UNSET:
		case ZEND_FETCH_DIM_FUNC_ARG:
		case ZEND_FETCH_LIST_R:
		case ZEND_FETCH_LIST_W:
			/* TODO: generic arrays */
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def);
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		case ZEND_ASSIGN_OBJ:
		case ZEND_ASSIGN_OBJ_REF: {
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def); // TODO: vivification
			zend_type d1 = OP1_DATA_INFO();
			UPDATE_SSA_TYPE_EX(d1, ssa_op->result_def);
			break;
		}
		case ZEND_ASSIGN_OBJ_OP: {
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def); // TODO: vivification
			if (opline->op2_type != IS_CONST) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}
			zend_type d1 = OP1_DATA_REC_INFO();
			zend_type t = zend_symbolic_binop_auto(arena,
				opline->extended_value,
				zend_symbolic_prop(arena, 0, OP1_REC_INFO(), NULL,
					Z_STR_P(CRT_CONSTANT(opline->op2))),
				d1);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_ASSIGN_STATIC_PROP:
		case ZEND_ASSIGN_STATIC_PROP_OP:
		case ZEND_ASSIGN_STATIC_PROP_REF: {
			// TODO: intersect prop type and op2
		    UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		}
		case ZEND_BEGIN_SILENCE:
			UPDATE_SSA_TYPE(MAY_BE_LONG, ssa_op->result_def);
			break;
		case ZEND_BIND_GLOBAL:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->op1_def);
			break;
		case ZEND_BIND_INIT_STATIC_OR_JMP:
			/* TODO */
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->op1_def);
			break;
		case ZEND_BIND_LEXICAL:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->op2_def);
			break;
		case ZEND_BIND_STATIC:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->op1_def);
			break;
		case ZEND_CALLABLE_CONVERT: {
			zend_string *cname = zend_ce_closure->name;
			zend_type t = ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_CAST:
			/* TODO: preserve object type */
			UPDATE_SSA_TYPE(1 << opline->extended_value, ssa_op->result_def);
			break;
		case ZEND_TYPE_CHECK:
			UPDATE_SSA_TYPE(MAY_BE_BOOL, ssa_op->result_def);
			break;
		case ZEND_VERIFY_RETURN_TYPE:
			if (opline->op1_type & (IS_TMP_VAR|IS_VAR|IS_CV)) {
				UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def);
			} else {
				UPDATE_SSA_TYPE_EX(t1, ssa_op->result_def);
			}
			break;
		case ZEND_CLONE:
			UPDATE_SSA_TYPE_EX(t1, ssa_op->result_def);
			break;
		case ZEND_COUNT:
		case ZEND_STRLEN:
		case ZEND_FUNC_NUM_ARGS:
			UPDATE_SSA_TYPE(MAY_BE_LONG, ssa_op->result_def);
			break;
		case ZEND_DECLARE_ANON_CLASS:
			/* TODO: intersect parent and interfaces */
			UPDATE_SSA_TYPE(MAY_BE_OBJECT, ssa_op->result_def);
			break;
		case ZEND_DEFINED:
			UPDATE_SSA_TYPE(MAY_BE_BOOL, ssa_op->result_def);
			break;
		case ZEND_FETCH_CONSTANT: {
			zend_type t = zend_symbolic_const(arena,
					Z_STR_P(CRT_CONSTANT(opline->op2) + 1),
					(opline->op1.num & IS_CONSTANT_UNQUALIFIED_IN_NAMESPACE)
						? Z_STR_P(CRT_CONSTANT(opline->op2) + 2) : NULL);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_ASSIGN_OP: {
			zend_type t = zend_symbolic_binop_auto(arena,
				opline->extended_value,
				OP1_REC_INFO_EX(opline, ssa_op, ssa_op->op1_def),
				OP2_REC_INFO_EX(opline, ssa_op, ssa_op->op1_def));
			UPDATE_SSA_TYPE_EX(t, ssa_op->op1_def);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_OBJ_R:
		case ZEND_FETCH_OBJ_W:
		case ZEND_FETCH_OBJ_RW:
		case ZEND_FETCH_OBJ_IS:
		case ZEND_FETCH_OBJ_UNSET:
		case ZEND_FETCH_OBJ_FUNC_ARG: {
			if (opline->op2_type != IS_CONST) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}
			uint32_t fetch_type = ZEND_FETCH_CLASS_DEFAULT;
			zend_type object = {0};
			zend_string *lcname = NULL;
			if (opline->op1_type == IS_UNUSED) {
				if (zend_resolve_relative_class_base(arena,
							&fetch_type, &object, &lcname, op_array, &opline->op1) == FAILURE) {
					UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
					return SUCCESS;
				}
			} else {
				object = OP1_REC_INFO();
			}
			zend_type t = zend_symbolic_prop(arena, fetch_type, OP1_REC_INFO(),
					lcname, Z_STR_P(CRT_CONSTANT(opline->op2)));
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_STATIC_PROP_R:
		case ZEND_FETCH_STATIC_PROP_W:
		case ZEND_FETCH_STATIC_PROP_RW:
		case ZEND_FETCH_STATIC_PROP_IS:
		case ZEND_FETCH_STATIC_PROP_UNSET:
		case ZEND_FETCH_STATIC_PROP_FUNC_ARG: {
			if (opline->op2_type != IS_CONST) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}
			uint32_t fetch_type = ZEND_FETCH_CLASS_DEFAULT;
			zend_type object = {0};
			zend_string *lcname = NULL;
			if (opline->op2_type == IS_UNUSED) {
				if (zend_resolve_relative_class_base(arena,
							&fetch_type, &object, &lcname, op_array, &opline->op2) == FAILURE) {
					UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
					return SUCCESS;
				}
			} else if (opline->op2_type == IS_CONST) {
				zval *zcname = CRT_CONSTANT_EX(op_array, opline, opline->op2);
				ZEND_ASSERT(Z_TYPE_P(zcname) == IS_STRING);
				zend_string *cname = Z_STR_P(zcname);
				zend_string_addref(cname);
				object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
				lcname = Z_STR_P(zcname + 1);
			} else {
				object = OP1_REC_INFO();
			}
			zend_type t = zend_symbolic_prop(arena, fetch_type, object,
					lcname, Z_STR_P(CRT_CONSTANT(opline->op1)));
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_CLASS_CONSTANT: {
			if (opline->op2_type != IS_CONST) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}
			zend_type object = {0};
			zend_string *lcname = NULL;
			uint32_t fetch_type = ZEND_FETCH_CLASS_DEFAULT;
			if (opline->op1_type == IS_UNUSED) {
				if (!op_array->scope
						|| (op_array->scope->ce_flags & ZEND_ACC_TRAIT)) {
					UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
					break;
				}
				zend_string *cname = op_array->scope->name;
				zend_string_addref(cname);
				object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
				fetch_type = opline->op1.num & ZEND_FETCH_CLASS_MASK;
			} else if (opline->op1_type == IS_CONST) {
				zval *zcname = CRT_CONSTANT_EX(op_array, opline, opline->op1);
				ZEND_ASSERT(Z_TYPE_P(zcname) == IS_STRING);
				zend_string *cname = Z_STR_P(zcname);
				zend_string_addref(cname);
				object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
				lcname = Z_STR_P(zcname + 1);
			} else {
				object = OP1_REC_INFO();
			}
			zend_type t = zend_symbolic_class_const(arena, fetch_type, object,
					lcname, Z_STR_P(CRT_CONSTANT(opline->op2)));
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_BOOL_NOT:
		case ZEND_BOOL_XOR:
		case ZEND_BOOL:
		case ZEND_IS_IDENTICAL:
		case ZEND_IS_NOT_IDENTICAL:
		case ZEND_IS_EQUAL:
		case ZEND_IS_NOT_EQUAL:
		case ZEND_IS_SMALLER:
		case ZEND_IS_SMALLER_OR_EQUAL:
		case ZEND_INSTANCEOF:
		case ZEND_JMPZ_EX:
		case ZEND_JMPNZ_EX:
		case ZEND_CASE:
		case ZEND_CASE_STRICT:
		case ZEND_ISSET_ISEMPTY_CV:
		case ZEND_ISSET_ISEMPTY_VAR:
		case ZEND_ISSET_ISEMPTY_DIM_OBJ:
		case ZEND_ISSET_ISEMPTY_PROP_OBJ:
		case ZEND_ISSET_ISEMPTY_STATIC_PROP:
		case ZEND_ASSERT_CHECK:
		case ZEND_IN_ARRAY:
		case ZEND_ARRAY_KEY_EXISTS:
			UPDATE_SSA_TYPE(MAY_BE_BOOL, ssa_op->result_def);
			break;
		case ZEND_ADD:
		case ZEND_MUL:
		case ZEND_DIV:
		case ZEND_SUB:
		case ZEND_POW:
		case ZEND_BW_AND:
		case ZEND_BW_OR:
		case ZEND_BW_XOR:
		case ZEND_BW_NOT:
		case ZEND_MOD:
		case ZEND_SL:
		case ZEND_SR:
		case ZEND_CONCAT: {
			zend_type t = zend_symbolic_binop_auto(arena, opline->opcode,
				OP1_REC_INFO(), OP2_REC_INFO());
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_PRE_INC:
		case ZEND_PRE_DEC:
		case ZEND_POST_INC:
		case ZEND_POST_DEC: {
			UPDATE_SSA_TYPE(MAY_BE_LONG | MAY_BE_DOUBLE, ssa_op->result_def);
			UPDATE_SSA_TYPE(MAY_BE_LONG | MAY_BE_DOUBLE, ssa_op->op1_def);
			break;
	    }
		case ZEND_INIT_FCALL:
		case ZEND_INIT_FCALL_BY_NAME:
		case ZEND_INIT_NS_FCALL_BY_NAME:
		case ZEND_INIT_DYNAMIC_CALL:
		case ZEND_INIT_USER_CALL:
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		case ZEND_INIT_METHOD_CALL:
		case ZEND_INIT_STATIC_METHOD_CALL: {
			zend_type_list *list = zend_arena_calloc(arena, 1, ZEND_TYPE_LIST_SIZE(opline->extended_value + 1));
			list->num_types = opline->extended_value + 1;
			list->types[0] = t1;
			zend_type result = ZEND_TYPE_INIT_UNION(list, _ZEND_TYPE_ARENA_BIT);
			UPDATE_SSA_TYPE_EX(result, ssa_op->result_def);
			break;
		}
		case ZEND_DO_FCALL:
		case ZEND_DO_ICALL:
		case ZEND_DO_UCALL:
		case ZEND_DO_FCALL_BY_NAME: {
			const zend_op *init = zend_find_fcall_init_op(op_array, opline);

			if (init->op2_type != IS_CONST) {
				// TODO: invokable
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}

			uint32_t nargs = init->extended_value;
			zend_type *args;

			zend_type result;

			switch (init->opcode) {
				case ZEND_INIT_FCALL_BY_NAME: {
					zend_string *fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2)+1);
					result = zend_symbolic_fcall(arena, fname, nargs);
					args = SYMBOLIC_FCALL_ARGS(ZEND_TYPE_LIST(result));
					break;
				}
				case ZEND_INIT_FCALL: {
					zend_string *fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2));
					result = zend_symbolic_fcall(arena, fname, nargs);
					args = SYMBOLIC_FCALL_ARGS(ZEND_TYPE_LIST(result));
					break;
				}
				case ZEND_INIT_NS_FCALL_BY_NAME: {
					zend_string *fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2)+1);
					zend_string *ns_fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2)+2);
					result = zend_symbolic_ns_fcall(arena, fname, ns_fname, nargs);
					args = SYMBOLIC_NS_FCALL_ARGS(ZEND_TYPE_LIST(result));
					break;
				}
				case ZEND_INIT_METHOD_CALL: {
					zend_string *fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2)+1);
					zend_type object = OP1_REC_INFO_EX(init, &ssa->ops[init-op_array->opcodes], ssa_op->result_def);
					result = zend_symbolic_mcall(arena, object, fname, nargs);
					args = SYMBOLIC_MCALL_ARGS(ZEND_TYPE_LIST(result));
					break;
				}
				case ZEND_INIT_STATIC_METHOD_CALL: {
					zend_string *fname = Z_STR_P(CRT_CONSTANT_EX(op_array, init, init->op2)+1);
					uint32_t fetch_type = ZEND_FETCH_CLASS_DEFAULT;
					zend_type object = {0};
					zend_string *lcname = NULL;
					if (opline->op1_type == IS_UNUSED) {
						if (zend_resolve_relative_class_base(arena,
									&fetch_type, &object, &lcname, op_array,
									&init->op1) == FAILURE) {
							UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
							return SUCCESS;
						}
					} else if (opline->op1_type == IS_CONST) {
						zval *zcname = CRT_CONSTANT_EX(op_array, opline, opline->op1);
						ZEND_ASSERT(Z_TYPE_P(zcname) == IS_STRING);
						zend_string *cname = Z_STR_P(zcname);
						zend_string_addref(cname);
						object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
						lcname = Z_STR_P(zcname + 1);
					} else {
						object = OP1_REC_INFO();
					}
					result = zend_symbolic_scall(arena, fetch_type, object,
							lcname, fname, nargs);
					args = SYMBOLIC_MCALL_ARGS(ZEND_TYPE_LIST(result));
					break;
				}
				EMPTY_SWITCH_DEFAULT_CASE();
			}

			zend_get_fcall_arg_types(arena, ssa, ssa_var_info, op_array, init,
					ssa_op->result_def, args, nargs);

			UPDATE_SSA_TYPE_EX(result, ssa_op->result_def);

			break;
		}
		case ZEND_SEND_VAR_NO_REF:
		case ZEND_SEND_VAR_NO_REF_EX:
		case ZEND_SEND_VAR_EX:
		case ZEND_SEND_VAR:
		case ZEND_SEND_FUNC_ARG:
		case ZEND_SEND_REF:
		case ZEND_SEND_VAL:
		case ZEND_SEND_VAL_EX:
		case ZEND_SEND_UNPACK: {
			UPDATE_SSA_TYPE_EX(t1, ssa_op->op1_def);
			zend_type result = RES_USE_INFO();
			if (!ZEND_TYPE_IS_UNION(result)) {
				const zend_op *init_op = zend_find_fcall_init_op(op_array, opline);
				zend_type_list *list = zend_arena_calloc(arena, 1, ZEND_TYPE_LIST_SIZE(init_op->extended_value + 1));
				list->num_types = opline->extended_value + 1;
				list->types[opline->extended_value+1] = t1;
				result = (zend_type) ZEND_TYPE_INIT_UNION(list, _ZEND_TYPE_ARENA_BIT);
			} else {
				zend_type_list *list = ZEND_TYPE_LIST(result);
				// TODO: wasting arena space
				zend_type_list *new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(list->num_types));
				memcpy(new_list, list, ZEND_TYPE_LIST_SIZE(list->num_types));
				ZEND_ASSERT(opline->extended_value+1 < list->num_types);
				new_list->types[opline->extended_value+1] = t1;
				ZEND_TYPE_SET_PTR(result, new_list);
			}
			UPDATE_SSA_TYPE_EX(result, ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_THIS: {
			if (!op_array->scope) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}
			// TODO: generic scope
			zend_packed_name_reference pnr = ZEND_PNR_ENCODE_NAME(op_array->scope->name);
			UPDATE_SSA_TYPE_EX((zend_type) ZEND_TYPE_INIT_PNR(pnr, 0, 0), ssa_op->result_def);
			break;
		}
		case ZEND_FETCH_CLASS: {
			uint32_t fetch_type = ZEND_FETCH_CLASS_DEFAULT;
			zend_type object = {0};
			zend_string *lcname = NULL;
			if (opline->op2_type == IS_UNUSED) {
				if (!op_array->scope
						|| (op_array->scope->ce_flags & ZEND_ACC_TRAIT)) {
					UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
					break;
				}
				zend_string *cname = op_array->scope->name;
				zend_string_addref(cname);
				object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
				fetch_type = opline->op2.num & ZEND_FETCH_CLASS_MASK;
			} else if (opline->op2_type == IS_CONST) {
				zval *zcname = CRT_CONSTANT_EX(op_array, opline, opline->op1);
				ZEND_ASSERT(Z_TYPE_P(zcname) == IS_STRING);
				zend_string *cname = Z_STR_P(zcname);
				zend_string_addref(cname);
				object = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
				lcname = Z_STR_P(zcname + 1);
			} else {
				object = OP2_REC_INFO();
			}
			zend_type t = zend_symbolic_class(arena, fetch_type, object,
					lcname);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_NEW: {
			if (opline->op1_type != IS_CONST) {
				UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
				break;
			}

			zval *zclass = CRT_CONSTANT(opline->op1);
			zend_packed_name_reference pnr = Z_PNR_P(zclass);
			if (ZEND_PNR_IS_COMPLEX(pnr)) {
				UPDATE_SSA_TYPE_EX((zend_type) ZEND_TYPE_INIT_PNR(pnr, 0, 0), ssa_op->result_def);
				break;
			}

			zval *ztypes = CRT_CONSTANT(opline->op1)+2;
			zend_type_list *args;
			if (Z_TYPE_P(ztypes) == IS_NULL) {
				int nargs = opline->extended_value;
				args = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(nargs));
				args->num_types = nargs;
				ZVAL_PTR(ztypes, args);
			} else {
				args = Z_PTR_P(ztypes);
			}

			zend_get_fcall_arg_types(arena, ssa, ssa_var_info, op_array, opline, ssa_op->result_def, args->types, args->num_types);

			zend_string *cname = Z_STR_P(CRT_CONSTANT_EX(op_array, opline, opline->op1));
			zend_string_addref(cname);
			zend_type class = ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
			zend_type result = zend_symbolic_new(arena, class, args->types, args->num_types);

			UPDATE_SSA_TYPE_EX(result, ssa_op->result_def);
			break;
		}
		case ZEND_FE_RESET_R:
		case ZEND_FE_RESET_RW:
			break;
		case ZEND_FE_FETCH_R:
		case ZEND_FE_FETCH_RW: {
			/* TODO: symbolic type should be "iter_key" or "iter_value" */
			/* TODO: generic arrays */
			/* TODO: generic iterable and Iterator */
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		}
		case ZEND_JMP_SET:
		case ZEND_COALESCE: {
			zend_type t = zend_symbolic_coalesce(arena, OP1_REC_INFO());
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		case ZEND_FAST_CONCAT:
		case ZEND_ROPE_INIT:
		case ZEND_ROPE_ADD:
		case ZEND_ROPE_END:
			UPDATE_SSA_TYPE(MAY_BE_STRING, ssa_op->result_def);
			break;
		case ZEND_RECV:
		case ZEND_RECV_INIT: {
			zend_arg_info *arg_info = &op_array->arg_info[opline->op1.num-1];
			zend_type t = arg_info->type;
			if (ZEND_TYPE_IS_SET(t)) {
				ZEND_TYPE_FULL_MASK(t) &= _ZEND_TYPE_MASK;
				UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
				break;
			}
			UPDATE_SSA_TYPE(MAY_BE_ANY, ssa_op->result_def);
			break;
		}
		case ZEND_RECV_VARIADIC:
			UPDATE_SSA_TYPE(MAY_BE_ARRAY, ssa_op->result_def);
			break;
		case ZEND_DECLARE_LAMBDA_FUNCTION: {
			zend_string *cname = zend_ce_closure->name;
			zend_type t = ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(cname), 0, 0);
			UPDATE_SSA_TYPE_EX(t, ssa_op->result_def);
			break;
		}
		default:
			fprintf(stderr, "Unhandled opcode: %d\n", opline->opcode);
			break;
	}

	return SUCCESS;
}

static void zend_type_list_copy(zend_type_list *dest, zend_type_list *src, uint32_t num_types)
{
	zend_type *cur = src->types;
	zend_type *end = cur+num_types;
	zend_type *cur_dest = dest->types;
	for (; cur < end; cur++, cur_dest++) {
		*cur_dest = *cur;
	}
	dest->num_types = num_types;
}

static zend_result zend_resolve_recursive_type_placeholders(zend_arena **arena, zend_ssa *ssa, zend_type *ssa_var_info, zend_type *t, zend_bitset seen)
{
	if (ZEND_TYPE_IS_SSA_VAR(*t)) {
		int var = ZEND_TYPE_SSA_VAR(*t);
		if (zend_bitset_in(seen, var)) {
			return FAILURE;
		}
		zend_bitset_incl(seen, var);
		zend_type tmp = ssa_var_info[var];
		zend_result result = zend_resolve_recursive_type_placeholders(arena, ssa, ssa_var_info, &tmp, seen);
		zend_bitset_excl(seen, var);
		*t = tmp;
		return result;
	}

	if (ZEND_TYPE_IS_UNION(*t) || ZEND_TYPE_IS_INTERSECTION(*t)) {
		zend_type *list_type;
		zend_type_list *list = ZEND_TYPE_LIST(*t);
		zend_type_list *new_list = NULL;
		ZEND_TYPE_LIST_FOREACH(list, list_type) {
			zend_type tmp = *list_type;
			if (zend_resolve_recursive_type_placeholders(arena, ssa, ssa_var_info, &tmp, seen) == FAILURE) {
				if (new_list == NULL) {
					new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(list->num_types-1));
					zend_type_list_copy(new_list, list, list_type-list->types);
					ZEND_TYPE_SET_LIST(*t, new_list);
				}
				continue;
			} else if (tmp.type_mask != list_type->type_mask
					|| ((ZEND_TYPE_IS_COMPLEX(tmp) || ZEND_TYPE_IS_SSA_VAR(tmp))
						&& tmp.ptr != list_type->ptr)) {
				if (new_list == NULL) {
					new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(list->num_types));
					zend_type_list_copy(new_list, list, list_type-list->types);
					ZEND_TYPE_SET_LIST(*t, new_list);
				}
			}
			if (new_list) {
				new_list->types[new_list->num_types++] = tmp;
			}
		} ZEND_TYPE_LIST_FOREACH_END();
		if (new_list && new_list->num_types == 0) {
			if (ZEND_TYPE_PURE_MASK(*t) == 0) {
				return FAILURE;
			}
			ZEND_TYPE_FULL_MASK(*t) = ZEND_TYPE_PURE_MASK(*t);
		}
	} else if (ZEND_TYPE_HAS_COMPLEX_PNR(*t)) {
		zend_name_reference *ref = ZEND_PNR_COMPLEX_GET_REF(ZEND_TYPE_PNR(*t));
		zend_name_reference *new_ref = NULL;
		zend_type *list_type;
		ZEND_TYPE_LIST_FOREACH(&ref->args, list_type) {
			zend_type tmp = *list_type;
			if (zend_resolve_recursive_type_placeholders(arena, ssa, ssa_var_info, &tmp, seen) == FAILURE) {
				if (ZEND_TYPE_PURE_MASK(*t) == 0) {
					return FAILURE;
				}
				ZEND_TYPE_FULL_MASK(*t) = ZEND_TYPE_PURE_MASK(*t);
			} else if (tmp.type_mask != list_type->type_mask
					|| ((ZEND_TYPE_IS_COMPLEX(tmp) || ZEND_TYPE_IS_SSA_VAR(tmp))
						&& tmp.ptr != list_type->ptr)) {
				if (new_ref == NULL) {
					new_ref = zend_arena_alloc(arena, ZEND_CLASS_REF_SIZE(ref->args.num_types));
					zend_type_list_copy(&new_ref->args, &ref->args, list_type-ref->args.types);
					new_ref->name = ref->name;
					ZEND_TYPE_SET_PNR(*t, ZEND_PNR_ENCODE_REF(new_ref));
				}
			}
			if (new_ref) {
				new_ref->args.types[new_ref->args.num_types++] = tmp;
			}
		} ZEND_TYPE_LIST_FOREACH_END();
	} else if (ZEND_TYPE_IS_SYMBOLIC(*t)) {
		zend_type_list *new_list = NULL;
		zend_type_list *list = ZEND_TYPE_LIST(*t);
		zend_type *list_type;
		ZEND_TYPE_LIST_FOREACH(ZEND_TYPE_LIST(*t), list_type) {
			zend_type tmp = *list_type;
			if (zend_resolve_recursive_type_placeholders(arena, ssa, ssa_var_info, &tmp, seen) == FAILURE) {
				if (ZEND_TYPE_PURE_MASK(*t) == 0) {
					return FAILURE;
				}
				ZEND_TYPE_FULL_MASK(*t) = ZEND_TYPE_PURE_MASK(*t);
			} else if (tmp.type_mask != list_type->type_mask
					|| ((ZEND_TYPE_IS_COMPLEX(tmp) || ZEND_TYPE_IS_SSA_VAR(tmp))
						&& tmp.ptr != list_type->ptr)) {
				if (new_list == NULL) {
					new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(list->num_types));
					zend_type_list_copy(new_list, list, list_type-list->types);
					ZEND_TYPE_SET_PTR(*t, new_list);
				}
			}
			if (new_list) {
				new_list->types[new_list->num_types++] = tmp;
			}
		} ZEND_TYPE_LIST_FOREACH_END();
	}

	return SUCCESS;
}

static void zend_resolve_recursive_placeholders(zend_arena **arena, const zend_op_array *op_array, zend_ssa *ssa, zend_type *ssa_var_info)
{
	ALLOCA_FLAG(use_heap);
	uint32_t seen_len = zend_bitset_len(ssa->vars_count);
	zend_bitset seen = ZEND_BITSET_ALLOCA(seen_len, use_heap);
	zend_bitset_clear(seen, seen_len);

	for (int i = 0; i < op_array->last; i++) {
		const zend_op *opline = &op_array->opcodes[i];
		if (opline->opcode == ZEND_NEW && opline->op1_type == IS_CONST &&
				ZEND_PNR_IS_SIMPLE(Z_PNR_P(CRT_CONSTANT(opline->op1)))) {
			zval *ztypes = CRT_CONSTANT(opline->op1)+2;
			if (Z_TYPE_P(ztypes) != IS_NULL) {
				zend_type_list *args = Z_PTR_P(ztypes);
				for (uintptr_t j = 0; j < args->num_types; j++) {
					zend_type tmp = args->types[j];
					if (ZEND_TYPE_IS_SSA_VAR(tmp)) {
						tmp = ssa_var_info[ZEND_TYPE_SSA_VAR(tmp)];
					}
					zend_result result = zend_resolve_recursive_type_placeholders(arena,
							ssa, ssa_var_info, &tmp, seen);
					//ZEND_ASSERT(tmp.type_mask != 0);
					args->types[j] = tmp;
					ZEND_ASSERT(result == SUCCESS);
				}
			}
		}
	}

	free_alloca(seen, use_heap);
}

static zend_result zend_build_type_pnr_keys(zend_type type)
{
	zend_type *type_elem;
	ZEND_TYPE_FOREACH(type, type_elem) {
		if (ZEND_TYPE_HAS_COMPLEX_PNR(*type_elem)) {
			if (ZEND_TYPE_IS_SYMBOLIC(*type_elem)) {
				return FAILURE;
			}
			zend_name_reference *ref = ZEND_PNR_COMPLEX_GET_REF(ZEND_TYPE_PNR(*type_elem));
			for (uint32_t i = 0; i < ref->args.num_types; i++) {
				if (zend_build_type_pnr_keys(ref->args.types[i]) == FAILURE) {
					return FAILURE;
				}
			}
			zend_string *lcname = zend_string_tolower(ref->name);
			zend_compile_name_reference_key(ref, lcname);
			zend_string_release(lcname);
		} else if (ZEND_TYPE_HAS_LIST(*type_elem)) {
			zend_build_type_pnr_keys(*type_elem);
		}
	} ZEND_TYPE_FOREACH_END();

	return SUCCESS;
}

static void zend_build_pnr_keys(const zend_op_array *op_array)
{
	for (int i = 0; i < op_array->last; i++) {
		const zend_op *opline = &op_array->opcodes[i];
		if (opline->opcode == ZEND_NEW && opline->op1_type == IS_CONST &&
				ZEND_PNR_IS_SIMPLE(Z_PNR_P(CRT_CONSTANT(opline->op1)))) {
			zval *ztypes = CRT_CONSTANT(opline->op1)+2;
			if (Z_TYPE_P(ztypes) != IS_NULL) {
				zend_type_list *args = Z_PTR_P(ztypes);
				for (uintptr_t j = 0; j < args->num_types; j++) {
					zend_build_type_pnr_keys(args->types[j]);
				}
			}
		}
	}
}

void ssa_verify_integrity(zend_op_array *op_array, zend_ssa *ssa, const char *extra);

/* Infers the symbolic type of ZEND_NEW arguments, at compile time. */
ZEND_API zend_result zend_symbolic_inference(const zend_op_array *op_array, zend_arena **arena)
{
	zend_ssa *ssa = zend_arena_alloc(arena, sizeof(zend_ssa));
	memset(ssa, 0, sizeof(zend_ssa));

	zend_build_cfg(arena, op_array, ZEND_CFG_NO_ENTRY_PREDECESSORS, &ssa->cfg);

	zend_cfg_build_predecessors(arena, &ssa->cfg);

	/* Compute Dominators Tree */
	zend_cfg_compute_dominators_tree(op_array, &ssa->cfg);

	/* Identify reducible and irreducible loops */
	zend_cfg_identify_loops(op_array, &ssa->cfg);

#if 0
	if (zend_string_equals_cstr(op_array->function_name, "main", 4)) {
		zend_dump_op_array(op_array, ZEND_DUMP_CFG, "symbolic inference (CFG)", &ssa->cfg);
	}
#endif

	zend_script script = {0};
	uint32_t build_flags = ZEND_SSA_CALL_CHAINS;
	if (zend_build_ssa(arena, &script, op_array, build_flags, ssa) == FAILURE) {
		return FAILURE;
	}

	zend_ssa_compute_use_def_chains(arena, op_array, ssa);
	zend_ssa_find_sccs(op_array, ssa);

#if 0
	if (!op_array->function_name || zend_string_equals_cstr(op_array->function_name, "test", 4)) {
		//ssa_verify_integrity((zend_op_array*)op_array, ssa, "symbolic inference");
		zend_dump_op_array(op_array, ZEND_DUMP_SSA, "symbolic inference", ssa);
		zend_dump_ssa_variables(op_array, ssa, 0);
	}
#endif

	ALLOCA_FLAG(use_heap);
	uint32_t worklist_len = zend_bitset_len(ssa->vars_count);
	zend_bitset worklist = do_alloca(sizeof(zend_ulong) * worklist_len, use_heap);
	memset(worklist, 0, sizeof(zend_ulong) * worklist_len);
	for (int i = op_array->last_var; i < ssa->vars_count; i++) {
		zend_bitset_incl(worklist, i);
	}

	zend_ssa_var *ssa_vars = ssa->vars;
	zend_type *ssa_var_info = zend_arena_calloc(arena, ssa->vars_count, sizeof(*ssa_var_info));
	for (int i = 0; i < op_array->last_var; i++) {
		ZEND_TYPE_FULL_MASK(ssa_var_info[i]) = MAY_BE_UNDEF;
	}

	zend_basic_block *blocks = ssa->cfg.blocks;
	while (!zend_bitset_empty(worklist, worklist_len)) {
		int j = zend_bitset_first(worklist, worklist_len);
		zend_bitset_excl(worklist, j);
		if (ssa_vars[j].definition_phi) {
			zend_ssa_phi *p = ssa_vars[j].definition_phi;
			if (p->pi >= 0) {
				zend_type tmp = get_ssa_var_info(ssa_var_info, p->sources[0]);
				UPDATE_SSA_TYPE_EX(tmp, j);
			} else {
				zend_type tmp;
				ALLOCA_FLAG(use_heap);
				zend_type_list *new_list = NULL;

				for (int i = 0; i < blocks[p->block].predecessors_count; i++) {
					if (i == 0) {
						tmp = get_ssa_var_info(ssa_var_info, p->sources[i]);
					} else if (!ZEND_TYPE_IS_COMPLEX(tmp) && !ZEND_TYPE_IS_SSA_VAR(tmp)) {
						uint32_t mask = ZEND_TYPE_PURE_MASK(tmp);
						tmp = get_ssa_var_info(ssa_var_info, p->sources[i]);
						ZEND_TYPE_FULL_MASK(tmp) |= mask;
					} else {
						zend_type tmp2 = get_ssa_var_info(ssa_var_info, p->sources[i]);
						if (tmp2.type_mask == 0) {
							continue;
						}

						if (ZEND_TYPE_IS_INTERSECTION(tmp2)) {
							if (zend_type_equals(tmp, tmp2)) {
								continue;
							}
							if (!new_list) {
								uint32_t num_types = ZEND_TYPE_IS_UNION(tmp) ? ZEND_TYPE_LIST(tmp)->num_types : 1;
								for (int j = i; j < blocks[p->block].predecessors_count; j++) {
									zend_type tmp = get_ssa_var_info(ssa_var_info, p->sources[i]);
									if (ZEND_TYPE_IS_UNION(tmp)) {
										num_types += ZEND_TYPE_LIST(tmp)->num_types;
									} else if (ZEND_TYPE_IS_COMPLEX(tmp) || ZEND_TYPE_IS_SSA_VAR(tmp)) {
										num_types++;
									}
								}

								new_list = do_alloca(ZEND_TYPE_LIST_SIZE(num_types), use_heap);
								if (ZEND_TYPE_IS_UNION(tmp)) {
									memcpy(new_list, ZEND_TYPE_LIST(tmp), ZEND_TYPE_LIST_SIZE(ZEND_TYPE_LIST(tmp)->num_types));
								} else {
									new_list->types[0] = tmp;
									new_list->num_types = 1;
								}

								ZEND_TYPE_SET_LIST(tmp, new_list);
								ZEND_TYPE_FULL_MASK(tmp) = ZEND_TYPE_PURE_MASK(tmp) | _ZEND_TYPE_LIST_BIT | _ZEND_TYPE_UNION_BIT;
							}
							new_list->types[new_list->num_types++] = tmp2;
							continue;
						}

						if (!ZEND_TYPE_IS_COMPLEX(tmp2) && !ZEND_TYPE_IS_SSA_VAR(tmp2)) {
							ZEND_TYPE_FULL_MASK(tmp) |= ZEND_TYPE_PURE_MASK(tmp2);
							continue;
						}

						zend_type *tmp2_elem;
						ZEND_TYPE_FOREACH_EX(tmp2, tmp2_elem, ~_ZEND_TYPE_SYMBOLIC_BIT) {
							if (zend_union_type_contains(tmp, *tmp2_elem)) {
								continue;
							}
							if (!new_list) {
								uint32_t num_types = ZEND_TYPE_IS_UNION(tmp) ? ZEND_TYPE_LIST(tmp)->num_types : 1;
								for (int j = i; j < blocks[p->block].predecessors_count; j++) {
									zend_type tmp = get_ssa_var_info(ssa_var_info, p->sources[i]);
									if (ZEND_TYPE_IS_UNION(tmp)) {
										num_types += ZEND_TYPE_LIST(tmp)->num_types;
									} else if (ZEND_TYPE_IS_COMPLEX(tmp) || ZEND_TYPE_IS_SSA_VAR(tmp)) {
										num_types++;
									}
								}

								new_list = do_alloca(ZEND_TYPE_LIST_SIZE(num_types), use_heap);
								if (ZEND_TYPE_IS_UNION(tmp)) {
									memcpy(new_list, ZEND_TYPE_LIST(tmp), ZEND_TYPE_LIST_SIZE(ZEND_TYPE_LIST(tmp)->num_types));
								} else {
									new_list->types[0] = tmp;
									new_list->num_types = 1;
								}

								ZEND_TYPE_SET_LIST(tmp, new_list);
								ZEND_TYPE_FULL_MASK(tmp) = ZEND_TYPE_PURE_MASK(tmp) | _ZEND_TYPE_LIST_BIT | _ZEND_TYPE_UNION_BIT;
							}
							new_list->types[new_list->num_types++] = *tmp2_elem;
						} ZEND_TYPE_FOREACH_END();
					}
				}

				if (new_list) {
					size_t size = ZEND_TYPE_LIST_SIZE(new_list->num_types);
					zend_type_list *list = zend_arena_alloc(arena, size);
					memcpy(list, new_list, size);
					ZEND_TYPE_SET_LIST(tmp, list);
					free_alloca(new_list, use_heap);
				}

				UPDATE_SSA_TYPE_EX(tmp, j);
			}
		} else if (ssa_vars[j].definition >= 0) {
			int i = ssa_vars[j].definition;
			if (zend_update_symbolic_var_type(arena, ssa, worklist, ssa_var_info, op_array, op_array->opcodes + i) == FAILURE) {
				return FAILURE;
			}
		}
	}

	free_alloca(worklist, use_heap);

	zend_resolve_recursive_placeholders(arena, op_array, ssa, ssa_var_info);
	zend_build_pnr_keys(op_array);

	return SUCCESS;
}

static zend_type zend_resolve_overloaded_type(zend_type type)
{
	zend_type result = ZEND_TYPE_INIT_MASK(0);
	zend_type *single_type;

	ZEND_ASSERT(!ZEND_TYPE_IS_SYMBOLIC(type));

	ZEND_TYPE_FOREACH(type, single_type) {
		if (ZEND_TYPE_HAS_PNR(type)) {
			zend_string *name = ZEND_TYPE_PNR_NAME(type);
			zend_class_entry *ce = zend_fetch_class(name, ZEND_FETCH_CLASS_AUTO | ZEND_FETCH_CLASS_SILENT);
			if (!ce) {
				continue;
			}
			if (ce->default_object_handlers->do_operation) {
				// TODO
				if (zend_string_equals_cstr(ce->name, "GMP", 3)) {
					result = (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(ce->name), 0, 0);
				} else {
					ZEND_TYPE_FULL_MASK(result) = IS_OBJECT;
				}
				// TODO: join with existing result
				return result;
			}
		} else if (ZEND_TYPE_IS_INTERSECTION(type)) {
			zend_type inner = zend_resolve_overloaded_type(*single_type);
			ZEND_ASSERT(!ZEND_TYPE_IS_COMPLEX(inner)); /* TODO */
			ZEND_TYPE_FULL_MASK(result) |= ZEND_TYPE_FULL_MASK(inner);
		}
	} ZEND_TYPE_FOREACH_END();

	return result;
}

zend_type zend_materialize_generic_types(zend_arena **arena, zend_type t, zend_class_reference *scope);

static zend_result zend_resolve_relative_class(zend_arena **arena,
		uint32_t fetch_type, zend_type *object) {
	ZEND_ASSERT(ZEND_TYPE_HAS_PNR(*object));
	switch (fetch_type) {
		case ZEND_FETCH_CLASS_PARENT: {
			zend_class_reference *class_ref = zend_lookup_class_by_pnr(
					ZEND_TYPE_PNR(*object), NULL, ZEND_FETCH_CLASS_SILENT);
			if (!class_ref || !class_ref->ce->num_parents) {
				return FAILURE;
			}
			zend_class_reference *parent = class_ref->ce->parents[0];
			if (parent->args.num_types == 0) {
				ZEND_TYPE_SET_PNR(*object, ZEND_PNR_ENCODE_NAME(parent->ce->name));
			} else {
				zend_name_reference *name_ref = zend_arena_alloc(
						arena, ZEND_CLASS_REF_SIZE(parent->args.num_types));
				memcpy(name_ref, parent, ZEND_CLASS_REF_SIZE(parent->args.num_types));
				name_ref->name = parent->ce->name;
				ZEND_TYPE_SET_PNR(*object, ZEND_PNR_ENCODE_REF(name_ref));
				*object = zend_materialize_generic_types(arena, *object, class_ref);
			}
			break;
		}
		case ZEND_FETCH_CLASS_STATIC:
			return FAILURE;
		case ZEND_FETCH_CLASS_DEFAULT:
		case ZEND_FETCH_CLASS_SELF:
			break;
		default:
			ZEND_UNREACHABLE();
	}

	return SUCCESS;
}

zend_type zend_resolve_symbolic_type_symbolic(zend_arena **arena, zend_type type);

/* Resolve a symbolic type in the form fcall<fname,arg,..> to a concrete
 * zend_type at runtime. Result is cacheable. */
ZEND_API zend_type zend_resolve_symbolic_type(zend_arena **arena, zend_type type)
{
	if (ZEND_TYPE_IS_UNION(type)) {
		zend_type_list *list = ZEND_TYPE_LIST(type);
		zend_type *single_type;
		uint32_t num_types = 0;
		ZEND_TYPE_LIST_FOREACH(list, single_type) {
			zend_type t = zend_resolve_symbolic_type(arena, *single_type);
			if (!ZEND_TYPE_IS_COMPLEX(t)) {
				ZEND_TYPE_FULL_MASK(type) |= ZEND_TYPE_PURE_MASK(t);
			} else {
				list->types[num_types++] = t;
			}
		} ZEND_TYPE_LIST_FOREACH_END();
		list->num_types = num_types;
		if (num_types == 0) {
			ZEND_TYPE_FULL_MASK(type) = ZEND_TYPE_PURE_MASK(type);
		}
		return type;
	}

	if (ZEND_TYPE_IS_SYMBOLIC(type)) {
		uint32_t mask = ZEND_TYPE_PURE_MASK(type);
		type = zend_resolve_symbolic_type_symbolic(arena, type);
		ZEND_TYPE_FULL_MASK(type) |= mask;
	}

	return type;
}

zend_type zend_resolve_symbolic_type_symbolic(zend_arena **arena, zend_type type)
{
	zend_type_list *list = ZEND_TYPE_LIST(type);
	uint32_t symtype = ZEND_TYPE_FULL_MASK(list->types[0]);
	switch (symtype) {
		case SYMTYPE_BINOP: {
			zend_type a = zend_resolve_symbolic_type(arena, SYMBOLIC_BINOP_OP1(list));
			zend_type b = zend_resolve_symbolic_type(arena, SYMBOLIC_BINOP_OP2(list));
			uint32_t mask = ZEND_TYPE_FULL_MASK(a) | ZEND_TYPE_FULL_MASK(b);
			switch (SYMBOLIC_BINOP_TYPE_MASK(list)) {
				case BINOP_NUMBER: {
					if (!(mask & ~(MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_STRING | MAY_BE_BOOL | MAY_BE_NULL))) {
						return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG | MAY_BE_DOUBLE);
					}
					break;
				}
				case BINOP_LONG: {
					if (!(mask & ~(MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_STRING |
									MAY_BE_BOOL | MAY_BE_NULL))) {
						return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
					}
					break;
				}
				case BINOP_NUMBER_ARRAY: {
					switch (mask & ~(MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_STRING |
								MAY_BE_BOOL | MAY_BE_NULL)) {
						case 0:
							return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG | MAY_BE_DOUBLE);
						case MAY_BE_ARRAY:
							return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ARRAY);
					}
					break;
				}
				case BINOP_LONG_STRING: {
					switch (mask &
							~(MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_BOOL | MAY_BE_NULL)) {
						case 0:
							return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
						case MAY_BE_STRING:
							return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_STRING);
					}
					break;
				}
				case BINOP_STRING: {
					if (!(mask & ~(MAY_BE_LONG | MAY_BE_DOUBLE | MAY_BE_STRING |
									MAY_BE_BOOL | MAY_BE_NULL))) {
						return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_STRING);
					}
					break;
				}
				EMPTY_SWITCH_DEFAULT_CASE();
			}

			/* Operator may be overloaded */
			a = zend_resolve_overloaded_type(a);
			if (ZEND_TYPE_FULL_MASK(a)) {
				return a;
			}
			b = zend_resolve_overloaded_type(b);
			if (ZEND_TYPE_FULL_MASK(b)) {
				return b;
			}

			/* We could not prove the operator is overloaded: fallback to
			 * optimistic defaults */
			switch (SYMBOLIC_BINOP_TYPE_MASK(list)) {
				case BINOP_NUMBER:
				case BINOP_NUMBER_ARRAY:
					return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG | MAY_BE_DOUBLE);
					return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG | MAY_BE_DOUBLE);
				case BINOP_LONG:
				case BINOP_LONG_STRING:
					return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
				case BINOP_STRING:
					return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_STRING);
				EMPTY_SWITCH_DEFAULT_CASE();
			}

			ZEND_UNREACHABLE();
		}
		case SYMTYPE_FCALL: {
			// TODO: generic functions
			zend_string *fname = SYMBOLIC_FCALL_FNAME(list);
			zval *zfunc = zend_hash_find_known_hash(EG(function_table), fname);
			if (!zfunc) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			zend_function *func = Z_FUNC_P(zfunc);
			if (!func->common.arg_info) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			return (func->common.arg_info - 1)->type;
		}
		case SYMTYPE_METHOD_CALL:
		case SYMTYPE_STATIC_METHOD_CALL: {
			zend_type object = SYMBOLIC_MCALL_OBJECT(list);
			if (ZEND_TYPE_HAS_PNR(object) && zend_resolve_relative_class(
						arena, SYMBOLIC_MCALL_FETCH_TYPE(list),
						&object) == FAILURE) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			zend_type result = {0};
			zend_type *object_elem;
			ZEND_TYPE_FOREACH_EX(SYMBOLIC_MCALL_OBJECT(list), object_elem, ~(_ZEND_TYPE_INTERSECTION_BIT|_ZEND_TYPE_SYMBOLIC_BIT)) {
				zend_type resolved = zend_resolve_symbolic_type(arena, *object_elem);
				zend_type inner = {0};
				ZEND_TYPE_FOREACH(resolved, object_elem) {
					if (!ZEND_TYPE_HAS_PNR(*object_elem)) {
						return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
					}

					zend_class_reference *class_ref;

					if (ZEND_TYPE_HAS_COMPLEX_PNR(resolved)) {
						class_ref = zend_fetch_generic_class_by_ref(ZEND_PNR_COMPLEX_GET_REF(ZEND_TYPE_PNR(resolved)), NULL, ZEND_FETCH_CLASS_AUTO | ZEND_FETCH_CLASS_SILENT);
					} else {
						zend_string *cname = ZEND_TYPE_PNR_SIMPLE_NAME(resolved);
						zend_class_entry *ce = zend_lookup_class_ex(
								cname, NULL, ZEND_FETCH_CLASS_AUTO | ZEND_FETCH_CLASS_SILENT);
						if (!ce) {
							return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
						}
						class_ref = ZEND_CE_TO_REF(ce);
					}
					if (!class_ref) {
						return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
					}

					zend_string *fname = SYMBOLIC_MCALL_FNAME(list);
					zend_function *func = zend_hash_find_ptr(&class_ref->ce->function_table, fname);
					if (!func) {
						if (symtype == SYMTYPE_METHOD_CALL && class_ref->ce->__call) {
							func = class_ref->ce->__call;
						} else if (symtype == SYMTYPE_STATIC_METHOD_CALL && class_ref->ce->__callstatic) {
							func = class_ref->ce->__callstatic;
						}
					}
					zend_type t;
					if (!func || !func->common.arg_info) {
						t = (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
					} else {
						// TODO: scope/visibility?
						t = (func->common.arg_info - 1)->type;
					}
					if (class_ref->ce->num_generic_params) {
						t = zend_materialize_generic_types(arena, t, class_ref);
					}
					if (ZEND_TYPE_IS_INTERSECTION(resolved)) {
						inner = zend_type_intersect(arena, inner, t);
					} else {
						inner = zend_type_union(arena, inner, t);
					}
				} ZEND_TYPE_FOREACH_END();
				if (ZEND_TYPE_IS_INTERSECTION(SYMBOLIC_MCALL_OBJECT(list))) {
					result = zend_type_intersect(arena, result, inner);
				} else {
					result = zend_type_union(arena, result, inner);
				}
			} ZEND_TYPE_FOREACH_END();

			return result;
		}
		case SYMTYPE_NEW: {
			// TODO: generic classes
			// TODO: relative class
			zend_type object = zend_resolve_symbolic_type(arena, SYMBOLIC_NEW_OBJECT(list));
			if (!ZEND_TYPE_HAS_PNR(object)) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			return object;
		}
		case SYMTYPE_PROP: {
			zend_type object = SYMBOLIC_PROP_OBJECT(list);
			if (ZEND_TYPE_HAS_PNR(object) && zend_resolve_relative_class(
						arena, SYMBOLIC_MCALL_FETCH_TYPE(list),
						&object) == FAILURE) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			object = zend_resolve_symbolic_type(arena, object);
			// TODO compound types
			if (!ZEND_TYPE_HAS_PNR(object)) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			zend_class_reference *class_ref = zend_lookup_class_by_pnr(
					ZEND_TYPE_PNR(object), NULL, ZEND_FETCH_CLASS_SILENT);
			if (!class_ref) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			zend_string *pname = SYMBOLIC_PROP_PNAME(list);
			zend_class_entry *prev_scope = EG(fake_scope);
			EG(fake_scope) = class_ref->ce; // TODO
			zend_property_info *prop_info = zend_get_property_info(
					class_ref->ce, pname, 1);
			EG(fake_scope) = prev_scope;
			if (prop_info && prop_info != ZEND_WRONG_PROPERTY_INFO) {
				if (ZEND_TYPE_IS_SET(prop_info->type)) {
					// TODO: materialize is not always required
					// (e.g. for $this->prop)
					return zend_materialize_generic_types(arena,
							prop_info->type, class_ref);
				}
			}
			return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
		}
		case SYMTYPE_CLASS: {
			zend_type object = zend_resolve_symbolic_type(arena, SYMBOLIC_CLASS_OBJECT(list));
			if (!ZEND_TYPE_HAS_PNR(object)) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			switch (SYMBOLIC_CCONST_FETCH_TYPE(list)) {
				case ZEND_FETCH_CLASS_DEFAULT:
				case ZEND_FETCH_CLASS_SELF:
					return object;
				case ZEND_FETCH_CLASS_PARENT: {
					zend_string *cname = ZEND_TYPE_PNR_SIMPLE_NAME(object);
					zend_string *lcname = SYMBOLIC_CLASS_LCNAME(list);
					zend_class_entry *ce = zend_lookup_class_ex(cname, lcname,
							ZEND_FETCH_CLASS_SILENT);
					if (!ce || !ce->num_parents) {
						return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
					}
					ce = ce->parents[0]->ce;
					return (zend_type) ZEND_TYPE_INIT_PNR(ZEND_PNR_ENCODE_NAME(ce->name), 0, 0);
				}
				case ZEND_FETCH_CLASS_STATIC:
					return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
				default:
					ZEND_UNREACHABLE();
					return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
		}
		case SYMTYPE_CLASS_CONSTANT: {
			zend_type object = zend_resolve_symbolic_type(arena, SYMBOLIC_CCONST_OBJECT(list));
			if (!ZEND_TYPE_HAS_PNR(object)) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			zend_string *cname = ZEND_TYPE_PNR_SIMPLE_NAME(object);
			zend_string *lcname = SYMBOLIC_CCONST_LCNAME(list);
			// TODO: $this call
			// TODO: relative call
			// TODO: ::class
			// TODO: AST
			// TODO: enum
			zend_class_entry *ce = zend_lookup_class_ex(cname, lcname,
					ZEND_FETCH_CLASS_SILENT);
			if (!ce) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			switch (SYMBOLIC_CCONST_FETCH_TYPE(list)) {
				case ZEND_FETCH_CLASS_DEFAULT:
					break;
				case ZEND_FETCH_CLASS_SELF:
					break;
				case ZEND_FETCH_CLASS_PARENT: {

					if (!ce->num_parents) {
						return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
					}
					ce = ce->parents[0]->ce;
					break;
				}
				case ZEND_FETCH_CLASS_STATIC:
					// TODO: maybe conservatively use highest parent
					// declaring the constant, if typed?
					return (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
				EMPTY_SWITCH_DEFAULT_CASE();
			}

			zend_string *constant_name = SYMBOLIC_CCONST_CNAME(list);
			zval *zv = zend_hash_find_known_hash(CE_CONSTANTS_TABLE(ce),
					constant_name);
			if (!zv) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}
			zend_class_constant *constant = Z_PTR_P(zv);
			if (ZEND_TYPE_IS_SET(constant->type)) {
				return constant->type;
			}
			return _const_op_type(&constant->value);
		}
		case SYMTYPE_CONSTANT: {
			zval *zv = zend_hash_find_known_hash(EG(zend_constants),
					SYMBOLIC_CONST_KEY1(list));
			zend_constant *constant = NULL;
			if (zv) {
				constant = (zend_constant*)Z_PTR_P(zv);
			} else if (SYMBOLIC_CONST_KEY2(list)) {
				zv = zend_hash_find_known_hash(EG(zend_constants), SYMBOLIC_CONST_KEY2(list));
				if (zv) {
					constant = (zend_constant*)Z_PTR_P(zv);
				}
			}

			if (!constant) {
				return (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_ANY);
			}

			return _const_op_type(&constant->value);
		}
		case SYMTYPE_NOT_NULL: {
			type = zend_resolve_symbolic_type(arena, SYMBOLIC_NOT_NULL_OP(list));
			ZEND_TYPE_FULL_MASK(type) &= ~MAY_BE_NULL;
			return type;
		}
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}
