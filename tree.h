#ifndef TREE_H
#define TREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct Node {
    int64_t key;
    struct Node* left_node;
    struct Node* right_node;
    int page_no;
    uint16_t row_no;
    int16_t height;
} Node;



int getHeight(Node* node);
void preOrder(Node *root);
Node* newNode(int64_t key, int page_no, uint16_t row_no);
Node* insert(Node* node, int key, int page_no, uint16_t row_no);
Node* search(Node* root, int key);
Node* deleteNode(Node* root, int key);

#endif