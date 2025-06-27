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
   | Authors: krakjoe <krakjoe@php.net>                                   |
   +----------------------------------------------------------------------+
*/
#include "zend.h"
#include "zend_API.h"
#include "zend_compile.h"
#include "zend_hash.h"
#include "zend_interfaces.h"
#include "zend_closures.h"
#include "zend_partial.h"
#include "zend_portability.h"
#include "zend_types.h"
#include "zend_vm.h"
#include "zend_observer.h"

typedef struct _zend_partial {
	/* Common zend_closure fields */
	zend_object      std;
	/* this will be returned by get_closure, and will invoke func. Reflects the
	 * partial signature. */
	zend_function    trampoline;
	zval             This;
	uint32_t         closure_flags;
	/* End of common fields */
	/* this is the unmodified function that will be invoked at call time */
	zend_function    func;
	uint32_t         argc;
	zval            *argv;
	zend_array      *named;
} zend_partial;

static zend_object_handlers zend_partial_handlers;

static zend_arg_info zend_call_magic_arginfo[1];

// TODO: Does this need to be in EG()?
static union {
	zend_op opcodes[2];
	char * buf[sizeof(zend_op)*2 + sizeof(zval)];
} partial_call_trampoline_op;

#define Z_IS_PLACEHOLDER_ARG_P(p) \
	(Z_TYPE_P(p) == _IS_PLACEHOLDER_ARG)

#define Z_IS_PLACEHOLDER_VARIADIC_P(p) \
	(Z_TYPE_P(p) == _IS_PLACEHOLDER_VARIADIC)

#define Z_IS_PLACEHOLDER_P(p) \
	(Z_IS_PLACEHOLDER_ARG_P(p) || Z_IS_PLACEHOLDER_VARIADIC_P(p))

#define Z_IS_NOT_PLACEHOLDER_P(p) \
	(!Z_IS_PLACEHOLDER_ARG_P(p) && !Z_IS_PLACEHOLDER_VARIADIC_P(p))

#define ZEND_PARTIAL_IS_CALL_TRAMPOLINE(func) \
	UNEXPECTED(((func)->type == ZEND_USER_FUNCTION) && ((func)->op_array.opcodes == &EG(call_trampoline_op)))

#define ZEND_PARTIAL_FUNC_SIZE(func) \
	(((func)->type == ZEND_INTERNAL_FUNCTION) ? sizeof(zend_internal_function) : sizeof(zend_op_array))

#define ZEND_PARTIAL_FUNC_FLAG(func, flags) \
	(((func)->common.fn_flags & flags) != 0)

#define ZEND_PARTIAL_FUNC_DEL(func, flag) \
	((func)->common.fn_flags &= ~flag)

#define ZEND_PARTIAL_FUNC_ADD(func, flag) \
	((func)->common.fn_flags |= flag)

#define ZEND_PARTIAL_CALL_FLAG(partial, flag) \
	(ZEND_CALL_INFO(partial) & flag)

static zend_always_inline uint32_t zend_partial_signature_size(zend_partial *partial) {
	uint32_t count = partial->argc;

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_HAS_RETURN_TYPE)) {
		count++;
	}

	if (ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC)
			|| ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_VARIADIC)) {
		count++;
	}

	return count * sizeof(zend_arg_info);
}

