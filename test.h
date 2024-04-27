#include <stdio.h>
#include <stdlib.h>

/*
 *  Author:     Kyle Lukaszek
 *  Date:       04/25/2024
 *  Email:      klukasze@uoguelph.ca
 *
 *  Description:
 *
 *  This file contains a set of macros and functions to create and manage node
 *  based data structures. At the core of this file is the node_t structure
 *  which is a generic structure that can be used to store any type of data. At
 *  the moment, node structures can be used to create linked lists or trees with
 *  any number of children per node.
 *
 *  The idea behind this file is to let me copy and paste my code into any
 *  project and have a working data structure that can be used to store any type
 *  of data.
 *
 *  The data structures are currently all strongly typed, meaning that you must
 *  use the correct type of node for the data you are storing. For example, if
 *  you want to you must DEINE_NODE_STRUCT(int) to let the compiler know to
 *  construct the int_node, and int_list structures.
 *
 *  A node structure is defined as follows:
 *
 *  typedef struct node_t {
 *      T *data;              // Pointer to the data to store in the node
 *      struct node_t **next;   // Pointer to the children of the node
 *      node_config type;       // Configuration enum (LIST or TREE)
 *      int num_children;
 *      void (*free)(T *);                // Function pointer to free the node
 *      void (*print)(T *);               // Function pointer to print the node
 *      int (*compare)(T *, T *);         // Function pointer to compare nodes
 *  } node_t;
 *
 *  Future Work:
 *
 *  - Implement queue data structures
 *      - Queue should be properly implemented such that all data is contiguous
 *      - Queue size should be static
 *      - Queue should have pointers to the front and back of the queue
 *
 */

/* ---------------------------- Type Definitions --------------------------- */

// Enumeration for the node types
typedef enum { LIST, TREE } node_config;

// Function pointer types for the node_t node
typedef void (*free_func)(void *);
typedef void (*print_func)(void *);
typedef int (*compare_func)(void *, void *);

// Structure for the node_t node
// It might be better to declare the function pointers in the list_t and tree_t
// structs to save 24 bytes per node, but I like having them here :)
typedef struct node_t {
    void *data;           // Pointer to the data to store in the node
    struct node_t **next; // Pointer to the children of the node
    node_config type;     // Configuration enum (LIST or TREE)
    size_t num_children;
    free_func free;       // Function pointer to free the node
    print_func print;     // Function pointer to print the node
    compare_func compare; // Function pointer to compare nodes
} node_t;

// Structure for the list_t linked-list
// The list_t structure is a wrapper around the generic node_t structure
// We can use any type of node_t node in the list.
// The macro DEFINE_NODE_STRUCT will be used to define additional list_t structs
// for strict typing of the list
typedef struct list_t {
    node_t *head;  // Pointer to the head of the list
    node_t *tail;  // Pointer to the tail of the list
    size_t length; // Number of entries in the list
} list_t;

// Structure for the stack_t stack
// Identical to the list_t structure.
typedef struct stack_t {
    node_t *head;  // Pointer to the head of the stack
    node_t *tail;  // Pointer to the tail of the stack
    size_t length; // Number of entries in the stack
} stack_t;

// Macro to define a node_t structure
#define DEFINE_NODE_STRUCT(T)                                                  \
    typedef struct T##_node {                                                  \
        T *data;                                                               \
        struct T##_node **next;                                                \
        node_config type;                                                      \
        size_t num_children;                                                   \
        void (*free)(T *);                                                     \
        void (*print)(T *);                                                    \
        int (*compare)(T *, T *);                                              \
    } T##_node;                                                                \
                                                                               \
    typedef struct T##_list {                                                  \
        T##_node *head;                                                        \
        T##_node *tail;                                                        \
        int length;                                                            \
    } T##_list;                                                                \
                                                                               \
    typedef struct T##_stack {                                                 \
        T##_node *head;                                                        \
        T##_node *tail;                                                        \
        int length;                                                            \
    } T##_stack;

