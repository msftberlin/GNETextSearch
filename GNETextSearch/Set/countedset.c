//
//  countedset.c
//  GNETextSearch
//
//  Created by Anthony Drendel on 11/14/15.
//  Copyright © 2015 Gone East LLC. All rights reserved.
//

#include "countedset.h"
#include "GNETextSearchPrivate.h"
#include <string.h>

// ------------------------------------------------------------------------------------------

#define LEFT_HEAVY 1
#define BALANCED 0
#define RIGHT_HEAVY -1

typedef struct _tsearch_countedset_node
{
    GNEInteger integer;
    size_t count;
    int balance;
    size_t left;
    size_t right;
} _tsearch_countedset_node;


typedef struct tsearch_countedset
{
    _tsearch_countedset_node *nodes;
    size_t count; // The number of nodes whose count > 0.
    size_t nodesCapacity;
    size_t insertIndex;
} tsearch_countedset;

// ------------------------------------------------------------------------------------------

_tsearch_countedset_node * _tsearch_countedset_copy_nodes(const tsearch_countedset_ptr ptr);
result _tsearch_countedset_copy_ints(const tsearch_countedset_ptr ptr, GNEInteger *integers,
                                  const size_t integersCount);
int _tsearch_countedset_compare(const void *valuePtr1, const void *valuePtr2);
result _tsearch_countedset_add_int(const tsearch_countedset_ptr ptr,
                                   const GNEInteger newInteger, const size_t countToAdd);
_tsearch_countedset_node * _tsearch_countedset_get_node_for_int(const tsearch_countedset_ptr ptr,
                                                                const GNEInteger integer);
size_t _tsearch_countedset_get_node_idx_for_int_insert(const tsearch_countedset_ptr ptr, const GNEInteger integer);
size_t _tsearch_countedset_get_node_and_parent_idx_for_int_insert(const tsearch_countedset_ptr ptr,
                                                                  const GNEInteger integer,
                                                                  size_t *outParentIndex);
int _tsearch_countedset_balance_node_at_idx(_tsearch_countedset_node *nodes, const size_t index);
void _tsearch_countedset_rotate_left(_tsearch_countedset_node *nodes, const size_t index);
void _tsearch_countedset_rotate_right(_tsearch_countedset_node *nodes, const size_t index);
result _tsearch_countedset_node_init(const tsearch_countedset_ptr ptr, const GNEInteger integer,
                                     const size_t count, size_t *outIndex);
result _tsearch_countedset_increase_values_buf(const tsearch_countedset_ptr ptr);

// ------------------------------------------------------------------------------------------
#pragma mark - Counted Set
// ------------------------------------------------------------------------------------------
tsearch_countedset_ptr tsearch_countedset_init(void)
{
    tsearch_countedset_ptr ptr = calloc(1, sizeof(tsearch_countedset));
    if (ptr == NULL) { return NULL; }

    size_t count = 5;
    size_t size = sizeof(_tsearch_countedset_node);
    _tsearch_countedset_node *nodes = calloc(count, size);
    if (nodes == NULL) { tsearch_countedset_free(ptr); return NULL; }

    ptr->nodes = nodes;
    ptr->count = 0;
    ptr->nodesCapacity = (count * size);
    ptr->insertIndex = 0;
    return ptr;
}


tsearch_countedset_ptr tsearch_countedset_copy(const tsearch_countedset_ptr ptr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return NULL; }

    tsearch_countedset_ptr copyPtr = calloc(1, sizeof(tsearch_countedset));
    if (copyPtr == NULL) { return NULL; }

    _tsearch_countedset_node *nodes = malloc(ptr->nodesCapacity);
    if (nodes == NULL) { tsearch_countedset_free(copyPtr); return NULL; }

    memcpy(nodes, ptr->nodes, ptr->nodesCapacity);

    copyPtr->nodes = nodes;
    copyPtr->count = ptr->count;
    copyPtr->nodesCapacity = ptr->nodesCapacity;
    copyPtr->insertIndex = ptr->insertIndex;
    return copyPtr;
}


