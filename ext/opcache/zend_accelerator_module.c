/*
   +----------------------------------------------------------------------+
   | Zend OPcache                                                         |
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
   | Authors: Andi Gutmans <andi@php.net>                                 |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Stanislav Malyshev <stas@zend.com>                          |
   |          Dmitry Stogov <dmitry@php.net>                              |
   +----------------------------------------------------------------------+
*/

#include <time.h>

#include "php.h"
#include "ZendAccelerator.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_alloc_sizes.h"
#include "zend_closures.h"
#include "zend_compile.h"
#include "zend_constants.h"
#include "zend_hash.h"
#include "zend_objects_API.h"
#include "zend_portability.h"
#include "zend_shared_alloc.h"
#include "zend_accelerator_blacklist.h"
#include "php_ini.h"
#include "SAPI.h"
#include "zend_smart_str.h"
#include "zend_snapshot.h"
#include "zend_string.h"
#include "zend_types.h"
#include "zend_virtual_cwd.h"
#include "ext/standard/info.h"
#include "ext/standard/php_filestat.h"
#include "ext/date/php_date.h"
#include "opcache_arginfo.h"
#include "zend_weakrefs.h"
#include "zend_iterators.h"
// TODO: do not depend on these directly
#include "ext/spl/php_spl.h"
#include "ext/standard/basic_functions.h"
#include "ext/date/php_date.h"

#ifdef HAVE_JIT
#include "jit/zend_jit.h"
#endif

#define STRING_NOT_NULL(s) (NULL == (s)?"":s)
#define MIN_ACCEL_FILES 200
#define MAX_ACCEL_FILES 1000000
/* Max value of opcache.interned_strings_buffer */
#define MAX_INTERNED_STRINGS_BUFFER_SIZE ((zend_long)MIN( \
	MIN( \
		/* STRTAB_STR_TO_POS() must not overflow (zend_string_table_pos_t) */ \
		(ZEND_STRING_TABLE_POS_MAX - sizeof(zend_string_table)) / (1024 * 1024 / ZEND_STRING_TABLE_POS_ALIGNMENT), \
		/* nTableMask must not overflow (uint32_t) */ \
		UINT32_MAX / (32 * 1024 * sizeof(zend_string_table_pos_t)) \
	), \
	/* SHM allocation must not overflow (size_t) */ \
	(SIZE_MAX - sizeof(zend_accel_shared_globals)) / (1024 * 1024) \
))
#define TOKENTOSTR(X) #X

static zif_handler orig_file_exists = NULL;
static zif_handler orig_is_file = NULL;
static zif_handler orig_is_readable = NULL;

static int validate_api_restriction(void)
{
	if (ZCG(accel_directives).restrict_api && *ZCG(accel_directives).restrict_api) {
		size_t len = strlen(ZCG(accel_directives).restrict_api);

		if (!SG(request_info).path_translated ||
		    strlen(SG(request_info).path_translated) < len ||
		    memcmp(SG(request_info).path_translated, ZCG(accel_directives).restrict_api, len) != 0) {
			zend_error(E_WARNING, ACCELERATOR_PRODUCT_NAME " API is restricted by \"restrict_api\" configuration directive");
			return 0;
		}
	}
	return 1;
}

static ZEND_INI_MH(OnUpdateMemoryConsumption)
{
	zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
	zend_long memsize = atoi(ZSTR_VAL(new_value));
	/* sanity check we must use at least 8 MB */
	if (memsize < 8) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.memory_consumption is set below the required 8MB.\n");
		return FAILURE;
	}
	if (UNEXPECTED(memsize > ZEND_LONG_MAX / (1024 * 1024))) {
		*p = ZEND_LONG_MAX & ~(1024 * 1024 - 1);
	} else {
		*p = memsize * (1024 * 1024);
	}
	return SUCCESS;
}

static ZEND_INI_MH(OnUpdateInternedStringsBuffer)
{
	zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
	zend_long size = zend_ini_parse_quantity_warn(new_value, entry->name);

	if (size < 0) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.interned_strings_buffer must be greater than or equal to 0, " ZEND_LONG_FMT " given.\n", size);
		return FAILURE;
	}
	if (size > MAX_INTERNED_STRINGS_BUFFER_SIZE) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.interned_strings_buffer must be less than or equal to " ZEND_LONG_FMT ", " ZEND_LONG_FMT " given.\n", MAX_INTERNED_STRINGS_BUFFER_SIZE, size);
		return FAILURE;
	}

	*p = size;

	return SUCCESS;
}

static ZEND_INI_MH(OnUpdateMaxAcceleratedFiles)
{
	zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
	zend_long size = atoi(ZSTR_VAL(new_value));
	/* sanity check we must use a value between MIN_ACCEL_FILES and MAX_ACCEL_FILES */
	if (size < MIN_ACCEL_FILES) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.max_accelerated_files is set below the required minimum (%d).\n", MIN_ACCEL_FILES);
		return FAILURE;
	}
	if (size > MAX_ACCEL_FILES) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.max_accelerated_files is set above the limit (%d).\n", MAX_ACCEL_FILES);
		return FAILURE;
	}
	*p = size;
	return SUCCESS;
}

static ZEND_INI_MH(OnUpdateMaxWastedPercentage)
{
	double *p = (double *) ZEND_INI_GET_ADDR();
	zend_long percentage = atoi(ZSTR_VAL(new_value));

	if (percentage <= 0 || percentage > 50) {
		zend_accel_error(ACCEL_LOG_WARNING, "opcache.max_wasted_percentage must be set between 1 and 50.\n");
		return FAILURE;
	}
	*p = (double)percentage / 100.0;
	return SUCCESS;
}

static ZEND_INI_MH(OnEnable)
{
	if (stage == ZEND_INI_STAGE_STARTUP ||
	    stage == ZEND_INI_STAGE_SHUTDOWN ||
	    stage == ZEND_INI_STAGE_DEACTIVATE) {
		return OnUpdateBool(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
	} else {
		/* It may be only temporary disabled */
		bool *p = (bool *) ZEND_INI_GET_ADDR();
		if (zend_ini_parse_bool(new_value)) {
			zend_error(E_WARNING, ACCELERATOR_PRODUCT_NAME " can't be temporary enabled (it may be only disabled till the end of request)");
			return FAILURE;
		} else {
			*p = 0;
			ZCG(accelerator_enabled) = 0;
			return SUCCESS;
		}
	}
}

static ZEND_INI_MH(OnUpdateFileCache)
{
	if (new_value) {
		if (!ZSTR_LEN(new_value)) {
			new_value = NULL;
		}
	}
	OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
	return SUCCESS;
}

#ifdef HAVE_JIT
static ZEND_INI_MH(OnUpdateJit)
{
	if (zend_jit_config(new_value, stage) == SUCCESS) {
		return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
	}
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateJitDebug)
{
	zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);

	if (zend_jit_debug_config(*p, val, stage) == SUCCESS) {
		*p = val;
		return SUCCESS;
	}
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateCounter)
{
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);
	if (val >= 0 && val < 256) {
		zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
		*p = val;
		return SUCCESS;
	}
	zend_error(E_WARNING, "Invalid \"%s\" setting; using default value instead. Should be between 0 and 255", ZSTR_VAL(entry->name));
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateUnrollC)
{
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);
	if (val > 0 && val < ZEND_JIT_TRACE_MAX_CALL_DEPTH) {
		zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
		*p = val;
		return SUCCESS;
	}
	zend_error(E_WARNING, "Invalid \"%s\" setting. Should be between 1 and %d", ZSTR_VAL(entry->name),
		ZEND_JIT_TRACE_MAX_CALL_DEPTH);
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateUnrollR)
{
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);
	if (val >= 0 && val < ZEND_JIT_TRACE_MAX_RET_DEPTH) {
		zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
		*p = val;
		return SUCCESS;
	}
	zend_error(E_WARNING, "Invalid \"%s\" setting. Should be between 0 and %d", ZSTR_VAL(entry->name),
		ZEND_JIT_TRACE_MAX_RET_DEPTH);
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateUnrollL)
{
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);
	if (val > 0 && val < ZEND_JIT_TRACE_MAX_LOOPS_UNROLL) {
		zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
		*p = val;
		return SUCCESS;
	}
	zend_error(E_WARNING, "Invalid \"%s\" setting. Should be between 1 and %d", ZSTR_VAL(entry->name),
		ZEND_JIT_TRACE_MAX_LOOPS_UNROLL);
	return FAILURE;
}

static ZEND_INI_MH(OnUpdateMaxTraceLength)
{
	zend_long val = zend_ini_parse_quantity_warn(new_value, entry->name);
	if (val > 3 && val <= ZEND_JIT_TRACE_MAX_LENGTH) {
		zend_long *p = (zend_long *) ZEND_INI_GET_ADDR();
		*p = val;
		return SUCCESS;
	}
	zend_error(E_WARNING, "Invalid \"%s\" setting. Should be between 4 and %d", ZSTR_VAL(entry->name),
		ZEND_JIT_TRACE_MAX_LENGTH);
	return FAILURE;
}
#endif