// Define the node_t structure for int and float data types
DEFINE_NODE_STRUCT(int);
DEFINE_NODE_STRUCT(float);

/* ---------------------------- Node Functions ------------------------------ */

// Generic function to initialize a node_t node with a given data and type.
// You should not use this function directly, use CREATE_NODE instead unless you
// know what you are doing
#define init_node(node, _data, node_config, _free, _print, _compare)           \
    ({                                                                         \
        node->data = _data;                                                    \
        node->type = node_config;                                              \
        node->next = NULL;                                                     \
        node->num_children = 0;                                                \
        node->free = _free;                                                    \
        node->print = _print;                                                  \
        node->compare = _compare;                                              \
    })

/**
 * Creates a node of type `T`, initializes it with `data`, `node_config`, and
 * function pointers for `free`, `print`, and `compare`.
 * @param T The node type definition struct. (int_node, float_node, etc.)
 * @param data Pointer to the data to store in the node.
 * @param node_config Configuration enum (LIST or TREE).
 * @param free Function pointer to free the node data.
 * @param print Function pointer to print the node data.
 * @param compare Function pointer to compare two node data elements.
 * @return Pointer to the newly created node or NULL if memory allocation fails.
 */
#define create_node(T, data, node_config, free, print, compare)                \
    ({                                                                         \
        T##_node *node = (T##_node *)malloc(sizeof(T##_node));                 \
        if (node == NULL) {                                                    \
            printf("Error: create_node() -> Could not allocate memory for "    \
                   "node\n");                                                  \
            node = NULL;                                                       \
        } else {                                                               \
            typeof(data) _data = (data); /* Deduce type from T */              \
            void (*_free)(typeof(_data)) = (free);                             \
            void (*_print)(typeof(_data)) = (print);                           \
            int (*_compare)(typeof(_data), typeof(_data)) = (compare);         \
            init_node(node, _data, node_config, _free, _print, _compare);      \
        }                                                                      \
        node;                                                                  \
    })

// Generic function to create a list node (1 child) with a given data and
// children
//
// Calls: create_node
#define create_list_node(T, data, free, print, compare)                        \
    create_node(T, data, LIST, free, print, compare)

// Generic function to create a tree node (n children) with a given data and
// children
//
// Calls: create_node
#define create_tree_node(T, data, free, print, compare)                        \
    create_node(T, data, TREE, free, print, compare)

