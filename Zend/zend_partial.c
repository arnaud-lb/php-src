/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) Zend Technologies Ltd. (http://www.zend.com)           |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_API.h"
#include "zend_arena.h"
#include "zend_ast.h"
#include "zend_compile.h"
#include "zend_closures.h"
#include "zend_attributes.h"
#include "zend_exceptions.h"
#include "ext/opcache/ZendAccelerator.h"

#define Z_IS_PLACEHOLDER_ARG_P(p) \
	(Z_TYPE_P(p) == _IS_PLACEHOLDER_ARG)

#define Z_IS_PLACEHOLDER_VARIADIC_P(p) \
	(Z_TYPE_P(p) == _IS_PLACEHOLDER_VARIADIC)

#define Z_IS_PLACEHOLDER_P(p) \
	(Z_IS_PLACEHOLDER_ARG_P(p) || Z_IS_PLACEHOLDER_VARIADIC_P(p))

#define IS_STATIC_CLOSURE(function) \
	(((function)->common.fn_flags & (ZEND_ACC_STATIC|ZEND_ACC_CLOSURE)) == (ZEND_ACC_STATIC|ZEND_ACC_CLOSURE))

static zend_string* zend_partial_symbol_name(zend_execute_data *call, zend_function *function) {
	zend_string *name = function->common.function_name,
				*scope = (function->common.scope ? function->common.scope->name : NULL),
				*symbol;

	if (scope) {
		symbol = zend_create_member_string(scope, name);
	} else {
		symbol = zend_string_copy(name);
	}

	return symbol;
}

static zend_always_inline void zend_partial_args_underflow(
		zend_function *function, zend_string *symbol, uint32_t args,
		uint32_t expected)
{
	const char *limit = function->common.num_args <= function->common.required_num_args ?
			"exactly" : "at least";

	zend_throw_error(NULL,
		"not enough arguments or placeholders for application of %s, "
		"%d given and %s %d expected",
		ZSTR_VAL(symbol), args, limit, expected);
}

static zend_always_inline void zend_partial_args_overflow(
		zend_function *function, zend_string *symbol, uint32_t args,
		uint32_t expected)
{
	if (function->type == ZEND_USER_FUNCTION) {
		zend_throw_error(NULL,
			"too many arguments or placeholders for application of %s, "
			"%d given and a maximum of %d expected",
			ZSTR_VAL(symbol), args, expected);
	} else {
		zend_throw_error(NULL,
			"too many arguments or placeholders for application of %s, "
			"%d given and a maximum of %d expected",
			ZSTR_VAL(symbol), args, expected);
	}
}

void zend_partial_args_check(zend_execute_data *call) {
	/* this is invoked by VM before the creation of partial */
	// TODO: merge with zend_partial_create()?

	if (ZEND_CALL_INFO(call) & ZEND_CALL_HAS_EXTRA_NAMED_PARAMS) {
		zend_array *named_args = call->extra_named_params;
		zval *arg;
		zend_string *key;
		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(named_args, key, arg) {
			if (UNEXPECTED(Z_TYPE_P(arg) == _IS_PLACEHOLDER_ARG)) {
				zend_throw_error(NULL,
						"Cannot use named placeholder for unknown or variadic parameter $%s",
						ZSTR_VAL(key));
				return;
			}
		} ZEND_HASH_FOREACH_END();
	}

	zend_function *function = call->func;

	/* Z_EXTRA(ZEND_CALL_ARG(call, 1)) is set by ZEND_SEND_PLACEHOLDER */
	bool variadic = Z_EXTRA_P(ZEND_CALL_ARG(call, 1)) == _IS_PLACEHOLDER_VARIADIC;

	uint32_t num = ZEND_CALL_NUM_ARGS(call) + (variadic ? -1 : 0);

	if (num < function->common.required_num_args) {
		if (variadic) {
			return;
		}

		zend_string *symbol = zend_partial_symbol_name(call, function);
		zend_partial_args_underflow(
			function, symbol,
			num, function->common.required_num_args);
		zend_string_release(symbol);
	} else if (num > function->common.num_args &&
			!(function->common.fn_flags & ZEND_ACC_VARIADIC)) {
		zend_string *symbol = zend_partial_symbol_name(call, function);
		zend_partial_args_overflow(
			function, symbol,
			num, function->common.num_args);
		zend_string_release(symbol);
	}
}

static bool zp_name_exists(zend_string **names, uint32_t num_names, zend_string *name)
{
	for (uint32_t i = 0; i < num_names; i++) {
		if (names[i] && zend_string_equals(names[i], name)) {
			return true;
		}
	}
	return false;
}

static zend_string *zp_get_param_name(zend_function *function, uint32_t arg_offset)
{
	zend_arg_info *arg_info = &function->common.arg_info[arg_offset];

	if (function->type == ZEND_USER_FUNCTION
			|| (function->common.fn_flags & ZEND_ACC_USER_ARG_INFO)) {
		return zend_string_copy(arg_info->name);
	} else {
		const char *name = ((zend_internal_arg_info*)arg_info)->name;
		return zend_string_init(name, strlen(name), false);
	}
}

static void zp_assign_names(zend_string **names, uint32_t num_names,
		uint32_t argc, zval *argv,
		zend_function *function, bool variadic_partial,
		zend_array *extra_named_params)
{
	/* Assign names for normal params. We never rename those. */
	for (uint32_t offset = 0; offset < MIN(argc, function->common.num_args); offset++) {
		if (Z_IS_PLACEHOLDER_P(&argv[offset])) {
			names[offset] = zp_get_param_name(function, offset);
		}
	}

	/* Assign names for the variadic param. Never renamed. */
	if (variadic_partial && (function->common.fn_flags & ZEND_ACC_VARIADIC)) {
		names[argc] = zp_get_param_name(function, function->common.num_args);
	}

	/* Assign names for placeholders that run into variadic.
	 * By default these are named $origNameN with N the offset from the
	 * variadic param. In case of clash we increment N until a free name is
	 * found. */
	for (uint32_t offset = function->common.num_args; offset < argc; offset++) {
		ZEND_ASSERT(function->common.fn_flags & ZEND_ACC_VARIADIC);
		if (!Z_IS_PLACEHOLDER_P(&argv[offset])) {
			continue;
		}
		int n = offset - function->common.num_args;
		zend_string *orig_name = zp_get_param_name(function, function->common.num_args);
		zend_string *new_name;
		do {
			new_name = zend_strpprintf_unchecked(0, "%S%d", orig_name, n);
			if (!zp_name_exists(names, num_names, new_name)) {
				break;
			}
			n++;
			zend_string_release(new_name);
		} while (true);
		names[offset] = new_name;
		zend_string_release(orig_name);
	}

	/* Assign names for pre-bound params (lexical vars).
	 * There may be clashes, we ensure to generate unique names. */
	for (uint32_t offset = 0; offset < argc; offset++) {
		if (Z_IS_PLACEHOLDER_P(&argv[offset]) || Z_ISUNDEF(argv[offset])) {
			continue;
		}
		int n = -1;
		zend_string *orig_name = zp_get_param_name(function, MIN(offset, function->common.num_args));
		zend_string *new_name = zend_string_copy(orig_name);
		while (zp_name_exists(names, num_names, new_name)) {
			zend_string_release(new_name);
			n++;
			new_name = zend_strpprintf_unchecked(0, "%S%d", orig_name, n);
		}
		names[offset] = new_name;
		zend_string_release(orig_name);
	}

	/* Assign name for $extra_named_params */
	if (extra_named_params) {
		int n = 1;
		zend_string *new_name = ZSTR_INIT_LITERAL("extra_named_params", 0);
		while (zp_name_exists(names, num_names, new_name)) {
			zend_string_release(new_name);
			n++;
			new_name = zend_strpprintf(0, "%s%d", "extra_named_params", n);
		}
		names[argc + variadic_partial] = new_name;
	}

	/* Assign name for $fn */
	if (function->common.fn_flags & ZEND_ACC_CLOSURE) {
		int n = 1;
		zend_string *new_name = ZSTR_INIT_LITERAL("fn", 0);
		while (zp_name_exists(names, num_names, new_name)) {
			zend_string_release(new_name);
			n++;
			new_name = zend_strpprintf(0, "%s%d", "fn", n);
		}
		names[argc + variadic_partial + (extra_named_params != NULL)] = new_name;
	}
}

static bool zp_is_single_may_be_type(uint32_t type_mask)
{
	return ((type_mask > 0) && (type_mask & (type_mask - 1)) == 0)
		|| type_mask == MAY_BE_BOOL
		|| type_mask == MAY_BE_ANY;
}

static zend_ast *zp_single_may_be_type_to_ast(uint32_t type)
{
	zend_string *name;

	switch (type) {
		case MAY_BE_NULL:
			name = ZSTR_KNOWN(ZEND_STR_NULL_LOWERCASE);
			break;
		case MAY_BE_TRUE:
			name = ZSTR_KNOWN(ZEND_STR_TRUE);
			break;
		case MAY_BE_FALSE:
			name = ZSTR_KNOWN(ZEND_STR_FALSE);
			break;
		case MAY_BE_LONG:
			name = ZSTR_KNOWN(ZEND_STR_INT);
			break;
		case MAY_BE_DOUBLE:
			name = ZSTR_KNOWN(ZEND_STR_FLOAT);
			break;
		case MAY_BE_STRING:
			name = ZSTR_KNOWN(ZEND_STR_STRING);
			break;
		case MAY_BE_BOOL:
			name = ZSTR_KNOWN(ZEND_STR_BOOL);
			break;
		case MAY_BE_VOID:
			name = ZSTR_KNOWN(ZEND_STR_VOID);
			break;
		case MAY_BE_NEVER:
			name = ZSTR_KNOWN(ZEND_STR_NEVER);
			break;
		case MAY_BE_OBJECT:
			name = ZSTR_KNOWN(ZEND_STR_OBJECT);
			break;
		case MAY_BE_ANY:
			name = ZSTR_KNOWN(ZEND_STR_MIXED);
			break;
		case MAY_BE_CALLABLE:
			return zend_ast_create_ex(ZEND_AST_TYPE, IS_CALLABLE);
		case MAY_BE_ARRAY:
			return zend_ast_create_ex(ZEND_AST_TYPE, IS_ARRAY);
		case MAY_BE_STATIC:
			return zend_ast_create_ex(ZEND_AST_TYPE, IS_STATIC);
		EMPTY_SWITCH_DEFAULT_CASE()
	}

	zend_ast *ast = zend_ast_create_zval_from_str(name);
	ast->attr = ZEND_NAME_NOT_FQ;

	return ast;
}

static zend_ast *zp_type_name_to_ast(zend_string *name)
{
	zend_ast *ast = zend_ast_create_zval_from_str(name);

	if (zend_get_class_fetch_type(name) != ZEND_FETCH_CLASS_DEFAULT) {
		ast->attr = ZEND_NAME_NOT_FQ;
	} else {
		ast->attr = ZEND_NAME_FQ;
	}

	return ast;
}

static zend_ast *zp_type_to_ast(const zend_type type)
{
	if (!ZEND_TYPE_IS_SET(type)) {
		return NULL;
	}

	if (ZEND_TYPE_IS_UNION(type)
			|| (ZEND_TYPE_IS_COMPLEX(type) && ZEND_TYPE_PURE_MASK(type))
			|| (ZEND_TYPE_PURE_MASK(type) && !zp_is_single_may_be_type(ZEND_TYPE_PURE_MASK(type)))) {
		zend_ast *type_ast = zend_ast_create_list(0, ZEND_AST_TYPE_UNION);
		if (ZEND_TYPE_HAS_LIST(type)) {
			const zend_type *type_ptr;
			ZEND_TYPE_LIST_FOREACH(ZEND_TYPE_LIST(type), type_ptr) {
				type_ast = zend_ast_list_add(type_ast, zp_type_to_ast(*type_ptr));
			} ZEND_TYPE_LIST_FOREACH_END();
		} else if (ZEND_TYPE_HAS_NAME(type)) {
			zend_ast *name_ast = zp_type_name_to_ast(
					zend_string_copy(ZEND_TYPE_NAME(type)));
			type_ast = zend_ast_list_add(type_ast, name_ast);
		} else if (ZEND_TYPE_IS_COMPLEX(type)) {
			ZEND_UNREACHABLE();
		}
		uint32_t type_mask = ZEND_TYPE_PURE_MASK(type);
		if ((type_mask & MAY_BE_BOOL) == MAY_BE_BOOL) {
			type_ast = zend_ast_list_add(type_ast, zp_single_may_be_type_to_ast(MAY_BE_BOOL));
			type_mask &= ~MAY_BE_BOOL;
		}
		for (uint32_t may_be_type = 1; may_be_type < _ZEND_TYPE_MAY_BE_MASK; may_be_type <<= 1) {
			if (type_mask & may_be_type) {
				type_ast = zend_ast_list_add(type_ast, zp_single_may_be_type_to_ast(may_be_type));
			}
		}
		return type_ast;
	}

	if (ZEND_TYPE_IS_INTERSECTION(type)) {
		zend_ast *type_ast = zend_ast_create_list(0, ZEND_AST_TYPE_INTERSECTION);
		const zend_type *type_ptr;
		ZEND_TYPE_LIST_FOREACH(ZEND_TYPE_LIST(type), type_ptr) {
			type_ast = zend_ast_list_add(type_ast, zp_type_to_ast(*type_ptr));
		} ZEND_TYPE_LIST_FOREACH_END();
		ZEND_ASSERT(!ZEND_TYPE_PURE_MASK(type));
		return type_ast;
	}

	if (ZEND_TYPE_HAS_NAME(type)) {
		zend_ast *type_ast = zp_type_name_to_ast(
				zend_string_copy(ZEND_TYPE_NAME(type)));
		return type_ast;
	}

	ZEND_ASSERT(!ZEND_TYPE_IS_COMPLEX(type));

	uint32_t type_mask = ZEND_TYPE_PURE_MASK(type);
	ZEND_ASSERT(zp_is_single_may_be_type(type_mask));

	return zp_single_may_be_type_to_ast(type_mask);
}

static zend_result zp_get_param_default_value(zval *result, zend_function *function, uint32_t arg_offset)
{
	ZEND_ASSERT(arg_offset < function->op_array.last);

	if (function->type == ZEND_USER_FUNCTION) {
		zend_op *opline = &function->op_array.opcodes[arg_offset];
		if (EXPECTED(opline->opcode == ZEND_RECV_INIT)) {
			ZVAL_COPY(result, RT_CONSTANT(opline, opline->op2));
			return SUCCESS;
		} else {
			ZEND_ASSERT(opline->opcode == ZEND_RECV);
		}
	} else if (function->type == ZEND_INTERNAL_FUNCTION) {
		if (function->common.fn_flags & ZEND_ACC_USER_ARG_INFO) {
			goto error;
		}

		zend_internal_arg_info *arg_info = &function->internal_function.arg_info[arg_offset];

		if (zend_get_default_from_internal_arg_info(result, arg_info) == SUCCESS) {
			return SUCCESS;
		}
	}

error:
	zend_argument_error(zend_ce_argument_count_error, arg_offset + 1,
			"must be passed explicitly, because the default value is not known");

	return FAILURE;
}

static bool zp_arg_must_be_sent_by_ref(zend_function *function, uint32_t arg_num)
{
	if (EXPECTED(arg_num <= MAX_ARG_FLAG_NUM)) {
		if (QUICK_ARG_MUST_BE_SENT_BY_REF(function, arg_num)) {
			return true;
		}
	} else if (ARG_MUST_BE_SENT_BY_REF(function, arg_num)) {
		return true;
	}
	return false;
}

static zend_ast *zp_attribute_to_ast(zend_attribute *attribute)
{
	zend_ast *args_ast;
	if (attribute->argc) {
		args_ast = zend_ast_create_arg_list(0, ZEND_AST_ARG_LIST);
		for (uint32_t i = 0; i < attribute->argc; i++) {
			zend_ast *arg_ast = zend_ast_create_zval(&attribute->args[i].value);
			if (attribute->args[i].name) {
				arg_ast = zend_ast_create(ZEND_AST_NAMED_ARG,
						zend_ast_create_zval_from_str(
							zend_string_copy(attribute->args[i].name)),
						arg_ast);
			}
			args_ast = zend_ast_list_add(args_ast, arg_ast);
		}
	} else {
		args_ast = NULL;
	}
	return zend_ast_create(ZEND_AST_ATTRIBUTE,
			zend_ast_create_zval_from_str(zend_string_copy(attribute->name)),
			args_ast);
}

static zend_ast *zp_param_attributes_to_ast(zend_function *function,
		uint32_t offset)
{
	zend_ast *attributes_ast = NULL;
	if (!function->common.attributes) {
		return NULL;
	}

	/* Inherit the SensitiveParameter attribute */
	zend_attribute *attr = zend_get_parameter_attribute_str(
			function->common.attributes,
			"sensitiveparameter", strlen("sensitiveparameter"), offset);
	if (attr) {
		attributes_ast = zend_ast_create_list(1, ZEND_AST_ATTRIBUTE_GROUP,
				zp_attribute_to_ast(attr));
	}

	if (attributes_ast) {
		attributes_ast = zend_ast_create_list(1, ZEND_AST_ATTRIBUTE_LIST,
				attributes_ast);
	}

	return attributes_ast;
}

zend_op_array *zp_compile(zval *this_ptr, zend_function *function,
		uint32_t argc, zval *argv, zend_array *extra_named_params,
		const zend_op_array *declaring_op_array,
		const zend_op *declaring_opline, void **cache_slot) {

	zend_class_entry *called_scope;

	if (Z_TYPE_P(this_ptr) == IS_OBJECT) {
		called_scope = Z_OBJCE_P(this_ptr);
	} else {
		called_scope = Z_CE_P(this_ptr);
	}

	if (EXPECTED(cache_slot[0] == function)) {
		return cache_slot[1];
	}

	zend_op_array *op_array = zend_accel_pfa_cache_get(declaring_op_array,
			declaring_opline, function);

	if (EXPECTED(op_array)) {
		cache_slot[0] = function;
		cache_slot[1] = op_array;
		return op_array;
	}

	zend_arena *orig_ast_arena = CG(ast_arena);
	CG(ast_arena) = zend_arena_create(1024 * 32);

	int orig_lineno = CG(zend_lineno);
	CG(zend_lineno) = zend_get_executed_lineno();

	zend_ast *params_ast = zend_ast_create_list(0, ZEND_AST_ARG_LIST);
	zend_ast *lexical_vars_ast = zend_ast_create_list(0, ZEND_AST_CLOSURE_USES);

	zend_ast *stmts_ast = zend_ast_create_list(0, ZEND_AST_STMT_LIST);

	bool variadic_partial = false;
	int new_argc = argc;

	/* Z_EXTRA(ZEND_CALL_ARG(call, 1)) is set in ZEND_SEND_PLACEHOLDER */
	if (Z_EXTRA(argv[0]) == _IS_PLACEHOLDER_VARIADIC) {
		variadic_partial = true;
		if (Z_IS_PLACEHOLDER_VARIADIC_P(&argv[argc-1])
				&& argc > function->common.num_args) {
			/* A variadic placeholder at this position is only used to mark the
			 * PFA variadic. We have taken note that the PFA is variadic.
			 * The placeholder stands for the variadoc placeholder. No other
			 * placeholders can follow. */
			argc--;
			new_argc--;
		}
		new_argc = MAX(new_argc, function->common.num_args);
	}

	zval *tmp = zend_arena_alloc(&CG(arena), new_argc * sizeof(zval));
	memcpy(tmp, argv, new_argc * sizeof(zval));
	argv = tmp;

	if (variadic_partial) {
		/* All unspecified parameters become placeholders */
		for (uint32_t i = 0; i < argc; i++) {
			if (Z_ISUNDEF(argv[i])) {
				Z_TYPE_INFO(argv[i]) = _IS_PLACEHOLDER_ARG;
			}
		}
		for (uint32_t i = argc; i < new_argc; i++) {
			Z_TYPE_INFO(argv[i]) = _IS_PLACEHOLDER_ARG;
		}
	}

	argc = new_argc;

	/* Assign param names */
	uint32_t num_names = argc + variadic_partial + (extra_named_params != NULL)
		+ ((function->common.fn_flags & ZEND_ACC_CLOSURE) != 0);
	zend_string **param_names = zend_arena_alloc(&CG(arena),
			sizeof(zend_string*) * num_names);
	memset(param_names, 0, sizeof(zend_string*) * num_names);
	zp_assign_names(param_names, num_names, argc, argv, function,
			variadic_partial, extra_named_params);

	/* Compute number of required args */
	uint32_t num_args = 0, num_required = 0;
	for (uint32_t offset = 0; offset < argc; offset++) {
		if (!Z_IS_PLACEHOLDER_P(&argv[offset])) {
			continue;
		}
		if (offset < function->common.num_args) {
			num_args++;
			if (offset < function->common.required_num_args) {
				num_required = num_args;
			}
		} else {
			ZEND_ASSERT(function->common.fn_flags & ZEND_ACC_VARIADIC);
			num_args++;
			/* Placeholders that run into the variadic portion become
			 * required and make all params before them required */
			num_required = num_args;
		}
	}

	/* Generate params and lexical vars AST */
	uint32_t arg_num = 0;
	for (uint32_t offset = 0; offset < argc; offset++) {
		if (Z_IS_PLACEHOLDER_P(&argv[offset])) {
			arg_num++;
			if (offset < function->common.num_args) {
				zend_arg_info *arg_info = &function->common.arg_info[offset];
				int param_flags = 0;
				if (zp_arg_must_be_sent_by_ref(function, offset+1)) {
					param_flags |= ZEND_PARAM_REF;
				}
				zend_ast *param_type_ast = zp_type_to_ast(arg_info->type);
				zend_ast *default_value_ast = NULL;
				if (arg_num > num_required) {
					zval default_value;
					if (zp_get_param_default_value(&default_value, function, offset) == FAILURE) {
						return NULL;
					}
					default_value_ast = zend_ast_create_zval(&default_value);
				}
				zend_ast *attributes_ast = zp_param_attributes_to_ast(function, offset);
				params_ast = zend_ast_list_add(params_ast, zend_ast_create_ex(ZEND_AST_PARAM,
						param_flags, param_type_ast,
						zend_ast_create_zval_from_str(param_names[offset]),
						default_value_ast, attributes_ast, NULL, NULL));
			} else {
				ZEND_ASSERT(function->common.fn_flags & ZEND_ACC_VARIADIC);
				zend_arg_info *arg_info = &function->common.arg_info[function->common.num_args];
				int param_flags = 0;
				if (zp_arg_must_be_sent_by_ref(function, offset+1)) {
					param_flags |= ZEND_PARAM_REF;
				}
				zend_ast *param_type_ast = zp_type_to_ast(arg_info->type);
				params_ast = zend_ast_list_add(params_ast, zend_ast_create_ex(ZEND_AST_PARAM,
						param_flags, param_type_ast,
						zend_ast_create_zval_from_str(param_names[offset]),
						NULL, NULL, NULL, NULL));
			}
		} else if (!Z_ISUNDEF(argv[offset])) {
			// TODO: If the pre-bound parameter is a literal, it can be a
			// literal in the function body instead of a lexical var.
			zend_ast *lexical_var_ast = zend_ast_create_zval_from_str(param_names[offset]);
			if (zp_arg_must_be_sent_by_ref(function, offset+1)) {
				lexical_var_ast->attr = ZEND_BIND_REF;
			}
			lexical_vars_ast = zend_ast_list_add(
					lexical_vars_ast, lexical_var_ast);
		}
	}

	if (extra_named_params) {
		zend_ast *lexical_var_ast = zend_ast_create_zval_from_str(param_names[argc + variadic_partial]);
		lexical_vars_ast = zend_ast_list_add(lexical_vars_ast, lexical_var_ast);
	}

	if (function->common.fn_flags & ZEND_ACC_CLOSURE) {
		zend_ast *lexical_var_ast = zend_ast_create_zval_from_str(param_names[argc + variadic_partial + (extra_named_params != NULL)]);
		lexical_vars_ast = zend_ast_list_add(lexical_vars_ast, lexical_var_ast);
	}

	/* If we have a variadic placeholder and the underlying function is
	 * variadic, add a variadic param. */
	if (variadic_partial
			&& (function->common.fn_flags & ZEND_ACC_VARIADIC)) {
		zend_arg_info *arg_info = &function->common.arg_info[function->common.num_args];
		int param_flags = ZEND_PARAM_VARIADIC;
		if (zp_arg_must_be_sent_by_ref(function, function->common.num_args+1)) {
			param_flags |= ZEND_PARAM_REF;
		}
		zend_ast *param_type_ast = zp_type_to_ast(arg_info->type);
		params_ast = zend_ast_list_add(params_ast, zend_ast_create_ex(ZEND_AST_PARAM,
				param_flags, param_type_ast,
				zend_ast_create_zval_from_str(param_names[argc]),
				NULL, NULL, NULL, NULL));
	}

	/* Generate function body */
	zend_ast *args_ast = zend_ast_create_list(0, ZEND_AST_ARG_LIST);
	for (uint32_t offset = 0; offset < argc; offset++) {
		if (Z_ISUNDEF(argv[offset])) {
			/* Argument was not passed. Pass its default value. */
			if (offset < function->common.required_num_args) {
				zend_argument_error(zend_ce_argument_count_error, offset + 1, "not passed");
				// TODO: proper cleanup
				goto clean_argv;
			}
			zval default_value;
			if (zp_get_param_default_value(&default_value, function, offset) == FAILURE) {
				// TODO: proper cleanup
				goto clean_argv;
			}
			zend_ast *default_value_ast;
			if (Z_TYPE(default_value) == IS_CONSTANT_AST) {
				default_value_ast = zend_ast_dup(Z_ASTVAL(default_value));
			} else {
				default_value_ast = zend_ast_create_zval(&default_value);
			}
			args_ast = zend_ast_list_add(args_ast, default_value_ast);
		} else {
			args_ast = zend_ast_list_add(args_ast, zend_ast_create(ZEND_AST_VAR,
						zend_ast_create_zval_from_str(zend_string_copy(param_names[offset]))));
		}
	}
	if (extra_named_params) {
		args_ast = zend_ast_list_add(args_ast, zend_ast_create(ZEND_AST_UNPACK,
					zend_ast_create(ZEND_AST_VAR,
						zend_ast_create_zval_from_str(zend_string_copy(param_names[argc + variadic_partial])))));
	}
	if (variadic_partial && (function->common.fn_flags & ZEND_ACC_VARIADIC)) {
		args_ast = zend_ast_list_add(args_ast, zend_ast_create(ZEND_AST_UNPACK,
					zend_ast_create(ZEND_AST_VAR,
						zend_ast_create_zval_from_str(zend_string_copy(param_names[argc])))));
	}

	zend_ast *call_ast;
	if (function->common.fn_flags & ZEND_ACC_CLOSURE) {
		zend_ast *fn_ast = zend_ast_create(ZEND_AST_VAR,
					zend_ast_create_zval_from_str(zend_string_copy(param_names[argc + variadic_partial + (extra_named_params != NULL)])));
		call_ast = zend_ast_create(ZEND_AST_CALL, fn_ast, args_ast);
	} else if (Z_TYPE_P(this_ptr) == IS_OBJECT) {
		zend_ast *this_ast = zend_ast_create(ZEND_AST_VAR,
					zend_ast_create_zval_from_str(ZSTR_KNOWN(ZEND_STR_THIS)));
		zend_ast *method_name_ast = zend_ast_create_zval_from_str(
				zend_string_copy(function->common.function_name));
		call_ast = zend_ast_create(ZEND_AST_METHOD_CALL, this_ast,
				method_name_ast, args_ast);
	} else if (called_scope) {
		zend_ast *class_name_ast = zend_ast_create_zval_from_str(ZSTR_KNOWN(ZEND_STR_STATIC));
		class_name_ast->attr = ZEND_NAME_NOT_FQ;
		zend_ast *method_name_ast = zend_ast_create_zval_from_str(
				zend_string_copy(function->common.function_name));
		call_ast = zend_ast_create(ZEND_AST_STATIC_CALL, class_name_ast,
				method_name_ast, args_ast);
	} else {
		zend_ast *func_name_ast = zend_ast_create_zval_from_str(zend_string_copy(function->common.function_name));
		func_name_ast->attr = ZEND_NAME_FQ;
		call_ast = zend_ast_create(ZEND_AST_CALL, func_name_ast, args_ast);
	}

	zend_ast *return_type_ast = NULL;
	zend_type return_type = {0};
	if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
		return_type = (function->common.arg_info-1)->type;
		return_type_ast = zp_type_to_ast(return_type);
	}

	/* Void functions can not 'return $expr' */
	if (ZEND_TYPE_FULL_MASK(return_type) & MAY_BE_VOID) {
		stmts_ast = zend_ast_list_add(stmts_ast, call_ast);
	} else {
		zend_ast *return_ast = zend_ast_create(ZEND_AST_RETURN, call_ast);
		stmts_ast = zend_ast_list_add(stmts_ast, return_ast);
	}

	/* Inherit the NoDiscard attribute */
	zend_ast *attributes_ast = NULL;
	if (function->common.attributes) {
		zend_attribute *attr = zend_get_attribute_str(
				function->common.attributes, "nodiscard", strlen("nodiscard"));
		if (attr) {
			attributes_ast = zend_ast_create_list(1, ZEND_AST_ATTRIBUTE_GROUP,
					zp_attribute_to_ast(attr));
		}
		if (attributes_ast) {
			attributes_ast = zend_ast_create_list(1, ZEND_AST_ATTRIBUTE_LIST,
					attributes_ast);
		}
	}

	int closure_flags = 0;
	zend_ast *closure_ast = zend_ast_create_decl(ZEND_AST_CLOSURE,
			closure_flags, CG(zend_lineno), NULL,
			NULL, params_ast, lexical_vars_ast, stmts_ast,
			return_type_ast, attributes_ast);

	if (Z_TYPE_P(this_ptr) != IS_OBJECT || IS_STATIC_CLOSURE(function)) {
		((zend_ast_decl*)closure_ast)->flags |= ZEND_ACC_STATIC;
	}

#if ZEND_DEBUG
	{
		const char *tmp = getenv("DUMP_PFA_AST");
		if (tmp && ZEND_ATOL(tmp)) {
			zend_string *str = zend_ast_export("", closure_ast, "");
			fprintf(stderr, "PFA AST: %s\n", ZSTR_VAL(str));
			zend_string_release(str);
		}
	}
#endif

	op_array = zend_accel_compile_pfa(closure_ast, declaring_op_array,
			declaring_opline, function);

	zend_ast_destroy(closure_ast);

clean_argv:
	zend_arena_destroy(CG(ast_arena));
	CG(ast_arena) = orig_ast_arena;
	CG(zend_lineno) = orig_lineno;

	if (op_array) {
		cache_slot[0] = function;
		cache_slot[1] = op_array;
	}

	return op_array;
}