static zend_always_inline void zend_partial_signature_create(zend_partial *partial) {
	zend_arg_info *signature = emalloc(zend_partial_signature_size(partial)), *info = signature;

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->trampoline, ZEND_ACC_HAS_RETURN_TYPE)) {
		memcpy(info,
			partial->trampoline.common.arg_info - 1, sizeof(zend_arg_info));
		info++;
	}

	uint32_t offset = 0, num = 0, required = 0, limit = partial->argc;
	bool byref = false;

	memset(partial->trampoline.op_array.arg_flags, 0,
			sizeof(partial->trampoline.op_array.arg_flags));

	while (offset < limit) {
		zval *arg = &partial->argv[offset];

		if (Z_IS_PLACEHOLDER_P(arg)) {
			if (offset < partial->func.common.num_args) {
				num++;
				if (offset < partial->func.common.required_num_args) {
					required = num;
				}
				memcpy(info,
					&partial->func.common.arg_info[offset],
					sizeof(zend_arg_info));
				if (EXPECTED(num < MAX_ARG_FLAG_NUM)) {
					uint32_t mode = ZEND_ARG_SEND_MODE(info);
					if (mode) {
						ZEND_SET_ARG_FLAG(&partial->trampoline, num, mode);
						byref = true;
					}
				}
				info++;
			} else {
				ZEND_ASSERT(ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_VARIADIC));
				num++;
				/* Placeholders that run into the variadic portion become
				 * required and make all params before them required */
				required = num;
				info->name = zend_empty_string;
				info->default_value = NULL;
				if (ZEND_PARTIAL_IS_CALL_TRAMPOLINE(&partial->func)) {
					info->type = (zend_type){0};
				} else {
					info->type = (partial->func.common.arg_info + partial->func.common.num_args)->type;
					ZEND_TYPE_FULL_MASK(info->type) &= ~_ZEND_IS_VARIADIC_BIT;
				}
				if (EXPECTED(num < MAX_ARG_FLAG_NUM)) {
					uint32_t mode = ZEND_ARG_SEND_MODE(info);
					if (mode) {
						ZEND_SET_ARG_FLAG(&partial->trampoline, num, mode);
						byref = true;
					}
				}
				info++;
			}
		} else if (Z_ISUNDEF_P(arg)) {
			ZEND_ASSERT(!ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC));
			ZEND_ADD_CALL_FLAG(partial, ZEND_APPLY_UNDEF);
		} else {
			if (num > required) {
				ZEND_ADD_CALL_FLAG(partial, ZEND_APPLY_UNDEF);
			}
		}
		offset++;
	}

	/* If we have a variadic placeholder and the underlying function is
	 * variadic, add a variadic param. Otherwise, allow extra args to be passed
	 * (implied by the ZEND_APPLY_VARIADIC flag) but don't add an explicit
	 * variadic param. */
	if (ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC)
			&& ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_VARIADIC)) {
		if (ZEND_PARTIAL_IS_CALL_TRAMPOLINE(&partial->func)) {
			memcpy(info, zend_call_magic_arginfo, sizeof(zend_arg_info));
		} else {
			memcpy(info,
					partial->func.common.arg_info + partial->func.common.num_args,
					sizeof(zend_arg_info));
		}
		ZEND_PARTIAL_FUNC_ADD(&partial->trampoline, ZEND_ACC_VARIADIC);
		uint32_t mode = ZEND_ARG_SEND_MODE(info);
		if (mode) {
			for (uint32_t i = num + 1; i < MAX_ARG_FLAG_NUM; i++) {
				ZEND_SET_ARG_FLAG(&partial->trampoline, i, mode);
			}
			byref = true;
		}
		info++;
	}

	if (byref) {
		ZEND_ADD_CALL_FLAG(partial, ZEND_APPLY_BYREF);
	}

	partial->trampoline.common.num_args = num;
	partial->trampoline.common.required_num_args = required;
	partial->trampoline.common.arg_info = signature;

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->trampoline, ZEND_ACC_HAS_RETURN_TYPE)) {
		partial->trampoline.common.arg_info++;
	}

	if (partial->func.type == ZEND_INTERNAL_FUNCTION
			&& !ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_USER_ARG_INFO)) {
		/* Internal functions use an incompatible arg_info struct
		 * TODO: unify zend_arg_info / zend_internal_arg_info? */
		zend_arg_info *end = info;
		info = signature;
		if (ZEND_PARTIAL_FUNC_FLAG(&partial->trampoline, ZEND_ACC_HAS_RETURN_TYPE)) {
			info++;
		}
		for (; info < end; info++) {
			zend_internal_arg_info *ii = (zend_internal_arg_info*) info;
			if ((zend_string*)ii->name == zend_empty_string) {
				continue;
			}
			info->name = zend_string_init(ii->name, strlen(ii->name), false);
			if (ii->default_value) {
				info->default_value = zend_string_init(ii->default_value, strlen(ii->default_value), false);
			}
		}
	}

	partial->trampoline.common.prototype = &partial->func;
}

bool zend_is_partial_function(zend_function *function) {
	if (!(function->common.fn_flags & ZEND_ACC_CLOSURE)) {
		return false;
	}

	zend_partial *ptr = (zend_partial*)((char*)function - XtOffsetOf(zend_partial, trampoline));

	if (!(ptr->closure_flags & ZEND_CLOSURE_PARTIAL)) {
		return false;
	}

	return true;
}

static zend_always_inline zend_partial* zend_partial_fetch(zval *This) {
	if (!This || Z_TYPE_P(This) != IS_OBJECT) {
		return NULL;
	}

	if (UNEXPECTED(!instanceof_function(Z_OBJCE_P(This), zend_ce_closure))) {
		return NULL;
	}

	zend_partial *ptr = (zend_partial*) Z_OBJ_P(This);

	if (!(ptr->closure_flags & ZEND_CLOSURE_PARTIAL)) {
		return NULL;
	}

	return ptr;
}

static zend_always_inline void zend_partial_trampoline_create(zend_partial *partial, zend_function *trampoline)
{
	/* We use non-NULL value to avoid useless run_time_cache allocation.
	 * The low bit must be zero, to not be interpreted as a MAP_PTR offset.
	 */
	static const void *dummy = (void*)(intptr_t)2;

	const uint32_t keep_flags =
		ZEND_ACC_RETURN_REFERENCE | ZEND_ACC_HAS_RETURN_TYPE | ZEND_ACC_STRICT_TYPES;

	trampoline->common = partial->func.common;
	trampoline->type = ZEND_USER_FUNCTION;

	/* ZEND_ACC_CALL_VIA_TRAMPOLINE | ZEND_ACC_TRAMPOLINE_PERMANENT: Don't copy extra args
	 * ZEND_ACC_HAS_TYPE_HINTS: Don't try to skip RECV ops */
	trampoline->op_array.fn_flags =
		ZEND_ACC_PUBLIC | ZEND_ACC_HAS_TYPE_HINTS | ZEND_ACC_CALL_VIA_TRAMPOLINE | ZEND_ACC_TRAMPOLINE_PERMANENT | ZEND_ACC_CLOSURE | (partial->func.common.fn_flags & keep_flags);

	zend_partial_signature_create(partial);

	trampoline->op_array.opcodes = partial_call_trampoline_op.opcodes;
	trampoline->op_array.filename = zend_get_executed_filename_ex();
	if (trampoline->op_array.filename == NULL) {
		trampoline->op_array.filename = ZSTR_EMPTY_ALLOC();
	} else {
		zend_string_addref(trampoline->op_array.filename);
	}
	trampoline->op_array.line_start = zend_get_executed_lineno();
	trampoline->op_array.line_end = trampoline->op_array.line_start;

	trampoline->op_array.last = sizeof(partial_call_trampoline_op.opcodes) / sizeof(zend_op);

	/* Ensure that trampoline and func have the same stack size */
	trampoline->op_array.T = partial->func.common.T;
	if (partial->func.type == ZEND_USER_FUNCTION) {
		trampoline->op_array.last_var = partial->argc + partial->func.op_array.last_var - MIN(partial->func.op_array.num_args, partial->argc);
	} else {
		trampoline->op_array.last_var = MAX(partial->func.op_array.num_args, partial->argc);
	}
	ZEND_ASSERT(zend_vm_calc_used_stack(trampoline->common.num_args, trampoline) == zend_vm_calc_used_stack(partial->argc, &partial->func));

	ZEND_MAP_PTR_INIT(trampoline->op_array.run_time_cache, (void**)dummy);
}

