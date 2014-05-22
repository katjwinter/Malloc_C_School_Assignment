#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*
 * mm.c - functions to replace the standard malloc, free, and realloc. These are implemented using a circularly linked doubly linked list. Free blocks are split 
 * 	and coalesced where possible, and where a free block is not already available, the heap is expanded by twice the requested size.
 */
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Winter",
    /* First member's full name */
    "Kat Winter",
    /* First member's email address */
    "katjwinter@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WORD_SIZE 4

/* 8 byte offset into a block to return a properly aligned block */
#define OFFSET 8

/* Overhead includes the listBlock struct and also the footer - aligning it accounts for the footer even though it isn't in the struct */
#define OVERHEAD ALIGN(sizeof(listBlock))

/* Struct to represent the information necessary to manage free and allocated blocks */
typedef struct blockoverhead listBlock;

struct blockoverhead {
  int header;
  listBlock *next;
  listBlock *prev;
};

listBlock *find_free_block(size_t size);
void check_heap();

/* 
 * mm_init - initialize the malloc package. Creates a 'dumnmy' first free block which will never be allocated.
 */
int mm_init(void)
{
  listBlock *start = mem_sbrk(OVERHEAD);
  start->header = OVERHEAD;
  start->next = start;
  start->prev = start;
  int *footer = (int*)(((char *)start) + OVERHEAD - WORD_SIZE); 
  *footer = OVERHEAD;
  return 0;
}

/* check_heap - Check the heap for consistency etc. Convenient to manually call while running GDB but is not called otherwise */
void check_heap() {  

  listBlock* ptr;
  int *footer;
  listBlock* heapStart = (listBlock*)mem_heap_lo();
  listBlock* heapEnd = (listBlock*)mem_heap_hi();

  printf("Address       Block size      Allocated\n");
  ptr = heapStart;
  while (ptr < heapEnd) {
    footer = (int *)(((char *)ptr) + (ptr->header&~0x1) - WORD_SIZE);
    printf("%5p%14d%12d%10s\n", ptr, (int)(ptr->header & ~0x1), ptr->header & 0x1, ptr->header == *footer?"OK":"corrupt");
    ptr = (listBlock*)((char*)ptr + (ptr->header & ~0x1));
  }
  
  printf("Heap Size Total: %5d\n", (int)mem_heapsize());
}

/* 
 * mm_malloc - Checks for an available free block of at least the required size (taking overhead and alignment into account).
 *     If no suitable free block is found, the heap is expanded by 2 x size, split, and coalesced if possible. If one is found, it's
 *     split if possible, otherwise the whole block is used.
 */
