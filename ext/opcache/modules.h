
#ifndef ZEND_MODULES_H
#define ZEND_MODULES_H

#include "Zend/zend_types.h"
#include "Zend/zend_compile.h"
#include "ZendAccelerator.h"

typedef struct _zend_persistent_user_module {
	zend_user_module                      module;
	zend_persistent_script               *script;
	uint32_t                              num_dependencies;
	struct _zend_persistent_user_module **dependencies;
} zend_persistent_user_module;

ZEND_API void zend_require_user_module(zend_string *module_path);

#endif /* ZEND_MODULES_H */
