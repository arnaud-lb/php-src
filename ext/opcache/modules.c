
#include "modules.h"
#include "Zend/zend.h"
#include "zend_accelerator_hash.h"
#include "zend_alloc.h"
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

#include <fcntl.h>
#include <glob.h>
#include <fnmatch.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

ZEND_API zend_never_inline ZEND_COLD zend_string *zend_autoload_stack_str(void);

#if ZTS
# error Not tested with ZTS
#endif

#define ZUM_DEBUG_LEVEL ZEND_DEBUG

#if ZUM_DEBUG_LEVEL
# define ZUM_DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
# define ZUM_DEBUG(...) do { } while(0)
#endif
#if ZUM_DEBUG_LEVEL
# define ZUM_DEBUG_VALIDATION(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
# define ZUM_DEBUG_VALIDATION(...) do { } while(0)
#endif

static void zum_desc_destroy(zend_user_module_desc *module_desc)
{
	if (module_desc->desc_path) {
		zend_string_release(module_desc->desc_path);
	}
	if (module_desc->lcname) {
		zend_string_release(module_desc->lcname);
	}
	if (module_desc->name) {
		zend_string_release(module_desc->name);
	}
	if (module_desc->root) {
		zend_string_release(module_desc->root);
	}
	if (module_desc->include_patterns) {
		zend_string **str = module_desc->include_patterns;
		zend_string **end = str + module_desc->num_include_patterns;
		for ( ; str < end; str++) {
			zend_string_release(*str);
		}
		efree(module_desc->include_patterns);
	}
	if (module_desc->exclude_patterns) {
		zend_string **str = module_desc->exclude_patterns;
		zend_string **end = str + module_desc->num_exclude_patterns;
		for ( ; str < end; str++) {
			zend_string_release(*str);
		}
		efree(module_desc->exclude_patterns);
	}
}

static void zum_optimize_exclude_pattern(zend_string *pattern)
{
	// "Test/*" -> "Test/"
	if (ZSTR_VAL(pattern)[ZSTR_LEN(pattern)-1] == '*'
			&& ZSTR_VAL(pattern)[ZSTR_LEN(pattern)-2] == '/'
			&& memchr(ZSTR_VAL(pattern), '*', ZSTR_LEN(pattern)-1) == NULL) {
		ZSTR_LEN(pattern) -= 1;
	}
}

static zend_string **zum_split_patterns(zend_string *str, size_t *num_patterns_p)
{
	size_t num_patterns = 0;
	size_t capacity = 1;
	zend_string **buf = emalloc(sizeof(zend_string*) * capacity);

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
					if (num_patterns == capacity) {
						capacity = capacity * 2;
						buf = erealloc(buf, sizeof(zend_string*) * capacity);
					}
					buf[num_patterns++] = smart_str_extract(&current);
				}
				break;
			default:
				smart_str_appendc(&current, *pos);
				break;
		}
		pos++;
	}

	if (smart_str_get_len(&current)) {
		if (num_patterns == capacity) {
			capacity = capacity + 1;
			buf = erealloc(buf, sizeof(zend_string*) * capacity);
		}
		buf[num_patterns++] = smart_str_extract(&current);
	}

	*num_patterns_p = num_patterns;
	return buf;
}

static void zum_validate_name(zend_string *name)
{
	// TODO: For now just check that there is no leading/trailing backslash
	ZEND_ASSERT(ZSTR_LEN(name));
	if (ZSTR_VAL(name)[0] == '\\' || ZSTR_VAL(name)[ZSTR_LEN(name)]-1 == '\\') {
		zend_error_noreturn(E_CORE_ERROR,
				"Module name must not start or end with '\\': '%s'",
				ZSTR_VAL(name));
	}
}