ZEND_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("opcache.enable"             , "1", PHP_INI_ALL,    OnEnable,     enabled                             , zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.use_cwd"            , "1", PHP_INI_SYSTEM, OnUpdateBool, accel_directives.use_cwd            , zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.validate_timestamps", "1", PHP_INI_ALL   , OnUpdateBool, accel_directives.validate_timestamps, zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.validate_permission", "0", PHP_INI_SYSTEM, OnUpdateBool, accel_directives.validate_permission, zend_accel_globals, accel_globals)
#ifndef ZEND_WIN32
	STD_PHP_INI_BOOLEAN("opcache.validate_root"      , "0", PHP_INI_SYSTEM, OnUpdateBool, accel_directives.validate_root      , zend_accel_globals, accel_globals)
#endif
	STD_PHP_INI_BOOLEAN("opcache.dups_fix"           , "0", PHP_INI_ALL   , OnUpdateBool, accel_directives.ignore_dups        , zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.revalidate_path"    , "0", PHP_INI_ALL   , OnUpdateBool, accel_directives.revalidate_path    , zend_accel_globals, accel_globals)

	STD_PHP_INI_ENTRY("opcache.log_verbosity_level"   , "1"   , PHP_INI_SYSTEM, OnUpdateLong, accel_directives.log_verbosity_level,       zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.memory_consumption"    , "128"  , PHP_INI_SYSTEM, OnUpdateMemoryConsumption,    accel_directives.memory_consumption,        zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.interned_strings_buffer", "8"  , PHP_INI_SYSTEM, OnUpdateInternedStringsBuffer,	 accel_directives.interned_strings_buffer,   zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.max_accelerated_files" , "10000", PHP_INI_SYSTEM, OnUpdateMaxAcceleratedFiles,	 accel_directives.max_accelerated_files,     zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.max_wasted_percentage" , "5"   , PHP_INI_SYSTEM, OnUpdateMaxWastedPercentage,	 accel_directives.max_wasted_percentage,     zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.force_restart_timeout" , "180" , PHP_INI_SYSTEM, OnUpdateLong,	             accel_directives.force_restart_timeout,     zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.revalidate_freq"       , "2"   , PHP_INI_ALL   , OnUpdateLong,	             accel_directives.revalidate_freq,           zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.file_update_protection", "2"   , PHP_INI_ALL   , OnUpdateLong,                accel_directives.file_update_protection,    zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.preferred_memory_model", ""    , PHP_INI_SYSTEM, OnUpdateStringUnempty,       accel_directives.memory_model,              zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.blacklist_filename"    , ""    , PHP_INI_SYSTEM, OnUpdateString,	             accel_directives.user_blacklist_filename,   zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.max_file_size"         , "0"   , PHP_INI_SYSTEM, OnUpdateLong,	             accel_directives.max_file_size,             zend_accel_globals, accel_globals)

	STD_PHP_INI_BOOLEAN("opcache.protect_memory"        , "0"  , PHP_INI_SYSTEM, OnUpdateBool,                  accel_directives.protect_memory,            zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.save_comments"         , "1"  , PHP_INI_SYSTEM, OnUpdateBool,                  accel_directives.save_comments,             zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.record_warnings"       , "0"  , PHP_INI_SYSTEM, OnUpdateBool,                  accel_directives.record_warnings,           zend_accel_globals, accel_globals)

	STD_PHP_INI_ENTRY("opcache.optimization_level"    , DEFAULT_OPTIMIZATION_LEVEL , PHP_INI_SYSTEM, OnUpdateLong, accel_directives.optimization_level,   zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.opt_debug_level"       , "0"      , PHP_INI_SYSTEM, OnUpdateLong,             accel_directives.opt_debug_level,            zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.enable_file_override"	, "0"   , PHP_INI_SYSTEM, OnUpdateBool,              accel_directives.file_override_enabled,     zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.enable_cli"             , "0"   , PHP_INI_SYSTEM, OnUpdateBool,              accel_directives.enable_cli,                zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.error_log"                , ""    , PHP_INI_SYSTEM, OnUpdateString,	         accel_directives.error_log,                 zend_accel_globals, accel_globals)
	STD_PHP_INI_ENTRY("opcache.restrict_api"             , ""    , PHP_INI_SYSTEM, OnUpdateString,	         accel_directives.restrict_api,              zend_accel_globals, accel_globals)

#ifndef ZEND_WIN32
	STD_PHP_INI_ENTRY("opcache.lockfile_path"             , "/tmp"    , PHP_INI_SYSTEM, OnUpdateString,           accel_directives.lockfile_path,              zend_accel_globals, accel_globals)
#else
	STD_PHP_INI_ENTRY("opcache.mmap_base", NULL, PHP_INI_SYSTEM,	OnUpdateString,	                             accel_directives.mmap_base,                 zend_accel_globals, accel_globals)
#endif

	STD_PHP_INI_ENTRY("opcache.file_cache"                    , NULL  , PHP_INI_SYSTEM, OnUpdateFileCache, accel_directives.file_cache,                    zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.file_cache_read_only"          , "0"   , PHP_INI_SYSTEM, OnUpdateBool,    accel_directives.file_cache_read_only,          zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.file_cache_only"               , "0"   , PHP_INI_SYSTEM, OnUpdateBool,	   accel_directives.file_cache_only,               zend_accel_globals, accel_globals)
	STD_PHP_INI_BOOLEAN("opcache.file_cache_consistency_checks" , "1"   , PHP_INI_SYSTEM, OnUpdateBool,	   accel_directives.file_cache_consistency_checks, zend_accel_globals, accel_globals)
#if ENABLE_FILE_CACHE_FALLBACK
	STD_PHP_INI_BOOLEAN("opcache.file_cache_fallback"           , "1"   , PHP_INI_SYSTEM, OnUpdateBool,	   accel_directives.file_cache_fallback,           zend_accel_globals, accel_globals)
#endif
#ifdef HAVE_HUGE_CODE_PAGES
	STD_PHP_INI_BOOLEAN("opcache.huge_code_pages"             , "0"   , PHP_INI_SYSTEM, OnUpdateBool,      accel_directives.huge_code_pages,               zend_accel_globals, accel_globals)
#endif
	STD_PHP_INI_ENTRY("opcache.preload"                       , ""    , PHP_INI_SYSTEM, OnUpdateStringUnempty,    accel_directives.preload,                zend_accel_globals, accel_globals)
#ifndef ZEND_WIN32
	STD_PHP_INI_ENTRY("opcache.preload_user"                  , ""    , PHP_INI_SYSTEM, OnUpdateStringUnempty,    accel_directives.preload_user,           zend_accel_globals, accel_globals)
#endif
#ifdef ZEND_WIN32
	STD_PHP_INI_ENTRY("opcache.cache_id"                      , ""    , PHP_INI_SYSTEM, OnUpdateString,           accel_directives.cache_id,               zend_accel_globals, accel_globals)
#endif
#ifdef HAVE_JIT
	STD_PHP_INI_ENTRY("opcache.jit"                           , "disable",                    PHP_INI_ALL,    OnUpdateJit,      options,               zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_buffer_size"               , ZEND_JIT_DEFAULT_BUFFER_SIZE, PHP_INI_SYSTEM, OnUpdateLong,     buffer_size,           zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_debug"                     , "0",                          PHP_INI_ALL,    OnUpdateJitDebug, debug,                 zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_bisect_limit"              , "0",                          PHP_INI_ALL,    OnUpdateLong,     bisect_limit,          zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_prof_threshold"            , "0.005",                      PHP_INI_ALL,    OnUpdateReal,     prof_threshold,        zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_root_traces"           , "1024",                       PHP_INI_SYSTEM, OnUpdateLong,     max_root_traces,       zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_side_traces"           , "128",                        PHP_INI_SYSTEM, OnUpdateLong,     max_side_traces,       zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_exit_counters"         , "8192",                       PHP_INI_SYSTEM, OnUpdateLong,     max_exit_counters,     zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_hot_loop"                  , "64",                         PHP_INI_SYSTEM, OnUpdateCounter,  hot_loop,              zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_hot_func"                  , "127",                        PHP_INI_SYSTEM, OnUpdateCounter,  hot_func,              zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_hot_return"                , "8",                          PHP_INI_SYSTEM, OnUpdateCounter,  hot_return,            zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_hot_side_exit"             , "8",                          PHP_INI_ALL,    OnUpdateCounter,  hot_side_exit,         zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_blacklist_root_trace"      , "16",                         PHP_INI_ALL,    OnUpdateCounter,  blacklist_root_trace,  zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_blacklist_side_trace"      , "8",                          PHP_INI_ALL,    OnUpdateCounter,  blacklist_side_trace,  zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_loop_unrolls"          , "8",                          PHP_INI_ALL,    OnUpdateUnrollL,  max_loop_unrolls,      zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_recursive_calls"       , "2",                          PHP_INI_ALL,    OnUpdateUnrollC,  max_recursive_calls,   zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_recursive_returns"     , "2",                          PHP_INI_ALL,    OnUpdateUnrollR,  max_recursive_returns, zend_jit_globals, jit_globals)
	STD_PHP_INI_ENTRY("opcache.jit_max_polymorphic_calls"     , "2",                          PHP_INI_ALL,    OnUpdateLong,     max_polymorphic_calls, zend_jit_globals, jit_globals)
    STD_PHP_INI_ENTRY("opcache.jit_max_trace_length"          , "1024",                       PHP_INI_ALL,    OnUpdateMaxTraceLength, max_trace_length, zend_jit_globals, jit_globals)
#endif
ZEND_INI_END()

static int filename_is_in_cache(zend_string *filename)
{
	zend_string *key;

	key = accel_make_persistent_key(filename);
	if (key != NULL) {
		zend_persistent_script *persistent_script = zend_accel_hash_find(&ZCSG(hash), key);
		if (persistent_script && !persistent_script->corrupted) {
			if (ZCG(accel_directives).validate_timestamps) {
				zend_file_handle handle;
				int ret;

				zend_stream_init_filename_ex(&handle, filename);
				ret = validate_timestamp_and_record_ex(persistent_script, &handle) == SUCCESS
					? 1 : 0;
				zend_destroy_file_handle(&handle);
				return ret;
			}

			return 1;
		}
	}

	return 0;
}

static int accel_file_in_cache(INTERNAL_FUNCTION_PARAMETERS)
{
	if (ZEND_NUM_ARGS() == 1) {
		zval *zv = ZEND_CALL_ARG(execute_data , 1);

		if (Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) != 0) {
			return filename_is_in_cache(Z_STR_P(zv));
		}
	}
	return 0;
}

static ZEND_NAMED_FUNCTION(accel_file_exists)
{
	if (accel_file_in_cache(INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
		RETURN_TRUE;
	} else {
		orig_file_exists(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}
}

static ZEND_NAMED_FUNCTION(accel_is_file)
{
	if (accel_file_in_cache(INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
		RETURN_TRUE;
	} else {
		orig_is_file(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}
}

static ZEND_NAMED_FUNCTION(accel_is_readable)
{
	if (accel_file_in_cache(INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
		RETURN_TRUE;
	} else {
		orig_is_readable(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}
}

static ZEND_MINIT_FUNCTION(zend_accelerator)
{
	(void)type; /* keep the compiler happy */

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

void zend_accel_override_file_functions(void)
{
	zend_function *old_function;
	if (ZCG(enabled) && accel_startup_ok && ZCG(accel_directives).file_override_enabled) {
		if (file_cache_only) {
			zend_accel_error(ACCEL_LOG_WARNING, "file_override_enabled has no effect when file_cache_only is set");
			return;
		}
		/* override file_exists */
		if ((old_function = zend_hash_str_find_ptr(CG(function_table), "file_exists", sizeof("file_exists")-1)) != NULL) {
			orig_file_exists = old_function->internal_function.handler;
			old_function->internal_function.handler = accel_file_exists;
		}
		if ((old_function = zend_hash_str_find_ptr(CG(function_table), "is_file", sizeof("is_file")-1)) != NULL) {
			orig_is_file = old_function->internal_function.handler;
			old_function->internal_function.handler = accel_is_file;
		}
		if ((old_function = zend_hash_str_find_ptr(CG(function_table), "is_readable", sizeof("is_readable")-1)) != NULL) {
			orig_is_readable = old_function->internal_function.handler;
			old_function->internal_function.handler = accel_is_readable;
		}
	}
}

static ZEND_MSHUTDOWN_FUNCTION(zend_accelerator)
{
	(void)type; /* keep the compiler happy */

	UNREGISTER_INI_ENTRIES();
	accel_shutdown();
	return SUCCESS;
}

void zend_accel_info(ZEND_MODULE_INFO_FUNC_ARGS)
{
	php_info_print_table_start();

	if (ZCG(accelerator_enabled) || file_cache_only) {
		php_info_print_table_row(2, "Opcode Caching", "Up and Running");
	} else {
		php_info_print_table_row(2, "Opcode Caching", "Disabled");
	}
	if (ZCG(enabled) && accel_startup_ok && ZCG(accel_directives).optimization_level) {
		php_info_print_table_row(2, "Optimization", "Enabled");
	} else {
		php_info_print_table_row(2, "Optimization", "Disabled");
	}
	if (!file_cache_only) {
		php_info_print_table_row(2, "SHM Cache", "Enabled");
	} else {
		php_info_print_table_row(2, "SHM Cache", "Disabled");
	}
	if (ZCG(accel_directives).file_cache) {
		php_info_print_table_row(2, "File Cache", "Enabled");
	} else {
		php_info_print_table_row(2, "File Cache", "Disabled");
	}
#ifdef HAVE_JIT
	if (JIT_G(enabled)) {
		if (JIT_G(on)) {
			php_info_print_table_row(2, "JIT", "On");
		} else {
			php_info_print_table_row(2, "JIT", "Off");
		}
	} else {
		php_info_print_table_row(2, "JIT", "Disabled");
	}
#else
	php_info_print_table_row(2, "JIT", "Not Available");
#endif
	if (file_cache_only) {
		if (!accel_startup_ok || zps_api_failure_reason) {
			php_info_print_table_row(2, "Startup Failed", zps_api_failure_reason);
		} else {
			php_info_print_table_row(2, "Startup", "OK");
		}
	} else
	if (ZCG(enabled)) {
		if (!accel_startup_ok || zps_api_failure_reason) {
			php_info_print_table_row(2, "Startup Failed", zps_api_failure_reason);
		} else {
			char buf[32];
			zend_string *start_time, *restart_time, *force_restart_time;
			zval *date_ISO8601 = zend_get_constant_str("DATE_ISO8601", sizeof("DATE_ISO8601")-1);

			php_info_print_table_row(2, "Startup", "OK");
			php_info_print_table_row(2, "Shared memory model", zend_accel_get_shared_model());
			snprintf(buf, sizeof(buf), ZEND_ULONG_FMT, ZCSG(hits));
			php_info_print_table_row(2, "Cache hits", buf);
			snprintf(buf, sizeof(buf), ZEND_ULONG_FMT, ZSMMG(memory_exhausted)?ZCSG(misses):ZCSG(misses)-ZCSG(blacklist_misses));
			php_info_print_table_row(2, "Cache misses", buf);
			snprintf(buf, sizeof(buf), ZEND_LONG_FMT, ZCG(accel_directives).memory_consumption-zend_shared_alloc_get_free_memory()-ZSMMG(wasted_shared_memory));
			php_info_print_table_row(2, "Used memory", buf);
			snprintf(buf, sizeof(buf), "%zu", zend_shared_alloc_get_free_memory());
			php_info_print_table_row(2, "Free memory", buf);
			snprintf(buf, sizeof(buf), "%zu", ZSMMG(wasted_shared_memory));
			php_info_print_table_row(2, "Wasted memory", buf);
			if (ZCSG(interned_strings).start && ZCSG(interned_strings).end) {
				snprintf(buf, sizeof(buf), "%zu", (size_t)((char*)ZCSG(interned_strings).top - (char*)(accel_shared_globals + 1)));
				php_info_print_table_row(2, "Interned Strings Used memory", buf);
				snprintf(buf, sizeof(buf), "%zu", (size_t)((char*)ZCSG(interned_strings).end - (char*)ZCSG(interned_strings).top));
				php_info_print_table_row(2, "Interned Strings Free memory", buf);
			}
			snprintf(buf, sizeof(buf), "%" PRIu32, ZCSG(hash).num_direct_entries);
			php_info_print_table_row(2, "Cached scripts", buf);
			snprintf(buf, sizeof(buf), "%" PRIu32, ZCSG(hash).num_entries);
			php_info_print_table_row(2, "Cached keys", buf);
			snprintf(buf, sizeof(buf), "%" PRIu32, ZCSG(hash).max_num_entries);
			php_info_print_table_row(2, "Max keys", buf);
			snprintf(buf, sizeof(buf), ZEND_ULONG_FMT, ZCSG(oom_restarts));
			php_info_print_table_row(2, "OOM restarts", buf);
			snprintf(buf, sizeof(buf), ZEND_ULONG_FMT, ZCSG(hash_restarts));
			php_info_print_table_row(2, "Hash keys restarts", buf);
			snprintf(buf, sizeof(buf), ZEND_ULONG_FMT, ZCSG(manual_restarts));
			php_info_print_table_row(2, "Manual restarts", buf);

			start_time = php_format_date(Z_STRVAL_P(date_ISO8601), Z_STRLEN_P(date_ISO8601), ZCSG(start_time), 1);
			php_info_print_table_row(2, "Start time", ZSTR_VAL(start_time));
			zend_string_release(start_time);

			if (ZCSG(last_restart_time)) {
				restart_time = php_format_date(Z_STRVAL_P(date_ISO8601), Z_STRLEN_P(date_ISO8601), ZCSG(last_restart_time), 1);
				php_info_print_table_row(2, "Last restart time", ZSTR_VAL(restart_time));
				zend_string_release(restart_time);
			} else {
				php_info_print_table_row(2, "Last restart time", "none");
			}

			if (ZCSG(force_restart_time)) {
				force_restart_time = php_format_date(Z_STRVAL_P(date_ISO8601), Z_STRLEN_P(date_ISO8601), ZCSG(force_restart_time), 1);
				php_info_print_table_row(2, "Last force restart time", ZSTR_VAL(force_restart_time));
				zend_string_release(force_restart_time);
			} else {
				php_info_print_table_row(2, "Last force restart time", "none");
			}
		}
	}

	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}

static zend_module_entry accel_module_entry = {
	STANDARD_MODULE_HEADER,
	ACCELERATOR_PRODUCT_NAME,
	ext_functions,
	ZEND_MINIT(zend_accelerator),
	ZEND_MSHUTDOWN(zend_accelerator),
	ZEND_RINIT(zend_accelerator),
	NULL,
	zend_accel_info,
	PHP_VERSION,
	NO_MODULE_GLOBALS,
	accel_post_deactivate,
	STANDARD_MODULE_PROPERTIES_EX
};

int start_accel_module(void)
{
	return zend_startup_module(&accel_module_entry);
}

/* {{{ Get the scripts which are accelerated by ZendAccelerator */
static int accelerator_get_scripts(zval *return_value)
{
	uint32_t i;
	zval persistent_script_report;
	zend_accel_hash_entry *cache_entry;
	struct tm *ta;
	struct timeval exec_time;
	struct timeval fetch_time;

	if (!ZCG(accelerator_enabled) || accelerator_shm_read_lock() != SUCCESS) {
		return 0;
	}

	array_init(return_value);
	for (i = 0; i<ZCSG(hash).max_num_entries; i++) {
		for (cache_entry = ZCSG(hash).hash_table[i]; cache_entry; cache_entry = cache_entry->next) {
			zend_persistent_script *script;
			char *str;
			size_t len;

			if (cache_entry->indirect) continue;

			script = (zend_persistent_script *)cache_entry->data;

			array_init(&persistent_script_report);
			add_assoc_str(&persistent_script_report, "full_path", zend_string_dup(script->script.filename, 0));
			add_assoc_long(&persistent_script_report, "hits", script->dynamic_members.hits);
			add_assoc_long(&persistent_script_report, "memory_consumption", script->dynamic_members.memory_consumption);
			ta = localtime(&script->dynamic_members.last_used);
			str = asctime(ta);
			len = strlen(str);
			if (len > 0 && str[len - 1] == '\n') len--;
			add_assoc_stringl(&persistent_script_report, "last_used", str, len);
			add_assoc_long(&persistent_script_report, "last_used_timestamp", script->dynamic_members.last_used);
			if (ZCG(accel_directives).validate_timestamps) {
				add_assoc_long(&persistent_script_report, "timestamp", (zend_long)script->timestamp);
			}
			timerclear(&exec_time);
			timerclear(&fetch_time);

			add_assoc_long(&persistent_script_report, "revalidate", (zend_long)script->dynamic_members.revalidate);

			zend_hash_update(Z_ARRVAL_P(return_value), cache_entry->key, &persistent_script_report);
		}
	}
	accelerator_shm_read_unlock();

	return 1;
}

/* {{{ Obtain statistics information regarding code acceleration */
ZEND_FUNCTION(opcache_get_status)
{
	zend_long reqs;
	zval memory_usage, statistics, scripts;
	bool fetch_scripts = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &fetch_scripts) == FAILURE) {
		RETURN_THROWS();
	}

	if (!validate_api_restriction()) {
		RETURN_FALSE;
	}

	if (!accel_startup_ok) {
		RETURN_FALSE;
	}

	array_init(return_value);

	/* Trivia */
	add_assoc_bool(return_value, "opcache_enabled", ZCG(accelerator_enabled));

	if (ZCG(accel_directives).file_cache) {
		add_assoc_string(return_value, "file_cache", ZCG(accel_directives).file_cache);
	}
	if (file_cache_only) {
		add_assoc_bool(return_value, "file_cache_only", 1);
		return;
	}

	add_assoc_bool(return_value, "cache_full", ZSMMG(memory_exhausted));
	add_assoc_bool(return_value, "restart_pending", ZCSG(restart_pending));
	add_assoc_bool(return_value, "restart_in_progress", ZCSG(restart_in_progress));

	/* Memory usage statistics */
	array_init(&memory_usage);
	add_assoc_long(&memory_usage, "used_memory", ZCG(accel_directives).memory_consumption-zend_shared_alloc_get_free_memory()-ZSMMG(wasted_shared_memory));
	add_assoc_long(&memory_usage, "free_memory", zend_shared_alloc_get_free_memory());
	add_assoc_long(&memory_usage, "wasted_memory", ZSMMG(wasted_shared_memory));
	add_assoc_double(&memory_usage, "current_wasted_percentage", (((double) ZSMMG(wasted_shared_memory))/ZCG(accel_directives).memory_consumption)*100.0);
	add_assoc_zval(return_value, "memory_usage", &memory_usage);

	if (ZCSG(interned_strings).start && ZCSG(interned_strings).end) {
		zval interned_strings_usage;

		array_init(&interned_strings_usage);
		add_assoc_long(&interned_strings_usage, "buffer_size", (char*)ZCSG(interned_strings).end - (char*)(accel_shared_globals + 1));
		add_assoc_long(&interned_strings_usage, "used_memory", (char*)ZCSG(interned_strings).top - (char*)(accel_shared_globals + 1));
		add_assoc_long(&interned_strings_usage, "free_memory", (char*)ZCSG(interned_strings).end - (char*)ZCSG(interned_strings).top);
		add_assoc_long(&interned_strings_usage, "number_of_strings", ZCSG(interned_strings).nNumOfElements);
		add_assoc_zval(return_value, "interned_strings_usage", &interned_strings_usage);
	}

	/* Accelerator statistics */
	array_init(&statistics);
	add_assoc_long(&statistics, "num_cached_scripts", ZCSG(hash).num_direct_entries);
	add_assoc_long(&statistics, "num_cached_keys",    ZCSG(hash).num_entries);
	add_assoc_long(&statistics, "max_cached_keys",    ZCSG(hash).max_num_entries);
	add_assoc_long(&statistics, "hits", (zend_long)ZCSG(hits));
	add_assoc_long(&statistics, "start_time", ZCSG(start_time));
	add_assoc_long(&statistics, "last_restart_time", ZCSG(last_restart_time));
	add_assoc_long(&statistics, "oom_restarts", ZCSG(oom_restarts));
	add_assoc_long(&statistics, "hash_restarts", ZCSG(hash_restarts));
	add_assoc_long(&statistics, "manual_restarts", ZCSG(manual_restarts));
	add_assoc_long(&statistics, "misses", ZSMMG(memory_exhausted)?ZCSG(misses):ZCSG(misses)-ZCSG(blacklist_misses));
	add_assoc_long(&statistics, "blacklist_misses", ZCSG(blacklist_misses));
	reqs = ZCSG(hits)+ZCSG(misses);
	add_assoc_double(&statistics, "blacklist_miss_ratio", reqs?(((double) ZCSG(blacklist_misses))/reqs)*100.0:0);
	add_assoc_double(&statistics, "opcache_hit_rate", reqs?(((double) ZCSG(hits))/reqs)*100.0:0);
	add_assoc_zval(return_value, "opcache_statistics", &statistics);

	if (ZCSG(preload_script)) {
		array_init(&statistics);

		add_assoc_long(&statistics, "memory_consumption", ZCSG(preload_script)->dynamic_members.memory_consumption);

		if (zend_hash_num_elements(&ZCSG(preload_script)->script.function_table)) {
			zend_op_array *op_array;

			array_init(&scripts);
			ZEND_HASH_MAP_FOREACH_PTR(&ZCSG(preload_script)->script.function_table, op_array) {
				add_next_index_str(&scripts, op_array->function_name);
			} ZEND_HASH_FOREACH_END();
			add_assoc_zval(&statistics, "functions", &scripts);
		}

		if (zend_hash_num_elements(&ZCSG(preload_script)->script.class_table)) {
			zval *zv;
			zend_string *key;

			array_init(&scripts);
			ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(&ZCSG(preload_script)->script.class_table, key, zv) {
				if (Z_TYPE_P(zv) == IS_ALIAS_PTR) {
					add_next_index_str(&scripts, key);
				} else {
					add_next_index_str(&scripts, Z_CE_P(zv)->name);
				}
			} ZEND_HASH_FOREACH_END();
			add_assoc_zval(&statistics, "classes", &scripts);
		}

		if (ZCSG(saved_scripts)) {
			zend_persistent_script **p = ZCSG(saved_scripts);

			array_init(&scripts);
			while (*p) {
				add_next_index_str(&scripts, (*p)->script.filename);
				p++;
			}
			add_assoc_zval(&statistics, "scripts", &scripts);
		}
		add_assoc_zval(return_value, "preload_statistics", &statistics);
	}

	if (fetch_scripts) {
		/* accelerated scripts */
		if (accelerator_get_scripts(&scripts)) {
			add_assoc_zval(return_value, "scripts", &scripts);
		}
	}
#ifdef HAVE_JIT
	zend_jit_status(return_value);
#endif
}

static int add_blacklist_path(zend_blacklist_entry *p, zval *return_value)
{
	add_next_index_stringl(return_value, p->path, p->path_length);
	return 0;
}

/* {{{ Obtain configuration information */
ZEND_FUNCTION(opcache_get_configuration)
{
	zval directives, version, blacklist;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	if (!validate_api_restriction()) {
		RETURN_FALSE;
	}

	array_init(return_value);

	/* directives */
	array_init(&directives);
	add_assoc_bool(&directives, "opcache.enable",              ZCG(enabled));
	add_assoc_bool(&directives, "opcache.enable_cli",          ZCG(accel_directives).enable_cli);
	add_assoc_bool(&directives, "opcache.use_cwd",             ZCG(accel_directives).use_cwd);
	add_assoc_bool(&directives, "opcache.validate_timestamps", ZCG(accel_directives).validate_timestamps);
	add_assoc_bool(&directives, "opcache.validate_permission", ZCG(accel_directives).validate_permission);
#ifndef ZEND_WIN32
	add_assoc_bool(&directives, "opcache.validate_root",       ZCG(accel_directives).validate_root);
#endif
	add_assoc_bool(&directives, "opcache.dups_fix",            ZCG(accel_directives).ignore_dups);
	add_assoc_bool(&directives, "opcache.revalidate_path",     ZCG(accel_directives).revalidate_path);

	add_assoc_long(&directives,   "opcache.log_verbosity_level",    ZCG(accel_directives).log_verbosity_level);
	add_assoc_long(&directives,	 "opcache.memory_consumption",     ZCG(accel_directives).memory_consumption);
	add_assoc_long(&directives,	 "opcache.interned_strings_buffer",ZCG(accel_directives).interned_strings_buffer);
	add_assoc_long(&directives, 	 "opcache.max_accelerated_files",  ZCG(accel_directives).max_accelerated_files);
	add_assoc_double(&directives, "opcache.max_wasted_percentage",  ZCG(accel_directives).max_wasted_percentage);
	add_assoc_long(&directives, 	 "opcache.force_restart_timeout",  ZCG(accel_directives).force_restart_timeout);
	add_assoc_long(&directives, 	 "opcache.revalidate_freq",        ZCG(accel_directives).revalidate_freq);
	add_assoc_string(&directives, "opcache.preferred_memory_model", STRING_NOT_NULL(ZCG(accel_directives).memory_model));
	add_assoc_string(&directives, "opcache.blacklist_filename",     STRING_NOT_NULL(ZCG(accel_directives).user_blacklist_filename));
	add_assoc_long(&directives,   "opcache.max_file_size",          ZCG(accel_directives).max_file_size);
	add_assoc_string(&directives, "opcache.error_log",              STRING_NOT_NULL(ZCG(accel_directives).error_log));

	add_assoc_bool(&directives,   "opcache.protect_memory",         ZCG(accel_directives).protect_memory);
	add_assoc_bool(&directives,   "opcache.save_comments",          ZCG(accel_directives).save_comments);
	add_assoc_bool(&directives,   "opcache.record_warnings",        ZCG(accel_directives).record_warnings);
	add_assoc_bool(&directives,   "opcache.enable_file_override",   ZCG(accel_directives).file_override_enabled);
	add_assoc_long(&directives, 	 "opcache.optimization_level",     ZCG(accel_directives).optimization_level);

#ifndef ZEND_WIN32
	add_assoc_string(&directives, "opcache.lockfile_path",          STRING_NOT_NULL(ZCG(accel_directives).lockfile_path));
#else
	add_assoc_string(&directives, "opcache.mmap_base",              STRING_NOT_NULL(ZCG(accel_directives).mmap_base));
#endif

	add_assoc_string(&directives, "opcache.file_cache",                    ZCG(accel_directives).file_cache ? ZCG(accel_directives).file_cache : "");
	add_assoc_bool(&directives,   "opcache.file_cache_read_only",          ZCG(accel_directives).file_cache_read_only);
	add_assoc_bool(&directives,   "opcache.file_cache_only",               ZCG(accel_directives).file_cache_only);
	add_assoc_bool(&directives,   "opcache.file_cache_consistency_checks", ZCG(accel_directives).file_cache_consistency_checks);
#if ENABLE_FILE_CACHE_FALLBACK
	add_assoc_bool(&directives,   "opcache.file_cache_fallback",           ZCG(accel_directives).file_cache_fallback);
#endif

	add_assoc_long(&directives,   "opcache.file_update_protection",  ZCG(accel_directives).file_update_protection);
	add_assoc_long(&directives,   "opcache.opt_debug_level",         ZCG(accel_directives).opt_debug_level);
	add_assoc_string(&directives, "opcache.restrict_api",            STRING_NOT_NULL(ZCG(accel_directives).restrict_api));
#ifdef HAVE_HUGE_CODE_PAGES
	add_assoc_bool(&directives,   "opcache.huge_code_pages",         ZCG(accel_directives).huge_code_pages);
#endif
	add_assoc_string(&directives, "opcache.preload", STRING_NOT_NULL(ZCG(accel_directives).preload));
#ifndef ZEND_WIN32
	add_assoc_string(&directives, "opcache.preload_user", STRING_NOT_NULL(ZCG(accel_directives).preload_user));
#endif
#ifdef ZEND_WIN32
	add_assoc_string(&directives, "opcache.cache_id", STRING_NOT_NULL(ZCG(accel_directives).cache_id));
#endif
#ifdef HAVE_JIT
	add_assoc_string(&directives, "opcache.jit", JIT_G(options));
	add_assoc_long(&directives,   "opcache.jit_buffer_size", JIT_G(buffer_size));
	add_assoc_long(&directives,   "opcache.jit_debug", JIT_G(debug));
	add_assoc_long(&directives,   "opcache.jit_bisect_limit", JIT_G(bisect_limit));
	add_assoc_long(&directives,   "opcache.jit_blacklist_root_trace", JIT_G(blacklist_root_trace));
	add_assoc_long(&directives,   "opcache.jit_blacklist_side_trace", JIT_G(blacklist_side_trace));
	add_assoc_long(&directives,   "opcache.jit_hot_func", JIT_G(hot_func));
	add_assoc_long(&directives,   "opcache.jit_hot_loop", JIT_G(hot_loop));
	add_assoc_long(&directives,   "opcache.jit_hot_return", JIT_G(hot_return));
	add_assoc_long(&directives,   "opcache.jit_hot_side_exit", JIT_G(hot_side_exit));
	add_assoc_long(&directives,   "opcache.jit_max_exit_counters", JIT_G(max_exit_counters));
	add_assoc_long(&directives,   "opcache.jit_max_loop_unrolls", JIT_G(max_loop_unrolls));
	add_assoc_long(&directives,   "opcache.jit_max_polymorphic_calls", JIT_G(max_polymorphic_calls));
	add_assoc_long(&directives,   "opcache.jit_max_recursive_calls", JIT_G(max_recursive_calls));
	add_assoc_long(&directives,   "opcache.jit_max_recursive_returns", JIT_G(max_recursive_returns));
	add_assoc_long(&directives,   "opcache.jit_max_root_traces", JIT_G(max_root_traces));
	add_assoc_long(&directives,   "opcache.jit_max_side_traces", JIT_G(max_side_traces));
	add_assoc_double(&directives, "opcache.jit_prof_threshold", JIT_G(prof_threshold));
	add_assoc_long(&directives,   "opcache.jit_max_trace_length", JIT_G(max_trace_length));
#endif

	add_assoc_zval(return_value, "directives", &directives);

	/*version */
	array_init(&version);
	add_assoc_string(&version, "version", PHP_VERSION);
	add_assoc_string(&version, "opcache_product_name", ACCELERATOR_PRODUCT_NAME);
	add_assoc_zval(return_value, "version", &version);

	/* blacklist */
	array_init(&blacklist);
	zend_accel_blacklist_apply(&accel_blacklist, add_blacklist_path, &blacklist);
	add_assoc_zval(return_value, "blacklist", &blacklist);
}

/* {{{ Request that the contents of the opcode cache to be reset */
ZEND_FUNCTION(opcache_reset)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	if (!validate_api_restriction()) {
		RETURN_FALSE;
	}

	if ((!ZCG(enabled) || !accel_startup_ok || !ZCSG(accelerator_enabled))
#if ENABLE_FILE_CACHE_FALLBACK
	&& !fallback_process
#endif
	) {
		RETURN_FALSE;
	}

	/* exclusive lock */
	zend_shared_alloc_lock();
	zend_accel_schedule_restart(ACCEL_RESTART_USER);
	zend_shared_alloc_unlock();
	RETURN_TRUE;
}

/* {{{ Invalidates cached script (in necessary or forced) */
ZEND_FUNCTION(opcache_invalidate)
{
	zend_string *script_name;
	bool force = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|b", &script_name, &force) == FAILURE) {
		RETURN_THROWS();
	}

	if (!validate_api_restriction()) {
		RETURN_FALSE;
	}

	if (zend_accel_invalidate(script_name, force) == SUCCESS) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}

/* {{{ Prevents JIT on function. Call it before the first invocation of the given function. */
ZEND_FUNCTION(opcache_jit_blacklist)
{
	zval *closure;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &closure, zend_ce_closure) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef HAVE_JIT
	const zend_function *func = zend_get_closure_method_def(Z_OBJ_P(closure));
	if (ZEND_USER_CODE(func->type)) {
		zend_jit_blacklist_function((zend_op_array *)&func->op_array);
	}
#endif
}

ZEND_FUNCTION(opcache_compile_file)
{
	zend_string *script_name;
	zend_file_handle handle;
	zend_op_array *op_array = NULL;
	zend_execute_data *orig_execute_data = NULL;
	uint32_t orig_compiler_options;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &script_name) == FAILURE) {
		RETURN_THROWS();
	}

	if (!accel_startup_ok) {
		zend_error(E_NOTICE, ACCELERATOR_PRODUCT_NAME " has not been properly started, can't compile file");
		RETURN_FALSE;
	}

	zend_stream_init_filename_ex(&handle, script_name);

	orig_execute_data = EG(current_execute_data);
	orig_compiler_options = CG(compiler_options);
	CG(compiler_options) |= ZEND_COMPILE_WITHOUT_EXECUTION;

	if (CG(compiler_options) & ZEND_COMPILE_PRELOAD) {
		/* During preloading, a failure in opcache_compile_file() should result in an overall
		 * preloading failure. Otherwise we may include partially compiled files in the preload
		 * state. */
		op_array = persistent_compile_file(&handle, ZEND_INCLUDE);
	} else {
		zend_try {
			op_array = persistent_compile_file(&handle, ZEND_INCLUDE);
		} zend_catch {
			EG(current_execute_data) = orig_execute_data;
			zend_error(E_WARNING, ACCELERATOR_PRODUCT_NAME " could not compile file %s", ZSTR_VAL(handle.filename));
		} zend_end_try();
	}

	CG(compiler_options) = orig_compiler_options;

	if(op_array != NULL) {
		destroy_op_array(op_array);
		efree(op_array);
		RETVAL_TRUE;
	} else {
		RETVAL_FALSE;
	}
	zend_destroy_file_handle(&handle);
}

/* {{{ Return true if the script is cached in OPCache, false if it is not cached or if OPCache is not running. */
ZEND_FUNCTION(opcache_is_script_cached)
{
	zend_string *script_name;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(script_name)
	ZEND_PARSE_PARAMETERS_END();

	if (!validate_api_restriction()) {
		RETURN_FALSE;
	}

	if (!ZCG(accelerator_enabled)) {
		RETURN_FALSE;
	}

	RETURN_BOOL(filename_is_in_cache(script_name));
}

#if 0
static void snapshot_check_value(zval *zv, smart_str *path);

static bool snapshot_is_internal_class(zend_class_entry *ce, zend_class_entry **internal_ce)
{
	if (ce->type == ZEND_INTERNAL_CLASS) {
		*internal_ce = ce;
		return true;
	}
	if (ce->parent) {
		return snapshot_is_internal_class(ce->parent, internal_ce);
	}
	return false;
}

static void snapshot_check_func(const zend_function *func, smart_str *path)
{
	if (!ZEND_USER_CODE(func->type)) {
		return;
	}

	zend_array *ht = ZEND_MAP_PTR_GET(func->op_array.static_variables_ptr);
	if (!ht) {
		return;
	}

	zval *zv;
	zend_string *key;
	size_t plen = ZSTR_LEN(path->s);
	smart_str_appends(path, " $");
	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(ht, key, zv) {
		smart_str_append_escaped(path, ZSTR_VAL(key), ZSTR_LEN(key));
		snapshot_check_value(zv, path);
		ZSTR_LEN(path->s) = plen + strlen(" $");
	} ZEND_HASH_FOREACH_END();
	ZSTR_LEN(path->s) = plen;
}

static void snapshot_check_ce(zend_class_entry *ce, smart_str *path)
{
	if (ce->type != ZEND_USER_CLASS) {
		return;
	}

	// TODO: constants
	// TODO: default props

	/*
	zval *table = ZEND_MAP_PTR_GET(ce->static_members_table);
	size_t plen = ZSTR_LEN(path->s);
	smart_str_appends(path, "::$");
	for (uint32_t i = 0; i < ce->default_
	for (int i = 0; i < ce->default_static_members_count; i++) {
		smart_str_append_escaped(
		snapshot_check_value(&table[i], path);
	}
	*/
}
// TODO
extern zend_class_entry *date_ce_timezone;

/* Check that value 'zv' can be snapshotted.
 * Internal classes and resources can not. */
// TODO: this may visit objects multiple times
static void snapshot_check_value(zval *zv, smart_str *path) {
	uint8_t type = Z_TYPE_P(zv);
	if (type < IS_STRING) {
		return;
	}

	switch(type) {
		case IS_STRING:
			if (ZSTR_IS_INTERNED(Z_STR_P(zv))) {
				return;
			}
			if (GC_FLAGS(Z_STR_P(zv)) & IS_STR_PERSISTENT) {
				smart_str_0(path);
				fprintf(stderr, "%s: persistent string\n", ZSTR_VAL(path->s));
				return;
			}
			break;
		case IS_ARRAY: {
			if (GC_IS_RECURSIVE(Z_ARR_P(zv))) {
				/* We are checking this array already */
				break;
			}
			// TODO: immutable/persistent arrays?
			GC_TRY_PROTECT_RECURSION(Z_ARR_P(zv));

			zend_long h;
			zend_string *key;
			zval *v;
			size_t plen = ZSTR_LEN(path->s);
			smart_str_appends(path, "[\"");
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARR_P(zv), h, key, v) {
				if (key) {
					smart_str_append_escaped(path, ZSTR_VAL(key), ZSTR_LEN(key));
				} else {
					smart_str_append_long(path, h);
				}
				smart_str_appends(path, "\"]");
				snapshot_check_value(v, path);
				ZSTR_LEN(path->s) = plen + strlen("[\"");
			} ZEND_HASH_FOREACH_END();
			ZSTR_LEN(path->s) = plen;
			GC_TRY_UNPROTECT_RECURSION(Z_ARR_P(zv));
			break;
		}
		case IS_OBJECT: {
			zend_class_entry *ce;
			if (snapshot_is_internal_class(Z_OBJCE_P(zv), &ce)) {
				// TODO: Add a new object handler
				/* Closures can not have extra properties, but can be scoped to
				 * a class, and have static variables */
				if (ce == zend_ce_closure) {
					size_t plen = ZSTR_LEN(path->s);

					zval *this_ptr = zend_get_closure_this_ptr(zv);
					smart_str_appends(path, " $this");
					snapshot_check_value(this_ptr, path);
					ZSTR_LEN(path->s) = plen;

					const zend_function *func = zend_get_closure_method_def(Z_OBJ_P(zv));
					snapshot_check_func(func, path);
				/* WeakMap can not have properties, but can reference objects */
				} else if (ce == zend_ce_weakmap) {
					zend_object_iterator *it = ce->get_iterator(ce, zv, 0);
					size_t plen = ZSTR_LEN(path->s);
					smart_str_appends(path, "[object #");
					while (it->funcs->valid(it) == SUCCESS) {
						zval key;
						it->funcs->get_current_key(it, &key);
						zval *value = it->funcs->get_current_data(it);
						smart_str_append_long(path, Z_OBJ(key)->handle);
						smart_str_appendc(path, ']');
						snapshot_check_value(&key, path);
						if (value) {
							snapshot_check_value(value, path);
						}
						it->funcs->move_forward(it);
						ZSTR_LEN(path->s) = plen + strlen("[object #");
					}
					zend_object_release(&it->std);
					ZSTR_LEN(path->s) = plen;
				/* ArrayIterator only uses heap memory, but can have properties */
				} else if (ce == spl_ce_ArrayIterator) {
					goto check_obj;
				/* DateTimeZone only uses heap memory, but can have properties */
				} else if (ce == date_ce_timezone) {
					goto check_obj;
				} else {
					smart_str_0(path);
					fprintf(stderr, "%s: internal class %s\n", ZSTR_VAL(path->s), ZSTR_VAL(ce->name));
				}
				return;
			}

check_obj:
			if (GC_IS_RECURSIVE(Z_ARR_P(zv))) {
				/* We are checking this object already */
				break;
			}
			GC_TRY_PROTECT_RECURSION(Z_ARR_P(zv));

			zend_object *obj = Z_OBJ_P(zv);
			ZEND_ASSERT(!zend_object_is_lazy(obj) && "TODO: lazy object support");

			size_t plen = ZSTR_LEN(path->s);
			smart_str_appends(path, "->");

			if (obj->properties) {
				zend_long h;
				zend_string *key;
				zval *v;
				ZEND_HASH_FOREACH_KEY_VAL(Z_ARR_P(zv), h, key, v) {
					if (key) {
						smart_str_append_escaped(path, ZSTR_VAL(key), ZSTR_LEN(key));
					} else {
						smart_str_append_long(path, h);
					}
					snapshot_check_value(v, path);
					ZSTR_LEN(path->s) = plen + strlen("->");
				} ZEND_HASH_FOREACH_END();
			} else {
				smart_str_appends(path, "->");
				for (uint32_t i = 0; i < obj->ce->default_properties_count; i++) {
					zval *v = &obj->properties_table[i];
					zend_property_info *prop_info = zend_get_property_info_for_slot_self(obj, v);
					if (!prop_info) {
						continue;
					}
					smart_str_append_escaped(path, ZSTR_VAL(prop_info->name), ZSTR_LEN(prop_info->name));
					snapshot_check_value(v, path);
					ZSTR_LEN(path->s) = plen + strlen("->");
				}
			}
			ZSTR_LEN(path->s) = plen;
			GC_TRY_UNPROTECT_RECURSION(Z_ARR_P(zv));
			break;
		}
		case IS_RESOURCE:
			smart_str_0(path);
			fprintf(stderr, "%s: resource\n", ZSTR_VAL(path->s));
			break;
		case IS_REFERENCE:
			snapshot_check_value(&Z_REF_P(zv)->val, path);
			break;
		case IS_CONSTANT_AST:
			break;
		case IS_INDIRECT:
			snapshot_check_value(Z_INDIRECT_P(zv), path);
			break;
		EMPTY_SWITCH_DEFAULT_CASE();
	}
}
#endif