void tsearch_countedset_free(const tsearch_countedset_ptr ptr)
{
    if (ptr != NULL) {
        free(ptr->nodes);
        ptr->nodes = NULL;
        ptr->count = 0;
        ptr->nodesCapacity = 0;
        ptr->insertIndex = 0;
        free(ptr);
    }
}


size_t tsearch_countedset_get_count(const tsearch_countedset_ptr ptr)
{
    return (ptr == NULL) ? 0 : ptr->count;
}


bool tsearch_countedset_contains_int(const tsearch_countedset_ptr ptr, const GNEInteger integer)
{
    _tsearch_countedset_node *nodePtr = _tsearch_countedset_get_node_for_int(ptr, integer);
    return (nodePtr == NULL || nodePtr->count == 0) ? false : true;
}


size_t tsearch_countedset_get_count_for_int(const tsearch_countedset_ptr ptr, const GNEInteger integer)
{
    if (ptr == NULL || ptr->nodes == NULL) { return 0; }
    _tsearch_countedset_node *nodePtr = _tsearch_countedset_get_node_for_int(ptr, integer);
    return (nodePtr == NULL) ? 0 : nodePtr->count;
}


result tsearch_countedset_copy_ints(const tsearch_countedset_ptr ptr, GNEInteger **outIntegers, size_t *outCount)
{
    if (ptr == NULL || ptr->nodes == NULL || outIntegers == NULL || outCount == NULL) { return failure; }
    size_t integersCount = ptr->count;
    size_t size = sizeof(GNEInteger);
    GNEInteger *integers = calloc(integersCount, size);
    if (_tsearch_countedset_copy_ints(ptr, integers, integersCount) == failure) {
        free(integers);
        *outCount = 0;
        return failure;
    }
    *outIntegers = integers;
    *outCount = integersCount;
    return success;
}


result tsearch_countedset_add_int(const tsearch_countedset_ptr ptr, const GNEInteger integer)
{
    return _tsearch_countedset_add_int(ptr, integer, 1);
}


result tsearch_countedset_remove_int(const tsearch_countedset_ptr ptr, const GNEInteger integer)
{
    if (ptr == NULL) { return failure; }
    _tsearch_countedset_node *nodePtr = _tsearch_countedset_get_node_for_int(ptr, integer);
    if (nodePtr == NULL || nodePtr->count == 0) { return success; }
    nodePtr->count = 0;
    ptr->count -= 1;
    return success;
}


result tsearch_countedset_remove_all_ints(const tsearch_countedset_ptr ptr)
{
    if (ptr == NULL) { return failure; }
    size_t count = ptr->insertIndex;
    for (size_t i = 0; i < count; i++) {
        ptr->nodes[i].count = 0;
    }
    ptr->count = 0;
    return success;
}


result tsearch_countedset_union(const tsearch_countedset_ptr ptr, const tsearch_countedset_ptr otherPtr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    if (otherPtr == NULL || otherPtr->nodes == NULL) { return success; }

    size_t otherCount = otherPtr->insertIndex;
    _tsearch_countedset_node *otherNodes = otherPtr->nodes;
    for (size_t i = 0; i < otherCount; i++) {
        if (otherNodes[i].count == 0) { continue; }
        _tsearch_countedset_node otherValue = otherNodes[i];
        int result = _tsearch_countedset_add_int(ptr, otherValue.integer, otherValue.count);
        if (result == failure) { return failure; }
    }
    return success;
}


result tsearch_countedset_intersect(const tsearch_countedset_ptr ptr, const tsearch_countedset_ptr otherPtr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    if (otherPtr == NULL || otherPtr->nodes == NULL) {
        return tsearch_countedset_remove_all_ints(ptr);
    }

    size_t actualCount = ptr->insertIndex;

    // Copy all of the counted set's values so that we can iterate over them
    // while modifying the set.
    _tsearch_countedset_node *nodesCopy = _tsearch_countedset_copy_nodes(ptr);
    if (nodesCopy == NULL) { return failure; }

    for (size_t i = 0; i < actualCount; i++) {
        _tsearch_countedset_node node = nodesCopy[i];
        if (node.count == 0) { continue; }
        _tsearch_countedset_node *nodePtr = _tsearch_countedset_get_node_for_int(otherPtr, node.integer);
        if (nodePtr == NULL || nodePtr->count == 0) {
            int result = tsearch_countedset_remove_int(ptr, node.integer);
            if (result == failure) { free(nodesCopy); return failure; }
        } else {
            size_t count = tsearch_countedset_get_count_for_int(otherPtr, node.integer);
            int result = _tsearch_countedset_add_int(ptr, node.integer, count);
            if (result == failure) { free(nodesCopy); return failure; }
        }
    }
    free(nodesCopy);
    return success;
}