static void zum_desc_parser_cb(zval *arg1, zval *arg2, zval *arg3, int callback_type, void *module_desc_ptr)
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
				zum_validate_name(Z_STR_P(arg2));
				module_desc->name = zend_string_copy(Z_STR_P(arg2));
			} else if (zend_string_equals_cstr(Z_STR_P(arg1), "files", strlen("files"))) {
				if (module_desc->include_patterns) {
					zend_error_noreturn(E_CORE_ERROR, "Duplicated 'files' entry");
				}

				convert_to_string(arg2);
				module_desc->include_patterns = zum_split_patterns(Z_STR_P(arg2), &module_desc->num_include_patterns);
			} else if (zend_string_equals_cstr(Z_STR_P(arg1), "exclude", strlen("exclude"))) {
				if (module_desc->exclude_patterns) {
					zend_error_noreturn(E_CORE_ERROR, "Duplicated 'exclude' entry");
				}

				convert_to_string(arg2);
				module_desc->exclude_patterns = zum_split_patterns(Z_STR_P(arg2), &module_desc->num_exclude_patterns);
				zend_string **p = module_desc->exclude_patterns;
				zend_string **end = p + module_desc->num_exclude_patterns;
				while (p < end) {
					zum_optimize_exclude_pattern(*p);
					p++;
				}
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

zend_op_array *persistent_compile_file_ex(zend_file_handle *file_handle, int type, zend_persistent_script **persistent_script_p);

static zend_persistent_script *zum_compile_file(zend_user_module *module, zend_file_handle *file_handle)
{
	uint32_t num_included_files = EG(included_files).nNumUsed;

	zend_persistent_script *persistent_script = NULL;
	zend_op_array *op_array = persistent_compile_file_ex(file_handle, ZEND_REQUIRE, &persistent_script);

	if (num_included_files != EG(included_files).nNumUsed) {
		// persistent_compile_file() updates EG(included_files) in some cases
		zend_hash_discard(&EG(included_files), num_included_files);
	}

	if (!op_array) {
		ZEND_ASSERT(EG(exception));
		return NULL;
	}
	destroy_op_array(op_array);
	efree_size(op_array, sizeof(zend_op_array));

	if (!persistent_script) {
		/* For PoC purpose, we only support files that can be cached in SHM for
		 * now. */
		zend_error_noreturn(E_ERROR, "File %s could not be cached in opcache (SHM or file cache). This is unsupported in modules (yet).",
				ZSTR_VAL(file_handle->filename));
		return NULL;
	}

	op_array = &persistent_script->script.main_op_array;

	if (op_array->user_module) {
		ZEND_ASSERT(zend_string_equals(op_array->user_module, module->desc.lcname));
	} else {
		zend_error_noreturn(E_COMPILE_ERROR,
				"Module file %s was expected to declare module %s, but did not declared a module",
				ZSTR_VAL(file_handle->filename), ZSTR_VAL(module->desc.name));
	}

	return persistent_script;
}

static zend_always_inline int zum_compare_filename_keys(Bucket *f, Bucket *s)
{
	size_t len = MIN(ZSTR_LEN(f->key), ZSTR_LEN(s->key)) + 1;
	return memcmp(ZSTR_VAL(f->key), ZSTR_VAL(s->key), len);
}

static zend_always_inline bool zum_file_excluded(zend_user_module_desc *module_desc, zend_string *filename)
{
	char *rel_filename = ZSTR_VAL(filename) + ZSTR_LEN(module_desc->root);
	zend_string **exclude_pattern_p = module_desc->exclude_patterns;
	if (!exclude_pattern_p) {
		return false;
	}

	zend_string **exclude_pattern_p_end = exclude_pattern_p + module_desc->num_exclude_patterns;
	for (; exclude_pattern_p < exclude_pattern_p_end; exclude_pattern_p++) {
		if (fnmatch(ZSTR_VAL(*exclude_pattern_p), rel_filename, 0) == 0) {
			return true;
		}
	}

	return false;
}

static zend_always_inline bool zum_file_matches(zend_user_module_desc *module_desc, zend_string *filename)
{
	ZEND_ASSERT(module_desc->include_patterns && module_desc->num_include_patterns);
	ZEND_ASSERT(ZSTR_VAL(module_desc->root)[ZSTR_LEN(module_desc->root)-1] == DEFAULT_SLASH);

	char *rel_filename = ZSTR_VAL(filename) + ZSTR_LEN(module_desc->root);

	if (zum_file_excluded(module_desc, filename)) {
		return false;
	}

	zend_string **pattern_p = module_desc->include_patterns;
	zend_string **pattern_p_end = pattern_p + module_desc->num_include_patterns;
	for (; pattern_p < pattern_p_end; pattern_p++) {
		if (fnmatch(ZSTR_VAL(*pattern_p), rel_filename, 0) == 0) {
			return true;
		}
	}

	return false;
}

/* dtor_func_t */
static void zum_dir_cache_entry_dtor(zval *pDest) {
	zend_user_module_dir_cache *dc = Z_PTR_P(pDest);
	if (dc) {
		zend_hash_destroy(&dc->entries);
		efree(dc);
	}
}

#define ZUM_OPENDIR_FLAGS O_RDONLY|O_NDELAY|O_DIRECTORY|O_LARGEFILE|O_CLOEXEC

static bool zum_validate_timestamps_fd(zend_user_module_desc *module_desc,
		int dirfd, zend_user_module_dir_cache *cache)
{
	// TODO: We may want to update the cached directory timestamps, instead
	// of invalidating the module, in case the directory was changed without
	// affecting the list of files (e.g. an ignored file was added/removed).

	zend_string *name;
	zval *zv;
	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(&cache->entries, name, zv) {
		if (ZUM_IS_FILE_ENTRY(zv)) {
			accel_time_t mtime = ZUM_FILE_ENTRY_MTIME(zv);

			struct stat st;
			if (fstatat(dirfd, ZSTR_VAL(name), &st, 0) != 0) {
				ZUM_DEBUG("stat(%s) failed: %s\n", ZSTR_VAL(name), strerror(errno));
				return false;
			}

			if (st.st_mtim.tv_sec != mtime) {
				ZUM_DEBUG("Module file changed: %s\n", ZSTR_VAL(name));
				return false;
			}
		} else {
			int fd = openat(dirfd, ZSTR_VAL(name), ZUM_OPENDIR_FLAGS);
			if (fd == -1) {
				ZUM_DEBUG("opendat(%s) failed: %s\n", ZSTR_VAL(name), strerror(errno));
				return false;
			}

			struct stat st;
			if (fstat(fd, &st) != 0) {
				ZUM_DEBUG("stat(%s) failed: %s\n", ZSTR_VAL(name), strerror(errno));
				close(fd);
				continue;
			}

			if (st.st_mtim.tv_sec != ZUM_DIR_ENTRY(zv)->mtime) {
				ZUM_DEBUG("Module dir changed: %s\n", ZSTR_VAL(name));
				close(fd);
				return false;
			}

			if (!zum_validate_timestamps_fd(module_desc, fd, ZUM_DIR_ENTRY(zv))) {
				close(fd);
				return false;
			}

			close(fd);
		}
	} ZEND_HASH_FOREACH_END();

	return true;
}

static bool zum_validate_timestamps(zend_user_module_desc *module_desc,
		zend_user_module_dir_cache *cache)
{
	int fd = open(ZSTR_VAL(module_desc->root), ZUM_OPENDIR_FLAGS);
	if (fd == -1) {
		ZUM_DEBUG("Failed opening module root directory: %s: %s\n",
				ZSTR_VAL(module_desc->root), strerror(errno));
		return false;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		ZUM_DEBUG("stat(%s) failed: %s\n",
				ZSTR_VAL(module_desc->root), strerror(errno));
		close(fd);
		return false;
	}

	bool valid = zum_validate_timestamps_fd(module_desc, fd, cache);
	close(fd);

	return valid;
}

static zend_user_module_dir_cache *zum_dir_cache_new(accel_time_t mtime)
{
	zend_user_module_dir_cache *cache = emalloc(sizeof(zend_user_module_dir_cache));
	cache->mtime = mtime;
	zend_hash_init(&cache->entries, 0, NULL, zum_dir_cache_entry_dtor, 0);

	return cache;
}

/* bucket_compare_func_t: regular files, then directories, stable */
static int zum_compare_dir_entries(Bucket *a, Bucket *b)
{
	int diff = ZUM_IS_FILE_ENTRY(&b->val) - ZUM_IS_FILE_ENTRY(&a->val);
	if (diff != 0) {
		return diff;
	}

	return (uintptr_t)a - (uintptr_t)b;
}

static void zum_collect_files(zend_user_module_desc *module_desc,
		smart_str *root,
		zend_user_module_dir_cache *cache, zend_array *filenames)
{
	ZEND_ASSERT(ZSTR_VAL(module_desc->root)[ZSTR_LEN(module_desc->root)-1] == DEFAULT_SLASH);
	ZUM_DEBUG("Scanning %s\n", ZSTR_VAL(root->s));

	size_t root_len = ZSTR_LEN(root->s);

	DIR *d = opendir(ZSTR_VAL(root->s));
	if (!d) {
		ZUM_DEBUG("Failed opening %s: %s\n", ZSTR_VAL(root->s), strerror(errno));
		return;
	}

	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
			continue;
		}

		int fd = dirfd(d);
		if (fd == -1) {
			continue;
		}

		struct stat st;
		if (fstatat(fd, de->d_name, &st, 0) != 0) {
			continue;
		}

		bool is_dir;
		if (S_ISDIR(st.st_mode)) {
			is_dir = true;
		} else if (S_ISREG(st.st_mode)) {
			is_dir = false;
		} else {
			continue;
		}

		ZSTR_LEN(root->s) = root_len;
		smart_str_appends(root, de->d_name);
		if (is_dir) {
			smart_str_appendc(root, DEFAULT_SLASH);
		}
		smart_str_0(root);

		bool excluded = is_dir
			? zum_file_excluded(module_desc, root->s)
			: !zum_file_matches(module_desc, root->s);
		if (excluded) {
			continue;
		}

		zval entry;
		if (is_dir) {
			zend_user_module_dir_cache *new_cache = zum_dir_cache_new(st.st_mtim.tv_sec);
			ZVAL_PTR(&entry, new_cache);
		} else {
			ZUM_FILE_ENTRY(&entry, st.st_mtim.tv_sec);
			ZUM_DEBUG("Module file: %s\n", ZSTR_VAL(root->s));
			// Using str_add because we need to duplicate root->s
			zend_hash_str_add(filenames, ZSTR_VAL(root->s),
					ZSTR_LEN(root->s), &entry);
		}

		zend_hash_str_add(&cache->entries, de->d_name, strlen(de->d_name),
			&entry);
	}

	zend_string *name;
	zval *zv;
	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(&cache->entries, name, zv) {
		if (ZUM_IS_FILE_ENTRY(zv)) {
			continue;
		}

		ZSTR_LEN(root->s) = root_len;
		smart_str_append(root, name);
		smart_str_appendc(root, DEFAULT_SLASH);
		smart_str_0(root);

		zum_collect_files(module_desc, root, ZUM_DIR_ENTRY(zv), filenames);
	} ZEND_HASH_FOREACH_END();

	// Move regular files first, directories last, so zum_validate_timestamps
	// is breadth-first.
	zend_hash_sort(&cache->entries, zum_compare_dir_entries, false);

	closedir(d);
}