zend_op_array *zp_get_op_array(zval *this_ptr, zend_function *function,
		uint32_t argc, zval *argv, zend_array *extra_named_params,
		const zend_op_array *declaring_op_array,
		const zend_op *declaring_opline, void **cache_slot) {

	if (EXPECTED(cache_slot[0] == function)) {
		return cache_slot[1];
	}

	zend_op_array *op_array = zend_accel_pfa_cache_get(declaring_op_array,
			declaring_opline, function);

	if (EXPECTED(op_array)) {
		cache_slot[0] = function;
		cache_slot[1] = op_array;
		return op_array;
	}

	return zp_compile(this_ptr, function, argc, argv,
			extra_named_params, declaring_op_array, declaring_opline,
			cache_slot);
}

/* Bind pre-bound arguments as lexical vars */
void zp_bind(zval *result, zend_function *function, uint32_t argc, zval *argv,
		zend_array *extra_named_params) {

	zend_arg_info *arg_infos = function->common.arg_info;
	uint32_t bind_offset = 0;
	for (uint32_t offset = 0; offset < argc; offset++) {
		zval *var = &argv[offset];
		if (Z_IS_PLACEHOLDER_P(var) || Z_ISUNDEF_P(var)) {
			continue;
		}
		zend_arg_info *arg_info;
		if (offset < function->common.num_args) {
			arg_info = &arg_infos[offset];
		} else if (function->common.fn_flags & ZEND_ACC_VARIADIC) {
			arg_info = &arg_infos[function->common.num_args];
		} else {
			arg_info = NULL;
		}
		if (arg_info && ZEND_TYPE_IS_SET(arg_info->type)
				&& UNEXPECTED(!zend_check_type_ex(&arg_info->type, var, function->common.scope, 0, 0))) {
			zend_verify_arg_error(function, arg_info, offset+1, var);
			zval_ptr_dtor(result);
			ZVAL_NULL(result);
			return;
		}
		ZEND_ASSERT(zp_arg_must_be_sent_by_ref(function, offset+1) ? Z_ISREF_P(var) : !Z_ISREF_P(var));
		zend_closure_bind_var_ex(result, bind_offset, var);
		bind_offset += sizeof(Bucket);
	}

	if (extra_named_params) {
		zval var;
		ZVAL_ARR(&var, extra_named_params);
		Z_ADDREF(var);
		zend_closure_bind_var_ex(result, bind_offset, &var);
		bind_offset += sizeof(Bucket);
	}

	if (function->common.fn_flags & ZEND_ACC_CLOSURE) {
		zval var;
		ZVAL_OBJ(&var, ZEND_CLOSURE_OBJECT(function));
		Z_ADDREF(var);
		zend_closure_bind_var_ex(result, bind_offset, &var);
	}
}

void zend_partial_create(zval *result, zval *this_ptr, zend_function *function,
		uint32_t argc, zval *argv, zend_array *extra_named_params,
		const zend_op_array *declaring_op_array,
		const zend_op *declaring_opline, void **cache_slot) {

	zend_op_array *op_array = zp_get_op_array(this_ptr, function, argc, argv,
			extra_named_params, declaring_op_array, declaring_opline,
			cache_slot);

	if (UNEXPECTED(!op_array)) {
		ZEND_ASSERT(EG(exception));
		return;
	}

	zend_class_entry *called_scope;
	zval object;

	if (Z_TYPE_P(this_ptr) == IS_OBJECT) {
		called_scope = Z_OBJCE_P(this_ptr);
	} else {
		called_scope = Z_CE_P(this_ptr);
	}

	if (Z_TYPE_P(this_ptr) == IS_OBJECT && !IS_STATIC_CLOSURE(function)) {
		ZVAL_COPY_VALUE(&object, this_ptr);
	} else {
		ZVAL_UNDEF(&object);
	}

	zend_create_partial_closure(result, (zend_function*)op_array,
			function->common.scope, called_scope, &object);

	destroy_op_array(op_array);

	zp_bind(result, function, argc, argv, extra_named_params);
}