static zend_always_inline zend_object* zend_partial_new(zend_class_entry *type, uint32_t info) {
	zend_partial *partial = ecalloc(1, sizeof(zend_partial));

	zend_object_std_init(&partial->std, type);

	partial->std.handlers = &zend_partial_handlers;
	partial->closure_flags = ZEND_CLOSURE_PARTIAL;

	ZEND_ADD_CALL_FLAG(partial, info);

	return (zend_object*) partial;
}

static zend_always_inline void zend_partial_debug_add(zend_function *function, HashTable *ht, zend_arg_info *info, zval *value) {
	if (function->type == ZEND_USER_FUNCTION || ZEND_PARTIAL_FUNC_FLAG(function, ZEND_ACC_USER_ARG_INFO)) {
		zend_hash_add_new(ht, info->name, value);
	} else {
		zend_internal_arg_info *internal = (zend_internal_arg_info*) info;

		zend_hash_str_add_new(ht, internal->name, strlen(internal->name), value);
	}
}

static zend_always_inline void zend_partial_debug_fill(zend_partial *partial, HashTable *ht) {
	zval *arg = partial->argv;
	zval *aend = arg + partial->argc;

	zend_arg_info *info = partial->func.common.arg_info;
	zend_arg_info *iend = info + partial->func.common.num_args;

	while (info < iend) {
		zval param;

		ZVAL_NULL(&param);

		if (arg < aend) {
			if (Z_IS_NOT_PLACEHOLDER_P(arg)) {
				ZVAL_COPY(&param, arg);
			}
			arg++;
		}

		zend_partial_debug_add(&partial->func, ht, info, &param);

		info++;
	}

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_VARIADIC)) {
		zval variadics;

		array_init(&variadics);

		zend_partial_debug_add(&partial->func, ht, info, &variadics);

		while (arg < aend) {
			zval param;

			ZVAL_NULL(&param);

			if (Z_IS_NOT_PLACEHOLDER_P(arg)) {
				ZVAL_COPY(&param, arg);
			}

			zend_hash_next_index_insert(Z_ARRVAL(variadics), &param);
			arg++;
		}

		if (partial->named) {
			zend_hash_merge(Z_ARRVAL(variadics), partial->named, zval_copy_ctor, true);
		}
	}
}

static HashTable *zend_partial_debug(zend_object *object, int *is_temp) {
	zend_partial *partial = (zend_partial*) object;
	zval args;
	HashTable *ht;

	ht = zend_closure_get_debug_info(object, is_temp);

	array_init(&args);
	zend_hash_update(ht, ZSTR_KNOWN(ZEND_STR_ARGS), &args);

	zend_partial_debug_fill(partial, Z_ARRVAL(args));

	return ht;
}

static HashTable *zend_partial_get_gc(zend_object *obj, zval **table, int *n)
{
	zend_partial *partial = (zend_partial *)obj;

	if (!partial->argc && !partial->named) {
		*table = Z_TYPE(partial->This) == IS_OBJECT ? &partial->This : NULL;
		*n = Z_TYPE(partial->This) == IS_OBJECT ? 1 : 0;
	} else {
		zend_get_gc_buffer *buffer = zend_get_gc_buffer_create();

		if (Z_TYPE(partial->This) == IS_OBJECT) {
			zend_get_gc_buffer_add_zval(buffer, &partial->This);
		}

		for (uint32_t arg = 0; arg < partial->argc; arg++) {
			zend_get_gc_buffer_add_zval(buffer, &partial->argv[arg]);
		}

		if (partial->named) {
			zval named;

			ZVAL_ARR(&named, partial->named);

			zend_get_gc_buffer_add_zval(buffer, &named);
		}

		zend_get_gc_buffer_use(buffer, table, n);
	}

	return NULL;
}

static zend_function *zend_partial_get_method(zend_object **object, zend_string *method, const zval *key) /* {{{ */
{
	if (zend_string_equals_literal_ci(method, ZEND_INVOKE_FUNC_NAME)) {
		return zend_get_closure_invoke_method(*object);
	}

	return zend_std_get_method(object, method, key);
}
/* }}} */

