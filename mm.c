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
// 테스트입니다.아닙니다.
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
////////// 기본 상수 및 매크로 ////////
#define WSIZE 4
#define DSIZE 8
// 일반적으로 CHUNKSIZE 는 4096바이트 설정 2의 12승
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) // 비트연산을 통해 값을 합쳐준다.

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

static char *heap_listp;
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
///////////////////////////////////////////////////////////////////////////////////////

// coalesce() 결합 로직
static void* coalesce(void* bp)
{
    //bp 이전 블록 할당여부
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));

    //bp 다음 블록 할당여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    
    //bp 사이즈 정보
    size_t size = GET_SIZE(HDRP(bp));
    // 1
    if (prev_alloc && next_alloc){ //이전 다음 블럭 할당
        return bp;

    } // 2
    else if (prev_alloc && !next_alloc){ // 이전블럭 할당, 다음블럭 미할당
        //bp 사이즈 + 다음 블럭 사이즈
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // bp 헤더에 사이즈/ 미할당 갱신
        PUT(HDRP(bp), PACK(size, 0));
        // bp 헤더가 이미 갱신됨 > bp의 풋터가 다음 블럭의 풋터
        PUT(FTRP(bp), PACK(size, 0));

    } // 3
    else if (!prev_alloc && next_alloc){ // 이전블럭 미할당, 다음블럭 할당
        //bp 사이즈 + 다음 블럭 사이즈
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // bp 풋터에 우선 사이즈/ 미할당 갱신
        PUT(FTRP(bp), PACK(size, 0));
        // bp 이전 블럭 헤더 사이즈/ 미할당 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 새로운 bp 위치 갱신
        bp = PREV_BLKP(bp);

    }
    else{// 4  이전, 다음 블럭 미할당
        //bp 사이즈 + 다음 블럭 사이즈 + 이전 블럭 사이즈
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // bp이전 헤더 사이즈/ 미할당 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // bp다음 풋터 사이즈/ 미할당 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 새로운 bp 위치 갱신
        bp = PREV_BLKP(bp);                    

    }

    return bp;
}

// extend_heap() 힙영역 확장후 결합
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp),PACK(size, 0));
    PUT(FTRP(bp),PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0, 1));

    return coalesce(bp);

}

// 
static void *find_fit(size_t asize)
{   // 기존 코드 - 에러발생
    // void *bp = mem_heap_lo() + 2 * WSIZE; // 첫번째 블록(주소: 힙의 첫 부분 + 8bytes)부터 탐색 시작
    // static char *heap_listp; 위에 전역변수 선언
    void*bp = heap_listp;
    while (GET_SIZE(HDRP(bp)) > 0)
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) // 가용 상태이고, 사이즈가 적합하면
            return bp;                                             // 해당 블록 포인터 리턴
        bp = NEXT_BLKP(bp);                                        // 조건에 맞지 않으면 다음 블록으로 이동해서 탐색을 이어감
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 크기

    if ((csize - asize) >= (2 * DSIZE)) // 차이가 최소 블록 크기 16보다 같거나 크면 분할
    {
        PUT(HDRP(bp), PACK(asize, 1)); // 현재 블록에는 필요한 만큼만 할당
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0)); // 남은 크기를 다음 블록에 할당(가용 블록)
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 해당 블록 전부 사용
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   // 주소포인터 선언을 밖에서 해줬기때문에 지워야한다!
    // char* heap_listp;
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
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

    // 예외처리
    if (size <= 0)
        return NULL;

    if (size <= DSIZE)     // 헤더, 푸터 8바이트 차지
        asize = 2 * DSIZE; // 
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); 
        // 정수의 나눗셈은 내림처리를 해준다. 
        // size에 정렬이 부족한 바이트를 DSIZE - 1가 채워주는 느낌
        // 다시 DSIZE를 곱해주면 알맞은 값을 구할 수 있다.

    /* 가용 블록 검색 */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); // 할당
        return bp;        // 새로 할당된 블록의 포인터 리턴
    }

    /* 적합한 블록이 없을 경우 힙확장 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)
{
        /* 예외 처리 */
    if (ptr == NULL) // 포인터가 NULL인 경우 할당만 수행
        return mm_malloc(size);

    if (size <= 0) // size가 0인 경우 메모리 반환만 수행
    {
        mm_free(ptr);
        return 0;
    }

        /* 새 블록에 할당 */
    void *newptr = mm_malloc(size); // 새로 할당한 블록의 포인터
    if (newptr == NULL)
        return NULL; // 할당 실패

        /* 데이터 복사 */
    size_t copySize = GET_SIZE(HDRP(ptr));         // payload만큼 복사
    if (size < copySize)                           // 기존 사이즈가 새 크기보다 더 크면
        copySize = size;                           // size로 크기 변경 (기존 메모리 블록보다 작은 크기에 할당하면, 일부 데이터만 복사)

    memcpy(newptr, ptr, copySize); // 새 블록으로 데이터 복사

        /* 기존 블록 반환 */
    mm_free(ptr);

    return newptr;
}

// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;
//     newptr = mm_malloc(size);

//     if (newptr == NULL)
//       return NULL;
//     // 아래 작동안됨
//     // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     // 현 bp의 헤더를 찾을려면 4바이트 만큼 이동해야하지만 위 코드는 8바이트만큼 이동함 
//     copySize = *(size_t *)((char *)oldptr - WSIZE);

//     if (size < copySize)
//       copySize = size;
      
//     memcpy(newptr, oldptr, copySize);

//     mm_free(oldptr);

//     return newptr;
// }














