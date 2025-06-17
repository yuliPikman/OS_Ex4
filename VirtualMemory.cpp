#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <iostream>
#include "TreeUtils.h"
#include "PageUtils.h"
#include "FrameUtils.h"


void VMinitialize() {
    for (int i = 0; i < PAGE_SIZE; ++i)
    {
    PMwrite(i, 0);
    }
}

int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0; 
    }
    
    uint64_t foundFrame;
    if(!traverse_tree(virtualAddress, foundFrame)) {
        return 0;
    }
    if (foundFrame >= NUM_FRAMES) {
        return 0;
    }
    uint64_t offset = get_offset(virtualAddress);
    uint64_t addr = foundFrame * PAGE_SIZE + offset;
    if (addr >= RAM_SIZE) return 0;

    
    PMread(addr, value);

    return 1;
}



int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }

    uint64_t foundFrame;
    if (!traverse_tree(virtualAddress, foundFrame)) {
        return 0;
    }

    if (foundFrame >= NUM_FRAMES) {
        return 0;
    }

    uint64_t offset = get_offset(virtualAddress);
    uint64_t addr = foundFrame * PAGE_SIZE + offset;

    if (addr >= RAM_SIZE) return 0;

    std::cout << "[VMwrite] frame: " << foundFrame << ", offset: " << offset
              << ", value: " << value << ", addr: " << addr << std::endl;

    PMwrite(addr, value);

    return 1;
}
