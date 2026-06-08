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
#include "zend_enum.h"
#include "ext/standard/file.h"
#include "ext/standard/io_poll.h"
#include "ext/standard/io_hooks.h"
#include "io_hooks_arginfo.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#elif HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

static zend_class_entry *php_io_hooks_poll_info_ce;
PHPAPI zend_class_entry *php_io_hooks_poll_result_ce;
static zend_class_entry *php_io_hooks_hooks_ce;

static void php_pollfd_events_to_io_poll_events(zend_array *dest, int events)
{
	zval zv;

	if (events & POLLIN) {
		ZVAL_OBJ_COPY(&zv, zend_enum_get_case_cstr(php_io_poll_event_class_entry, "Read"));
		zend_hash_next_index_insert(dest, &zv);
	}
	if (events & POLLOUT) {
		ZVAL_OBJ_COPY(&zv, zend_enum_get_case_cstr(php_io_poll_event_class_entry, "Write"));
		zend_hash_next_index_insert(dest, &zv);
	}
	if (events & POLLERR) {
		ZVAL_OBJ_COPY(&zv, zend_enum_get_case_cstr(php_io_poll_event_class_entry, "Error"));
		zend_hash_next_index_insert(dest, &zv);
	}
	if (events & POLLHUP) {
		ZVAL_OBJ_COPY(&zv, zend_enum_get_case_cstr(php_io_poll_event_class_entry, "HangUp"));
		zend_hash_next_index_insert(dest, &zv);
	}
}

PHPAPI zend_object *php_io_hooks_poll_stream(php_stream *stream, int events, const struct timeval *timeout)
{
	uint32_t orig_no_fclose = stream->flags & PHP_STREAM_FLAG_NO_FCLOSE;
	stream->flags |= PHP_STREAM_FLAG_NO_FCLOSE;

	// TODO: optimize poll_info object creation+init. Maybe reuse it too (e.g. if RC=1)
	zval poll_info;
	object_init_ex(&poll_info, php_io_hooks_poll_info_ce);

	zval handle;
	php_stream_poll_handle_from_stream(&handle, stream);
	zend_update_property(php_io_hooks_poll_info_ce, Z_OBJ(poll_info),
			"handle", sizeof("handle") - 1, &handle);
	zval_ptr_dtor(&handle);

	zval events_arr;
	array_init(&events_arr);
	php_pollfd_events_to_io_poll_events(Z_ARRVAL(events_arr), events);
	zend_update_property(php_io_hooks_poll_info_ce, Z_OBJ(poll_info),
			"events", sizeof("events") - 1, &events_arr);
	zval_ptr_dtor(&events_arr);

	zend_long timeout_ms;
	if (timeout == NULL) {
		timeout_ms = -1;
	} else {
		timeout_ms = (zend_long)timeout->tv_sec * 1000 + (zend_long)timeout->tv_usec / 1000;
	}
	zend_update_property_long(php_io_hooks_poll_info_ce, Z_OBJ(poll_info),
			"timeout_ms", sizeof("timeout_ms") - 1, timeout_ms);

	zval retval;
	ZVAL_UNDEF(&retval);
	zend_call_known_fcc(&FG(io_hooks_poll_fcc), &retval, 1, &poll_info, NULL);
	zval_ptr_dtor(&poll_info);

	if (EG(exception)) {
		goto return_error;
	}

	if (UNEXPECTED(Z_TYPE(retval) != IS_OBJECT || Z_OBJCE(retval) != php_io_hooks_poll_result_ce)) {
		zend_type_error("%s::poll() must return %s, %s returned",
				ZSTR_VAL(php_io_hooks_hooks_ce->name),
				ZSTR_VAL(php_io_hooks_poll_result_ce->name),
				zend_zval_type_name(&retval));
		goto return_error;
	}

	stream->flags &= ~PHP_STREAM_FLAG_NO_FCLOSE;
	stream->flags |= orig_no_fclose;

	return Z_OBJ(retval);

return_error:
	ZEND_ASSERT(EG(exception));
	zval_ptr_dtor(&retval);

	stream->flags &= ~PHP_STREAM_FLAG_NO_FCLOSE;
	stream->flags |= orig_no_fclose;

	return NULL;
}

PHP_FUNCTION(Io_Hooks_set_hooks)
{
	zval *hooks_obj = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS_OR_NULL(hooks_obj, php_io_hooks_hooks_ce)
	ZEND_PARSE_PARAMETERS_END();

	if (!Z_ISUNDEF(FG(io_hooks))) {
		ZVAL_COPY(return_value, &FG(io_hooks));
		zval_ptr_dtor(&FG(io_hooks));
		zend_fcc_dtor(&FG(io_hooks_poll_fcc));
	}

	if (hooks_obj == NULL || Z_TYPE_P(hooks_obj) == IS_NULL) {
		ZVAL_UNDEF(&FG(io_hooks));
		return;
	}

	ZVAL_COPY(&FG(io_hooks), hooks_obj);

	zend_string *method_name = zend_string_init("poll", sizeof("poll") - 1, false);
	zend_object *obj = Z_OBJ_P(hooks_obj);
	zend_function *fn = obj->handlers->get_method(&obj, method_name, NULL);
	zend_string_release(method_name);
	ZEND_ASSERT(fn != NULL);

	FG(io_hooks_poll_fcc) = (zend_fcall_info_cache){
		.function_handler = fn,
		.object = obj,
		.called_scope = obj->ce,
	};
	GC_ADDREF(obj);
}

PHP_MINIT_FUNCTION(io_hooks)
{
	php_io_hooks_poll_info_ce = register_class_Io_Hooks_PollInfo();
	php_io_hooks_poll_result_ce = register_class_Io_Hooks_PollResult();
	php_io_hooks_hooks_ce = register_class_Io_Hooks_Hooks();

	zend_register_functions(NULL, ext_functions, NULL, type);

	return SUCCESS;
}
