
#include "Zend/zend.h"
#include "zend_errors.h"
#include "zend_hash.h"
#include "zend_API.h"
#include "zend_ini.h"
#include "zend_ini_scanner.h"
#include "zend_operators.h"
#include "zend_smart_str.h"
#include "zend_string.h"
#include "zend_types.h"
#include "ZendAccelerator.h"

#include <glob.h>
#include <fnmatch.h>

typedef struct _zend_user_module_desc {
	zend_string *name;
	zend_string *dir;
	zend_string **include_patterns; /** NULL terminated */;
	zend_string **exclude_patterns; /** NULL terminated */;
} zend_user_module_desc;

static void zend_user_module_desc_destroy(zend_user_module_desc *module_desc)
{
	if (module_desc->name) {
		zend_string_release(module_desc->name);
	}
	if (module_desc->dir) {
		zend_string_release(module_desc->dir);
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

	ZEND_ASSERT(buf_len+1 <= buf_capacity);
	buf[buf_len] = NULL;
	return buf;
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

static void zend_user_module_compile_file(zend_string *file)
{
	zend_file_handle file_handle;
	zend_stream_init_filename(&file_handle, ZSTR_VAL(file));

	zend_op_array *op_array = zend_compile_file(&file_handle, ZEND_REQUIRE);
	if (file_handle.opened_path) {
		zend_hash_add_empty_element(&EG(included_files), file_handle.opened_path);
	}
	zend_destroy_file_handle(&file_handle);

	if (op_array) {
		zend_execute(op_array, NULL);
		zend_exception_restore();
		if (UNEXPECTED(EG(exception))) {
			return;
		}
		destroy_op_array(op_array);
		efree_size(op_array, sizeof(zend_op_array));
	} else {
		if (EG(exception)) {
			zend_exception_error(EG(exception), E_ERROR);
		}

		CG(unclean_shutdown) = true;
		ret = FAILURE;
	}

}

static zend_string **zend_user_module_find_files(zend_user_module_desc *module_desc)
{
	size_t buf_capacity = 8;
	size_t buf_len = 0;
	zend_string **buf = emalloc(sizeof(char*) * buf_capacity);

	zend_string **pattern_p = module_desc->include_patterns;

	while (*pattern_p) {
		// TODO: check path traversal
		zend_string *pattern = *pattern_p;

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
			ZEND_ASSERT(path_len >= ZSTR_LEN(module_desc->dir));

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
			const char *rel_path = path + ZSTR_LEN(module_desc->dir) + 1;
			zend_string **exclude_pattern_p = module_desc->exclude_patterns;
			while (*exclude_pattern_p) {
				zend_string *pattern = *exclude_pattern_p;
				if (!fnmatch(ZSTR_VAL(pattern), rel_path, 0)) {
					goto next;
				}
				exclude_pattern_p++;
			}

			if (buf_len + 1 == buf_capacity) {
				buf = erealloc(buf, buf_capacity * 2);
			}
			buf[buf_len++] = zend_string_init(path, path_len, 0);
next:
			n++;
		}

		globfree(&globbuf);
		pattern_p++;
	}

	buf[buf_len] = NULL;
	return buf;
}

ZEND_API void zend_require_user_module(zend_string *module_path)
{
	zend_file_handle file_handle;
	zend_user_module_desc module_desc = {0};

	zend_stream_init_filename_ex(&file_handle, module_path);

	if (zend_parse_ini_file(&file_handle, 0, ZEND_INI_SCANNER_NORMAL, zend_user_module_desc_parser_cb, &module_desc) == FAILURE) {
		zend_destroy_file_handle(&file_handle);
		zend_user_module_desc_destroy(&module_desc);
		return;
	}
	zend_destroy_file_handle(&file_handle);

	size_t module_dir_len = zend_dirname(ZSTR_VAL(module_path), ZSTR_LEN(module_path));
	module_desc.dir = zend_string_init(ZSTR_VAL(module_path), module_dir_len, 0);

	// Prefix patterns with the module dir
	if (module_desc.include_patterns) {
		zend_string **pattern_p = module_desc.include_patterns;
		while (*pattern_p) {
			*pattern_p = zend_string_concat3(
				ZSTR_VAL(module_desc.dir), ZSTR_LEN(module_desc.dir),
				"/", 1,
				ZSTR_VAL(*pattern_p), ZSTR_LEN(*pattern_p));
			pattern_p++;
		}
	}
#if 0
	if (module_desc.exclude_patterns) {
		zend_string **pattern_p = module_desc.exclude_patterns;
		while (*pattern_p) {
			*pattern_p = zend_string_concat3(
				ZSTR_VAL(module_desc.dir), ZSTR_LEN(module_desc.dir),
				"/", 1,
				ZSTR_VAL(*pattern_p), ZSTR_LEN(*pattern_p));
			pattern_p++;
		}
	}
#endif

	zend_string **files = zend_user_module_find_files(&module_desc);

	ZCG(enabled) = false;
	ZCG(accelerator_enabled) = false;

	uint32_t orig_compiler_options = CG(compiler_options);
	// CG(compiler_options) |= ZEND_COMPILE_PRELOAD;
	CG(compiler_options) |= ZEND_COMPILE_HANDLE_OP_ARRAY;
	CG(compiler_options) |= ZEND_COMPILE_DELAYED_BINDING;
	CG(compiler_options) |= ZEND_COMPILE_NO_CONSTANT_SUBSTITUTION;
	CG(compiler_options) |= ZEND_COMPILE_IGNORE_OTHER_FILES;
	CG(skip_shebang) = true;

	for (zend_string **file_p = files; *file_p; files++) {
		zend_user_module_compile_file(*file_p);
	}

	for (zend_string **file_p = files; *file_p; files++) {
		efree(*file_p);
	}
	efree(files);

	CG(compiler_options) = orig_compiler_options;

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

	zend_user_module_desc_destroy(&module_desc);
}

