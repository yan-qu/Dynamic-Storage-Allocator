# Dynamic-Storage-Allocator

A dynamic storage allocaotor for C programs (same as "new" in C++)

There are four functions:

    int   mm_init(void);
    void *mm_malloc(size_t size);
    void  mm_free(void *ptr);
    void *mm_realloc(void *ptr, size_t size);

1. mm_init: starts the application program by performing any necessary initializations, such as allocating the initial heap area.

2. mm_malloc: returns a pointer to an allocated block payload of at least size bytes.

3. mm_free: The mm free routine frees the block pointed to by ptr. 

4. mm_realloc: The mm realloc routine returns a pointer to an allocated region of at least size bytes.
