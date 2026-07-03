/*
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
*/

#include "php.h"
#include "ext/standard/file.h"
#include "ext/standard/io_poll.h"
#include "ext/standard/io_hooks.h"
#include "io_hooks_arginfo.h"

PHPAPI zend_class_entry *php_io_hooks_poll_info_ce;
PHPAPI zend_class_entry *php_io_hooks_poll_result_ce;
static zend_class_entry *php_io_hooks_hooks_ce;

/* Data held by the PHP-object adapter registered via php_set_io_hooks() */
typedef struct _php_io_hooks_php_data {
	zend_fcall_info_cache poll_fcc;
	zend_fcall_info_cache pollMulti_fcc;
	zend_fcall_info_cache sleep_fcc;
} php_io_hooks_php_data;

/* -----------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Build a PollInfo PHP object from a php_io_hooks_poll_info. */
static void php_poll_info_to_zval(php_io_hooks_poll_info *info, zval *dest)
{
	object_init_ex(dest, php_io_hooks_poll_info_ce);

	zval handle_zv;
	ZVAL_OBJ_COPY(&handle_zv, info->handle);
	zend_update_property(php_io_hooks_poll_info_ce, Z_OBJ_P(dest),
		"handle", sizeof("handle") - 1, &handle_zv);
	zval_ptr_dtor(&handle_zv);

	zval events_zv;
	php_io_poll_events_to_event_enums(info->events, &events_zv);
	zend_update_property(php_io_hooks_poll_info_ce, Z_OBJ_P(dest),
		"events", sizeof("events") - 1, &events_zv);
	zval_ptr_dtor(&events_zv);

	zend_update_property_long(php_io_hooks_poll_info_ce, Z_OBJ_P(dest),
		"timeout_ms", sizeof("timeout_ms") - 1, info->timeout_ms);
}

/* Convert a PHP PollResult object to a php_io_hooks_poll_result. */
static php_io_hooks_poll_result *php_poll_result_from_zval(zval *result_zv)
{
	zend_object *result_obj = Z_OBJ_P(result_zv);
	zval rv;

	zval *handle_prop = zend_read_property(php_io_hooks_poll_result_ce, result_obj,
		"handle", sizeof("handle") - 1, /* silent */ 1, &rv);
	zval *events_prop = zend_read_property(php_io_hooks_poll_result_ce, result_obj,
		"events", sizeof("events") - 1, /* silent */ 1, &rv);
	zval *timeout_prop = zend_read_property(php_io_hooks_poll_result_ce, result_obj,
		"timeout", sizeof("timeout") - 1, /* silent */ 1, &rv);

	php_io_hooks_poll_result *result = emalloc(sizeof(php_io_hooks_poll_result));
	if (Z_TYPE_P(handle_prop) == IS_OBJECT) {
		GC_ADDREF(Z_OBJ_P(handle_prop));
		result->handle = Z_OBJ_P(handle_prop);
	} else {
		result->handle = NULL;
	}
	result->events = (Z_TYPE_P(events_prop) == IS_ARRAY)
		? php_io_poll_event_enums_to_events(events_prop) : 0;
	result->timeout = zend_is_true(timeout_prop);
	return result;
}

/* -----------------------------------------------------------------------
 * PHP adapter: hook implementations that forward to a PHP Hooks object
 * ---------------------------------------------------------------------- */

static php_io_hooks_poll_result *php_io_hooks_php_poll(void *data, php_io_hooks_poll_info *info)
{
	php_io_hooks_php_data *php_data = (php_io_hooks_php_data *)data;

	zval poll_info_zv;
	php_poll_info_to_zval(info, &poll_info_zv);

	zval retval;
	ZVAL_UNDEF(&retval);
	zend_call_known_fcc(&php_data->poll_fcc, &retval, 1, &poll_info_zv, NULL);
	zval_ptr_dtor(&poll_info_zv);

	if (EG(exception)) {
		zval_ptr_dtor(&retval);
		return NULL;
	}

	php_io_hooks_poll_result *result = php_poll_result_from_zval(&retval);
	zval_ptr_dtor(&retval);
	return result;
}

static php_io_hooks_poll_result *php_io_hooks_php_poll_multi(void *data, zend_long timeout_ms,
		uint32_t num_infos, php_io_hooks_poll_info *infos)
{
	php_io_hooks_php_data *php_data = (php_io_hooks_php_data *)data;

	zval *params = safe_emalloc(num_infos, sizeof(zval), sizeof(zval));

	if (timeout_ms < 0) {
		ZVAL_NULL(&params[0]);
	} else {
		ZVAL_LONG(&params[0], timeout_ms);
	}

	for (uint32_t i = 0; i < num_infos; i++) {
		php_poll_info_to_zval(&infos[i], &params[1 + i]);
	}

	zval retval;
	ZVAL_UNDEF(&retval);
	zend_call_known_fcc(&php_data->pollMulti_fcc, &retval, 1 + num_infos, params, NULL);

	for (uint32_t i = 0; i < 1 + num_infos; i++) {
		zval_ptr_dtor(&params[i]);
	}
	efree(params);

	if (EG(exception)) {
		zval_ptr_dtor(&retval);
		return NULL;
	}

	if (Z_TYPE(retval) == IS_NULL) {
		return NULL;
	}

	php_io_hooks_poll_result *result = php_poll_result_from_zval(&retval);
	zval_ptr_dtor(&retval);
	return result;
}

