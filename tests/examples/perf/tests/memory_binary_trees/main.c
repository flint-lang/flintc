#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    struct Node *left;
    struct Node *right;
    uint32_t value;
} Node;

Node *make_node(uint32_t value) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

Node *make_tree(uint32_t depth) {
    if (depth == 0) {
        return NULL;
    }
    Node *node = make_node(depth);
    node->left = make_tree(depth - 1);
    node->right = make_tree(depth - 1);
    return node;
}

uint32_t check_tree(Node *node) {
    if (node == NULL) {
        return 0;
    }
    return node->value + check_tree(node->left) + check_tree(node->right);
}

void free_tree(Node *node) {
    if (node == NULL) {
        return;
    }
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

int main(int argc, char *argv[]) {
    uint32_t depth = 12;
    if (argc > 1) {
        depth = atoi(argv[1]);
    }

    Node *root = make_tree(depth);
    int result = check_tree(root);
    free_tree(root);

    printf("%d\n", result);
    return 0;
}
