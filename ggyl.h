#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

/*
 *  Author:     Kyle Lukaszek
 *  Date:       04/25/2024
 *  Email:      klukasze@uoguelph.ca
 */

/* -------- My Personal Data Structure Definitions --------- */

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

// Macro to define a list structure for a given type
// You can technically just use the list struct, but this is more explicit and I
// find it easier to read when using the functions
#define DEFINE_LIST_STRUCT(T) typedef struct list_t T##_list;

// Define the list structs for int and float types
DEFINE_LIST_STRUCT(int)
DEFINE_LIST_STRUCT(float)

// Tree node with n children
typedef struct node_t {
    void *data;
    struct node_t *parent;
    struct node_t **children;
    int num_children;
} node_t;

// Tree with n children
typedef struct tree_t {
    node_t *root;
    int num_children;
    free_func free;
    compare_func compare;
    to_string_func to_str;
    print_func print;
} tree_t;

// Macro to define a tree structure for a given type
#define DEFINE_TREE_STRUCT(T) typedef struct tree_t T##_tree;

DEFINE_TREE_STRUCT(int)
DEFINE_TREE_STRUCT(float)

/* -------------------- GGYL Definitions ------------------------- */

#define MAX_LEN 1024
#define MAX_REGEX 128
#define MAX_WATCHES 1024

typedef struct {
    regex_t *regex;
    int compiled;
} regex_entry;

typedef struct {
    int fd;
    char dir[MAX_LEN];
    char cmd[MAX_LEN];
    regex_entry **regex_entries;
    int num_patterns;
    int_tree *wd_entries;
    uint32_t mask;
} monitor_t;

/* -------------------------- Doubly-LList Macros ------------------------- */

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
        void *_data = NULL;                                                    \
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

/* -------------------------- Tree Macros ------------------------- */

/*
 * Create a node with n children
 * The data is a void pointer to the data
 * The number of children is set
 * The children are NULL and to be set when adding nodes to the tree
 */
#define create_node(_data)                                                    \
    ({                                                                         \
        node_t *node = (node_t *)malloc(sizeof(node_t));                       \
        node->data = (_data);                                                  \
        node->num_children = 0;                                                \
        node->children = NULL;                                                 \
        node->parent = NULL;                                                   \
        node;                                                                  \
    })

/*
 * Realloc the list of children and add a child node of the data
 * The function returns a pointer to the data if added, NULL otherwise
 * */
