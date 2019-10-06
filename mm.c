/*
    Adapted the code from Computer Systems: A Programmer's Perspective, 3rd Edition 9.9.12 Implementing a Simple Allocator
 */

/*
 * mm.c - A malloc allocator package implemented based on an implicit free list
 * 
 * In this approach, a block is allocated by setting its header and footer to corresponding sizes
 *  There are headers or footers.  Blocks are immediately coalesced or reused. 
 *  Realloc is optimized based on the least copying possible policy
 *  
 * A block is split when necessary to enable larger utilization, and a next-fit approach is used to increase
 * throughput
 *
 * OPTIMIZATION DONE: a next fit approach is utilized and optimized mm_reaclloc to prevent copying the original content
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"



/* Basic constants and macros */
#define WSIZE 4 /*Word and header/footer size (bytes) */
#define DSIZE 8 /*Double word size (bytes) */
#define CHUNKSIZE (1<<12) /*Extend heap by this amount (bytes)*/

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/*Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)=(val))

/*Read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE 4

/*a global variable that always points to the prologue block */
static char *heap_listp = 0;
static char *previous = 0;

/* Prototypes for helper methods */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);

/*
 *  * mm_check heap consistency checker, see if an implicit free list is correctly linked and implemented
 *   */


int mm_check(void)
{
    void *heap_start = mem_heap_lo();
    void *heap_end = (char*)mem_heap_hi()+1;
    void *t = HDRP(NEXT_BLKP(heap_listp));
    while (t!=heap_end)
    {
        int *header = (int *)t;
        int size = (*header)&(~0x1);
/*check that the blocks are correctly aligned*/
        if (size %8 != 0)
        {
           printf("not multiple of 8!");
           return 1;
        }
/*check that no free blocks are contiguous*/
	if (GET_ALLOC(HDRP(t))==1 && GET_ALLOC(HDRP(NEXT_BLKP(t)))==1)
	{
	   printf("Two consecutive free blocks!");
           return 1;
        }
/*check that pointers in heap block are valid heap addresses.*/
	if (t<heap_start||t>heap_end)
        {
           printf("Invalid heap addresses");
           return 1;
        }
/*check for no overlapping between blocks*/
	if (t == HDRP(NEXT_BLKP(t)))
	{
	   printf("overlapped blocks!");
           return 1;
        }
        t = HDRP(NEXT_BLKP(t));
    }
    return 0;
}

/* mm_coalesce - coalesce freed blocks */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        {return bp;}

    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
/*fix the next pointer to prevent it from being erased by coalesce*/
    if ((char *)previous > (char *)bp && previous < NEXT_BLKP(bp))
	previous = bp;

    return bp;
}

/* extend_heap - extends the heap */
static void *extend_heap (size_t words)
{
    char *bp;
    size_t size;
    
    /* allocate an even number of words to maintain alignment */
    size = (words %2) ? (words +1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
	{return NULL;}
    /*Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /*free block header*/
    PUT(FTRP(bp), PACK(size, 0)); /*free block footer*/
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /*epilogue header*/

    /* Coalesce if the previous block was free*/
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
//initialize an unused block to satisfy the alignment requirement
    /*CREATE THE INITIAL EMPTY HEAP*/
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
	return -1;
    PUT(heap_listp, 0);  /*First word unused for alignment*/
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /*Prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /*Prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /*Epilogue header*/
    

    heap_listp+=(2*WSIZE);
    
    previous = heap_listp;
/* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
	return -1;
    return 0;
}

/*
 * mm_findfit - Search the implicit free list for a first fit
 * if no fit can be found (i.e.the end of heap is reached without a fit), return -1
 */
static void *find_fit(size_t asize)
{
   /*first-fit search*/
   void *bp;
   
   for (bp = previous; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))
   {
	if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp))))
	{
		previous = NEXT_BLKP(bp);
		return bp;
        }
   }
/*wrap around when next_fit pointer reaches the end of the heap*/
   for (bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))
   {
        if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp))))
        {
                previous = NEXT_BLKP(bp);
                return bp;
        }
   }
   return NULL;

}

static void place (void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
/*to split the blocks if the size is greater than what is required*/    
    if ((csize - asize)>=(2*DSIZE))
    {
	PUT(HDRP(bp), PACK(asize,1));
	PUT(FTRP(bp), PACK(asize,1));
	bp = NEXT_BLKP(bp);
	PUT(HDRP(bp), PACK(csize-asize,0));
	PUT(FTRP(bp), PACK(csize-asize,0));
    }

    else
    {
	PUT(HDRP(bp), PACK(csize,1));
	PUT(FTRP(bp), PACK(csize,1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /*adjusted block size*/
    size_t extendsize; /*amount to extend heap if no fit */
    char *bp;
    
    /*ignore spurious requests*/
    if (size == 0)
	return NULL;

    /*ADJUST BLOCK SIZE TO INCLUDE OVERHEAD AND ALIGNMENT REQS. */
    if (size <= DSIZE)
	asize = 2*DSIZE;
    else
	asize = DSIZE * ((size + (DSIZE)+ (DSIZE-1))/DSIZE);

    /*Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
	place(bp,asize);
	return bp;
    }

    /*no fit found get more memory and place the block*/
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	return NULL;
    place(bp, asize);
    return bp;
}


/*
 * mm_free - Freeing a block changes the alloctated bit to status 0
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{

  if (ptr == NULL)
    return mm_malloc(size);
  else if (size == 0)
  {
    mm_free(ptr);
    return (void *)-1;
  }
  else
  {
    size_t newsize = ALIGN(size + SIZE_T_SIZE*2);
    void *oldptr = ptr;
    void *newptr;
    int copySize;
    int totalSize = GET_SIZE(HDRP(oldptr)) + GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
/*if the next block is free and size is enough*/
    if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (newsize <= totalSize))
    { 
	PUT(HDRP(oldptr), PACK(totalSize, 1));
	PUT(FTRP(oldptr), PACK(totalSize, 1));
	place(oldptr, newsize);
	return oldptr;	
    }
/*if the next block is free and size is not enough but it's the epilogue block*/
    else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (newsize > totalSize) && GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(oldptr))))==0)
    {
	int newWords = newsize/WSIZE;
	int totalWords = totalSize/WSIZE;
	int adding = newWords - totalWords;
	void * addedBlock = extend_heap(adding);
	int addedSize = GET_SIZE(HDRP(addedBlock));
	int totalSize1 = GET_SIZE(HDRP(oldptr)) + addedSize;
	PUT(HDRP(oldptr), PACK(totalSize1, 1));
        PUT(FTRP(oldptr), PACK(totalSize1, 1));
	return oldptr;
    }
/*if the next block is the epilogue block*/
    else if (GET_SIZE(HDRP(NEXT_BLKP(oldptr)))==0)
    {
	int newWords = newsize/WSIZE;
	int oldWords = (GET_SIZE(HDRP(oldptr)))/WSIZE;
	int adding = newWords - oldWords;
	void * addedBlock = extend_heap(adding);
        int addedSize = GET_SIZE(HDRP(addedBlock));
        int totalSize1 = GET_SIZE(HDRP(oldptr)) + addedSize;
        PUT(HDRP(oldptr), PACK(totalSize1, 1));
        PUT(FTRP(oldptr), PACK(totalSize1, 1));
        return oldptr;
    }
    else
    {
    newptr = mm_malloc(newsize);
    if (newptr == NULL)
      return NULL;

    copySize = (*(int *)((char *)oldptr - SIZE_T_SIZE))-(2*SIZE_T_SIZE +1);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
    }
  }
}



