static void zum_find_files(zend_user_module_desc *module_desc, zend_user_module_dir_cache **cache_p, HashTable *filenames)
{
	/* Naive implementation of a recursive glob:
	 * - List root recursively
	 * - Filter with fnmatch() ('*' allows '/' in fnmatch, unlike glob)
	 */

	if (!module_desc->num_include_patterns) {
		ZUM_DEBUG("Module has no include patterns\n");
		return;
	}

	smart_str root = {0};
	smart_str_append(&root, module_desc->root);
	smart_str_0(&root);

	struct stat st;
	if (stat(ZSTR_VAL(root.s), &st) != 0) {
		return;
	}

	zend_user_module_dir_cache *cache = zum_dir_cache_new(st.st_mtim.tv_sec);
	*cache_p = cache;

	zum_collect_files(module_desc, &root, cache, filenames);

	smart_str_free(&root);

	zend_hash_sort(filenames, zum_compare_filename_keys, false);
}

static bool zum_op_array_has_stmts(const zend_op_array *op_array) {
	zend_op *opline = op_array->opcodes;
	zend_op *end = opline + op_array->last;

	while (opline != end) {
		switch (opline->opcode) {
			case ZEND_DECLARE_CLASS:
			case ZEND_DECLARE_CLASS_DELAYED:
			case ZEND_DECLARE_FUNCTION:
			case ZEND_RETURN:
				break;
			default:
				return true;
		}
		opline++;
	}

	return false;
}

static zend_result zum_check_deps_class_name(zend_user_module *module, zend_string *name, zend_string *lcname);

static zend_result zum_check_deps_type(zend_user_module *module, zend_type type)
{
	zend_type *single_type;
	ZEND_TYPE_FOREACH(type, single_type) {
		if (ZEND_TYPE_HAS_NAME(*single_type)) {
			if (UNEXPECTED(zum_check_deps_class_name(module, ZEND_TYPE_NAME(*single_type), NULL) == FAILURE)) {
				ZEND_ASSERT(EG(exception));
				return FAILURE;
			}
		} else if (ZEND_TYPE_HAS_LIST(*single_type)) {
			if (UNEXPECTED(zum_check_deps_type(module, *single_type) == FAILURE)) {
				ZEND_ASSERT(EG(exception));
				return FAILURE;
			}
		}
	} ZEND_TYPE_FOREACH_END();

	return SUCCESS;
}

static zend_result zum_check_deps_op_array(zend_user_module *module, zend_op_array *op_array);

static zend_result zum_check_deps_class(zend_user_module *module, zend_class_entry *ce)
{
	ZEND_ASSERT(!EG(exception));

	if (ce->type != ZEND_USER_CLASS) {
		return SUCCESS;
	}

	if (UNEXPECTED(!ce->info.user.user_module)) {
		zend_throw_error(NULL,
				"Module %s can not depend on non-module class %s",
				ZSTR_VAL(module->desc.name), ZSTR_VAL(ce->name));
		return FAILURE;
	}

	if (!zend_string_equals(ce->info.user.user_module, module->desc.lcname)) {
		ZUM_DEBUG("Module %s depends on %s\n", ZSTR_VAL(module->desc.lcname), ZSTR_VAL(ce->info.user.user_module));

		if (!(ce->ce_flags & ZEND_ACC_IMMUTABLE) && !ZCG(accel_directives).file_cache_only) {
			zend_throw_error(NULL,
					"Module %s can not depend on uncacheable class %s",
					ZSTR_VAL(module->desc.name), ZSTR_VAL(ce->name));
			return FAILURE;
		}

		zend_user_module *dep = zend_hash_find_ptr(CG(module_table), ce->info.user.user_module);
		ZEND_ASSERT(dep);
		zend_hash_add_ptr(&module->deps, ce->info.user.user_module, dep);
	}

	return SUCCESS;
}


static zend_result zum_check_deps_class_decl_dep(zend_user_module *module, zend_class_entry *ce)
{
	if (ce->type != ZEND_USER_CLASS) {
		return SUCCESS;
	}

	return zum_check_deps_class(module, ce);
}

static zend_result zum_check_deps_class_name(zend_user_module *module, zend_string *name, zend_string *lcname);

static zend_result zum_check_deps_class_decl(zend_user_module *module, zend_class_entry *ce)
{
	ZEND_ASSERT(!EG(exception));
	ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);

	if (!(ce->ce_flags & ZEND_ACC_IMMUTABLE) && !ZCG(accel_directives).file_cache_only) {
		zend_throw_error(NULL,
				"Class %s is uncacheable",
				ZSTR_VAL(ce->name));
		return FAILURE;
	}

	// TODO: may need to zum_check_deps_class_decl() for classes
	// we don't visit through op_array walk, e.g. eval'ed classes.
	if (ce->parent) {
		if (zum_check_deps_class_decl_dep(module, ce->parent) == FAILURE) {
			ZEND_ASSERT(EG(exception));
			return FAILURE;
		}
	}

	for (uint32_t i = 0; i < ce->num_interfaces; i++) {
		if (zum_check_deps_class_decl_dep(module, ce->interfaces[i]) == FAILURE) {
			ZEND_ASSERT(EG(exception));
			return FAILURE;
		}
	}

	for (uint32_t i = 0; i < ce->num_traits; i++) {
		zend_class_name *trait_name = &ce->trait_names[i];
		if (zum_check_deps_class_name(module, trait_name->name, trait_name->lc_name) == FAILURE) {
			ZEND_ASSERT(EG(exception));
			return FAILURE;
		}
	}

	zend_property_info *prop_info;
	ZEND_HASH_MAP_FOREACH_PTR(&ce->properties_info, prop_info) {
		if (UNEXPECTED(zum_check_deps_type(module, prop_info->type) == FAILURE)) {
			ZEND_ASSERT(EG(exception));
			return FAILURE;
		}
		// TODO: hooks
	} ZEND_HASH_FOREACH_END();

	zend_function *fn;
	ZEND_HASH_MAP_FOREACH_PTR(&ce->function_table, fn) {
		if (fn->type == ZEND_USER_FUNCTION && fn->op_array.scope == ce
				&& !(fn->common.fn_flags & ZEND_ACC_TRAIT_CLONE)) {
			if (UNEXPECTED(zum_check_deps_op_array(module, &fn->op_array) == FAILURE)) {
				ZEND_ASSERT(EG(exception));
				return FAILURE;
			}
		}
	} ZEND_HASH_FOREACH_END();

	return SUCCESS;
}

