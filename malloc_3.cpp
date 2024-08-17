//
// Created by royda on 8/14/2024.
//
//
// Created by royda on 8/13/2024.
//

#include <unistd.h>
#include <cstring> //for memmove
#include <sys/mman.h> //for mmap
#include <iostream>
#define MAX_SIZE 100000000  //10^8
#define MAX_ORDER 10
#define INIT_ALLOC (128*1024*32) //128KB * 32 = 4MB
#define INIT_NUM_BLOCKS 32
//#define INIT_BLOCK_SIZE 128*1024 //128KB
#define BLOCK_SIZE(order) (1 << (order + 7))    // 128 bytes for order 0
//#define MAX_BLOCK_SIZE BLOCK_SIZE(MAX_ORDER) // 128KB for order 10

//TODO- implement malloc_3 - this is copy of malloc_2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

//this does log2(size//2^7)
int getOrder(size_t size){
    int order = 0;
    size = size >> 8; //deviding by 2^7
    while (size != 0){
        size >> 1;
        order++;
    }
    return order;

}

typedef struct MallocMetadata {
    size_t size;
    size_t block_size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} * Meta;


class Block_Table {

private:

    Meta table[MAX_ORDER+1] = {nullptr}; //array of linked lists
    bool isInitialized = false;

    //Meta head = nullptr;

public:
    size_t  num_free_block = 0;
    size_t num_free_bytes = 0;
    size_t num_allocated_blocks = 0;
    size_t num_meta_data_bytes = 0;

    Block_Table() : table() {};
    void initTable();


    void* getAvailableBlock(size_t size);

    //trys to merge block with buddies to obtain a block with size big enough to fit , if not found returns nullptr
    Meta tryRealocUsingSameBlock(Meta block, size_t desired_size);

    Meta mergeBlocksToFitSize(Meta block, size_t desired_size);

    void* findBlockGivenOrder(int order); //TODO - EASY, check addresses when finding place in list

    Meta recurseSplitAlloc(size_t size, int order, Meta block);

    void* splitAlloc(size_t size, int order);

    void freeBlock(Meta block);

    void recurseSplitFree(Meta block);

    void insertBlockIntoList(Meta block, int order);

    void removeFromList(Meta block , int order);

    //bool checkBuddies(Meta block1, Meta block2);
    //Meta mergeBuddies(Meta block1, Meta block2);
    //Meta splitBlock(Meta block, int order);

    void recurseMergeFree(Meta block, int order);

    //helper funcs:
    bool checkPrevBuddy(Meta block, int order);
    bool checkNextBuddy(Meta block, int order);
    Meta mergeRemoveBuddies(Meta block1, Meta block2);

};



class MmapedList{
private:
    MallocMetadata* head;


public:
    size_t  num_free_block = 0;
    size_t num_free_bytes = 0;
    size_t num_allocated_blocks = 0;
    size_t num_meta_data_bytes = 0;
    MmapedList():head(nullptr){}
    void insertBlock(Meta block);
    void removeBlock(Meta block);
};

void* largeAlloc(size_t size);
void largeFree(Meta block);
void* largeRealloc(void* oldp, size_t size);


Block_Table block_table = Block_Table();
MmapedList mmaped_list = MmapedList();


