/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 66bb22256116838bff490d86f41a65b5aeb5dd14 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_ContextManager_enterContext, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_ContextManager_exitContext, 0, 0, _IS_BOOL, 1)
	ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, e, Throwable, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_ResourceContextManager___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, resource, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_ResourceContextManager_enterContext arginfo_class_ContextManager_enterContext

#define arginfo_class_ResourceContextManager_exitContext arginfo_class_ContextManager_exitContext

ZEND_METHOD(ResourceContextManager, __construct);
ZEND_METHOD(ResourceContextManager, enterContext);
ZEND_METHOD(ResourceContextManager, exitContext);

static const zend_function_entry class_ContextManager_methods[] = {
	ZEND_RAW_FENTRY("enterContext", NULL, arginfo_class_ContextManager_enterContext, ZEND_ACC_PUBLIC|ZEND_ACC_ABSTRACT, NULL, NULL)
	ZEND_RAW_FENTRY("exitContext", NULL, arginfo_class_ContextManager_exitContext, ZEND_ACC_PUBLIC|ZEND_ACC_ABSTRACT, NULL, NULL)
	ZEND_FE_END
};

static const zend_function_entry class_ResourceContextManager_methods[] = {
	ZEND_ME(ResourceContextManager, __construct, arginfo_class_ResourceContextManager___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(ResourceContextManager, enterContext, arginfo_class_ResourceContextManager_enterContext, ZEND_ACC_PUBLIC)
	ZEND_ME(ResourceContextManager, exitContext, arginfo_class_ResourceContextManager_exitContext, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_ContextManager(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "ContextManager", class_ContextManager_methods);
	class_entry = zend_register_internal_interface(&ce);

	return class_entry;
}

static zend_class_entry *register_class_ResourceContextManager(zend_class_entry *class_entry_ContextManager)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "ResourceContextManager", class_ResourceContextManager_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);
	zend_class_implements(class_entry, 1, class_entry_ContextManager);

	zval property_resource_default_value;
	ZVAL_UNDEF(&property_resource_default_value);
	zend_declare_typed_property(class_entry, ZSTR_KNOWN(ZEND_STR_RESOURCE), &property_resource_default_value, ZEND_ACC_PRIVATE|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ANY));

	return class_entry;
}
