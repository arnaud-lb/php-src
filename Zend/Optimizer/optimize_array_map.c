/*
   +----------------------------------------------------------------------+
   | Zend OPcache                                                         |
   +----------------------------------------------------------------------+
   | Copyright © The PHP Group and Contributors.                          |
   +----------------------------------------------------------------------+
   | This source file is subject to the Modified BSD License that is      |
   | bundled with this package in the file LICENSE, and is available      |
   | through the World Wide Web at <https://www.php.net/license/>.        |
   |                                                                      |
   | SPDX-License-Identifier: BSD-3-Clause                                |
   +----------------------------------------------------------------------+
*/

/* Inline array_map() calls whose callback is a partial function application
 * (PFA) or first-class callable (FCC) created in place:
 *
 *   array_map(foo($x, ?), $array)
 *
 * is rewritten to (approximately):
 *
 *   $tmp = $x;                        // memoized pre-bound argument
 *   TYPE_ASSERT 'array_map', $array
 *   $result = [];
 *   foreach ($array as $key => $value) {
 *       $result[$key] = foo($tmp, $value);
 *   }
 *
 * Pre-bound arguments are evaluated exactly once, before the loop, as the PFA
 * creation would have done. A non-literal pre-bound argument is memoized into
 * a temporary at the position of its original SEND, so that later
 * modifications of the sent variable are not observed, matching the by-value
 * capture the PFA would have performed. For the same reason we do not inline
 * when a non-literal pre-bound argument may be received by reference by the
 * callback.
 *
 * array_map() invokes the callback without strict types (as any internal
 * caller does), so functions compiled with strict_types=1 are not optimized.
 */

#include "Optimizer/zend_optimizer.h"
#include "Optimizer/zend_optimizer_internal.h"
#include "Optimizer/zend_cfg.h"
#include "zend_API.h"
#include "zend_execute.h"
#include "zend_partial.h"
#include "zend_vm.h"

#define ZEND_ARRAY_MAP_MAX_SENDS 32
#define ZEND_ARRAY_MAP_MAX_DEPTH 32

typedef struct {
	uint32_t init_fcall;    /* INIT_FCALL "array_map" */
	uint32_t pfa_init;      /* INIT_* of the callback */
	uint32_t convert;       /* CALLABLE_CONVERT(_PARTIAL) */
	uint32_t send_callback; /* SEND_VAL <callback> 1 */
	uint32_t send_array;    /* SEND_* <array> 2 */
	uint32_t do_fcall;      /* DO_ICALL */
	uint32_t placeholder;   /* SEND_PLACEHOLDER, or (uint32_t)-1 */
	bool variadic;          /* FCC / variadic placeholder */
	uint32_t num_sends;     /* pre-bound argument SENDs, excluding the placeholder */
	uint32_t sends[ZEND_ARRAY_MAP_MAX_SENDS];
	zend_function *fbc;     /* the callback, if known */
	uint32_t fbc_am_bucket; /* bucket of array_map() in EG(function_table) */
} array_map_call_info;

static bool zend_optimizer_is_init_opcode(uint8_t opcode)
{
	switch (opcode) {
		case ZEND_INIT_FCALL:
		case ZEND_INIT_FCALL_BY_NAME:
		case ZEND_INIT_NS_FCALL_BY_NAME:
		case ZEND_INIT_DYNAMIC_CALL:
		case ZEND_INIT_METHOD_CALL:
		case ZEND_INIT_STATIC_METHOD_CALL:
		case ZEND_INIT_PARENT_PROPERTY_HOOK_CALL:
		case ZEND_INIT_USER_CALL:
		case ZEND_NEW:
			return true;
		default:
			return false;
	}
}

static bool zend_optimizer_is_call_terminator_opcode(uint8_t opcode)
{
	switch (opcode) {
		case ZEND_DO_FCALL:
		case ZEND_DO_ICALL:
		case ZEND_DO_UCALL:
		case ZEND_DO_FCALL_BY_NAME:
		case ZEND_CALLABLE_CONVERT:
		case ZEND_CALLABLE_CONVERT_PARTIAL:
			return true;
		default:
			return false;
	}
}

