# Decision Tree

Decision Tree format:
config                                  - 20 bytes
num_attr                                - 4 bytes
num_attr_names                          - 4 bytes
read in num_attr names:
    M, number of chars in name          - 4 bytes
    read in M chars:
        char                            - 1 byte
N, the number of nodes in the tree      - 4 bytes
read in N nodes, preorder traversal:
    base                                - 4 bytes
    attr_idx                            - 4 bytes
    discrete                            - 4 bytes
    label                               - 4 bytes
read in N nums, inorder traversal:
    node_idx                            - 4 bytes