static zend_always_inline void zend_snapshot_class_table_copy(zend_array *target, zend_array *source, void *map_ptr_snapshot, bool call_observers)
{
	Bucket *p, *end;
	zval *t;

	intptr_t map_ptr_snapshot_base = (intptr_t)map_ptr_snapshot + zend_map_ptr_static_size * sizeof(void *) - 1;

	zend_hash_extend(target, target->nNumUsed + source->nNumUsed, 0);
	p = source->arData;
	end = p + source->nNumUsed;
	for (; p != end; p++) {
		ZEND_ASSERT(Z_TYPE(p->val) != IS_UNDEF);
		ZEND_ASSERT(p->key);
		t = zend_hash_find_known_hash(target, p->key);
		if (UNEXPECTED(t != NULL)) {
			zend_class_entry *ce1 = Z_PTR(p->val);
			CG(in_compilation) = 1;
			zend_set_compiled_filename(ce1->info.user.filename);
			CG(zend_lineno) = ce1->info.user.line_start;
			zend_class_redeclaration_error(E_ERROR, Z_PTR_P(t));
			return;
		} else {
			zend_class_entry *ce = Z_PTR(p->val);
			_zend_hash_append_ptr_ex(target, p->key, Z_PTR(p->val), 1);
			if ((ce->ce_flags & ZEND_ACC_LINKED) && ZSTR_VAL(p->key)[0]) {
				if (ZSTR_HAS_CE_CACHE(ce->name)) {
					ZSTR_SET_CE_CACHE_EX(ce->name, ce, 0);
				}
				if (UNEXPECTED(call_observers)) {
					// TODO _zend_observer_class_linked_notify(ce, p->key);
				}
			}
			if (ce->default_static_members_count) {
				ZEND_ASSERT(ZEND_MAP_PTR_IS_OFFSET(ce->static_members_table));
				zval *src = *(zval**)(map_ptr_snapshot_base + (intptr_t)ZEND_MAP_PTR(ce->static_members_table));
				zval **dst = (zval**)((char*)CG(map_ptr_base) + (intptr_t)ZEND_MAP_PTR(ce->static_members_table));
				if (src) {
					*dst = src;
				} else {
					ZEND_ASSERT(!*dst);
				}
			}
			// TODO: default ast props
			if (ZEND_MAP_PTR(ce->mutable_data)) {
				ZEND_ASSERT(ZEND_MAP_PTR_IS_OFFSET(ce->mutable_data));
				zval *src = *(zval**)(map_ptr_snapshot_base + (intptr_t)ZEND_MAP_PTR(ce->mutable_data));
				zval **dst = (zval**)((char*)CG(map_ptr_base) + (intptr_t)ZEND_MAP_PTR(ce->mutable_data));
				if (src) {
					*dst = src;
				} else {
					ZEND_ASSERT(!*dst);
				}
			}
		}
	}
	target->nInternalPointer = 0;
}

