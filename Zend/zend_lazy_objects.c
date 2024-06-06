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

/* Lazy objects are standard zend_object whose initialization is defered until
 * one of their properties backing store is accessed for the first time.
 *
 * This is implemented by using the same fallback mechanism as __get and __set
 * magic methods that is triggered when an undefined property is accessed.
 *
 * Execution of methods or virtual property hooks do not trigger initialization.
 *
 * A lazy object can be created via the Reflection API. The user specifies an
 * initializer function that is called when initialization is required.
 *
 * There are two kinds of lazy objects:
 *
 * - Ghosts: These are initialized in-place by the initializer function
 * - Virtual: The initializer returns a new instance. After initialization,
 *   interaction with the virtual object are proxied to the instance.
 *
 * Internal objects are not supported.
 */

#include "zend_API.h"
#include "zend_execute.h"
#include "zend_hash.h"
#include "zend_operators.h"
#include "zend_types.h"
#include "zend_variables.h"
#include "zend_lazy_objects.h"
#include "zend_fibers.h"

/**
 * Information about each lazy object is stored outside of zend_objects, in
 * EG(lazy_objects_store). This information is deleted when an object becomes
 * non-lazy.
 */
typedef struct _zend_lazy_object_info {
	union {
		zend_fcall_info_cache initializer;
		zend_object *instance; /* For initialized virtual lazy objects */
	} u;
	zend_lazy_object_flags_t flags;
	int lazy_properties_count;
	int passthru; /* If non-zero, access to object must not trigger initialization */
} zend_lazy_object_info;

/* zend_hash dtor_func_t for zend_lazy_objects_store.infos */
static void zend_lazy_object_info_dtor_func(zval *pElement)
{
	zend_lazy_object_info *info = (zend_lazy_object_info*) Z_PTR_P(pElement);

	if (info->flags & ZEND_LAZY_OBJECT_INITIALIZED) {
		ZEND_ASSERT(info->flags & ZEND_LAZY_OBJECT_STRATEGY_VIRTUAL);
		zend_object_release(info->u.instance);
	} else {
		zend_fcc_dtor(&info->u.initializer);
	}

	efree(info);
}

void zend_lazy_objects_init(zend_lazy_objects_store *store)
{
	zend_hash_init(&store->infos, 8, NULL, zend_lazy_object_info_dtor_func, false);
}

void zend_lazy_objects_destroy(zend_lazy_objects_store *store)
{
	ZEND_ASSERT(zend_hash_num_elements(&store->infos) == 0 || CG(unclean_shutdown));
	zend_hash_destroy(&store->infos);
}

void zend_lazy_object_set_info(zend_object *obj, zend_lazy_object_info *info)
{
	ZEND_ASSERT(OBJ_EXTRA_FLAGS(obj) & IS_OBJ_LAZY);

	zval *zv = zend_hash_index_add_new_ptr(&EG(lazy_objects_store).infos, obj->handle, info);
	ZEND_ASSERT(zv);
}

zend_lazy_object_info* zend_lazy_object_get_info(zend_object *obj)
{
	ZEND_ASSERT(OBJ_EXTRA_FLAGS(obj) & (IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY));

	zend_lazy_object_info *info = zend_hash_index_find_ptr(&EG(lazy_objects_store).infos, obj->handle);
	ZEND_ASSERT(info);

	return info;
}

zend_fcall_info_cache* zend_lazy_object_get_initializer(zend_object *obj)
{
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	zend_lazy_object_info *info = zend_lazy_object_get_info(obj);

	ZEND_ASSERT(!(info->flags & ZEND_LAZY_OBJECT_INITIALIZED));

	return &info->u.initializer;
}

zend_object* zend_lazy_object_get_instance(zend_object *obj)
{
	ZEND_ASSERT(zend_lazy_object_initialized(obj));

	if (zend_object_is_virtual(obj)) {
		zend_lazy_object_info *info = zend_lazy_object_get_info(obj);

		ZEND_ASSERT(info->flags & ZEND_LAZY_OBJECT_INITIALIZED);

		return info->u.instance;
	}

	return obj;
}

zend_lazy_object_flags_t zend_lazy_object_get_flags(zend_object *obj)
{
	return zend_lazy_object_get_info(obj)->flags;
}