result tsearch_countedset_minus(const tsearch_countedset_ptr ptr, const tsearch_countedset_ptr otherPtr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    if (otherPtr == NULL || otherPtr->nodes == NULL) { return success; }

    size_t otherUsedCount = otherPtr->insertIndex;
    _tsearch_countedset_node *otherNodes = otherPtr->nodes;
    for (size_t i = 0; i < otherUsedCount; i++) {
        _tsearch_countedset_node otherValue = otherNodes[i];
        _tsearch_countedset_node *nodePtr = _tsearch_countedset_get_node_for_int(ptr, otherValue.integer);
        if (nodePtr == NULL) { continue; }
        if (otherValue.count >= nodePtr->count) {
            int result = tsearch_countedset_remove_int(ptr, otherValue.integer);
            if (result == failure) { return failure; }
        } else {
            nodePtr->count -= otherValue.count;
        }
    }
    return success;
}


// ------------------------------------------------------------------------------------------
#pragma mark - Private
// ------------------------------------------------------------------------------------------
_tsearch_countedset_node * _tsearch_countedset_copy_nodes(const tsearch_countedset_ptr ptr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return NULL; }
    size_t actualCount = ptr->insertIndex;
    size_t size = sizeof(_tsearch_countedset_node);
    _tsearch_countedset_node *nodesCopy = calloc(actualCount, size);
    if (nodesCopy == NULL) { return failure; }
    memcpy(nodesCopy, ptr->nodes, actualCount * size);
    return nodesCopy;
}


result _tsearch_countedset_copy_ints(const tsearch_countedset_ptr ptr, GNEInteger *integers,
                                     const size_t integersCount)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    if (integers == NULL || integersCount == 0) { return failure; }

    size_t nodesCount = ptr->insertIndex;
    size_t size = sizeof(_tsearch_countedset_node);
    if (integersCount > nodesCount) { return failure; }

    _tsearch_countedset_node *nodesCopy = _tsearch_countedset_copy_nodes(ptr);
    if (nodesCopy == NULL) { return failure; }

    qsort(nodesCopy, nodesCount, size, &_tsearch_countedset_compare);

    // The nodes are sorted in descending order. So, all of the nodes with zero counts
    // are at the end of the array. The integers count only includes nodes with
    // non-zero counts.
    for (size_t i = 0; i < integersCount; i++) {
        _tsearch_countedset_node value = nodesCopy[i];
        integers[i] = value.integer;
    }
    free(nodesCopy);
    return success;
}


int _tsearch_countedset_compare(const void *valuePtr1, const void *valuePtr2)
{
    if (valuePtr1 == NULL || valuePtr2 == NULL) { return 0; }
    _tsearch_countedset_node value1 = *(_tsearch_countedset_node *)valuePtr1;
    _tsearch_countedset_node value2 = *(_tsearch_countedset_node *)valuePtr2;

    if (value1.count > value2.count) { return -1; }
    if (value1.count < value2.count) { return 1; }
    return 0;
}


