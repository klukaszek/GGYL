#include "test.h"

void add_1(int_node *node) {
    int *i = node->data;
    *i += 1;
}

void test_nodes() {
    int_node *node =
        create_list_node(int, create_int(1), free_int, print_int, compare_int);
    int_node *node2 =
        create_list_node(int, create_int(2), free_int, print_int, compare_int);

    int_node *node3 =
        create_list_node(int, create_int(3), free_int, print_int, compare_int);

    add_child(node, node2);
    add_child(node, node3);

    print_node_rec(node);

    remove_node_at(node, 1);

    print_node_rec(node);

    remove_node(node, node2);

    print_node_rec(node);

    print_node(node3);

    free_node(node);
    free_node(node3);
}

void test_lists() {

    int_list *list = create_list(int_list);

    list_add(list, int, create_int(0));
    list_add(list, int, create_int(1));
    list_add(list, int, create_int(2));
    int_node *node = list_add(list, int, create_int(3));
    list_add(list, int, create_int(4));

    print_list(list);

    int_node *node2 = list_get(list, 3);

    // list_remove(list, node);

    list_remove_at(list, 4);

    print_list(list);

    list_t *list2 = NULL;

    list_is_empty(list2);

    list_map(list, add_1);

    print_list(list);

    int *data = get_node_data(list->head);

    // print_node(list->tail);

    printf("%d\n", *data);

    free_list(list);
}

int main() {
    test_lists();
    return 0;
}
//
// void test_tree() {
//     tree_t *tree = create_tree(int, create_int(1), free_int, print_int,
//                                compare_int);
//
//     tree_t_add(tree, create_tree_node(int, create_int(2), free_int,
//     print_int,
//                                        compare_int));
//
//     tree_t_add(tree, create_tree_node(int, create_int(3), free_int,
//     print_int,
//                                        compare_int));
//
//     print_node_rec(tree->root);
//
//     tree_t_map(tree, add_1);
//
//     print_node_rec(tree->root);
//
//     tree_t_remove(tree, tree->root->next[0]);
//
//     print_node_rec(tree->root);
//
//     tree_t_remove_at(tree, 0);
//
//     print_node(tree->root);
//
//     tree_t_free(tree);
// }
