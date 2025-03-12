
#ifndef ZEND_SNAPSHOT_H
#define ZEND_SNAPSHOT_H

#include "zend_config.h"
#include "zend_types.h"
#include "zend_alloc.h"
#include "zend_stack.h"
#include "zend_objects_API.h"

typedef struct _zend_snapshot {
	int             restores;
	zend_array      symbol_table;
	int             chunks_count;
	void          **chunks;
	void           *copy;
	int             memfd;
	void           *map_ptr_real_base;
	void           *map_ptr_base;
	size_t          map_ptr_static_size;
	size_t          map_ptr_last;
	zend_array      class_table;
	zend_array      function_table;
	zend_array      constant_table;
	zend_array      included_files;
	zend_objects_store objects_store;
	int             user_error_handler_error_reporting;
	zval            user_error_handler;
	zval            user_exception_handler;
	zend_stack      user_error_handlers_error_reporting;
	zend_stack      user_error_handlers;
	zend_stack      user_exception_handlers;

	// exts (TODO: ht and ext handlers)
	void           *date_snapshot;
	void           *spl_snapshot;
	void           *standard_snapshot;
} zend_snapshot;

#endif /* ZEND_SNAPSHOT_H */