static uint32_t zend_optimizer_get_arg_num(const zend_function *fbc, const zend_string *arg_name)
{
	for (uint32_t i = 0; i < fbc->common.num_args; i++) {
		if (zend_string_equals(fbc->common.arg_info[i].name, arg_name)) {
			return i + 1;
		}
	}
	if (fbc->common.fn_flags & ZEND_ACC_VARIADIC) {
		return fbc->common.num_args + 1;
	}
	/* Invalid argument name */
	return (uint32_t) -1;
}

/* Matches the opcode sequence of "array_map(<pfa>, <array>)", where the
 * INIT_FCALL at init_pos is known to refer to the internal array_map()
 * function. Anything unexpected fails the match. */
static bool zend_optimizer_match_array_map_call(
		const zend_op_array *op_array, uint32_t init_pos, array_map_call_info *info)
{
	uint32_t frame_stack[ZEND_ARRAY_MAP_MAX_DEPTH];
	uint32_t depth = 0;
	uint32_t i;

	memset(info, 0, sizeof(*info));
	info->init_fcall = init_pos;
	info->pfa_init = (uint32_t) -1;
	info->convert = (uint32_t) -1;
	info->send_callback = (uint32_t) -1;
	info->send_array = (uint32_t) -1;
	info->do_fcall = (uint32_t) -1;
	info->placeholder = (uint32_t) -1;

	for (i = init_pos + 1; i < op_array->last; i++) {
		const zend_op *opline = &op_array->opcodes[i];

		if (zend_optimizer_is_init_opcode(opline->opcode)) {
			if (depth >= ZEND_ARRAY_MAP_MAX_DEPTH) {
				return false;
			}
			if (depth == 0 && info->convert == (uint32_t) -1) {
				/* This may become the PFA frame: forget the argument SENDs of
				 * any preceding sibling frame, which was a plain call
				 * evaluated in place. */
				info->num_sends = 0;
				info->placeholder = (uint32_t) -1;
			}
			frame_stack[depth++] = i;
			continue;
		}

		if (zend_optimizer_is_call_terminator_opcode(opline->opcode)) {
			if (depth == 0) {
				/* This terminates the array_map() call itself */
				if (opline->opcode != ZEND_DO_ICALL
						|| info->send_array != i - 1) {
					return false;
				}
				info->do_fcall = i;
				return info->pfa_init != (uint32_t) -1;
			}
			depth--;
			if (opline->opcode == ZEND_CALLABLE_CONVERT
					|| opline->opcode == ZEND_CALLABLE_CONVERT_PARTIAL) {
				if (depth == 0) {
					if (info->convert != (uint32_t) -1) {
						/* A second FCC/PFA cannot be the array argument */
						return false;
					}
					info->convert = i;
					info->pfa_init = frame_stack[0];
				}
				/* An FCC/PFA created at a deeper level is a plain value */
			}
			continue;
		}

		if (depth == 0) {
			/* Direct argument of the array_map() call */
			switch (opline->opcode) {
				case ZEND_SEND_VAL:
				case ZEND_SEND_VAR:
					if (opline->op2_type == IS_CONST) {
						/* Named argument */
						return false;
					}
					if (opline->op2.num == 1) {
						if (info->convert != i - 1
								|| opline->opcode != ZEND_SEND_VAL
								|| opline->op1_type != op_array->opcodes[info->convert].result_type
								|| opline->op1.var != op_array->opcodes[info->convert].result.var) {
							return false;
						}
						info->send_callback = i;
					} else if (opline->op2.num == 2) {
						if (info->send_callback == (uint32_t) -1) {
							return false;
						}
						info->send_array = i;
					} else {
						return false;
					}
					break;
				case ZEND_SEND_VAL_EX:
				case ZEND_SEND_VAR_EX:
				case ZEND_SEND_REF:
				case ZEND_SEND_UNPACK:
				case ZEND_SEND_ARRAY:
				case ZEND_SEND_USER:
				case ZEND_SEND_FUNC_ARG:
				case ZEND_SEND_PLACEHOLDER:
				case ZEND_CHECK_FUNC_ARG:
				case ZEND_CHECK_UNDEF_ARGS:
					/* Unexpected way of passing arguments to array_map() */
					return false;
				default:
					/* Evaluation of an argument; left in place */
					break;
			}
		} else if (depth == 1 && info->convert == (uint32_t) -1) {
			/* Direct argument of what may become the PFA frame */
			switch (opline->opcode) {
				case ZEND_SEND_VAL:
				case ZEND_SEND_VAL_EX:
				case ZEND_SEND_VAR:
				case ZEND_SEND_VAR_EX:
					if (info->num_sends >= ZEND_ARRAY_MAP_MAX_SENDS) {
						return false;
					}
					info->sends[info->num_sends++] = i;
					break;
				case ZEND_SEND_PLACEHOLDER:
					if (info->placeholder != (uint32_t) -1) {
						/* A PFA with multiple placeholders will usually error
						 * due to a missing argument; don't optimize those. */
						return false;
					}
					info->placeholder = i;
					break;
				case ZEND_SEND_REF:
				case ZEND_SEND_UNPACK:
				case ZEND_SEND_ARRAY:
				case ZEND_SEND_USER:
				case ZEND_SEND_FUNC_ARG:
				case ZEND_CHECK_FUNC_ARG:
				case ZEND_CHECK_UNDEF_ARGS:
					return false;
				default:
					/* Evaluation of an argument; left in place */
					break;
			}
		}
	}

	return false;
}

