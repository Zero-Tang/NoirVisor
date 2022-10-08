# Cache List Algorithm
The `Cache List` is a simple data structure algorithm that sorts the data by time of reference into an acyclic doubly-linked list. \
NoirVisor adopts this algorithm to automatically allocate and free nodes used for nested virtualizations. \
`Cache List` achieves O(1) running time complexity in both insertion and reference.

## Top-Level Idea
The execution context of the host is extremely critical, meaning that one cannot invoke synchronizing APIs in this context. In this regard, no dynamic memory allocations are permitted. \
Therefore, NoirVisor allocates a pool of nodes for static allocation purpose. \
Note that the pseudo-code provided are mere demonstrations.

```C
typedef struct cache_list
{
    struct cache_list *next;
    struct cache_list *prev;
    u64 data;
};

cache_list *list_pool;
cache_list *head,*tail;
```

There are only three runtime operations in cache list: `insert`, `remove` and `reference`. Although `search` can be implemented by traversing the doubly-linked list, it lacks efficiency: the O(n) running time is not ideal. It is recommended to use other techniques to implement the `search` operation (e.g.: using balanced binary-search trees, buckets, skiplists, etc.).

### Initialization
```C
bool init(size_t number)
{
    // It is meaningless if there are too few nodes in the list.
    if(number<2)return false;
    list_pool=malloc(sizeof(cache_list)*number);
    if(list_pool)
    {
        // Initialize the pointers of head and tail.
        head=list_pool;
        tail=&list_pool[number-1];
        // Initialize list head.
        head->next=&list_pool[1];
        head->prev=null;
        // Initialize nodes in the middle.
        for(size_t i=1;i<number-1;i++)
        {
            list_pool[i].next=&list_pool[i+1];
            list_pool[i].prev=&list_pool[i-1];
            list_pool[i].time=0;
        }
        // Initialize list tail.
        tail->prev=&list_pool[number-2];
        tail->next=null;
        // Initialization complete.
        return true;
    }
    return false;
}
```

Disposing the cache list requires only one call to the `free` function.

### Insertion
The insertion will pick the oldest node in the pool. Replace the data content and then make it the newest. \
In order to make it the newest, link it before the head.
```C
void insert(u64 data)
{
    // Pick the oldest node.
    cache_list *node=tail,*tailx=tail->prev;
    // Set the node up.
    node->data=data;
    node->prev=null;
    // Remove it from the tail.
    tail->next=head;
    tail->prev=null;
    // Reset the tail.
    tailx->next=null;
    tail=tailx;
    // Reset the head.
    head->prev=node;
    head=node;
}
```

### Reference
Referencing a node would cause it being placed at the head. The `reference` operation is the counterpart of `remove` operation.
```C
void reference(cache_list* node)
{
    // Remove the node from the middle.
    if(node->next)node->next->prev=node->prev;
    if(node->prev)node->prev->next=node->next;
    // Place the node at the head.
    node->prev=null;
    node->next=head;
    // Reset the head.
    head->prev=node;
    head=node;
}
```

### Removal
Removing a node would cause it being placed at the tail. The `remove` operation is the counterpart of `reference` operation.
```C
void remove(cache_list* node)
{
    // Remove the node from the middle.
    if(node->next)node->next->prev=node->prev;
    if(node->prev)node->prev->next=node->next;
    // Place the node at the tail.
    node->next=null;
    node->prev=tail;
    // Reset the head.
    tail->next=node;
    tail=node;
}
```

## Combine with other Data Structures
`Cache List` does not implement `search` operation by virtue of the inefficiency inherited from doubly-linked list. Instead, it is perfect at keeping track of the ages of each node. \
For example, `Cache List` can combine with AVL binary search tree.

```C
typedef struct _avl_with_cache_list
{
    // AVL tree structure
    struct _avl_with_cache_list *bst_parent;
    struct _avl_with_cache_list *bst_left;
    struct _avl_with_cache_list *bst_right;
    size_t height;
    // Cache List structure
    struct _avl_with_cache_list *cl_next;
    struct _avl_with_cache_list *cl_prev;
    // Data Content
    u64 data;
}avl_with_cache_list,*avl_with_cache_list_p;

// Allocation goes here.
avl_with_cache_list *node_pool;
avl_with_cache_list *root;
avl_with_cache_list *head,*tail;
```

## AVL With Cache List
NoirVisor adopts the AVL tree combined with Cache List. From NoirVisor's perspective, only the `insert` and `search` operations are needed.

### Insertion
The `insert` operation picks the tail node from the Cache List, then removes this node from both the Cache List and the AVL tree. This node is the new node to be inserted. \
Place this node at the head of the Cache List, then perform regular `insert` operation of AVL tree. \
The `insert` operation has logarithmic running time complexity.

```C
void insert(u64 data)
{
    // Allocate a new node from cache list.
    avl_with_cache_list_p node;
    // Allocation causes this node being invalid, remove from the AVL tree.
    remove_from_avl_tree(node);
    insert_to_cache_list(data);
    // The node at the head is the allocated node.
    node=head;
    // Insert to the AVL tree.
    node->data=data;
    insert_to_avl_tree(node);
}
```

### Search
The `search` operation would search the node with regular AVL searching algorithm. This step takes logarithmic running time. \
If the item is found, reference this node in Cache List (i.e.: relocate this node at the head of Cache List). This step takes constant running time. \
The `search` operation has logarithmic running time complexity.

```C
avl_with_cache_list_p search(u64 data)
{
    avl_with_cache_list_p node=root;
    while(node)
    {
        if(node->data>data)
            node=node->bst_right;
        else if(node->data<data)
            node=node->bst_left;
        else
        {
            // Put the node to the list head.
            reference_cache_list_node(node);
            return node;
        }
    }
    return null;
}
```