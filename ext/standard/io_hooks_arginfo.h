/* This is a generated file, edit io_hooks.stub.php instead.
 * Stub hash: 86abce168edd66424958da19478649fc38f059e6 */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_Io_Hooks_set_hooks, 0, 1, Io\\Hooks\\Hooks, 1)
	ZEND_ARG_OBJ_INFO(0, hooks, Io\\Hooks\\Hooks, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Io_Hooks_Hooks_poll, 0, 1, Io\\Hooks\\PollResult, 0)
	ZEND_ARG_OBJ_INFO(0, info, Io\\Hooks\\PollInfo, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Io_Hooks_Hooks_poll_multi, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, timeout_ms, IS_LONG, 1)
	ZEND_ARG_VARIADIC_OBJ_INFO(0, info, Io\\Hooks\\PollInfo, 0)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(Io_Hooks_set_hooks);

static const zend_function_entry ext_functions[] = {
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Io\\Hooks", "set_hooks"), zif_Io_Hooks_set_hooks, arginfo_Io_Hooks_set_hooks, 0, NULL, NULL)
	ZEND_FE_END
};

static const zend_function_entry class_Io_Hooks_Hooks_methods[] = {
	ZEND_RAW_FENTRY("poll", NULL, arginfo_class_Io_Hooks_Hooks_poll, ZEND_ACC_PUBLIC|ZEND_ACC_ABSTRACT, NULL, NULL)
	ZEND_RAW_FENTRY("poll_multi", NULL, arginfo_class_Io_Hooks_Hooks_poll_multi, ZEND_ACC_PUBLIC|ZEND_ACC_ABSTRACT, NULL, NULL)
	ZEND_FE_END
};

static zend_class_entry *register_class_Io_Hooks_PollInfo(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Io\\Hooks", "PollInfo", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_handle_default_value;
	ZVAL_UNDEF(&property_handle_default_value);
	zend_string *property_handle_name = zend_string_init("handle", sizeof("handle") - 1, true);
	zend_string *property_handle_class_Io_Poll_Handle = zend_string_init("Io\\Poll\\Handle", sizeof("Io\\Poll\\Handle")-1, 1);
	zend_declare_typed_property(class_entry, property_handle_name, &property_handle_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_CLASS(property_handle_class_Io_Poll_Handle, 0, 0));
	zend_string_release_ex(property_handle_name, true);

	zval property_events_default_value;
	ZVAL_UNDEF(&property_events_default_value);
	zend_string *property_events_name = zend_string_init("events", sizeof("events") - 1, true);
	zend_declare_typed_property(class_entry, property_events_name, &property_events_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ARRAY));
	zend_string_release_ex(property_events_name, true);

	zval property_timeout_ms_default_value;
	ZVAL_UNDEF(&property_timeout_ms_default_value);
	zend_string *property_timeout_ms_name = zend_string_init("timeout_ms", sizeof("timeout_ms") - 1, true);
	zend_declare_typed_property(class_entry, property_timeout_ms_name, &property_timeout_ms_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_timeout_ms_name, true);

	return class_entry;
}

static zend_class_entry *register_class_Io_Hooks_PollResult(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Io\\Hooks", "PollResult", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_handle_default_value;
	ZVAL_UNDEF(&property_handle_default_value);
	zend_string *property_handle_name = zend_string_init("handle", sizeof("handle") - 1, true);
	zend_string *property_handle_class_Io_Poll_Handle = zend_string_init("Io\\Poll\\Handle", sizeof("Io\\Poll\\Handle")-1, 1);
	zend_declare_typed_property(class_entry, property_handle_name, &property_handle_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_CLASS(property_handle_class_Io_Poll_Handle, 0, 0));
	zend_string_release_ex(property_handle_name, true);

	zval property_events_default_value;
	ZVAL_UNDEF(&property_events_default_value);
	zend_string *property_events_name = zend_string_init("events", sizeof("events") - 1, true);
	zend_declare_typed_property(class_entry, property_events_name, &property_events_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ARRAY));
	zend_string_release_ex(property_events_name, true);

	zval property_timeout_default_value;
	ZVAL_UNDEF(&property_timeout_default_value);
	zend_string *property_timeout_name = zend_string_init("timeout", sizeof("timeout") - 1, true);
	zend_declare_typed_property(class_entry, property_timeout_name, &property_timeout_default_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_BOOL));
	zend_string_release_ex(property_timeout_name, true);

	return class_entry;
}

static zend_class_entry *register_class_Io_Hooks_Hooks(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Io\\Hooks", "Hooks", class_Io_Hooks_Hooks_methods);
	class_entry = zend_register_internal_interface(&ce);

	return class_entry;
}
