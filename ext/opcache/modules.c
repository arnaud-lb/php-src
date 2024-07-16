
#include "Zend/zend.h"
#include "zend_accelerator_hash.h"
#include "zend_compile.h"
#include "zend_errors.h"
#include "zend_exceptions.h"
#include "zend_execute.h"
#include "zend_hash.h"
#include "zend_API.h"
#include "zend_ini.h"
#include "zend_ini_scanner.h"
#include "zend_operators.h"
#include "zend_portability.h"
#include "zend_smart_str.h"
#include "zend_string.h"
#include "zend_types.h"
#include "ZendAccelerator.h"
#include "zend_virtual_cwd.h"
#include "zend_vm_opcodes.h"
#include "zend_inheritance.h"
#include "zend_observer.h"
#include "zend_shared_alloc.h"
#include "zend_accelerator_util_funcs.h"
#include "zend_persist.h"
#include "zend_vm.h"

#include <glob.h>
#include <fnmatch.h>

typedef struct _zend_user_module_desc {
	zend_string *name;
	zend_string *path; /* ends with DEFAULT_SLASH_STR */
	zend_string *resolved_path; /* ends with DEFAULT_SLASH_STR */
	zend_string **include_patterns; /* NULL terminated */;
	zend_string **exclude_patterns; /* NULL terminated */;
} zend_user_module_desc;

static void zend_user_module_desc_destroy(zend_user_module_desc *module_desc)
{
	if (module_desc->name) {
		zend_string_release(module_desc->name);
	}
	if (module_desc->path) {
		zend_string_release(module_desc->path);
	}
	if (module_desc->resolved_path) {
		zend_string_release(module_desc->resolved_path);
	}
	if (module_desc->include_patterns) {
		zend_string **str = module_desc->include_patterns;
		while (*str) {
			zend_string_release(*str);
			str++;
		}
		efree(module_desc->include_patterns);
	}
	if (module_desc->exclude_patterns) {
		zend_string **str = module_desc->exclude_patterns;
		while (*str) {
			zend_string_release(*str);
			str++;
		}
		efree(module_desc->exclude_patterns);
	}

}

#if 0
static void zend_user_module_destroy(zend_user_module *module)
{
	zend_string_release(module->name);
	zend_string_release(module->lcname);
	zend_string_release(module->path);
	zend_string_release(module->resolved_path);
	zend_hash_destroy(&module->op_arrays);
}
#endif

static zend_string **split_patterns(zend_string *str)
{
	size_t buf_capacity = 1;
	size_t buf_len = 0;
	zend_string **buf = emalloc(sizeof(zend_string*));

	char *start = ZSTR_VAL(str);
	char *end = start + ZSTR_LEN(str);
	char *pos = start;
	smart_str current = {0};

	// TODO: comma separated?
	// TODO: allow to escape separator
	while (pos < end) {
		switch (*pos) {
			case ' ':
				if (smart_str_get_len(&current)) {
					if (buf_len+1 == buf_capacity) {
						buf_capacity = buf_capacity * 2;
						buf = erealloc(buf, buf_capacity);
					}
					buf[buf_len++] = smart_str_extract(&current);
				}
				break;
			default:
				smart_str_appendc(&current, *pos);
				break;
		}
		pos++;
	}

	if (smart_str_get_len(&current)) {
		if (buf_len+1 == buf_capacity) {
			buf_capacity = buf_capacity+1;
			buf = erealloc(buf, buf_capacity * sizeof(zend_string*));
		}
		buf[buf_len++] = smart_str_extract(&current);
	}

	ZEND_ASSERT(buf_len+1 <= buf_capacity);
	buf[buf_len] = NULL;
	return buf;
}

static void zend_validate_user_module_name(zend_string *name)
{
	// TODO: For now just check there is no leading/trailing backslash
	ZEND_ASSERT(ZSTR_LEN(name));
	if (ZSTR_VAL(name)[0] == '\\' || ZSTR_VAL(name)[ZSTR_LEN(name)]-1 == '\\') {
		zend_error_noreturn(E_CORE_ERROR,
				"Module name must not start or end with '\\': '%s'",
				ZSTR_VAL(name));
	}
}

static void zend_user_module_desc_parser_cb(zval *arg1, zval *arg2, zval *arg3, int callback_type, void *module_desc_ptr)
{
	zend_user_module_desc *module_desc = (zend_user_module_desc*) module_desc_ptr;

	switch (callback_type) {
		case ZEND_INI_PARSER_ENTRY:
			if (!arg2) {
				/* no value given */;
				break;
			}
			if (zend_string_equals_cstr(Z_STR_P(arg1), "module", strlen("module"))) {
				if (module_desc->name) {
					zend_error_noreturn(E_CORE_ERROR, "Duplicated 'module' entry");
				}

				convert_to_string(arg2);
				zend_validate_user_module_name(Z_STR_P(arg2));
				module_desc->name = zend_string_copy(Z_STR_P(arg2));
			} else if (zend_string_equals_cstr(Z_STR_P(arg1), "files", strlen("files"))) {
				if (module_desc->include_patterns) {
					zend_error_noreturn(E_CORE_ERROR, "Duplicated 'files' entry");
				}

				convert_to_string(arg2);
				module_desc->include_patterns = split_patterns(Z_STR_P(arg2));
			} else if (zend_string_equals_cstr(Z_STR_P(arg1), "exclude", strlen("exclude"))) {
				if (module_desc->exclude_patterns) {
					zend_error_noreturn(E_CORE_ERROR, "Duplicated 'exclude' entry");
				}

				convert_to_string(arg2);
				module_desc->exclude_patterns = split_patterns(Z_STR_P(arg2));
			} else {
				zend_error_noreturn(E_CORE_ERROR, "Unknown entry: '%s'", Z_STRVAL_P(arg1));
			}
			break;

		case ZEND_INI_PARSER_POP_ENTRY:
			zend_error_noreturn(E_CORE_ERROR, "Unknown entry: '%s'", Z_STRVAL_P(arg1));
			break;

		default:
			break;
	}
}

static zend_op_array *zend_user_module_compile_file(zend_user_module *module, zend_string *file)
{
	zend_file_handle file_handle;
	zend_stream_init_filename(&file_handle, ZSTR_VAL(file));

	zend_op_array *op_array = zend_compile_file(&file_handle, ZEND_REQUIRE);
	zend_destroy_file_handle(&file_handle);

	ZEND_ASSERT(!(op_array->fn_flags & ZEND_ACC_IMMUTABLE));

	if (op_array->user_module != module) {
		zend_error_noreturn(E_COMPILE_ERROR,
				"Module files must declare module '%s'",
				ZSTR_VAL(module->name));
	}

	return op_array;
}

