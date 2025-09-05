
#include "zend.h"
#include "zend_multiply.h"
#include "zend_alloc.h"
#include "zend_atomic.h"
#include "zend_globals_macros.h"
#include "zend_globals.h"

ZEND_API void* ZEND_FASTCALL _emalloc(size_t size ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	return GC_MALLOC(size);
}

ZEND_API void ZEND_FASTCALL _efree(void *ptr ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	(void)ptr;
}

ZEND_API void* ZEND_FASTCALL _safe_emalloc(size_t nmemb, size_t size, size_t offset ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	return GC_MALLOC(zend_safe_address_guarded(nmemb, size, offset));
}

ZEND_API void* ZEND_FASTCALL _safe_emalloc_atomic(size_t nmemb, size_t size, size_t offset ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	return GC_MALLOC_ATOMIC(zend_safe_address_guarded(nmemb, size, offset));
}

ZEND_API void* ZEND_FASTCALL _ecalloc(size_t nmemb, size_t size ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	return GC_MALLOC(zend_safe_address_guarded(nmemb, size, 0));
}

ZEND_API void* ZEND_FASTCALL _safe_erealloc(void *ptr, size_t nmemb, size_t size, size_t offset ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	return erealloc(ptr, zend_safe_address_guarded(nmemb, size, offset));
}

ZEND_API char *_estrdup(const char *s ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	size_t length = strlen(s);
	if (UNEXPECTED(length + 1 == 0)) {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (1 * %zu + 1)", length);
	}
	char *p = GC_MALLOC_ATOMIC(length+1);
	memcpy(p, s, length+1);
	return p;
}

ZEND_API char* _estrndup(const char *s, size_t length ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	if (UNEXPECTED(length + 1 == 0)) {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (1 * %zu + 1)", length);
	}
	char *p = GC_MALLOC_ATOMIC(length+1);
	memcpy(p, s, length);
	p[length] = 0;
	return p;
}

static ZEND_COLD ZEND_NORETURN void zend_out_of_memory(void)
{
	fprintf(stderr, "Out of memory\n");
	exit(1);
}

ZEND_API char* ZEND_FASTCALL zend_strndup(const char *s, size_t length)
{
	char *p;

	if (UNEXPECTED(length + 1 == 0)) {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (1 * %zu + 1)", length);
	}
	p = (char *) GC_MALLOC(length + 1);
	if (UNEXPECTED(p == NULL)) {
		zend_out_of_memory();
	}
	if (EXPECTED(length)) {
		memcpy(p, s, length);
	}
	p[length] = 0;
	return p;
}

/* Called once per collection when there are objects to be finalized */
static void GC_CALLBACK zend_finalizer_notifier_proc(void)
{
	zend_atomic_bool_store_ex(&EG(pending_finalizations), true);
	zend_atomic_bool_store_ex(&EG(vm_interrupt), true);
}

static size_t peak_usage = 0;

static size_t zend_gc_query_mm_usage(void)
{
	GC_word heap_size;
	GC_word free_size;
	GC_word unmapped_bytes;
	GC_word bytes_since_gc;
	GC_word total_bytes;
	GC_get_heap_usage_safe(&heap_size, &free_size, &unmapped_bytes,
			&bytes_since_gc, &total_bytes);
	return heap_size - free_size;
}

static void GC_CALLBACK zend_gc_on_collection_event_proc(GC_EventType event_type)
{
	if (event_type == GC_EVENT_START) {
		peak_usage = MAX(peak_usage, zend_gc_query_mm_usage());
	}
}

static bool memory_manager_initialized = false;

