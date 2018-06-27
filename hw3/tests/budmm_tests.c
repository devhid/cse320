#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <time.h>
#include "budmm.h"
#include "debug.h"
#include "budprint.h"

#define UNALLOCATED 0
#define UNPADDED 0
#define ALLOCATED 1
#define PADDED 1

#define HEADER_TO_PAYLOAD(hdr) (((char *)hdr) + sizeof(bud_header))
#define PAYLOAD_TO_HEADER(ptr) (bud_header *)(((char *)ptr - sizeof(bud_header)))

static int free_list_is_empty(int order) {
    int i = order - ORDER_MIN;
    return(free_list_heads[i].next == &free_list_heads[i]);
}

static void assert_empty_free_list(int order) {
    cr_assert_neq(free_list_is_empty(order), 0,
		  "List [%d] contains an unexpected block!", order - ORDER_MIN);
}

static void assert_nonempty_free_list(int order) {
    cr_assert_eq(free_list_is_empty(order), 0,
		 "List [%d] should not be empty!", order - ORDER_MIN);
}

void assert_null_free_lists() {
    for (int order = ORDER_MIN; order < ORDER_MAX; order++)
	assert_empty_free_list(order);
}

void expect_errno_value(int exp_errno) {
    cr_assert(errno == exp_errno,
	      "`errno` was incorrectly set. Expected [%d] Actual: [%d]\n",
	      exp_errno, errno);
}