// Generic function to add a child to a node_t node, this will catch if the node
// is a list node or a tree node
//
// Calls: _add_child
#define add_child(parent, child)                                               \
    ({                                                                         \
        if (parent == NULL || child == NULL) {                                 \
            printf("Error: add_child() -> Parent or child node is NULL\n");    \
        } else {                                                               \
            node_config parent_type = parent->type;                            \
            switch (parent_type) {                                             \
                case LIST:                                                     \
                    {                                                          \
                        if (parent->num_children != 0 ||                       \
                            parent->next != NULL) {                            \
                            printf("Error: add_child() -> Parent \"%s\" is "   \
                                   "not a list node\n",                        \
                                   #parent);                                   \
                        } else {                                               \
                            parent->next = (typeof(parent) *)malloc(           \
                                sizeof(typeof(parent)));                       \
                            parent->next[0] = child;                           \
                            parent->num_children++;                            \
                        }                                                      \
                        break;                                                 \
                    }                                                          \
                case TREE:                                                     \
                    {                                                          \
                        parent->next = (typeof(parent) *)realloc(              \
                            parent->next, (parent->num_children + 1) *         \
                                              sizeof(typeof(parent)));         \
                        if (parent->next == NULL) {                            \
                            printf("Error: add_child() -> Could not allocate " \
                                   "memory for "                               \
                                   "\"%s\"\n",                                 \
                                   #parent);                                   \
                        } else {                                               \
                            parent->next[parent->num_children] =               \
                                (typeof(parent))child;                         \
                            parent->num_children++;                            \
                        }                                                      \
                        break;                                                 \
                    }                                                          \
            }                                                                  \
        }                                                                      \
    })

#define get_node_data(node) ((node)->data)

// Free a node_t node with a given data type
// Checks if the node is NULL, if the data is NULL, and if the free function is
// null If the data is not NULL, it will free the data using the free function
// if it exists Otherwise, it will use free() It will then free all of the
// children of the node recursively If you do not want to free the node's
// children from your structure, use remove_node() instead
//
// Calls: _free_node
#define free_node(node) _free_node((void *)node)

// Generic function to remove a node and all of its children from a list or tree
void _free_node(void *data) {
    node_t *node = (node_t *)data;
    if (node == NULL) {
        printf("Error: _free_node() -> Node is NULL\n");
    } else {
        if (node->data != NULL) {
            if (node->free != NULL) {
                node->free(node->data);
            } else {
                printf("Warning: _free_node() -> No free function provided for "
                       "node\n");
                free(node->data);
            }
        }
        for (int i = 0; i < node->num_children; i++) {
            _free_node((void *)node->next[i]);
        }
    }
    free(node->next);
    free(node);
}

// Remove a node_t node from the tree and free data if it exists.
// This function will search for the parent of the node and remove the node from
// the parent's children. The children of the target node will be orphaned to
// parent node and will not be freed. If you want to free the children of the
// node use free_node instead.
//
// Calls: _find_and_remove_node -> _remove_node
#define remove_node(root, target)                                              \
    ({                                                                         \
        typeof(root) _root = (root);                                           \
        typeof(target) _target = (target);                                     \
        void *current = _root;                                                 \
        _find_and_remove_node(&current, _target);                              \
    })

// Helper function to find a node in the tree and remove it
void _find_and_remove_node(void **cur, void *tar) {
    if (*cur == NULL)
        return;

    // Cast to generic node_t pointers so we can access attributes
    node_t **current = (node_t **)cur;
    node_t *target = (node_t *)tar;

    // Iterate through the children of the current node to find the target node
    // Use the compare function to determine if the current node is the target
    // node
    for (int i = 0; i < (*current)->num_children; i++) {
        if ((*current)->compare((*current)->next[i], target)) {
            // Remove the target node from the current node's children
            for (int j = i; j < (*current)->num_children - 1; j++) {
                (*current)->next[j] = (*current)->next[j + 1];
            }
            _free_node(target);
            (*current)->num_children--;
            return;
        }

        // Recursively search for the target node in the current node's children
        _find_and_remove_node((void **)&(*current)->next[i], (void *)target);
    }
}

// Function to remove a target node from the parent node's children array
#define remove_node_at(parent, index)                                          \
    ({                                                                         \
        typeof(parent) _parent = (parent);                                     \
        typeof(index) _index = (index);                                        \
        int success = _remove_node_at(_parent, _index);                        \
        success;                                                               \
    })

// Helper function to remove a node at a specific index from the parent node
int _remove_node_at(void *p, int index) {
    if (p == NULL) {
        printf("Error: remove_node_at() -> Parent node is NULL\n");
        return 0;
    }

    node_t *parent = (node_t *)p;

    if (index < 0 || index >= parent->num_children) {
        return 0;
    }

    // Get the node to remove
    node_t *to_remove = parent->next[index];

    // Reattach the children of the node to the parent
    for (int i = index; i < parent->num_children; i++) {
        parent->next[parent->num_children++] = to_remove->next[i];
    }

    // Remove the reference to the node from the parent's children array
    parent->num_children--;
    for (int i = index; i < parent->num_children; i++) {
        parent->next[i] = parent->next[i + 1];
    }

    _free_node(to_remove);
    return 1;
}

// Print the information of an individual node_t node
// Uses the node_t->print function if it exists, otherwise print warnings
//
// Output:
// Node:
// -> Data: 1
// -> Type: List or Tree
#define print_node(node)                                                       \
    ({                                                                         \
        if (node == NULL) {                                                    \
            printf("Error: print_node() -> Node is NULL\n");                   \
        } else {                                                               \
            if (node->print != NULL) {                                         \
                printf("Node: %s\n-> Data: ", #node);                          \
                node->print(node->data);                                       \
                printf("\n-> Type: %s\n",                                      \
                       node->type == LIST ? "List" : "Tree");                  \
                printf("-> Children: %ld\n", node->num_children);              \
            } else {                                                           \
                printf("Warning: print_node() -> No print function provided "  \
                       "for node\n");                                          \
                printf("Node:\n-> Type: %s\n",                                 \
                       node->type == LIST ? "List" : "Tree");                  \
            }                                                                  \
        }                                                                      \
    })

// Print out a node_t node and all of its children recursively
//
// Calls: _print_list_helper || _print_tree_helper
//
// Output:
//
// List: 1 -> 2 -> 3 -> 4
//
// Tree:
// 1
//  -> 2 children
//      2
//      -> 1 children
#define print_node_rec(node)                                                   \
    ({                                                                         \
        if (node == NULL) {                                                    \
            printf("Error: print_node_rec(%s) -> Node is NULL\n", #node);      \
        } else {                                                               \
            switch (node->type) {                                              \
                case LIST:                                                     \
                    {                                                          \
                        _print_list_helper(node);                              \
                        printf("\n");                                          \
                        break;                                                 \
                    }                                                          \
                case TREE:                                                     \
                    {                                                          \
                        _print_tree_helper(node, 0);                           \
                        break;                                                 \
                    }                                                          \
            }                                                                  \
        }                                                                      \
    })

// Helper function to print a list of node_t nodes
void _print_list_helper(void *data) {
    if (data == NULL) {
        return;
    }

    // Cast the data to a node_t node so we can access general attributes
    node_t *node = (node_t *)data;

    // Print the node data if a print function is provided
    if (node->print != NULL) {
        node->print(node->data);
        if (node->num_children > 0)
            printf(" -> ");
    } else {
        printf("(Warning: No print function)");
    }

    // Print the children of the node recursively
    for (int i = 0; i < node->num_children; i++) {
        _print_list_helper(node->next[i]);
    }
}

// Helper function to print out the indentation for the tree print function
void _indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

// Helper function to print a tree of node_t nodes
void _print_tree_helper(void *data, int depth) {
    if (data == NULL) {
        return;
    }

    // Cast the data to a node_t node so we can access general attributes
    node_t *node = (node_t *)data;

    _indent(depth);

    if (node->print != NULL) {
        node->print(node->data);
        printf("\n");
    } else {
        printf("Warning: No print function provided for tree node\n");
    }

    _indent(depth);

    printf("-> %ld children\n", node->num_children);

    for (int i = 0; i < node->num_children; i++) {
        _print_tree_helper(node->next[i], depth + 1);
    }
}

/* ----------------------------- List Functions ----------------------------- */

// Function to create an empty list_t list, this will work for any type of
// list_t. This function will allocate memory for the list and set the head and
// tail to NULL.
#define create_list(T)                                                         \
    ({                                                                         \
        T *list = (T *)malloc(sizeof(typeof(T)));                              \
        if (list == NULL) {                                                    \
            printf("Error: create_list(%s) -> Could not allocate memory for "  \
                   "list\n",                                                   \
                   #T);                                                        \
            list = NULL;                                                       \
        } else {                                                               \
            list->head = NULL;                                                 \
            list->tail = NULL;                                                 \
            list->length = 0;                                                  \
        }                                                                      \
        list;                                                                  \
    })

// Function to check if a list_t list is empty
#define list_is_empty(list)                                                    \
    ({                                                                         \
        int is_empty = 0;                                                      \
        if (list != NULL) {                                                    \
            is_empty = (list->length == 0);                                    \
        } else {                                                               \
            printf("Error: list_is_empty(%s) -> List is NULL\n", #list);       \
        }                                                                      \
        is_empty;                                                              \
    })

// Function to get the data of a node at a specific index in a list_t list.
// This function will check if the list is empty and return NULL if it is.
// Otherwise, it will return the data of the node at the target index.
#define list_get(list, index)                                                  \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_get(%s, %s) -> List is NULL\n", #list,         \
                   #index);                                                    \
            NULL;                                                              \
        } else if (list->length == 0) {                                        \
            printf("Error: list_get(%s, %s) -> List is empty\n", #list,        \
                   #index);                                                    \
            NULL;                                                              \
        }                                                                      \
        typeof(list->head) node = list->head;                                  \
        int i = 0;                                                             \
        while (node->next != NULL && i < index) {                              \
            node = (typeof(node))node->next[0];                                \
            i++;                                                               \
        }                                                                      \
        node;                                                                  \
    })

// Function to add a node to the end of a list_t.
#define list_add_node(list, node)                                              \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_add_node(%s, %s) -> List is NULL\n", #list,    \
                   #node);                                                     \
        } else {                                                               \
            if (list_is_empty(list)) {                                         \
                list->head = node;                                             \
                list->tail = node;                                             \
            } else {                                                           \
                add_child(list->tail, node);                                   \
                list->tail = node;                                             \
            }                                                                  \
            list->length++;                                                    \
        }                                                                      \
    })

// This function will take a list, a type, and a data pointer. It will check if
// the list or data is NULL and print an error message if they are. Otherwise,
// create a new node and add it to the end of the list.
//
// If the list is empty, the head and tail of the list will be set to the new
// node.
//
// Calls: add_child
#define list_add(list, T, data)                                                \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_add(%s, %s, %s) -> List is NULL\n", #list, #T, \
                   #data);                                                     \
            NULL;                                                              \
        }                                                                      \
        T##_node *new_node =                                                   \
            create_list_node(T, data, free_##T, print_##T, compare_##T);       \
        list_add_node(list, new_node);                                         \
        new_node;                                                              \
    })

