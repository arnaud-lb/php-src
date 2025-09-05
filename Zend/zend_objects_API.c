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
   | Authors: Andi Gutmans <andi@php.net>                                 |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Dmitry Stogov <dmitry@php.net>                              |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_globals.h"
#include "zend_objects.h"
#include "zend_portability.h"
#include "zend_variables.h"
#include "zend_API.h"
#include "zend_objects_API.h"
#include "zend_fibers.h"

#ifdef USE_LIBGC
# define USE_FINALIZERS 1
# define ALL_FINALIZERS 1
# define USE_LONG_LINKS 1

# if USE_FINALIZERS
static void zend_object_dtor_finalizer(void *ptr, void *data);
static void zend_object_free_finalizer(void *ptr, void *data);
# endif
#endif

static void ZEND_FASTCALL zend_objects_store_del_ex(zend_object *object, bool from_finalizer);

ZEND_API void ZEND_FASTCALL zend_objects_store_init(zend_objects_store *objects, uint32_t init_size)
{
	objects->object_buckets = (zend_object **) emalloc(init_size * sizeof(zend_object*));
	objects->top = 1; /* Skip 0 so that handles are true */
	objects->size = init_size;
	objects->free_list_head = -1;
	memset(&objects->object_buckets[0], 0, sizeof(zend_object*));
}

ZEND_API void ZEND_FASTCALL zend_objects_store_destroy(zend_objects_store *objects)
{
	efree(objects->object_buckets);
	objects->object_buckets = NULL;
}

#ifdef USE_LIBGC
void ZEND_FASTCALL zend_objects_store_set_weakref_finalizer(zend_object *object)
{
# if USE_FINALIZERS
	if ((object->handlers->dtor_obj != zend_objects_destroy_object
			|| object->ce->destructor)
			&& !(OBJ_FLAGS(object) & IS_OBJ_DESTRUCTOR_CALLED)
	) {
		/* Do nothing: object already has a finalizer */
#  if ZEND_DEBUG
		/* Check that object already has a finalizer */
		GC_finalization_proc ofn;
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_dtor_finalizer, (void*)GC_HIDE_POINTER(object), &ofn, NULL);
		ZEND_ASSERT(ofn == zend_object_dtor_finalizer);
#  endif
	} else if (ALL_FINALIZERS
			|| object->handlers->free_obj != zend_object_std_dtor) {
		/* Do nothing: object already has a finalizer */
#  if ZEND_DEBUG
		/* Check that object already has a finalizer */
		GC_finalization_proc ofn;
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_free_finalizer, (void*)GC_HIDE_POINTER(object), &ofn, NULL);
		ZEND_ASSERT(ofn == zend_object_dtor_finalizer);
#  endif
	} else {
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_free_finalizer, (void*)GC_HIDE_POINTER(object), NULL, NULL);
		zend_object *placeholder = EG(objects_store).object_buckets[object->handle];
		if (IS_OBJ_PLACEHOLDER(placeholder)) {
			EG(objects_store).object_buckets[object->handle] = (zend_object*)MASK_PTR(object);
		}
	}
# endif
}
#endif

ZEND_API void ZEND_FASTCALL zend_objects_store_call_destructors(zend_objects_store *objects)
{
	EG(flags) |= EG_FLAGS_OBJECT_STORE_NO_REUSE;
	if (objects->top > 1) {
		uint32_t i;
		for (i = 1; i < objects->top; i++) {
			zend_object *obj = (zend_object*)UNMASK_PTR(objects->object_buckets[i]);
			if (IS_OBJ_VALID(obj)) {
#ifdef USE_LIBGC
# if USE_FINALIZERS
				GC_finalization_proc finalizer;
				if (ALL_FINALIZERS
						|| obj->handlers->free_obj != zend_object_std_dtor
						|| (OBJ_FLAGS(obj) & IS_OBJ_WEAKLY_REFERENCED)) {
					finalizer = zend_object_free_finalizer;
				} else {
					finalizer = NULL; /* unregister */
				}
				GC_REGISTER_FINALIZER_NO_ORDER((char*)obj - obj->handlers->offset,
						finalizer, (void*)GC_HIDE_POINTER(obj), NULL, NULL);
# endif
#endif

				if (!(OBJ_FLAGS(obj) & IS_OBJ_DESTRUCTOR_CALLED)) {
					GC_ADD_FLAGS(obj, IS_OBJ_DESTRUCTOR_CALLED);

					if (obj->handlers->dtor_obj != zend_objects_destroy_object
							|| obj->ce->destructor) {
						GC_ADDREF_OBJ(obj);
						obj->handlers->dtor_obj(obj);
						GC_DELREF_OBJ(obj);
					}
				}
			}
		}
	}
}