static void zend_user_module_find_files(zend_user_module_desc *module_desc, HashTable *filenames)
{
	zend_string **pattern_p = module_desc->include_patterns;

	while (*pattern_p) {
		// TODO: check path traversal

		zend_string *pattern = zend_string_concat2(
				ZSTR_VAL(module_desc->resolved_path), ZSTR_LEN(module_desc->resolved_path),
				ZSTR_VAL(*pattern_p), ZSTR_LEN(*pattern_p));

		glob_t globbuf = {0};
		int ret = glob(ZSTR_VAL(pattern), GLOB_MARK, NULL, &globbuf);

		if (ret != 0) {
			if (ret == GLOB_NOMATCH) {
				globbuf.gl_pathc = 0;
			} else {
				globfree(&globbuf);
				zend_error_noreturn(E_CORE_ERROR, "Invalid pattern '%s'", ZSTR_VAL(*pattern_p));
			}
		}

		for (size_t n = 0; n < (size_t)globbuf.gl_pathc;) {
			const char *path = globbuf.gl_pathv[n];
			size_t path_len = strlen(path);
			ZEND_ASSERT(path_len);
			ZEND_ASSERT(path_len >= ZSTR_LEN(module_desc->path));

			// Skip directories (GLOB_MARK appends a slash at the end of
			// directories)
#ifdef ZEND_WIN32
			const char slash = '\\';
#else
			const char slash = '/';
#endif
			if (path[path_len-1] == slash) {
				goto next;
			}

			// Skip excluded files
			const char *rel_path = path + ZSTR_LEN(module_desc->path) + 1;
			zend_string **exclude_pattern_p = module_desc->exclude_patterns;
			while (*exclude_pattern_p) {
				zend_string *pattern = *exclude_pattern_p;
				if (!fnmatch(ZSTR_VAL(pattern), rel_path, 0)) {
					goto next;
				}
				exclude_pattern_p++;
			}

			zend_string *filename = zend_string_init(path, path_len, 0);
			zend_hash_add_empty_element(filenames, filename);
			zend_string_release(filename);
next:
			n++;
		}

		globfree(&globbuf);
		zend_string_release(pattern);
		pattern_p++;
	}
}

// TODO: reset flags
#define ZEND_ACC_HAS_STATEMENTS ZEND_ACC_PRIVATE
#define ZEND_ACC_VISITED        ZEND_ACC_PROTECTED

static void zend_user_module_update_class_map(zend_user_module *module, zend_op_array *op_array, HashTable *map, uint32_t offset)
{
	zend_class_entry *ce;
	ZEND_HASH_FOREACH_PTR_FROM(CG(class_table), ce, offset) {
		ZEND_ASSERT(ce->info.user.module == module);
		ZEND_ASSERT(ce->info.user.filename == op_array->filename);
		ZEND_ASSERT(ce->type == ZEND_USER_CLASS);
		if (!(ce->ce_flags & ZEND_ACC_TOP_LEVEL)) {
			continue;
		}
		zend_string *lcname = zend_string_tolower(ce->name);
		if (!zend_hash_add_new_ptr(map, lcname, op_array)) {
			// TODO: improve error message (include definition locations)
			zend_error_noreturn(E_COMPILE_ERROR,
					"Duplicate class declaration: '%s'",
					ZSTR_VAL(ce->name));
		}
		zend_string_release(lcname);
	} ZEND_HASH_FOREACH_END();
}

static void zend_user_module_update_func_map(zend_user_module *module, zend_op_array *op_array, HashTable *map, uint32_t offset)
{
	zend_function *fn;
	ZEND_HASH_FOREACH_PTR_FROM(CG(function_table), fn, offset) {
		ZEND_ASSERT(fn->type == ZEND_USER_CLASS);
		zend_op_array *op_array = &fn->op_array;
		ZEND_ASSERT(op_array->user_module == module);
		ZEND_ASSERT(op_array->filename == op_array->filename);
		if (!(op_array->fn_flags & ZEND_ACC_TOP_LEVEL)) {
			continue;
		}
		if (!zend_hash_add_new_ptr(map,
					zend_string_tolower(op_array->function_name), op_array)) {
			// TODO: improve error message (include definition locations)
			zend_error_noreturn(E_COMPILE_ERROR,
					"Duplicate function declaration: '%s'",
					ZSTR_VAL(op_array->function_name));
		}
	} ZEND_HASH_FOREACH_END();
}

typedef struct _zend_user_module_ordered_file_list {
	zend_op_array **files; /* All files */
	zend_op_array **next_file; /* Next file is inserted here */
	zend_op_array **next_file_with_stmts; /* Next file with statements is inserted here */
	size_t          num_files;
} zend_user_module_ordered_file_list;

static ZEND_COLD ZEND_NORETURN void zend_user_module_dep_circular_error(zend_user_module *module, zend_op_array *op_array)
{
	smart_str s = {0};
	zend_op_array *prev = NULL;
	zend_op_array *next = NULL;
	do {
		next = (zend_op_array*)op_array->function_name;
		op_array->function_name = (zend_string*)prev;
		prev = op_array;
		op_array = next;
	} while (op_array != (zend_op_array*)module);

	op_array = prev;
	do {
		if (smart_str_get_len(&s)) {
			smart_str_appends(&s, " -> ");
		}
		smart_str_append(&s, op_array->filename);
	} while (op_array);

	smart_str_0(&s);

	zend_error_noreturn(E_COMPILE_ERROR,
			"Circular dependency detected between the following files: %s",
			ZSTR_VAL(s.s));
}

static void zend_user_module_dep_add_op_array(zend_user_module *module, zend_op_array *op_array, zend_op_array *prev_op_array, HashTable *classmap, HashTable *funcmap, zend_user_module_ordered_file_list *list);

static void zend_user_module_dep_add_class_lc(zend_user_module *module, zend_string *lcname, zend_op_array *op_array, zend_op_array *prev_op_array, HashTable *classmap, HashTable *funcmap, zend_user_module_ordered_file_list *list)
{
	if (!zend_string_starts_with(lcname, module->lcname)
			|| ZSTR_VAL(lcname)[ZSTR_LEN(module->lcname)] != '\\') {
		return;
	}

	zend_op_array *dep_op_array = zend_hash_find_ptr(classmap, lcname);
	if (dep_op_array && dep_op_array != op_array) {
		zend_user_module_dep_add_op_array(module, dep_op_array, op_array, classmap, funcmap, list);
	}
}