static int zend_partial_get_closure(zend_object *obj, zend_class_entry **ce_ptr, zend_function **fptr_ptr, zend_object **obj_ptr, bool check_only)
{
	zend_partial *partial = (zend_partial*) obj;

	*fptr_ptr = &partial->trampoline;
	*obj_ptr  = (zend_object*) &partial->std;
	*ce_ptr = Z_TYPE(partial->This) == IS_OBJECT ? Z_OBJCE(partial->This) : NULL;

	return SUCCESS;
}

zend_function *zend_partial_get_trampoline(zend_object *obj) {
	zend_partial *partial = (zend_partial*) obj;

	return &partial->trampoline;
}

static void zend_partial_free(zend_object *object) {
	zend_partial *partial = (zend_partial*) object;

	zval *arg = partial->argv,
		 *end = arg + partial->argc;

	while (arg < end) {
		if (Z_OPT_REFCOUNTED_P(arg)) {
			zval_ptr_dtor(arg);
		}
		arg++;
	}

	efree(partial->argv);

	if (partial->named) {
		zend_array_release(partial->named);
	}

	zend_arg_info *info = partial->trampoline.common.arg_info;

	if (partial->func.type == ZEND_INTERNAL_FUNCTION
			&& !ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_USER_ARG_INFO)) {
		zend_arg_info *cur = info;
		zend_arg_info *end = info + partial->trampoline.common.num_args;
		if (ZEND_PARTIAL_FUNC_FLAG(&partial->trampoline, ZEND_ACC_VARIADIC)) {
			end++;
		}
		while (cur < end) {
			zend_string_release(cur->name);
			if (cur->default_value) {
				zend_string_release(cur->default_value);
			}
			cur++;
		}
	}

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_HAS_RETURN_TYPE)) {
		info--;
	}

	efree(info);

	if (partial->trampoline.op_array.filename) {
		zend_string_release(partial->trampoline.op_array.filename);
	}
	if (partial->func.type == ZEND_USER_FUNCTION) {
		destroy_op_array(&partial->func.op_array);
	}

	if (Z_TYPE(partial->This) == IS_OBJECT) {
		zval_ptr_dtor(&partial->This);
	}

	zend_object_std_dtor(object);
}

static void zend_partial_init_call_trampoline_op(void) {
	zend_op_array tmp_op_array = {
		.opcodes = partial_call_trampoline_op.opcodes,
		.literals = (zval*)(partial_call_trampoline_op.opcodes + 2),
	};

	tmp_op_array.opcodes[0].opcode = ZEND_CALL_PARTIAL;
	ZEND_VM_SET_OPCODE_HANDLER(&tmp_op_array.opcodes[0]);

	// TODO: still needed?
	tmp_op_array.opcodes[1].opcode = ZEND_RETURN;
	tmp_op_array.opcodes[1].op1_type = IS_CONST;
	tmp_op_array.opcodes[1].op1.constant = tmp_op_array.last_literal++;
	ZVAL_NULL(&tmp_op_array.literals[tmp_op_array.opcodes[1].op1.constant]);
	ZEND_PASS_TWO_UPDATE_CONSTANT(&tmp_op_array, &tmp_op_array.opcodes[1], tmp_op_array.opcodes[1].op1);
	ZEND_VM_SET_OPCODE_HANDLER(&tmp_op_array.opcodes[1]);
}

void zend_partial_startup(void) {
	memcpy(&zend_partial_handlers,
			&std_object_handlers, sizeof(zend_object_handlers));

	zend_partial_handlers.free_obj = zend_partial_free;
	zend_partial_handlers.get_debug_info = zend_partial_debug;
	zend_partial_handlers.get_gc = zend_partial_get_gc;
	zend_partial_handlers.get_closure = zend_partial_get_closure;
	zend_partial_handlers.get_method = zend_partial_get_method;

	memset(&zend_call_magic_arginfo, 0, sizeof(zend_arg_info) * 1);

	zend_call_magic_arginfo[0].name = ZSTR_KNOWN(ZEND_STR_ARGS);
	zend_call_magic_arginfo[0].type = (zend_type) ZEND_TYPE_INIT_NONE(_ZEND_IS_VARIADIC_BIT);

	zend_partial_init_call_trampoline_op();
}

static zend_always_inline zend_string* zend_partial_function_name(zend_function *function) {
	if (function->type == ZEND_INTERNAL_FUNCTION) {
		if (function->internal_function.handler == zend_pass_function.handler) {
			return zend_string_init("__construct", sizeof("__construct")-1, 0);
		}
	}
	return zend_string_copy(function->common.function_name);
}

static zend_always_inline zend_string* zend_partial_scope_name(zend_execute_data *execute_data, zend_function *function) {
	if (function->type == ZEND_INTERNAL_FUNCTION) {
		if (function->internal_function.handler == zend_pass_function.handler) {
			if (Z_OBJ(EX(This))) {
				return Z_OBJCE(EX(This))->name;
			}
		}
	}

	if (function->common.scope) {
		return function->common.scope->name;
	}

	return NULL;
}

zend_string* zend_partial_symbol_name_ex(zend_partial *partial) {
	zend_string *name = zend_partial_function_name(&partial->func),
				*scope = partial->func.common.scope ? partial->func.common.scope->name : NULL,
				*symbol;

	if (scope) {
		symbol = zend_create_member_string(scope, name);
	} else {
		symbol = zend_string_copy(name);
	}

	zend_string_release(name);
	return symbol;
}