ZEND_API void ZEND_FASTCALL zend_objects_store_mark_destructed(zend_objects_store *objects)
{
	if (objects->object_buckets && objects->top > 1) {
		zend_object **obj_ptr = objects->object_buckets + 1;
		zend_object **end = objects->object_buckets + objects->top;

		do {
			zend_object *obj = (zend_object*)UNMASK_PTR(*obj_ptr);

			if (IS_OBJ_VALID(obj)) {
				GC_ADD_FLAGS(obj, IS_OBJ_DESTRUCTOR_CALLED);
#ifdef USE_LIBGC
# if USE_FINALIZERS
				GC_finalization_proc finalizer = NULL;
				if (ALL_FINALIZERS
						|| obj->handlers->free_obj != zend_object_std_dtor
						|| (OBJ_FLAGS(obj) & IS_OBJ_WEAKLY_REFERENCED)) {
					finalizer = zend_object_free_finalizer;
				} else {
					finalizer = NULL; /* unregister */
				}
				GC_REGISTER_FINALIZER_NO_ORDER((char*)obj - obj->handlers->offset,
						finalizer, (void*)GC_HIDE_POINTER(obj), NULL, NULL);
# endif
#endif
			}
			obj_ptr++;
		} while (obj_ptr != end);
	}
}

ZEND_API void ZEND_FASTCALL zend_objects_store_free_object_storage(zend_objects_store *objects, bool fast_shutdown)
{
	zend_object **obj_ptr, **end, *obj;

	if (objects->top <= 1) {
		return;
	}

	/* Free object contents, but don't free objects themselves, so they show up as leaks.
	 * Also add a ref to all objects, so the object can't be freed by something else later. */
	end = objects->object_buckets + 1;
	obj_ptr = objects->object_buckets + objects->top;

	if (fast_shutdown) {
		do {
			obj_ptr--;
			obj = (zend_object*)UNMASK_PTR(*obj_ptr);
			if (IS_OBJ_VALID(obj)) {
				if (!(OBJ_FLAGS(obj) & IS_OBJ_FREE_CALLED)) {
					GC_ADD_FLAGS(obj, IS_OBJ_FREE_CALLED);
					if (obj->handlers->free_obj != zend_object_std_dtor
					 || (OBJ_FLAGS(obj) & IS_OBJ_WEAKLY_REFERENCED)
					) {
						GC_ADDREF_OBJ(obj);
						obj->handlers->free_obj(obj);
#ifdef USE_LIBGC
# if USE_FINALIZERS
						GC_REGISTER_FINALIZER_NO_ORDER((char*)obj - obj->handlers->offset,
								NULL /* Unregister finalizer */, NULL, NULL, NULL);
# endif
#endif
					}
				}
			}
		} while (obj_ptr != end);
	} else {
		do {
			obj_ptr--;
			obj = (zend_object*)UNMASK_PTR(*obj_ptr);
			if (IS_OBJ_VALID(obj)) {
				if (!(OBJ_FLAGS(obj) & IS_OBJ_FREE_CALLED)) {
					GC_ADD_FLAGS(obj, IS_OBJ_FREE_CALLED);
					GC_ADDREF_OBJ(obj);
					obj->handlers->free_obj(obj);
#ifdef USE_LIBGC
# if USE_FINALIZERS
					GC_REGISTER_FINALIZER_NO_ORDER((char*)obj - obj->handlers->offset,
							NULL /* Unregister finalizer */, NULL, NULL, NULL);
# endif
#endif
				}
			}
		} while (obj_ptr != end);
	}
}


