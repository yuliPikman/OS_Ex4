#include "PageUtils.h"
#include "TreeUtils.h"

uint64_t get_page_number(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}


uint64_t get_offset(uint64_t virtualAddress) {
    return virtualAddress & ((1ULL << OFFSET_WIDTH) - 1);
}



uint64_t get_page_mapping(uint64_t frame) {
    word_t page_number = 0;
    PMread(frame * PAGE_SIZE, &page_number);
    return page_number;
}


uint64_t get_index_in_level(uint64_t virtualAddress, int level) {
    uint64_t k = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH)/TABLES_DEPTH;
    uint64_t shift = virtualAddress >> (OFFSET_WIDTH + k * (TABLES_DEPTH - level - 1));
    return shift & ((1ULL << k) - 1);
}



uint64_t get_page_number_from_frame_with_map(uint64_t frame, const uint64_t parent_of[]) {
    uint64_t page_number = 0;
    uint64_t current_frame = frame;

    for (int level = TABLES_DEPTH - 1; level >= 0; --level) {
        uint64_t parent = parent_of[current_frame];

        word_t value;
        bool found = false;
        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            PMread(parent * PAGE_SIZE + index, &value);
            if ((uint64_t)value == current_frame) {
                page_number |= (index << (level * LEVEL_BITS));
                found = true;
                break;
            }
        }

        if (!found) {
            // std::cerr << "[BUG] Could not find child frame " << current_frame
            //           << " in parent " << parent << std::endl;
            break; // fail-safe
        }

        current_frame = parent;
    }

    return page_number;
}



//הערה לניר - בגלל שאסור משתנים גלובלים אז נראה שאין דרך יעילה יותר, גם לפי הג'יפיטי
uint64_t get_page_number_from_frame(uint64_t frame) {
    uint64_t page_number = 0;
    uint64_t current_frame = frame;

    for (int level = TABLES_DEPTH - 1; level >= 0; --level) {
        Pair pair = find_parent_and_index(current_frame); 
        uint64_t parent_frame = pair.first;
        uint64_t index = pair.second;

        page_number |= (index << (level * LEVEL_BITS)); 
        
        current_frame = parent_frame; 
    }

    return page_number;
}