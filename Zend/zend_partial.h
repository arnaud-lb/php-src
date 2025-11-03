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

void zend_partial_startup(void);

void zend_partial_create(zval *result, zval *this_ptr, zend_function *function,
		uint32_t argc, zval *argv, zend_array *extra_named_params,
		const zend_array *named_positions,
		const zend_op_array *declaring_op_array,
		const zend_op *declaring_opline, void **cache_slot);

void zend_partial_args_check(zend_execute_data *call);

END_EXTERN_C()

#endif