/* Store objects API */
static ZEND_COLD zend_never_inline void ZEND_FASTCALL zend_objects_store_put_cold(zend_object *object, void *bucket_value)
{
	int handle;
	uint32_t new_size = 2 * EG(objects_store).size;
#ifdef USE_LIBGC
# if USE_LONG_LINKS
	uint32_t free_list_head = -1;

	if (!(EG(flags) & EG_FLAGS_OBJECT_STORE_NO_REUSE)) {
		ZEND_ASSERT(EG(objects_store).free_list_head == -1);
		for (uint32_t i = 1, l = EG(objects_store).size; i < l; i++) {
			if (EG(objects_store).object_buckets[i] == NULL) {
				SET_OBJ_BUCKET_NUMBER(EG(objects_store).object_buckets[i], free_list_head);
				free_list_head = i;
			}
		}
		EG(objects_store).free_list_head = free_list_head;
	}

	if (free_list_head != -1) {
		handle = free_list_head;
		EG(objects_store).free_list_head = GET_OBJ_BUCKET_NUMBER(EG(objects_store).object_buckets[handle]);
		object->handle = handle;
		EG(objects_store).object_buckets[handle] = (zend_object*)MASK_PTR(bucket_value);
	} else
# endif
#endif
	{
#ifdef USE_LIBGC
#if USE_LONG_LINKS
		zend_object **orig_buckets = EG(objects_store).object_buckets;
# endif
#endif
		EG(objects_store).object_buckets = (zend_object **) erealloc(EG(objects_store).object_buckets, new_size * sizeof(zend_object*));

#ifdef USE_LIBGC
# if USE_LONG_LINKS
		/* Move links from old buckets to new ones */
		if (orig_buckets != EG(objects_store).object_buckets) {
			for (uint32_t i = 0, l = EG(objects_store).size; i < l; i++) {
				if (IS_OBJ_VALID(EG(objects_store).object_buckets[i]) || IS_OBJ_PLACEHOLDER(EG(objects_store).object_buckets[i])) {
					if (GC_move_long_link((void**)&orig_buckets[i],
								(void**)&EG(objects_store).object_buckets[i]) != GC_SUCCESS) {
						ZEND_UNREACHABLE();
					}
				}
			}
		}
# endif
#endif

		/* Assign size after realloc, in case it fails */
		EG(objects_store).size = new_size;
		handle = EG(objects_store).top++;

	}
	object->handle = handle;
	EG(objects_store).object_buckets[handle] = (zend_object*)MASK_PTR(bucket_value);
#ifdef USE_LIBGC
# if USE_LONG_LINKS
	if (IS_OBJ_PLACEHOLDER(bucket_value)) {
		if (GC_REGISTER_LONG_LINK((void**)&EG(objects_store).object_buckets[handle],
				(char*)object - object->handlers->offset) != GC_SUCCESS) {
			ZEND_UNREACHABLE();
		}
	}
# endif
#endif
}

#ifdef USE_LIBGC
# if USE_FINALIZERS
static void zend_object_free_finalizer(void *ptr, void *data)
{
	zend_objects_store_del_ex((zend_object*)GC_REVEAL_POINTER(data), true);
}

static void zend_object_dtor_finalizer(void *ptr, void *data)
{
	zend_object *object = (zend_object*)GC_REVEAL_POINTER(data);

	if (!(OBJ_FLAGS(object) & IS_OBJ_DESTRUCTOR_CALLED)) {
		GC_ADD_FLAGS(object, IS_OBJ_DESTRUCTOR_CALLED);

		if (object->handlers->dtor_obj != zend_objects_destroy_object
				|| object->ce->destructor) {
			object->handlers->dtor_obj(object);
		}
	}

	if (ALL_FINALIZERS
			|| object->handlers->free_obj != zend_object_std_dtor
			|| (OBJ_FLAGS(object) & IS_OBJ_WEAKLY_REFERENCED)) {
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_free_finalizer, (void*)GC_HIDE_POINTER(object), NULL, NULL);
	}
}
# endif
#endif

ZEND_API void ZEND_FASTCALL zend_objects_store_put(zend_object *object)
{
	int handle;
	void *bucket_value = object;

#ifdef USE_LIBGC
# if USE_FINALIZERS
	if (object->handlers->dtor_obj != zend_objects_destroy_object
			|| object->ce->destructor) {
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_dtor_finalizer, (void*)GC_HIDE_POINTER(object), NULL, NULL);
	} else if (ALL_FINALIZERS
			|| object->handlers->free_obj != zend_object_std_dtor) {
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				zend_object_free_finalizer, (void*)GC_HIDE_POINTER(object), NULL, NULL);
	} else
# endif
	{
		bucket_value = SET_OBJ_PLACEHOLDER(SET_OBJ_INVALID(object));
	}