void zend_lazy_object_del_info(zend_object *obj)
{
	zend_result res = zend_hash_index_del(&EG(lazy_objects_store).infos, obj->handle);
	ZEND_ASSERT(res == SUCCESS);
}

bool zend_lazy_object_decr_lazy_props(zend_object *obj)
{
	ZEND_ASSERT(zend_object_is_lazy(obj));
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	zend_lazy_object_info *info = zend_lazy_object_get_info(obj);

	ZEND_ASSERT(info->lazy_properties_count > 0);

	info->lazy_properties_count--;

	return info->lazy_properties_count == 0;
}

void zend_lazy_object_passthru_begin(zend_object *obj)
{
	if (zend_object_is_lazy(obj) && !zend_lazy_object_initialized(obj)) {
		zend_lazy_object_get_info(obj)->passthru++;
	}
}

void zend_lazy_object_passthru_end(zend_object *obj)
{
	if (zend_object_is_lazy(obj) && !zend_lazy_object_initialized(obj)) {
		zend_lazy_object_get_info(obj)->passthru--;
	}
}

bool zend_lazy_object_passthru(zend_object *obj)
{
	ZEND_ASSERT(zend_object_is_lazy(obj));
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	return zend_lazy_object_get_info(obj)->passthru;
}

/**
 * Making objects lazy
 */

/* Make object 'obj' lazy. If 'obj' is NULL, create a lazy instance of
 * class 'class_type' */
ZEND_API zend_object *zend_object_make_lazy(zend_object *obj,
		zend_class_entry *class_type, zend_fcall_info_cache *initializer,
		zend_lazy_object_flags_t flags)
{
	ZEND_ASSERT(!(flags & ~(ZEND_LAZY_OBJECT_USER_FLAGS|ZEND_LAZY_OBJECT_STRATEGY_FLAGS)));
	ZEND_ASSERT((flags & ZEND_LAZY_OBJECT_STRATEGY_FLAGS) == ZEND_LAZY_OBJECT_STRATEGY_GHOST
			|| (flags & ZEND_LAZY_OBJECT_STRATEGY_FLAGS) == ZEND_LAZY_OBJECT_STRATEGY_VIRTUAL);

	ZEND_ASSERT(!obj || !zend_object_is_lazy(obj));
	ZEND_ASSERT(!obj || obj->ce == class_type);

	/* Internal classes are not supported */
	if (UNEXPECTED(class_type->type == ZEND_INTERNAL_CLASS && class_type != zend_standard_class_def)) {
		zend_throw_error(NULL, "Cannot make instance of internal class lazy: %s is internal", ZSTR_VAL(class_type->name));
		return NULL;
	}

	for (zend_class_entry *parent = class_type->parent; parent; parent = parent->parent) {
		if (UNEXPECTED(parent->type == ZEND_INTERNAL_CLASS && parent != zend_standard_class_def)) {
			zend_throw_error(NULL, "Cannot make instance of internal class lazy: %s inherits internal class %s",
				ZSTR_VAL(class_type->name), ZSTR_VAL(parent->name));
			return NULL;
		}
	}

	if (!obj) {
		zval zobj;
		if (UNEXPECTED(class_type->ce_flags & (ZEND_ACC_INTERFACE|ZEND_ACC_TRAIT|ZEND_ACC_IMPLICIT_ABSTRACT_CLASS|ZEND_ACC_EXPLICIT_ABSTRACT_CLASS|ZEND_ACC_ENUM))) {
			/* Slow path: use object_init_ex() */
			if (object_init_ex(&zobj, class_type) == FAILURE) {
				ZEND_ASSERT(EG(exception));
				return NULL;
			}
			obj = Z_OBJ(zobj);
		} else {
			obj = zend_objects_new(class_type);
		}

		for (int i = 0; i < obj->ce->default_properties_count; i++) {
			zval *p = &obj->properties_table[i];
			ZVAL_UNDEF(p);
			Z_PROP_FLAG_P(p) = IS_PROP_UNINIT | IS_PROP_LAZY;
		}
	} else {
		if (!(flags & ZEND_LAZY_OBJECT_SKIP_DESTRUCTOR)
				&& !(OBJ_FLAGS(obj) & IS_OBJ_DESTRUCTOR_CALLED)) {
			if (obj->handlers->dtor_obj != zend_objects_destroy_object
					|| obj->ce->destructor) {
				GC_ADD_FLAGS(obj, IS_OBJ_DESTRUCTOR_CALLED);
				zend_fiber_switch_block();
				GC_ADDREF(obj);
				obj->handlers->dtor_obj(obj);
				GC_DELREF(obj);
				zend_fiber_switch_unblock();
				if (EG(exception)) {
					return NULL;
				}
			}
		}

		GC_DEL_FLAGS(obj, IS_OBJ_DESTRUCTOR_CALLED);

		/* unset() dynamic properties */
		zend_object_dtor_dynamic_properties(obj);
		obj->properties = NULL;

		/* Objects become non-lazy if all properties are made non-lazy before
		 * initialization is triggerd. If the object has no properties to begin
		 * with, this happens immediately. */
		if (!obj->ce->default_properties_count) {
			return obj;
		}

		/* unset() declared properties */
		for (int i = 0; i < obj->ce->default_properties_count; i++) {
			zval *p = &obj->properties_table[i];
			zend_object_dtor_property(obj, p);
			ZVAL_UNDEF(p);
			Z_PROP_FLAG_P(p) = IS_PROP_UNINIT | IS_PROP_LAZY;
		}
	}

	OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_LAZY;

	if (flags & ZEND_LAZY_OBJECT_STRATEGY_VIRTUAL) {
		OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_VIRTUAL_LAZY;
	} else {
		ZEND_ASSERT(flags & ZEND_LAZY_OBJECT_STRATEGY_GHOST);
	}

	zend_lazy_object_info *info = emalloc(sizeof(*info));
	zend_fcc_dup(&info->u.initializer, initializer);
	info->flags = flags;
	info->lazy_properties_count = obj->ce->default_properties_count;
	info->passthru = 0;
	zend_lazy_object_set_info(obj, info);

	return obj;
}

