#ifndef BUDPRINT_H
#define BUDPRINT_H
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "budmm.h"

/*
 * Print block header. Print next and prev if the block is free.
 */
void bud_blockprint(bud_header *hdr);

/*
 * Print the elements address of free_list_heads[i].
 * Printing loop ends at the comeback to head.
 * If there is no free block on the list, print NONE after the head.
 */
void bud_listprint(int i);

/*
 * Print all of the lists from ORDER_MIN to ORDER_MAX - 1.
 */
void bud_print_all_lists(void);

#endif