ZEND_API void init_memory_manager(void)
{
	const char *tmp = getenv("GC_INCREMENTAL");
	if (tmp && ZEND_ATOL(tmp)) {
		GC_enable_incremental();
	}

	/* Queue objects for finalization even if they are reachable from other
	 * queued objects.
	 * After queuing, all objects reachable from them are marked
	 * (considered reachable).
	 * Objects queued for finalization are reachable via GC_fnlz_roots until the
	 * finalizer is invoked. */
	GC_set_java_finalization(1);

	GC_set_finalize_on_demand(1);
	GC_set_finalizer_notifier(zend_finalizer_notifier_proc);

	ZEND_ASSERT(!GC_is_init_called() && "init_memory_manager() must be called before any allocation");
	GC_set_handle_fork(1);

	ZEND_ASSERT(GC_get_all_interior_pointers());

	GC_set_on_collection_event(zend_gc_on_collection_event_proc);

	/* Set a lower bound to the minimum bytes allocated since last GC before a
	 * run is triggered automatically. */
	tmp = getenv("GC_MIN_BYTES_ALLOCD");
	if (tmp) {
		GC_set_min_bytes_allocd(ZEND_ATOL(tmp));
	}

	GC_word allocd_bytes_per_finalizer = 0;
	tmp = getenv("GC_ALLOC_BYTES_PER_FINALIZER");
	if (tmp) {
		allocd_bytes_per_finalizer = ZEND_ATOL(tmp);
	}
	GC_set_allocd_bytes_per_finalizer(allocd_bytes_per_finalizer);

	memory_manager_initialized = true;
}

ZEND_API void start_memory_manager(void)
{
	ZEND_ASSERT(memory_manager_initialized && "init_memory_manager() was not called");
}

ZEND_API void shutdown_memory_manager(bool silent, bool full_shutdown)
{
	// The best time to force a collection might be just after
	// php_request_startup(), as this is when the live set is the smallest.
	// GC_gcollect();

	/* We should have unregistered all finalizers at this point */
	ZEND_ASSERT(!GC_should_invoke_finalizers());

	peak_usage = 0;
}

ZEND_API void prepare_memory_manager(void)
{
	/* Run GC when the live set is likely to be the smallest */
	const char *tmp = getenv("GC_AFTER_STARTUP");
	if (tmp && ZEND_ATOL(tmp)) {
		GC_gcollect();
	}
}

ZEND_API zend_result zend_set_memory_limit(size_t memory_limit)
{
	return SUCCESS;
}

ZEND_API size_t zend_memory_usage(bool real_usage)
{
	GC_alloc_lock();
	size_t usage = zend_gc_query_mm_usage();
	GC_alloc_unlock();

	return usage;
}

ZEND_API size_t zend_memory_peak_usage(bool real_usage)
{
	GC_alloc_lock();
	peak_usage = MAX(peak_usage, zend_gc_query_mm_usage());
	GC_alloc_unlock();

	return peak_usage;
}

ZEND_API size_t zend_mm_gc(zend_mm_heap *heap)
{
	GC_gcollect_and_unmap();
	return 0;
}

ZEND_API bool is_zend_ptr(const void *ptr)
{
	return false;
}

#if ZEND_DEBUG
void zend_mm_print_backtrace(void *ptr)
{
	GC_print_backtrace(ptr);
}
#endif

#if !ZEND_DEBUG
# define _ZEND_BIN_ALLOCATOR(_num, _size, _elements, _pages, _min_size, y) \
	ZEND_API void* ZEND_FASTCALL _emalloc_ ## _size(void) { \
		return GC_MALLOC(_size); \
	}

ZEND_MM_BINS_INFO(_ZEND_BIN_ALLOCATOR, ZEND_MM_MIN_USEABLE_BIN_SIZE, y)

# define _ZEND_BIN_FREE(_num, _size, _elements, _pages, _min_size, y) \
	ZEND_API void ZEND_FASTCALL _efree_ ## _size(void *ptr) { \
		GC_FREE(ptr); \
	}

ZEND_MM_BINS_INFO(_ZEND_BIN_FREE, ZEND_MM_MIN_USEABLE_BIN_SIZE, y)
#endif