void *mm_malloc(size_t size)
{
    unsigned int totalSizeNeeded = ALIGN(size + OVERHEAD);
    listBlock *newBlock = find_free_block(totalSizeNeeded);
    int *footer;

    if (newBlock == NULL) { // no free block large enough
      newBlock = (listBlock *)mem_sbrk(totalSizeNeeded * 2);
      if ((long)newBlock == -1) {
	return NULL;
      }
      else { 
	newBlock->header = totalSizeNeeded; // set up free block at front
	footer = (int *)(((char *)newBlock) + totalSizeNeeded - WORD_SIZE);
	*footer = totalSizeNeeded;
	listBlock *firstFree = (listBlock *)mem_heap_lo(); // insert into free list
	newBlock->next = firstFree->next;
	newBlock->prev = firstFree;
	firstFree->next = newBlock; 
	newBlock->next->prev = newBlock;

	listBlock *allocate = (listBlock *)(((char *)newBlock) + totalSizeNeeded); // create a new allocated block after the free block 
	allocate->header = totalSizeNeeded | 0x1;
	footer = (int *)(((char *)allocate) + totalSizeNeeded - WORD_SIZE);
	*footer = totalSizeNeeded | 0x1;

	/* try coalesing the new leading free portion with the block in front */
	int *lowerFooter = (int *)(((char *)newBlock) - WORD_SIZE);
	listBlock *lowerBlock = (listBlock *)(((char *)newBlock) - (*lowerFooter & ~0x1));
	if ((lowerBlock->header & 0x1) == 0) { // if previous block is free
	  if (lowerBlock != (listBlock *)mem_heap_lo()) { // coalesce if it isn't the dummy starting block
	    lowerBlock->header = lowerBlock->header + newBlock->header;
	    footer = (int *)(((char *)lowerBlock) + lowerBlock->header - WORD_SIZE);
	    *footer = lowerBlock->header;
	    newBlock->next->prev = newBlock->prev; // remove block being absorbed from free list
	    newBlock->prev->next = newBlock->next;
	  }
	}
	return (char *)allocate + OFFSET;
      }
    }

    /* If a free block was found, split if possible and relink the free list appropriately */
    else {
      if (newBlock->header - totalSizeNeeded < OVERHEAD) { // not enough room left over to accomodate overhead required for a free block 
	footer = (int *)(((char *)newBlock) + newBlock->header - WORD_SIZE); // set pointer to the footer
	newBlock->header = newBlock->header | 0x1; // then just allocate the entire block
	*footer = newBlock->header;
	newBlock->prev->next = newBlock->next; // splice it out from the free list
	newBlock->next->prev = newBlock->prev;
      }
      
      else { // there is room left over to split the blocks
	int oldBlockSize = newBlock->header; // store the original block size
	newBlock->header = totalSizeNeeded | 0x1; // allocate a block of just the necessary size
	footer = (int *)(((char *)newBlock) + totalSizeNeeded - WORD_SIZE); // set pointer to footer
	*footer = totalSizeNeeded | 0x1; // set footer to size and allocated
	listBlock *splitBlock = (listBlock *)(((char *)newBlock) + totalSizeNeeded); // place a pointer immediately following the newly allocated block
	splitBlock->header = oldBlockSize - totalSizeNeeded; // set the size of the new free block
	footer = (int *)(((char *)splitBlock) + splitBlock->header - WORD_SIZE); // set pointer to footer
	*footer = oldBlockSize - totalSizeNeeded;
	splitBlock->prev = newBlock->prev; // insert splitBlock into the free list
	splitBlock->next = newBlock->next;
	newBlock->prev->next = splitBlock; // remove newBlock from the free list and redirect to splitBlock
	newBlock->next->prev = splitBlock;
      }
    }
    return (char *)newBlock + OFFSET;
}

/* Coalesce - Merge free blocks together */
int coalesce(listBlock *ptr) {
  listBlock *upperBlock = (listBlock *)(((char *)ptr) + ptr->header);
  if (upperBlock < (listBlock *)mem_heap_hi()) { // don't try to coalesce out of bounds
    if ((upperBlock->header & 0x1) == 0) { // if upperBlock is Free
      ptr->header = ptr->header + upperBlock->header; // absorb it into ptr
      int *footer = (int *)(((char *)ptr) + ptr->header - WORD_SIZE);
      *footer = ptr->header;
      upperBlock->next->prev = upperBlock->prev; // splite out the upperBlock from the free list
      upperBlock->prev->next = upperBlock->next;
    }
  }

  int *lowerFooter = (int *)(((char *)ptr) - WORD_SIZE);
  listBlock *lowerBlock = (listBlock *)(((char *)ptr) - (*lowerFooter & ~0x1));
  if ((lowerBlock->header & 0x1) == 0) { // if previous block is free
    if (lowerBlock != (listBlock *)mem_heap_lo()) { // don't coalesce with the dummy block because we don't want it to ever get allocated
      lowerBlock->header = lowerBlock->header + ptr->header;
      int *footer = (int *)(((char *)lowerBlock) + lowerBlock->header - WORD_SIZE);
      *footer = lowerBlock->header;
      return 0;
    }
  }

  return 1;
}

/* find_free_block - Search the linked list of free blocks to find one of sufficient size. If the search is taking too long without
 * 	locating a suitable block, give up and just expand the heap, my assumption being that if the first 400 blocks are that fragmented
 * 	then it's better to create a new, larger block at the front than have to keep scanning through so many that are too small. */
