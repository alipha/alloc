#ifndef CONTAINER_ARRAY_H
#define CONTAINER_ARRAY_H

#include "alloc.h"

#define ARRAY_INIT_NULL(type, name) \
	array_##type name = { 0 }

#define ARRAY_INIT(type, name, elements, reserved)	\
	array_##type name = { 0 };						\
	array_##type##_init(name, elements, reserved)		


#define TEMPLATE_ARRAY(type) 													\
	typedef struct internal_array_##type {										\
		alloc_ptr ptr;															\
		size_t elements_used;													\
	} array_##type[1], *ret_array_##type;										\
																				\
	static void array_##type##_init(array_##type var, size_t elements, size_t reserved) {		\
		if(reserved < elements)													\
			reserved = elements;												\
		alloc_init(&var->ptr, reserved * sizeof(type));							\
		if(var->ptr.node)														\
			var->elements_used = elements;										\
		else																	\
			var->elements_used = 0;												\
	}																			\
																				\
	static ret_array_##type array_##type##_new(size_t elements) {				\
		ret_array_##type ret = alloc_return_new(elements * sizeof(type));		\
		if(ret)																	\
			ret->elements_used = elements;										\
		return ret;																\
	}																			\
																				\
	static void array_##type##_assign(array_##type to, array_##type from) {		\
		if(from) {																\
			alloc_assign(&to->ptr, &from->ptr);									\
			to->elements_used = from->elements_used;							\
		} else {																\
			alloc_assign(&to->ptr, NULL);										\
			to->elements_used = 0;												\
		}																		\
	}																			\
																				\
	static void array_##type##_global_assign(array_##type to, array_##type from) {	\
		if(from) {																\
			alloc_global_assign(&to->ptr, &from->ptr);							\
			to->elements_used = from->elements_used;							\
		} else {																\
			alloc_global_assign(&to->ptr, NULL);								\
			to->elements_used = 0;												\
		}																		\
	}																			\
																				\
	static type* array_##type##_raw(array_##type var) {							\
		return alloc_data(&var->ptr);											\
	}																			\
																				\
	static size_t array_##type##_size(array_##type var) {						\
		return var->elements_used;												\
	}																			\
																				\
	static BOOL array_##type##_add(array_##type var, type element) {			\
		size_t size = alloc_size(&var->ptr);									\
		if(var->elements_used * sizeof(type) == alloc_size(&var->ptr)) {		\
			struct alloc_node *new_node = alloc_resize(var->ptr.node, (size + sizeof(type)) * 2);\
			if(!new_node)														\
				return FALSE;													\
			var->ptr.node = new_node;											\
		}																		\
		array_##type##_raw(var)[var->elements_used++] = element;				\
		return TRUE;															\
	}																			\
																				\
	static type array_##type##_get(array_##type var, size_t pos) {				\
		type value = {0};														\
		if(pos < var->elements_used)											\
			value = array_##type##_raw(var)[pos];								\
		return value;															\
	}																			\
																				\
	static BOOL array_##type##_set(array_##type var, size_t pos, type value) {	\
		if(pos >= var->elements_used)											\
			return FALSE;														\
		array_##type##_raw(var)[pos] = value;									\
		return TRUE;															\
	}																			\
																				\
	typedef type type##_expected_semicolon_after_macro



#define TEMPLATE_ARRAY_OBJ(type) 												\
	typedef struct internal_array_##type {										\
		alloc_ptr ptr;															\
		size_t elements_used;													\
	} array_##type[1], *ret_array_##type;										\
																				\
	static void array_##type##_init(array_##type var, size_t elements, size_t reserved) {	\
		if(reserved < elements)													\
			reserved = elements;												\
		alloc_init(&var->ptr, reserved * sizeof(type));							\
		if(var->ptr.node)														\
			var->elements_used = elements;										\
		else																	\
			var->elements_used = 0;												\
	}																			\
																				\
	static ret_array_##type array_##type##_new(size_t elements) {				\
		ret_array_##type ret = alloc_return_new(elements * sizeof(type));		\
		if(ret)																	\
			ret->elements_used = elements;										\
		return ret;																\
	}																			\
																				\
	static void array_##type##_assign(array_##type to, array_##type from) {		\
		if(from) {																\
			alloc_assign(&to->ptr, &from->ptr);									\
			to->elements_used = from->elements_used;							\
		} else {																\
			alloc_assign(&to->ptr, NULL);										\
			to->elements_used = 0;												\
		}																		\
	}																			\
																				\
	static void array_##type##_global_assign(array_##type to, array_##type from) {	\
		if(from) {																\
			alloc_global_assign(&to->ptr, &from->ptr);							\
			to->elements_used = from->elements_used;							\
		} else {																\
			alloc_global_assign(&to->ptr, NULL);								\
			to->elements_used = 0;												\
		}																		\
	}																			\
																				\
	static ret_##type array_##type##_raw(array_##type var) {					\
		return alloc_data(&var->ptr);											\
	}																			\
																				\
	static size_t array_##type##_size(array_##type var) {						\
		return var->elements_used;												\
	}																			\
																				\
	static BOOL array_##type##_add(array_##type var, type element) {			\
		size_t size = alloc_size(&var->ptr);									\
		if(var->elements_used * sizeof(type) == alloc_size(&var->ptr)) {		\
			struct alloc_node *new_node = alloc_resize(var->ptr.node, (size + sizeof(type)) * 2);\
			if(!new_node)														\
				return FALSE;													\
			var->ptr.node = new_node;											\
		}																		\
		type##_assign(&array_##type##_raw(var)[var->elements_used++], element);	\
		return TRUE;															\
	}																			\
																				\
	static ret_##type array_##type##_get(array_##type var, size_t pos) {		\
		if(pos < var->elements_used)											\
			return &array_##type##_raw(var)[pos];								\
		else																	\
			return NULL;														\
	}																			\
																				\
	static BOOL array_##type##_set(array_##type var, size_t pos, type value) {	\
		if(pos >= var->elements_used)											\
			return FALSE;														\
		type##_assign(&array_##type##_raw(var)[pos], value);					\
		return TRUE;															\
	}																			\
																				\
	typedef type type##obj_expected_semicolon_after_macro



#define TEMPLATE_ARRAY_TYPEDEF(type, alias)										\
	typedef array_##type alias; 												\
	typedef ret_array_##type ret_##alias;										\
																				\
	static void alias##_init(alias var, size_t elements, size_t reserved) {		\
		array_##type##_init(var, elements, reserved);							\
	}																			\
																				\
	static ret_##alias alias##_new(size_t elements) {							\
		return array_##type##_new(elements); 									\
	}																			\
																				\
	static void alias##_assign(alias to, alias from) {							\
		array_##type##_assign(to, from); 										\
	}																			\
																				\
	static void alias##_global_assign(alias to, alias from) {					\
		array_##type##_global_assign(to, from); 								\
	}																			\
																				\
	static type* alias##_raw(alias var) {										\
		return array_##type##_raw(var);											\
	}																			\
																				\
	static size_t alias##_size(alias var) {										\
		return array_##type##_size(var);										\
	}																			\
																				\
	static BOOL alias##_add(alias var, type element) {							\
		return array_##type##_add(var, element); 								\
	}																			\
																				\
	static type alias##_get(alias var, size_t pos) {							\
		return array_##type##_get(var, pos); 									\
	}																			\
																				\
	static BOOL alias##_set(alias var, size_t pos, type value) {				\
		return array_##type##_set(var, pos, value);								\
	}																			\
																				\
	typedef alias alias##typedef_expected_semicolon_after_macro
#endif