static void php_io_hooks_php_sleep(void *data, zend_long seconds, zend_long nanoseconds)
{
	php_io_hooks_php_data *php_data = (php_io_hooks_php_data *)data;
	zval params[2];
	ZVAL_LONG(&params[0], seconds);
	ZVAL_LONG(&params[1], nanoseconds);
	zend_call_known_fcc(&php_data->sleep_fcc, NULL, 2, params, NULL);
}

static void php_io_hooks_php_dtor(void *data)
{
	php_io_hooks_php_data *php_data = (php_io_hooks_php_data *)data;
	zend_fcc_dtor(&php_data->poll_fcc);
	zend_fcc_dtor(&php_data->pollMulti_fcc);
	zend_fcc_dtor(&php_data->sleep_fcc);
	efree(php_data);
}

static const php_io_hooks php_io_hooks_php_adapter = {
	.poll       = php_io_hooks_php_poll,
	.poll_multi = php_io_hooks_php_poll_multi,
	.sleep      = php_io_hooks_php_sleep,
	.dtor       = php_io_hooks_php_dtor,
};

/* -----------------------------------------------------------------------
 * PHP function: Io\Hooks\set_hooks()
 * ---------------------------------------------------------------------- */

PHP_FUNCTION(Io_Hooks_set_hooks)
{
	zval *hooks_obj = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS_OR_NULL(hooks_obj, php_io_hooks_hooks_ce)
	ZEND_PARSE_PARAMETERS_END();

	bool has_hooks = FG(io_hooks).poll || FG(io_hooks).poll_multi
	              || FG(io_hooks).sleep || FG(io_hooks).dtor;

	if (hooks_obj != NULL && Z_TYPE_P(hooks_obj) != IS_NULL) {
		if (has_hooks) {
			zend_throw_error(NULL,
				"I/O hooks are already set; call %s(null) to unregister them first",
				ZSTR_VAL(execute_data->func->common.function_name));
			RETURN_THROWS();
		}
	} else {
		php_set_io_hooks(NULL, 0, NULL);
		return;
	}

	zend_object *obj = Z_OBJ_P(hooks_obj);

	php_io_hooks_php_data *php_data = emalloc(sizeof(php_io_hooks_php_data));

	zend_string *poll_name = zend_string_init("poll", sizeof("poll") - 1, false);
	zend_function *poll_fn = obj->handlers->get_method(&obj, poll_name, NULL);
	zend_string_release(poll_name);
	ZEND_ASSERT(poll_fn != NULL);
	php_data->poll_fcc = (zend_fcall_info_cache){
		.function_handler = poll_fn,
		.object           = obj,
		.called_scope     = obj->ce,
	};
	GC_ADDREF(obj);

	zend_string *poll_multi_name = zend_string_init("pollMulti", sizeof("pollMulti") - 1, false);
	zend_function *poll_multi_fn = obj->handlers->get_method(&obj, poll_multi_name, NULL);
	zend_string_release(poll_multi_name);
	ZEND_ASSERT(poll_multi_fn != NULL);
	php_data->pollMulti_fcc = (zend_fcall_info_cache){
		.function_handler = poll_multi_fn,
		.object           = obj,
		.called_scope     = obj->ce,
	};
	GC_ADDREF(obj);

	zend_string *sleep_name = zend_string_init("sleep", sizeof("sleep") - 1, false);
	zend_function *sleep_fn = obj->handlers->get_method(&obj, sleep_name, NULL);
	zend_string_release(sleep_name);
	ZEND_ASSERT(sleep_fn != NULL);
	php_data->sleep_fcc = (zend_fcall_info_cache){
		.function_handler = sleep_fn,
		.object           = obj,
		.called_scope     = obj->ce,
	};
	GC_ADDREF(obj);

	php_set_io_hooks(&php_io_hooks_php_adapter, sizeof(php_io_hooks), php_data);
}

PHP_MINIT_FUNCTION(io_hooks)
{
	php_io_hooks_poll_info_ce = register_class_Io_Hooks_PollInfo();
	php_io_hooks_poll_result_ce = register_class_Io_Hooks_PollResult();
	php_io_hooks_hooks_ce = register_class_Io_Hooks_Hooks();

	zend_register_functions(NULL, ext_functions, NULL, type);

	return SUCCESS;
}