#define _add_child(_node, _data)                                               \
    ({                                                                         \
        node_t *child = NULL;                                                  \
        if (_node == NULL) {                                                   \
            fprintf(stderr, "Error: add_child(%s, %s) -> \'%s\' is NULL\n",    \
                    #_node, #_data, #_node);                                   \
        } else {                                                               \
            if (_node->children == NULL) {                                     \
                _node->children = (node_t **)malloc(sizeof(node_t *));         \
            } else {                                                           \
                _node->children = (node_t **)realloc(                          \
                    _node->children,                                           \
                    sizeof(node_t *) * (_node->num_children + 1));             \
            }                                                                  \
            child = create_node(_data);                                       \
            child->parent = _node;                                             \
            _node->children[_node->num_children] = child;                      \
            _node->num_children++;                                             \
        }                                                                      \
        child;                                                                 \
    })

/*
 * Free a node and all of its children
 * The node->free function is used to free the data. If the node->free function
 * is NULL, the data is freed using the free function. This will most likely
 * result in a memory leak if the data is not a primitive type.
 */
#define free_nodes(node, free_data) ({ _free_nodes(node, free_data); })

// Helper function to free nodes since the a macro cannot recurse
void _free_nodes(node_t *node, free_func free_data) {

    if (node == NULL) {
        fprintf(stderr, "Error: free_nodes(node, free_data) -> NULL");
        return;
    } else {
        if (node->children != NULL) {
            for (int i = 0; i < node->num_children; i++) {
                _free_nodes(node->children[i], free_data);
            }
            free(node->children);
        }
        free_data(node->data);
        free(node);
    }
}

/*
 * Remove a node from the tree and link its children to the parent node.
 * The tree->free function is passed to free the data. If the tree->free
 * function is NULL, the data is freed using the free() function. This will most
 * likely result in a memory leak if the data is not a primitive type.
 */
#define _remove_node(node, free_data)                                          \
    ({                                                                         \
        if (node == NULL) {                                                    \
            fprintf(stderr, "Error: remove_node(%s) -> \'%s\' is NULL\n",      \
                    #node, #node);                                             \
        } else {                                                               \
            /* Link the children to the parent node */                         \
            if (node->children != NULL) {                                      \
                for (int i = 0; i < node->num_children; i++) {                 \
                    node->children[i]->parent = node->parent;                  \
                }                                                              \
            }                                                                  \
            if (node->parent != NULL) {                                        \
                /* Remove the node from the parent's children */               \
                for (int i = 0; i < node->parent->num_children; i++) {         \
                    if (node->parent->children[i] == node) {                   \
                        node->parent->children[i] = NULL;                      \
                        node->num_children--;                                  \
                    }                                                          \
                }                                                              \
                /* Shift the children to the left and realloc */               \
                for (int i = 0; i < node->parent) {                            \
                    if (node->parent->children[i] == NULL) {                   \
                        for (int j = i; j < node->parent->num_children; j++) { \
                            node->parent->children[j] =                        \
                                node->parent->children[j + 1];                 \
                        }                                                      \
                    }                                                          \
                }                                                              \
                /* Increment parent's num_children and realloc children */     \
                node->parent->num_children += node->num_children;              \
                node->parent->children = (node_t **)realloc(                   \
                    node->parent->children,                                    \
                    sizeof(node_t *) * node->parent->num_children);            \
                /* Add the children to the parent's children */                \
                for (int i = 0; i < node->num_children; i++) {                 \
                    int index =                                                \
                        node->parent->num_children - node->num_children + i;   \
                    node->parent->children[index] = node->children[i];         \
                }                                                              \
            }                                                                  \
            /* Free the data using the free_data argument */                   \
            if (free_data != NULL) {                                           \
                free_data(node->data);                                         \
            } else {                                                           \
                free(node->data);                                              \
            }                                                                  \
            /* Free the children list and the node */                          \
            /* Do not free the parent attribute */                             \
            free(node->children);                                              \
            free(node);                                                        \
        }                                                                      \
    })

/*
 * Initialize a tree
 * The root is NULL and the number of children is set
 */
#define _tree_init(tree, _free, _compare, _to_str, _print)                     \
    ({                                                                         \
        if (tree == NULL) {                                                    \
            fprintf(stderr,                                                    \
                    "Error: tree_init(%s, %s, %s, %s, %s) -> \'%s\' "          \
                    "is NULL\n",                                               \
                    #tree, #_free, #_compare, #_to_str, #_print, #tree);       \
        }                                                                      \
        tree->root = NULL;                                                     \
        tree->num_children = 0;                                                \
        tree->free = (_free);                                                  \
        tree->compare = (_compare);                                            \
        tree->to_str = (_to_str);                                              \
        tree->print = (_print);                                                \
    })

/*
 * Create an empty tree
 * The tree returns with 0 children and the root is NULL
 */
#define create_tree(T, free, compare, to_str, print)                           \
    ({                                                                         \
        T##_tree *tree = (T##_tree *)malloc(sizeof(T##_tree));                 \
        free_func _free = (free);                                              \
        compare_func _compare = (compare);                                     \
        to_string_func _to_str = (to_str);                                     \
        print_func _print = (print);                                           \
        _tree_init(tree, _free, _compare, _to_str, _print);                    \
        tree;                                                                  \
    })

/*
 * Add a child node to the target node of a tree. If the target node is not
 * null, add a child to the target node. If the target node is NULL, set the
 * tree root to the data. The data is a void pointer to the data. The function
 * returns a pointer to the data if added, NULL otherwise.
 *
 * Calls: add_child(node, data)
 */
#define tree_add(tree, node, data)                                             \
    ({                                                                         \
        void *_data = NULL;                                                    \
        node_t *_node = (node);                                                \
        node_t *added = NULL;                                                  \
        if (tree == NULL) {                                                    \
            fprintf(stderr, "Error: tree_add(%s, %s, %s) -> \'%s\' is NULL\n", \
                    #tree, #node, #data, #tree);                               \
        } else {                                                               \
            _data = (data);                                                    \
            if (_node == NULL) {                                               \
                if (tree->root == NULL) {                                      \
                    tree->root = create_node(_data);                          \
                } else {                                                       \
                    added = _add_child(_node, _data);                          \
                }                                                              \
            } else {                                                           \
                added = _add_child(_node, _data);                              \
            }                                                                  \
            tree->num_children++;                                              \
        }                                                                      \
        added;                                                                 \
    })

/* Return the first node found with matching data */
#define tree_find(tree, target_data)                                           \
    ({                                                                         \
        int index = -1;                                                        \
        void *_target_data = NULL;                                             \
        if (tree == NULL) {                                                    \
            fprintf(stderr, "Error: tree_find(%s, %s) -> \'%s\' is NULL\n",    \
                    #tree, #target_data, #tree);                               \
        } else {                                                               \
            _target_data = (target_data);                                      \
            node_t *current = tree->root;                                      \
            index = _find_node(current, _target_data, tree->compare);          \
        }                                                                      \
        index;                                                                 \
    })

// Helper function to find a node with matching data
// Returns a pointer to the node if found, NULL otherwise
node_t *_find_node(node_t *node, void *target_data, compare_func compare) {
    if (node == NULL) {
        return NULL;
    }
    if (compare(node->data, target_data)) {
        return node;
    }
    for (int i = 0; i < node->num_children; i++) {
        node_t *found = _find_node(node->children[i], target_data, compare);
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

/*
 *
 * Insert data into the tree at the first occurrence of the target data.
 * Iterate through all nodes in the tree and find the first node with matching
 * data. Data is compared using the tree->compare function. If the target data
 * is found, add the data as a child to the target node. The function returns a
 * pointer to the data if added, NULL otherwise.
 */
#define tree_insert(tree, target_data, data)                                   \
    ({                                                                         \
        void *_data = NULL;                                                    \
        void *_target_data = NULL;                                             \
        node_t *target = NULL;                                                 \
        node_t *added = NULL;                                                  \
        if (tree == NULL) {                                                    \
            fprintf(stderr,                                                    \
                    "Error: tree_insert(%s, %s, %s) -> \'%s\' is NULL\n",      \
                    #tree, #target_data, #data, #tree);                        \
        } else {                                                               \
            _data = (data);                                                    \
            _target_data = (target_data);                                      \
            node_t *target = tree_find(tree, _target_data);                    \
            if (target != -1) {                                                \
                added = _add_child(target, _data);                             \
            }                                                                  \
        }                                                                      \
        added;                                                                 \
    })

/*
 * Free all tree nodes and the tree itself.
 * free_node() is used to free the nodes and their children.
 */
#define free_tree(tree)                                                        \
    ({                                                                         \
        if (tree == NULL) {                                                    \
            fprintf(stderr, "Error: free_tree(%s) -> \'%s\' is NULL\n", #tree, \
                    #tree);                                                    \
        } else {                                                               \
            if (tree->root != NULL) {                                          \
                free_nodes(tree->root, tree->free);                            \
            }                                                                  \
            free(tree);                                                        \
        }                                                                      \
    })

/*
 * Print the tree
 */
#define print_tree(tree)                                                       \
    ({                                                                         \
        if (tree == NULL) {                                                    \
            fprintf(stderr, "Error: print_tree(%s) -> \'%s\' is NULL\n",       \
                    #tree, #tree);                                             \
        } else {                                                               \
            fprintf(stdout, "Tree: %s\n", #tree);                              \
            if (tree->root != NULL) {                                          \
                _print_tree(tree, tree->root, 0, tree->print);                 \
            }                                                                  \
        }                                                                      \
    })

// Helper function to print the tree
void _print_tree(tree_t *tree, node_t *node, int depth, print_func print) {
    if (node == NULL) {
        return;
    }
    for (int i = 0; i < depth; i++) {
        fprintf(stdout, "  ");
    }
    tree->print(node->data);
    fprintf(stdout, "\n");
    for (int i = 0; i < node->num_children; i++) {
        _print_tree(tree, node->children[i], depth + 1, print);
    }
}

/* -------------------------- Int Elem Functions ------------------------ */

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
// Not needed since the data is a primitive type and can be passed with &...
void free_int(void *ptr) { return; }

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
