#ifndef _ZEND_TYPE_TOOLS_H_
#define _ZEND_TYPE_TOOLS_H_

#include "zend.h"
#include "zend_arena.h"
#include "zend_types.h"

void zend_type_copy_ctor(zend_type *const type, bool use_arena, bool persistent);
zend_type zend_type_intersect(zend_arena **arena, zend_type dest, zend_type src);
zend_type zend_type_union(zend_arena **arena, zend_type dest, zend_type src);
zend_type zend_type_normalize_union_in_place(zend_type t);
zend_type zend_type_normalize_intersection_in_place(zend_type t);

#endif /* _ZEND_TYPE_TOOLS_H_ */
