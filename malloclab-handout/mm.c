/*
 * mm-naive.c - The least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by allocating a
 * new page as needed.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused.
 *
 * The heap check and free check always succeeds, because the
 * allocator doesn't depend on any of the old data.
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

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

//header and footer macros
#define GET_SIZE(p) ((block_header *)(p))->size
#define GET_ALLOC(p) ((block_header *)(p))->allocated
#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))
//page macros
#define ADDRESS_PAGE_START(p) ((void *)(((size_t)p) & ~(mem_pagesize()-1)))
#define GET_NEXT_PAGE_PTR(ptr) ((pageHeaderBlock *)(ptr))->nextPage
#define GET_PREV_PAGE_PTR(ptr) ((pageHeaderBlock *)(ptr))->prevPage
#define GET_NEXT_PAGE_VAL(ptr) (*(((pageHeaderBlock *)(ptr))->nextPage))
#define GET_PREV_PAGE_VAL(ptr) (*(((pageHeaderBlock *)(ptr))->prevPage))
//free list macros
#define GET_NEXT_FREE_PTR(ptr) ((freePointerBlock *)(ptr))->nextFree
#define GET_PREV_FREE_PTR(ptr) ((freePointerBlock *)(ptr))->prevFree


void *firstPage = NULL; //points to first allocated page, right at start of pageHeaderBlock at front of page
void *current_avail = NULL; //this is free list pointer
void *freeListPtr = NULL;
int current_avail_size = 0; //total amount of free memory
int biggestFreeSize = 0; //current biggest free block of memory
int numPagesToAllocate = 1;

typedef struct 
{
	size_t size;
	char allocated;
} block_header;

typedef struct 
{
	size_t size;
	int filler;
} block_footer;

typedef struct
{
	void* nextPage;
	void* prevPage;
} pageHeaderBlock;

typedef struct
{
	void* nextFree;
	void* prevFree;
} freePointerBlock;

static void *coalesce(void *bp);
static void set_allocated(void *bp, size_t size);
static void extend(size_t new_size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	mem_reset();
	mem_init();
	firstPage = NULL;
	current_avail = NULL; //this is free list pointer
	freeListPtr = NULL;
	current_avail_size = 0;
	biggestFreeSize = 0;
	numPagesToAllocate = 1;

	size_t amount = PAGE_ALIGN(mem_pagesize());
	amount *= numPagesToAllocate;
	firstPage = mem_map(amount);
	if(firstPage == NULL)
	{
		return -1;
	}
	numPagesToAllocate *= 2;
	GET_NEXT_PAGE_PTR(firstPage) = NULL;
	GET_PREV_PAGE_PTR(firstPage) = NULL; //this is incorrect currently, put into struct. MACROS and functions? it might be right now?
	
	freeListPtr = firstPage + 2*ALIGNMENT; //Points to first payload block that is free
	GET_NEXT_FREE_PTR(freeListPtr) = NULL;
	GET_PREV_FREE_PTR(freeListPtr) = NULL;

	current_avail = firstPage + 2*ALIGNMENT; //skip over first 16 bytes for room of the pointers for page locations and header bytes to get to the start of the first payload
	current_avail_size = amount - 2*ALIGNMENT; //can't include bytes needed for blank first buffer space, null terminator, or pointers to new pages
	biggestFreeSize = current_avail_size;
	//set free space header and footer blocks
	GET_SIZE(HDRP(current_avail)) = current_avail_size;
	GET_ALLOC(HDRP(current_avail)) = 0;
	GET_SIZE(FTRP(current_avail)) = current_avail_size;
	//set terminator
	GET_SIZE(HDRP(NEXT_BLKP(current_avail))) = 0;
	GET_ALLOC(HDRP(NEXT_BLKP(current_avail))) = 1;
	//malloc the prologue block
	mm_malloc(0);
	
  	return 0;
}

/* 
 * mm_malloc - Allocate a block by using bytes from current_avail,
 *     grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{
	int newsize = ALIGN(size + OVERHEAD);
	printf("Just made newsize %d\n", newsize);
	if (current_avail_size < newsize || biggestFreeSize < newsize) //change this to extend which will need to give us more memory 
	{
		printf("Current availale size %d was less than new size %d\n", current_avail_size, newsize);
		extend(newsize);
		
		if (current_avail == NULL)
		{
			return NULL;
		}
	}

	void *p = freeListPtr;
	printf("Set p to current avail %p\n", p);
	printf("Size of first block is: %d\n", GET_SIZE(HDRP(p)));

	//this is first fit find of unallocated block with implicit free list rather than explicit
	while (GET_SIZE(HDRP(p)) < newSize && GET_NEXT_FREE_PTR(p) != NULL) 
	{
		p = GET_NEXT_FREE_PTR(p);
/*
		printf("Current payload pointer looking at has alloc of: %d\n", GET_ALLOC(HDRP(p)));
		if (!GET_ALLOC(HDRP(p)) && (GET_SIZE(HDRP(p)) >= newsize)) 
		{
			printf("Found something to allocate\n");
			set_allocated(p, newsize);
			current_avail += newsize;
			current_avail_size -= newsize;
			biggestFreeSize = findBiggestBlockAvailable();
			return p;
		}
		p = NEXT_BLKP(p);
*/
	}
	
	set_allocated(p, newSize);
	
	//p = current_avail;
	//printf("Found nothing.\n");

	return p;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	printf("Calling free on %p\n", ptr);
	if(mm_can_free(ptr)) //also maybe check mm_check returning 1
	{

	}
}

