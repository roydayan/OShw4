//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#define MAX_SIZE 100000000  //10^8

void* smalloc(size_t size){
    if (size == 0 || size > MAX_SIZE) {
        return NULL;
    }
    void* alloc_ptr = sbrk(static_cast<intptr_t>(size)); //TODO - should I use C-style cast??? sbrk((intptr_t)size);
    if (alloc_ptr == (void*)(-1)) {
        return NULL;
    }
    return alloc_ptr; //TODO - does this return the first allocated bit as desired or the last bit from previous block??
}