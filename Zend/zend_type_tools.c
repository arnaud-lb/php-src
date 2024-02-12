
#include "zend.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_arena.h"
#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_string.h"
#include "zend_types.h"
#include "zend_type_info.h"
#include "zend_bitset.h"

void zend_type_copy_ctor(zend_type *const type, bool use_arena, bool persistent);

// TODO: dedup this function
static void zend_type_list_copy_ctor(
	zend_type *const parent_type,
	bool use_arena,
	bool persistent
) {
	const zend_type_list *const old_list = ZEND_TYPE_LIST(*parent_type);
	size_t size = ZEND_TYPE_LIST_SIZE(old_list->num_types);
	zend_type_list *new_list = use_arena
		? zend_arena_alloc(&CG(arena), size) : pemalloc(size, persistent);

	memcpy(new_list, old_list, size);
	ZEND_TYPE_SET_LIST(*parent_type, new_list);
	if (use_arena) {
		ZEND_TYPE_FULL_MASK(*parent_type) |= _ZEND_TYPE_ARENA_BIT;
	}

	zend_type *list_type;
	ZEND_TYPE_LIST_FOREACH(new_list, list_type) {
		zend_type_copy_ctor(list_type, use_arena, persistent);
	} ZEND_TYPE_LIST_FOREACH_END();
}

// TODO: dedup this function
void zend_type_copy_ctor(zend_type *const type, bool use_arena, bool persistent) {
	if (ZEND_TYPE_HAS_LIST(*type)) {
		zend_type_list_copy_ctor(type, use_arena, persistent);
	} else if (ZEND_TYPE_HAS_PNR(*type)) {
		// TODO: Duplicate name reference...
		zend_string_addref(ZEND_TYPE_PNR_NAME(*type));
	}
}

#define REALLOC_INTERSECT_LIST() do { \
	if (!new_list) { \
		zend_type_list *old_list = ZEND_TYPE_IS_INTERSECTION(dest) ? ZEND_TYPE_LIST(dest) : NULL; \
		/* Worse case */ \
		size_t num_types = (old_list ? old_list->num_types : 1) \
			+ (ZEND_TYPE_IS_INTERSECTION(src) ? ZEND_TYPE_LIST(src)->num_types : 1); \
		new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(num_types)); \
		if (old_list) { \
			memcpy(new_list, old_list, ZEND_TYPE_LIST_SIZE(old_list->num_types)); \
		} else { \
			new_list->types[0] = dest; \
			new_list->num_types = 1; \
		} \
		dest.ptr = new_list; \
		dest.type_mask = ZEND_TYPE_PURE_MASK(dest) | _ZEND_TYPE_LIST_BIT|_ZEND_TYPE_INTERSECTION_BIT|_ZEND_TYPE_ARENA_BIT; \
		dest_elems = new_list->types; \
	} \
} while (0)