void* smalloc(size_t size) {
    block_table.initTable();//does nothing if already initialized

    if (size == 0) {
        return nullptr; //Note - this is .cpp file, so I am using nullptr instead of NULL
    }
    if (size+sizeof(MallocMetadata) >= BLOCK_SIZE(MAX_ORDER)) {
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

    if( size+sizeof(MallocMetadata) >= BLOCK_SIZE(MAX_ORDER) ){
        return largeRealloc(oldp, size);
    }

    Meta old_block = (Meta)oldp - 1;
    if (old_block->size >= size) {
        return oldp;
    }

    void* newp;
    Meta new_block = block_table.tryRealocUsingSameBlock(old_block ,size);

    if(new_block != nullptr){
        newp = (void*)(new_block+1);
        std::memmove(newp, oldp, old_block->size);
    }
    else{
        newp = smalloc(size);
        if(newp == nullptr) return nullptr;

        std::memmove(newp, oldp, old_block->size);
        sfree(oldp);
    }

    return newp;
}

void* largeRealloc(void* oldp, size_t size){

    if(oldp == nullptr){//should'nt happen
        return smalloc(size);
    }

    Meta old_block =  ( (Meta)(oldp) ) - 1;
    if(old_block->size == size){
        return oldp;
    }

    void* newp = smalloc(size);//will call mmap because size > 128kb
    std::memmove(newp, oldp, old_block->size);
    sfree(oldp);//will call munmap

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
        new_block->size = BLOCK_SIZE(MAX_ORDER) - sizeof(MallocMetadata);
        new_block->block_size = BLOCK_SIZE(MAX_ORDER);
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
        return nullptr; // Note: not supposed to reach this b/c if size >= MAX_SIZE, largeAlloc will be called
    }
    // check if there is a free block of the right size (no need to split) allocate as before
    if (table[order]) {
        //TODO-- fix this: findBlockGivenOrder should return Meta
        Meta found_block = ((Meta)findBlockGivenOrder(order) - 1);
        found_block->size = size;
        return (void*)(found_block+1);
    }
    else { //no free block of right size
        // Find the tightest fit block
        for (int current_order = order + 1; current_order <= MAX_ORDER; current_order++) {
            if (table[current_order]) {
                return splitAlloc(size, current_order);
            }
        }
    }
    //TODO-- do we need to return null? or just allocate using mmap?
    return nullptr;
}