// Function to remove a node from a list_t list.
// This function will check if the list is empty and return NULL if it is. If
// the list is not empty, it will remove some target node.
//
// Calls: remove_node
#define list_remove(list, target)                                              \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_remove(%s, %s) -> List is NULL\n", #list,      \
                   #target);                                                   \
            NULL;                                                              \
        } else if (target == NULL) {                                           \
            printf("Error: list_remove(%s, %s) -> Target is NULL\n", #list,    \
                   #target);                                                   \
            NULL;                                                              \
        } else {                                                               \
            if (list->length == 0) {                                           \
                printf("Error: list_remove(%s, %s) -> List is empty\n", #list, \
                       #target);                                               \
                NULL;                                                          \
            } else {                                                           \
                if (list->head->compare(list->head->data, target->data)) {     \
                    node_t *temp = (node_t *)list->head;                       \
                    list->head = list->head->next[0];                          \
                    remove_node(temp, target);                                 \
                    list->length--;                                            \
                } else {                                                       \
                    if (list->tail->compare(list->tail->data, target->data)) { \
                        node_t *temp = (node_t *)list->tail;                   \
                        list->tail = list_get(list, list->length - 2);         \
                        remove_node(temp, target);                             \
                    }                                                          \
                    remove_node(list->head, target);                           \
                    list->length--;                                            \
                }                                                              \
            }                                                                  \
        }                                                                      \
    })

