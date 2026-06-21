#include "tree.h"
#include "futils.h"
#include <stdio.h>

int getHeight(Node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->height;
}

Node *newNode(int64_t key, int page_no, uint16_t row_no) {
  Node *node = malloc(sizeof(Node));
  node->key = key;
  node->left_node = NULL;
  node->right_node = NULL;
  node->height = 1;
  node->page_no = page_no;
  node->row_no = row_no;
  return node;
}

static Node *right_rotate(Node *node) {
  Node *left = node->left_node;
  Node *left_right = left->right_node;

  left->right_node = node;
  node->left_node = left_right;

  node->height =
      1 + max_int(getHeight(node->left_node), getHeight(node->right_node));
  left->height =
      1 + max_int(getHeight(left->left_node), getHeight(left->right_node));

  return left;
}

static Node *left_rotate(Node *node) {
  Node *right = node->right_node;
  Node *right_left = right->left_node;

  right->left_node = node;
  node->right_node = right_left;

  node->height =
      1 + max_int(getHeight(node->left_node), getHeight(node->right_node));
  right->height =
      1 + max_int(getHeight(right->left_node), getHeight(right->right_node));

  return right;
}

int getBalanceFactor(Node *node) {
  if (node == NULL) {
    return -1;
  }
  return getHeight(node->left_node) - getHeight(node->right_node);
}

Node *insert(Node *node, int key, int page_no, uint16_t row_no) {
  if (node == NULL) {
    return newNode(key, page_no, row_no);
  }

  if (key < node->key) {
    node->left_node = insert(node->left_node, key, page_no, row_no);
  } else if (key > node->key) {
    node->right_node = insert(node->right_node, key, page_no, row_no);
  } else {
    return node;
  }

  node->height =
      1 + max_int(getHeight(node->left_node), getHeight(node->right_node));
  int balance_factor = getBalanceFactor(node);

  if (balance_factor > 1) {
    if (key < node->left_node->key) {
      return right_rotate(node);
    }
    node->left_node = left_rotate(node->left_node);
    return right_rotate(node);
  }

  if (balance_factor < -1) {
    if (key > node->right_node->key) {
      return left_rotate(node);
    }
    node->right_node = right_rotate(node->right_node);
    return left_rotate(node);
  }

  return node;
}

void preOrder(Node *root) {
  if (root != NULL) {
    printf("%ld ", root->key);
    preOrder(root->left_node);
    preOrder(root->right_node);
  }
}

Node *search(Node *root, int key) {
  if (root == NULL || root->key == key) {
    return root;
  }

  if (root->key < key) {
    return search(root->right_node, key);
  }

  return search(root->left_node, key);
}

Node *minValueNode(Node *node) {
  Node *current = node;
  while (current->left_node != NULL) {
    current = current->left_node;
  }
  return current;
}

Node *deleteNode(Node *root, int key) {
  if (root == NULL) {
    return root;
  }

  if (key < root->key) {
    root->left_node = deleteNode(root->left_node, key);
  } else if (key > root->key) {
    root->right_node = deleteNode(root->right_node, key);
  } else {
    if (root->left_node == NULL || root->right_node == NULL) {
      Node *temp = root->left_node ? root->left_node : root->right_node;
      if (temp == NULL) {
        temp = root;
        root = NULL;
      } else {
        *root = *temp;
      }
      free(temp);
    } else {
      Node *successor = minValueNode(root->right_node);
      root->key = successor->key;
      root->page_no = successor->page_no;
      root->row_no = successor->row_no;
      root->right_node = deleteNode(root->right_node, successor->key);
    }
  }

  if (root == NULL) {
    return root;
  }

  root->height =
      1 + max_int(getHeight(root->left_node), getHeight(root->right_node));
  int balance_factor = getBalanceFactor(root);

  if (balance_factor > 1) {
    if (getBalanceFactor(root->left_node) >= 0) {
      return right_rotate(root);
    }
    root->left_node = left_rotate(root->left_node);
    return right_rotate(root);
  }

  if (balance_factor < -1) {
    if (getBalanceFactor(root->right_node) <= 0) {
      return left_rotate(root);
    }
    root->right_node = right_rotate(root->right_node);
    return left_rotate(root);
  }

  return root;
}
