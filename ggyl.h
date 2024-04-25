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
 *  - Implement stack and queue data structures
 *      - Stack can extend the list node structure
 *      - Queue should be properly implemented such that all data is contiguous
 *      - Queue size should be static
 *      - Queue should have pointers to the front and back of the queue
 *
 */

/* ---------------------------- Node Structure --------------------------- */

/* I am creating a tree that can have any number of children per node. To
 * accomplish this, I am storing the children in a linked list so they can
 * easily be removed from memory once they are no longer necessary. */

// Enumeration for the node types
typedef enum { LIST, TREE } node_config;

// Function pointer types for the node_t node
typedef void (*free_func)(void *);
typedef void (*print_func)(void *);
typedef int (*compare_func)(void *, void *);

// Abstract structure for the node_t node.
// It might be better to declare the function pointers in the list_t and tree_t
// to save 24 bytes per node, but I like having them here :)
#define DEFINE_NODE_STRUCT(T)                                                  \
    typedef struct node_t {                                                    \
        T *data;                                                               \
        struct node_t **next;                                                  \
        node_config type;                                                      \
        int num_children;                                                      \
        void (*free)(T *);                                                     \
        void (*print)(T *);                                                    \
        int (*compare)(T *, T *);                                              \
    } node_t;

// Define the node_t structure for int data
DEFINE_NODE_STRUCT(int);
// DEFINE_NODE_STRUCT(float_node, float);

// List structure to store a list of node_t nodes
typedef struct list_t {
    node_t *head;
    node_t *tail;
    int size;
} list_t;

// Tree structure to store a tree of node_t nodes
typedef struct tree_t {
    node_t *root;
    int size;
} tree_t;

/* ---------------------------- Node Functions ------------------------------ */

// Generic function to initialize a node_t node with a given data and type.
// You should not use this function directly, use CREATE_NODE instead unless you
// know what you are doing
#define init_node(node, _data, node_config, _free, _print, _compare)           \
    do {                                                                       \
        node->data = _data;                                                    \
        node->type = node_config;                                              \
        node->next = NULL;                                                     \
        node->num_children = 0;                                                \
        node->free = _free;                                                    \
        node->print = _print;                                                  \
        node->compare = _compare;                                              \
    } while (0)

/**
 * Creates a node of type `T`, initializes it with `data`, `node_config`, and
 * function pointers for `free`, `print`, and `compare`.
 * @param T The data type of the node's data.
 * @param data Pointer to the data to store in the node.
 * @param node_config Configuration enum (LIST or TREE).
 * @param free Function pointer to free the node data.
 * @param print Function pointer to print the node data.
 * @param compare Function pointer to compare two node data elements.
 * @return Pointer to the newly created node or NULL if memory allocation fails.
 */
#define create_node(T, data, node_config, free, print, compare)                \
    ({                                                                         \
        node_t *node = (node_t *)malloc(sizeof(node_t));                       \
        typeof(data)(_data) = (data); /* Handle expressions */                 \
        void (*_free)(T *) = (free);                                           \
        void (*_print)(T *) = (print);                                         \
        int (*_compare)(T *, T *) = (compare);                                 \
        if (node == NULL) {                                                    \
            printf("Error: Could not allocate memory for node\n");             \
            node = NULL;                                                       \
        } else {                                                               \
            init_node(node, _data, node_config, _free, _print, _compare);      \
        }                                                                      \
        node;                                                                  \
    })

// Generic function to create a list node (1 child) with a given data and
// children
#define create_list_node(T, data, free, print, compare)                        \
    create_node(T, data, LIST, free, print, compare)

// Generic function to create a tree node (n children) with a given data and
// children
//
// Calls: CREATE_NODE
#define create_tree_node(T, data, free, print, compare)                        \
    create_node(T, data, TREE, free, print, compare)

// Helper function to add a child to a node_t node
void _add_child(node_t *parent, node_t *child) {

    if (parent == NULL || child == NULL) {
        printf("Error: Parent or child node is NULL\n");
        return;
    }

    node_config parent_type = parent->type;

    switch (parent_type) {
        // If the parent is a list node, add the child to the next pointer
        // If the next pointer is not NULL, keep traversing until the next
        // available NULL pointer
        case LIST:
            {
                if (parent->num_children != 0 || parent->next != NULL) {
                    parent = parent->next[0];
                    _add_child(parent, child);
                    return;
                }

                // Allocate memory for the child node but only allocate memory
                // for 1 child Since the node is type LIST, any functions should
                // know that it can only have 1 child
                parent->next = (node_t **)malloc(sizeof(node_t *));
                parent->next[0] = child;
                parent->num_children++;
                return;
            }

        // If the parent is a tree node, add the child to the list of children
        case TREE:
            {
                // Reallocation of the children array to add the new child
                parent->next = (node_t **)realloc(parent->next,
                                                  (parent->num_children + 1) *
                                                      sizeof(node_t *));
                if (parent->next == NULL) {
                    printf("Error: Could not allocate memory for new child\n");
                    return;
                }

                // Add the child to the parent's children array and increment
                parent->next[parent->num_children] = child;
                parent->num_children++;
                return;
            }
    }
}

