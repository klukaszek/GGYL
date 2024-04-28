#include "test.h"

void add_1(int *data) { *data += 1; }

int main() {
    int_list *list =
        create_list(int, free_int, compare_int, int_to_str, print_int);

    list_add(list, create_int(1));
    list_add(list, create_int(2));
    list_add(list, create_int(3));
    list_add(list, create_int(4));
    list_add(list, create_int(5));

    list_map(list, add_1);

    int target = 3;
    int index = list_find(list, &target);

    printf("Index of 3: %d\n", index);

    print_list(list);

    int_list *list2 = NULL;

    print_list(list2);

    free_list(list);
    free_list(list2);

    return 0;
}
