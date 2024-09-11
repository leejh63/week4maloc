/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "12222",
    /* First member's email address */
    "2",
    /* Second member's full name (leave blank if none) */
    "3",
    /* Second member's email address (leave blank if none) */
    "4"
};

/*explicit code*/

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4             
#define DSIZE 8             
#define CHUNKSIZE (1 << 12) 

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))
static char *heap_listp;
static char *start_buddy;

void remove_in_freelist(void *bp);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static int buddy_index(size_t asize);

/* for explicit*/ 
// 이중 포인터 미사용 
#define ADBL(bp) (*(void**)(bp))
#define HD(bp) (*(void**)(bp))

#define SUCC(bp) (*(void**)(bp+WSIZE))
#define BP

/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(28 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                                
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1));

    for (int i = 2; i <= 24; i++)
    {
        PUT(heap_listp + (i * WSIZE), NULL); 
    }

    PUT(heap_listp + (25 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (26 * WSIZE), 0); 
    PUT(heap_listp + (27 * WSIZE), PACK(0, 1));         
    heap_listp += (2 * WSIZE);

    int extendsize = (1 << 23);
    if ((start_buddy = extend_heap( extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    
    int ind = buddy_index(extendsize);
    void* block_pointer = heap_listp + ((ind) * WSIZE);
    PUT(start_buddy, PACK(extendsize, 0));
    SUCC(start_buddy) = NULL;
    HD(block_pointer) = start_buddy;

    return 0;
}

static int buddy_index(size_t size){
    int n = 0;
    unsigned int power = 1;

    while (power < size) {
        power <<= 1;
        n += 1;
    }

    return n;
}

void putbuddylist(void *bp, size_t size)
{
    int ind = buddy_index(size);
    void* block_pointer = heap_listp + ((ind) * WSIZE);
    HD(bp) = size;
    SUCC(bp) = HD(block_pointer);
    HD(block_pointer) = bp;
}

void *mm_malloc(size_t size)
{
    void* bp;
    int ind = buddy_index(size);
    printf("%d", ind);
    if (size == 0)
        return NULL;
    

    return bp;
}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    

    return bp;

}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

}

static void *find_fit(size_t asize) //
{

    void* block_pointer;
    int n = buddy_index(asize);

    block_pointer = heap_listp + ((n+2) * WSIZE);

    if (block_pointer == NULL){
        return NULL;
    }
    
    return block_pointer; 

}
/////////////////////////

void remove_in_freelist(void *bp)
{
    ADBL(bp) = ADBL(ADBL(bp));
}
//-------------


void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    copySize = *(size_t *)((char *)oldptr - WSIZE);

    if (size < copySize)
      copySize = size;
    
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}