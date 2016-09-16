#include "alloc.h"
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#define ALLOC_DATA(node) ((void*)((char*)(node) + sizeof(alloc_node)))
#define ADJUST_OFFSET(ptr, offset) ((void*)((char*)(ptr) + (offset)))

// TODO: make this a balanced tree
typedef struct alloc_node {
	struct alloc_node *parent;
	struct alloc_node *left;
	struct alloc_node *right;
	struct alloc_ptr *ptr_list;
	//struct alloc_weak_ptr *weak_ptr_list;
	size_t ref_count;
	size_t size;
} alloc_node;

static size_t max_usage_max = SIZE_MAX / 2;
static size_t max_usage = SIZE_MAX / 2;
static size_t memory_usage = 0;

static alloc_ptr end_ptr = { 0 };

static alloc_node allocations = { NULL, NULL, NULL, NULL, 1, 0 };
static alloc_frame global_frame = { 0 };
static alloc_frame *current_frame = &global_frame;

static void assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr);

static alloc_ptr *remove_ptrs(alloc_ptr *ptr_list, char *data, size_t start_pos, size_t end_pos);
static alloc_ptr *adjust_next_ptrs(alloc_ptr *ptr_list, ptrdiff_t offset);
static void adjust_node_ptrs(alloc_ptr *ptr_list, alloc_node *old_node, alloc_node *new_node);
static void adjust_node_tree_node_ptrs(alloc_node *parent, alloc_node *old_node, alloc_node *new_node);

static alloc_ptr *find_prev_ptr(alloc_ptr *ptr_list, alloc_ptr *current_ptr);

static void decrement_list_ref_count(alloc_ptr *ptr);
static void decrement_ref_count(alloc_ptr *ptr);

static alloc_node* malloc_node(size_t size);
static void free_node(alloc_node *node);

static void mark_nodes(alloc_ptr *ptr_list);
static void gc_nodes(alloc_node *parent);
static void unmark_nodes(alloc_node *parent);

static void add_alloc_node(alloc_node *parent, alloc_node *node);
static void remove_alloc_node(alloc_node *node);
static alloc_node* find_alloc_node(alloc_ptr *contains);

static void alloc_ptr_list_debug_info(alloc_ptr *ptr_list);
static void alloc_node_tree_debug_info(alloc_node *node, int depth);



void alloc_begin(alloc_frame *frame, const char *filename, size_t line_number) {
	assert(frame != NULL);
	assert(current_frame != NULL);

	frame->next_frame = current_frame;
	frame->ptr_list = &frame->return_value.ptr;
	frame->filename = filename;
	frame->line_number = line_number;
	current_frame = frame;
}


void alloc_end() {
	assert(current_frame != NULL);
	assert(current_frame->next_frame != NULL);

	decrement_list_ref_count(current_frame->ptr_list);
	current_frame = current_frame->next_frame;

	if(current_frame == &global_frame) {
		decrement_list_ref_count(current_frame->ptr_list);
		
		if(memory_usage > 0) {
			printf("\n\n%d BYTES OF UNFREED MEMORY\n\n", (int)memory_usage);
		}

		if(allocations.left || allocations.right) {
			puts("\n\nUNFREED MEMORY AT PROGRAM EXIT");
			puts("------------------------------");
			alloc_node_tree_debug_info(&allocations, 0);
			puts("------------------------------");
		}
	}
}


void* alloc_return(alloc_ptr *ptr, size_t size) {
	assert(current_frame != NULL);
	assert(current_frame->next_frame != NULL);
	assert(size <= sizeof current_frame->return_value);

	alloc_frame *next_frame = current_frame->next_frame;
	alloc_ptr *return_ptr = &next_frame->return_value.ptr;

	ptr->node->ref_count++;
	decrement_ref_count(return_ptr);
	
	if(ptr)
		memcpy(&next_frame->return_value, ptr, size);
	else
		memset(&next_frame->return_value, 0, size);

	return_ptr->next = NULL;
	
	alloc_end();
	return &next_frame->return_value;
}


void* alloc_return_new(size_t size) {
	assert(current_frame != NULL);

	alloc_ptr *ptr = &current_frame->return_value.ptr;
	decrement_ref_count(ptr);
	ptr->node = alloc_new(size);

	if(ptr->node)
		return ptr;
	else
		return NULL;
}