result _tsearch_countedset_add_int(const tsearch_countedset_ptr ptr,
                                   const GNEInteger newInteger,
                                   const size_t countToAdd)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    if (ptr->insertIndex == 0) {
        size_t index = SIZE_MAX;
        int result = _tsearch_countedset_node_init(ptr, newInteger, countToAdd, &index);
        if (result == failure || index == SIZE_MAX) { return failure; }
        return success;
    }

    size_t parentIndex = SIZE_MAX;
    size_t insertIndex = _tsearch_countedset_get_node_and_parent_idx_for_int_insert(ptr,
                                                                                    newInteger,
                                                                                    &parentIndex);
    if (insertIndex == SIZE_MAX) { return failure; }

    _tsearch_countedset_node *nodePtr = &(ptr->nodes[insertIndex]);
    GNEInteger nodeInteger = nodePtr->integer;

    if (nodeInteger == newInteger) {
        size_t newCount = ((SIZE_MAX - nodePtr->count) >= countToAdd) ? (nodePtr->count + countToAdd) : SIZE_MAX;
        nodePtr->count = newCount;
        if (newCount == 1) {
            ptr->count += 1;
        }
        return success;
    }

    size_t index = SIZE_MAX;
    int result = _tsearch_countedset_node_init(ptr, newInteger, countToAdd, &index);
    if (result == failure || index == SIZE_MAX) { return failure; }
    nodePtr = &(ptr->nodes[insertIndex]); // If ptr->nodes was realloced, we need to refresh the pointer.

    if (newInteger < nodeInteger) { nodePtr->left = index; }
    else { nodePtr->right = index; }

    _tsearch_countedset_balance_node_at_idx(ptr->nodes, parentIndex);

    return success;
}


/// Returns the exact node containing the specified integer or NULL if the integer isn't
/// present in the counted set.
_tsearch_countedset_node * _tsearch_countedset_get_node_for_int(const tsearch_countedset_ptr ptr,
                                                                const GNEInteger integer)
{
    if (ptr == NULL || ptr->nodes == NULL || ptr->count == 0) { return NULL; }
    size_t index = _tsearch_countedset_get_node_idx_for_int_insert(ptr, integer);
    if (index == SIZE_MAX) { return NULL; }
    _tsearch_countedset_node *insertionNodePtr = &(ptr->nodes[index]);
    return (insertionNodePtr->integer == integer) ? insertionNodePtr : NULL;
}


/// Returns the index for the node representing the specified integer or the parent node into which
/// a new node should be inserted. Returns SIZE_MAX on failure.
size_t _tsearch_countedset_get_node_idx_for_int_insert(const tsearch_countedset_ptr ptr, const GNEInteger integer)
{
    return _tsearch_countedset_get_node_and_parent_idx_for_int_insert(ptr, integer, NULL);
}


size_t _tsearch_countedset_get_node_and_parent_idx_for_int_insert(const tsearch_countedset_ptr ptr,
                                                                  const GNEInteger integer,
                                                                  size_t *outParentIndex)
{
    if (ptr == NULL || ptr->nodes == NULL || ptr->insertIndex == 0) { return SIZE_MAX; }

    _tsearch_countedset_node *nodes = ptr->nodes;
    size_t parentIndex = SIZE_MAX;
    size_t nextIndex = 0; // Start at root
    do {
        if (outParentIndex != NULL) { *outParentIndex = parentIndex; }
        parentIndex = nextIndex;
        if (integer < nodes[parentIndex].integer) { nextIndex = nodes[parentIndex].left; }
        else if (integer > nodes[parentIndex].integer) { nextIndex = nodes[parentIndex].right; }
        else { return parentIndex; }
    } while (nextIndex != SIZE_MAX);

    return parentIndex;
}


int _tsearch_countedset_balance_node_at_idx(_tsearch_countedset_node *nodes, const size_t index)
{
    if (nodes == NULL || index == SIZE_MAX) { return 0; }
    int leftHeight = _tsearch_countedset_balance_node_at_idx(nodes, nodes[index].left);
    int rightHeight = _tsearch_countedset_balance_node_at_idx(nodes, nodes[index].right);
    int height = leftHeight - rightHeight;
    if (abs(leftHeight - rightHeight) > 1) {
        if (height < 0) {
            _tsearch_countedset_rotate_right(nodes, index);
        } else {
            _tsearch_countedset_rotate_left(nodes, index);
        }
        height = BALANCED;
    }
    nodes[index].balance = height;
    return abs(height) + 1;
}


