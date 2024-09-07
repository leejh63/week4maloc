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
    "team 8",
    /* First member's full name */
    "D_cron",
    /* First member's email address */
    "dcron@jungle.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
// Basic constants and macros
#define WSIZE 4 // 워드 = 헤더 = 풋터 사이즈(bytes)
#define DSIZE 8 // 더블워드 사이즈(bytes)
#define CHUNKSIZE (1<<12) // heap을 이정도 늘린다(bytes)

#define MAX(x, y) ((x) > (y)? (x):(y))

// pack a size and allocated bit into a word 
#define PACK(size, alloc) ((size) | (alloc))

// Read and wirte a word at address p
//p는 (void*)포인터이며, 이것은 직접 역참조할 수 없다.
#define GET(p)     (*(unsigned int *)(p)) //p가 가리키는 놈의 값을 가져온다
#define PUT(p,val) (*(unsigned int *)(p) = (val)) //p가 가리키는 포인터에 val을 넣는다

// Read the size and allocated fields from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7) // ~0x00000111 -> 0x11111000(얘와 and연산하면 size나옴)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// Given block ptr bp, compute address of next and previous blocks
// 현재 bp에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
// 그럼 현재 블록 크기를 return하고(헤더+데이터+풋터), 그걸 현재 bp에 더하면 next_bp나옴
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
/* 
 * mm_init - initialize the malloc package.
 */
static char *heap_listp; //prolouge사이를 가리키는 놈
int mm_init(void)
{
    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;                              // 불러올 수 없으면 -1 return  
    PUT(heap_listp, 0);                         // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // P.H 8(크기)/1(할당됨)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // P.F 8/1
    PUT(heap_listp + (3*WSIZE), PACK(0,1));     // E.H(헤더로만 구성) 0/1
    heap_listp += (2*WSIZE); // 처음에 항상 prolouge 사이를 가리킴
    // 나중에 find_fit 함수에서 find할 때 사용됨
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) //word가 몇개인지 확인해서 넣으려고
        return -1;
    return 0;
}
//연결
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    // case 1
    if (prev_alloc && next_alloc){
        return bp;
    }
    // case 2
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));//header가 바뀌었으니까 size도 바뀐다!
    }
    // case 3
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); //bp를 prev로 옮겨줌
    }
    // case 4
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); //bp를 prev로 옮겨줌
    }
    return bp;
}
// heap을 CHUNKSIZE byte로 확장하고 초기 가용 블록을 생성한다.
// 여기까지 진행되면 할당기는 초기화를 완료하고, application으로부터
// 할당과 반환 요청을 받을 준비를 완료하였다.
// extend_heap은 두 가지 경우에 호출된다.
// 1. heap이 초기화될 때
// 2. mm_malloc이 적당한 맞춤(fit)을 찾지 못했을 때
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) //size를 불러올 수 없으면
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size,0)); // Free block header(bp에서 -word로 header자리 가서 에필로그 자리에 넣게 된다.)
    PUT(FTRP(bp), PACK(size,0)); // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // New epilogue header

    // Coalesce(연결후 합침) if the previous block was free
    return coalesce(bp);
}
// find_fit함수, frist-fit으로 구현
static void *find_fit(size_t asize){
    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL; // No fit
}
//place 함수
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1));//현재 크기를 헤더에 집어넣고
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else{
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
    size_t asize; //할당할 블록 사이즈
    size_t extendsize;
    char *bp;

    // Ignore spurious requests - size가 0이면 할당x
    if(size <= 0) // == 대신 <=
        return NULL;

    // Adjust block size to include overhead and alignment reqs.
    if(size <= DSIZE) // size가 8byte보다 작다면,
        asize = 2*DSIZE; // 최소블록조건인 16byte로 맞춤
    else              // size가 8byte보다 크다면
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1)) / DSIZE);

    //Search the free list for a fit - 적절한 가용(free)블록을 가용리스트에서 검색
    if((bp = find_fit(asize))!=NULL){
        place(bp,asize); //가능하면 초과부분 분할
        return bp;
    }

    //No fit found -> Get more memory and place the block
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);    
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;

//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//       return NULL;
//     // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     copySize = GET_SIZE(HDRP(oldptr));
//     if (size < copySize)
//       copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }

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