static zend_result zum_check_deps_class_name_ex(zend_user_module *module, zend_string *name, zend_string *lcname, uint32_t fetch_type)
{
	if (zend_string_equals_literal_ci(name, "self")) {
		return SUCCESS;
	} else if (zend_string_equals_literal_ci(name, "parent")) {
		return SUCCESS;
	} else if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_STATIC))) {
		return SUCCESS;
	}

	zend_class_entry *ce = zend_fetch_class_by_name(name, lcname, fetch_type);
	if (UNEXPECTED(!ce)) {
		if (fetch_type & ZEND_FETCH_CLASS_SILENT) {
			return SUCCESS;
		}
		ZEND_ASSERT(EG(exception));
		return FAILURE;
	}

	return zum_check_deps_class(module, ce);
}


static zend_result zum_check_deps_class_name(zend_user_module *module, zend_string *name, zend_string *lcname)
{
	return zum_check_deps_class_name_ex(module, name, lcname, 0);
}

static zend_result zum_check_deps_class_name_silent(zend_user_module *module, zend_string *name, zend_string *lcname)
{
	return zum_check_deps_class_name_ex(module, name, lcname, ZEND_FETCH_CLASS_SILENT);
}

#define CHECK_CLASS(node) \
	do { \
		uint32_t ___lineno = EG(lineno_override); \
		EG(lineno_override) = opline->lineno; \
		zum_check_deps_class_name(module, Z_STR_P(RT_CONSTANT(opline, node)), Z_STR_P(RT_CONSTANT(opline, node)+1)); \
		EG(lineno_override) = ___lineno; \
		if (EG(exception)) { \
			return FAILURE; \
		} \
	} while (0)

/* Relax restrictions for now, for class references that do not impact linking */
#define CHECK_CLASS_SILENT(node) \
	do { \
		uint32_t ___lineno = EG(lineno_override); \
		EG(lineno_override) = opline->lineno; \
		zum_check_deps_class_name_silent(module, Z_STR_P(RT_CONSTANT(opline, node)), Z_STR_P(RT_CONSTANT(opline, node)+1)); \
		EG(lineno_override) = ___lineno; \
		if (EG(exception)) { \
			return FAILURE; \
		} \
	} while (0)