/**
 * Initialization of lazy objects
 */

/* Revert initializer effects */
static void zend_lazy_object_revert_init(zend_object *obj, zval *properties_table_snapshot, HashTable *properties_snapshot)
{
	zend_class_entry *ce = obj->ce;

	if (ce->default_properties_count) {
		ZEND_ASSERT(properties_table_snapshot);
		zval *properties_table = obj->properties_table;

		for (int i = 0; i < ce->default_properties_count; i++) {
			zval *p = &properties_table[i];
			zend_object_dtor_property(obj, p);
			ZVAL_COPY_PROP(p, &properties_table_snapshot[i]);
			Z_TRY_DELREF_P(p);

			zend_property_info *prop_info = zend_get_property_info_for_slot(obj, p);
			if (Z_ISREF_P(p) && ZEND_TYPE_IS_SET(prop_info->type)) {
				ZEND_REF_ADD_TYPE_SOURCE(Z_REF_P(p), prop_info);
			}
		}

		efree(properties_table_snapshot);
	}
	if (properties_snapshot) {
		if (obj->properties != properties_snapshot) {
			ZEND_ASSERT(GC_REFCOUNT(properties_snapshot) >= 1 || (GC_FLAGS(properties_snapshot) & IS_ARRAY_IMMUTABLE));
			zend_release_properties(obj->properties);
			obj->properties = properties_snapshot;
		} else {
			ZEND_ASSERT(GC_REFCOUNT(properties_snapshot) > 1 || (GC_FLAGS(properties_snapshot) & IS_ARRAY_IMMUTABLE));
			zend_release_properties(properties_snapshot);
		}
	} else if (obj->properties) {
		zend_release_properties(obj->properties);
		obj->properties = NULL;
	}

	OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_LAZY;
}

static bool zend_lazy_object_compatible(zend_object *realized_object, zend_object *lazy_object)
{
	if (instanceof_function(realized_object->ce, lazy_object->ce)) {
		return true;
	}

	if (instanceof_function(lazy_object->ce, realized_object->ce) && lazy_object->ce->default_properties_count == realized_object->ce->default_properties_count) {
		return true;
	}

	return false;
}

