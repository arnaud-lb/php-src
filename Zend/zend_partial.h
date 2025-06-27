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
#ifndef ZEND_PARTIAL_H
#define ZEND_PARTIAL_H

#include "zend_types.h"
#include "zend_API.h"

BEGIN_EXTERN_C()

/* This macro depends on zend_closure structure layout */
#define ZEND_PARTIAL_OBJECT(func) \
	((zend_object*)((char*)(func) - XtOffsetOf(struct{uint32_t a; zend_function b;}, b) - sizeof(zval) - sizeof(zend_function) - sizeof(zend_object)))

typedef struct _zend_partial zend_partial;

void zend_partial_startup(void);

#define ZEND_APPLY_VARIADIC (1<<16)
#define ZEND_APPLY_UNDEF    (1<<17) /* Some arguments maybe undef, default value needs to be fetched */
#define ZEND_APPLY_BYREF    (1<<18) /* Some arguments should be sent by ref */

void zend_partial_create(zval *result, uint32_t info, zval *this_ptr, zend_function *function, uint32_t argc, zval *argv, zend_array *extra_named_params);

void zend_partial_bind(zval *result, zval *partial, zval *this_ptr, zend_class_entry *scope);

void zend_partial_args_check(zend_execute_data *call);

zend_function *zend_partial_get_trampoline(zend_object *object);

zend_result zend_partial_init_call(zend_execute_data *call);

bool zend_is_partial_function(zend_function *function);

END_EXTERN_C()

#endif