static void zend_user_module_dep_add_class(zend_user_module *module, zend_string *name, zend_op_array *op_array, zend_op_array *prev_op_array, HashTable *classmap, HashTable *funcmap, zend_user_module_ordered_file_list *list)
{
	if (!zend_string_starts_with_ci(name, module->lcname)
			|| ZSTR_VAL(name)[ZSTR_LEN(module->lcname)] != '\\') {
		return;
	}

	zend_string *lcname = zend_string_tolower(name);
	zend_op_array *dep_op_array = zend_hash_find_ptr(classmap, lcname);
	if (dep_op_array && dep_op_array != op_array) {
		zend_user_module_dep_add_op_array(module, dep_op_array, op_array, classmap, funcmap, list);
	}
	zend_string_release(lcname);
}

static void zend_user_module_dep_add_op_array(zend_user_module *module, zend_op_array *op_array, zend_op_array *prev_op_array, HashTable *classmap, HashTable *funcmap, zend_user_module_ordered_file_list *list)
{
	zend_op *opline = op_array->opcodes;
	zend_op *end = opline + op_array->last;
	zend_string *key;

	if (op_array->fn_flags & ZEND_ACC_VISITED) {
		return;
	}
	if (op_array->function_name != NULL) {
		zend_user_module_dep_circular_error(module, op_array);
	}
	op_array->function_name = (zend_string*)prev_op_array;

	while (opline != end) {
		switch (opline->opcode) {
			case ZEND_DECLARE_CLASS:
			case ZEND_DECLARE_CLASS_DELAYED:
				key = Z_STR_P(RT_CONSTANT(opline, opline->op1) + 1);
				zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), key);
				ZEND_ASSERT(ce);
				ZEND_ASSERT(ce->ce_flags & ZEND_ACC_TOP_LEVEL);
				if (ce->parent_name) {
					zend_user_module_dep_add_class(module, ce->parent_name, op_array, prev_op_array, classmap, funcmap, list);
				}
				for (uint32_t i = 0; i < ce->num_interfaces; i++) {
					zend_user_module_dep_add_class_lc(module, ce->interface_names[i].lc_name, op_array, prev_op_array, classmap, funcmap, list);
				}
				for (uint32_t i = 0; i < ce->num_traits; i++) {
					zend_user_module_dep_add_class_lc(module, ce->trait_names[i].lc_name, op_array, prev_op_array, classmap, funcmap, list);
				}
				break;
			case ZEND_DECLARE_FUNCTION:
			case ZEND_RETURN:
				break;
			default:
				op_array->fn_flags |= ZEND_ACC_HAS_STATEMENTS;
				goto end;
		}
		opline++;
	}

end:
	if (op_array->fn_flags & ZEND_ACC_HAS_STATEMENTS) {
		*list->next_file_with_stmts = op_array;
		list->next_file_with_stmts--;
	} else {
		*list->next_file = op_array;
		list->next_file++;
	}
	op_array->fn_flags |= ZEND_ACC_VISITED;
	op_array->function_name = NULL;
}

static zend_user_module_ordered_file_list *zend_user_module_sort_initial_op_arrays(zend_user_module *module, HashTable *classmap, HashTable *funcmap)
{
	zend_user_module_ordered_file_list *list = safe_emalloc(
			module->op_arrays.nNumOfElements, sizeof(zend_op_array*),
			sizeof(*list));

	list->num_files = module->op_arrays.nNumOfElements;
	list->files = (zend_op_array**)((char*)list + sizeof(*list));
	list->next_file = list->files;
	list->next_file_with_stmts = list->files + list->num_files - 1;

	zend_op_array *op_array;
	ZEND_HASH_FOREACH_PTR(&module->op_arrays, op_array) {
		zend_user_module_dep_add_op_array(module, op_array, (zend_op_array*)module, classmap, funcmap, list);
	} ZEND_HASH_FOREACH_END();

	ZEND_ASSERT(list->next_file == list->next_file_with_stmts + 1);

	return list;
}

static zend_result zend_user_module_check_class_exists(zend_string *name, zend_string *lcname)
{
	if (zend_string_equals_literal_ci(name, "self")) {
		return SUCCESS;
	} else if (zend_string_equals_literal_ci(name, "parent")) {
		return SUCCESS;
	} else if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_STATIC))) {
		return SUCCESS;
	}
	if (UNEXPECTED(!zend_fetch_class_by_name(name, lcname, 0))) {
		return FAILURE;
	}
	return SUCCESS;
}

static zend_result zend_user_module_check_deps_type(zend_type type)
{
	zend_type *single_type;
	ZEND_TYPE_FOREACH(type, single_type) {
		if (ZEND_TYPE_HAS_NAME(*single_type)) {
			if (UNEXPECTED(zend_user_module_check_class_exists(ZEND_TYPE_NAME(*single_type), NULL) == FAILURE)) {
				return FAILURE;
			}
		} else if (ZEND_TYPE_HAS_LIST(*single_type)) {
			if (UNEXPECTED(!zend_user_module_check_deps_type(*single_type))) {
				return FAILURE;
			}
		}
	} ZEND_TYPE_FOREACH_END();

	return SUCCESS;
}

static zend_result zend_user_module_check_deps_op_array(HashTable *class_table, HashTable *function_table, zend_op_array *op_array);

#define CHECK_CLASS(node) do {          \
	if (UNEXPECTED(zend_user_module_check_class_exists(Z_STR_P(RT_CONSTANT(opline, node)), Z_STR_P(RT_CONSTANT(opline, node)+1))) == FAILURE) { \
		return FAILURE;                 \
	}                                   \
} while (0)

static zend_result zend_user_module_check_deps_class(HashTable *class_table, HashTable *function_table, zend_class_entry *ce)
{
	ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);

	// TODO: parent class or interface or trait not in a module?
	// also classes depending on evaluated code

	zend_property_info *prop_info;
	ZEND_HASH_MAP_FOREACH_PTR(&ce->properties_info, prop_info) {
		if (UNEXPECTED(zend_user_module_check_deps_type(prop_info->type) == FAILURE)) {
			return FAILURE;
		}
		// TODO: hooks
	} ZEND_HASH_FOREACH_END();

	zend_function *fn;
	ZEND_HASH_MAP_FOREACH_PTR(&ce->function_table, fn) {
		if (fn->type == ZEND_USER_FUNCTION) {
			if (UNEXPECTED(zend_user_module_check_deps_op_array(class_table, function_table, &fn->op_array) == FAILURE)) {
				return FAILURE;
			}
		}
	} ZEND_HASH_FOREACH_END();

	return SUCCESS;
}