static bool zend_optimizer_array_map_call_is_optimizable(
		const zend_op_array *op_array, zend_optimizer_ctx *ctx,
		array_map_call_info *info)
{
	const zend_op *pfa_init = &op_array->opcodes[info->pfa_init];
	const zend_op *convert = &op_array->opcodes[info->convert];
	bool is_prototype;
	uint32_t i;

	/* The bucket of array_map() in EG(function_table) is recorded in the
	 * TYPE_ASSERT literal, as done by zend_compile.c for internal function
	 * type assertions. */
	{
		const zval *lcname = CRT_CONSTANT_EX(op_array, (&op_array->opcodes[info->init_fcall]),
			op_array->opcodes[info->init_fcall].op2);
		const zval *fbc_zv = zend_hash_find(EG(function_table), Z_STR_P(lcname));
		if (!fbc_zv || ((zend_function *) Z_PTR_P(fbc_zv))->type != ZEND_INTERNAL_FUNCTION) {
			return false;
		}
		const Bucket *fbc_bucket = ZEND_CONTAINER_OF(fbc_zv, Bucket, val);
		info->fbc_am_bucket = fbc_bucket - EG(function_table)->arData;
		if (info->fbc_am_bucket == 0) {
			/* ZEND_TYPE_ASSERT requires a non-zero bucket */
			return false;
		}
	}

	switch (pfa_init->opcode) {
		case ZEND_INIT_FCALL:
		case ZEND_INIT_FCALL_BY_NAME:
		case ZEND_INIT_NS_FCALL_BY_NAME: {
			/* assert() has special compilation and calling rules; do not
			 * inline calls to it. */
			const zval *lcname = CRT_CONSTANT_EX(op_array, pfa_init, pfa_init->op2)
				+ (pfa_init->opcode != ZEND_INIT_FCALL);
			if (zend_string_equals_literal(Z_STR_P(lcname), "assert")) {
				return false;
			}
			break;
		}
		case ZEND_INIT_STATIC_METHOD_CALL:
		case ZEND_INIT_DYNAMIC_CALL:
			break;
		default:
			/* Instance method PFAs would require re-resolving the method on
			 * every iteration */
			return false;
	}

	info->fbc = zend_optimizer_get_called_func(
		ctx->script, op_array, &op_array->opcodes[info->pfa_init], &is_prototype);

	if (convert->opcode == ZEND_CALLABLE_CONVERT) {
		/* Plain FCC: all arguments are collected by a variadic placeholder */
		info->variadic = true;
	} else {
		info->variadic = (convert->extended_value & ZEND_PARTIAL_USES_VARIADIC_PLACEHOLDER) != 0;
	}

	if (info->placeholder != (uint32_t) -1 && info->variadic) {
		/* Multiple placeholders (see above) */
		return false;
	}
	if (info->placeholder == (uint32_t) -1 && !info->variadic) {
		/* Not actually a PFA */
		return false;
	}

	for (i = 0; i < info->num_sends; i++) {
		const zend_op *send = &op_array->opcodes[info->sends[i]];

		if (info->variadic && send->op2_type == IS_CONST) {
			/* Inlining would result in a positional argument after a named
			 * one: f(name: $v, ...) -> f(name: $v, $value) */
			return false;
		}

		if (send->op1_type == IS_CONST) {
			continue;
		}

		/* A non-literal pre-bound argument must be memoized before the loop.
		 * This is only possible when the callback is known to receive it by
		 * value. */
		if (!info->fbc) {
			return false;
		}
		uint32_t arg_num;
		if (send->op2_type == IS_CONST) {
			arg_num = zend_optimizer_get_arg_num(info->fbc, Z_STR_P(CRT_CONSTANT_EX(op_array, send, send->op2)));
			if (arg_num == (uint32_t) -1) {
				return false;
			}
		} else {
			arg_num = send->op2.num;
		}
		if (ARG_SHOULD_BE_SENT_BY_REF(info->fbc, arg_num)) {
			return false;
		}
	}

	if (pfa_init->opcode == ZEND_INIT_FCALL && !info->fbc && info->variadic) {
		/* We cannot recompute the used stack size for the extra argument */
		return false;
	}

	return true;
}

