/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 824b6cb4fad1e1ba2e28d41fdfbbea531a48196a */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dl_test_test1, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dl_test_test2, 0, 0, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, str, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()


ZEND_FUNCTION(dl_test_test1);
ZEND_FUNCTION(dl_test_test2);


static const zend_function_entry ext_functions[] = {
	ZEND_FE(dl_test_test1, arginfo_dl_test_test1)
	ZEND_FE(dl_test_test2, arginfo_dl_test_test2)
	ZEND_FE_END
};


static const zend_function_entry class_DlTestStringEnum_methods[] = {
	ZEND_FE_END
};

static zend_class_entry *register_class_DlTestStringEnum(void)
{
	zend_class_entry *class_entry = zend_register_internal_enum("DlTestStringEnum", IS_STRING, class_DlTestStringEnum_methods);

	zval enum_case_Foo_value;
	zend_string *enum_case_Foo_value_str = zend_string_init("Test1", sizeof("Test1") - 1, 1);
	ZVAL_STR(&enum_case_Foo_value, enum_case_Foo_value_str);
	zend_enum_add_case_cstr(class_entry, "Foo", &enum_case_Foo_value);

	zval enum_case_Bar_value;
	zend_string *enum_case_Bar_value_str = zend_string_init("Test2", sizeof("Test2") - 1, 1);
	ZVAL_STR(&enum_case_Bar_value, enum_case_Bar_value_str);
	zend_enum_add_case_cstr(class_entry, "Bar", &enum_case_Bar_value);

	zval enum_case_Baz_value;
	zend_string *enum_case_Baz_value_str = zend_string_init("Test2\\a", sizeof("Test2\\a") - 1, 1);
	ZVAL_STR(&enum_case_Baz_value, enum_case_Baz_value_str);
	zend_enum_add_case_cstr(class_entry, "Baz", &enum_case_Baz_value);

	zval enum_case_FortyTwo_value;
	zend_string *enum_case_FortyTwo_value_str = zend_string_init("42", sizeof("42") - 1, 1);
	ZVAL_STR(&enum_case_FortyTwo_value, enum_case_FortyTwo_value_str);
	zend_enum_add_case_cstr(class_entry, "FortyTwo", &enum_case_FortyTwo_value);

	return class_entry;
}
