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
static void *coalesce_with_LIFO(void *bp);
void remove_in_freelist(void *bp);
void put_front_of_freelist(void *bp);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/* for explicit*/ 
// 이중 포인터 미사용 
// #define PREV_FREEP(bp) (*(unsigned int*)(bp))
// #define NEXT_FREEP(bp) (*(unsigned int*)(bp + WSIZE))

// 이중 포인터 사용
#define PREV_FREEP(bp) (*(void **)(bp))
#define NEXT_FREEP(bp) (*(void **)(bp + WSIZE))
#define INDEXA(bp) (*(void **)(bp))

// 크기별 리스트 위치 포인터는 heap_listp로 진행


/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */

    if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                                
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1)); 
    // for 문을 통해 특정 값을 받고 인덱스 늘려주기 
    PUT(heap_listp + (2 * WSIZE), NULL);  //  ~32    
    PUT(heap_listp + (3 * WSIZE), NULL);  // 33~64    
    PUT(heap_listp + (4 * WSIZE), NULL);  // 65~128                
    PUT(heap_listp + (5 * WSIZE), NULL);  // 129~inf
    // 일단 고정 인덱스              
    PUT(heap_listp + (6 * WSIZE), PACK(DSIZE * 2, 1)); 
    PUT(heap_listp + (7 * WSIZE), PACK(0, 1));         
    heap_listp += (2 * WSIZE);

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
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp),PACK(size, 0));

    // 아래 코드 넣어주면 에러가 난다.. 이유를 모르겠다..
    // //--임시--
    // PUT(bp, NULL); 
    // PUT(free_listp, bp);
    // PUT(NEXT_FREEP(bp), free_listp);
    // free_listp = bp;
    // //--

    PUT(FTRP(bp),PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0, 1));

    return coalesce_with_LIFO(bp);

}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce_with_LIFO(ptr);
}

static void *coalesce_with_LIFO(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (!prev_alloc && !next_alloc)
    {// case4
        remove_in_freelist(PREV_BLKP(ptr));
        remove_in_freelist(NEXT_BLKP(ptr));

        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) +
                GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    else if (prev_alloc && !next_alloc)
    { // case 2

        remove_in_freelist(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));

    }
    else if (!prev_alloc && next_alloc)
    { // case3
        remove_in_freelist(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }
    put_front_of_freelist(ptr);
    return ptr;
}

void remove_in_freelist(void *bp)
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
//////////////////////////////////////////////////////////////
void put_front_of_freelist(void *bp) // 수정 필요
{   
    int asize = GET_SIZE(HDRP(bp));
    int n = 0;
    unsigned int power = 1;
    
    while (power <= asize) {
        power <<= 1;  // 2의 제곱수를 구하기 위해 비트를 왼쪽으로 이동
        n += 1;      // n 증가
    }
    
    // // n을 사용하여 적절한 범위에 맞춰 주소를 선택
    // void *bp; //= heap_listp + (n * WSIZE); // 간단히 n에 맞춰 가상 블록 포인터 계산
    ////////// 맞는 범위 찾는 함수//////////

    if (asize <= 32) {
        // ~32 범위: heap_listp + (2 * WSIZE)
        free_listp = heap_listp + (2 * WSIZE);

    } else if (asize <= 64) {
        // 33~64 범위: heap_listp + (3 * WSIZE)
        free_listp = heap_listp + (3 * WSIZE);

    } else if (asize <= 128) {
        // 65~128 범위: heap_listp + (4 * WSIZE)
        free_listp = heap_listp + (4 * WSIZE);

    } else {
        // 129~inf 범위: heap_listp + (5 * WSIZE)
        free_listp = heap_listp + (5 * WSIZE);
    }
    /////////////////////////////////////////////////

    NEXT_FREEP(bp) = INDEXA(free_listp);
    PREV_FREEP(bp) = NULL;
    PREV_FREEP(INDEXA(free_listp)) = INDEXA(bp);
    free_listp = bp; 
}


// from chat gpt
static void *find_fit(size_t asize) 
{
    // 주어진 asize와 비교하기 위해 n값을 구함
    int n = 0;
    unsigned int power = 1;
    void *bp;

    while (power <= asize) {
        power <<= 1;  // 2의 제곱수를 구하기 위해 비트를 왼쪽으로 이동
        n += 1;      // n 증가
    }
    
    // // n을 사용하여 적절한 범위에 맞춰 주소를 선택
    void *bp; //= heap_listp + (n * WSIZE); // 간단히 n에 맞춰 가상 블록 포인터 계산
    ////////// 맞는 범위 찾는 함수//////////

    if (asize <= 32) {
        // ~32 범위: heap_listp + (2 * WSIZE)
        bp = heap_listp + (2 * WSIZE);

    } else if (asize <= 64) {
        // 33~64 범위: heap_listp + (3 * WSIZE)
        bp = heap_listp + (3 * WSIZE);

    } else if (asize <= 128) {
        // 65~128 범위: heap_listp + (4 * WSIZE)
        bp = heap_listp + (4 * WSIZE);

    } else {
        // 129~inf 범위: heap_listp + (5 * WSIZE)
        bp = heap_listp + (5 * WSIZE);
    }
    /////////////////////////////////////////////////

    while (bp != NULL)
    {
        // 현재 블록의 사이즈 확인 (헤더에서 가져오기)
        if (asize <= GET_SIZE(HDRP(bp)))
        {
            return bp; // 적합한 블록을 찾으면 반환
        }

        // 다음 블록으로 이동 (실제 구현에 맞게 수정 필요)
        bp = (char *)bp + WSIZE; // 임시로 다음 블록으로 이동
    }

    return NULL;  // 적합한 블록을 찾지 못한 경우 NULL 반환
}
/////////////////////////////////////////////////////////////////////////
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_in_freelist(bp);
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        put_front_of_freelist(bp);
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
    
    copySize = *(size_t *)((char *)oldptr - WSIZE);

    if (size < copySize)
      copySize = size;
    
    memcpy(newptr, oldptr, copySize);

    mm_free(oldptr);

    return newptr;
}