static zend_result zend_user_module_check_deps_op_array(HashTable *class_table, HashTable *function_table, zend_op_array *op_array)
{
	if (op_array->arg_info) {
		zend_arg_info *start = op_array->arg_info
			- (bool)(op_array->fn_flags & ZEND_ACC_HAS_RETURN_TYPE);
		zend_arg_info *end = op_array->arg_info
			+ op_array->num_args
			+ (bool)(op_array->fn_flags & ZEND_ACC_VARIADIC);
		while (start < end) {
			zend_user_module_check_deps_type(start->type);
			start++;
		}
	}

	zend_op *opline = op_array->opcodes;
	zend_op *end = opline + op_array->last;

	while (opline < end) {
		switch (opline->opcode) {
			case ZEND_INIT_STATIC_METHOD_CALL:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS(opline->op1);
				}
				break;
			case ZEND_CATCH:
				CHECK_CLASS(opline->op1);
				break;
			case ZEND_FETCH_CLASS_CONSTANT:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS(opline->op1);
				}
				break;
			case ZEND_ASSIGN_STATIC_PROP:
			case ZEND_ASSIGN_STATIC_PROP_REF:
			case ZEND_FETCH_STATIC_PROP_R:
			case ZEND_FETCH_STATIC_PROP_W:
			case ZEND_FETCH_STATIC_PROP_RW:
			case ZEND_FETCH_STATIC_PROP_IS:
			case ZEND_FETCH_STATIC_PROP_UNSET:
			case ZEND_FETCH_STATIC_PROP_FUNC_ARG:
			case ZEND_UNSET_STATIC_PROP:
			case ZEND_ISSET_ISEMPTY_STATIC_PROP:
			case ZEND_PRE_INC_STATIC_PROP:
			case ZEND_PRE_DEC_STATIC_PROP:
			case ZEND_POST_INC_STATIC_PROP:
			case ZEND_POST_DEC_STATIC_PROP:
			case ZEND_ASSIGN_STATIC_PROP_OP:
				if (opline->op2_type == IS_CONST) {
					CHECK_CLASS(opline->op2);
				}
				break;
			case ZEND_FETCH_CLASS:
			case ZEND_INSTANCEOF:
				if (opline->op2_type == IS_CONST) {
					CHECK_CLASS(opline->op2);
				}
				break;
			case ZEND_NEW:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS(opline->op1);
				}
				break;
			case ZEND_DECLARE_CLASS:
			case ZEND_DECLARE_CLASS_DELAYED: {
				if (op_array->function_name) {
					zend_error_noreturn(E_COMPILE_ERROR, "Can not declare named class in function body");
				}
				zend_string *lcname = Z_STR_P(RT_CONSTANT(opline, opline->op1));
				zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), lcname);
				if (ce) {
					ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);
					if (UNEXPECTED(zend_user_module_check_deps_class(class_table, function_table, ce) == FAILURE)) {
						return FAILURE;
					}
					if (class_table) {
						zend_hash_add_new_ptr(class_table, lcname, ce);
						ce->refcount++;
					}
				}
				break;
			}
			case ZEND_DECLARE_ANON_CLASS: {
				zend_string *rtd_key = Z_STR_P(RT_CONSTANT(opline, opline->op1));
				zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), rtd_key);
				ZEND_ASSERT(ce);
				ce = zend_do_link_class(ce, (opline->op2_type == IS_CONST) ? Z_STR_P(RT_CONSTANT(opline, opline->op2)) : NULL, rtd_key);
				if (UNEXPECTED(!ce)) {
					return FAILURE;
				}
				ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);
				if (UNEXPECTED(zend_user_module_check_deps_class(class_table, function_table, ce) == FAILURE)) {
					return FAILURE;
				}
				if (class_table) {
					zend_hash_add_new_ptr(class_table, rtd_key, ce);
					ce->refcount++;
				}
				break;
			}
			case ZEND_DECLARE_FUNCTION:
				if (function_table) {
					zend_string *lcname = Z_STR_P(RT_CONSTANT(opline, opline->op1));
					zend_function *fn = (zend_function*) op_array->dynamic_func_defs[opline->op2.num];
					zend_hash_add_new_ptr(function_table, lcname, fn);
					ZEND_ASSERT(fn->type == ZEND_USER_FUNCTION);
					if (fn->op_array.refcount) {
						(*fn->op_array.refcount)++;
					}
				}
				break;
		}
		opline++;
	}

	if (op_array->num_dynamic_func_defs) {
		for (uint32_t i = 0; i < op_array->num_dynamic_func_defs; i++) {
			if (UNEXPECTED(zend_user_module_check_deps_op_array(class_table, function_table, op_array->dynamic_func_defs[i]) == FAILURE)) {
				return FAILURE;
			}
		}
	}

	return SUCCESS;
}