zend_string* zend_partial_symbol_name(zend_execute_data *call, zend_function *function) {
	zend_string *name = zend_partial_function_name(function),
				*scope = zend_partial_scope_name(call, function),
				*symbol;

	if (scope) {
		symbol = zend_create_member_string(scope, name);
	} else {
		symbol = zend_string_copy(name);
	}

	zend_string_release(name);
	return symbol;
}

ZEND_COLD void zend_partial_prototype_underflow(zend_function *function, zend_string *symbol, uint32_t args, uint32_t expected, bool variadic) {
	char *limit = variadic ? "at least" : "exactly";

	if (function->type == ZEND_USER_FUNCTION) {
		zend_throw_error(NULL,
			"not enough arguments for application of %s, "
			"%d given and %s %d expected, declared in %s on line %d",
			ZSTR_VAL(symbol), args, limit, expected,
			ZSTR_VAL(function->op_array.filename), function->op_array.line_start);
	} else {
		zend_throw_error(NULL,
			"not enough arguments for application of %s, %d given and %s %d expected",
			ZSTR_VAL(symbol), args, limit, expected);
	}
}

ZEND_COLD void zend_partial_prototype_overflow(zend_function *function, zend_string *symbol, uint32_t args, uint32_t expected) {
	if (function->type == ZEND_USER_FUNCTION) {
		zend_throw_error(NULL,
			"too many arguments for application of %s, "
			"%d given and a maximum of %d expected, declared in %s on line %d",
			ZSTR_VAL(symbol), args, expected,
			ZSTR_VAL(function->op_array.filename), function->op_array.line_start);
	} else {
		zend_throw_error(NULL,
			"too many arguments for application of %s, %d given and a maximum of %d expected",
			ZSTR_VAL(symbol), args, expected);
	}
}

static zend_always_inline void zend_partial_args_underflow(zend_function *function, zend_string *symbol, uint32_t args, uint32_t expected, bool calling, bool prototype) {
	const char *what = calling ?
			"arguments" : "arguments or placeholders";
	const char *from = prototype ?
		   "application" : "implementation";
	const char *limit = function->common.num_args <= function->common.required_num_args ?
			"exactly" : "at least";

	if (function->type == ZEND_USER_FUNCTION) {
		zend_throw_error(NULL,
			"not enough %s for %s of %s, "
			"%d given and %s %d expected, declared in %s on line %d",
			what, from, ZSTR_VAL(symbol), args, limit, expected,
			ZSTR_VAL(function->op_array.filename), function->op_array.line_start);
	} else {
		zend_throw_error(NULL,
			"not enough %s for %s of %s, %d given and %s %d expected",
			what, from, ZSTR_VAL(symbol), args, limit, expected);
	}
}

static zend_always_inline void zend_partial_args_overflow(zend_function *function, zend_string *symbol, uint32_t args, uint32_t expected, bool calling, bool prototype) {
	const char *what = calling ?
			"arguments" : "arguments or placeholders";
	const char *from = prototype ?
		   "application" : "implementation";

	if (function->type == ZEND_USER_FUNCTION) {
		zend_throw_error(NULL,
			"too many %s for %s of %s, "
			"%d given and a maximum of %d expected, declared in %s on line %d",
			what, from, ZSTR_VAL(symbol), args, expected,
			ZSTR_VAL(function->op_array.filename), function->op_array.line_start);
	} else {
		zend_throw_error(NULL,
			"too many %s for %s of %s, %d given and a maximum of %d expected",
			what, from, ZSTR_VAL(symbol), args, expected);
	}
}

void zend_partial_args_check(zend_execute_data *call) {
	/* this is invoked by VM before the creation of zend_partial */

	if (ZEND_PARTIAL_CALL_FLAG(call, ZEND_CALL_HAS_EXTRA_NAMED_PARAMS)) {
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
		/* this check is delayed in the case of variadic application */
		if (variadic) {
			return;
		}

		zend_string *symbol = zend_partial_symbol_name(call, function);
		zend_partial_args_underflow(
			function, symbol,
			num, function->common.required_num_args, false, true);
		zend_string_release(symbol);
	} else if (num > function->common.num_args &&
			!ZEND_PARTIAL_FUNC_FLAG(function, ZEND_ACC_VARIADIC)) {
		zend_string *symbol = zend_partial_symbol_name(call, function);
		zend_partial_args_overflow(
			function, symbol,
			num, function->common.num_args, false, true);
		zend_string_release(symbol);
	}
}

static zend_always_inline void zend_partial_send_var(
		zend_partial *partial, zend_execute_data *call,
		uint32_t arg_num, zval *var, zval *dest)
{
	if (UNEXPECTED(ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_BYREF))
			&& ((EXPECTED(arg_num <= MAX_ARG_FLAG_NUM)
				&& UNEXPECTED(QUICK_ARG_SHOULD_BE_SENT_BY_REF(call->func, arg_num)))
			|| UNEXPECTED(ARG_SHOULD_BE_SENT_BY_REF(call->func, arg_num)))) {
		if (UNEXPECTED(!Z_OPT_ISREF_P(var))) {
			zend_param_must_be_ref(call->func, arg_num);
			if (UNEXPECTED(EG(exception))) {
				/* TODO: clean stack and let the exception unwind.
				 * For now, trigger a fatal error. */
				zend_error_noreturn(E_CORE_ERROR, "TODO");
			}
		}

		ZVAL_COPY_VALUE(dest, var);
	} else if (UNEXPECTED(Z_OPT_ISREF_P(var))) {
		ZVAL_COPY(dest, Z_REFVAL_P(var));
		zval_ptr_dtor(var);
	} else {
		ZVAL_COPY_VALUE(dest, var);
	}
}