static uint32_t zend_optimizer_new_tmp(zend_op_array *op_array)
{
	return NUM_VAR(op_array->last_var + op_array->T++);
}

static zend_op *zend_optimizer_start_op(zend_op *opline, uint8_t opcode, uint32_t lineno)
{
	MAKE_NOP(opline);
	opline->opcode = opcode;
	opline->extended_value = 0;
	opline->lineno = lineno;
	return opline;
}

/* Rewrite one matched array_map() call into a foreach loop */
static uint32_t zend_optimizer_inline_array_map_call(
		zend_op_array *op_array, const array_map_call_info *info)
{
	/* Find how much memoized values we need, and how many named args we have */
	uint32_t num_memos = 0;
	bool named_args = false;
	for (uint32_t i = 0; i < info->num_sends; i++) {
		const zend_op *send = &op_array->opcodes[info->sends[i]];
		if (send->op1_type != IS_CONST) {
			num_memos++;
		}
		if (send->op2_type == IS_CONST) {
			named_args = true;
		}
	}
	if (info->placeholder != (uint32_t) -1
			&& op_array->opcodes[info->placeholder].op2_type == IS_CONST) {
		named_args = true;
	}
	const bool memoized_init_operand =
		(op_array->opcodes[info->pfa_init].opcode == ZEND_INIT_DYNAMIC_CALL
			|| op_array->opcodes[info->pfa_init].opcode == ZEND_INIT_STATIC_METHOD_CALL)
		&& op_array->opcodes[info->pfa_init].op2_type != IS_CONST
		&& op_array->opcodes[info->pfa_init].op2_type != IS_UNUSED;
	num_memos += memoized_init_operand;

	/* Find how much oplines we are going to insert */
	const bool result_unused = RESULT_UNUSED((&op_array->opcodes[info->do_fcall]));
	const uint32_t tail_frees = num_memos + (result_unused ? 1 : 0);

	uint32_t block_len = 3 /* INIT_ARRAY, FE_RESET_R, FE_FETCH_R */
		+ 1 /* INIT_FCALL or variants */
		+ info->num_sends + 1 /* SENDs + placeholder SEND */
		+ num_memos /* COPY_TMPs */
		+ 1 /* DO_FCALL or variants */
		+ (named_args ? 1 : 0) /* CHECK_UNDEF_ARGS */
		+ 2 /* ADD_ARRAY_ELEMENT, JMP */
		+ 1 /* FE_FREE */
		+ tail_frees /* FREEs */;

	zend_optimizer_insert_oplines(op_array, info->do_fcall + 1, block_len - 1);

	zend_op *opcodes = op_array->opcodes;

	/* Capture the original oplines before rewriting anything */
	const zend_op orig_pfa_init = opcodes[info->pfa_init];
	const zend_op orig_send_array = opcodes[info->send_array];
	const zend_op orig_do = opcodes[info->do_fcall];
	zend_op orig_placeholder = {0};
	zend_op orig_sends[ZEND_ARRAY_MAP_MAX_SENDS];
	if (info->placeholder != (uint32_t) -1) {
		orig_placeholder = opcodes[info->placeholder];
	}
	for (uint32_t i = 0; i < info->num_sends; i++) {
		orig_sends[i] = opcodes[info->sends[i]];
	}
	zend_string *array_map_name = Z_STR_P(CRT_CONSTANT_EX(op_array,
		(&opcodes[info->init_fcall]), opcodes[info->init_fcall].op2));
	const uint32_t call_lineno = opcodes[info->init_fcall].lineno;
	const uint32_t lineno = orig_do.lineno;

	/* Drop the call sequences of array_map() and of the PFA */
	MAKE_NOP(&opcodes[info->init_fcall]);
	MAKE_NOP(&opcodes[info->convert]);
	MAKE_NOP(&opcodes[info->send_callback]);
	if (info->placeholder != (uint32_t) -1) {
		MAKE_NOP(&opcodes[info->placeholder]);
	}

	/* Memoize the callable / method name of the callback */
	uint32_t init_operand_memo_var = 0;
	{
		zend_op *opline = &opcodes[info->pfa_init];
		if (memoized_init_operand) {
			init_operand_memo_var = zend_optimizer_new_tmp(op_array);
			zend_optimizer_start_op(opline, ZEND_QM_ASSIGN, opline->lineno);
			opline->op1_type = orig_pfa_init.op2_type;
			opline->op1 = orig_pfa_init.op2;
			opline->result_type = IS_TMP_VAR;
			opline->result.var = init_operand_memo_var;
		} else {
			MAKE_NOP(opline);
		}
	}

	/* Memoize non-literal pre-bound arguments in place of their SENDs, so
	 * that each value is captured at the same point the PFA would have
	 * captured it. */
	uint32_t arg_memo_vars[ZEND_ARRAY_MAP_MAX_SENDS];
	for (uint32_t i = 0; i < info->num_sends; i++) {
		zend_op *opline = &opcodes[info->sends[i]];
		if (orig_sends[i].op1_type == IS_CONST) {
			MAKE_NOP(opline);
			continue;
		}
		arg_memo_vars[i] = zend_optimizer_new_tmp(op_array);
		zend_optimizer_start_op(opline, ZEND_QM_ASSIGN, opline->lineno);
		opline->op1_type = orig_sends[i].op1_type;
		opline->op1 = orig_sends[i].op1;
		opline->result_type = IS_TMP_VAR;
		opline->result.var = arg_memo_vars[i];
	}

	/* Replace "SEND <array> 2" with the type check that array_map() would
	 * have performed on its second argument. */
	{
		zval fname;
		ZVAL_STR_COPY(&fname, array_map_name);
		zend_op *opline = zend_optimizer_start_op(&opcodes[info->send_array],
			ZEND_TYPE_ASSERT, call_lineno);
		opline->extended_value = (2 << 16) | IS_ARRAY;
		opline->op1_type = IS_CONST;
		opline->op1.constant = zend_optimizer_add_literal(op_array, &fname);
		Z_EXTRA(op_array->literals[opline->op1.constant]) = info->fbc_am_bucket;
		opline->op2_type = orig_send_array.op1_type;
		opline->op2 = orig_send_array.op1;
	}

	/* The array operand is used both by ZEND_TYPE_ASSERT above and by
	 * ZEND_FE_RESET_R below */
	uint32_t array_literal_dup = 0;
	if (orig_send_array.op1_type == IS_CONST) {
		zval copy;
		ZVAL_COPY(&copy, CRT_CONSTANT_EX(op_array, (&orig_send_array), orig_send_array.op1));
		array_literal_dup = zend_optimizer_add_literal(op_array, &copy);
	}

	/* Emit the loop in place of the DO_ICALL */
	uint32_t pos = info->do_fcall;
	const uint32_t fe_var = zend_optimizer_new_tmp(op_array);
	const uint32_t value_var = zend_optimizer_new_tmp(op_array);
	const uint32_t key_var = zend_optimizer_new_tmp(op_array);
	const uint32_t call_result_var = zend_optimizer_new_tmp(op_array);
	const uint8_t result_type = result_unused ? IS_TMP_VAR : orig_do.result_type;
	const uint32_t result_var = result_unused
		? zend_optimizer_new_tmp(op_array) : orig_do.result.var;
	const uint32_t fetch_pos = pos + 2;
	const uint32_t end_pos = pos + block_len - 1 - tail_frees; /* FE_FREE */
	zend_op *opline;

	/* RESULT = INIT_ARRAY */
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_INIT_ARRAY, lineno);
	opline->result_type = result_type;
	opline->result.var = result_var;
	pos++;

	/* FE = FE_RESET_R <array>, ->end */
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FE_RESET_R, lineno);
	if (orig_send_array.op1_type == IS_CONST) {
		opline->op1_type = IS_CONST;
		opline->op1.constant = array_literal_dup;
	} else {
		opline->op1_type = orig_send_array.op1_type;
		opline->op1 = orig_send_array.op1;
	}
	opline->result_type = IS_VAR;
	opline->result.var = fe_var;
	ZEND_SET_OP_JMP_ADDR(opline, opline->op2, &opcodes[end_pos]);
	pos++;

	/* KEY = FE_FETCH_R FE, VALUE, ->end */
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FE_FETCH_R, lineno);
	opline->op1_type = IS_VAR;
	opline->op1.var = fe_var;
	opline->op2_type = IS_TMP_VAR;
	opline->op2.var = value_var;
	opline->result_type = IS_TMP_VAR;
	opline->result.var = key_var;
	opline->extended_value = ZEND_OPLINE_TO_OFFSET(opline, &opcodes[end_pos]);
	pos++;

	/* Callback call: INIT */
	if (memoized_init_operand) {
		/* Send a copy of the memoized callable / method name */
		uint32_t copy_var = zend_optimizer_new_tmp(op_array);
		opline = zend_optimizer_start_op(&opcodes[pos], ZEND_COPY_TMP, lineno);
		opline->op1_type = IS_TMP_VAR;
		opline->op1.var = init_operand_memo_var;
		opline->result_type = IS_TMP_VAR;
		opline->result.var = copy_var;
		pos++;

		opcodes[pos] = orig_pfa_init;
		opcodes[pos].lineno = lineno;
		opcodes[pos].op2_type = IS_TMP_VAR;
		opcodes[pos].op2.var = copy_var;
	} else {
		opcodes[pos] = orig_pfa_init;
		opcodes[pos].lineno = lineno;
	}
	zend_op *loop_init = &opcodes[pos];
	loop_init->extended_value = orig_pfa_init.extended_value + (info->variadic ? 1 : 0);
	if (orig_pfa_init.opcode == ZEND_INIT_FCALL && info->fbc) {
		loop_init->op1.num = zend_vm_calc_used_stack(loop_init->extended_value, info->fbc);
	}
	pos++;

	/* Callback call: arguments, in their original order. The current element
	 * takes the place of the placeholder. */
	uint32_t next_send = 0;
	bool placeholder_sent = false;
	while (next_send < info->num_sends || !placeholder_sent) {
		if (!placeholder_sent
				&& info->placeholder != (uint32_t) -1
				&& (next_send >= info->num_sends
					|| info->placeholder < info->sends[next_send])) {
			opline = zend_optimizer_start_op(&opcodes[pos], ZEND_SEND_VAL_EX, lineno);
			opline->op1_type = IS_TMP_VAR;
			opline->op1.var = value_var;
			opline->op2_type = orig_placeholder.op2_type;
			opline->op2 = orig_placeholder.op2;
			/* The result of a SEND holds the call frame slot of the argument,
			 * or the cache slots of a named argument. SEND_PLACEHOLDER uses
			 * the same convention, so reuse it. */
			opline->result.num = orig_placeholder.result.num;
			placeholder_sent = true;
			pos++;
			continue;
		}
		if (next_send >= info->num_sends) {
			/* Variadic placeholder: the element is appended as the last,
			 * positional argument */
			ZEND_ASSERT(info->variadic && !placeholder_sent);
			opline = zend_optimizer_start_op(&opcodes[pos], ZEND_SEND_VAL_EX, lineno);
			opline->op1_type = IS_TMP_VAR;
			opline->op1.var = value_var;
			opline->op2.num = orig_pfa_init.extended_value + 1;
			opline->result.var = NUM_VAR(opline->op2.num - 1);
			placeholder_sent = true;
			pos++;
			continue;
		}

		const zend_op *orig_send = &orig_sends[next_send];
		if (orig_send->op1_type == IS_CONST) {
			/* Literal argument: re-send it as-is */
			opcodes[pos] = *orig_send;
			opcodes[pos].lineno = lineno;
			pos++;
		} else {
			/* Memoized argument: send a copy of the memoized temporary */
			uint32_t copy_var = zend_optimizer_new_tmp(op_array);
			opline = zend_optimizer_start_op(&opcodes[pos], ZEND_COPY_TMP, lineno);
			opline->op1_type = IS_TMP_VAR;
			opline->op1.var = arg_memo_vars[next_send];
			opline->result_type = IS_TMP_VAR;
			opline->result.var = copy_var;
			pos++;

			opline = zend_optimizer_start_op(&opcodes[pos], ZEND_SEND_VAL_EX, lineno);
			opline->op1_type = IS_TMP_VAR;
			opline->op1.var = copy_var;
			opline->op2_type = orig_send->op2_type;
			opline->op2 = orig_send->op2;
			/* Call frame slot, or the cache slots of a named argument */
			opline->result.num = orig_send->result.num;
			pos++;
		}
		next_send++;
	}

	/* Callback call: DO */
	opline = zend_optimizer_start_op(&opcodes[pos],
		zend_get_call_op(loop_init, info->fbc, /* result_used */ true), lineno);
	opline->result_type = IS_TMP_VAR;
	opline->result.var = call_result_var;
	pos++;

	if (named_args) {
		zend_optimizer_start_op(&opcodes[pos], ZEND_CHECK_UNDEF_ARGS, lineno);
		pos++;
	}

	/* RESULT[KEY] = call result */
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_ADD_ARRAY_ELEMENT, lineno);
	opline->op1_type = IS_TMP_VAR;
	opline->op1.var = call_result_var;
	opline->op2_type = IS_TMP_VAR;
	opline->op2.var = key_var;
	opline->result_type = result_type;
	opline->result.var = result_var;
	pos++;

	/* JMP ->fetch */
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_JMP, lineno);
	ZEND_SET_OP_JMP_ADDR(opline, opline->op1, &opcodes[fetch_pos]);
	pos++;

	/* FE_FREE FE */
	ZEND_ASSERT(pos == end_pos);
	opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FE_FREE, lineno);
	opline->op1_type = IS_VAR;
	opline->op1.var = fe_var;
	pos++;

	/* Free the memoized values */
	if (memoized_init_operand) {
		opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FREE, lineno);
		opline->op1_type = IS_TMP_VAR;
		opline->op1.var = init_operand_memo_var;
		pos++;
	}
	for (uint32_t i = 0; i < info->num_sends; i++) {
		if (orig_sends[i].op1_type != IS_CONST) {
			opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FREE, lineno);
			opline->op1_type = IS_TMP_VAR;
			opline->op1.var = arg_memo_vars[i];
			pos++;
		}
	}

	/* Free the unused result */
	if (result_unused) {
		opline = zend_optimizer_start_op(&opcodes[pos], ZEND_FREE, lineno);
		opline->op1_type = result_type;
		opline->op1.var = result_var;
		pos++;
	}

	ZEND_ASSERT(pos == info->do_fcall + block_len);
	return pos;
}

