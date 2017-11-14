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


void *firstPage = NULL;
void *current_avail = NULL; //this is free list pointer
int current_avail_size = 0;

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

static void *coalesce(void *bp);
static void set_allocated(void *bp, size_t size);
static void extend(size_t new_size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	size_t amount = PAGE_ALIGN(mem_pagesize());
	firstPage = mem_map(amount);
	if(firstPage == NULL)
	{
		return -1;
	}
	firstPage += sizeof(block_header); //store value of pointer to next page here somehow
	*firstPage = NULL;
	*(firstPage + 8) = NULL; //this is incorrect currently, put into struct. MACROS and functions?

	current_avail = firstPage + ALIGNMENT + sizeof(block_header); //skip over first 16 bytes for room of the pointers for page locations and header bytes to get to the start of the first payload
	current_avail_size = amount - 2*ALIGNMENT; //can't include bytes needed for blank first buffer space, null terminator, or pointers to new pages
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
	if (current_avail_size < newsize) //change this to extend which will need to give us more memory 
	{
		printf("Current availale size %d was less than new size %d\n", current_avail_size, newsize);
		extend(newsize);
		
		if (current_avail == NULL)
		{
			return NULL;
		}
	}

	void *p = current_avail;
	printf("Set p to current avail %p\n", p);
	printf("Size of first block is: %d\n", GET_SIZE(HDRP(p)));

	//this is first fit find of unallocated block with implicit free list rather than explicit
	while (GET_SIZE(HDRP(p)) != 0) 
	{
		printf("Current payload pointer looking at has alloc of: %d\n", GET_ALLOC(HDRP(p)));
		if (!GET_ALLOC(HDRP(p))
		&& (GET_SIZE(HDRP(p)) >= newsize)) 
		{
			printf("Found something to allocate\n");
			set_allocated(p, newsize);
			current_avail += newsize;
			current_avail_size -= newsize;
			return p;
		}
		p = NEXT_BLKP(p);
	}
	
	//p = current_avail;
	printf("Found nothing.\n");

	return NULL;
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
	void* bp = mem_map(chunk_size);
	void* pagePtr = *firstPage;
	while(pagePtr != NULL)
	{
		pagePtr = GET_NEXT_PAGE();
	}
	chunk_size -= (2*ALIGNMENT); // account for unusable space in newly requested page
	if(bp == NULL)
	{
		return;
	}
	//buffer space to start so that payloads are 16 byte aligned, plus step over bytes for page pointers and over header block bytes
	bp += (2*ALIGNMENT);
	GET_SIZE(HDRP(bp)) = chunk_size;
	GET_SIZE(FTRP(bp)) = chunk_size;
	GET_ALLOC(HDRP(bp)) = 0;
	GET_SIZE(HDRP(NEXT_BLKP(bp))) = 0;
	GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 1;

	mm_malloc(0);

	current_avail_size += chunk_size;
	current_avail = bp;
}

/*
* Set the size of the header and footer for the malloced block and set the free size of the
* remaining memory if there is any. Set the correct allocation bits.
*/
static void set_allocated(void *bp, size_t size) 
{
	size_t extra_size = GET_SIZE(HDRP(bp)) - size;
	if (extra_size > ALIGN(1 + OVERHEAD)) 
	{
		GET_SIZE(HDRP(bp)) = size;
		GET_SIZE(FTRP(bp)) = size;
		GET_SIZE(HDRP(NEXT_BLKP(bp))) = extra_size;
		GET_SIZE(FTRP(NEXT_BLKP(bp))) = extra_size;
		GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 0;
	}
	GET_ALLOC(HDRP(bp)) = 1;
}
