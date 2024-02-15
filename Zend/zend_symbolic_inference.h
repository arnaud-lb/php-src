
#ifndef ZEND_SYMBOLIC_INFERENCE_H
#define ZEND_SYMBOLIC_INFERENCE_H

#include <zend.h>
#include <zend_types.h>
#include <zend_arena.h>
#include <zend_compile.h>

ZEND_API zend_result zend_symbolic_inference(const zend_op_array *op_array, zend_arena **arena);
ZEND_API zend_type zend_resolve_symbolic_type(zend_arena **arena, zend_type type);

#endif /* ZEND_SYMBOLIC_INFERENCE_H */
