#include <stdio.h>
#include <stdlib.h>

/*
 *  Author:     Kyle Lukaszek
 *  Date:       04/25/2024
 *  Email:      klukasze@uoguelph.ca
 */

/* -------------------------- Structs & Definitions ------------------------- */

// Function pointer types for the node_t node
typedef void (*free_func)(void *);
typedef void (*print_func)(void *);
typedef int (*compare_func)(void *, void *);
typedef const char *(*to_string_func)(void *);

// Doubly linked list node
typedef struct elem_t {
    void *data;
    struct elem_t *next;
    struct elem_t *prev;
} elem_t;

// Doubly linked list
typedef struct list_t {
    elem_t *head;
    elem_t *tail;
    int size;
    free_func free;
    compare_func compare;
    to_string_func to_str;
    print_func print;
} list_t;

// Tree node with n children
typedef struct node_t {
    void *data;
    struct node_t **children;
    int num_children;
} node_t;

// Tree with n children
typedef struct tree_t {
    node_t *root;
    int num_children;
} tree_t;

/* -------------------------- Doubly-LList Macros ------------------------- */

// Macro to define a list structure for a given type
// You can technically just use the list struct, but this is more explicit and I
// find it easier to read when using the functions
#define DEFINE_LIST_STRUCT(T) typedef struct list_t T##_list;

/*
 * Initialize a list elements
 * Data is a void pointer to the data
 * Next and prev are NULL and to be set when adding to the list
 */
#define init_elem(elem, _data)                                                 \
    ({                                                                         \
        elem->data = _data;                                                    \
        elem->next = NULL;                                                     \
        elem->prev = NULL;                                                     \
        elem;                                                                  \
    })

/*
 * Initialize a list
 * Head and tail are NULL
 * It is important to set the helper functions for the list
 * The helper functions are used to free, compare, convert to string, and print
 *
 * If you have to implement your own helper functions, it is important to make
 * sure that they are of type free_func, compare_func, to_string_func, and
 * print_func
 *
 * There are pre-defined helper functions for int and float types that can be
 * used with the list:
 *
 * - free_int, compare_int, int_to_str, print_int
 * - free_float, compare_float, float_to_str, print_float
 *
 * You can pass NULL for any of the helper functions, but it is recommended to
 * assign the helper functions to avoid memory leaks and undefined behaviour.
 */
#define list_init(list, _free, _compare, _to_str, _print)                      \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(                                                           \
                stderr,                                                        \
                "Error: list_init(%s, %s, %s, %s, %s) -> \'%s\' is NULL\n",    \
                #list, #_free, #_compare, #_to_str, #_print, #list);           \
        }                                                                      \
        list->head = NULL;                                                     \
        list->tail = NULL;                                                     \
        list->size = 0;                                                        \
        if (_free == NULL || _compare == NULL || _to_str == NULL ||            \
            _print == NULL) {                                                  \
            fprintf(stderr,                                                    \
                    "Warning: list_init(%s, %s, %s, %s, %s) -> NULL helper "   \
                    "function(s).\n",                                          \
                    #list, #_free, #_compare, #_to_str, #_print);              \
        }                                                                      \
        list->free = (_free);                                                  \
        list->compare = (_compare);                                            \
        list->to_str = (_to_str);                                              \
        list->print = (_print);                                                \
    })

/*
 * Create an empty doubly linked list
 * The list is created with the helper functions for the data type
 * The helper functions are used to free, compare, convert to string, and print
 * The helper functions can be NULL, but it is recommended to assign them
 * SEE: list_init
 */
#define create_list(T, free, compare, to_str, print)                           \
    ({                                                                         \
        T##_list *list = malloc(sizeof(T##_list));                             \
        free_func _free = (free);                                              \
        compare_func _compare = (compare);                                     \
        to_string_func _to_str = (to_str);                                     \
        print_func _print = (print);                                           \
        list_init(list, _free, _compare, _to_str, _print);                     \
        list;                                                                  \
    })

/*
 * Free the list and all of its elements
 * The list->free function is used to free the data. If the list->free function
 * is NULL, the data is freed using the free function. This will most likely
 * result in a memory leak if the data is not a primitive type.
 */
#define free_list(list)                                                        \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Warning: free_list(%s) -> \'%s\' is NULL\n",      \
                    #list, #list);                                             \
        } else {                                                               \
            elem_t *current = list->head;                                      \
            while (current != NULL) {                                          \
                elem_t *next = current->next;                                  \
                if (list->free != NULL)                                        \
                    list->free(current->data);                                 \
                else                                                           \
                    free(current->data);                                       \
                free(current);                                                 \
                current = next;                                                \
            }                                                                  \
            free(list);                                                        \
        }                                                                      \
    })