static zend_result zum_check_deps_op_array(zend_user_module *module, zend_op_array *op_array)
{
	if (op_array->arg_info) {
		zend_arg_info *start = op_array->arg_info
			- (bool)(op_array->fn_flags & ZEND_ACC_HAS_RETURN_TYPE);
		zend_arg_info *end = op_array->arg_info
			+ op_array->num_args
			+ (bool)(op_array->fn_flags & ZEND_ACC_VARIADIC);
		while (start < end) {
			zum_check_deps_type(module, start->type);
			start++;
		}
	}

	zend_op *opline = op_array->opcodes;
	zend_op *end = opline + op_array->last;

	while (opline < end) {
		switch (opline->opcode) {
			case ZEND_INIT_STATIC_METHOD_CALL:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS_SILENT(opline->op1);
				}
				break;
			case ZEND_CATCH:
				CHECK_CLASS_SILENT(opline->op1);
				break;
			case ZEND_FETCH_CLASS_CONSTANT:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS_SILENT(opline->op1);
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
					CHECK_CLASS_SILENT(opline->op2);
				}
				break;
			case ZEND_INSTANCEOF:
				break;
			case ZEND_FETCH_CLASS:
				if (opline->op2_type == IS_CONST) {
					CHECK_CLASS_SILENT(opline->op2);
				}
				break;
			case ZEND_NEW:
				if (opline->op1_type == IS_CONST) {
					CHECK_CLASS_SILENT(opline->op1);
				}
				break;
			case ZEND_DECLARE_CLASS:
			case ZEND_DECLARE_CLASS_DELAYED: {
				if (op_array->function_name) {
					zend_error_noreturn(E_COMPILE_ERROR, "Can not declare named class in function body");
				}
				zend_string *lcname = Z_STR_P(RT_CONSTANT(opline, opline->op1));
				// ZUM_DEBUG("declare_class: %s\n", lcname->val);
				zend_string *rtd_key = Z_STR_P(RT_CONSTANT(opline, opline->op1) + 1);
				if (zend_hash_find_ptr(CG(class_table), rtd_key)) {
					// This declaration was not executed.
					//
					// We need to check both rtd_key and lcname for cases like
					// this:
					// class C {}
					// alias(C,D)
					// if(false) { class D{} }
					break;
				}
				zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), lcname);
				if (ce) {
					ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);
					if (UNEXPECTED(zum_check_deps_class_decl(module, ce) == FAILURE)) {
						ZEND_ASSERT(EG(exception));
						return FAILURE;
					}
					zend_hash_add_new_ptr(&module->class_table, lcname, ce);
					// ce->refcount++;
				}
				break;
			}
			case ZEND_DECLARE_ANON_CLASS: {
				zend_string *rtd_key = Z_STR_P(RT_CONSTANT(opline, opline->op1));
				zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), rtd_key);
				ZEND_ASSERT(ce);
				if (!(ce->ce_flags & ZEND_ACC_LINKED)) {
					EG(filename_override) = ce->info.user.filename;
					EG(lineno_override) = ce->info.user.line_start;
					ce = zend_do_link_class(ce, (opline->op2_type == IS_CONST) ? Z_STR_P(RT_CONSTANT(opline, opline->op2)) : NULL, rtd_key);
					if (UNEXPECTED(!ce)) {
						ZEND_ASSERT(EG(exception));
						return FAILURE;
					}
					ZEND_ASSERT(ce->ce_flags & ZEND_ACC_LINKED);
				}
				if (UNEXPECTED(zum_check_deps_class_decl(module, ce) == FAILURE)) {
					ZEND_ASSERT(EG(exception));
					return FAILURE;
				}
				if (zend_hash_add_ptr(&module->class_table, rtd_key, ce)) {
					// ce->refcount++;
				}
				break;
			}
			case ZEND_DECLARE_FUNCTION: {
				zend_string *lcname = Z_STR_P(RT_CONSTANT(opline, opline->op1));
				zend_function *fn = (zend_function*) op_array->dynamic_func_defs[opline->op2.num];
				ZEND_ASSERT(fn->type == ZEND_USER_FUNCTION);
				if (zend_hash_add_ptr(&module->function_table, lcname, fn)) {
					if (fn->op_array.refcount) {
						// (*fn->op_array.refcount)++;
					}
					if (UNEXPECTED(zum_check_deps_op_array(module, &fn->op_array) == FAILURE)) {
						ZEND_ASSERT(EG(exception));
						return FAILURE;
					}
				}
				break;
			}
		}
		opline++;
	}

	if (op_array->num_dynamic_func_defs) {
		for (uint32_t i = 0; i < op_array->num_dynamic_func_defs; i++) {
			if (UNEXPECTED(zum_check_deps_op_array(module, op_array->dynamic_func_defs[i]) == FAILURE)) {
				ZEND_ASSERT(EG(exception));
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

/* Check that every referenced symbol exists (or try to autoload it).
 * Also build the class_table / function_table of the module.
 *
 * Referenced symbols include parent classes, interfaces, type hints.
 *
 * It is important that we ignore classes and functions that are defined but
 * never declared, so that this does not error when Dependency does not exist:
 * if (class_exists(Dependency::class)) {
 *     class ... extends Dependency {
 *     }
 * }
 */
static zend_result zum_check_deps(zend_user_module *module,
		uint32_t class_table_offset, uint32_t function_table_offset)
{
	ZEND_ASSERT(!EG(exception));

	zend_string *filename_override = EG(filename_override);
	zend_long lineno_override = EG(lineno_override);
	ZEND_HASH_MAP_FOREACH_FROM_APPEND(&module->scripts, 0, 0) {
		zend_persistent_script *persistent_script = Z_PTR_P(_z);
		EG(filename_override) = persistent_script->script.filename;
		EG(lineno_override) = 0;
		zend_string *key;
		zend_class_entry *ce;
		if (UNEXPECTED(zum_check_deps_op_array(module, &persistent_script->script.main_op_array) == FAILURE)) {
			ZEND_ASSERT(EG(exception));
			goto failure;
		}
		// Opcache may elide some DECLARE opcodes, so we have to check
		// persistent_script->script.class_table as well.
		ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(&persistent_script->script.class_table, key, ce) {
			// Runtime declaration key: This is used by a DECLARE_CLASS opcode,
			// so we already handled this in
			// zum_check_deps_op_array().
			if (ZSTR_VAL(key)[0] == 0) {
				continue;
			}
			// Anon class: This is used by a DECLARE_ANON_CLASS, so we already
			// handled this in zum_check_deps_op_array(). Also,
			// checking anon classes here may check classes that are not
			// actually usable (e.g. declared in a class that were never
			// declared at runtime).
			if (ce->ce_flags & ZEND_ACC_ANON_CLASS) {
				continue;
			}
			if (!(ce->ce_flags & ZEND_ACC_LINKED)) {
				EG(filename_override) = ce->info.user.filename;
				EG(lineno_override) = ce->info.user.line_start;
				ce = zend_do_link_class(ce, NULL, key);
				if (!ce) {
					ZEND_ASSERT(EG(exception));
					goto failure;
				}
			}
			if (zend_hash_add_ptr(&module->class_table, key, ce)) {
				if (UNEXPECTED(zum_check_deps_class_decl(module, ce) == FAILURE)) {
					ZEND_ASSERT(EG(exception));
					goto failure;
				}
			}
		} ZEND_HASH_FOREACH_END();
		zend_function *function;
		ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(&persistent_script->script.function_table, key, function) {
			if (UNEXPECTED(zum_check_deps_op_array(module, &function->op_array) == FAILURE)) {
				ZEND_ASSERT(EG(exception));
				goto failure;
			}

			ZEND_ASSERT(function->type == ZEND_USER_FUNCTION);
			zend_hash_add_ptr(&module->function_table, key, function);
		} ZEND_HASH_FOREACH_END();
	} ZEND_HASH_FOREACH_END();

	EG(filename_override) = filename_override;
	EG(lineno_override) = lineno_override;
	return SUCCESS;

failure:
	EG(filename_override) = filename_override;
	EG(lineno_override) = lineno_override;
	ZEND_ASSERT(EG(exception));
	return FAILURE;
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

static zend_persistent_user_module* zum_save_in_shared_memory(zend_persistent_user_module *pmodule)
{
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
	if ((char*)pmodule->script->mem + memory_used != (char*)ZCG(mem)) {
		zend_accel_error(
			((char*)pmodule->script->mem + pmodule->script->size < (char*)ZCG(mem)) ? ACCEL_LOG_ERROR : ACCEL_LOG_WARNING,
			"Internal error: wrong size calculation: module %s start=" ZEND_ADDR_FMT ", end=" ZEND_ADDR_FMT ", real=" ZEND_ADDR_FMT "\n",
			ZSTR_VAL(pmodule->module.desc.desc_path),
			(size_t)pmodule->script->mem,
			(size_t)((char *)pmodule->script->mem + pmodule->script->size),
			(size_t)ZCG(mem));
	}

	pmodule->script->dynamic_members.memory_consumption = ZEND_ALIGNED_SIZE(pmodule->script->size);

	return pmodule;
}

static bool zum_cache_is_valid(zend_file_handle *desc_file_handle, zend_persistent_user_module *pmodule)
{
	if (!ZCG(accel_directives.validate_timestamps)) {
		return true;
	}

	struct stat st;
	if (stat(ZSTR_VAL(desc_file_handle->filename), &st) != 0) {
		ZUM_DEBUG_VALIDATION("Could not stat module descriptor: %s\n", ZSTR_VAL(desc_file_handle->filename));
		return false;
	}
	accel_time_t timestamp = st.st_mtim.tv_sec;
	if (timestamp != pmodule->module.desc.timestamp) {
		return false;
	}

	if (!zum_validate_timestamps(&pmodule->module.desc, pmodule->module.dir_cache)) {
		ZUM_DEBUG_VALIDATION("Module timestamps did not validate: %s\n", ZSTR_VAL(desc_file_handle->filename));
		return false;
	}

	return true;
}

static zend_result zum_load(zend_file_handle *desc_file_handle, zend_persistent_user_module *pmodule)
{
	ZUM_DEBUG("Loading cached module: %s\n", ZSTR_VAL(pmodule->module.desc.name));

	ZEND_ASSERT(zend_accel_in_shm(pmodule));

	// Register module early to prevent recursive loading in case of cycles
	zend_hash_add_ptr(CG(module_table), pmodule->module.desc.lcname, &pmodule->module);

	zend_user_module *dep;
	ZEND_HASH_MAP_FOREACH_PTR(&pmodule->module.deps, dep) {
		ZEND_ASSERT(zend_accel_in_shm(dep));
		zend_user_module *module = zend_hash_find_ptr(CG(module_table), dep->desc.lcname);

		if (module) {
			if (UNEXPECTED(module != dep)) {
				/* A different version of the dependency is loaded. This
				 * invalidates the module. */
				goto fail;
			}
			continue;
		}

		zend_persistent_user_module *pdep = (zend_persistent_user_module*)((char*)dep - XtOffsetOf(zend_persistent_user_module, module));

		zend_file_handle file_handle;
		zend_stream_init_filename_ex(&file_handle, pdep->module.desc.desc_path);

		if (zum_load(&file_handle, pdep) == FAILURE) {
			zend_destroy_file_handle(&file_handle);
			goto fail;
		}
		zend_destroy_file_handle(&file_handle);
	} ZEND_HASH_FOREACH_END();

	/* Validate file list and timestamps */
	if (!zum_cache_is_valid(desc_file_handle, pmodule)) {
		goto fail;
	}

	if (ZCSG(map_ptr_last) > CG(map_ptr_last)) {
		zend_map_ptr_extend(ZCSG(map_ptr_last));
	}

	/* Load into process tables */

#if 0
	if (zend_hash_num_elements(&pmodule->module.function_table) > 0) {
		if (EXPECTED(!zend_observer_function_declared_observed)) {
			zend_accel_function_hash_copy(CG(function_table), &pmodule->module.function_table);
		} else {
			zend_accel_function_hash_copy_notify(CG(function_table), &pmodule->module.function_table);
		}
	}

	if (zend_hash_num_elements(&pmodule->module.class_table) > 0) {
		if (EXPECTED(!zend_observer_class_linked_observed)) {
			zend_accel_class_hash_copy(CG(class_table), &pmodule->module.class_table);
		} else {
			zend_accel_class_hash_copy_notify(CG(class_table), &pmodule->module.class_table);
		}
	}
#else
	if (zend_hash_num_elements(&pmodule->module.function_table)) {
		Bucket *p = pmodule->module.function_table.arData;
		Bucket *end = p + pmodule->module.function_table.nNumUsed;
		HashTable *target = EG(function_table);

		zend_hash_extend(target,
			target->nNumUsed + pmodule->module.function_table.nNumUsed, 0);
		for (; p != end; p++) {
			zval *f = zend_hash_find_known_hash(target, p->key);
			if (UNEXPECTED(f != NULL)) {
				zend_error_noreturn(E_ERROR,
						"Could not load module %s: function %s declared in %s on line %d was previously declared in %s on line %d",
						ZSTR_VAL(pmodule->module.desc.name),
						ZSTR_VAL(Z_FUNC(p->val)->common.function_name),
						ZSTR_VAL(Z_FUNC(p->val)->op_array.filename),
						Z_FUNC(p->val)->op_array.line_start,
						ZSTR_VAL(Z_FUNC_P(f)->op_array.filename),
						Z_FUNC_P(f)->op_array.line_start);
			}
			_zend_hash_append_ptr_ex(target, p->key, Z_PTR(p->val), 1);
		}
	}

	if (zend_hash_num_elements(&pmodule->module.class_table)) {
		Bucket *p = pmodule->module.class_table.arData;
		Bucket *end = p + pmodule->module.class_table.nNumUsed;
		HashTable *target = EG(class_table);

		zend_hash_extend(target,
			target->nNumUsed + pmodule->module.class_table.nNumUsed, 0);
		for (; p != end; p++) {
			zval *f = zend_hash_find_known_hash(target, p->key);
			if (UNEXPECTED(f != NULL)) {
				zend_error_noreturn(E_ERROR,
						"Could not load module %s: class %s declared in %s on line %d was previously declared in %s on line %d",
						ZSTR_VAL(pmodule->module.desc.name),
						ZSTR_VAL(Z_CE(p->val)->name),
						ZSTR_VAL(Z_CE(p->val)->info.user.filename),
						Z_CE(p->val)->info.user.line_start,
						ZSTR_VAL(Z_CE_P(f)->info.user.filename),
						Z_CE_P(f)->info.user.line_start);
			}
			_zend_hash_append_ex(EG(class_table), p->key, &p->val, 1);
		}
	}
#endif

	zend_hash_add_ptr(CG(module_table), pmodule->module.desc.lcname, &pmodule->module);

	return SUCCESS;

fail:
	zend_hash_del(CG(module_table), pmodule->module.desc.lcname);

	return FAILURE;
}

static zend_result zum_execute(zend_op_array *op_array)
{
	ZEND_ASSERT(!EG(exception));

	zend_execute(op_array, NULL);
	zend_destroy_static_vars(op_array);
	destroy_op_array(op_array);
	efree_size(op_array, sizeof(zend_op_array));
	if (UNEXPECTED(EG(exception))) {
		return FAILURE;
	}

	return SUCCESS;
}

static zend_class_entry *(*zum_orig_zend_autoload)(zend_string *name, zend_string *lc_name) = NULL;

static bool zum_autoload_from_modules(zend_string *lc_name)
{
	ZUM_DEBUG("autoload: %s\n", ZSTR_VAL(lc_name));

	// Find module being loaded whose name is the longer prefix of the class
	// name
	zend_user_module *candidate;
	zend_user_module *module = NULL;
	ZEND_HASH_MAP_REVERSE_FOREACH_PTR(CG(module_table), candidate) {
		if (candidate->is_loading
				&& zend_string_starts_with(lc_name, candidate->desc.lcname)
				&& ZSTR_VAL(lc_name)[ZSTR_LEN(candidate->desc.lcname)] == '\\'
				&& (module == NULL || ZSTR_LEN(candidate->desc.lcname) > ZSTR_LEN(module->desc.lcname))) {
			module = candidate;
		}
	} ZEND_HASH_FOREACH_END();

	if (!module) {
		return false;
	}

	zend_user_module *orig_module = CG(active_module);
	CG(active_module) = module;

	zend_persistent_script *persistent_script = zend_hash_find_ptr(&module->classmap, lc_name);
	if (!persistent_script) {
		goto fail;
	}

	if (!zend_hash_add_empty_element(&EG(included_files), persistent_script->script.filename)) {
		goto fail;
	}

	zend_op_array *op_array = zend_accel_load_script(persistent_script, true);

	ZUM_DEBUG("execute: %s (autoload)\n", ZSTR_VAL(op_array->filename));
	if (zum_execute(op_array) == FAILURE || EG(exception)) {
		goto fail;
	}

	CG(active_module) = orig_module;
	return true;

fail:
	CG(active_module) = orig_module;
	return false;
}

static zend_class_entry *zum_autoload(zend_string *name, zend_string *lc_name)
{
	zend_user_module *module = CG(active_module);

	if (module) {
		if (zum_autoload_from_modules(lc_name)) {
			if (ZSTR_HAS_CE_CACHE(name) && ZSTR_GET_CE_CACHE(name)) {
				return (zend_class_entry*)ZSTR_GET_CE_CACHE(name);
			} else {
				zend_class_entry *ce = zend_hash_find_ptr(EG(class_table), lc_name);
				if (ce) {
					return ce;
				}
			}
		} else if (EG(exception)) {
			return NULL;
		}
	}

	return zum_orig_zend_autoload(name, lc_name);
}

static void zum_handle_loaded_module(zend_user_module *loaded_module, zend_string *module_path)
{
	if (loaded_module->is_loading) {
		zend_throw_error(NULL,
				"Can not require module %s while it is being loaded",
				ZSTR_VAL(loaded_module->desc.name));
	} else {
		if (!zend_string_equals(module_path, loaded_module->desc.desc_path)) {
			zend_throw_error(NULL,
					"Tried to load module %s from %s, but this module was already loaded from %s",
					ZSTR_VAL(loaded_module->desc.name),
					ZSTR_VAL(module_path),
					ZSTR_VAL(loaded_module->desc.desc_path));
		}
	}
}

static void zum_persist_modules(uint32_t module_table_offset);

ZEND_API void zend_require_user_module(zend_string *module_path)
{
	zend_user_module *orig_module = CG(active_module);
	uint32_t module_table_offset = CG(module_table)->nNumUsed;

	/* TODO: move this in MINIT */
	if (zum_orig_zend_autoload == NULL) {
		zum_orig_zend_autoload = zend_autoload;
		zend_autoload = zum_autoload;
	}

	ZUM_DEBUG("require module: %s\n", ZSTR_VAL(module_path));

	if (!ZCG(accelerator_enabled) && !ZCG(accel_directives).file_cache) {
		zend_throw_error(NULL, "require_modules() not supported when opcache is not enabled, yet");
		return;
	}

	/* Try loading from cache */

	zend_file_handle file_handle;
	zend_string *full_path = accelerator_orig_zend_resolve_path(module_path);
	if (!full_path) {
		zend_throw_error(NULL, "Could not resolve module descriptor path: %s\n",
				ZSTR_VAL(module_path));
		return;
	}
	zend_stream_init_filename_ex(&file_handle, full_path);
	zend_string_release(full_path);

	if (ZCG(accelerator_enabled)) {
		zend_string *module_key = zend_string_concat2(
				"module://", strlen("module://"),
				ZSTR_VAL(file_handle.filename), ZSTR_LEN(file_handle.filename));
		zend_persistent_user_module *cached_module = zend_accel_hash_find(&ZCSG(hash), module_key);
		zend_string_release(module_key);
		if (cached_module) {
			zend_user_module *loaded_module = zend_hash_find_ptr(EG(module_table), cached_module->module.desc.lcname);
			if (loaded_module) {
				zum_handle_loaded_module(loaded_module, file_handle.filename);
				zend_destroy_file_handle(&file_handle);
				return;
			}
			if (zum_load(&file_handle, cached_module) == SUCCESS) {
				zend_destroy_file_handle(&file_handle);
				return;
			}
		}
	}

	/* Parse module descriptor */

	ZUM_DEBUG("no valid cache for module %s\n", ZSTR_VAL(module_path));

	struct stat st;
	if (stat(ZSTR_VAL(file_handle.filename), &st) != 0) {
		zend_throw_error(NULL, "Could not stat module descriptor: %s\n", ZSTR_VAL(file_handle.filename));
		return;
	}
	zend_user_module_desc module_desc = {
		.desc_path = zend_string_copy(file_handle.filename),
		.timestamp = st.st_mtim.tv_sec,
	};

	if (zend_parse_ini_file(&file_handle, 0, ZEND_INI_SCANNER_NORMAL, zum_desc_parser_cb, &module_desc) == FAILURE) {
		zend_destroy_file_handle(&file_handle);
		zum_desc_destroy(&module_desc);
		return;
	}

	module_desc.lcname = zend_string_tolower(module_desc.name);

	/* Check if module with same namespace is already loaded */

	zend_user_module *loaded_module = zend_hash_find_ptr(EG(module_table), module_desc.lcname);
	if (loaded_module) {
		zum_handle_loaded_module(loaded_module, file_handle.filename);
		zend_destroy_file_handle(&file_handle);
		zum_desc_destroy(&module_desc);
		return;
	}

	char *dirname = estrndup(ZSTR_VAL(file_handle.opened_path), ZSTR_LEN(file_handle.opened_path));
	size_t dirname_len = zend_dirname(dirname, ZSTR_LEN(file_handle.opened_path));
	module_desc.root = zend_string_concat2(
			dirname, dirname_len,
			DEFAULT_SLASH_STR, strlen(DEFAULT_SLASH_STR));
	efree(dirname);

	zend_destroy_file_handle(&file_handle);

	zend_user_module *module = emalloc(sizeof(zend_user_module));
	module->desc = module_desc;
	module->is_loading = true;
	module->is_persistent = false;
	zend_hash_init(&module->scripts, 0, NULL, NULL, 0);
	zend_hash_init(&module->classmap, 0, NULL, NULL, 0);
	zend_hash_init(&module->class_table, 0, NULL, NULL, 0);
	zend_hash_init(&module->function_table, 0, NULL, NULL, 0);
	zend_hash_init(&module->deps, 0, NULL, NULL, 0);

	CG(active_module) = module;

	zend_hash_update_ptr(EG(module_table), module->desc.lcname, module);

	/* List files */

	zend_user_module_dir_cache *dir_cache = NULL;
	HashTable filenames;
	zend_hash_init(&filenames, 0, NULL, NULL, 0);
	zum_find_files(&module_desc, &dir_cache, &filenames);
	if (filenames.nNumOfElements == 0) {
		zend_error_noreturn(E_CORE_ERROR, "No files matched");
	}

	/* Compile files to build the classmap */

	uint32_t class_table_offset = CG(class_table)->nNumUsed;
	uint32_t function_table_offset = CG(function_table)->nNumUsed;

	zend_string *file;
	zval *val;
	ZEND_HASH_FOREACH_STR_KEY_VAL(&filenames, file, val) {
		uint32_t file_class_table_offset = CG(class_table)->nNumUsed;

		zend_file_handle file_handle;
		zend_stream_init_filename(&file_handle, ZSTR_VAL(file));

		zend_persistent_script *persistent_script = zum_compile_file(module, &file_handle);
		if (!persistent_script) {
			zend_destroy_file_handle(&file_handle);
			return;
		}

		ZVAL_PTR(val, persistent_script);

		zend_destroy_file_handle(&file_handle);

		zend_class_entry *ce;
		ZEND_HASH_FOREACH_PTR_FROM(CG(class_table), ce, file_class_table_offset) {
			ZEND_ASSERT(zend_string_equals(ce->info.user.user_module, module->desc.lcname));
			ZEND_ASSERT(ce->type == ZEND_USER_CLASS);
			zend_string *lcname = zend_string_tolower(ce->name);
			zend_hash_add_ptr(&module->classmap, lcname, persistent_script);
			zend_string_release(lcname);
		} ZEND_HASH_FOREACH_END();
	} ZEND_HASH_FOREACH_END();

	// Revert any compile-time declarations
	zend_class_entry *ce;
	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(class_table), ce, class_table_offset) {
		zend_string *name = ce->name;
		if (ZSTR_HAS_CE_CACHE(name)) {
			ZSTR_SET_CE_CACHE(name, NULL);
		}
	} ZEND_HASH_FOREACH_END();
	zend_hash_discard(EG(class_table), class_table_offset);
	zend_hash_discard(EG(function_table), function_table_offset);

	/* Execute files */

	zend_hash_init(&module->class_table, module->classmap.nNumOfElements, NULL, ZEND_CLASS_DTOR, 0);
	zend_hash_init(&module->function_table, 0, NULL, ZEND_FUNCTION_DTOR, 0);

	/* If we were called from autoloading, forget it because we may need to
	 * trigger autoloading for that class again */
	if (EG(in_autoload)) {
		HashPosition pos;
		zval key;
		zend_hash_internal_pointer_end_ex(EG(in_autoload), &pos);
		zend_hash_get_current_key_zval_ex(EG(in_autoload), &key, &pos);
		if (Z_TYPE(key) == IS_STRING) {
			zend_string *lc_name = Z_STR(key);
			if (zend_string_starts_with(lc_name, module->desc.lcname)
					&& ZSTR_VAL(lc_name)[ZSTR_LEN(module->desc.lcname)] == '\\') {
				zend_hash_del(EG(in_autoload), lc_name);
				// zum_autoload_internal(lc_name);
			}
			zend_string_release(Z_STR(key));
		}
	}

	zend_persistent_script *persistent_script;

	ZEND_HASH_FOREACH_PTR(&filenames, persistent_script) {
		if (!persistent_script) { // TODO: is this possible?
			continue;
		}
		if (!zum_op_array_has_stmts(&persistent_script->script.main_op_array)) {
			continue;
		}
		if (!zend_hash_add_empty_element(&EG(included_files), persistent_script->script.filename)) {
			continue;
		}
		ZUM_DEBUG("execute: %s\n", ZSTR_VAL(persistent_script->script.filename));
		zend_op_array *op_array = zend_accel_load_script(persistent_script, true);
		if (zum_execute(op_array) == FAILURE) {
			goto cleanup;
		}
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_FOREACH_PTR(&filenames, persistent_script) {
		if (!persistent_script) { // TODO: is this possible?
			continue;
		}
		if (!zend_hash_add_empty_element(&EG(included_files), persistent_script->script.filename)) {
			continue;
		}
		ZUM_DEBUG("execute: %s\n", ZSTR_VAL(persistent_script->script.filename));
		zend_op_array *op_array = zend_accel_load_script(persistent_script, true);
		if (zum_execute(op_array) == FAILURE) {
			goto cleanup;
		}
	} ZEND_HASH_FOREACH_END();

	/* Enforce that referenced symbols exist and build class/function tables */

	if (zum_check_deps(module, class_table_offset, function_table_offset) == FAILURE) {
		ZEND_ASSERT(EG(exception));
		goto cleanup;
	}

cleanup:
	{
		zend_hash_destroy(&module->scripts);
		zend_hash_destroy(&module->classmap);

		zend_hash_destroy(&filenames);

		CG(active_module) = orig_module;
		module->is_loading = false;
		module->dir_cache = dir_cache;

		if (EG(exception)) {
			zum_desc_destroy(&module_desc);
			zend_hash_del(EG(module_table), module->desc.lcname);
			efree(module);
		} else if (!orig_module) {
			zum_persist_modules(module_table_offset);
		}
	}
}

static zend_persistent_user_module *zum_build_persistent_module(zend_user_module *module)
{
	ZUM_DEBUG("Build persistent module %s\n", ZSTR_VAL(module->desc.name));

	ZEND_ASSERT(!module->is_loading);
	ZEND_ASSERT(!zend_accel_in_shm(module));

	zend_string *orig_compiled_filename = CG(compiled_filename);
	CG(compiled_filename) = zend_strpprintf(0, "module://%s", ZSTR_VAL(module->desc.desc_path));

	zend_persistent_script *pscript = emalloc(sizeof(zend_persistent_script));
	memset(pscript, 0, sizeof(zend_persistent_script));

	zend_hash_init(&pscript->script.class_table, 0, NULL, NULL, 0);
	zend_hash_init(&pscript->script.function_table, 0, NULL, NULL, 0);
	pscript->script.filename = CG(compiled_filename);

	zend_script *script = &pscript->script;

#if ZEND_USE_ABS_CONST_ADDR
	init_op_array(&script->main_op_array, ZEND_USER_FUNCTION, 1);
#else
	init_op_array(&script->main_op_array, ZEND_USER_FUNCTION, 2);
#endif
	script->main_op_array.fn_flags |= ZEND_ACC_DONE_PASS_TWO;
	script->main_op_array.last = 1;
	script->main_op_array.last_literal = 1;
	script->main_op_array.T = ZEND_OBSERVER_ENABLED;
#if ZEND_USE_ABS_CONST_ADDR
	script->main_op_array.literals = (zval*)emalloc(sizeof(zval));
#else
	script->main_op_array.literals = (zval*)(script->main_op_array.opcodes + 1);
#endif
	ZVAL_NULL(script->main_op_array.literals);
	memset(script->main_op_array.opcodes, 0, sizeof(zend_op));
	script->main_op_array.opcodes[0].opcode = ZEND_RETURN;
	script->main_op_array.opcodes[0].op1_type = IS_CONST;
	script->main_op_array.opcodes[0].op1.constant = 0;
	ZEND_PASS_TWO_UPDATE_CONSTANT(&script->main_op_array, script->main_op_array.opcodes, script->main_op_array.opcodes[0].op1);
	zend_vm_set_opcode_handler(script->main_op_array.opcodes);

	zend_persistent_user_module *pmodule = emalloc(sizeof(zend_persistent_user_module));
	memset(pmodule, 0, sizeof(zend_persistent_user_module));

	pmodule->script = pscript;
	pmodule->module = *module;
	pmodule->module.is_persistent = true;

	CG(compiled_filename) = orig_compiled_filename;

	zend_shared_alloc_register_xlat_entry(module, pmodule);

	return pmodule;
}

static void zum_persist_modules(uint32_t module_table_offset)
{
	ZEND_ASSERT(!CG(active_module));

	if (ZCG(accel_directives).file_cache_only) {
		// TODO: leaks
		return;
	}

	zend_shared_alloc_init_xlat_table();

	/* Replace zend_user_modules by zend_persistent_user_modules
	 * in module_table and module dependencies. */

	zval *zv;
	ZEND_HASH_MAP_FOREACH_VAL_FROM(CG(module_table), zv, module_table_offset) {
		zend_user_module *module = Z_PTR_P(zv);
		if (zend_accel_in_shm(module)) {
			continue;
		}
		zend_persistent_user_module *pmodule = zum_build_persistent_module(module);
		Z_PTR_P(zv) = &pmodule->module;
		efree(module);
	} ZEND_HASH_FOREACH_END();

	zend_user_module *module;
	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(module_table), module, module_table_offset) {
		if (zend_accel_in_shm(module)) {
			continue;
		}
		ZEND_HASH_MAP_FOREACH_VAL(&module->deps, zv) {
			zend_user_module *dep = Z_PTR_P(zv);
			zend_persistent_user_module *pdep = zend_shared_alloc_get_xlat_entry(dep);
			if (!pdep) {
				ZEND_ASSERT(zend_accel_in_shm(dep));
				continue;
			}
			Z_PTR_P(zv) = &pdep->module;
		} ZEND_HASH_FOREACH_END();
	} ZEND_HASH_FOREACH_END();

	zend_shared_alloc_clear_xlat_table();

	HANDLE_BLOCK_INTERRUPTIONS();
	SHM_UNPROTECT();
	zend_shared_alloc_lock();

	ZEND_HASH_MAP_FOREACH_VAL_FROM(CG(module_table), zv, module_table_offset) {
		zend_user_module *module = Z_PTR_P(zv);
		zend_persistent_user_module *pmodule = (zend_persistent_user_module*)((char*)module - XtOffsetOf(zend_persistent_user_module, module));
		zend_persistent_user_module *persisted_pmodule = zend_shared_alloc_get_xlat_entry(pmodule);
		if (!persisted_pmodule) {
			if (zend_accel_in_shm(pmodule)) {
				continue;
			}
			persisted_pmodule = zum_save_in_shared_memory(pmodule);
		}
		Z_PTR_P(zv) = &persisted_pmodule->module;
		zend_shared_alloc_register_xlat_entry(persisted_pmodule, persisted_pmodule);
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(module_table), module, module_table_offset) {
		zend_persistent_user_module *pmodule = (zend_persistent_user_module*)((char*)module - XtOffsetOf(zend_persistent_user_module, module));
		if (!zend_shared_alloc_get_xlat_entry(pmodule)) {
			continue;
		}

		zend_string *key = accel_new_interned_string(zend_string_concat2(
					"module://", strlen("module://"),
					ZSTR_VAL(pmodule->module.desc.desc_path), ZSTR_LEN(pmodule->module.desc.desc_path)));
		if (!ZSTR_IS_INTERNED(key)) {
			// TODO
			zend_error_noreturn(E_CORE_ERROR, "Could not intern module key (increase opcache.interned_strings_buffer size)");
		}

		zend_accel_hash_entry *bucket = zend_accel_hash_update(&ZCSG(hash), key, 0, pmodule);
		zend_string_release(key);
		if (bucket) {
			zend_accel_error(ACCEL_LOG_INFO, "Cached module '%s'", ZSTR_VAL(pmodule->module.desc.desc_path));
		}
	} ZEND_HASH_FOREACH_END();

	zend_shared_alloc_save_state();
	ZCSG(interned_strings).saved_top = ZCSG(interned_strings).top;
	zend_shared_alloc_unlock();
	SHM_PROTECT();
	HANDLE_UNBLOCK_INTERRUPTIONS();

	zend_shared_alloc_destroy_xlat_table();
}
