#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <algorithm>


#define LEVEL_BITS ((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / TABLES_DEPTH)


uint64_t get_page_number(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}


uint64_t get_offset(uint64_t virtualAddress) {
    return virtualAddress & ((1ULL << OFFSET_WIDTH) - 1);
}


//הערה לניר - בגלל שאסור משתנים גלובלים אז נראה שאין דרך יעילה יותר, גם לפי הג'יפיטי
uint64_t get_page_number_from_frame(uint64_t frame) {
    uint64_t page_number = 0;
    uint64_t current_frame = frame;

    for (int level = TABLES_DEPTH - 1; level >= 0; --level) {
        std::pair<uint64_t, uint64_t> pair = find_parent_and_index(current_frame); 
        uint64_t parent_frame = pair.first;
        uint64_t index = pair.second;

        page_number |= (index << (level * LEVEL_BITS)); 
        
        current_frame = parent_frame; 
    }

    return page_number;
}


bool traverse_tree(uint64_t virtualAddress, uint64_t& frame_found) {
    uint64_t currentFrame = 0; //root is 0
    
    for (int level = 0; level < TABLES_DEPTH; ++level) {
        uint64_t index = get_index_in_level(virtualAddress, level);
        word_t nextFrame;
        
        PMread(currentFrame * PAGE_SIZE + index, &nextFrame);
        
        if (nextFrame == 0) {
            if (!initialize_new_frame(currentFrame, virtualAddress, level, index)) {
                return false;
            }
        } else {
            currentFrame = nextFrame;
        }
    } 

    frame_found = currentFrame;
    return true;  
}


void VMinitialize() {
    for (int i = 0; i < PAGE_SIZE; ++i)
    {
    PMwrite(i, 0);
    }
}


uint64_t get_index_in_level(uint64_t virtualAddress, int level) {
    uint64_t k = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH)/TABLES_DEPTH;
    uint64_t shift = virtualAddress >> (OFFSET_WIDTH + k * (TABLES_DEPTH - level - 1));
    return shift & ((1ULL << k) - 1);
}


int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0; 
    }
    
    uint64_t foundFrame;
    if(!traverse_tree(virtualAddress, foundFrame)) {
        return 0;
    }
    
    uint64_t offset = get_offset(virtualAddress);
    PMread(foundFrame * PAGE_SIZE + offset, value);

    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0; 
    }
    
    uint64_t foundFrame;
    if(!traverse_tree(virtualAddress, foundFrame)) {
        return 0;
    }
    
    uint64_t offset = get_offset(virtualAddress);
    PMwrite(foundFrame * PAGE_SIZE + offset, value);
    
    return 1;
}


bool is_frame_in_use(uint64_t frame_to_check, uint64_t frames_in_tree[], uint64_t num_frames_in_tree) {
    for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
        uint64_t parent_frame = frames_in_tree[i];

        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            word_t value;
            PMread(parent_frame * PAGE_SIZE + index, &value);

            if (value == frame_to_check) {
                return true;
            }
        }
    }
    return false;
}


bool initialize_new_frame(uint64_t &currentFrame, uint64_t virtualAddress, int level, uint64_t index) {
    uint64_t newFrame = find_unused_frame_or_evict(get_page_number(virtualAddress));
    if (newFrame == 0)
    {
        return false;
    }
     
    if (level == TABLES_DEPTH - 1) {
        PMrestore(newFrame, get_page_number(virtualAddress));
    } else {
        for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
            PMwrite(newFrame * PAGE_SIZE + i, 0);
        }
    }
    
    PMwrite(currentFrame * PAGE_SIZE + index, newFrame);
    currentFrame = newFrame;
        
    return true;
}


void scan_tree(uint64_t currentFrame, uint64_t frames_in_tree[], uint64_t& num_frames_in_tree) {
    frames_in_tree[num_frames_in_tree++] = currentFrame;

    for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
        word_t value;
        PMread(currentFrame * PAGE_SIZE + index, &value);

        if (value != 0) {
            bool already_in_tree = false;
            for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
                if (frames_in_tree[i] == (uint64_t)value) {
                    already_in_tree = true;
                    break;
                }
            }

            if (!already_in_tree) {
                scan_tree((uint64_t)value, frames_in_tree, num_frames_in_tree);
            }
        }
    }
}


void update_distance_for_evict(uint64_t frame, uint64_t page_to_insert,
                            uint64_t& max_distance, uint64_t& frame_to_evict,
                            uint64_t& parent_frame_of_candidate, uint64_t& index_in_parent) {
    uint64_t page = get_page_number_from_frame(frame);

    uint64_t diff = (page_to_insert > page) ? (page_to_insert - page) : (page - page_to_insert);
    uint64_t distance = (NUM_PAGES - diff < diff) ? (NUM_PAGES - diff) : diff;

    if (distance > max_distance) {
        max_distance = distance;
        frame_to_evict = frame;

        auto parent_index = find_parent_and_index(frame);
        parent_frame_of_candidate = parent_index.first;
        index_in_parent = parent_index.second;
    }
}


uint64_t find_unused_frame_or_evict(uint64_t page_to_insert) {
    uint64_t frame_to_evict = 0;
    uint64_t max_distance = 0;
    uint64_t parent_frame_of_candidate = 0;
    uint64_t index_in_parent = 0;
    uint64_t max_frame_index = 0;

    uint64_t frames_in_tree[NUM_FRAMES];
    uint64_t num_frames_in_tree = 0;
    scan_tree(0, frames_in_tree, num_frames_in_tree);

    // Option 1
    for (uint64_t frame = 1; frame < NUM_FRAMES; frame++) {
        bool in_use = is_frame_in_use(frame, frames_in_tree, num_frames_in_tree);

        if (!in_use) {
            return frame;  
        }

        max_frame_index = std::max(max_frame_index, frame);

        // distance 
        update_distance_for_evict(frame, page_to_insert, max_distance, frame_to_evict, 
            parent_frame_of_candidate, index_in_parent);
    }

    // Option 2
    if (max_frame_index + 1 < NUM_FRAMES) {
        return max_frame_index + 1;
    }

    // Evict
    PMevict(frame_to_evict, get_page_number_from_frame(frame_to_evict));
    PMwrite(parent_frame_of_candidate * PAGE_SIZE + index_in_parent, 0);

    return frame_to_evict;
}


std::pair<uint64_t, uint64_t> find_parent_and_index(uint64_t child_frame) {
    for (uint64_t frame = 0; frame < NUM_FRAMES; ++frame) {
        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            word_t value;
            PMread(frame * PAGE_SIZE + index, &value);

            if (value == child_frame) {
                return {frame, index}; 
            }
        }
    }
    
    return {0, 0}; 
}



