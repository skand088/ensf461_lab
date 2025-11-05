#include <stdio.h>
#include <unistd.h>     // for getpagesize()
#include <sys/mman.h>   // for mmap, munmap
#include "myalloc.h"

void* _arena_start = NULL;
void* _arena_end = NULL; 
size_t _arena_size = 0;
node_t* _free_list_head = NULL;
int statusno = 0;


int myinit(size_t size)
{
    printf("Initializing arena:\n");
    printf("...requested size %lu bytes\n", size);

    if (size > (size_t)MAX_ARENA_SIZE)
    {
        printf("...error: requested size larger than MAX_ARENA_SIZE (%d)\n", MAX_ARENA_SIZE);
        return ERR_BAD_ARGUMENTS;
    }

    int pagesize = getpagesize();
    printf("...pagesize is %d bytes\n", pagesize);

    // Align size to page boundary
    if (size % pagesize != 0)
    {
        size += pagesize - (size % pagesize);
    }

    printf("...adjusting size with page boundaries\n");
    printf("...adjusted size is %lu bytes\n", size);

    printf("...mapping arena with mmap()\n");
    _arena_start = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (_arena_start == MAP_FAILED)
    {
        perror("mmap failed");
        return ERR_SYSCALL_FAILED;
    }
    _arena_size = size;
    _arena_end = (char*)_arena_start + size;

    _free_list_head = (node_t*)_arena_start;
    _free_list_head->size = _arena_size - sizeof(node_t);
    _free_list_head->is_free = 1;
    _free_list_head->fwd = NULL;
    _free_list_head->bwd = NULL;


    printf("...arena starts at %p\n", _arena_start);
    printf("...arena ends at %p\n", (char*)_arena_start + size);

    return _arena_size;
}

int mydestroy()
{
    printf("Destroying Arena:\n");
    if (_arena_start == NULL)
    {
        printf("...error: no arena to destroy\n");
        return ERR_UNINITIALIZED;
    }

    printf("...unmapping arena with munmap()\n");

    if (munmap(_arena_start, _arena_size) != 0)
    {
        perror("munmap failed");
        return ERR_SYSCALL_FAILED;
    }

    _arena_start = NULL;
    _arena_size = 0;

    return 0;
}

void* myalloc(size_t size)
{
    printf("Allocating memory:\n");

    if (_arena_start == NULL) {
        printf("...error: arena not initialized\n");
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }

    node_t* curr = _free_list_head;

    // Find first free chunk large enough
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) break;
        curr = curr->fwd;
    }

    if (curr == NULL) {
        printf("...no suitable chunk found, out of memory!\n");
        statusno = ERR_OUT_OF_MEMORY;
        return NULL;
    }

    printf("...found free chunk of %zu bytes with header at %p\n", curr->size, curr);

    // Split chunk if enough space remains for a new free chunk
    size_t remaining = curr->size - size;
    if (remaining > sizeof(node_t)) {
        node_t* new_chunk = (node_t*)((char*)curr + sizeof(node_t) + size);
        new_chunk->size = remaining - sizeof(node_t);
        new_chunk->is_free = 1;
        new_chunk->fwd = curr->fwd;
        new_chunk->bwd = curr;

        if (curr->fwd) curr->fwd->bwd = new_chunk;
        curr->fwd = new_chunk;
        curr->size = size;

        printf("...splitting chunk, new free chunk at %p of size %zu\n", new_chunk, new_chunk->size);
    }

    curr->is_free = 0;

    void* user_ptr = (char*)curr + sizeof(node_t);
    printf("...allocation starts at %p\n", user_ptr);
    return user_ptr;
}

void myfree(void* ptr)
{
    printf("Freeing allocated memory:\n");
    printf("...supplied pointer %p\n", ptr);

    if (ptr == NULL) return;

    if (_arena_start == NULL) {
        printf("...error: arena not initialized\n");
        statusno = ERR_UNINITIALIZED;
        return;
    }

    node_t* header = (node_t*)((char*)ptr - sizeof(node_t));
    printf("...accessing chunk header at %p\n", header);

    header->is_free = 1;
    printf("...chunk of size %zu\n", header->size);

    if (header->fwd && header->fwd->is_free) {
        node_t* next = header->fwd;
        header->size += sizeof(node_t) + next->size;
        header->fwd = next->fwd;
        if (next->fwd) next->fwd->bwd = header;
        printf("...coalesced with next chunk at %p\n", next);
    }

    if (header->bwd && header->bwd->is_free) {
        node_t* prev = header->bwd;
        prev->size += sizeof(node_t) + header->size;
        prev->fwd = header->fwd;
        if (header->fwd) header->fwd->bwd = prev;
        header = prev; // update header pointer for clarity
        printf("...coalesced with previous chunk at %p\n", prev);
    }

    printf("...coalescing done (if needed)\n");
}