#endif

	/* When in shutdown sequence - do not reuse previously freed handles, to make sure
	 * the dtors for newly created objects are called in zend_objects_store_call_destructors() loop
	 */
	if (EG(objects_store).free_list_head != -1 && EXPECTED(!(EG(flags) & EG_FLAGS_OBJECT_STORE_NO_REUSE))) {
		handle = EG(objects_store).free_list_head;
		EG(objects_store).free_list_head = GET_OBJ_BUCKET_NUMBER(EG(objects_store).object_buckets[handle]);
	} else if (UNEXPECTED(EG(objects_store).top == EG(objects_store).size)) {
		zend_objects_store_put_cold(object, bucket_value);
		return;
	} else {
		handle = EG(objects_store).top++;
	}
	object->handle = handle;
	EG(objects_store).object_buckets[handle] = (zend_object*) MASK_PTR(bucket_value);
#ifdef USE_LIBGC
# if USE_LONG_LINKS
	if (IS_OBJ_PLACEHOLDER(bucket_value)) {
		if (GC_REGISTER_LONG_LINK((void**)&EG(objects_store).object_buckets[handle],
				(char*)object - object->handlers->offset) != GC_SUCCESS) {
			ZEND_UNREACHABLE();
		}
	}
# endif
#endif
}

static void ZEND_FASTCALL zend_objects_store_del_ex(zend_object *object, bool from_finalizer) /* {{{ */
{
	uint32_t handle = object->handle;

	ZEND_ASSERT(handle != 0);

	if (!IS_OBJ_VALID(EG(objects_store).object_buckets[handle])) {
		// Object is being deleted already
		return;
	}

	EG(objects_store).object_buckets[handle] = (zend_object*)MASK_PTR(SET_OBJ_INVALID(object));

	/* No need to unregister link explicitly if we also don't add to free list bellow
#ifdef USE_LIBGC
# if USE_LONG_LINKS
	if (GC_unregister_long_link((void**)&EG(objects_store).object_buckets[handle]) != 1) {
		ZEND_UNREACHABLE();
	}
# endif
#endif
	*/

	/*	Make sure we hold a reference count during the destructor call
		otherwise, when the destructor ends the storage might be freed
		when the refcount reaches 0 a second time
	 */
	if (!(OBJ_FLAGS(object) & IS_OBJ_DESTRUCTOR_CALLED)) {
		GC_ADD_FLAGS(object, IS_OBJ_DESTRUCTOR_CALLED);

		if (object->handlers->dtor_obj != zend_objects_destroy_object
				|| object->ce->destructor) {
			GC_SET_REFCOUNT(object, 1);
			object->handlers->dtor_obj(object);
			GC_DELREF_OBJ(object);
		}
	}

	if (!(IS_OBJECT_EX & (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) || GC_REFCOUNT(object) == 0) {
		void *ptr;

		ZEND_ASSERT(EG(objects_store).object_buckets != NULL);
		if (!(OBJ_FLAGS(object) & IS_OBJ_FREE_CALLED)) {
			GC_ADD_FLAGS(object, IS_OBJ_FREE_CALLED);
			object->handlers->free_obj(object);
		}
		ptr = ((char*)object) - object->handlers->offset;
		GC_REMOVE_FROM_BUFFER(object);
		efree(ptr);
#if !defined(USE_LIBGC) || !defined(USE_LONG_LINKS)
		ZEND_OBJECTS_STORE_ADD_TO_FREE_LIST(handle);
#endif
	}

#ifdef USE_LIBGC
# if USE_FINALIZERS
	if (!from_finalizer) {
		GC_REGISTER_FINALIZER_NO_ORDER((char*)object - object->handlers->offset,
				NULL /* Unregister */, NULL, NULL, NULL);
	}
# endif
#endif
}
/* }}} */

ZEND_API void ZEND_FASTCALL zend_objects_store_del(zend_object *object) /* {{{ */
{
	zend_objects_store_del_ex(object, false);
}

ZEND_API ZEND_COLD zend_property_info *zend_get_property_info_for_slot_slow(zend_object *obj, zval *slot)
{
	uintptr_t offset = OBJ_PROP_SLOT_TO_OFFSET(obj, slot);
	zend_property_info *prop_info;
	ZEND_HASH_MAP_FOREACH_PTR(&obj->ce->properties_info, prop_info) {
		if (prop_info->offset == offset) {
			return prop_info;
		}
	} ZEND_HASH_FOREACH_END();
	return NULL;
}