/* Rearranges args on the stack, inplace */
static uint32_t zend_partial_apply_inplace(zend_partial *partial,
		zend_execute_data *call, zval *argv, uint32_t argc) {

	uint32_t num_args = 0;
	uint32_t type_flags = 0;

	uint32_t first_extra_arg_offset = partial->func.common.num_args;
	zval *first_extra_arg = argv + first_extra_arg_offset;
	size_t extra_arg_delta;
	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)
			|| partial->func.type == ZEND_INTERNAL_FUNCTION) {
		/* Trampolines and internal functions expect contiguous args */
		extra_arg_delta = 0;
	} else {
		extra_arg_delta = ((partial->func.op_array.last_var + partial->func.op_array.T) - first_extra_arg_offset);
	}

	/* Move extra trampoline args. These args are always extra args in
	 * partial->func as well, since the trampoline allows extra args only
	 * when partial->argc is at least as high as the func num args. */
	if (UNEXPECTED(argc > partial->trampoline.common.num_args)) {
		ZEND_ASSERT(partial->argc >= partial->func.common.num_args);

		uint32_t num_extra_args = argc - partial->trampoline.common.num_args;
		zval *dest = argv + partial->argc + num_extra_args - 1 + extra_arg_delta;
		zval *src = argv + argc - 1;

		num_args = partial->argc + num_extra_args;

		do {
			ZEND_ASSERT(src >= argv + partial->trampoline.common.num_args);
			if (!ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
				ZEND_ASSERT(dest >= argv + partial->func.op_array.last_var + partial->func.op_array.T);
			}

			zend_partial_send_var(partial, call, src - argv + 1, src, dest);
			type_flags |= Z_TYPE_INFO_P(dest);

			if (dest != src) {
				ZVAL_UNDEF(src);
			}
			dest--;
			src--;
		} while (--num_extra_args);
	}

	zval *dest = argv + partial->argc - 1;

	zval *partial_first = partial->argv;
	zval *partial_cur = partial->argv + partial->argc - 1;

	zval *call_cur = argv + partial->trampoline.common.num_args - 1;

	while (partial_cur >= partial_first) {
		zval *actual_dest = dest >= first_extra_arg
			? dest + extra_arg_delta
			: dest;
		if (Z_IS_PLACEHOLDER_P(partial_cur)) {
			if (call_cur - argv >= argc) {
				/* Optional arg not specified */
				// TODO: needs initialization?
			} else {
				/* Move call arg to the right */
				if (actual_dest != call_cur) {
					ZEND_ASSERT(actual_dest > call_cur);
					zend_partial_send_var(partial, call, call_cur - argv + 1, call_cur, actual_dest);
					if (dest >= first_extra_arg) {
						if (call_cur != actual_dest) {
							ZVAL_UNDEF(call_cur);
						}
					}
				}
				if (dest >= first_extra_arg) {
					type_flags |= Z_TYPE_INFO_P(actual_dest);
				}
				if (num_args == 0) {
					num_args = partial_cur - partial_first + 1;
				}
			}
			call_cur--;
		} else if (Z_ISUNDEF_P(partial_cur)) {
			ZVAL_UNDEF(actual_dest);
		} else {
			/* Copy partial arg */
			ZVAL_COPY(actual_dest, partial_cur);
			if (dest >= first_extra_arg) {
				type_flags |= Z_TYPE_INFO_P(partial_cur);
			}
			if (num_args == 0) {
				num_args = partial_cur - partial_first + 1;
			}
		}
		dest--;
		partial_cur--;
	}

	if (Z_TYPE_INFO_REFCOUNTED(type_flags)
			&& !ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
		ZEND_ADD_CALL_FLAG(call, ZEND_CALL_FREE_EXTRA_ARGS);
	}

	ZEND_ASSERT(partial_cur == partial_first - 1);
	ZEND_ASSERT(dest == argv - 1);
	ZEND_ASSERT(call_cur == argv - 1);

	return num_args;
}

static zend_always_inline void zend_partial_apply(
	zval *pStart, zval *pEnd,
	zval *cStart, zval *cEnd,
	zval *fParam) {

	while (pStart < pEnd) {
		if (Z_IS_PLACEHOLDER_P(pStart)) {
			if (cStart < cEnd) {
				ZVAL_COPY_VALUE(fParam, cStart);
				cStart++;
			} else {
				Z_TYPE_INFO_P(fParam) = _IS_PLACEHOLDER_ARG;
			}
		} else if (Z_ISUNDEF_P(pStart)) {
			ZVAL_UNDEF(fParam);
		} else {
			ZVAL_COPY(fParam, pStart);
		}
		fParam++;
		pStart++;
	}

	while (cStart < cEnd) {
		ZVAL_COPY_VALUE(fParam, cStart);
		fParam++;
		cStart++;
	}
}

/* Initializes a call to the real function. The call frame of the trampoline is
 * reused. */