static zend_always_inline void zend_snapshot_function_table_copy(HashTable *target, HashTable *source, bool call_observers)
{
	zend_function *function1, *function2;
	Bucket *p, *end;
	zval *t;

	zend_hash_extend(target, target->nNumUsed + source->nNumUsed, 0);
	p = source->arData;
	end = p + source->nNumUsed;
	for (; p != end; p++) {
		ZEND_ASSERT(Z_TYPE(p->val) != IS_UNDEF);
		ZEND_ASSERT(p->key);
		t = zend_hash_find_known_hash(target, p->key);
		if (UNEXPECTED(t != NULL)) {
			goto failure;
		}
		_zend_hash_append_ptr_ex(target, p->key, Z_PTR(p->val), 1);
		if (UNEXPECTED(call_observers) && *ZSTR_VAL(p->key)) { // if not rtd key
			// TODO
			// _zend_observer_function_declared_notify(Z_PTR(p->val), p->key);
		}
	}
	target->nInternalPointer = 0;

	return;

failure:
	function1 = Z_PTR(p->val);
	function2 = Z_PTR_P(t);
	CG(in_compilation) = 1;
	zend_set_compiled_filename(function1->op_array.filename);
	CG(zend_lineno) = function1->op_array.line_start;
	if (function2->type == ZEND_USER_FUNCTION
		&& function2->op_array.last > 0) {
		zend_error_noreturn(E_ERROR, "Cannot redeclare function %s() (previously declared in %s:%d)",
				   ZSTR_VAL(function1->common.function_name),
				   ZSTR_VAL(function2->op_array.filename),
				   (int)function2->op_array.line_start);
	} else {
		zend_error_noreturn(E_ERROR, "Cannot redeclare function %s()", ZSTR_VAL(function1->common.function_name));
	}
}

