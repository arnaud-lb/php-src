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
*/

#include "zend.h"
#include "zend_list.h"
#include "zend_API.h"
#include "zend_exceptions.h"
#include "zend_context_managers_arginfo.h"

ZEND_API zend_class_entry *zend_ce_context_manager = NULL;
ZEND_API zend_class_entry *zend_ce_resource_context_manager = NULL;

ZEND_MINIT_FUNCTION(context_managers)
{
	zend_ce_context_manager = register_class_ContextManager();
	zend_ce_resource_context_manager = register_class_ResourceContextManager(zend_ce_context_manager);

	return SUCCESS;
}

void zend_resource_context_manager_init(zend_object *obj, zend_resource *res)
{
	zval zres;
	ZVAL_RES(&zres, res);

	// TODO: reuse zend_update_property_num_checked() or similar
	zend_update_property(obj->ce, obj, "resource", strlen("resource"), &zres);
}

ZEND_METHOD(ResourceContextManager, __construct)
{
	zval *zres;
	zend_object *this = Z_OBJ_P(getThis());

	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_RESOURCE(zres);
	ZEND_PARSE_PARAMETERS_END();

	zend_resource_context_manager_init(this, Z_RES_P(zres));
}

ZEND_METHOD(ResourceContextManager, enterContext)
{
	zend_object *this = Z_OBJ_P(getThis());

	ZEND_PARSE_PARAMETERS_NONE();

	zval rv;
	zval *zres = zend_read_property(this->ce, this, "resource", strlen("resource"), false, &rv);

	if (Z_TYPE_P(zres) != IS_RESOURCE) {
		zend_throw_error(NULL, "%s object is not initialized", ZSTR_VAL(this->ce->name));
		RETURN_THROWS();
	}

	RETURN_COPY_DEREF(zres);
}

ZEND_METHOD(ResourceContextManager, exitContext)
{
	zend_object *this = Z_OBJ_P(getThis());
	zend_object *exception;

	ZEND_PARSE_PARAMETERS_START(0, 1);
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(exception, zend_ce_throwable);
	ZEND_PARSE_PARAMETERS_END();

	zval rv;
	zval *zres = zend_read_property(this->ce, this, "resource", strlen("resource"), false, &rv);

	if (Z_TYPE_P(zres) != IS_RESOURCE) {
		zend_throw_error(NULL, "%s object is not initialized", ZSTR_VAL(this->ce->name));
		RETURN_THROWS();
	}

	zend_resource *res = Z_RES_P(zres);

	if (res->type == -1) {
		return;
	}

	zend_resource_dtor(res);

	/* Do not swallow exception */
	RETURN_FALSE;
}
