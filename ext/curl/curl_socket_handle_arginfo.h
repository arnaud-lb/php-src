/* This is a generated file, edit curl_socket_handle.stub.php instead.
 * Stub hash: 1aefd753a56696f9723cdb95277da5e86514a363 */

static zend_class_entry *register_class_Io_Curl_SocketWeakHandle(zend_class_entry *class_entry_Io_Poll_Handle)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Io\\Curl", "SocketWeakHandle", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL|ZEND_ACC_NO_DYNAMIC_PROPERTIES|ZEND_ACC_NOT_SERIALIZABLE);
	zend_class_implements(class_entry, 1, class_entry_Io_Poll_Handle);

	return class_entry;
}