struct alloc_node* alloc_new(size_t size) {
	assert(allocations.parent == NULL);
	assert(allocations.size == 0);
	assert(end_ptr.next == NULL);
	assert(end_ptr.node == NULL);

	alloc_node *node = malloc_node(size);

	if (!node)
		return NULL;

	add_alloc_node(&allocations, node);
	node->left = NULL;
	node->right = NULL;

	node->ptr_list = &end_ptr;
	node->ref_count = 1;
	node->size = size;

	return node;
}


struct alloc_node* alloc_resize(struct alloc_node *node, size_t new_size) {
	assert(current_frame != NULL);

	if(!node)
		return alloc_new(new_size);

	size_t old_size = node->size;

	alloc_node *new_node = malloc_node(new_size);

	if(new_size > 0 && !new_node)
		return NULL;


	if(new_size > 0) {
		assert(new_node != NULL);

		ptrdiff_t offset = (char*)new_node - (char*)node;

		if(new_size < old_size)
			node->ptr_list = remove_ptrs(node->ptr_list, ALLOC_DATA(node), new_size, old_size);

		node->ptr_list = adjust_next_ptrs(node->ptr_list, offset);

		memcpy(new_node, node, sizeof(alloc_node) + (new_size < old_size ? new_size : old_size));
		new_node->size = new_size;
		new_node->left = NULL;
		new_node->right = NULL;
	} else {
		assert(new_node == NULL);
		decrement_list_ref_count(node->ptr_list);
	}

	
	alloc_frame *frame = current_frame;

	while(frame) {
		adjust_node_ptrs(frame->ptr_list, node, new_node);
		frame = frame->next_frame;
	}


	if(new_node) {	
		remove_alloc_node(node);
		adjust_node_tree_node_ptrs(&allocations, node, new_node);
		add_alloc_node(&allocations, new_node);
	} else {
		adjust_node_tree_node_ptrs(&allocations, node, new_node);
	}

	free_node(node);
	return new_node;
}


void* alloc_data(alloc_ptr *ptr) {
	assert(ptr != NULL);

	if(ptr->node)
		return ALLOC_DATA(ptr->node);
	else
		return NULL;
}


size_t alloc_size(alloc_ptr *ptr) {
	assert(ptr != NULL);

	if(ptr->node)
		return ptr->node->size;
	else
		return 0;
}


void alloc_init(alloc_ptr *ptr, size_t size) {
	assert(current_frame != NULL);

	ptr->next = current_frame->ptr_list;
	ptr->node = alloc_new(size);
	current_frame->ptr_list = ptr;
}


void alloc_assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr) {

	assign(to_ptr, from_ptr);

	//if(to_ptr->next)	// faster, but assumes always zero-initialized
	//	return;

	alloc_node *parent = find_alloc_node(to_ptr);

	if(parent) {
		if(parent->ptr_list == to_ptr || find_prev_ptr(parent->ptr_list, to_ptr))
			return;

		to_ptr->next = parent->ptr_list;
		parent->ptr_list = to_ptr;
		return;
	}


	alloc_frame *frame = current_frame;

	while(frame && frame->ptr_list != to_ptr && !find_prev_ptr(frame->ptr_list, to_ptr))
		frame = frame->next_frame;

	if(frame)
		return;

	to_ptr->next = current_frame->ptr_list;
	current_frame->ptr_list = to_ptr;
}


void alloc_global_assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr) {
	
	assign(to_ptr, from_ptr);

	if(find_prev_ptr(global_frame.ptr_list, to_ptr))
		return;

	to_ptr->next = global_frame.ptr_list;
	global_frame.ptr_list = to_ptr;
}


void alloc_gc() {
	alloc_frame *frame = current_frame;

	while(frame) {
		mark_nodes(frame->ptr_list);
		frame = frame->next_frame;
	}

	gc_nodes(allocations.left);
	gc_nodes(allocations.right);

	unmark_nodes(allocations.left);
	unmark_nodes(allocations.right);
}


BOOL alloc_set_max_memory_usage(size_t max_bytes) {
	BOOL success = TRUE;

	if(max_bytes < memory_usage)
		alloc_gc();

	if(max_bytes < memory_usage) {
		max_bytes = memory_usage;
		success = FALSE;
	}

	if(max_bytes > max_usage_max) {
		max_bytes = max_usage_max;
		success = FALSE;
	}

	max_usage = max_bytes;
	return success;
}


size_t alloc_max_memory_usage() {
	return max_usage;
}


size_t alloc_memory_usage() {
	return memory_usage;
}