void zend_optimize_array_map_calls(zend_op_array *op_array, zend_optimizer_ctx *ctx)
{
	uint32_t i;
	bool changed = false;

	/* array_map() invokes its callback without strict types, a direct call
	 * would not. */
	if (op_array->fn_flags & ZEND_ACC_STRICT_TYPES) {
		return;
	}

	for (i = 0; i + 4 < op_array->last; i++) {
		const zend_op *opline = &op_array->opcodes[i];
		array_map_call_info info;

		if (opline->opcode != ZEND_INIT_FCALL
				|| opline->extended_value != 2
				|| !zend_string_equals_literal(Z_STR_P(CRT_CONSTANT(opline->op2)), "array_map")) {
			continue;
		}

		if (!zend_optimizer_match_array_map_call(op_array, i, &info)
				|| !zend_optimizer_array_map_call_is_optimizable(op_array, ctx, &info)) {
			continue;
		}

		zend_optimizer_inline_array_map_call(op_array, &info);
		changed = true;
		/* The array argument may contain a nested array_map() call, which is
		 * now part of the code preceding the loop: rescan the region. */
	}

	if (changed && !op_array->live_range) {
		/* The new foreach loop variable needs a live-range. Force the
		 * recalculation that runs after the optimizer, which is otherwise
		 * skipped for op_arrays that had none. */
		op_array->live_range = emalloc(sizeof(zend_live_range));
		op_array->last_live_range = 0;
	}
}
