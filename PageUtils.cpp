#include "PageUtils.h"
#include "TreeUtils.h"
#include <iostream>

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



uint64_t get_index_in_parent(uint64_t parent, uint64_t child) {
    word_t value;
    for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
        PMread(parent * PAGE_SIZE + index, &value);
        if ((uint64_t)value == child) {
            return index;
        }
    }

    // כישלון – לא נמצא (צריך לוודא שקלטים נכונים)
    return PAGE_SIZE; // ערך בלתי אפשרי שמציין "לא נמצא"
}

uint64_t get_page_number_from_frame_with_map(uint64_t frame, const uint64_t parent_of[]) {
    uint64_t page_number = 0;
    uint64_t current_frame = frame;

    for (int level = TABLES_DEPTH - 1; level >= 0; --level) {
        uint64_t parent = parent_of[current_frame];
        std::cout << "[DEBUG] Level: " << level << ", Current Frame: " << current_frame
                  << ", Parent: " << parent << std::endl;
        uint64_t index = get_index_in_parent(parent, current_frame);
        
        if (index == PAGE_SIZE) {
            std::cerr << "[BUG] get_page_number_from_frame_with_map: could not find frame "
                      << current_frame << " in parent " << parent << std::endl;
            return NUM_PAGES;  // ערך לא חוקי
        }

        word_t val;
        PMread(parent * PAGE_SIZE + index, &val);
        if ((uint64_t)val != current_frame) {
            std::cerr << "[MISMATCH] parent_of[" << current_frame << "] = " << parent
                      << ", but PMread(" << parent * PAGE_SIZE + index << ") = " << val
                      << std::endl;
        }

        page_number |= (index << (level * LEVEL_BITS));
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