/*
 * Print the list
 * The list->print function is used to print the data for each element
 * The print_list macro applies the style of the print function
 */
#define print_list(list)                                                       \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Warning: print_list(%s) -> \'%s\' is NULL\n",     \
                    #list, #list);                                             \
        } else {                                                               \
            fprintf(stdout, "List: %s\n -> [", #list);                         \
            elem_t *current = list->head;                                      \
            while (current != NULL) {                                          \
                list->print(current->data);                                    \
                if (current->next != NULL)                                     \
                    fprintf(stdout, ", ");                                     \
                current = current->next;                                       \
            }                                                                  \
            fprintf(stdout, "]\n");                                            \
        }                                                                      \
    })

/*
 * Add an element to the list
 * The data is a void pointer to the data
 * Returns the data if added, NULL otherwise
 */
#define list_add(list, data)                                                   \
    ({                                                                         \
        typeof(list) _data = NULL;                                             \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_add(%s, %s) -> \'%s\' is NULL\n",     \
                    #list, #data, #list);                                      \
        } else {                                                               \
            _data = (data);                                                    \
            elem_t *element = (elem_t *)malloc(sizeof(elem_t));                \
            element = init_elem(element, _data);                               \
            if (list->head == NULL) {                                          \
                list->head = element;                                          \
            } else {                                                           \
                list->tail->next = element;                                    \
            }                                                                  \
            list->tail = element;                                              \
            list->size++;                                                      \
        }                                                                      \
        _data;                                                                 \
    })

/*
 * Get an element at a given index
 * Returns the element if found, NULL otherwise
 */
#define list_at(list, index)                                                   \
    ({                                                                         \
        elem_t *current = NULL;                                                \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_at(%s, %d) -> \'%s\' is NULL\n",      \
                    #list, index, #list);                                      \
        } else if (index < 0 || index >= list->size) {                         \
            fprintf(stderr, "Error: list_at(%s, %d) -> Index out of bounds\n", \
                    #list, index);                                             \
        } else {                                                               \
            current = list->head;                                              \
            for (int i = 0; i < index; i++) {                                  \
                current = current->next;                                       \
            }                                                                  \
        }                                                                      \
        current;                                                               \
    })

/*
 * Find an element in the list based on the data pointer
 * The compare function is used to determine if the data is equal
 * Returns the index if found, -1 otherwise
 *
 * Example:
 * int data = 5
 * list_find(list, &data) -> Returns the index of the element with data 5
 */
#define list_find(list, target)                                                \
    ({                                                                         \
        int i = -1;                                                            \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_find(%s, %s) -> \'%s\' is NULL\n",    \
                    #list, #target, #list);                                    \
        } else {                                                               \
            i = 0;                                                             \
            elem_t *current = list->head;                                      \
            while (current != NULL) {                                          \
                i++;                                                           \
                if (list->compare(current->data, target)) {                    \
                    break;                                                     \
                }                                                              \
                current = current->next;                                       \
            }                                                                  \
        }                                                                      \
        i;                                                                     \
    })

/*
Remove an element from the list based on the data pointer
The compare function is used to determine if the data is equal
The list->free function is used to free the data.

If your data is a primitive type, you can just pass the data directly
Example: list_remove(list, 5) -> Removes the element with data 5

If your data is a pointer, you must pass the address of the pointer
Example: list_remove(list, &data) -> Removes the element with data pointer
*/
#define list_remove(list, data)                                                \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_remove(%s, %s) -> \'%s\' is NULL\n",  \
                    #list, #data, #list);                                      \
        }                                                                      \
        elem_t *current = list->head;                                          \
        while (current != NULL) {                                              \
            if (list->compare(current->data, data) == 0) {                     \
                if (current->prev != NULL) {                                   \
                    current->prev->next = current->next;                       \
                } else {                                                       \
                    list->head = current->next;                                \
                }                                                              \
                if (current->next != NULL) {                                   \
                    current->next->prev = current->prev;                       \
                } else {                                                       \
                    list->tail = current->prev;                                \
                }                                                              \
                list->free(current->data);                                     \
                free(current);                                                 \
                list->size--;                                                  \
                break;                                                         \
            }                                                                  \
            current = current->next;                                           \
        }                                                                      \
    })

// Remove an element at a given index
#define list_remove_at(list, index)                                            \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr,                                                    \
                    "Error: list_remove_at(%s, %d) -> \'%s\' is NULL\n",       \
                    #list, index, #list);                                      \
        }                                                                      \
        if (index < 0 || index >= list->size) {                                \
            fprintf(stderr,                                                    \
                    "Error: list_remove_at(%s, %d) -> Index out of bounds\n",  \
                    #list, index);                                             \
        }                                                                      \
        elem_t *current = list->head;                                          \
        for (int i = 0; i < index; i++) {                                      \
            current = current->next;                                           \
        }                                                                      \
        if (current->prev != NULL) {                                           \
            current->prev->next = current->next;                               \
        } else {                                                               \
            list->head = current->next;                                        \
        }                                                                      \
        if (current->next != NULL) {                                           \
            current->next->prev = current->prev;                               \
        } else {                                                               \
            list->tail = current->prev;                                        \
        }                                                                      \
        list->free(current->data);                                             \
        free(current);                                                         \
        list->size--;                                                          \
    })

// Map a function to the data in the list
#define list_map(list, func)                                                   \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_map(%s, %s) -> \'%s\' is NULL\n",     \
                    #list, #func, #list);                                      \
        }                                                                      \
        if (func == NULL) {                                                    \
            fprintf(stderr, "Error: list_map(%s, %s) -> \'%s\' is NULL\n",     \
                    #list, #func, #func);                                      \
        }                                                                      \
        elem_t *current = list->head;                                          \
        while (current != NULL) {                                              \
            func(current->data);                                               \
            current = current->next;                                           \
        }                                                                      \
    })