void assign(alloc_ptr *to_ptr, alloc_ptr *from_ptr) {
	assert(current_frame != NULL);
	assert(to_ptr != NULL);

	alloc_node *from_node = NULL;

	if(from_ptr)
		from_node = from_ptr->node;

	if(to_ptr == from_ptr || to_ptr->node == from_node)
		return;

	if(from_node)
		from_node->ref_count++;

	decrement_ref_count(to_ptr);

	to_ptr->node = from_node;
}


alloc_ptr *remove_ptrs(alloc_ptr *ptr_list, char *data, size_t start_pos, size_t end_pos) {
	assert(ptr_list != NULL);
	assert(data != NULL);
	assert(start_pos < end_pos);

	void *start = data + start_pos;
	void *end = data + end_pos;
	alloc_ptr *ptr = ptr_list;
	alloc_ptr *prev_ptr;
	
	while(ptr != &end_ptr && (void*)ptr >= start && (void*)ptr < end) {
		assert(ptr != NULL);
		decrement_ref_count(ptr);
		ptr = ptr->next;
	}

	ptr_list = prev_ptr = ptr;

	while(ptr != &end_ptr) {
		assert(ptr != NULL);

		while(ptr != &end_ptr && (void*)ptr >= start && (void*)ptr < end)
			ptr = ptr->next;
		
		while(prev_ptr != ptr) {
			decrement_ref_count(prev_ptr);
			prev_ptr = prev_ptr->next;

			assert(prev_ptr != NULL && prev_ptr != &end_ptr);
		}

		prev_ptr = ptr = ptr->next;
	}

	return ptr_list;
}


alloc_ptr *adjust_next_ptrs(alloc_ptr *ptr_list, ptrdiff_t offset) {
	assert(ptr_list != NULL);
	alloc_ptr *next_ptr;
	alloc_ptr *ptr = ptr_list;

	if(ptr == &end_ptr)
		return ptr;

	while(ptr->next != &end_ptr) {
		assert(ptr != NULL);

		next_ptr = ptr->next;
		ptr->next = ADJUST_OFFSET(ptr->next, offset);
		ptr = next_ptr;
	}

	return ADJUST_OFFSET(ptr_list, offset);
}


void adjust_node_ptrs(alloc_ptr *ptr_list, alloc_node *old_node, alloc_node *new_node) {
	alloc_ptr *ptr = ptr_list;

	while(ptr) {
		if(ptr->node == old_node)
			ptr->node = new_node;
		ptr = ptr->next;
	}
}


void adjust_node_tree_node_ptrs(alloc_node *parent, alloc_node *old_node, alloc_node *new_node) {
	if(!parent)
		return;

	adjust_node_ptrs(parent->ptr_list, old_node, new_node);
	adjust_node_tree_node_ptrs(parent->left, old_node, new_node);
	adjust_node_tree_node_ptrs(parent->right, old_node, new_node);
}


alloc_ptr *find_prev_ptr(alloc_ptr *ptr_list, alloc_ptr *current_ptr) {
	alloc_ptr *ptr = ptr_list;

	while(ptr && ptr->next != current_ptr)
		ptr = ptr->next;

	return ptr;
}


void decrement_list_ref_count(alloc_ptr *ptr) {
	while (ptr) {
		decrement_ref_count(ptr);
		ptr = ptr->next;
	}
}

void decrement_ref_count(alloc_ptr *ptr) {
	assert(ptr != NULL);

	if(!ptr->node)
		return;

	alloc_node *node = ptr->node;
	node->ref_count--;

	if (node->ref_count > 0)
		return;

	decrement_list_ref_count(node->ptr_list);

	remove_alloc_node(node);
	free_node(node);
}


alloc_node* malloc_node(size_t size) {
	assert(memory_usage <= max_usage);

	if(size == 0)
		return NULL;

	size_t malloc_amount = sizeof(alloc_node) + size;

	if(malloc_amount > max_usage)
		return NULL;

	if(malloc_amount + memory_usage > max_usage)
		alloc_gc();

	if(malloc_amount + memory_usage > max_usage)
		return NULL;

	alloc_node *node = malloc(malloc_amount);

	if(!node)
		return NULL;

	memory_usage += malloc_amount;
	return node;
}


void free_node(alloc_node *node) {
	if(node) {
		assert(memory_usage >= sizeof(alloc_node) + node->size);
		memory_usage -= sizeof(alloc_node) + node->size;
	}

	free(node);
}


