#ifndef CONTAINER_ALLOC_H
#define CONTAINER_ALLOC_H

#include <stddef.h>

#define BEGIN { alloc_frame _frame = {0}; alloc_begin(&_frame, __FILE__, __LINE__);
#define END alloc_end(); }

#define RETURN_VOID do { alloc_end(); return; } while(0)
#define RETURN_BASIC(x) do { alloc_end(); return (x); } while(0)
#define RETURN(x) do { return alloc_return(&(x)->ptr, sizeof *(x)); } while(0)


#ifndef BOOL
	#define BOOL int
#endif

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif


//
// The below are implementation details. Do not use them directly.
//

typedef struct alloc_ptr {
	struct alloc_ptr *next;
	struct alloc_node *node;
} alloc_ptr;

typedef struct alloc_frame {
	struct alloc_frame *next_frame;
	alloc_ptr *ptr_list;
	struct { alloc_ptr ptr; size_t other_stuff; } return_value;
	const char *filename;
	size_t line_number;
} alloc_frame;

/*
typedef struct alloc_weak_ptr {
	struct alloc_ptr ptr;
	struct alloc_weak_ptr *next_weak_ptr;
} alloc_weak_ptr;
*/


void alloc_begin(alloc_frame *frame, const char *filename, size_t line_number);
void alloc_end();

void* alloc_return(alloc_ptr *ptr, size_t size);
void* alloc_return_new(size_t size);

struct alloc_node* alloc_new(size_t size);
struct alloc_node* alloc_resize(struct alloc_node *node, size_t new_size);
void* alloc_data(alloc_ptr *ptr);
size_t alloc_size(alloc_ptr *ptr);

void alloc_init(alloc_ptr *ptr, size_t size);
void alloc_assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr);

void alloc_global_assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr);

void alloc_gc();
BOOL alloc_set_max_memory_usage(size_t max_bytes);
size_t alloc_max_memory_usage();
size_t alloc_memory_usage();

void alloc_debug_info();

#endif