void* Block_Table::findBlockGivenOrder(int order) {
    Meta curr = table[order];
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

void* Block_Table::splitAlloc(size_t size, int order){
    //Removing first block in table[order]
    Meta curr_block = this->table[order];
    if(curr_block->next != nullptr){
        curr_block->next->prev = nullptr;
    }
    this->table[order] = curr_block->next;

    //splitting block
    Meta allocated_block = recurseSplitAlloc(size, order, curr_block);
    return (void*)(allocated_block+1);
}


Meta Block_Table::recurseSplitAlloc(size_t size, int order, Meta block) {
    //block is not "large enough"
    if(order == 0 || size+sizeof(MallocMetadata) > BLOCK_SIZE(order-1) ){
        *block = {size, static_cast<size_t>(BLOCK_SIZE(order)),false, nullptr, nullptr};
        return block;
    }

    //split
    size_t size_after_split = static_cast<size_t> (BLOCK_SIZE(order-1));

    Meta allocated_block = block;
    *allocated_block = {size_after_split,size_after_split ,true, nullptr, nullptr};

    Meta free_block = (Meta) ( ( static_cast<char *>( (void*)block) ) + BLOCK_SIZE(order-1) );
    *free_block = {size_after_split, size_after_split ,true, nullptr , nullptr};

    insertBlockIntoList(free_block, order-1);

    return recurseSplitAlloc(size, order-1, allocated_block);

}

Meta Block_Table::tryRealocUsingSameBlock(Meta block, size_t desired_size){
    if(block->block_size >= desired_size + sizeof(MallocMetadata)){
        block->size = desired_size;
        return block;
    }

    //calculating if block could be merged to fit desired_size
    Meta merged_block = block;
    size_t merged_block_size = block->block_size;
    while ( merged_block_size < BLOCK_SIZE(MAX_ORDER) ){
        long long base_addr = (long long) merged_block;
        Meta buddy = (Meta) ( base_addr ^ ( (long long)(merged_block_size) ) );

        if(!buddy->is_free){
            return nullptr;
        }
        merged_block = (merged_block < buddy) ? merged_block : buddy;
        merged_block_size = merged_block_size *2;

        //block could be merged to fit desired_size. perform the merges
        if(merged_block_size >= desired_size + sizeof(MallocMetadata)){
            return mergeBlocksToFitSize(block, desired_size);
        }
    }
    return nullptr;

}
void Block_Table::freeBlock(Meta block) {
    // assumes not nullptr. size < BLOCK_SIZE(MAX_ORDER) b/c large blocks are handled separately
    int order = 0;
    while (BLOCK_SIZE(order) - sizeof(MallocMetadata) < block->size) {
        order++;
    }
    block->is_free = true;
    recurseMergeFree(block, order);
}


void Block_Table::recurseMergeFree(Meta block, int order) {
    insertBlockIntoList(block, order);
    block->block_size = BLOCK_SIZE(order);
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
    new_block->is_free = true;
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

void* largeAlloc(size_t size){
    void* block = mmap(NULL, size+sizeof(MallocMetadata), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    long long block_long = (long long) block;
    if( block_long == -1){
        return nullptr;
    }

    Meta block_meta = (Meta)(block);
    *block_meta = {size, block_meta->block_size ,false, nullptr, nullptr};

    mmaped_list.insertBlock(block_meta);
    return (void*) (block_meta+1);

}


Meta Block_Table::mergeBlocksToFitSize(Meta block, size_t desired_size){
    //no need to merge further
    if(block->block_size >= desired_size+ sizeof(MallocMetadata)){
        return block;
    }
    //cannot merge any more (shouldnt happen if called by tryRealloc...)
    if(block->block_size == BLOCK_SIZE(MAX_ORDER) ){
        return nullptr;
    }

    //find buddy
    long long base_addr = (long long)(block);
    Meta buddy = (Meta) (base_addr ^ (long long)(block->block_size));

    //ensure buddy and block can be merged (shouldnt happen if called by tryRealloc...)
    if(!buddy->is_free){
        return nullptr;
    }

    //remove buddy from free list
    removeFromList(buddy, getOrder(buddy->block_size));

    //merge block and buddy
    Meta merged_block = (block < buddy) ? block : buddy;
    merged_block->is_free = false;
    merged_block->size = desired_size;
    merged_block->block_size = 2 * block->block_size;
    merged_block->next = nullptr;
    merged_block->prev = nullptr;

    //keep on merging
    return mergeBlocksToFitSize(merged_block, desired_size);

}

void Block_Table::removeFromList(Meta block, int order){
    if(order > MAX_ORDER || table[order] == nullptr){//should'nt happen
        return;
    }

    if(table[order] == block){
        table[order] = block->next;
    }
    if(block->prev != nullptr){
        block->prev->next = block->next;
    }
    if(block->next != nullptr){
        block->next->prev = block->prev;
    }

    block->next, block->prev = nullptr;
    block->is_free = false;
    return;

}


void largeFree(Meta block) {
    // use munmap to free large blocks, assumes block is not nullptr
    mmaped_list.removeBlock(block);
    munmap(block, block->size + sizeof(MallocMetadata));

    //TODO - update stats!!!!!!!!!!!!!!
}


void MmapedList::removeBlock(Meta block) {
    //remove block from list
    if (block->prev) {
        block->prev->next = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    if (head == block) {
        head = block->next;
    }
    block->is_free = true;
}


void MmapedList::insertBlock(Meta block){
    if(head == nullptr){
        head = block;
        head->next = nullptr;
        head->prev = nullptr;
        return;
    }
    head->prev = block;
    block->next = head;
    block->prev = nullptr;
    block->is_free = false;
    head = block;
    return;
}







//int main(){
//    char* p = (char*) smalloc(BLOCK_SIZE(MAX_ORDER) - sizeof(MallocMetadata) -1);
//    p[0] = 'a';
//    p[1] = 'b';
//    p[2] = '\0';
//    std::cout<<p<<std::endl;
//    return  0;
//}
int main() {
    int i = 100;
    void* ptr = smalloc(i*sizeof(int) );
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