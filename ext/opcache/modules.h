
#ifndef ZEND_MODULES_H
#define ZEND_MODULES_H

#include "Zend/zend_types.h"
#include "Zend/zend_compile.h"
#include "ZendAccelerator.h"

// Not using zend_value.ptr as it may be too small on x32
#define ZUM_IS_FILE_ENTRY(zv)       ((bool)((*(uintptr_t*)&(zv)->value) & 1))
#define ZUM_FILE_ENTRY(zv, mtime)                               \
	do {                                                        \
		((*(uintptr_t*)&(zv)->value) = (((mtime) << 1) | 1));   \
		Z_TYPE_INFO_P(zv) = IS_PTR;                             \
	} while (0)
#define ZUM_FILE_ENTRY_MTIME(zv)    ((accel_time_t)((*(uintptr_t*)&(zv)->value) >> 1))
#define ZUM_DIR_ENTRY(zv)           ((zend_user_module_dir_cache*)Z_PTR_P(zv))

typedef struct _zend_user_module_dir_cache {
	accel_time_t mtime;
	HashTable entries;
} zend_user_module_dir_cache;

typedef struct _zend_persistent_user_module {
	zend_user_module                      module;
	zend_persistent_script               *script;
} zend_persistent_user_module;

ZEND_API void zend_require_user_module(zend_string *module_path);

#endif /* ZEND_MODULES_H */
