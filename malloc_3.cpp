//
// Created by royda on 8/14/2024.
//
//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#include <cstring> //for memmove
#define MAX_SIZE 100000000  //10^8



//TODO- implement malloc_3 - this is copy of malloc_2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// TODO - IMPORTANT NOTE 2 in pdf - keep SORTED LIST of the allocations in system




struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* head = nullptr;



void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr; //Note - this is .cpp file, so I am using nullptr instead of NULL
    }

    MallocMetadata* curr = head;

    // Search for a pre-existing suitable free block
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = false;
            return (void*)(curr + 1);
            // return pointer after the metadata (cast it back to void* b/c it was cast to MallocMetadata* in creation)
            // note: adding one advances the ptr by sizeof(metadata) b/c curr is a pointer to metadata
        }
        curr = curr->next;
    }

    // No suitable block found, allocate a new one
    void* block_start = sbrk(static_cast<intptr_t>(size + sizeof(MallocMetadata))); //TODO - should I not use cast?? or use C-style cast?? sbrk((intptr_t)size);
    if (block_start == (void*)(-1)) {
        return nullptr; //sbrk failed
    }

    //update metadata of new block
    MallocMetadata* new_block;
    new_block = (MallocMetadata*)block_start;
    new_block->size = size;
    new_block->is_free = false;

    //insert new block last in list b/c we want to keep the list sorted in addresses (and update data)
    curr = head; //head is "bottom of heap"
    while (curr) {
        //if we reached last block, attach new block to end
        if (curr->next == nullptr) {
            curr->next = new_block;
            new_block->prev = curr;
            new_block->next = nullptr;
            break;
        }
        curr = curr->next;
    }

    return (void*)(new_block + 1);
    // note: adding one advances the ptr by sizeof(metadata) b/c new_block is a pointer to metadata
}


void* scalloc(size_t num, size_t size) {
    // Calculate the total size of the block
    size_t total_size = num * size;

    // Try to find a free block or allocate a new one (check size in smalloc)
    void* ptr = smalloc(total_size);
    if (ptr == nullptr) {
        return nullptr;
    }

    // Zero out the allocated memory
    std::memset(ptr, 0, total_size);

    return ptr;
}


void sfree(void* p) {
    if (!p) {
        return;
    }

    // cast and decrement to get to metadata
    MallocMetadata* block = (MallocMetadata*)p - 1;
    block->is_free = true;
}


void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }

    // If oldp is null, realloc is equivalent to malloc
    if (!oldp) {
        return smalloc(size);
    }

    MallocMetadata* old_block = (MallocMetadata*)oldp - 1;
    if (old_block->size >= size) {
        return oldp;
    }

    void* newp = smalloc(size);
    if (!newp) {
        return nullptr; //in this case the old block is still valid
    }

    // Copy the data from oldp to newp
    std::memmove(newp, oldp, old_block->size);
    sfree(oldp);

    // Note: old block freed only if realloc was successful!!!!!!!!!!!!!!!!!!!!!!!
    return newp;
}


//--------------------------------------------------------------------------------
//Stats Methods
//--------------------------------------------------------------------------------

size_t _num_free_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while (curr) {
        if (curr->is_free) {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_free_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while (curr) {
        if (curr->is_free) {
            count += curr->size;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while (curr) {
        count++;
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while (curr) {
        count += curr->size;
        curr = curr->next;
    }
    return count;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

/*
//--------------------------------------------------------------------------------
//FOR TEST
//--------------------------------------------------------------------------------
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
        void* ptr1 = smalloc(MAX_SIZE-1000);
        if (ptr1 == nullptr) {
            return q;
        }
    }
    //try allocating a bunch of new blocks and write into them
    for (int r = 11; r < 1000; r++) {
        void* ptr3 = smalloc(MAX_SIZE-1000);
        if (ptr3 == nullptr) {
            return r;
        }
        int *new_arr = (int*)ptr3;
        //write into arr
        new_arr[10000] = 1;
    }

    //test calloc
    void* ptr4 = scalloc(100, sizeof(int));
    if (ptr4 == nullptr) {
        return 100;
    }
    int* int_arr2 = (int*)ptr4;
    for (int j = 0; j < 100; j++) {
        if (int_arr2[j] != 0) {
            return 101;
        }
    }

    //test realloc
    void* ptr5 = srealloc(ptr4, 1000*sizeof(int));
    if (ptr5 == nullptr) {
        return 102;
    }
    int* int_arr3 = (int*)ptr5;
    for (int j = 0; j < 100; j++) {
        if (int_arr3[j] != 0) {
            return 103;
        }
    }

    return 0;
}
 */