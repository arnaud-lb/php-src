
#include "Zend/zend.h"
#include "zend_compile.h"
#include "zend_errors.h"
#include "zend_execute.h"
#include "zend_hash.h"
#include "zend_API.h"
#include "zend_ini.h"
#include "zend_ini_scanner.h"
#include "zend_operators.h"
#include "zend_smart_str.h"
#include "zend_string.h"
#include "zend_types.h"
#include "ZendAccelerator.h"
#include "zend_virtual_cwd.h"
#include "zend_vm_opcodes.h"

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

static void zend_user_module_destroy(zend_user_module *module)
{
	zend_string_release(module->name);
	zend_string_release(module->lcname);
	zend_string_release(module->path);
	zend_string_release(module->resolved_path);
	zend_hash_destroy(&module->op_arrays);
}

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
				if (module_desc->include_patterns) {
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
	if (file_handle.opened_path) {
		zend_hash_add_empty_element(&EG(included_files), file_handle.opened_path);
	}
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

		if (globbuf.gl_pathc == 0) {
			globfree(&globbuf);
			zend_error_noreturn(E_CORE_ERROR, "No files matched");
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

#if 0
typedef struct _zend_dependency_stack {
	size_t len;
	size_t capacity;
	zend_op_array **stack;
} zend_dependency_stack;
#endif

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
	size_t size;
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

	list->size = module->op_arrays.nNumOfElements;
	list->files = (zend_op_array**)((char*)list + sizeof(*list));
	list->next_file = list->files;
	list->next_file_with_stmts = list->files + list->size - 1;

	zend_op_array *op_array;
	ZEND_HASH_FOREACH_PTR(&module->op_arrays, op_array) {
		zend_user_module_dep_add_op_array(module, op_array, (zend_op_array*)module, classmap, funcmap, list);
	} ZEND_HASH_FOREACH_END();

	ZEND_ASSERT(list->next_file == list->next_file_with_stmts + 1);

	return list;
}

ZEND_API void zend_require_user_module(zend_string *module_path)
{
	zend_user_module *orig_module = CG(active_module);
	// TODO: check circular loading

	/* Load module metadata */

	zend_file_handle file_handle;
	zend_user_module_desc module_desc = {0};

	zend_stream_init_filename_ex(&file_handle, module_path);

	if (zend_parse_ini_file(&file_handle, 0, ZEND_INI_SCANNER_NORMAL, zend_user_module_desc_parser_cb, &module_desc) == FAILURE) {
		zend_destroy_file_handle(&file_handle);
		zend_user_module_desc_destroy(&module_desc);
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

	zend_user_module module = {
		.name = zend_string_copy(module_desc.name),
		.lcname = zend_string_tolower(module_desc.name),
		.path = zend_string_copy(module_desc.path),
		.resolved_path = zend_string_copy(module_desc.resolved_path),
	};
	zend_hash_init(&module.op_arrays, 0, NULL, NULL, 0);

	CG(active_module) = &module;

	/* List initial files and compile */

	HashTable filenames;
	zend_hash_init(&filenames, 0, NULL, NULL, 0);
	zend_user_module_find_files(&module_desc, &filenames);

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

	zend_long h;
	zend_string *file;
	ZEND_HASH_FOREACH_KEY(&filenames, h, file) {
		ZEND_ASSERT(h);

		uint32_t class_table_offset = CG(class_table)->nNumUsed;
		uint32_t function_table_offset = CG(function_table)->nNumUsed;

		zend_op_array *op_array = zend_user_module_compile_file(&module, file);

		zend_user_module_update_class_map(&module, op_array, &classmap, class_table_offset);
		zend_user_module_update_func_map(&module, op_array, &funcmap, function_table_offset);
	} ZEND_HASH_FOREACH_END();

	/* Execute initial files */

	zend_user_module_ordered_file_list *ordered_list = zend_user_module_sort_initial_op_arrays(&module, &classmap, &funcmap);

	for (zend_op_array **op_array_p = ordered_list->files + ordered_list->size - 1;
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

	/* Enforce that referenced symbols exist */

	// TODO

	/* Cache module */

	// TODO

cleanup:

	zend_hash_destroy(&classmap);
	zend_hash_destroy(&funcmap);

	for (size_t i = 0; i < ordered_list->size; i++) {
		zend_op_array *op_array = ordered_list->files[i];
		destroy_op_array(op_array);
		efree_size(op_array, sizeof(zend_op_array));
	}
	efree(ordered_list);

	zend_hash_destroy(&filenames);

	zend_user_module_destroy(&module);
	zend_user_module_desc_destroy(&module_desc);

	CG(compiler_options_for_modules) = orig_compiler_options;
	CG(active_module) = orig_module;

	// - If module is being compiled, throw cyclic ref
	//   if CG(modules).find(module).state == COMPILING:
	//       throw
	// - Take note of current module being compiled
	//   CG(current_module) = module
	//   CG(modules).set(module, module)
	//   module.state = COMPILING
	//
	// - Disable opcache (We are going to cache the file privately, so this would duplicate the cache)
	//   ZCG(enabled) = false;
	//   ZCG(accelerator_enabled) = false;
	//
	// - Intercept compilations. Compiling a file with a module decl != current module is an error
	//   accelerator_orig_compile_file = module_compile_file
	//   module_compile_file:
	//       compile(file)
	//       if file.module != CG(current_module):
	//           throw
	//       CG(current_module).files.set(file, op_array)
	//
	// - For each file:
	//   - Compile
	//   - Execute
	//     compile(file)
	//     - What about error handlers triggering compilations / executions?
	//     - Take note of compiled scripts, add them to current module (handled by module_compile_file)
	//
	// - Enable opcache again. Eager load will call non-module files.
	//   ZCG(enabled) = ZCG(accelerator_enabled) = true;
	//
	// - Restore any state (e.g. compile_file function, compiler flags)
	//   CG(compiler_options) = orig;
	//
	// - Set current module to none
	//   CG(current_module) = NULL;
	//
	// - For each file:
	//   - Eager load
	//     - Ignore symbols from module namespace
	//     - If eager load triggers an include of a module file, this should error (because model decl != current module)
	//     - If eager load triggers a require_module, cyclic checks will apply
	//     - Take note of required modules as dependencies
	//     foreach referenced symbols:
	//         if namespace in module:
	//             continue
	//         if !lookup_class():
	//             throw
	//
	//
	// - For each file:
	//   - Link, declare classes and functions
	//   - Add declared classes to
	//   - No-op declare ops
	//
	// - Set current module again
	//
	// - For each file:
	//   - Add to EG(included_files) (for require_once)
	//     - Code depending on cond symbols from same module should require_once
	//   - Execute (for conditional decls and requires)
	//     - Execution of files via require/autoload should:
	//       - Take note of new module files
	//         - If current module, it's ok
	//           - Eager load, link, etc
	//         - If no module, it's ok (but don't take note of the file)
	//         - If != current module, error
	//       - TODO
	//     - Intercept DECLARE ops:
	//       - Eager load, link, etc
	//
	// - Create a big zend_script for the module
	//
	// - For each file:
	//   - Copy symbols to module script, hack filenames
	//
	// - Optimize the script
	// - Cache it
	//   - Cache an entry for the module
	//   - List dependencies in the zend_script
	//   - List files in the zend_script
	//   - Add script to each of its dependencies (for invalidation)
	//   - Add files to a map of file->module (for invalidation)
	//
	// Loading a cached module:
	// - Find the zend_script
	// - For each dependency: also load it
	//
	// Invalidation:
	// - Watch all module files
	// - When a module file changes, invalidate the module and its dependents
	//
	//


	// EXECUTE-FIRST variant:
	//
	// - If module is being compiled, throw cyclic ref
	//   if CG(modules).find(module).state == COMPILING:
	//       throw
	//
	// - Take note of new module as dependency of current module:
	//   CG(current_module).deps.push(module)
	//
	// - Take note of current module being compiled
	//   CG(current_module) = module
	//   CG(modules).set(module, module)
	//   module.state = COMPILING
	//
	// - Disable opcache (We are going to cache the file privately, so this would duplicate the cache)
	//   ZCG(enabled) = false;
	//   ZCG(accelerator_enabled) = false;
	//
	// - Intercept compilations. Compiling a file with a module decl != current module is an error
	//   accelerator_orig_compile_file = module_compile_file
	//   module_compile_file(file):
	//       if file in CG(current_module).dir:
	//           ZCG(enabled) = ZCG(accelerator_enabled) = false
	//           CG(compiler_options) = ...
	//           op_array = compile(file)
	//           if op_array.module == CG(current_module):
	//              CG(current_module).files.set(file, op_array)
	//       else:
	//          ZCG(enabled) = ZCG(accelerator_enabled) = orig
	//          CG(compiler_options) = orig
	//          op_array = compile(file)
	//          if op_array.module:
	//              throw
	//
	// - CG(current_module).files = files
	// - For each CG(current_module).files (including ones added during this loop)
	//   - Compile (note the logic in module_compile_file)
	//   - Take note of class_table len
	//   - Execute
	//   - For each class in class_table[len:]:
	//     - If class in module:
	//       - CG(current_module).class.push(class)
    //     - TODO: check if we actually need this. Can h
    //   - Similar stuff for functions
	//   - Eager load
	//     - Ignore symbols from module namespace
	//     - If eager load triggers an include of a module file, this should error (because model decl != current module)
	//     - If eager load triggers a require_module, cyclic checks will apply
	//     - Take note of required modules as dependencies (handled by nested require_module() call)
	//     foreach referenced symbols:
	//         if namespace in module:
	//             continue
	//         if !lookup_class():
	//             throw
	//   - Link (or ensure linked)
	//
	// - For each file:
	//   - Link, declare classes and functions
	//   - Add declared classes to
	//   - No-op declare ops
	//
	// - Set current module again
	//
	// - For each file:
	//   - Add to EG(included_files) (for require_once)
	//     - Code depending on cond symbols from same module should require_once
	//   - Execute (for conditional decls and requires)
	//     - Execution of files via require/autoload should:
	//       - Take note of new module files
	//         - If current module, it's ok
	//           - Eager load, link, etc
	//         - If no module, it's ok (but don't take note of the file)
	//         - If != current module, error
	//       - TODO
	//     - Intercept DECLARE ops:
	//       - Eager load, link, etc
	//
	// - Create a big zend_script for the module
	//
	// - For each file:
	//   - Copy symbols to module script, hack filenames
	//
	// - Optimize the script
	// - Cache it
	//   - Cache an entry for the module
	//   - List dependencies in the zend_script (flatten deps recursively)
	//   - List files in the zend_script
	//   - Add script to each of its dependencies (for invalidation)
	//   - Add files to a map of file->module (for invalidation)
	//


	// In-module loading order:
	//
	// Preload-like: Compile files, build a class map, decorate autoloader to load from class map
	// Pros:
	// - Simple, probably
	// - Usual behavior
	// Cons:
	// - Not the fastest, probably
	//
	// Execute in topo order: Compile files, execute in topo order. Files with statements are executed last. Files that depend on symbols declared conditionally may need a require.
	// Pros:
	// - We can provide nicer messages about cyclic deps
	// Cons:
	// - Execute order is kinda unpredictable. Especially, adding a statement (e.g. a debug statement) in a file changes its order
	//
	// Mix: Execute in topo order, but also have a class map autoloader to load from class map, as a fallback
	// - Best of both, probably.

	// Questions:
	//
	// Can we execute `class A extends B {}` if B does not exist yet?
	// - No
	//
	// Would it be easy to delay linking until A is used?
	//
	//

}