void _tsearch_countedset_rotate_left(_tsearch_countedset_node *nodes, const size_t index)
{
    _tsearch_countedset_node node = nodes[index];
    size_t childIndex = node.left;
    _tsearch_countedset_node childNode = nodes[childIndex];
    size_t grandchildIndex = (childNode.balance > 0) ? childNode.left : childNode.right;
    _tsearch_countedset_node grandchildNode = nodes[grandchildIndex];
    if (childNode.balance > 0) {
        //     8         7
        //   7    ==>  2   8
        // 2
        nodes[index] = childNode;
        nodes[index].left = grandchildIndex;
        nodes[index].right = childIndex;
        nodes[index].balance = BALANCED;

        nodes[childIndex] = node;
        nodes[childIndex].left = SIZE_MAX;
        nodes[childIndex].balance = BALANCED;
    } else {
        //   8         7
        // 2    ==>  2   8
        //   7
        nodes[index] = grandchildNode;
        nodes[index].left = childIndex;
        nodes[index].right = grandchildIndex;
        nodes[index].balance = BALANCED;

        nodes[grandchildIndex] = node;
        nodes[grandchildIndex].left = SIZE_MAX;
        nodes[grandchildIndex].balance = BALANCED;

        nodes[childIndex].right = SIZE_MAX;
        nodes[childIndex].balance = BALANCED;
    }
}


void _tsearch_countedset_rotate_right(_tsearch_countedset_node *nodes, const size_t index)
{
    _tsearch_countedset_node node = nodes[index];
    size_t childIndex = node.right;
    _tsearch_countedset_node childNode = nodes[childIndex];
    size_t grandchildIndex = (childNode.balance > 0) ? childNode.left : childNode.right;
    _tsearch_countedset_node grandchildNode = nodes[grandchildIndex];
    if (childNode.balance > 0) {
        // 2           7
        //   8  ==>  2   8
        // 7
        nodes[index] = grandchildNode;
        nodes[index].left = grandchildIndex;
        nodes[index].right = childIndex;
        nodes[index].balance = BALANCED;

        nodes[grandchildIndex] = node;
        nodes[grandchildIndex].left = SIZE_MAX;
        nodes[grandchildIndex].right = SIZE_MAX;
        nodes[grandchildIndex].balance = BALANCED;

        nodes[childIndex].left = SIZE_MAX;
        nodes[childIndex].balance = BALANCED;
    } else {
        // 2             7
        //   7    ==>  2   8
        //     8
        nodes[index] = childNode;
        nodes[index].left = childIndex;
        nodes[index].right = grandchildIndex;
        nodes[index].balance = BALANCED;

        nodes[childIndex] = node;
        nodes[childIndex].left = SIZE_MAX;
        nodes[childIndex].right = SIZE_MAX;
        nodes[childIndex].balance = BALANCED;
    }
}


/// Returns a pointer to a new counted set node and increments the GNEIntegerCountedSet's count.
result _tsearch_countedset_node_init(const tsearch_countedset_ptr ptr, const GNEInteger integer,
                                     const size_t count, size_t *outIndex)
{
    if (outIndex == NULL) { return failure; }
    if (ptr == NULL || ptr->nodes == NULL) { *outIndex = SIZE_MAX; return failure; }
    if (ptr->insertIndex == SIZE_MAX - 1) { *outIndex = SIZE_MAX; return failure; }
    if (_tsearch_countedset_increase_values_buf(ptr) == failure) {
        *outIndex = SIZE_MAX;
        return failure;
    }
    size_t index = ptr->insertIndex;
    ptr->insertIndex += 1;
    ptr->count += 1;
    ptr->nodes[index].integer = integer;
    ptr->nodes[index].count = count;
    ptr->nodes[index].balance = BALANCED;
    ptr->nodes[index].left = SIZE_MAX;
    ptr->nodes[index].right = SIZE_MAX;
    *outIndex = index;
    return success;
}


result _tsearch_countedset_increase_values_buf(const tsearch_countedset_ptr ptr)
{
    if (ptr == NULL || ptr->nodes == NULL) { return failure; }
    size_t usedCount = ptr->insertIndex;
    size_t capacity = ptr->nodesCapacity;
    size_t emptySpaces = (capacity / sizeof(_tsearch_countedset_node)) - usedCount;
    if (emptySpaces <= 2) {
        size_t newCapacity = capacity * 2;
        _tsearch_countedset_node *newNodes = realloc(ptr->nodes, newCapacity);
        if (newNodes == NULL) { return failure; }
        ptr->nodes = newNodes;
        ptr->nodesCapacity = newCapacity;
    }
    return success;
}