static zend_always_inline void zend_snapshot_constants_table_copy(HashTable *target, HashTable *source)
{
	zend_constant *c1, *c2;
	Bucket *p, *end;
	zval *t;

	zend_hash_extend(target, target->nNumUsed + source->nNumUsed, 0);
	p = source->arData;
	end = p + source->nNumUsed;
	for (; p != end; p++) {
		ZEND_ASSERT(Z_TYPE(p->val) != IS_UNDEF);
		ZEND_ASSERT(p->key);
		t = zend_hash_find_known_hash(target, p->key);
		if (UNEXPECTED(t != NULL)) {
			goto failure;
		}
		_zend_hash_append_ptr_ex(target, p->key, Z_PTR(p->val), 1);
	}
	target->nInternalPointer = 0;

	return;

failure:
	c1 = Z_PTR(p->val);
	c2 = Z_PTR_P(t);
	CG(in_compilation) = 1;
	zend_set_compiled_filename(c1->filename);
	CG(zend_lineno) = 0;
	zend_error_noreturn(E_ERROR, "Cannot redeclare constant %s (previously declared in %s)",
			   ZSTR_VAL(c1->name),
			   ZSTR_VAL(c2->filename));
}

ZEND_FUNCTION(opcache_restore)
{
	zend_execute_data *ex = EG(current_execute_data);
	if (ex->prev_execute_data->prev_execute_data) {
		zend_throw_error(NULL, "opcache_restore() must be called from the initial script");
		RETURN_THROWS();
	}

	zend_snapshot *s = EG(snapshot);

	if (!s) {
		RETURN_FALSE;
	}

	zend_mm_heap *heap = zend_mm_get_heap();
	char *src = s->copy;
	for (int i = 0; i < s->chunks_count; i++) {
		zend_mm_chunk *chunk = s->chunks[i];
		memcpy(chunk, src, ZEND_MM_CHUNK_SIZE);
		src += ZEND_MM_CHUNK_SIZE;
		zend_mm_adopt_chunk(heap, chunk);
	}

	zend_snapshot_class_table_copy(EG(class_table), &s->class_table, s->map_ptr_real_base, 0);
	zend_snapshot_function_table_copy(EG(function_table), &s->function_table, 0);
	zend_snapshot_constants_table_copy(EG(zend_constants), &s->constant_table);
	// TODO: function/method static vars

	if (s->objects_count > 0) {
		for (zend_object **p = s->objects, **end = p + s->objects_count; p < end; p++) {
			zend_objects_store_put(*p);
		}
	}

	EG(user_error_handler_error_reporting) = s->user_error_handler_error_reporting;
	EG(user_error_handler) = s->user_error_handler;
	EG(user_exception_handler) = s->user_exception_handler;

	zend_stack_destroy(&EG(user_error_handlers_error_reporting));
	EG(user_error_handlers_error_reporting) = s->user_error_handlers_error_reporting;

	ZEND_ASSERT(EG(user_error_handlers).size == sizeof(zval));
	for (int i = 0, l = EG(user_error_handlers).top; i < l; i++) {
		zval *zv = &((zval*)EG(user_error_handlers).elements)[i];
		zval_ptr_dtor(zv);
	}
	zend_stack_destroy(&EG(user_error_handlers));
	EG(user_error_handlers) = s->user_error_handlers;

	ZEND_ASSERT(EG(user_exception_handlers).size == sizeof(zval));
	for (int i = 0, l = EG(user_exception_handlers).top; i < l; i++) {
		zval *zv = &((zval*)EG(user_exception_handlers).elements)[i];
		zval_ptr_dtor(zv);
	}
	zend_stack_destroy(&EG(user_exception_handlers));
	EG(user_exception_handlers) = s->user_exception_handlers;

	// TODO: call via extension handlers?
	php_standard_restore(s->standard_snapshot);
	spl_restore(s->spl_snapshot);
	date_restore(s->date_snapshot);

	zend_array *current_symbol_table = zend_rebuild_symbol_table();
	zend_array *garbage = zend_new_array(current_symbol_table->nNumUsed);

	zend_string *key;
	zval *zv;
	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(&s->symbol_table, key, zv) {
		zval *entry = zend_hash_lookup(current_symbol_table, key);
		if (Z_TYPE_P(entry) == IS_INDIRECT) {
			entry = Z_INDIRECT_P(entry);
		}
		if (Z_REFCOUNTED_P(entry)) {
			zend_hash_next_index_insert(garbage, entry);
		}
		ZVAL_COPY_VALUE(entry, zv);
	} ZEND_HASH_FOREACH_END();

	zend_array_destroy(garbage);

	efree(s->map_ptr_real_base);

	RETURN_TRUE;
}

