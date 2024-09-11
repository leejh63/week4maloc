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
void removefreelist(void *bp);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static int free_pos(size_t asize);

#define ADBL(bp) (*(void**)(bp))

/*///////////////////////////////////////////////////////////////////
안쓰는 상수 및 매크로 정리안함
현재 free 함수 미구현, 추가적인 함수 구현 후 및 에러 확인 필요
일부분 통과 확인
*////////////////////////////////////////////////////////////////////

/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */

    if ((heap_listp = mem_sbrk(16 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1));

    // 범위는 16~32, 32~64, 64~128... 이런식으로 지정
    for (int i = 2; i <= 12; i++)
    {
        PUT(heap_listp + (i * WSIZE), NULL);
    }
    PUT(heap_listp + (13 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (14 * WSIZE), 0);
    PUT(heap_listp + (15 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    void *bpp;

    if (size == 0){
        return NULL;
    }

    if (size <= DSIZE){
        asize = 2 * DSIZE;
    } 
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bpp = find_fit(asize)) != NULL){
        bp = ADBL(bpp);
    }

    // 범위가 2^n 단위 따라서 비트 연산가능
    extendsize = 1 << (free_pos(asize));
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    return bp;
}

static void* extend_heap(size_t words){
    char* bp;
    size_t size;
    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    return bp;
}

void mm_free(void *ptr){ 
    // free를 진행하면서 받은 가용사이즈에 맞게 분할하는 로직을 생각함.
    // 구현은 패스
    size_t size = GET_SIZE(HDRP(ptr));
}

static int free_pos(size_t asize){
    int n = 0;
    unsigned int power = 1;

    while (power < asize) {
        power <<= 1;
        n += 1;
        // free 함수 구현 후 아래 조건 적용
        // if (n >= 12)
        //     break;
    }
    return n;
}

static void *find_fit(size_t asize)
{
    void* block_pointer;
    int n = free_pos(asize);
    block_pointer = heap_listp + ((n) * WSIZE);

    if (block_pointer == NULL){
        return NULL;
    }
    return block_pointer;
}

void removefreelist(void *bp){
    ADBL(bp) = ADBL(ADBL(bp));
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