// Function to remove a node at a specific index from a list_t list.
// This function will check if the list is empty and return NULL if it is. If
// the list is not empty, it will remove the node at the target index.
// The index should be between 0 and the size of the list.
//
// Calls: remove_node_at
#define list_remove_at(list, index)                                            \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_remove_at(%s, %s) -> List is NULL\n", #list,   \
                   #index);                                                    \
        } else {                                                               \
            if (list->length == 0) {                                           \
                printf("Error: list_remove_at(%s, %s) -> List is empty\n",     \
                       #list, #index);                                         \
            } else {                                                           \
                int success = 0;                                               \
                if (index == list->length - 1) {                               \
                    node_t *temp = (node_t *)list->tail;                       \
                    list->tail = list_get(list, list->length - 2);             \
                    remove_node(list->head, temp);                             \
                } else {                                                       \
                    success = remove_node_at(list->head, index);               \
                }                                                              \
                if (success) {                                                 \
                    list->length--;                                            \
                } else {                                                       \
                    printf("Error: list_remove_at(%s, %s) -> Could not "       \
                           "remove node "                                      \
                           "at index %d\n",                                    \
                           #list, #index, index);                              \
                }                                                              \
            }                                                                  \
        }                                                                      \
    })

// Function to map a function to all nodes in a list_t list.
// Evaluates the function `func` on each node in the list.
#define list_map(list, func)                                                   \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: list_map(%s, %s) -> List is NULL\n", #list, #func); \
        } else {                                                               \
            typeof(list->head) node = list->head;                              \
            while (node != NULL) {                                             \
                func(node);                                                    \
                if (node->next != NULL) {                                      \
                    node = (typeof(node))node->next[0];                        \
                } else {                                                       \
                    break;                                                     \
                }                                                              \
            }                                                                  \
        }                                                                      \
    })