zend_result zend_partial_init_call(zend_execute_data *call)
{
	ZEND_ASSERT(call->opline->opcode == ZEND_CALL_PARTIAL);

	/* call->func points to partial->trampoline */
	zend_partial *partial = (zend_partial*)((char*)call->func - XtOffsetOf(zend_partial, trampoline));

	if (UNEXPECTED(ZEND_CALL_NUM_ARGS(call) < partial->trampoline.common.required_num_args)) {
		zend_string *symbol = zend_partial_symbol_name_ex(partial);
		zend_partial_prototype_underflow(
			&partial->trampoline, symbol, ZEND_CALL_NUM_ARGS(call),
			partial->trampoline.common.required_num_args,
			ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC));
		zend_string_release(symbol);
		if (ZEND_PARTIAL_IS_CALL_TRAMPOLINE(&partial->trampoline)) {
			EG(trampoline).common.function_name = NULL;
		}
		return FAILURE;
	} else if (UNEXPECTED(ZEND_CALL_NUM_ARGS(call) > partial->trampoline.common.num_args &&
			  !ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC))) {
		zend_string *symbol = zend_partial_symbol_name_ex(partial);
		zend_partial_prototype_overflow(
			&partial->trampoline, symbol, ZEND_CALL_NUM_ARGS(call),
			partial->trampoline.common.num_args);
		zend_string_release(symbol);
		if (ZEND_PARTIAL_IS_CALL_TRAMPOLINE(&partial->trampoline)) {
			EG(trampoline).common.function_name = NULL;
		}
		return FAILURE;
	}

	ZEND_ASSERT(zend_vm_calc_used_stack(ZEND_CALL_NUM_ARGS(call), &partial->func) <= (size_t)(((char*)EG(vm_stack_end)) - (char*)call));

	uint32_t num_args = zend_partial_apply_inplace(partial, call,
			ZEND_CALL_ARG(call, 1), ZEND_CALL_NUM_ARGS(call));

	uint32_t orig_call_info = ZEND_CALL_INFO(call);
	uint32_t call_info = orig_call_info & (ZEND_CALL_NESTED | ZEND_CALL_TOP | ZEND_CALL_ALLOCATED | ZEND_CALL_FREE_EXTRA_ARGS | ZEND_CALL_HAS_EXTRA_NAMED_PARAMS);
	void *object_or_called_scope;
	if (!ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
		call_info |= ZEND_CALL_FAKE_CLOSURE;
	}
	if (Z_TYPE(partial->This) == IS_OBJECT) {
		object_or_called_scope = Z_OBJ(partial->This);
		ZEND_ADD_CALL_FLAG_EX(call_info, ZEND_CALL_HAS_THIS);
	} else if (Z_TYPE(partial->This) == IS_UNDEF && Z_CE(partial->This)) {
		object_or_called_scope = Z_CE(partial->This);
	} else {
		object_or_called_scope = NULL;
	}

	zend_vm_init_call_frame(call, call_info, &partial->func,
			num_args, object_or_called_scope);

	zend_array *named_args;
	if (ZEND_CALL_INFO(call) & ZEND_CALL_HAS_EXTRA_NAMED_PARAMS) {
		named_args = call->extra_named_params;
	} else {
		named_args = NULL;
	}

	if (partial->named) {
		if (!named_args) {
			call->extra_named_params = partial->named;
			GC_ADDREF(partial->named);
			ZEND_CALL_INFO(call) |= ZEND_CALL_HAS_EXTRA_NAMED_PARAMS;
		} else {
			/* Preserve order: partial args first, then call args */
			zend_array *nested = zend_array_dup(partial->named);
			zend_hash_merge(nested, named_args, zval_copy_ctor, true);
			zend_array_release(named_args);
			call->extra_named_params = nested;
		}
	}

	if (ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_UNDEF)) {
		/* zend_handle_undef_args() creates a fake frame that links to itself
		 * due to EG(current_execute_data) == call */
		EG(current_execute_data) = call->prev_execute_data;
		zend_handle_undef_args(call);
		EG(current_execute_data) = call;
	}

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
		zend_string_addref(partial->func.common.function_name);
		OBJ_RELEASE(&partial->std);
	}

	return SUCCESS;
}

