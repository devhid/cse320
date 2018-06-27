/*
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "budmm.h"
#include "budprint.h"

// Macro to get the correct index for a specific order in the free list.
#define FREE_LIST_INDEX(ord) (ord - ORDER_MIN)

/*
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in budmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
extern bud_free_block free_list_heads[NUM_FREE_LIST];

/* Helper Functions */
uint32_t get_required_padding(uint8_t order, uint32_t rsize);
uint8_t get_required_order(uint32_t rsize);
void insert_into_freelist(bud_free_block *block);
void coalesce_blocks(bud_free_block *block);
int validate_pointer(void *ptr);
int increase_heap();
int delete_from_freelist_b(bud_free_block *block);
bud_free_block *delete_from_freelist(uint8_t order);
bud_free_block *split_block(uint32_t rsize, bud_free_block *block);
bud_free_block *process_new_block(uint32_t rsize);
bud_free_block *get_buddy(bud_free_block *block);

/* Check header file for documentation. */
void *bud_malloc(uint32_t rsize) {
    // Check if the requested size is invalid.
    if( rsize == 0 || rsize > (MAX_BLOCK_SIZE - sizeof(bud_header)) ) {
        errno = EINVAL;
        return NULL;
    }

    // Try to get available free block.
    bud_free_block *block = delete_from_freelist(get_required_order(rsize));

    // If there's no free block available for that specific size,
    // process a new block which may require splitting.
    if(block == NULL) {
        block = process_new_block(rsize);

        // If we still can't get a free block, then set the error and return NULL.
        if(block == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    // Set the block properties to mark the block as allocated.
    block->header.allocated = 1;
    block->header.rsize = rsize;

    // If we need padding, set padded to 1, otherwise set it to 0.
    block->header.padded = get_required_padding(block->header.order, rsize) != 0 ? 1 : 0;

    // Advance the pointer to the payload area.
    block = (bud_free_block *) (((char *) block) + sizeof(bud_header));

    // Return the allocated block.
    return (void *) block;
}

/* Check header file for documentation. */
void *bud_realloc(void *ptr, uint32_t rsize) {
    // If the pointer is NULL, then this function should behave like a call to
    // bud_malloc with the same rsize.
    if(ptr == NULL) {
        return bud_malloc(rsize);
    }

    // If the requested size is 0, then this method should behave like a call to
    // bud_free with the same pointer and return NULL.
    if(rsize == 0) {
        bud_free(ptr);
        return NULL;
    }

    // Check if the requested size is invalid.
    if( rsize > (MAX_BLOCK_SIZE - sizeof(bud_header)) ) {
        errno = EINVAL;
        return NULL;
    }

    // Check if the pointer is invalid. If so, abort.
    if(!validate_pointer(ptr)) {
        abort();
    }

    // Convert from payload to header pointer.
    bud_free_block *block = (bud_free_block *) (((char *) ptr) - sizeof(bud_header));
    bud_header header = block->header;

    // Get the appropriate order for the given block size.
    uint8_t order = get_required_order(rsize);

    // If the pointer already has a block size that can accomodate the requested size,
    // then we just have to return the ptr because no other work is required.
    // Note: This includes the case where the requested size is less than the
    //       requested size of the original block but the difference is not
    //       by a factor of 2.
    if(header.order == order) {
        // Update the rsize.
        block->header.rsize = rsize;

        // If we need padding, set padded to 1, otherwise set it to 0.
        block->header.padded = get_required_padding(block->header.order, rsize) != 0 ? 1 : 0;

        return ptr;
    }

    // If the requested size is larger than what we already have,
    // then we have to bud_malloc the larger size, move the information from
    // the old block to the new block, and free the old block.
    else if(header.order < order) {
        void *new_block = bud_malloc(rsize);

        // If we can't allocate more space, return NULL.
        if(new_block == NULL) {
            return NULL;
        }

        // Save the rsize before we move block pointer to payload area.
        uint32_t rsize = block->header.rsize;

        // Copy the information from the old block to the new block.
        memcpy(new_block, ptr, rsize);

        // Free the old block.
        bud_free(ptr);

        // Return the new bigger block with the data copied over.
        return new_block;
    }

    // If the requested size if smaller whan what we already have
    // but the current block is at least twice as large as necessary,
    // then we keep splitting the block and adding the higher-addressed blocks
    // to the free list before returning the original pointer.
    else {
        // Split the block.
        // We can ignore the return value because we already have a reference.
        split_block(rsize, block);

        // Set the block properties.
        block->header.allocated = 1;
        block->header.rsize = rsize;

        // If we need padding, set padded to 1, otherwise set it to 0.
        block->header.padded = get_required_padding(block->header.order, rsize) != 0 ? 1 : 0;

        // Return the reallocated block.
        return ptr;
    }
}

/* Check header file for documentation. */
void bud_free(void *ptr) {
    // If the pointer is NULL or invalid, abort.
    if(ptr == NULL || !validate_pointer(ptr)) {
        abort();
    }

    // Convert from payload to header pointer.
    bud_free_block *block = (bud_free_block *) (((char *) ptr) - sizeof(bud_header));

    // Change block allocation status to 0 to mark it as free.
    block->header.allocated = 0;

    // We do not coalesce if the order of the block is ORDER_MAX - 1
    // because we do not want it to coalesce into a block of ORDER MAX.
    if(block->header.order == (ORDER_MAX - 1)) {
        insert_into_freelist(block);
    } else {
        // Attempt immediate coalescing.
        // This will try to coalesce the blocks. If we cannot coalesce,
        // we will just add the block to the free list as intended.
        coalesce_blocks(block);
    }
}

/*
 * Repeatedly attempts to combine (coalesce) adjacent free blocks to
 * the given block.
 *
 * @param block the block whose "buddies" are being coalesced if they are valid.
 */
void coalesce_blocks(bud_free_block *block) {
    bud_free_block *buddy = NULL;
    bud_free_block *lower = block;

    while(1) {
        // Get the buddy of the block that will be coalesced.
        buddy = get_buddy(lower);

        // Check if lower has an order size of ORDER_MAX - 1.
        // Check if the buddy has a different order.
        // Check if the buddy is not free.
        // Check if the buddy is not a block that's already being coalesced.
        //      Note: Not sure if this check is necessary.
        if(
            buddy == NULL ||
            lower->header.order == ORDER_MAX - 1 ||
            buddy->header.order != lower->header.order ||
            buddy->header.allocated == 1 ||
            (buddy->next == NULL && buddy->prev == NULL)) {
                insert_into_freelist(lower);
                return;
        }

        // Remove the buddy from the free list so it can be coalesced.
        if(delete_from_freelist_b(buddy)) {
            // Keeps track of the lower addressed block.
            lower = (lower < buddy) ? lower : buddy;

            // Increment the order of the block that is lower addressed.
            (lower->header.order)++;
        }
    }
}

/* Returns the buddy of a specific block.
 * The buddy of a block with address A and size S will have the address A^S.
 *
 * @param block the block whose buddy is being looked for.
 */
bud_free_block *get_buddy(bud_free_block *block) {
    return (bud_free_block *) ( ((uintptr_t) block) ^ (ORDER_TO_BLOCK_SIZE(block->header.order)) );
}

/*
 * Splits a block until the lower buddy of a block fits the given rsize appropriately.
 *
 * @param rsize the requested payload (in bytes)
 * @param block the block being split
 */
bud_free_block *split_block(uint32_t rsize, bud_free_block *block) {
    bud_free_block *higher = NULL;

    // If we don't have the appropriate fit yet, keep on splitting the block.
    while(get_required_order(rsize) != block->header.order) {
        // Get the block size (in bytes) of the block.
        uint32_t block_size = ORDER_TO_BLOCK_SIZE(block->header.order);

        // Split the block into half.
        higher = (bud_free_block *) ( ((char *) block) + (block_size >> 1) );

        // Edit the header for the higher addressed block.
        higher->header.allocated = 0; // Set its allocation status to free.
        higher->header.order = block->header.order - 1; // Decrease the order by 1.

        // Decrease original block order by 1.
        (block->header.order)--;

        // Add the higher addressed block to the appropriate free list.
        insert_into_freelist(higher);
    }

    // Return a pointer to the block with its appropriate size.
    return block;
}

/*
 * Returns a new block for the given rsize if one is not available in the free list.
 * If the required size is not of ORDER_MAX, then splitting must occur.
 *
 * @param rsize the requested payload (in bytes)
 */
bud_free_block *process_new_block(uint32_t rsize) {
    bud_free_block *new_block = NULL;

    for(uint8_t order = get_required_order(rsize); order < ORDER_MAX; order++) {
        // Get the sentinel node for the given order in the free list.
        bud_free_block *sentinel = &free_list_heads[FREE_LIST_INDEX(order)];

         // If the list is non-empty for that block size,
         // set the new block equal to available block for that size.
        if(sentinel->next != sentinel && sentinel->prev != sentinel) {
            new_block = sentinel->next;
            break;
        }
    }

    // If there are no more free blocks bigger than the requested size,
    // increase heap size and try again.
    if(new_block == NULL) {
        if(!increase_heap()) {
            return NULL;
        }

        return process_new_block(rsize);
    } else {
        if(new_block->header.order == 0) {
            return NULL;
        }
        // Delete the block from the freelist.
        // We can ignore the return value because we have a reference to new_block.
        delete_from_freelist(new_block->header.order);

        // If the block is of order max, then we just return it because no splitting is required.
        if(new_block->header.order == (ORDER_MAX - 1) && get_required_order(rsize) == (ORDER_MAX - 1)) {
            return new_block;
        }

        // Otherwise, split the block until we get a block with the correct size.
        return split_block(rsize, new_block);
    }
}

/*
 * Validates a pointer according to the commented criteria below.
 *
 * @param ptr the pointer being validated
 */
int validate_pointer(void *ptr) {
    // Check if address if is not between heap_start and heap_end.
    if(ptr < bud_heap_start() || ptr >= bud_heap_end()) {
        return 0;
    }

    // Convert from payload area to header area.
    bud_free_block *block = (bud_free_block *) (((char *) ptr) - sizeof(bud_header));
    bud_header header = block->header;

    // Check if address is not aligned to a multiple of 8.
    if( ((uintptr_t) block) % 8 != 0) {
        return 0;
    }

    // Check if the value in the order field is not between [ORDER_MIN, ORDER_MAX).
    if(header.order < ORDER_MIN || header.order >= ORDER_MAX) {
        return 0;
    }

    // Check if the allocated bit is 0.
    if(header.allocated == 0) {
        return 0;
    }

    uint32_t required_padding = get_required_padding(header.order, header.rsize);

    // Check if padded bit is 0 when it's not supposed to.
    if(header.padded == 0 && required_padding != 0) {
        return 0;
    }

    // Check if padded bit is 1 when it's not supposed to.
    if(header.padded == 1 && required_padding == 0) {
        return 0;
    }

    // Check if requested size and order don't make sense.
    // Example: rsize = 100 but order = 6 (supposed to be 7)
    if(header.order != get_required_order(header.rsize)) {
        return 0;
    }

    return 1;
}

/*
 * Calculates the padding required for a specific block size
 * given a requested payload.
 *
 * @param order the order of the block
 * @param rsize the requested payload (in bytes)
 */
uint32_t get_required_padding(uint8_t order, uint32_t rsize) {
    return (uint32_t) ((ORDER_TO_BLOCK_SIZE(order) - sizeof(bud_header)) - rsize);
}

/*
 * Calculates the appropriate order for a given rsize.
 *
 * @param rsize the requested payload (in bytes)
 */
uint8_t get_required_order(uint32_t rsize) {
    uint8_t order = 0;
    for(order = ORDER_MIN; ORDER_TO_BLOCK_SIZE(order) < (rsize + sizeof(bud_header)); order++);

    return order;
}

/*
 * Deletes the given block and returns 1 if the block was actually in
 * the free list and was removed, 0 otherwise.
 *
 * @param block the block that is being deleted from the free list.
 */
int delete_from_freelist_b(bud_free_block *block) {
    // Get the sentinel node from the free list for the given block's order.
    bud_free_block *sentinel = &free_list_heads[FREE_LIST_INDEX(block->header.order)];

    // If the list is empty, return 0.
    if(sentinel->next == sentinel && sentinel->prev == sentinel) {
        return 0;
    }

    // Loop through the list until the block we find matches the given block.
    // If we find it, delete it and return 1, otherwise return 0.
    for(bud_free_block *current = sentinel; current->next != sentinel; current = current->next) {
        if(current->next != block) {
            continue;
        }

        current->next = block->next;
        block->next->prev = current;

        block->next = NULL;
        block->prev = NULL;

        return 1;
    }

    return 0;
}

/*
 * Delete and retrieve the first free block in the free list for the given order.
 *
 * @param order the order of the block being retrieved (and deleted) from the free list.
 */
bud_free_block *delete_from_freelist(uint8_t order) {
    // Get the sentinel node from the free list for the given order.
    bud_free_block *sentinel = &free_list_heads[FREE_LIST_INDEX(order)];

    // Get the first non-sentinel node in that free list.
    bud_free_block *deleted = sentinel->next;

    // If the list is empty, return NULL.
    if(sentinel->next == sentinel && sentinel->prev == sentinel) {
        return NULL;
    }

    // Else, delete the node and return it.
    sentinel->next = deleted->next;
    deleted->next->prev = sentinel;

    deleted->next = NULL;
    deleted->prev = NULL;

    return deleted;
}

/*
 * Inserts the given block into its appropriate free list according to its order.
 *
 * @param the block being inserted into the free list.
 */
void insert_into_freelist(bud_free_block *block) {
    // Get the sentinel node from the free list for the given block's order.
    bud_free_block *sentinel = &free_list_heads[FREE_LIST_INDEX(block->header.order)];

    // If the list is empty.
    if(sentinel->next == sentinel && sentinel->prev == sentinel) {
        sentinel->next = block;
        sentinel->prev = block;

        block->prev = sentinel;
        block->next = sentinel;
    } else {
        block->next = sentinel->next;
        sentinel->next->prev = block;

        sentinel->next = block;
        block->prev = sentinel;
    }
}

/*
 * Increases the size of the heap by MAX_BLOCK_SIZE.
 * Note: This function is a combination of bud_sbrk() + actually adding the
 *       the block of size ORDER_MAX into its free list.
 *
 * @return 1 if the heap could successfully be increased, 0 otherwise.
 */
int increase_heap() {
    void *current = bud_sbrk();

    // If the heap cannot be increased, return 0.
    if(current == (void *) -1) {
        return 0;
    }

    // Cast the given memory to a bud_free_block.
    bud_free_block *block = (bud_free_block *) current;

    // Edit the allocation status and order of the new block.
    block->header.allocated = 0;
    block->header.order = ORDER_MAX - 1;

    // Insert the new block into the free list.
    insert_into_freelist(block);

    // Return 1 because the heap could be successfully expanded.
    return 1;
}