zend_type zend_type_intersect(zend_arena **arena, zend_type dest, zend_type src)
{
	if (ZEND_TYPE_PURE_MASK(dest) || !ZEND_TYPE_IS_COMPLEX(src)) {
		return dest;
	}

	if (ZEND_TYPE_IS_UNION(dest) || ZEND_TYPE_IS_UNION(src)) {
		zend_type_list *new_list = NULL;
		zend_type *src_elem;
		ZEND_TYPE_FOREACH_EX(src, src_elem, ~_ZEND_TYPE_INTERSECTION_BIT) {
			zend_type *dest_elems = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest)->types : &dest;
			uint32_t num_types = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest)->num_types : 1;
			for (uint32_t i = 0; i < num_types; i++) {
				zend_type *dest_elem = &dest_elems[i];
				zend_type intersection = zend_type_intersect(arena, *dest_elem, *src_elem);
				if (dest_elem->type_mask != intersection.type_mask
						|| (ZEND_TYPE_IS_COMPLEX(*dest_elem) && dest_elem->ptr != intersection.ptr)) {
					bool add = true;
					for (uint32_t j = 0; j < i; j++) {
						if (zend_type_accepts(&intersection, NULL, &dest_elems[j], NULL, NULL)) {
							add = false;
							break;
						}
					}
					if (add) {
						if (!new_list) {
							zend_type_list *old_list = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest) : NULL;
							/* Worse case */
							size_t num_types = (old_list ? old_list->num_types : 1)
								* (ZEND_TYPE_IS_UNION(src) ? ZEND_TYPE_LIST(src)->num_types : 1);
							new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(num_types));
							if (old_list) {
								memcpy(new_list, old_list, sizeof(zend_type)*i);
								new_list->num_types = i;
							} else {
								new_list->num_types = 0;
							}
							dest.ptr = new_list;
							dest.type_mask = ZEND_TYPE_PURE_MASK(dest) | _ZEND_TYPE_LIST_BIT|_ZEND_TYPE_UNION_BIT|_ZEND_TYPE_ARENA_BIT;
						}
						new_list->types[new_list->num_types++] = intersection;
					}
				}
			}
		} ZEND_TYPE_FOREACH_END();

		return dest;
	}

	zend_type_list *new_list = NULL;
	zend_type *src_elem;
	ZEND_TYPE_FOREACH(src, src_elem) {
		bool add = true;
		zend_type *dest_elems = ZEND_TYPE_HAS_LIST(dest) ? ZEND_TYPE_LIST(dest)->types : &dest;
		uint32_t num_types = ZEND_TYPE_HAS_LIST(dest) ? ZEND_TYPE_LIST(dest)->num_types : 1;
		for (uint32_t i = 0; i < num_types; i++) {
			zend_type *dest_elem = &dest_elems[i];
			if (zend_type_accepts(src_elem, NULL, dest_elem, NULL, NULL)) {
				REALLOC_INTERSECT_LIST();
				*dest_elem = *src_elem;
				add = false;
			} else if (zend_type_accepts(dest_elem, NULL, src_elem, NULL, NULL)) {
				add = false;
				break;
			}
		}
		if (add) {
			REALLOC_INTERSECT_LIST();
			new_list->types[new_list->num_types++] = *src_elem;
		}
	} ZEND_TYPE_FOREACH_END();

	return dest;
}

#define REALLOC_UNION_LIST() do { \
	if (!new_list) { \
		zend_type_list *old_list = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest) : NULL; \
		/* Worse case */ \
		size_t num_types = (old_list ? old_list->num_types : 1) \
			+ (ZEND_TYPE_IS_UNION(src) ? ZEND_TYPE_LIST(src)->num_types : 1); \
		new_list = zend_arena_alloc(arena, ZEND_TYPE_LIST_SIZE(num_types)); \
		if (old_list) { \
			memcpy(new_list, old_list, ZEND_TYPE_LIST_SIZE(old_list->num_types)); \
		} else { \
			new_list->types[0] = dest; \
			new_list->num_types = 1; \
		} \
		dest.ptr = new_list; \
		dest.type_mask = ZEND_TYPE_PURE_MASK(dest) | _ZEND_TYPE_LIST_BIT|_ZEND_TYPE_UNION_BIT|_ZEND_TYPE_ARENA_BIT; \
		dest_elems = new_list->types; \
	} \
} while (0)

zend_type zend_type_union(zend_arena **arena, zend_type dest, zend_type src)
{
	if (!ZEND_TYPE_IS_COMPLEX(dest) && !ZEND_TYPE_IS_COMPLEX(src)) {
		ZEND_TYPE_FULL_MASK(dest) |= ZEND_TYPE_PURE_MASK(src);
		return dest;
	}

	if (!ZEND_TYPE_IS_COMPLEX(dest)) {
		ZEND_ASSERT(ZEND_TYPE_IS_COMPLEX(src));
		uint32_t mask = ZEND_TYPE_PURE_MASK(dest);
		dest = src;
		ZEND_TYPE_FULL_MASK(dest) |= mask;
		return dest;
	}

	zend_type_list *new_list = NULL;
	zend_type *src_elem;
	ZEND_TYPE_FOREACH_EX(src, src_elem, ~_ZEND_TYPE_INTERSECTION_BIT) {
		bool add = true;
		zend_type *dest_elems = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest)->types : &dest;
		uint32_t num_types = ZEND_TYPE_IS_UNION(dest) ? ZEND_TYPE_LIST(dest)->num_types : 1;
		for (uint32_t i = 0; i < num_types; i++) {
			zend_type *dest_elem = &dest_elems[i];
			if (zend_type_accepts(src_elem, NULL, dest_elem, NULL, NULL)) {
				REALLOC_UNION_LIST();
				*dest_elem = *src_elem;
				add = false;
			} else if (zend_type_accepts(dest_elem, NULL, src_elem, NULL, NULL)) {
				add = false;
				break;
			}
		}
		if (add) {
			REALLOC_UNION_LIST();
			if (!ZEND_TYPE_IS_COMPLEX(*src_elem)) {
				ZEND_TYPE_FULL_MASK(dest) |= ZEND_TYPE_PURE_MASK(*src_elem);
			} else {
				new_list->types[new_list->num_types++] = *src_elem;
			}
		}
	} ZEND_TYPE_FOREACH_END();

	if (new_list && new_list->num_types == 0) {
		ZEND_TYPE_SET_PTR(dest, NULL);
		ZEND_TYPE_FULL_MASK(dest) = ZEND_TYPE_PURE_MASK(dest);
	}

	return dest;
}