void zend_partial_create(zval *result, uint32_t info, zval *this_ptr, zend_function *function, uint32_t argc, zval *argv, zend_array *extra_named_params) {
	ZVAL_OBJ(result, zend_partial_new(zend_ce_closure, info));

	// TODO: static vars / lexical vars
	// TODO: run_time_cache?

	zend_partial *applied, *partial = (zend_partial*) Z_OBJ_P(result);

	ZEND_ASSERT(ZEND_PARTIAL_OBJECT(&partial->func) == &partial->std);

	if ((applied = zend_partial_fetch(this_ptr))) {
		ZEND_ADD_CALL_FLAG(partial, ZEND_CALL_INFO(applied) & ~ZEND_APPLY_VARIADIC);

		function  = &applied->func;

		/* Z_EXTRA(ZEND_CALL_ARG(call, 1)) is set by ZEND_SEND_PLACEHOLDER */
		if (Z_EXTRA(argv[0]) == _IS_PLACEHOLDER_VARIADIC
				|| (ZEND_CALL_INFO(applied) & ZEND_APPLY_VARIADIC)) {
			ZEND_ADD_CALL_FLAG(partial, ZEND_APPLY_VARIADIC);
		}
		if (Z_EXTRA(argv[0]) == _IS_PLACEHOLDER_VARIADIC) {
			if (Z_IS_PLACEHOLDER_VARIADIC_P(&argv[argc-1])
					&& argc > applied->trampoline.common.num_args) {
				argc--;
			}
		}
		partial->argc = applied->argc - applied->trampoline.common.num_args + MAX(applied->trampoline.common.num_args, argc);
		partial->argv = safe_emalloc(partial->argc, sizeof(zval), 0);
		zend_partial_apply(
			applied->argv, applied->argv + applied->argc,
			argv, argv + argc,
			partial->argv);

		if (ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC)) {
			for (uint32_t i = 0; i < partial->argc; i++) {
				if (Z_ISUNDEF(partial->argv[i])) {
					Z_TYPE_INFO(partial->argv[i]) = _IS_PLACEHOLDER_ARG;
				}
			}
		}

		if (extra_named_params) {
			if (applied->named) {
				partial->named = zend_array_dup(applied->named);

				zend_hash_merge(partial->named, extra_named_params, zval_copy_ctor, true);
			} else {
				partial->named = extra_named_params;
				GC_ADDREF(extra_named_params);
			}
		}
	} else {
		/* Z_EXTRA(ZEND_CALL_ARG(call, 1)) is set in ZEND_SEND_PLACEHOLDER */
		if (Z_EXTRA(argv[0]) == _IS_PLACEHOLDER_VARIADIC) {
			ZEND_ADD_CALL_FLAG(partial, ZEND_APPLY_VARIADIC);
			if (Z_IS_PLACEHOLDER_VARIADIC_P(&argv[argc-1])
					&& argc > function->common.num_args) {
				argc--;
			}
			partial->argc = MAX(argc, function->common.num_args);
		} else {
			partial->argc = argc;
		}

		partial->argv = safe_emalloc(partial->argc, sizeof(zval), 0);
		memcpy(partial->argv, argv, argc * sizeof(zval));

		if (ZEND_PARTIAL_CALL_FLAG(partial, ZEND_APPLY_VARIADIC)) {
			for (uint32_t i = 0; i < argc; i++) {
				if (Z_ISUNDEF(partial->argv[i])) {
					Z_TYPE_INFO(partial->argv[i]) = _IS_PLACEHOLDER_ARG;
				}
			}
			for (uint32_t i = argc; i < partial->argc; i++) {
				Z_TYPE_INFO(partial->argv[i]) = _IS_PLACEHOLDER_ARG;
			}
		}

		if (extra_named_params) {
			partial->named = extra_named_params;
			GC_ADDREF(extra_named_params);
		}
	}

	memcpy(&partial->func, function, ZEND_PARTIAL_FUNC_SIZE(function));

	ZEND_PARTIAL_FUNC_DEL(&partial->func, ZEND_ACC_CLOSURE);
	ZEND_PARTIAL_FUNC_ADD(&partial->func, ZEND_ACC_IMMUTABLE);

	if (ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
		ZEND_PARTIAL_FUNC_ADD(&partial->func, ZEND_ACC_TRAMPOLINE_PERMANENT);
	}

	ZEND_PARTIAL_FUNC_ADD(&partial->func, ZEND_ACC_FAKE_CLOSURE);

	if (partial->func.type == ZEND_USER_FUNCTION) {
		if (!ZEND_PARTIAL_FUNC_FLAG(&partial->func, ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
			/* When the function is a trampoline, the function name is already
			 * addref'ed as it's supposed to be consumed by the call. */
			zend_string_addref(partial->func.common.function_name);
		}

		partial->func.op_array.refcount = NULL;
	}

	zend_partial_trampoline_create(partial, &partial->trampoline);

	/* partial info may contain ZEND_APPLY_VARIADIC */
	uint32_t backup_info = ZEND_CALL_INFO(partial);

	if (Z_TYPE_P(this_ptr) == IS_UNDEF && Z_CE_P(this_ptr)) {
		ZVAL_COPY_VALUE(&partial->This, this_ptr);
	} else if (Z_TYPE_P(this_ptr) == IS_OBJECT) {
		zval *This;
		if (instanceof_function(Z_OBJCE_P(this_ptr), zend_ce_closure)) {
			if (zend_string_equals_ci(
					partial->func.common.function_name,
					ZSTR_KNOWN(ZEND_STR_MAGIC_INVOKE))) {
				This = this_ptr;
			} else {
				zend_partial *p = (zend_partial*)Z_OBJ_P(this_ptr);
				This = &p->This;
			}
		} else {
			This = this_ptr;
		}
		ZVAL_COPY(&partial->This, This);
	}

	ZEND_ADD_CALL_FLAG(partial, backup_info);
}

void zend_partial_bind(zval *result, zval *partial, zval *this_ptr, zend_class_entry *scope) {
	zval This;
	zend_partial *object = (zend_partial*) Z_OBJ_P(partial);

	ZVAL_UNDEF(&This);

	if (!this_ptr || Z_TYPE_P(this_ptr) != IS_OBJECT) {
		ZEND_ASSERT(scope && "scope must be set");

		Z_CE(This) = scope;
	} else {
		ZVAL_COPY_VALUE(&This, this_ptr);
	}

	zend_partial_create(result, ZEND_CALL_INFO(object), &This,
		&object->func, object->argc, object->argv, object->named);

	zval *argv = object->argv,
		 *end = argv + object->argc;

	while (argv < end) {
		Z_TRY_ADDREF_P(argv);
		argv++;
	}
}