// Function to peek at the last node in a list_t list.
// This function will check if the list is empty and return NULL if it is.
// Otherwise, it will return the tail of the list.
// Essentially, this function is used to turn the linked list into a stack.
#define list_peek(list) ((list)->tail)

// Removes the last added node from the list
// Essentially used to turn the linked list into a stack
//
// Calls: list_remove_at
#define list_pop(list) list_remove_at(list, list->length - 1)

// Function to free a list_t list and all of its nodes.
// This function will check if the list is NULL and print an error message if it
// is. Otherwise, it will free the head of the list and all of its children
// using the free_node function.
//
// Calls: free_node
#define free_list(list)                                                        \
    ({                                                                         \
        if (list == NULL) {                                                    \
            printf("Error: free_list(%s) -> list is NULL\n", #list);           \
        } else {                                                               \
            free_node(list->head); /* free the head and all kids */            \
            free(list);                                                        \
        }                                                                      \
    })

// Function to print a list_t list and all of its nodes.
// Calls: The print_node_rec function to print the list
#define print_list(list)                                                       \
    ({                                                                         \
        printf("List: %s\n", #list);                                           \
        print_node_rec(list->head);                                            \
    })

/* ---------------------------- Stack Functions ---------------------------- */
/* The stack is implemented as a linked list. */

// Function to create an empty stack_t stack for a given declared type.
// Calls: create_list
#define create_stack(T) create_list(T)

// Returns true if the stack is empty, false otherwise
// Calls: list_is_empty
#define stack_is_empty(stack) list_is_empty(stack)

// Function to peek at the last node in a list_t list.
// Calls: list_peek
#define stack_peek(stack) list_peek(stack)

// Removes the last added node from the stack
// Make sure to peek at the node before popping it since doesn't return a
// pointer (we just removed it from memory)
// Calls: list_pop
#define stack_pop(list) list_pop(list)

// Function to add a node to the end of a stack_t stack.
// Calls: list_add
#define stack_push(stack, T, data) list_add(stack, T, data)

/* ---------------------------- Tree Functions ----------------------------- */

/* -------------------------- Int Node Functions ------------------------- */

// Create an int pointer
int *create_int(int i) {
    int *i_ptr = (int *)malloc(sizeof(int));
    *i_ptr = i;
    return i_ptr;
}

// Compare int pointer data
int compare_int(int *a, int *b) { return (*a == *b); }

// Print int pointer data
void print_int(int *ptr) { printf("%d", *ptr); }

// Free int pointer data
void free_int(int *ptr) { free(ptr); }

// Never called by a macro, I already think 24 bytes per node for 3 function
// pointers is enough
const char *int_to_str(int *i) {
    char *str = (char *)malloc(12);
    sprintf(str, "%d", *i);
    return str;
}

/* -------------------------- Float Node Functions ------------------------- */
