//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#define MAX_SIZE 100000000  //10^8

void* smalloc(size_t size){
    if (size == 0 || size > MAX_SIZE) {
        return nullptr; //Note - this is .cpp file, so I am using nullptr instead of NULL
    }
    void* alloc_ptr = sbrk(static_cast<intptr_t>(size)); //TODO - should I use C-style cast??? sbrk((intptr_t)size);
    if (alloc_ptr == (void*)(-1)) {
        return nullptr;
    }
    return alloc_ptr; //TODO - does this return the first allocated bit as desired or the last bit from previous block??
}

/*
//--------------------------------
//FOR TEST - Note: passed this
//--------------------------------
int main() {
    int i = 1000;
    void* ptr = smalloc(i);
    void* ptrFAIL1 = smalloc(MAX_SIZE+10);
    void *ptrFAIL2 = smalloc(0);
    if (ptr == nullptr) {
        return 1;
    }
    int *int_arr = (int*)ptr;
    int x = int_arr[0];
    int y = int_arr[i-1];
    for (int j = 0; j < i; j++) {
        int_arr[j] = j;
    }
    int z = int_arr[i-1];
    if (y == z) { //make sure value changed
        return 2;
    }

    //try allocating a bunch of new blocks
    for (int q = 3; q < 10; q++) {
        void* ptr = smalloc(MAX_SIZE-1000);
        if (ptr == nullptr) {
            return q;
        }
    }
    //try allocating a bunch of new blocks and write into them
    for (int r = 11; r < 100000; r++) {
        void* ptr3 = smalloc(MAX_SIZE-1000);
        if (ptr3 == nullptr) {
            return r;
        }
        int *new_arr = (int*)ptr3;
        //write into arr
        new_arr[10000] = 1;
    }


    return 0;
}
*/