/* ZEND_HASH_MAP_FOREACH_FROM, but append-safe and iterate on newly added elements */
#define ZEND_HASH_MAP_FOREACH_FROM_APPEND(_ht, indirect, _from) do { \
		const HashTable *__ht = (_ht); \
		ZEND_ASSERT(!HT_IS_PACKED(__ht)); \
		for (uint32_t _i = (_from); _i < __ht->nNumUsed; _i++) { \
			Bucket *_p = __ht->arData + _i; \
			zval *_z = &_p->val; \
			if (indirect && Z_TYPE_P(_z) == IS_INDIRECT) { \
				_z = Z_INDIRECT_P(_z); \
			} \
			if (UNEXPECTED(Z_TYPE_P(_z) == IS_UNDEF)) continue; \

static zend_result zend_user_module_check_deps(zend_user_module *module, HashTable *class_table, HashTable *function_table)
{
	ZEND_HASH_MAP_FOREACH_FROM_APPEND(&module->op_arrays, 0, 0) {
		zend_op_array *op_array = Z_PTR_P(_z);
		if (UNEXPECTED(zend_user_module_check_deps_op_array(class_table, function_table, op_array) == FAILURE)) {
			return FAILURE;
		}
	} ZEND_HASH_FOREACH_END();

	return SUCCESS;
}

#if 0
static void zend_user_module_link(uint32_t orig_class_table_offset)
{
	zval *zv;
	zend_persistent_script *script;
	zend_class_entry *ce;
	zend_string *key;
	bool changed;

	HashTable errors;
	zend_hash_init(&errors, 0, NULL, NULL, 0);

	/* Resolve class dependencies */
	do {
		changed = false;

		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL_FROM(EG(class_table), key, zv, orig_class_table_offset) {
			ce = Z_PTR_P(zv);
			ZEND_ASSERT(ce->type != ZEND_INTERNAL_CLASS);

			if (!(ce->ce_flags & (ZEND_ACC_TOP_LEVEL|ZEND_ACC_ANON_CLASS))
					|| (ce->ce_flags & ZEND_ACC_LINKED)) {
				continue;
			}

			zend_string *lcname = zend_string_tolower(ce->name);
			if (!(ce->ce_flags & ZEND_ACC_ANON_CLASS)) {
				if (zend_hash_exists(EG(class_table), lcname)) {
					zend_string_release(lcname);
					continue;
				}
			}

			preload_error error_info;
			if (preload_resolve_deps(&error_info, ce) == FAILURE) {
				zend_string_release(lcname);
				continue;
			}

			zv = zend_hash_set_bucket_key(EG(class_table), (Bucket*)zv, lcname);
			ZEND_ASSERT(zv && "We already checked above that the class doesn't exist yet");

			/* Set the FILE_CACHED flag to force a lazy load, and the CACHED flag to
			 * prevent freeing of interface names. */
			void *checkpoint = zend_arena_checkpoint(CG(arena));
			zend_class_entry *orig_ce = ce;
			uint32_t temporary_flags = ZEND_ACC_FILE_CACHED|ZEND_ACC_CACHED;
			ce->ce_flags |= temporary_flags;
			if (ce->parent_name) {
				zend_string_addref(ce->parent_name);
			}

			/* Record and suppress errors during inheritance. */
			orig_error_cb = zend_error_cb;
			zend_error_cb = preload_error_cb;
			zend_begin_record_errors();

			/* Set filename & lineno information for inheritance errors */
			CG(in_compilation) = true;
			CG(compiled_filename) = ce->info.user.filename;
			CG(zend_lineno) = ce->info.user.line_start;
			zend_try {
				ce = zend_do_link_class(ce, NULL, lcname);
				if (!ce) {
					ZEND_ASSERT(0 && "Class linking failed?");
				}
				ce->ce_flags &= ~temporary_flags;
				changed = true;

				/* Inheritance successful, print out any warnings. */
				zend_error_cb = orig_error_cb;
				zend_emit_recorded_errors();
			} zend_catch {
				/* Clear variance obligations that were left behind on bailout. */
				if (CG(delayed_variance_obligations)) {
					zend_hash_index_del(
						CG(delayed_variance_obligations), (uintptr_t) Z_CE_P(zv));
				}

				/* Restore the original class. */
				zv = zend_hash_set_bucket_key(EG(class_table), (Bucket*)zv, key);
				Z_CE_P(zv) = orig_ce;
				orig_ce->ce_flags &= ~temporary_flags;
				zend_arena_release(&CG(arena), checkpoint);

				/* Remember the last error. */
				zend_error_cb = orig_error_cb;
				EG(record_errors) = false;
				ZEND_ASSERT(EG(num_errors) > 0);
				zend_hash_update_ptr(&errors, key, EG(errors)[EG(num_errors)-1]);
				EG(num_errors)--;
			} zend_end_try();
			CG(in_compilation) = false;
			CG(compiled_filename) = NULL;
			zend_free_recorded_errors();
			zend_string_release(lcname);
		} ZEND_HASH_FOREACH_END();
	} while (changed);

	do {
		changed = false;

		ZEND_HASH_MAP_REVERSE_FOREACH_VAL(EG(class_table), zv) {
			ce = Z_PTR_P(zv);
			if (ce->type == ZEND_INTERNAL_CLASS) {
				break;
			}
			if ((ce->ce_flags & ZEND_ACC_LINKED) && !(ce->ce_flags & ZEND_ACC_CONSTANTS_UPDATED)) {
				if (!(ce->ce_flags & ZEND_ACC_TRAIT)) { /* don't update traits */
					CG(in_compilation) = true; /* prevent autoloading */
					if (preload_try_resolve_constants(ce)) {
						changed = true;
					}
					CG(in_compilation) = false;
				}
			}
		} ZEND_HASH_FOREACH_END();
	} while (changed);
}
#endif

#if 0
static void zend_load_user_module_dependency(zend_user_module *module)
{
	// TODO: check if module is in CG(module_table) (is already loaded)
	// TODO: load classes and functions
}

static void zend_load_user_module(zend_user_module *module)
{
	for (uint32_t i = 0; i < module->num_dependencies; i++) {
		zend_load_user_module_dependency(module->dependencies[i]);
	}

	// TODO: load classes and functions
}
#endif

static void zend_user_module_sort_classes(void *base, size_t count, size_t siz, compare_func_t compare, swap_func_t swp)
{
	Bucket *b1 = base;
	Bucket *b2;
	Bucket *end = b1 + count;
	Bucket tmp;
	zend_class_entry *ce, *p;

	while (b1 < end) {
try_again:
		ce = (zend_class_entry*)Z_PTR(b1->val);
		if (ce->parent && (ce->ce_flags & ZEND_ACC_LINKED)) {
			p = ce->parent;
			if (p->type == ZEND_USER_CLASS) {
				b2 = b1 + 1;
				while (b2 < end) {
					if (p ==  Z_PTR(b2->val)) {
						tmp = *b1;
						*b1 = *b2;
						*b2 = tmp;
						goto try_again;
					}
					b2++;
				}
			}
		}
		if (ce->num_interfaces && (ce->ce_flags & ZEND_ACC_LINKED)) {
			uint32_t i = 0;
			for (i = 0; i < ce->num_interfaces; i++) {
				p = ce->interfaces[i];
				if (p->type == ZEND_USER_CLASS) {
					b2 = b1 + 1;
					while (b2 < end) {
						if (p ==  Z_PTR(b2->val)) {
							tmp = *b1;
							*b1 = *b2;
							*b2 = tmp;
							goto try_again;
						}
						b2++;
					}
				}
			}
		}
		b1++;
	}
}

static void zend_user_module_register_trait_methods(zend_class_entry *ce) {
	zend_op_array *op_array;
	ZEND_HASH_MAP_FOREACH_PTR(&ce->function_table, op_array) {
		if (!(op_array->fn_flags & ZEND_ACC_TRAIT_CLONE)) {
			ZEND_ASSERT(op_array->refcount && "Must have refcount pointer");
			zend_shared_alloc_register_xlat_entry(op_array->refcount, op_array);
		}
	} ZEND_HASH_FOREACH_END();
}

static void zend_user_module_fix_trait_methods(zend_class_entry *ce)
{
	zend_op_array *op_array;

	ZEND_HASH_MAP_FOREACH_PTR(&ce->function_table, op_array) {
		if (op_array->fn_flags & ZEND_ACC_TRAIT_CLONE) {
			zend_op_array *orig_op_array = zend_shared_alloc_get_xlat_entry(op_array->refcount);
			ZEND_ASSERT(orig_op_array && "Must be in xlat table");

			zend_string *function_name = op_array->function_name;
			zend_class_entry *scope = op_array->scope;
			uint32_t fn_flags = op_array->fn_flags;
			zend_function *prototype = op_array->prototype;
			HashTable *ht = op_array->static_variables;
			*op_array = *orig_op_array;
			op_array->function_name = function_name;
			op_array->scope = scope;
			op_array->fn_flags = fn_flags;
			op_array->prototype = prototype;
			op_array->static_variables = ht;
		}
	} ZEND_HASH_FOREACH_END();
}

static void zend_user_module_optimize(zend_user_module *module, zend_script *script)
{
	zend_class_entry *ce;

	zend_shared_alloc_init_xlat_table();

	ZEND_HASH_MAP_FOREACH_PTR(&script->class_table, ce) {
		if (ce->ce_flags & ZEND_ACC_TRAIT) {
			zend_user_module_register_trait_methods(ce);
		}
	} ZEND_HASH_FOREACH_END();

	zend_optimize_script(script, ZCG(accel_directives).optimization_level, ZCG(accel_directives).opt_debug_level);
	// TODO
	// zend_accel_finalize_delayed_early_binding_list(script);

	ZEND_HASH_MAP_FOREACH_PTR(&script->class_table, ce) {
		zend_user_module_fix_trait_methods(ce);
	} ZEND_HASH_FOREACH_END();

	zend_shared_alloc_destroy_xlat_table();
}

/**
 * Clear AVX/SSE2-aligned memory.
 */
static void bzero_aligned(void *mem, size_t size)
{
#if defined(__x86_64__)
	memset(mem, 0, size);
#elif defined(__AVX__)
	char *p = (char*)mem;
	char *end = p + size;
	__m256i ymm0 = _mm256_setzero_si256();

	while (p < end) {
		_mm256_store_si256((__m256i*)p, ymm0);
		_mm256_store_si256((__m256i*)(p+32), ymm0);
		p += 64;
	}
#elif defined(__SSE2__)
	char *p = (char*)mem;
	char *end = p + size;
	__m128i xmm0 = _mm_setzero_si128();

	while (p < end) {
		_mm_store_si128((__m128i*)p, xmm0);
		_mm_store_si128((__m128i*)(p+16), xmm0);
		_mm_store_si128((__m128i*)(p+32), xmm0);
		_mm_store_si128((__m128i*)(p+48), xmm0);
		p += 64;
	}
#else
	memset(mem, 0, size);
#endif
}

static zend_persistent_user_module* zend_user_module_save_script_in_shared_memory(zend_persistent_user_module *pmodule)
{
	zend_accel_hash_entry *bucket;
	uint32_t memory_used;
	uint32_t checkpoint;

	if (zend_accel_hash_is_full(&ZCSG(hash))) {
		zend_accel_error_noreturn(ACCEL_LOG_FATAL, "Not enough entries in hash table for caching module. Consider increasing the value for the opcache.max_accelerated_files directive in php.ini.");
	}

	checkpoint = zend_shared_alloc_checkpoint_xlat_table();

	/* Calculate the required memory size */
	memory_used = zend_accel_user_module_persist_calc(pmodule, 1);

	/* Allocate shared memory */
	ZCG(mem) = zend_shared_alloc_aligned(memory_used);
	if (!ZCG(mem)) {
		zend_accel_error_noreturn(ACCEL_LOG_FATAL, "Not enough shared memory for caching module. Consider increasing the value for the opcache.memory_consumption directive in php.ini.");
	}

	bzero_aligned(ZCG(mem), memory_used);

	zend_shared_alloc_restore_xlat_table(checkpoint);

	/* Copy into shared memory */
	pmodule = zend_accel_user_module_persist(pmodule, 1);

	// TODO
	// new_persistent_script->is_phar = is_phar_file(new_persistent_script->script.filename);

	/* Consistency check */
	if ((char*)pmodule->script->mem + pmodule->script->size != (char*)ZCG(mem)) {
		zend_accel_error(
			((char*)pmodule->script->mem + pmodule->script->size < (char*)ZCG(mem)) ? ACCEL_LOG_ERROR : ACCEL_LOG_WARNING,
			"Internal error: wrong size calculation: module %s start=" ZEND_ADDR_FMT ", end=" ZEND_ADDR_FMT ", real=" ZEND_ADDR_FMT "\n",
			ZSTR_VAL(pmodule->module.name),
			(size_t)pmodule->script->mem,
			(size_t)((char *)pmodule->script->mem + pmodule->script->size),
			(size_t)ZCG(mem));
	}

	/* store script structure in the hash table */
	// TODO: hack
	zend_string *key = zend_string_concat2(
			"module://", strlen("module://"),
			ZSTR_VAL(pmodule->module.name), ZSTR_LEN(pmodule->module.name));
	bucket = zend_accel_hash_update(&ZCSG(hash), key, 0, pmodule);
	zend_string_release(key);
	if (bucket) {
		zend_accel_error(ACCEL_LOG_INFO, "Cached module '%s'", ZSTR_VAL(pmodule->module.name));
	}

	pmodule->script->dynamic_members.memory_consumption = ZEND_ALIGNED_SIZE(pmodule->script->size);

	return pmodule;
}

static void zend_user_module_load(zend_persistent_user_module *pmodule)
{
	/* Load into process tables */
	if (zend_hash_num_elements(&pmodule->script->script.function_table)) {
		Bucket *p = pmodule->script->script.function_table.arData;
		Bucket *end = p + pmodule->script->script.function_table.nNumUsed;

		zend_hash_extend(CG(function_table),
			CG(function_table)->nNumUsed + pmodule->script->script.function_table.nNumUsed, 0);
		for (; p != end; p++) {
			_zend_hash_append_ptr_ex(CG(function_table), p->key, Z_PTR(p->val), 1);
		}
	}

	if (zend_hash_num_elements(&pmodule->script->script.class_table)) {
		Bucket *p = pmodule->script->script.class_table.arData;
		Bucket *end = p + pmodule->script->script.class_table.nNumUsed;

		zend_hash_extend(CG(class_table),
			CG(class_table)->nNumUsed + pmodule->script->script.class_table.nNumUsed, 0);
		for (; p != end; p++) {
			_zend_hash_append_ex(CG(class_table), p->key, &p->val, 1);
		}
	}

	if (CG(map_ptr_last) != ZCSG(map_ptr_last)) {
		size_t old_map_ptr_last = CG(map_ptr_last);
		CG(map_ptr_last) = ZCSG(map_ptr_last);
		CG(map_ptr_size) = ZEND_MM_ALIGNED_SIZE_EX(CG(map_ptr_last) + 1, 4096);
		CG(map_ptr_real_base) = perealloc(CG(map_ptr_real_base), CG(map_ptr_size) * sizeof(void*), 1);
		CG(map_ptr_base) = ZEND_MAP_PTR_BIASED_BASE(CG(map_ptr_real_base));
		memset((void **) CG(map_ptr_real_base) + old_map_ptr_last, 0,
			(CG(map_ptr_last) - old_map_ptr_last) * sizeof(void *));
	}

	for (uint32_t i = 0; i < pmodule->num_dependencies; i++) {
		zend_persistent_user_module *dep = pmodule->dependencies[i];
		zend_user_module *module = zend_hash_find_ptr(CG(module_table), dep->module.lcname);

		if (module) {
			if (UNEXPECTED(module != &dep->module)) {
				zend_error_noreturn(E_CORE_ERROR, "Internal error");
			}
			continue;
		}

		zend_user_module_load(dep);
	}
}

static zend_never_inline ZEND_COLD zend_string *zend_autoload_stack_str(void)
{
	smart_str s = {0};

	if (EG(in_autoload)) {
		zend_string *key;
		ZEND_HASH_FOREACH_STR_KEY(EG(in_autoload), key) {
			if (smart_str_get_len(&s) != 0) {
				smart_str_appends(&s, " -> ");
			}
			smart_str_append(&s, key);
		} ZEND_HASH_FOREACH_END();
	}

	return smart_str_extract(&s);
}

static zend_never_inline ZEND_COLD void zend_circular_module_dependency_error(zend_user_module *module)
{
	smart_str s = {0};
	zend_user_module *other;
	bool add = false;
	ZEND_HASH_FOREACH_PTR(CG(module_table), other) {
		if (!add) {
			if (other == module) {
				add = true;
			} else {
				continue;
			}
		}
		if (smart_str_get_len(&s) != 0) {
			smart_str_appends(&s, " -> ");
		}
		smart_str_append(&s, other->name);
	} ZEND_HASH_FOREACH_END();

	smart_str_0(&s);

	zend_string *autoload_str = zend_autoload_stack_str();

	zend_throw_exception_ex(NULL, 0,
			"Circular dependency found between the following modules: %s (while autoloading classes: %s)",
			ZSTR_VAL(s.s),
			ZSTR_VAL(autoload_str));

	zend_string_release(autoload_str);
	smart_str_free(&s);
}

ZEND_API void zend_require_user_module(zend_string *module_path)
{
	zend_user_module *orig_module = CG(active_module);

	/* Load module metadata */

	zend_file_handle file_handle;
	zend_user_module_desc module_desc = {0};

	zend_stream_init_filename_ex(&file_handle, module_path);

	if (zend_parse_ini_file(&file_handle, 0, ZEND_INI_SCANNER_NORMAL, zend_user_module_desc_parser_cb, &module_desc) == FAILURE) {
		zend_destroy_file_handle(&file_handle);
		zend_user_module_desc_destroy(&module_desc);
		return;
	}

	zend_string *lcname = zend_string_tolower(module_desc.name);
	zend_user_module *loaded_module = zend_hash_find_ptr(EG(module_table), lcname);
	if (loaded_module) {
		zend_string_release(lcname);
		zend_user_module_desc_destroy(&module_desc);
		if (loaded_module->loading) {
			if (CG(active_module)
					&& zend_string_equals_ci(CG(active_module)->lcname, lcname)) {
				zend_string *autoload_str = zend_autoload_stack_str();
				zend_throw_error(NULL,
						"Can not require module '%s' while it is being loaded (while autoloading classes: %s)",
						ZSTR_VAL(module_desc.name),
						ZSTR_VAL(autoload_str));
				zend_string_release(autoload_str);
			} else {
				zend_circular_module_dependency_error(loaded_module);
			}
		}
		return;
	}

	zend_string *module_key = zend_string_concat2(
			"module://", strlen("module://"),
			ZSTR_VAL(lcname), ZSTR_LEN(lcname));
	zend_persistent_user_module *cached_module = zend_accel_hash_find(&ZCSG(hash), module_key);
	zend_string_release(module_key);
	if (cached_module) {
		zend_user_module_load(cached_module);
		return;
	}

	if (!module_desc.include_patterns) {
		module_desc.include_patterns = ecalloc(1, sizeof(zend_string*));
	}

	if (!module_desc.exclude_patterns) {
		module_desc.exclude_patterns = ecalloc(1, sizeof(zend_string*));
	}

	size_t module_resolved_path_len = zend_dirname(ZSTR_VAL(file_handle.opened_path), ZSTR_LEN(file_handle.opened_path));
	module_desc.resolved_path = zend_string_concat2(
			ZSTR_VAL(file_handle.opened_path), module_resolved_path_len,
			DEFAULT_SLASH_STR, strlen(DEFAULT_SLASH_STR));

	zend_destroy_file_handle(&file_handle);

	size_t module_path_len = zend_dirname(ZSTR_VAL(module_path), ZSTR_LEN(module_path));
	module_desc.path = zend_string_concat2(
			ZSTR_VAL(module_path), module_path_len,
			DEFAULT_SLASH_STR, strlen(DEFAULT_SLASH_STR));

	zend_user_module *module = emalloc(sizeof(zend_user_module));
	module->loading = true;
	module->name = zend_string_copy(module_desc.name);
	module->lcname = lcname;
	module->path = zend_string_copy(module_desc.path);
	module->resolved_path = zend_string_copy(module_desc.resolved_path);
	zend_hash_init(&module->op_arrays, 0, NULL, NULL, 0);

	CG(active_module) = module;

	zend_hash_update_ptr(EG(module_table), lcname, module);

	/* List initial files and compile */

	HashTable filenames;
	zend_hash_init(&filenames, 0, NULL, NULL, 0);
	zend_user_module_find_files(&module_desc, &filenames);
	if (filenames.nNumOfElements == 0) {
		zend_error_noreturn(E_CORE_ERROR, "No files matched");
	}

	uint32_t orig_compiler_options = CG(compiler_options_for_modules);
	// CG(compiler_options) |= ZEND_COMPILE_PRELOAD;
	CG(compiler_options_for_modules) |= ZEND_COMPILE_HANDLE_OP_ARRAY;
	CG(compiler_options_for_modules) |= ZEND_COMPILE_DELAYED_BINDING;
	CG(compiler_options_for_modules) |= ZEND_COMPILE_NO_CONSTANT_SUBSTITUTION;
	CG(compiler_options_for_modules) |= ZEND_COMPILE_IGNORE_OTHER_FILES;
	CG(compiler_options_for_modules) |= ZEND_COMPILE_WITHOUT_EXECUTION;

	HashTable classmap;
	zend_hash_init(&classmap, filenames.nNumOfElements, NULL, NULL, 0);

	HashTable funcmap;
	zend_hash_init(&funcmap, filenames.nNumOfElements, NULL, NULL, 0);

	uint32_t orig_module_table_offset = CG(module_table)->nNumUsed;

	zend_long h;
	zend_string *file;
	ZEND_HASH_FOREACH_KEY(&filenames, h, file) {
		ZEND_ASSERT(h);

		uint32_t class_table_offset = CG(class_table)->nNumUsed;
		uint32_t function_table_offset = CG(function_table)->nNumUsed;

		zend_op_array *op_array = zend_user_module_compile_file(module, file);

		zend_user_module_update_class_map(module, op_array, &classmap, class_table_offset);
		zend_user_module_update_func_map(module, op_array, &funcmap, function_table_offset);
	} ZEND_HASH_FOREACH_END();

	/* Execute initial files */

	zend_user_module_ordered_file_list *ordered_list = zend_user_module_sort_initial_op_arrays(module, &classmap, &funcmap);

	zend_hash_destroy(&classmap);
	zend_hash_destroy(&funcmap);

	for (zend_op_array **op_array_p = ordered_list->files + ordered_list->num_files - 1;
			op_array_p != ordered_list->next_file_with_stmts;
			op_array_p--) {
		if (zend_hash_exists(&EG(included_files), (*op_array_p)->filename)) {
			continue;
		}
		zend_execute(*op_array_p, NULL);
		if (EG(exception)) {
			goto cleanup;
		}
	}

	for (zend_op_array **op_array_p = ordered_list->files;
			op_array_p != ordered_list->next_file;
			op_array_p++) {
		if (zend_hash_exists(&EG(included_files), (*op_array_p)->filename)) {
			continue;
		}
		zend_execute(*op_array_p, NULL);
		if (EG(exception)) {
			goto cleanup;
		}
	}

	/* Enforce that referenced symbols exist and build class/function tables */

	HashTable class_table;
	zend_hash_init(&class_table, module->op_arrays.nNumOfElements, NULL, ZEND_CLASS_DTOR, 0);

	HashTable function_table;
	zend_hash_init(&function_table, module->op_arrays.nNumOfElements, NULL, ZEND_FUNCTION_DTOR, 0);

	if (zend_user_module_check_deps(module, &class_table, &function_table) == FAILURE) {
		ZEND_ASSERT(EG(exception));
		return;
	}

	/* Optimize */

	zend_string *orig_compiled_filename = CG(compiled_filename);
	CG(compiled_filename) = ZSTR_INIT_LITERAL("$PRELOAD$", 0);

	zend_script script = {
		.class_table = class_table,
		.function_table = function_table,
		.filename = CG(compiled_filename),
	};

#if ZEND_USE_ABS_CONST_ADDR
	init_op_array(&script.main_op_array, ZEND_USER_FUNCTION, 1);
#else
	init_op_array(&script.main_op_array, ZEND_USER_FUNCTION, 2);
#endif
	script.main_op_array.fn_flags |= ZEND_ACC_DONE_PASS_TWO;
	script.main_op_array.last = 1;
	script.main_op_array.last_literal = 1;
	script.main_op_array.T = ZEND_OBSERVER_ENABLED;
#if ZEND_USE_ABS_CONST_ADDR
	script.main_op_array.literals = (zval*)emalloc(sizeof(zval));
#else
	script.main_op_array.literals = (zval*)(script.main_op_array.opcodes + 1);
#endif
	ZVAL_NULL(script.main_op_array.literals);
	memset(script.main_op_array.opcodes, 0, sizeof(zend_op));
	script.main_op_array.opcodes[0].opcode = ZEND_RETURN;
	script.main_op_array.opcodes[0].op1_type = IS_CONST;
	script.main_op_array.opcodes[0].op1.constant = 0;
	ZEND_PASS_TWO_UPDATE_CONSTANT(&script.main_op_array, script.main_op_array.opcodes, script.main_op_array.opcodes[0].op1);
	zend_vm_set_opcode_handler(script.main_op_array.opcodes);

	script.filename = CG(compiled_filename);
	CG(compiled_filename) = orig_compiled_filename;

	zend_hash_sort_ex(&script.class_table, zend_user_module_sort_classes, NULL, 0);

	zend_user_module_optimize(module, &script);

	/* Cache module */

	zend_persistent_script *pscript = emalloc(sizeof(zend_persistent_script));
	memset(pscript, 0, sizeof(zend_persistent_script));
	pscript->script = script;

	zend_persistent_user_module *pmodule = emalloc(sizeof(zend_persistent_user_module));
	memset(pmodule, 0, sizeof(zend_persistent_user_module));

	pmodule->script = pscript;
	pmodule->module = *module;
	pmodule->dependencies = emalloc(sizeof(zend_persistent_user_module*) * CG(module_table)->nNumOfElements - orig_module_table_offset);
	zend_user_module *dep;
	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(module_table), dep, orig_module_table_offset) {
		ZEND_ASSERT(zend_accel_in_shm(dep));
		zend_persistent_user_module *pdep = (zend_persistent_user_module*)((char*)dep - XtOffsetOf(zend_persistent_user_module, module));
		pmodule->dependencies[pmodule->num_dependencies++] = pdep;
	} ZEND_HASH_FOREACH_END();

	zend_shared_alloc_init_xlat_table();

	HANDLE_BLOCK_INTERRUPTIONS();
	SHM_UNPROTECT();
	zend_shared_alloc_lock();

	pmodule = zend_user_module_save_script_in_shared_memory(pmodule);

	zend_shared_alloc_destroy_xlat_table();
	zend_shared_alloc_save_state();
	ZCSG(interned_strings).saved_top = ZCSG(interned_strings).top;
	zend_shared_alloc_unlock();
	SHM_PROTECT();
	HANDLE_UNBLOCK_INTERRUPTIONS();

	zend_hash_update_ptr(CG(module_table), pmodule->module.lcname,
			&pmodule->module);

cleanup:
	{
		zend_op_array *op_array;
		ZEND_HASH_FOREACH_PTR(&module->op_arrays, op_array) {
			destroy_op_array(op_array);
			efree_size(op_array, sizeof(zend_op_array));
		} ZEND_HASH_FOREACH_END();
		efree(ordered_list);
		zend_hash_destroy(&module->op_arrays);
		efree(module);

		zend_hash_destroy(&filenames);

		//zend_user_module_destroy(&module);
		zend_user_module_desc_destroy(&module_desc);

		CG(compiler_options_for_modules) = orig_compiler_options;
		CG(active_module) = orig_module;
	}
}

