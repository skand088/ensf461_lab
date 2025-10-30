#include <stdio.h>
#include <unistd.h>     // for getpagesize()
#include <sys/mman.h>   // for mmap, munmap
#include "myalloc.h"

void* _arena_start = NULL;
void* _arena_end = NULL; 
size_t _arena_size = 0;

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
