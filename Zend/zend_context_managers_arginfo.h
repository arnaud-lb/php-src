/* This is a generated file, edit zend_context_managers.stub.php instead.
 * Stub hash: 08f0bd1ae7167bc76478418374cd21c826deba41 */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_ContextManager_getContextManager, 0, 0, Generator, 0)
ZEND_END_ARG_INFO()


static const zend_function_entry class_ContextManager_methods[] = {
	ZEND_RAW_FENTRY("getContextManager", NULL, arginfo_class_ContextManager_getContextManager, ZEND_ACC_PUBLIC|ZEND_ACC_ABSTRACT, NULL, NULL)
	ZEND_FE_END
};

static zend_class_entry *register_class_ContextManager(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "ContextManager", class_ContextManager_methods);
	class_entry = zend_register_internal_interface(&ce);

	return class_entry;
}