// Filter the list based on the function
// Remove elements from the list that return 1 from the function
#define list_filter(list, func)                                                \
    ({                                                                         \
        if (list == NULL) {                                                    \
            fprintf(stderr, "Error: list_filter(%s, %s) -> \'%s\' is NULL\n",  \
                    #list, #func, #list);                                      \
        }                                                                      \
        if (func == NULL) {                                                    \
            fprintf(stderr, "Error: list_filter(%s, %s) -> \'%s\' is NULL\n",  \
                    #list, #func, #func);                                      \
        }                                                                      \
        elem_t *current = list->head;                                          \
        while (current != NULL) {                                              \
            if (func(current->data)) {                                         \
                elem *next = current->next;                                    \
                list_remove(list, current->data);                              \
                current = next;                                                \
            } else {                                                           \
                current = current->next;                                       \
            }                                                                  \
        }                                                                      \
    })

// Define the list structs for int and float types
DEFINE_LIST_STRUCT(int)
DEFINE_LIST_STRUCT(float)

/* -------------------------- Int Elem Functions -------------------------
 */

// Create an int pointer
void *create_int(int i) {
    int *i_ptr = (int *)malloc(sizeof(int));
    *i_ptr = i;
    return (void *)i_ptr;
}

// Compare int pointer data
int compare_int(void *a, void *b) {
    if (a == NULL && b == NULL) {
        return 0;
    } else if (a == NULL) {
        return 0;
    } else if (b == NULL) {
        return 0;
    }

    int *ia = (int *)a;
    int *ib = (int *)b;
    return ((*ia - *ib) == 0);
}

// Print int pointer data
void print_int(void *ptr) {
    if (ptr == NULL)
        return;
    fprintf(stdout, "%d", *(int *)ptr);
}

// Free int pointer data
void free_int(void *ptr) {
    if (ptr == NULL)
        return;
    free(ptr);
}

// Convert int pointer data to string
// Must free the returned string
const char *int_to_str(void *i) {
    char *str = (char *)malloc(12);
    sprintf(str, "%d", *(int *)i);
    return str;
}

/* -------------------------- Float Elem Functions ------------------------- */

// Create a float pointer
void *create_float(float f) {
    float *f_ptr = (float *)malloc(sizeof(float));
    *f_ptr = f;
    return (void *)f_ptr;
}

// Compare float pointer data
// Return 1 if equal, 0 if not equal
int compare_float(void *a, void *b) {
    if (a == NULL && b == NULL) {
        return 0;
    } else if (a == NULL) {
        return 0;
    } else if (b == NULL) {
        return 0;
    }

    float *fa = (float *)a;
    float *fb = (float *)b;
    return ((*fa - *fb) == 0);
}

// Print float pointer data
void print_float(void *ptr) {
    if (ptr == NULL)
        return;
    fprintf(stdout, "%f", *(float *)ptr);
}

// Free float pointer data
void free_float(void *ptr) {
    if (ptr == NULL)
        return;
    free(ptr);
}

// Convert float pointer data to to_string
// Must free the returned string
const char *float_to_str(float *f) {
    char *str = (char *)malloc(12);
    sprintf(str, "%f", *f);
    return str;
}

/* -------------------------- Char Elem Functions ------------------------- */
