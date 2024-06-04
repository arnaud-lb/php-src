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
   | Authors: Arnaud Le Blanc <arnaud.lb@gmail.com>                       |
   +----------------------------------------------------------------------+
*/

#ifndef ZEND_LAZY_OBJECT_H
#define ZEND_LAZY_OBJECT_H

#include "zend.h"

/* Serialization skips initialization */
#define ZEND_LAZY_OBJECT_SKIP_INITIALIZATION_ON_SERIALIZE   (1<<0)

/* Lazy object is a virtual proxy */
#define ZEND_LAZY_OBJECT_STRATEGY_VIRTUAL                   (1<<1)

/* Lazy object is a ghost object */
#define ZEND_LAZY_OBJECT_STRATEGY_GHOST                     (1<<2)

/* Lazy object is initialized (info.u is an instance) */
#define ZEND_LAZY_OBJECT_INITIALIZED                        (1<<3)

#define ZEND_LAZY_OBJECT_USER_FLAGS (                                       \
	ZEND_LAZY_OBJECT_SKIP_INITIALIZATION_ON_SERIALIZE                       \
)

#define ZEND_LAZY_OBJECT_STRATEGY_FLAGS (                                   \
		ZEND_LAZY_OBJECT_STRATEGY_VIRTUAL |                                 \
		ZEND_LAZY_OBJECT_STRATEGY_GHOST                                     \
)

/* Temporarily allow raw access to the lazy object without triggering
 * initialization */
#define ZEND_LAZY_OBJECT_PASSTHRU(zobj)                                     \
	do {                                                                    \
		zend_object *_zobj = (zobj);                                        \
		zend_lazy_object_passthru_begin(_zobj);                             \
		do

#define ZEND_LAZY_OBJECT_PASSTHRU_END()                                     \
		while (0);                                                          \
		zend_lazy_object_passthru_end(_zobj);                               \
	} while (0)

typedef uint8_t zend_lazy_object_flags_t;

typedef struct _zend_lazy_objects_store {
	/* object handle -> *zend_lazy_object_info */
	HashTable infos;
} zend_lazy_objects_store;

typedef struct _zend_fcall_info_cache zend_fcall_info_cache;

ZEND_API zend_object *zend_object_make_lazy(zend_object *obj,
		zend_class_entry *class_type, zend_fcall_info_cache *initializer,
		zend_lazy_object_flags_t flags);
ZEND_API zend_object *zend_lazy_object_init(zend_object *obj);
ZEND_API zend_object *zend_lazy_object_init_with(zend_object *obj, zend_fcall_info_cache *custom_initializer);

void zend_lazy_objects_init(zend_lazy_objects_store *store);
void zend_lazy_objects_destroy(zend_lazy_objects_store *store);
zend_fcall_info_cache* zend_lazy_object_get_initializer(zend_object *obj);
zend_object *zend_lazy_object_get_instance(zend_object *obj);
zend_lazy_object_flags_t zend_lazy_object_get_flags(zend_object *obj);
void zend_lazy_object_del_info(zend_object *obj);
zend_object *zend_lazy_object_clone(zend_object *old_obj);
HashTable *zend_lazy_object_debug_info(zend_object *object, int *is_temp);
bool zend_lazy_object_decr_lazy_props(zend_object *obj);
void zend_lazy_object_realize(zend_object *obj);
void zend_lazy_object_passthru_begin(zend_object *obj);
void zend_lazy_object_passthru_end(zend_object *obj);
bool zend_lazy_object_passthru(zend_object *obj);

static zend_always_inline bool zend_object_is_lazy(zend_object *obj)
{
	return (OBJ_EXTRA_FLAGS(obj) & (IS_OBJ_LAZY | IS_OBJ_VIRTUAL_LAZY));
}

static zend_always_inline bool zend_object_is_virtual(zend_object *obj)
{
	return (OBJ_EXTRA_FLAGS(obj) & IS_OBJ_VIRTUAL_LAZY);
}

static zend_always_inline bool zend_lazy_object_initialized(zend_object *obj)
{
	return !(OBJ_EXTRA_FLAGS(obj) & IS_OBJ_LAZY);
}

/* True if accessing a lazy prop on obj mandates a call to
 * zend_lazy_object_init(),
 * False if obj is not lazy or passthru is enabled */
static zend_always_inline bool zend_lazy_object_must_init(zend_object *obj)
{
	if (EXPECTED(!zend_object_is_lazy(obj))) {
		return false;
	}

	if (zend_object_is_virtual(obj) && zend_lazy_object_initialized(obj)) {
		return true;
	}

	return !zend_lazy_object_passthru(obj);
}

static inline bool zend_lazy_object_initialize_on_serialize(zend_object *obj)
{
	return !(zend_lazy_object_get_flags(obj) & ZEND_LAZY_OBJECT_SKIP_INITIALIZATION_ON_SERIALIZE);
}

#endif /* ZEND_LAZY_OBJECT_H */
