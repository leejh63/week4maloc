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
static char *free_listp;
static void *coalesce(void *bp);
void removefreelist(void *bp);
void putfreelist(void *bp);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/* for explicit*/ 
// 이중 포인터 미사용 
#define PREV_FREEP(bp) (*(unsigned int*)(bp))
#define NEXT_FREEP(bp) (*(unsigned int*)(bp + WSIZE))

// 이중 포인터 사용
// #define PREV_FREEP(bp) (*(void **)(bp))
// #define NEXT_FREEP(bp) (*(void **)(bp + WSIZE))

/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */

    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                                
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1)); 
    PUT(heap_listp + (2 * WSIZE), NULL);               
    PUT(heap_listp + (3 * WSIZE), NULL);               
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE * 2, 1)); 
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));         
    heap_listp += (2 * WSIZE);
    free_listp = heap_listp;

    if (extend_heap(4) == NULL)
        return -1;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      
    size_t extendsize; 
    char *bp;

    if (size == 0)
        return NULL;


    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); 

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); 
        return bp; 
    }

    extendsize = MAX(asize, CHUNKSIZE);   
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) 
        return NULL;
    place(bp, asize);
    return bp;
}

static void* extend_heap(size_t words)
{
    void* bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp),PACK(size, 0));
    PUT(FTRP(bp),PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0, 1));

    return coalesce(bp);

}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc && !next_alloc)
    {// case4
        removefreelist(PREV_BLKP(bp));
        removefreelist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if (prev_alloc && !next_alloc)
    { // case 2

        removefreelist(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    }
    else if (!prev_alloc && next_alloc)
    { // case3
        removefreelist(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    putfreelist(bp);
    return bp;
}

void removefreelist(void *bp)
{
    if (bp == free_listp)
    {
        PREV_FREEP(NEXT_FREEP(bp)) = NULL;
        free_listp = NEXT_FREEP(bp);
    }
    else
    {
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
        PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
    }
}

void putfreelist(void *bp)
{
    NEXT_FREEP(bp) = free_listp;
    PREV_FREEP(bp) = NULL;
    PREV_FREEP(free_listp) = bp;
    free_listp = bp; //
}

static void *find_fit(size_t asize)
{   
    void*bp = free_listp;

    // while 문 사용
    while (GET_ALLOC(HDRP(bp)) != 1)
    {
        if (asize <= GET_SIZE(HDRP(bp))) 
            return bp;                                            
        bp = NEXT_FREEP(bp);                                        
    }

    // for문 사용
    // for (void *bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = NEXT_FREEP(bp))
    // {
    //     if (asize <= GET_SIZE(HDRP(bp)))
    //     {
    //         return bp;
    //     }
    // }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    removefreelist(bp);
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        putfreelist(bp);
    }
    else // 
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    copySize = *(size_t *)((char *)oldptr - WSIZE); // DSIZE 아님

    if (size < copySize)
      copySize = size;
    
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}