void assert_header_values(bud_header *bhdr, int exp_alloc, int exp_order,
			  int exp_pad, int exp_req_sz) {
    cr_assert(bhdr->allocated == exp_alloc,
	      "header `allocated` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      exp_alloc, bhdr->allocated);
    cr_assert(bhdr->order == exp_order,
	      "header `order` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      exp_order, bhdr->order);
    cr_assert(bhdr->padded == exp_pad,
	      "header `padded` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      exp_pad, bhdr->padded);
    cr_assert(bhdr->rsize == exp_req_sz,
	      "header `rsize` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      exp_req_sz, bhdr->rsize);
}

void assert_free_block_values(bud_free_block *fblk, int exp_order,
			      void *exp_prev_ptr, void *exp_next_ptr) {
    bud_header *bhdr = &fblk->header;

    cr_assert(bhdr->allocated == UNALLOCATED,
	      "header `allocated` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      UNALLOCATED, bhdr->allocated);
    cr_assert(bhdr->order == exp_order,
	      "header `order` bits were not properly set. Expected: [%d] Actual: [%d]\n",
	      exp_order, bhdr->order);
    cr_assert((void*)fblk->prev == exp_prev_ptr,
	      "`prev` pointer was not properly set. Expected: [%p] Actual: [%p]\n",
	      exp_prev_ptr, (void*)fblk->prev);
    cr_assert((void*)fblk->next == exp_next_ptr,
	      "`next` pointer was not properly set. Expected: [%p] Actual: [%p]\n",
	      exp_next_ptr, (void*)fblk->next);
}

Test(bud_malloc_suite, easy_malloc_a_pointer, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;
    int **x = bud_malloc(sizeof(int *));

    cr_assert_not_null(x, "bud_malloc returned null");

    int a = 4;
    *x = &a;

    cr_assert(*x == &a, "bud_malloc failed to give proper space for a pointer!");

    bud_header *bhdr = PAYLOAD_TO_HEADER(x);
    assert_header_values(bhdr, ALLOCATED, ORDER_MIN, PADDED, sizeof(int *));
    expect_errno_value(0);
}

Test(bud_malloc_suite, medium_malloc_diff_types, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    struct s1 {
        int a;
        float b;
        char *c;
    };

    struct s2 {
        int a[100];
        char *b;
    };

    uint32_t size = MIN_BLOCK_SIZE - sizeof(bud_header);
    char* carr = bud_malloc(size);
    cr_assert_not_null(carr, "bud_malloc returned null on the first call");
    for (int i = 0; i < size; i++) {
        carr[i] = 'a';
    }

    uint32_t sizeof_s1 = sizeof(struct s1);
    struct s1 *s_1 = bud_malloc(sizeof_s1);
    cr_assert_not_null(s_1, "bud_malloc returned null on the second call");
    s_1->a = 4;
    s_1->b = 2;

    uint32_t sizeof_s2 = sizeof(struct s2);
    struct s2 *s_2 = bud_malloc(sizeof_s2);
    cr_assert_not_null(s_2, "bud_malloc returned null on the third call");
    for (int i = 0; i < 100; i++) {
        s_2->a[i] = 5;
    }

    bud_header *carr_hdr = PAYLOAD_TO_HEADER(carr);
    bud_header *s1_hdr = PAYLOAD_TO_HEADER(s_1);
    bud_header *s2_hdr = PAYLOAD_TO_HEADER(s_2);

    assert_header_values(carr_hdr, ALLOCATED, ORDER_MIN, UNPADDED, size);
    for(int i = 0; i < size; i++) {
        cr_expect(carr[i] == 'a', "carr[%d] was changed!", i);
    }

    assert_header_values(s1_hdr, ALLOCATED, ORDER_MIN, PADDED, sizeof_s1);
    cr_expect(s_1->a == 4, "field `a` of struct s_1 was changed!");

    assert_header_values(s2_hdr, ALLOCATED, 9, PADDED, sizeof_s2);
    for (int i = 0; i < 100; i++) {
        cr_expect(s_2->a[0] == 5, "field `a` of struct s_2 was changed!");
    }

    expect_errno_value(0);
}

Test(bud_malloc_suite, malloc_max_heap, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    for(int n = 0; n < MAX_HEAP_SIZE / MAX_BLOCK_SIZE; n++) {
    	char *x = bud_malloc(MAX_BLOCK_SIZE - sizeof(bud_header));

    	for(int i = 0; i < MAX_BLOCK_SIZE - sizeof(bud_header); i++) {
    	    x[i] = 'b';
    	}

    	cr_assert_not_null(x);

    	bud_header *bhdr = PAYLOAD_TO_HEADER(x);
    	assert_header_values(bhdr, ALLOCATED, ORDER_MAX-1, UNPADDED,
    			     MAX_BLOCK_SIZE - sizeof(bud_header));
    	assert_null_free_lists();
    	expect_errno_value(0);

    }

    int *y = bud_malloc(sizeof(int));
    cr_assert_null(y);
    expect_errno_value(ENOMEM);
}

Test(bud_free_suite, free_no_coalesce, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    void *a = bud_malloc(4096 - sizeof(bud_header)); // -> 4096
    int *x = bud_malloc(sizeof(int)); // -> MIN_BLOCK_SIZE
    void *b = bud_malloc(sizeof(double)*2); // -> MIN_BLOCK_SIZE
    char *y = bud_malloc(sizeof(char)*100); // -> 128
    bud_header *bhdr_b = PAYLOAD_TO_HEADER(b);

    assert_header_values(bhdr_b, ALLOCATED, ORDER_MIN, PADDED, sizeof(double)*2);

    bud_free(x);

    bud_free_block *blk = free_list_heads[0].next; // only x is expected on the list
    assert_nonempty_free_list(ORDER_MIN);
    assert_free_block_values(blk, ORDER_MIN, &free_list_heads[0], &free_list_heads[0]);

    bud_free(y);

    blk = free_list_heads[7-ORDER_MIN].next;
    assert_nonempty_free_list(7);
    assert_free_block_values(blk, 7, &free_list_heads[7-ORDER_MIN],
			     &free_list_heads[7-ORDER_MIN]);

    cr_expect(bud_heap_start() + 1*MAX_BLOCK_SIZE == bud_heap_end(),
	      "Allocated more heap than necessary!");

    expect_errno_value(0);
}

Test(bud_free_suite, free_coalesce_higher_addr_check_ptrs, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

                                             //  5   6   7   8   9  10  11  12  13  14
                                             //  0   0   0   0   0   0   0   0   0   0
    void *a = bud_malloc(sizeof(long));      //  1   1   1   1   1   1   1   1   1   0
    void *w = bud_malloc(sizeof(int) * 100); //  1   1   1   1   0   1   1   1   1   0
    void *x = bud_malloc(sizeof(char));      //  0   1   1   1   0   1   1   1   1   0
    void *b = bud_malloc(sizeof(int));       //  1   0   1   1   0   1   1   1   1   0
    void *y = bud_malloc(sizeof(int) * 100); //  1   0   1   1   1   0   1   1   1   0
    void *z = bud_malloc(sizeof(char));      //  0   0   1   1   1   0   1   1   1   0
    void *c = bud_malloc(sizeof(int));       //  1   1   0   1   1   0   1   1   1   0
    void *d = bud_malloc(sizeof(int));       //  0   1   0   1   1   0   1   1   1   0

    assert_empty_free_list(5);
    assert_nonempty_free_list(6);
    assert_empty_free_list(7);
    assert_nonempty_free_list(8);
    assert_nonempty_free_list(9);
    assert_empty_free_list(10);

    bud_free(c);                             //  1   1   0   1   1   0   1   1   1   0
    bud_free(z);                             //  2   1   0   1   1   0   1   1   1   0
    bud_free(y);                             //  2   1   0   1   0   1   1   1   1   0
    bud_free(a);                             //  3   1   0   1   0   1   1   1   1   0
    bud_free(b);                             //  2   2   0   1   0   1   1   1   1   0
    bud_free(x);                             //  1   1   1   1   0   1   1   1   1   0

    assert_nonempty_free_list(5);
    assert_nonempty_free_list(6);
    assert_nonempty_free_list(7);
    assert_nonempty_free_list(8);
    assert_empty_free_list(9);
    assert_nonempty_free_list(10);

    bud_header *y_hdr = PAYLOAD_TO_HEADER(y);
    cr_assert(((void*)free_list_heads[10-ORDER_MIN].next == (void*)y_hdr),
		      "The block in free list %d should be %p!",
	      10-ORDER_MIN, y_hdr);
    assert_free_block_values((bud_free_block*)y_hdr, 10,
			     &free_list_heads[10-ORDER_MIN],
			     &free_list_heads[10-ORDER_MIN]);

    bud_header *a_hdr = PAYLOAD_TO_HEADER(a);
    cr_assert(((void*)free_list_heads[7-ORDER_MIN].next == (void*)a_hdr),
		      "The block in free list %d should be %p!",
	      7-ORDER_MIN, a_hdr);
    assert_free_block_values((bud_free_block*)a_hdr, 7,
			     &free_list_heads[7-ORDER_MIN],
			     &free_list_heads[7-ORDER_MIN]);

    expect_errno_value(0);
}

Test(bud_realloc_suite, realloc_diff_hdr, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5, .signal = SIGABRT) {
    errno = 0;
    int *x = bud_malloc(sizeof(int));

    bud_header *bhdr = PAYLOAD_TO_HEADER(x);

    bhdr->order = ORDER_MIN + 1;

    void *y = bud_realloc(x, 200);
    (void)y;
}

Test(bud_realloc_suite, realloc_size_zero_free, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    void *x = bud_malloc(sizeof(int));
    bud_malloc(sizeof(int));

    void *y = bud_realloc(x, 0); // should just free x

    cr_assert_null(y);

    assert_nonempty_free_list(ORDER_MIN);
    assert_free_block_values(free_list_heads[0].next, ORDER_MIN,
			     &free_list_heads[0], &free_list_heads[0]);

    expect_errno_value(0);
}

Test(bud_realloc_suite, realloc_larger_block, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    void *original = bud_malloc(sizeof(int)); // 13, 12, 11, 10, 9, 8, 7, 6, 5
    bud_malloc(500); // 13, 12, 11, 10, 8, 7, 6, 5
    int* new = bud_realloc(original, sizeof(int) * 100); // 400 -> 512
    // original will do a few steps of coalesce, resulting in 512
    // 13, 12, 11, 9, 9, 8, 7, 6, 5
    // 13, 12, 11, 9, 8, 7, 6, 5, 5

    bud_header *bhdr_new = PAYLOAD_TO_HEADER(new);
    assert_header_values(bhdr_new, ALLOCATED, 9, PADDED, sizeof(int) * 100);

    cr_assert_not_null(new, "bud_realloc returned NULL");
    assert_nonempty_free_list(9);
    cr_assert_neq(free_list_heads[9-ORDER_MIN].next->next,
		  &free_list_heads[9-ORDER_MIN],
		  "A second block is expected in free list #%d!",
		  9-ORDER_MIN);

    expect_errno_value(0);
}

Test(bud_realloc_suite, realloc_smaller_block_free_block, .init = bud_mem_init, .fini = bud_mem_fini,
     .timeout = 5) {
    errno = 0;

    void *x = bud_malloc(sizeof(double) * 4); // 32 -> 64
    void *y = bud_realloc(x, sizeof(int));

    cr_assert_not_null(y, "bud_realloc returned NULL!");

    bud_header *bhdr_y = PAYLOAD_TO_HEADER(y);
    assert_header_values(bhdr_y, ALLOCATED, ORDER_MIN, PADDED, sizeof(int));

    assert_nonempty_free_list(ORDER_MIN);
    cr_assert(((char *)(free_list_heads[0].next) == (char *)(bhdr_y) + 32),
	      "The split block of bud_realloc is not at the right place!");

    expect_errno_value(0);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

// Makes sure that the memory from new pointer is actually copied over to old pointer in a realloc call.
Test(bud_custom_suite, realloc_mem_copied_over, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    int *_int = (int *) bud_malloc(sizeof(int)); // 4 bytes -> block of ORDER 5 (32 bytes)
    *_int = 5;
    _int = bud_realloc(_int, sizeof(int) * 10); // 40 bytes -> block of ORDER 6 (64 bytes)

    cr_assert_eq(*_int, 5, "Integer value not copied over.");
    bud_free(_int);

    char *_string = (char *) bud_malloc(sizeof(char) * 7); // 7 bytes -> block of ORDER 5 (32 bytes)
    _string[0] = 'c';
    _string[1] = 's';
    _string[2] = 'e';
    _string[3] = '3';
    _string[4] = '2';
    _string[5] = '0';
    _string[6] = '\0';
    _string = bud_realloc(_string, sizeof(char) * 25); // 25 bytes -> block of ORDER 6 (64 bytes)

    cr_assert_arr_eq(_string, "cse320", 7, "String value not copied over.");
}

// Test will fail if the code attempts to coalesce blocks of ORDER_MAX - 1.
Test(bud_custom_suite, no_coalescing_of_max_block_size, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    int *max = bud_malloc(MAX_BLOCK_SIZE - sizeof(bud_header));
    int *max2 = bud_malloc(MAX_BLOCK_SIZE - sizeof(bud_header));
    bud_free(max);
    bud_free(max2);
}

// Makes sure that if a NULL pointer is passed with an rsize of 0, then a call to bud_malloc(0) is returned which will
// return NULL and set errno to EINVAL.
Test(bud_custom_suite, null_pointer_and_empty_rsize, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    int *ptr = bud_realloc(NULL, 0);

    cr_assert_null(ptr);
    expect_errno_value(22); // ERROR STATUS 22 = EINVAL
}

// Keep on malloc'ing small blocks to see how much the allocator can handle.
// This has a lot of overhead because the blocks will keep on having to be split to ensure the
// appropriate fit for the smallest block size.
Test(bud_custom_suite, numerous_malloc_calls, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    for(int i = 0; i < 10000000; i++) {
        void *ptr = bud_malloc(1);
    }
}

// Makes sure that if you try to realloc a block with an rsize greater than its original by at least an order of 1,
// but there is not enough heap space for the additional memory, then the pointer returned by bud_realloc is NULL
// and errno will be set to ENOMEM.
Test(bud_custom_suite, realloc_max_heap, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    // This will allocate all of the heap space available except
    // it will have space for one more BLOCK_SIZE_MAX - sizeof(bud_header)
    for(int i = 0; i < (MAX_HEAP_SIZE / MAX_BLOCK_SIZE) - 1; i++) {
        int *x = bud_malloc(MAX_BLOCK_SIZE - sizeof(bud_header));
    }

    // IF BLOCK_MAX_SIZE is of order 14, then we allocate a block size of exactly order 13 (with space for header).
    void *ptr = bud_malloc((MAX_BLOCK_SIZE - ORDER_TO_BLOCK_SIZE(ORDER_MAX - 2)) - sizeof(bud_header));

    // Since there is not enough memory for this call but we are specifying an rsize that is bigger than
    // the previous rsize by order 1, it will attempt to malloc the new block.
    ptr = bud_realloc(ptr, MAX_BLOCK_SIZE - sizeof(bud_header));

    cr_assert_null(ptr);
    expect_errno_value(12);
}

// Test function calls with random numbers of bytes continuously. Going to see if this type of combination
// with all of the calls will disrupt the allocator.
Test(bud_custom_suite, random_valid_continuous_input, .init = bud_mem_init, .fini = bud_mem_fini, .timeout = 5) {
    srand(time(0)); // Set the random seed according to the time.

    // Will malloc random number of bytes between 1 and MAX_BLOCK_SIZE (minus the header).
    // Will then do the same thing with another random number for realloc.
    // Will then try to free a random number of blocks.
    for(int i = 0; i < 1000000; i++) {
        char *random = bud_malloc((rand() % ((MAX_BLOCK_SIZE) - sizeof(bud_header))) + 1);
        random = bud_realloc(random, (rand() % ((MAX_BLOCK_SIZE) - sizeof(bud_header))) + 1);

        if((rand() % ((MAX_BLOCK_SIZE) - sizeof(bud_header))) + 1 > (rand() % ((MAX_BLOCK_SIZE) - sizeof(bud_header))) + 1) {
            if(random != NULL) {
                bud_free(random);
            }
        }
    }
}
