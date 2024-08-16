//
// Created by royda on 8/14/2024.
//
//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#include <cstring> //for memmove
#include <sys/mman.h> //for mmap
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
    bool isInitialized = false;

    Meta mmap_head = nullptr;

public:

    Block_Table() : table() {};
    void initTable();


    void* getAvailableBlock(size_t size);
    void* findBlockGivenOrder(int order);
    void* recurseSplitAlloc(size_t size, int order);
    void freeBlock(Meta block);
    void recurseMergeFree(Meta block, int order);

    //helper funcs:
    void insertBlockIntoList(Meta block, int order); //inserts block into list corresponding to order
    bool checkPrevBuddy(Meta block, int order);
    bool checkNextBuddy(Meta block, int order);
    Meta mergeRemoveBuddies(Meta block1, Meta block2);

    //large block methods
    void addLargeBlock(Meta block);
    void removeLargeBlock(Meta block);
};

void* largeAlloc(size_t size);
void largeFree(Meta block);



Block_Table block_table;


void* smalloc(size_t size) {
    if (size == 0) {
        return nullptr; //Note - this is .cpp file, so I am using nullptr instead of NULL
    }
    if (size + sizeof(MallocMetadata) >= BLOCK_SIZE(MAX_ORDER)) {
        return largeAlloc(size);
    }
    return block_table.getAvailableBlock(size);
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
    if ((block->size + sizeof(MallocMetadata)) >= BLOCK_SIZE(MAX_ORDER)) {
        largeFree(block);
    }
    block_table.freeBlock(block);
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



//-------------------------------------------------------------
//Block_Table methods
//-------------------------------------------------------------

//initialize memory to be aligned for easy buddy finding
void Block_Table::initTable() {
    if (isInitialized) {
        return;
    }
    isInitialized = true;
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
        if (table[MAX_ORDER] == nullptr) {
            table[MAX_ORDER] = new_block;
        }
        else {
            Meta curr = table[MAX_ORDER]; //head is "bottom of heap"
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

void* Block_Table::getAvailableBlock(size_t size) {
    int order = 0;
    while (BLOCK_SIZE(order) - sizeof(MallocMetadata) < size && order <= MAX_ORDER) {
        order++;
    }
    if (order > MAX_ORDER) {
        return nullptr; // Note: not supposed to reach this b/c if size >= BLOCK_SIZE(MAX_ORDER), largeAlloc will be called
    }
    // check if there is a free block of the right size (no need to split) allocate as before
    if (table[order]) {
        return findBlockGivenOrder(order);
    }
    else { //no free block of right size
        // Find the tightest fit block
        for (int current_order = order + 1; current_order <= MAX_ORDER; current_order++) {
            if (table[current_order]) {
                return recurseSplitAlloc(size, current_order);
            }
        }
    }
    return nullptr;
}


void* Block_Table::findBlockGivenOrder(int order) {
    Meta curr = table[order];
    while (curr) {
        if (curr->is_free) {
            curr->is_free = false;
            break;
            //TODO - should we check free or just return the first non-null block??????????
        }
        curr = curr->next;
    }
    //remove block from table
    if (curr->prev) {
        curr->prev->next = curr->next;
    }
    if (curr->next) {
        curr->next->prev = curr->prev;
    }
    if (table[order] == curr) {
        table[order] = curr->next;
    }
    return (void*)(curr + 1);
}


void* Block_Table::recurseSplitAlloc(size_t size, int order) {

    //TODO - implement!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    Meta curr = table[order];
    while (curr) {
        if (curr->is_free) {
            curr->is_free = false;
            size_t new_block_size = BLOCK_SIZE(order);
            //TODO - split!!!!
        }
    }
    //remove block from table
    if (curr->prev) {
        curr->prev->next = curr->next;
    }
    if (curr->next) {
        curr->next->prev = curr->prev;
    }
    if (table[order] == curr) {
        table[order] = curr->next;
    }
    return (void*)(curr + 1);

}


void Block_Table::freeBlock(Meta block) {
    // assumes not nullptr. size < BLOCK_SIZE(MAX_ORDER) b/c large blocks are handled separately
    int order = 0;
    while (BLOCK_SIZE(order) - sizeof(MallocMetadata) < block->size) {
        order++;
    }
    recurseMergeFree(block, order);
}


void Block_Table::recurseMergeFree(Meta block, int order) {
    insertBlockIntoList(block, order);
    if (order == MAX_ORDER) { //no merging in max order
        return;
    }
    else if (checkPrevBuddy (block, order)) {
        Meta mergedBlock = mergeRemoveBuddies(block->prev, block);
        recurseMergeFree(mergedBlock, order + 1);
    }
    else if (checkNextBuddy (block, order)) {
        Meta mergedBlock = mergeRemoveBuddies(block, block->next);
        recurseMergeFree(mergedBlock, order + 1);
    }
}


void Block_Table::insertBlockIntoList(Meta block, int order) {
    //insert new block last in list b/c we want to keep the list sorted in addresses (and update data)
    if (table[order] == nullptr) {
        table[order] = block;
        block->prev = nullptr;
        block->next = nullptr;
    }
    else if (block < table[order]) { //insert at beginning
        block->next = table[order];
        block->prev = nullptr;
        table[order]->prev = block;
        table[order] = block;
    }
    else { //search place in list
        Meta curr = table[order]->next;
        while (curr) {
            //insert if block has lower address
            if (block < curr) {
                block->next = curr;
                block->prev = curr->prev;
                curr->prev->next = block; // curr->prev != nullptr b/c we checked the head of list (table[order])
                curr->prev = block;
                break;
            }
            //if we reached last block, attach new block to end
            else if (curr->next == nullptr) {
                curr->next = block;
                block->prev = curr;
                break;
            }
            curr = curr->next;
        }
    }
}


bool Block_Table::checkPrevBuddy(Meta block, int order) {
    // assumes block is not nullptr
    // trick: buddy address = block address XOR block size
    Meta buddy = (Meta)((size_t)block ^ BLOCK_SIZE(order));
    //TODO - ensure buddy is free? I'm counting on valid updates of next and prev fields
    if ((block->prev != nullptr) && (block->prev == buddy)) {
        return true;
    }
    return false;
}

bool Block_Table::checkNextBuddy(Meta block, int order) {
    // assumes block is not nullptr
    // trick: buddy address = block address XOR block size
    Meta buddy = (Meta)((size_t)block ^ BLOCK_SIZE(order));
    if ((block->next != nullptr) && (block->next == buddy)) {
        return true;
    }
    return false;
}


Meta Block_Table::mergeRemoveBuddies(Meta block1, Meta block2) { //block 1 is prev of 2
    //assumes blocks are not nullptr
    Meta new_block = block1; //prev and is_free are already supposed to be set
    //new_block->size = (block1->size * 2); //size should still be 0 in free block
    //new_block->is_free = true;
    new_block->next = nullptr;
    new_block->prev = nullptr;

    //remove merged block from list
    if (block2->next) {
        block2->next->prev = block1->prev;
    }
    if (block1->prev) {
        block1->prev->next = block2->next;
    }

    return new_block;
}


//-------------------------------------------------------------
//large block methods
//-------------------------------------------------------------

void* largeAlloc(size_t size) {
    // use mmap to allocate large blocks
    void* ptr = mmap(nullptr, size + sizeof(MallocMetadata),
                     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return nullptr;
    }

    // Initialize metadata
    Meta block = (Meta)ptr;
    block->size = size;
    block->is_free = false;
    block->next = nullptr;
    block->prev = nullptr;

    // Add this block to the mmap'd blocks list (not necessarily sorted)
    block_table.addLargeBlock(block);

    // Return the pointer to the memory after the metadata
    return (void*)(block + 1);
}


void largeFree(Meta block) {
    // use munmap to free large blocks, assumes block is not nullptr
    block_table.removeLargeBlock(block);
    munmap(block, block->size + sizeof(MallocMetadata));
}


void Block_Table::addLargeBlock(Meta new_block) {
    //no need to keep list sorted - put new block at head
    if (mmap_head != nullptr) {
        mmap_head->prev = new_block;
    }
    new_block->next = mmap_head;
    mmap_head = new_block;
    new_block->prev = nullptr;
}


void Block_Table::removeLargeBlock(Meta block) {
    //remove block from list
    if (block->prev) {
        block->prev->next = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    if (mmap_head == block) {
        mmap_head = block->next;
    }
}


int main() {
    block_table.initTable();
    int* ptr = (int*)smalloc(1000);
    return 0;
}