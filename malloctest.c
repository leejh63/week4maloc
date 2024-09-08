#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
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

// 추가
static char* first_free;

#define NEXT_FREE(bp) ((char*)bp + WSIZE) // next의 포인터 /pre의 경우 bp를 사용하면 된다.

//
int mm_init(void)
{   // 주소포인터 선언을 밖에서 해줬기때문에 지워야한다!
    // char* heap_listp;
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));

    // pre/next의 루트
    PUT(heap_listp + (2*WSIZE), NULL); // 임시
    PUT(heap_listp + (3*WSIZE), NULL);
    //----

    PUT(heap_listp + (4*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);
    
    // 가용 리스트 초기 위치 
    first_free = heap_listp;

    // 첫 확장 후 병합 후 확장후의 bp 반환
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;

}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp),PACK(size, 0));

    //--임시--
    // pre-null
    PUT(bp, NULL);
    
    PUT(first_free, bp);

    //#define NEXT_FREE(bp) ((char*)bp + WSIZE)

    PUT(NEXT_FREE(bp), first_free);

    first_free = bp;
    //--

    PUT(FTRP(bp),PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0, 1));

    return coalesce(bp);

}

static void *find_fit(size_t asize)
{   // 기존 코드 - 에러발생
    // void *bp = mem_heap_lo() + 2 * WSIZE; // 첫번째 블록(주소: 힙의 첫 부분 + 8bytes)부터 탐색 시작
    // static char *heap_listp; 위에 전역변수 선언
    void* bp = first_free;
    while (GET(NEXT_FREE(bp)) != NULL)
    {
        if (asize <= GET_SIZE(HDRP(bp)))
            return bp;                       
        bp = GET(NEXT_FREE(bp));        
    }
    return NULL; // extend_heap?
}

static void place(void *bp, size_t asize)
{   //-
    char* pre_free = GET(bp);
    char* next_free = GET(NEXT_FREE(bp));
    //-

    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 크기

    //--
    PUT(NEXT_FREE(pre_free), next_free);
    PUT(next_free, pre_free);
    // --

    if ((csize - asize) >= (2 * DSIZE)) // 차이가 최소 블록 크기 16보다 같거나 크면 분할
    {
        PUT(HDRP(bp), PACK(asize, 1)); // 현재 블록에는 필요한 만큼만 할당
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0)); // 남은 크기를 다음 블록에 할당(가용 블록)
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));

        //첫위치 삽입
        PUT(NEXT_BLKP(bp), NULL);
        PUT(NEXT_FREE(NEXT_BLKP(bp)),first_free);
        PUT(first_free, NEXT_BLKP(bp));
        first_free = NEXT_BLKP(bp);
        //--
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 해당 블록 전부 사용
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// coalesce() 결합 로직
static void* coalesce(void* bp)
{
    char* pre_free;
    char* next_free;
    //bp 이전 블록 할당여부
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));

    //bp 다음 블록 할당여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    
    //bp 사이즈 정보
    size_t size = GET_SIZE(HDRP(bp));
    // 1
    if (prev_alloc && next_alloc){ //이전 다음 블럭 할당
        //--
        PUT(bp, NULL);
        PUT(NEXT_FREE(bp), first_free);
        PUT(first_free, bp);
        first_free = bp;
        //--
        return bp;

    } // 2
    else if (prev_alloc && !next_alloc){ // 이전블럭 할당, 다음블럭 미할당
        // --
        pre_free = GET(NEXT_BLKP(bp));
        next_free = GET(NEXT_FREE(NEXT_BLKP(bp)));
        
        PUT(next_free, pre_free);
        PUT(NEXT_FREE(pre_free), next_free);
        // --

        //-- 새로운 가용블록 리스트 삽입
        PUT(bp, NULL);
        PUT(NEXT_FREE(bp), first_free);
        PUT(first_free, bp);
        first_free = bp;
        //--

        //bp 사이즈 + 다음 블럭 사이즈
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // bp 헤더에 사이즈/ 미할당 갱신
        PUT(HDRP(bp), PACK(size, 0));
        // bp 헤더가 이미 갱신됨 > bp의 풋터가 다음 블럭의 풋터
        PUT(FTRP(bp), PACK(size, 0));


    } // 3
    else if (!prev_alloc && next_alloc){ // 이전블럭 미할당, 다음블럭 할당

        // --
        pre_free = GET(PREV_BLKP(bp));
        next_free = GET(NEXT_FREE(PREV_BLKP(bp)));
        
        PUT(next_free, pre_free);
        PUT(NEXT_FREE(pre_free), next_free);
        // --
        
        //-- 새로운 가용블록 리스트 삽입
        PUT(PREV_BLKP(bp), NULL);
        PUT(NEXT_FREE(PREV_BLKP(bp)), first_free);
        PUT(first_free, PREV_BLKP(bp));
        first_free = PREV_BLKP(bp);
        //--

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
        // --
        pre_free = GET(NEXT_BLKP(bp));
        next_free = GET(NEXT_FREE(NEXT_BLKP(bp)));
        
        PUT(next_free, pre_free);
        PUT(NEXT_FREE(pre_free), next_free);
        // --

        // --
        pre_free = GET(PREV_BLKP(bp));
        next_free = GET(NEXT_FREE(PREV_BLKP(bp)));
        
        PUT(next_free, pre_free);
        PUT(NEXT_FREE(pre_free), next_free);
        // --
        
        //-- 새로운 가용블록 리스트 삽입
        PUT(PREV_BLKP(bp), NULL);
        PUT(NEXT_FREE(PREV_BLKP(bp)), first_free);
        PUT(first_free, PREV_BLKP(bp));
        first_free = PREV_BLKP(bp);
        //--


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