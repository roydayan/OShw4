//
// Created by royda on 8/14/2024.
//
//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#include <cstring> //for memmove
#define MAX_SIZE 100000000  //10^8
#define MAX_ORDER 10
#define INIT_ALLOC (128*1024*32) //128KB * 32 = 4MB
#define INIT_NUM_BLOCKS 32
//#define INIT_BLOCK_SIZE 128*1024 //128KB
#define BLOCK_SIZE(order) (1 << (order + 7))    // 128 bytes for order 0
//#define MAX_BLOCK_SIZE BLOCK_SIZE(MAX_ORDER) // 128KB for order 10

//TODO- implement malloc_3 - this is copy of malloc_2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

typedef struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} * Meta;

class Block_Table {
private:
    Meta table[MAX_ORDER+1] = {nullptr}; //array of linked lists
public:
    Block_Table() : table() {};
};



Meta free_table[MAX_ORDER+1]; //array of linked lists

//initialize memory to be aligned for easy buddy finding
void init_memory() {
    //align memory to 4MB
    size_t base = (size_t)sbrk(0);   //get current program break
    size_t remainder = base % INIT_ALLOC;
    if (remainder != 0) {
        sbrk(INIT_ALLOC - remainder);
    }
    /*
    FOR TEST : ensure aligned   TODO - remove!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if ((size_t)sbrk(0) % INIT_ALLOC != 0) {
        throw;
        return; //sbrk failed
    }
    */
    //allocate 4MB - I prefer allocating once rather than for every block
    void* memory_start = sbrk(INIT_ALLOC);
    if (memory_start == (void*)(-1)) {
        return; //sbrk failed
    }
    size_t index = (size_t)memory_start;
    for (int i = 0; i < INIT_NUM_BLOCKS; i++) {
        Meta new_block = (Meta)index;
        new_block->size = BLOCK_SIZE(MAX_ORDER);
        new_block->is_free = true;
        new_block->next = nullptr;
        new_block->prev = nullptr;
        //insert new block last in list b/c we want to keep the list sorted in addresses (and update data)
        if (free_table[MAX_ORDER] == nullptr) {
            free_table[MAX_ORDER] = new_block;
        }
        else {
            Meta curr = free_table[MAX_ORDER]; //head is "bottom of heap"
            while (curr) {
                //if we reached last block, attach new block to end
                if (curr->next == nullptr) {
                    curr->next = new_block;
                    new_block->prev = curr;
                    break;
                }
                curr = curr->next;
            }
        }
        index += BLOCK_SIZE(MAX_ORDER);
    }
}


void* findBlockGivenOrder(int order);
void* recurseSplitAlloc(size_t size, int order);
void* largeAlloc(size_t size);


void* smalloc(size_t size) {
    if (size == 0) {
        return nullptr; //Note - this is .cpp file, so I am using nullptr instead of NULL
    }
    if (size >= BLOCK_SIZE(MAX_ORDER)) {
        return largeAlloc(size);
    }
    int order = 0;
    while (BLOCK_SIZE(order) - sizeof(MallocMetadata) < size && order <= MAX_ORDER) {
        order++;
    }
    if (order > MAX_ORDER) {
        return nullptr; // Note: not supposed to reach this b/c if size >= MAX_SIZE, largeAlloc will be called
    }
    // check if there is a free block of the right size (no need to split) allocate as before
    if (free_table[order]) {
        return findBlockGivenOrder(order);
    }
    else { //no free block of right size
        // Find the tightest fit block
        for (int current_order = order + 1; current_order <= MAX_ORDER; current_order++) {
            if (free_table[current_order]) {
                return recurseSplitAlloc(size, current_order);
            }
        }
    }
    return nullptr;
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
    Meta block = (Meta)p - 1;
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

    Meta old_block = (Meta)oldp - 1;
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




void* findBlockGivenOrder(int order) {
    Meta curr = free_table[order];
    while (curr) {
        if (curr->is_free) {
            curr->is_free = false;
            break;
        }
        curr = curr->next;
    }
    //remove block from free_table
    if (curr->prev) {
        curr->prev->next = curr->next;
    }
    if (curr->next) {
        curr->next->prev = curr->prev;
    }
    if (free_table[order] == curr) {
        free_table[order] = curr->next;
    }
    return (void*)(curr + 1);
}

void* recurseSplitAlloc(size_t size, int order) {
        Meta curr = free_table[order];
        while (curr) {
            if (curr->is_free) {
                curr->is_free = false;
                size_t new_block_size = BLOCK_SIZE(order);

                //TODO - split!!!!
            }
        }
        //remove block from free_table
        if (curr->prev) {
            curr->prev->next = curr->next;
        }
        if (curr->next) {
            curr->next->prev = curr->prev;
        }
        if (free_table[order] == curr) {
            free_table[order] = curr->next;
        }

};
