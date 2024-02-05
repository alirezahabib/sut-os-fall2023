/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"

#include <unistd.h>   // For sbrk
#include <string.h>   // For memset
#include <stdlib.h>
#include <stdio.h>

/* Your final implementation should comment out this macro. */
//#define MM_USE_STUBS

static s_block_ptr base = NULL; // Base of the memory list

/*
 * Find a suitable block using first-fit algorithm.
 */
s_block_ptr find_block(s_block_ptr *last, size_t size) {
    s_block_ptr b = base;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

/*
 * Extend the heap with a new block.
 */
s_block_ptr extend_heap(s_block_ptr last, size_t s) {
    s_block_ptr b = sbrk(0);
    if (sbrk(BLOCK_SIZE + s) == (void*)-1)
        return NULL;

    b->size = s;
    b->next = NULL;
    b->prev = last;
    b->free = 0;
    b->ptr = b->data;

    if (last)
        last->next = b;

    return b;
}

void* mm_malloc(size_t size) {
    s_block_ptr b, last;

    if (size <= 0) return NULL;

    // Initialize if this is the first call
    if (!base) {
        b = extend_heap(NULL, size);
        if (!b) return NULL;
        base = b;
    } else {
        // Find a block
        last = base;
        b = find_block(&last, size);
        if (!b) {
            // No fitting block, extend the heap
            b = extend_heap(last, size);
            if (!b) return NULL;
        } else if (b->size - size >= BLOCK_SIZE + sizeof(struct s_block)) {
            // Split block if too large
            split_block(b, size);
        }
        b->free = 0;
    }

    memset(b->data, 0, size);
    return b->data;
}

void *mm_realloc(void *ptr, size_t size) {
    void *new_ptr;
    s_block_ptr block;
    size_t copy_size;

    // Equivalent to mm_malloc(size) if ptr is NULL
    if (!ptr) return mm_malloc(size);

    // Equivalent to mm_free(ptr) if size is 0
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    block = get_block(ptr);
    if (block->size >= size) return ptr;  // Current block is big enough



    // Allocate new block
    new_ptr = mm_malloc(size);
    if (!new_ptr) return NULL; // Allocation failed

    // Copy old data to new block
    copy_size = block->size;
    memcpy(new_ptr, ptr, copy_size);

    // Free old block
    mm_free(ptr);

    // Initialize new bytes to zero if new size is larger
    if (size > copy_size)
        memset((char *)new_ptr + copy_size, 0, size - copy_size);

    return new_ptr;
}

/*
 * Get the block from a pointer.
 */
s_block_ptr get_block(void *p) {
    // Move backward to reach the block's metadata
    return (s_block_ptr)((char*)p - BLOCK_SIZE);
}

void split_block(s_block_ptr b, size_t s) {
    s_block_ptr new_block;
    size_t total_size;

    total_size = b->size;

    // Check if splitting is possible
    if (total_size < s + BLOCK_SIZE + sizeof(struct s_block)) return;

    // Create a new block at the address right after s
    new_block = (s_block_ptr)(b->data + s);
    new_block->size = total_size - s - BLOCK_SIZE;
    new_block->next = b->next;
    new_block->prev = b;
    new_block->free = 1;
    new_block->ptr = new_block->data;

    // Update the original block
    b->size = s;
    b->next = new_block;

    // Update the next block's previous pointer if it exists
    if (new_block->next) new_block->next->prev = new_block;
}


/*
 * Coalesce free adjacent blocks.
 */
s_block_ptr fusion(s_block_ptr b) {
    if (b->next && b->next->free) {
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next)
            b->next->prev = b;
    }
    return b;
}

void mm_free(void *ptr) {
    s_block_ptr b;

    if (!ptr) return;

    // Get the block from the pointer
    b = get_block(ptr);
    b->free = 1;

    // Merge with previous if possible
    if (b->prev && b->prev->free)
        b = fusion(b->prev);

    // Merge with next if possible
    if (b->next) {
        fusion(b);
    } else {
        // Reset the base/prev pointer
        if (b->prev) b->prev->next = NULL;
        else base = NULL;
    }
}