/* Initialize a virtual lazy object */
static zend_object *zend_lazy_object_init_virtual_with(zend_object *obj, zend_fcall_info_cache *initializer)
{
	ZEND_ASSERT(zend_object_is_virtual(obj));
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	zend_lazy_object_info *info = zend_lazy_object_get_info(obj);

	/* prevent reentrant initialization */
	OBJ_EXTRA_FLAGS(obj) &= ~(IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY);

	zval retval;
	int argc = 1;
	zval zobj;
	HashTable *named_params = NULL;

	ZVAL_OBJ(&zobj, obj);

	zend_call_known_fcc(initializer, &retval, argc, &zobj, named_params);

	if (EG(exception)) {
		OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY;
		return NULL;
	}

	if (Z_TYPE(retval) != IS_OBJECT || !zend_lazy_object_compatible(Z_OBJ(retval), obj)) {
		OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY;
		zend_throw_error(NULL, "Virtual object intializer was expected to return an instance of %s or a parent with the same properties, %s returned",
				ZSTR_VAL(obj->ce->name), zend_zval_value_name(&retval));
		zval_ptr_dtor(&retval);
		return NULL;
	}

	if (Z_OBJ(retval) == obj || (zend_object_is_lazy(obj) && !zend_lazy_object_must_init(obj))) {
		OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY;
		zend_throw_error(NULL, "Virtual object intializer must return a non-lazy object");
		zval_ptr_dtor(&retval);
		return NULL;
	}

	zend_fcc_dtor(&info->u.initializer);
	info->u.instance = Z_OBJ(retval);
	info->flags |= ZEND_LAZY_OBJECT_INITIALIZED;
	OBJ_EXTRA_FLAGS(obj) |= IS_OBJ_VIRTUAL_LAZY;

	/* unset() properties of the proxy. This ensures that all accesses are be
	 * delegated to the backing instance from now on. */
	// TODO: test that props are undef after initialization
	zend_object_dtor_dynamic_properties(obj);
	obj->properties = NULL;

	for (int i = 0; i < obj->ce->default_properties_count; i++) {
		zend_object_dtor_property(obj, &obj->properties_table[i]);
		ZVAL_UNDEF(&obj->properties_table[i]);
		Z_PROP_FLAG_P(&obj->properties_table[i]) = IS_PROP_UNINIT | IS_PROP_LAZY;
	}

	return Z_OBJ(retval);
}

/* Initialize a lazy object. Initializer may be NULL, in which case lazy
 * properties are initialized to their default value and no initializer is
 * called. */
ZEND_API zend_object *zend_lazy_object_init_with(zend_object *obj, zend_fcall_info_cache *initializer)
{
	ZEND_ASSERT(zend_object_is_lazy(obj));
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	zend_class_entry *ce = obj->ce;

	if (!initializer) {
		zval *default_properties_table = CE_DEFAULT_PROPERTIES_TABLE(ce);
		zval *properties_table = obj->properties_table;

		OBJ_EXTRA_FLAGS(obj) &= ~(IS_OBJ_LAZY|IS_OBJ_VIRTUAL_LAZY);

		for (int i = 0; i < ce->default_properties_count; i++) {
			if (Z_ISUNDEF(properties_table[i])) {
				ZVAL_COPY_PROP(&properties_table[i], &default_properties_table[i]);
			}
		}

		zend_lazy_object_del_info(obj);

		return obj;
	}

	if (zend_object_is_virtual(obj)) {
		return zend_lazy_object_init_virtual_with(obj, initializer);
	}

	/* Prevent reentrant initialization */
	OBJ_EXTRA_FLAGS(obj) &= ~IS_OBJ_LAZY;

	/* Snapshot dynamic properties */
	HashTable *properties_snapshot = obj->properties;
	if (properties_snapshot && !(GC_FLAGS(properties_snapshot) & IS_ARRAY_IMMUTABLE)) {
		GC_ADDREF(properties_snapshot);
	}

	zval *properties_table_snapshot = NULL;

	/* Snapshot declared properties and initialize to default values (except
	 * properties that was already defined). */
	if (ce->default_properties_count) {
		zval *default_properties_table = CE_DEFAULT_PROPERTIES_TABLE(ce);
		zval *properties_table = obj->properties_table;
		properties_table_snapshot = emalloc(sizeof(*properties_table_snapshot) * ce->default_properties_count);

		for (int i = 0; i < ce->default_properties_count; i++) {
			ZVAL_COPY_PROP(&properties_table_snapshot[i], &properties_table[i]);
			if (Z_ISUNDEF(properties_table[i])) {
				ZVAL_COPY_PROP(&properties_table[i], &default_properties_table[i]);
			}
		}
	}

	/* Reset ZEND_GUARD_PROPERTY_HOOK guards: property accesses during
	 * initialization should execute hooks */
	if (obj->ce->ce_flags & ZEND_ACC_USE_GUARDS) {
		zend_mask_property_guards(obj, ~ZEND_GUARD_PROPERTY_HOOK);
	}

	zval retval;
	int argc = 1;
	zval zobj;
	HashTable *named_params = NULL;

	ZVAL_OBJ(&zobj, obj);

	zend_call_known_fcc(initializer, &retval, argc, &zobj, named_params);

	if (EG(exception)) {
		zend_lazy_object_revert_init(obj, properties_table_snapshot, properties_snapshot);
		return NULL;
	}

	if (Z_TYPE(retval) != IS_NULL) {
		zend_lazy_object_revert_init(obj, properties_table_snapshot, properties_snapshot);
		zval_ptr_dtor(&retval);
		zend_throw_error(NULL, "Lazy object initializer must return NULL or no value");
		return NULL;
	}

	if (properties_table_snapshot) {
		for (int i = 0; i < obj->ce->default_properties_count; i++) {
			zval *p = &properties_table_snapshot[i];
			/* Use zval_ptr_dtor directly here (not zend_object_dtor_property),
			 * as any reference type_source will have already been deleted in
			 * case the prop is not bound to this value anymore. */
			i_zval_ptr_dtor(p);
		}
		efree(properties_table_snapshot);
	}

	if (properties_snapshot) {
		zend_release_properties(properties_snapshot);
	}

	zend_lazy_object_del_info(obj);

	return obj;
}