// Generic function to add a child to a node_t node, this will catch if the node
// is a list node or a tree node
//
// Calls: _add_child
#define add_child(parent, child) _add_child(parent, child);

// Free a node_t node with a given data type
// Checks if the node is NULL, if the data is NULL, and if the free function is
// null If the data is not NULL, it will free the data using the free function
// if it exists Otherwise, it will use free() It will then free all of the
// children of the node recursively If you do not want to free the node's
// children from your structure, use REMOVE_NODE instead
void _free_node(node_t *node) {
    if (node == NULL) {
        return;
    }

    if (node->data != NULL) {
        if (node->free != NULL) {
            node->free(node->data);
        } else {
            printf("Warning: No free function provided for node\n");
            free(node->data);
        }
    }

    for (int i = 0; i < node->num_children; i++) {
        _free_node(node->next[i]);
    }

    free(node->next);
    free(node);
}

// Free a node_t node with a given data type
// Checks if the node is NULL, if the data is NULL, and if the free function is
// null If the data is not NULL, it will free the data using the free function
// if it exists Otherwise, it will use free() It will then free all of the
// children of the node recursively If you do not want to free the node's
// children from your structure, use REMOVE_NODE instead
//
// Calls: _free_node
#define free_node(node) _free_node(node);

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
        node_t *parent = NULL;                                                 \
        node_t *current = _root;                                               \
        _find_and_remove_node(&parent, &current, _target);                     \
    })

// Helper function to remove a node at a specific index from the parent node
void _remove_node_at(node_t *parent, int index) {
    if (parent == NULL) {
        printf("Error: Parent node is NULL\n");
        return;
    }

    if (index < 0 || index >= parent->num_children) {
        printf("Error: Index out of bounds\n");
        return;
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
}

// Function to remove a target node from the parent node's children array
#define remove_node_at(parent, index)                                          \
    ({                                                                         \
        typeof(parent) _parent = (parent);                                     \
        typeof(index) _index = (index);                                        \
        _remove_node_at(_parent, _index);                                      \
    })

// Helper function to find a node in the tree and remove it
void _find_and_remove_node(node_t **parent, node_t **current, node_t *target) {
    if (*current == NULL)
        return;

    // Iterate through the children of the current node to find the target node
    for (int i = 0; i < (*current)->num_children; i++) {
        if ((*current)->next[i] == target) {
            // Remove the target node from the current node's children
            for (int j = i; j < (*current)->num_children - 1; j++) {
                (*current)->next[j] = (*current)->next[j + 1];
            }
            _free_node(target);
            (*current)->num_children--;
            return;
        }

        // Recursively search for the target node in the current node's children
        _find_and_remove_node(&(*current), &(*current)->next[i], target);
    }
}

// Helper function to print a list of node_t nodes
void _print_list_helper(node_t *node) {
    if (node == NULL) {
        return;
    }

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
void _print_tree_helper(node_t *node, int depth) {
    if (node == NULL) {
        return;
    }

    _indent(depth);

    if (node->print != NULL) {
        node->print(node->data);
        printf("\n");
    } else {
        printf("Warning: No print function provided for tree node\n");
    }

    _indent(depth);

    printf("-> %d children\n", node->num_children);

    for (int i = 0; i < node->num_children; i++) {
        _print_tree_helper(node->next[i], depth + 1);
    }
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
            printf("Error: Node is NULL\n");                                   \
        } else {                                                               \
            if (node->print != NULL) {                                         \
                printf("Node:\n-> Data: ");                                    \
                node->print(node->data);                                       \
                printf("\n-> Type: %s\n",                                      \
                       node->type == LIST ? "List" : "Tree");                  \
                printf("-> Children: %d\n", node->num_children);               \
            } else {                                                           \
                printf("Warning: No print function provided for node\n");      \
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
            printf("Error: Node is NULL\n");                                   \
        } else {                                                               \
            switch (node->type) {                                              \
                case LIST:                                                     \
                    {                                                          \
                        printf("List: ");                                      \
                        _print_list_helper(node);                              \
                        printf("\n");                                          \
                        break;                                                 \
                    }                                                          \
                case TREE:                                                     \
                    {                                                          \
                        printf("Tree:\n");                                     \
                        _print_tree_helper(node, 0);                           \
                        break;                                                 \
                    }                                                          \
            }                                                                  \
        }                                                                      \
    })

// Helper function to map a evaluate a function using an entire node_t data
// structure
//
// (list_t)[1, 2, 3, 4] -> map([1, 2, 3, 4], print_node) -> "1 2 3 4")
void map_node(node_t *node, void (*func)(node_t *)) {
    if (node == NULL) {
        return;
    }

    func(node);

    for (int i = 0; i < node->num_children; i++) {
        map_node(node->next[i], func);
    }
}

/* ---------------------------- Watch Descriptors --------------------------
 */
// Compare watch descriptor int pointers
int compare_int(int *a, int *b) { return (*a == *b); }

void print_int(int *wd) { printf("%d", *wd); }

void free_int(int *ptr) { free(ptr); }

int *create_int(int wd) {
    int *wd_ptr = (int *)malloc(sizeof(int));
    *wd_ptr = wd;
    return wd_ptr;
}

/* ---------------------------- List Functionality -----------------------------
 */

// Function to initialize a list_t structure
list_t *_init_list() {
    list_t *list = (list_t *)malloc(sizeof(list_t));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

// Generic function to create a linked list of node_t nodes
#define create_list(T, data, free, print, compare)                             \
    ({                                                                         \
        list_t *list = _init_list();                                           \
        list->head = create_list_node(T, data, free, print, compare);          \
        list->tail = list->head;                                               \
        list->size++;                                                          \
        list;                                                                  \
    })

// Function to add a node to the list_t structure
// If the list is empty, the head and tail will be the same
// Otherwise, the tail will be updated to the new node_t
//
// Calls: add_child
void _list_t_add(list_t *list, node_t *node) {
    if (node->type != LIST) {
        printf("list_add -> Error: Node is not a list node\n");
        return;
    }
    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        add_child(list->tail, node);
        list->tail = node;
    }
    list->size++;
}

// Function to remove a node from the list_t structure
// Calls: remove_node
void _list_t_remove(list_t *list, node_t *node) {
    if (node->type != LIST) {
        printf("list_remove -> Error: Node is not a list node\n");
        return;
    }
    remove_node(list->head, node);
    list->size--;
}

// Function to remove a node at a specific index from the list_t structure
// Calls: remove_node
void _list_t_remove_at(list_t *list, int index) {
    if (index < 0 || index >= list->size) {
        printf("Error: Index out of bounds\n");
        return;
    }

    node_t *current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next[0];
    }

    remove_node(list->head, current);
    list->size--;
}

