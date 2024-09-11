/*
/// 버디 시스템 로직-- 수정중 --////

- 블럭 사이즈 
사이즈의 경우 주소값을 통해서 구할 수 있다. 이외에도 다른 방법 존재할 것 같다.

- 연결 함수
기존 가용리스트 처음위치에 연결하는 함수와 큰 차이 없음

- 갱신 함수
기존 갱신방법과 큰 차이 없음

- 버디 주소 찾기 함수(블럭의 주소, 블럭의 사이즈)
만약 사이즈*2 % 주소 == 0 이면 
    (주소 + 사이즈)
아니면
    (주소 - 사이즈)
찾은 버디의 주소를 반환해준다.

1. 초기
현재 max 20MB 이므로 
max_heap_size = (1 << 23) + wsize*(1 + 1 + 24 + 1 + 1 \)
- 프롤로그헤더, 풋터, 힙 시작, 끝, 사이즈 클래스 총 24개 (2^0 ~ 2^23)
확장 후 23클래스에 가용 블럭과 next 포인터를 통해 연결  
가용블럭 헤더생성 후 헤더에 사이즈 및 할당여부 갱신(갱신은 함수로)

2. 할당 진행
특정 사이즈 n을 받았다고 가정한다면, (n의 최대치는 이미 정해져있음)

사이즈 n+1을 해준 뒤 진행

일단 사이즈 클래스 탐색 진행
만약 적절한 사이즈 클래스에 가용블럭 발견시 할당진행

없다면 반복 진행, 
초기 "n의 크기와 가용 블럭 사이즈 비교 > n이 작다면 블럭을 1/2로 분할"
분할시 오른쪽 가용블럭의 헤더의 포인터를 통해 사이즈에 맞는 클래스에 연결(연결은 함수로)
"왼쪽 가용블럭과 n의 크기 비교 > n이 작다면 블럭을 1/2로 크기 분할"
.... 계속 반복진행
진행 중 n이 2^k보다 커진다면 2^(k+1) 블럭할당진행
// 최소사이즈 도달시 그냥 할당?
할당 진행시 할당블럭 헤더에 사이즈 및 할당 여부 갱신 진행(갱신은 함수로)

3. 할당 해제
특정 블럭을 할당 해제 할 경우 병합(병합은 함수로)진행 후 헤더정보 수정 후
맞는 사이즈 클래스와 연결

4. 병합
버디 찾은후(함수로) 찾은 버디의 주소값을 통해 사이즈 확인 현 블럭의 사이즈와 같다면 
병합진행
이후 병합된 블럭의 버디를 찾은 후 병합 여부 확인 > 무한 반복
반복 진행 중 병합이 불가능 하다면 반복을 멈춘다.
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