ZEND_API zend_object *zend_lazy_object_init(zend_object *obj)
{
	ZEND_ASSERT(zend_object_is_lazy(obj));

	if (zend_lazy_object_initialized(obj)) {
		ZEND_ASSERT(zend_object_is_virtual(obj));
		zend_lazy_object_info *info = zend_lazy_object_get_info(obj);
		if (info->flags & ZEND_LAZY_OBJECT_INITIALIZED) {
			return info->u.instance;
		}
		return NULL;
	}

	zend_fcall_info_cache *initializer = zend_lazy_object_get_initializer(obj);

	return zend_lazy_object_init_with(obj, initializer);
}

/* Mark an object as non-lazy (after all properties were initialized) */
void zend_lazy_object_realize(zend_object *obj)
{
	ZEND_ASSERT(zend_object_is_lazy(obj));
	ZEND_ASSERT(!zend_lazy_object_initialized(obj));

	zend_lazy_object_del_info(obj);

#if ZEND_DEBUG
	for (int i = 0; i < obj->ce->default_properties_count; i++) {
		ZEND_ASSERT(!(Z_PROP_FLAG_P(&obj->properties_table[i]) & IS_PROP_LAZY));
	}
#endif

	OBJ_EXTRA_FLAGS(obj) &= ~(IS_OBJ_LAZY | IS_OBJ_VIRTUAL_LAZY);
}

zend_object *zend_lazy_object_clone(zend_object *old_obj)
{
	ZEND_ASSERT(zend_object_is_lazy(old_obj));

	zend_lazy_object_info *info = zend_lazy_object_get_info(old_obj);

	if (zend_object_is_virtual(old_obj) && zend_lazy_object_initialized(old_obj)) {
		return zend_objects_clone_obj(info->u.instance);
	}

	zend_object *new_obj = zend_object_make_lazy(NULL, old_obj->ce,
			&info->u.initializer, info->flags);
	ZEND_ASSERT(new_obj);

	zend_objects_clone_members(new_obj, old_obj);

	return new_obj;
}

HashTable *zend_lazy_object_debug_info(zend_object *object, int *is_temp)
{
	ZEND_ASSERT(zend_object_is_lazy(object));

	if (zend_object_is_virtual(object)) {
		if (zend_lazy_object_initialized(object)) {
			HashTable *properties = zend_new_array(0);
			zval instance;
			ZVAL_OBJ(&instance, zend_lazy_object_get_instance(object));
			Z_ADDREF(instance);
			zend_hash_str_add(properties, "instance", strlen("instance"), &instance);
			*is_temp = 1;
			return properties;
		}
	}

	HashTable *properties;
	ZEND_LAZY_OBJECT_PASSTHRU(object) {
		properties = object->handlers->get_properties(object);
	} ZEND_LAZY_OBJECT_PASSTHRU_END();

	*is_temp = 0;

	return properties;
}