/*
 * mm_check - Check whether the heap is ok, so that mm_malloc()
 *            and proper mm_free() calls won't crash.
 */
int mm_check()
{
  return 1;
}

/*
 * mm_check - Check whether freeing the given `p`, which means that
 *            calling mm_free(p) leaves the heap in an ok state.
 */
int mm_can_free(void *p)
{
  return 1;
}

static void *coalesce(void *bp) 
{
	size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	if (prev_alloc && next_alloc) 
	{ /* Case 1 */
		/* nothing to do */
	}
	else if (prev_alloc && !next_alloc) 
	{ /* Case 2 */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		GET_SIZE(HDRP(bp)) = size;
		GET_SIZE(FTRP(bp)) = size;
	}
	else if (!prev_alloc && next_alloc) 
	{ /* Case 3 */
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		GET_SIZE(FTRP(bp)) = size;
		GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
		bp = PREV_BLKP(bp);
	}
	else 
	{ /* Case 4 */
		size += (GET_SIZE(HDRP(PREV_BLKP(bp)))
		+ GET_SIZE(HDRP(NEXT_BLKP(bp))));
		GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
		GET_SIZE(FTRP(NEXT_BLKP(bp))) = size;
		bp = PREV_BLKP(bp);
	}
	return bp;
}

/*
* Asks for more memory from operating system to increase heap for allocating when there isn't
* enough free space to mm_alloc requesting a too big payload.
*/
static void extend(size_t new_size) 
{
	size_t chunk_size = PAGE_ALIGN(new_size);
	chunk_size *= numPagesToAllocate;
	void* bp = mem_map(chunk_size);
	if(bp == NULL)
	{
		return;
	}
	numPagesToAllocate *= 2;
	void* pagePtr = firstPage;
	while(GET_NEXT_PAGE_PTR(pagePtr) != NULL)
	{
		pagePtr = GET_NEXT_PAGE_PTR(pagePtr);
	}
	GET_NEXT_PAGE_PTR(pagePtr) = bp;
	GET_NEXT_PAGE_PTR(bp) = NULL;
	GET_PREV_PAGE_PTR(bp) = pagePtr;

	chunk_size -= (2*ALIGNMENT); // account for unusable space in newly requested page
	//buffer space to start so that payloads are 16 byte aligned, plus step over bytes for page pointers and over header block bytes
	bp += 2*ALIGNMENT;
	GET_SIZE(HDRP(bp)) = chunk_size;
	GET_SIZE(FTRP(bp)) = chunk_size;
	GET_ALLOC(HDRP(bp)) = 0;
	GET_SIZE(HDRP(NEXT_BLKP(bp))) = 0;
	GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 1;


	current_avail_size += chunk_size;
	biggestFreeSize = chunk_size;	
	current_avail = bp; // this not right

	mm_malloc(0);
}

/*
* Set the size of the header and footer for the malloced block and set the free size of the
* remaining memory if there is any. Set the correct allocation bits.
*/
static void set_allocated(void *bp, size_t size) 
{
	size_t extra_size = GET_SIZE(HDRP(bp)) - size;
	//when free block is split
	if (extra_size > ALIGN(1 + OVERHEAD)) 
	{
		GET_SIZE(HDRP(bp)) = size;
		GET_SIZE(FTRP(bp)) = size;
		GET_SIZE(HDRP(NEXT_BLKP(bp))) = extra_size;
		GET_SIZE(FTRP(NEXT_BLKP(bp))) = extra_size;
		GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 0;

		GET_NEXT_FREE_PTR(NEXT_BLKP(bp)) = GET_NEXT_FREE_PTR(bp);
		GET_PREV_FREE_PTR(GET_NEXT_FREE_PTR(bp)) = NEXT_BLKP(bp);
		GET_PREV_FREE_PTR(NEXT_BLKP(bp)) = GET_PREV_FREE_PTR(bp);
		GET_NEXT_FREE_PTR(GET_PREV_FREE_PTR(bp)) = NEXT_BLKP(bp);	
		
	}
	else //free block not split
	{
		GET_NEXT_FREE_PTR(GET_PREV_FREE_PTR(bp)) = GET_NEXT_FREE_PTR(bp);
		GET_PREV_FREE_PTR(GET_NEXT_FREE_PTR(bp)) = GET_PREV_FREE_PTR(bp);
	}
	GET_NEXT_FREE_PTR(bp) = NULL;
	GET_PREV_FREE_PTR(bp) = NULL;
	GET_ALLOC(HDRP(bp)) = 1;

}