void mark_nodes(alloc_ptr *ptr_list) {
	alloc_ptr *ptr = ptr_list;

	while(ptr) {
		if(ptr->node && ptr->node->size < max_usage_max) {
			ptr->node->size += max_usage_max;
			mark_nodes(ptr->node->ptr_list);
		}

		ptr = ptr->next;
	}
}


void gc_nodes(alloc_node *parent) {
	if(!parent)
		return;

	gc_nodes(parent->left);
	gc_nodes(parent->right);

	if(parent->size < max_usage_max) {
		parent->size -= max_usage_max;
		remove_alloc_node(parent);	// will break if the nodes become a balanced tree
		free_node(parent);
	}
}


void unmark_nodes(alloc_node *parent) {
	if(!parent)
		return;

	assert(parent->size >= max_usage_max);

	parent->size -= max_usage_max;

	unmark_nodes(parent->left);
	unmark_nodes(parent->right);
}


void add_alloc_node(alloc_node *parent, alloc_node *node) {
	assert(parent != NULL);
	assert(node != NULL);

	alloc_node *current_node = parent;
	alloc_node *parent_node;

	do {
		parent_node = current_node;

		assert(parent_node != NULL);
		assert(parent_node->left == NULL || parent_node->left < parent_node);
		assert(parent_node->right == NULL || parent_node->right > parent_node);

		if (node < parent_node)
			current_node = parent_node->left;
		else
			current_node = parent_node->right;
	} while (current_node);

	if (node < parent_node)
		parent_node->left = node;
	else
		parent_node->right = node;

	node->parent = parent_node;
}


void remove_alloc_node(alloc_node *node) {
	assert(node != NULL);
	assert(node->parent != NULL);

	alloc_node *parent = node->parent;
	alloc_node **child_ptr;

	if (parent->left == node)
		child_ptr = &parent->left;
	else
		child_ptr = &parent->right;

	if (!node->left) {
		*child_ptr = node->right;
	} else if (!node->right) {
		*child_ptr = node->left;
	} else {
		*child_ptr = node->left;
		add_alloc_node(node->left, node->right);
	}

	if(*child_ptr)
		(*child_ptr)->parent = parent;
}


alloc_node* find_alloc_node(alloc_ptr *contains) {
	assert(contains != NULL);

	alloc_node *node = &allocations;
	char *contains_ptr = (char*)contains;
	char *data = ALLOC_DATA(node);

	while (contains_ptr < data || contains_ptr > data + node->size) {
		assert(node->left == NULL || node->left < node);
		assert(node->right == NULL || node->right > node);

		if (contains_ptr < data)
			node = node->left;
		else
			node = node->right;

		if (!node)
			break;

		data = ALLOC_DATA(node);
	}

	return node;
}


void alloc_debug_info() {
	alloc_frame *frame = current_frame;

	puts("---------- FRAMES ----------");
	while(frame) {
		printf("%s:%d ret:(%p)", frame->filename, (int)frame->line_number, (void*)&frame->return_value);
		alloc_ptr_list_debug_info(frame->ptr_list);
		printf("\n");
		
		frame = frame->next_frame;
	}

	puts("-------- ALLOCATIONS -------");
	alloc_node_tree_debug_info(&allocations, 0);
	puts("----------------------------");
}


void alloc_ptr_list_debug_info(alloc_ptr *ptr_list) {
	alloc_ptr *ptr = ptr_list;

	while(ptr) {
		printf(" %p:%p", (void*)ptr, (void*)ptr->node);

		if(ptr == ptr->next) {
			printf(" ERROR: LOOP!");
			return;
		}

		ptr = ptr->next;
	}
}


void alloc_node_tree_debug_info(alloc_node *node, int depth) {
	if(!node)
		return;

	for(int i = 0; i < depth; i++)
		putchar('.');

	printf("%p ref:%d len:%d", (void*)node, (int)node->ref_count, (int)node->size);
	alloc_ptr_list_debug_info(node->ptr_list);
	printf("\n");

	if(node->left && node->left->parent != node)
		printf(" ERROR: left parent (%p). ", (void*)node->left->parent);

	if(node->right && node->right->parent != node)
		printf(" ERROR: right parent (%p). ", (void*)node->right->parent);

	alloc_node_tree_debug_info(node->left, depth + 1);
	alloc_node_tree_debug_info(node->right, depth + 1);
}