listBlock *find_free_block(size_t size) {

  int count = 0;
  listBlock *freeBlock;
  listBlock *heapStart = (listBlock *)mem_heap_lo();

  for (freeBlock = heapStart->next; (freeBlock != heapStart) && (freeBlock->header < size) && (count < 400); freeBlock = freeBlock->next) {
    count++;
  }

  if ((freeBlock != heapStart) && (freeBlock->header >= size)) {
    return freeBlock;
  }

  else {
    return NULL;
  }
}

/*
 * mm_free - Freeing a block with coalescing
 */
void mm_free(void *ptr)
{
  listBlock *blockToFree = ptr - OFFSET; // because the pointer the user has is to one that follows after the payload, so have to go back to start of block
  blockToFree->header = blockToFree->header & ~0x1; // set allocation bit to Free
  int *footer = (int *)(((char *)blockToFree) + blockToFree->header - WORD_SIZE); // set the footer even though it may well be unnecessary
  *footer = blockToFree->header; 

  int needsLinking = coalesce(blockToFree); // coalesce the block

  if (needsLinking == 1) { // if the block did not coalesce with the block in front of it, need to link it into the free list
    listBlock *firstFree = (listBlock *)mem_heap_lo();
    blockToFree->next = firstFree->next;
    blockToFree->prev = firstFree;
    firstFree->next = blockToFree; 
    blockToFree->next->prev = blockToFree;
  }
}

/*
 * mm_realloc - Implemented fairly simply by finding a free block (or expanding the heap), copying the data, and freeing the original block
 */
void *mm_realloc(void *ptr, size_t size)
{
  int totalSizeNeeded = ALIGN(size + OVERHEAD);
  listBlock *oldBlock = ptr - OFFSET;
  listBlock *newBlock = find_free_block(totalSizeNeeded);

  int copySize = (oldBlock->header & ~0x1) - OFFSET;
  if (totalSizeNeeded < copySize) {
    copySize = totalSizeNeeded;
  }

  if (newBlock == NULL) { // no free block found
    newBlock = (listBlock *)mem_sbrk(totalSizeNeeded);
    if ((long)newBlock == -1) {
      return NULL;
    }
    else {
      newBlock->header = totalSizeNeeded | 0x1;
      int *footer = (int *)(((char *)newBlock) + totalSizeNeeded - WORD_SIZE);
      *footer = totalSizeNeeded | 0x1;
    }
  }

  else { // if a free block was found
    if (newBlock->header - totalSizeNeeded < OVERHEAD) { // not enough room left over for overhead of a free block
      newBlock->header = newBlock->header | 0x1; // allocate the whole block
      newBlock->prev->next = newBlock->next; // remove from free list
      newBlock->next->prev = newBlock->prev;
    }
    else {
      int oldBlockSize = newBlock->header; // store the found block's size
      newBlock->header = totalSizeNeeded | 0x1;
      int *footer = (int *)(((char *)newBlock) + totalSizeNeeded - WORD_SIZE);
      *footer = totalSizeNeeded | 0x1;
      listBlock *splitBlock = (listBlock *)(((char *)newBlock) + totalSizeNeeded);
      splitBlock->header = oldBlockSize - totalSizeNeeded;
      footer = (int *)(((char *)splitBlock) + splitBlock->header - WORD_SIZE);
      *footer = oldBlockSize - totalSizeNeeded;
      splitBlock->prev = newBlock->prev; // insert splitBlock into the free list
      splitBlock->next = newBlock->next;
      newBlock->prev->next = splitBlock; // remove newBlock from the free list and redirect to splitBlock
      newBlock->next->prev = splitBlock;
    }
  }
  
  if (newBlock == NULL) {
    return NULL;
  }
  newBlock = (listBlock *)((char *)newBlock + OFFSET);

  memcpy(newBlock, ptr, copySize);
  mm_free(ptr);
  return newBlock;
}
