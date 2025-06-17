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
    uint64_t parent_of[NUM_FRAMES] = {0};
    uint64_t frames_in_tree[NUM_FRAMES];
    uint64_t num_frames_in_tree = 0;
    uint64_t page_of[NUM_FRAMES] = {0};
    uint64_t depth_of[NUM_FRAMES] = {0};

    scan_tree(0, 0, frames_in_tree, num_frames_in_tree, parent_of, page_of, depth_of);

    if (!traverse_tree(virtualAddress, foundFrame, parent_of,
                       frames_in_tree, num_frames_in_tree,
                       page_of, depth_of)) {
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
    uint64_t parent_of[NUM_FRAMES] = {0};
    uint64_t frames_in_tree[NUM_FRAMES];
    uint64_t num_frames_in_tree = 0;
    uint64_t page_of[NUM_FRAMES] = {0};
    uint64_t depth_of[NUM_FRAMES] = {0};

    scan_tree(0, 0, frames_in_tree, num_frames_in_tree, parent_of, page_of, depth_of);

    if (!traverse_tree(virtualAddress, foundFrame, parent_of,
                       frames_in_tree, num_frames_in_tree,
                       page_of, depth_of)) {
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