zend_type zend_type_normalize_union_in_place(zend_type t)
{
	ZEND_ASSERT(ZEND_TYPE_IS_UNION(t));

	zend_type_list *list = ZEND_TYPE_LIST(t);
	zend_type *a = list->types;
	zend_type *b;
	zend_type *last = a + list->num_types - 1;

	while (a <= last) {
restart:
		if (!ZEND_TYPE_IS_COMPLEX(*a)) {
			ZEND_TYPE_FULL_MASK(t) |= ZEND_TYPE_PURE_MASK(*a);
			// Remove a
			if (a != last) {
				*a = *last;
			}
			last--;
			continue;
		}
		b = &list->types[0];
		while (b <= last && b < a) {
			if (zend_type_accepts(b, NULL, a, NULL, NULL)) {
				// Remove a
				if (a != last) {
					*a = *last;
					last--;
					goto restart;
				} else {
					last--;
					break;
				}
			} else if (zend_type_accepts(a, NULL, b, NULL, NULL)) {
				// Remove b
                if (b != last) {
                    *b = *last;
                }
                if (a == last) {
                    b++;
                }
				last--;
				continue;
			}
			b++;
		}
		a++;
	}

	uint32_t num_types = last+1-list->types;
	if (num_types == 0) {
		ZEND_TYPE_FULL_MASK(t) &= ~(_ZEND_TYPE_LIST_BIT | _ZEND_TYPE_UNION_BIT | _ZEND_TYPE_ARENA_BIT);
	} else if (num_types == 1) {
		uint32_t mask = ZEND_TYPE_PURE_MASK(t);
		t = list->types[0];
		ZEND_TYPE_FULL_MASK(t) |= mask;
	} else {
		list->num_types = num_types;
	}

	return t;
}

zend_type zend_type_normalize_intersection_in_place(zend_type t)
{
	ZEND_ASSERT(ZEND_TYPE_IS_INTERSECTION(t));

	zend_type_list *list = ZEND_TYPE_LIST(t);
	zend_type *a = list->types;
	zend_type *b;
	zend_type *last = a + list->num_types - 1;

	while (a <= last) {
restart:
		if (!ZEND_TYPE_IS_COMPLEX(*a)) {
			ZEND_TYPE_FULL_MASK(t) |= ZEND_TYPE_PURE_MASK(*a);
			// Remove a
			if (a != last) {
				*a = *last;
			}
			last--;
			continue;
		}
		b = &list->types[0];
		while (b <= last && b < a) {
			if (zend_type_accepts(a, NULL, b, NULL, NULL)) {
				// Remove a
				if (a != last) {
					*a = *last;
					last--;
					goto restart;
				} else {
					last--;
					break;
				}
			} else if (zend_type_accepts(b, NULL, a, NULL, NULL)) {
				// Remove b
                if (b != last) {
                    *b = *last;
                }
                if (a == last) {
                    b++;
                }
				last--;
				continue;
			}
			b++;
		}
		a++;
	}

	uint32_t num_types = last+1-list->types;
	if (num_types == 0) {
		ZEND_TYPE_FULL_MASK(t) &= ~(_ZEND_TYPE_LIST_BIT | _ZEND_TYPE_INTERSECTION_BIT | _ZEND_TYPE_ARENA_BIT);
	} else if (num_types == 1) {
		uint32_t mask = ZEND_TYPE_PURE_MASK(t);
		t = list->types[0];
		ZEND_TYPE_FULL_MASK(t) |= mask;
	} else {
		list->num_types = num_types;
	}

	return t;
}