ZEND_FUNCTION(opcache_snapshot)
{
	if (EG(snapshot)) {
		zend_throw_error(NULL, "Can not use opcache_snapshot() when a snapshot already exists (yet)");
		RETURN_THROWS();
	}

	zend_execute_data *ex = EG(current_execute_data);
	if (ex->prev_execute_data->prev_execute_data) {
		zend_throw_error(NULL, "opcache_snapshot() must be called from the initial script");
		RETURN_THROWS();
	}

	zend_array *ht = zend_rebuild_symbol_table();

	zval *zv;
	zend_string *key;
#if 0
	smart_str path = {0};
	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(ht, key, zv) {
		smart_str_appendc(&path, '$');
		smart_str_append_escaped(&path, ZSTR_VAL(key), ZSTR_LEN(key));
		snapshot_check_value(zv, &path);
		ZSTR_LEN(path.s) = 0;
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL_FROM(EG(function_table), key, zv, EG(persistent_functions_count)) {
		zend_function *func = Z_FUNC_P(zv);
		if (!ZEND_USER_CODE(func->type)) {
			continue;
		}
		smart_str_append_escaped(&path, ZSTR_VAL(func->common.function_name), ZSTR_LEN(func->common.function_name));
		smart_str_appends(&path, "()");
		snapshot_check_func(func, &path);
		ZSTR_LEN(path.s) = 0;
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL_FROM(EG(class_table), key, zv, EG(persistent_classes_count)) {
		zend_class_entry *ce = Z_CE_P(zv);
		if (ce->type != ZEND_USER_CLASS) {
			continue;
		}
		smart_str_append_escaped(&path, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name));
		snapshot_check_ce(ce, &path);
		ZSTR_LEN(path.s) = 0;
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL_FROM(EG(zend_constants), key, zv, EG(persistent_constants_count)) {
		zend_constant *c = (zend_constant*)Z_PTR_P(zv);
		smart_str_append_escaped(&path, ZSTR_VAL(c->name), ZSTR_LEN(c->name));
		snapshot_check_value(&c->value, &path);
		ZSTR_LEN(path.s) = 0;
	} ZEND_HASH_FOREACH_END();
	smart_str_free(&path);
#endif

	// Snapshot:
	// - Add opcache setting: opcache.snapshot_memory
	// - Creates a private memory mapping when opcache starts. The ensures that
	//   every process will have a block at the same address. Why not re-use
	//   SHM? Because it's executable, and it would also break
	//   opcache.protect_memory.
	// - When SHM restarts, also restart snapshot_memory, because it points to
	//   SHM (e.g. classes)
	// - Re-alloc/move everything to a new heap
	//   - Make the heap adopt snapshot memory as blocks
	//   - Update addresses so they point to snapshot memory
	//   - This includes the symbol tables, declared
	//     variables, arrays, objects; and any state like error handlers,
	//     autoloaders.
	// - Copy heap blocks to snapshot memory
	// - When restoring snapshot:
	//   - Make the heap adopt the snapshot memory blocks
	//   - Update class/func/constant tables
	//   - Update declared vars
	//   - Update global state (error handlers, autoloaders)
	// - Need to copy map ptr buffer at some point
	// - We can not snapshot references to non-immutable classes/functions.
	//   Persist those?
	//
	// snapshot object handler:
	//
	// zend_object *snapshot(zend_object *obj, zend_mm_heap *heap) {
	//     obj = zend_mm_alloc(heap, ...);
	//     // for each property:
	//     //     prop = snapshot_value(prop)
	//     // Don't snapshot static props, constants
	// }
	//
	// Helpers:
	// zval *snapshot_value(zval *value);
	//
	// In theory, we can even snapshot just one variable:
	// opcache_snapshot($var).
	// Semantic would be:
	// - Restores all classes/funcs used by the var. Fails if already declared.
	// - Will not restore classes/funcs not used by the var.
	// - User has to restore error handler, class loader, etc by themself.


	zend_mm_heap *snapshot_heap = zend_mm_startup();
	zend_snapshot_builder *sb = zend_snapshot_builder_new(snapshot_heap);

	zend_array new_ht;
	zend_hash_init(&new_ht, ht->nNumUsed, NULL, NULL, 1);
	zend_hash_copy(&new_ht, ht, NULL);

	ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(&new_ht, key, zv) {
		if (strcmp(ZSTR_VAL(key), "_GET") == 0
				|| strcmp(ZSTR_VAL(key), "_POST") == 0
				|| strcmp(ZSTR_VAL(key), "_COOKIE") == 0
				|| strcmp(ZSTR_VAL(key), "_FILES") == 0
				|| strcmp(ZSTR_VAL(key), "_SERVER") == 0
				|| strcmp(ZSTR_VAL(key), "_ENV") == 0
				|| strcmp(ZSTR_VAL(key), "argv") == 0
				|| strcmp(ZSTR_VAL(key), "argc") == 0) {
			zend_hash_del(&new_ht, key);
			continue;
		}
		zval *dest = zv;
		if (Z_TYPE_P(zv) == IS_INDIRECT) {
			zv = Z_INDIRECT_P(zv);
		}
		*dest = zend_snapshot_zval(sb, *zv);
	} ZEND_HASH_FOREACH_END();

	if (EG(exception)) {
		zend_snapshot_builder_dtor(sb);
		RETURN_THROWS();
	}

	zend_array constants;
	zend_hash_init(&constants, 0, NULL, NULL, 1);
	zend_constant *c;
	ZEND_HASH_MAP_FOREACH_STR_KEY_PTR(EG(zend_constants), key, c) {
		if (ZEND_CONSTANT_MODULE_NUMBER(c) == PHP_USER_CONSTANT) {
			key = zend_snapshot_str(sb, key);
			bool is_new;
			zend_constant *dup = zend_snapshot_memdup_ex(sb, c, sizeof(zend_constant), &is_new);
			ZEND_ASSERT(is_new);
			dup->value = zend_snapshot_zval(sb, c->value);
			ZEND_ASSERT(ZEND_CONSTANT_MODULE_NUMBER(dup) == PHP_USER_CONSTANT);
			dup->name = zend_snapshot_str(sb, c->name);
			dup->filename = zend_snapshot_str(sb, c->filename);
			zend_hash_add_ptr(&constants, key, dup);
		}
	} ZEND_HASH_FOREACH_END();

	if (EG(exception)) {
		zend_snapshot_builder_dtor(sb);
		RETURN_THROWS();
	}

	// TODO: call via extension handlers?
	void *date = date_snapshot(sb);
	void *spl = spl_snapshot(sb);
	void *standard = php_standard_snapshot(sb);

	if (EG(exception)) {
		zend_snapshot_builder_dtor(sb);
		RETURN_THROWS();
	}

	zend_snapshot *s = malloc(sizeof(zend_snapshot));
	s->map_ptr_real_base = zend_snapshot_memdup(sb, CG(map_ptr_real_base), (zend_map_ptr_static_size + CG(map_ptr_size)) * sizeof(void*));
	s->symbol_table = new_ht;
	s->constant_table = constants;
	s->date_snapshot = date;
	s->spl_snapshot = spl;
	s->standard_snapshot = standard;

	s->user_error_handlers_error_reporting = zend_snapshot_stack(sb, EG(user_error_handlers_error_reporting));
	s->user_error_handler_error_reporting = EG(user_error_handler_error_reporting);

	s->user_error_handlers = zend_snapshot_stack(sb, EG(user_error_handlers));
	s->user_error_handler = zend_snapshot_zval(sb, EG(user_error_handler));

	s->user_exception_handlers = zend_snapshot_stack(sb, EG(user_exception_handlers));
	s->user_exception_handler = zend_snapshot_zval(sb, EG(user_exception_handler));

	ZEND_ASSERT(s->user_error_handlers.size == sizeof(zval));
	for (int i = 0, l = s->user_error_handlers.top; i < l; i++) {
		zval *zv = &((zval*)s->user_error_handlers.elements)[i];
		*zv = zend_snapshot_zval(sb, *zv);
	}

	ZEND_ASSERT(s->user_exception_handlers.size == sizeof(zval));
	for (int i = 0, l = s->user_exception_handlers.top; i < l; i++) {
		zval *zv = &((zval*)s->user_exception_handlers.elements)[i];
		*zv = zend_snapshot_zval(sb, *zv);
	}

	if (zend_mm_get_huge_list(snapshot_heap)) {
		zend_snapshot_builder_dtor(sb);
		free(s);
		zend_throw_error(NULL, "TODO: support zend mm huge blocks");
		RETURN_THROWS();
	}

	zend_hash_init(&s->class_table, EG(class_table)->nNumUsed - EG(persistent_classes_count), NULL, NULL, 1);
	zend_hash_real_init_mixed(&s->class_table);
	ZEND_HASH_FOREACH_STR_KEY_VAL_FROM(EG(class_table), key, zv, EG(persistent_classes_count)) {
		zend_class_entry *ce = Z_CE_P(zv);
		if (ce->type != ZEND_USER_CLASS) {
			zend_throw_error(NULL, "Can not snapshot class loaded by dl(): %s",
					ZSTR_VAL(Z_CE_P(zv)->name));
			RETURN_THROWS();
		}
		if (!(ce->ce_flags & ZEND_ACC_IMMUTABLE)) {
			zend_throw_error(NULL, "Can not snapshot non-cached class: %s",
					ZSTR_VAL(Z_CE_P(zv)->name));
			RETURN_THROWS();
		}

		/* Snapshot static props */
		if (ce->default_static_members_count) {
			ZEND_ASSERT(ZEND_MAP_PTR_IS_OFFSET(ce->static_members_table));
			zval **table_p = (zval**)((char*)ZEND_MAP_PTR_BIASED_BASE(s->map_ptr_real_base) + (intptr_t)ZEND_MAP_PTR(ce->static_members_table));
			if (*table_p) {
				*table_p = zend_snapshot_memdup(sb, *table_p, sizeof(zval) * ce->default_static_members_count);
				zval *table = *table_p;
				for (int i = 0; i < ce->default_static_members_count; i++) {
					table[i] = zend_snapshot_zval(sb, table[i]);
					if (EG(exception)) {
						RETURN_THROWS();
					}
				}
			}
		}

		/* Snapshot mutable data */
		if (ZEND_MAP_PTR(ce->mutable_data)) {
			ZEND_ASSERT(ZEND_MAP_PTR_IS_OFFSET(ce->mutable_data));
			zend_class_mutable_data **mutable_data_p = (zend_class_mutable_data**)((char*)ZEND_MAP_PTR_BIASED_BASE(s->map_ptr_real_base) + (intptr_t)ZEND_MAP_PTR(ce->mutable_data));
			if (*mutable_data_p) {
				*mutable_data_p = zend_snapshot_memdup(sb, *mutable_data_p, sizeof(zend_class_mutable_data));
				zend_class_mutable_data *mutable_data = *mutable_data_p;
				if (mutable_data->default_properties_table) {
					mutable_data->default_properties_table = zend_snapshot_memdup(sb, mutable_data->default_properties_table, ce->default_properties_count * sizeof(zval));
					for (int i = 0; i < ce->default_properties_count; i++) {
						mutable_data->default_properties_table[i] = zend_snapshot_zval(sb, mutable_data->default_properties_table[i]);
					}
				}
				if (mutable_data->constants_table) {
					mutable_data->constants_table = zend_snapshot_ht(sb, mutable_data->constants_table);
					zend_string *key;
					zval *zv;
					ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(mutable_data->constants_table, key, zv) {
						((Bucket*)zv)->key = zend_snapshot_str(sb, key);
						zend_class_constant *c = Z_PTR_P(zv);
						bool is_new;
						c = zend_snapshot_memdup_ex(sb, c, sizeof(zend_class_constant), &is_new);
						if (is_new) {
							c->value = zend_snapshot_zval(sb, c->value);
							if (c->doc_comment) {
								c->doc_comment = zend_snapshot_str(sb, c->doc_comment);
							}
						}
						Z_PTR_P(zv) = c;
					} ZEND_HASH_FOREACH_END();
				}
				if (mutable_data->backed_enum_table) {
					mutable_data->backed_enum_table = zend_snapshot_array(sb, mutable_data->backed_enum_table);
				}
			}
		}

		key = zend_snapshot_str(sb, key);
		zend_hash_add(&s->class_table, key, zv);
	} ZEND_HASH_FOREACH_END();

	zend_hash_init(&s->function_table, EG(function_table)->nNumUsed - EG(persistent_functions_count), NULL, NULL, 1);
	zend_hash_real_init_mixed(&s->function_table);
	ZEND_HASH_FOREACH_STR_KEY_VAL_FROM(EG(function_table), key, zv, EG(persistent_functions_count)) {
		zend_function *func = Z_FUNC_P(zv);
		if (!ZEND_USER_CODE(func->type)) {
			zend_throw_error(NULL, "Can not snapshot function loaded by dl(): %s",
					ZSTR_VAL(func->common.function_name));
			RETURN_THROWS();
		}
		if (!(func->common.fn_flags & ZEND_ACC_IMMUTABLE)) {
			zend_throw_error(NULL,
					"Can not snapshot non-cached function %s (TODO)",
					ZSTR_VAL(func->common.function_name));
			RETURN_THROWS();
		}
		key = zend_snapshot_str(sb, key);
		zend_hash_add(&s->function_table, key, zv);
	} ZEND_HASH_FOREACH_END();

	zend_objects_store *objects_store = &EG(objects_store);
	s->objects_count = 0;
	for (uint32_t i = 1; i < objects_store->top; i++) {
		zend_object *obj = objects_store->object_buckets[i];
		if (IS_OBJ_VALID(obj)) {
			s->objects_count++;
		}
	}
	if (s->objects_count > 0) {
		s->objects = malloc(s->objects_count * sizeof(zend_object*));
		zend_object **obj_dest = s->objects;
		for (uint32_t i = 1; i < objects_store->top; i++) {
			zend_object *obj = objects_store->object_buckets[i];
			if (IS_OBJ_VALID(obj)) {
				obj = zend_snapshot_xlat_get_entry(sb, obj);
				ZEND_ASSERT(obj);
				*obj_dest = obj;
				obj_dest++;
			}
		}
	}

	EG(snapshot) = s;

	zend_mm_chunk *chunk = zend_mm_get_chunk_list(snapshot_heap);
	s->chunks_count = zend_mm_get_chunks_count(snapshot_heap);
	s->chunks = malloc(s->chunks_count * sizeof(void*));
	s->copy = malloc(s->chunks_count * ZEND_MM_CHUNK_SIZE);
	void **chunk_dest = s->chunks;
	void *copy_dest = s->copy;
	do {
		copy_dest = mempcpy(copy_dest, chunk, ZEND_MM_CHUNK_SIZE);
		*chunk_dest = chunk;
		chunk_dest++;
		chunk = zend_mm_get_next_chunk(snapshot_heap, chunk);
	} while (chunk);

	zend_snapshot_builder_dtor(sb);

	RETURN_NULL();
}