// Function to free a list_t structure, most logic is handled in free_node
void _list_t_free(list_t *list) {
    // Free head node and all children
    free_node(list->head);
    free(list);
}

// Function to map a function to each node in the list_t structure
void _list_t_map(list_t *list, void (*func)(node_t *)) {
    map_node(list->head, func);
}

/* ---------------------------- Tree Functions ----------------------------- */

#define create_tree(T, data, free, print, compare)                             \
    ({                                                                         \
        tree_t *tree = (tree_t *)malloc(sizeof(tree_t));                       \
        tree->root = create_tree_node(T, data, free, print, compare);          \
        tree->size = 1;                                                        \
        tree;                                                                  \
    })

// Function to add a node to the tree_t structure
// If the tree is empty, the root will be the new node
// Otherwise, the node will be added to the root's children
//
// Calls: add_child
void _tree_t_add(tree_t *tree, node_t *node) {
    if (node->type != TREE) {
        printf("tree_add -> Error: Node is not a tree node\n");
        return;
    }
    if (tree->root == NULL) {
        tree->root = node;
    } else {
        add_child(tree->root, node);
    }
    tree->size++;
}

// Function to remove a node from the tree_t structure
// Calls: remove_node
void _tree_t_remove(tree_t *tree, node_t *node) {
    if (node->type != TREE) {
        printf("tree_remove -> Error: Node is not a tree node\n");
        return;
    }
    remove_node(tree->root, node);
    tree->size--;
}

// Remove child from root at index
void _tree_t_remove_at(tree_t *tree, int index) {

    if (index < 0 || index >= tree->root->num_children) {
        printf("Error: Index out of bounds\n");
        return;
    }

    node_t *current = tree->root->next[index];
    remove_node(tree->root, current);
    tree->size--;
}

// Function to free a tree_t structure, all logic is handled in free_node
// Calls: free_node
void _tree_t_free(tree_t *tree) {
    free_node(tree->root);
    free(tree);
}

// Function to map a function to each node in the tree_t structure
// Calls: map_node
void _tree_t_map(tree_t *tree, void (*func)(node_t *)) {
    map_node(tree->root, func);
}

/* ---------------------------- Definitions -------------------------------- */

// If you are using your own data structure you can implement functions with the
// following macros. This will allow you to use the same functions as list_t.
#define DEFINE_STRUCTURE_FUNCTIONS(TYPE)                                       \
    void TYPE##_add(TYPE *t, node_t *node) { _##TYPE##_add(t, node); }         \
    void TYPE##_free(TYPE *t) { _##TYPE##_free(t); }                           \
    void TYPE##_remove(TYPE *t, node_t *node) { _##TYPE##_remove(t, node); }   \
    void TYPE##_remove_at(TYPE *t, int index) {                                \
        _##TYPE##_remove_at(t, index);                                         \
    }                                                                          \
    void TYPE##_map(TYPE *t, void (*func)(node_t *)) {                         \
        _##TYPE##_map(t, func);                                                \
    };
