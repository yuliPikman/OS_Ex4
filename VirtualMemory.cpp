#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <algorithm>
#include <iostream>



#define LEVEL_BITS ((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / TABLES_DEPTH)


// Forward declarations
uint64_t find_unused_frame_or_evict(uint64_t page_to_insert);
std::pair<uint64_t, uint64_t> find_parent_and_index(uint64_t child_frame);
bool initialize_new_frame(uint64_t &currentFrame, uint64_t virtualAddress, int level, uint64_t index);
uint64_t get_index_in_level(uint64_t virtualAddress, int level);



uint64_t get_page_number(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}


uint64_t get_offset(uint64_t virtualAddress) {
    return virtualAddress & ((1ULL << OFFSET_WIDTH) - 1);
}


void update_page_mapping(uint64_t frame, uint64_t page_number) {
    PMwrite(frame * PAGE_SIZE, page_number);
}

uint64_t get_page_mapping(uint64_t frame) {
    word_t page_number = 0;
    PMread(frame * PAGE_SIZE, &page_number);
    return page_number;
}

bool is_leaf_frame(uint64_t frame) {
    if (frame == 0) return false; // לא מפנים root
    word_t value;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frame * PAGE_SIZE + i, &value);
        if (value != 0 && value < NUM_FRAMES) {
            return false;  // מצביע לפריים אחר → טבלת עמודים
        }
    }
    return true;  // לא מצביע לכלום → עלה
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
        
        uint64_t addr = currentFrame * PAGE_SIZE + index;
        if (addr >= RAM_SIZE) return false;
        PMread(addr, &nextFrame);
                
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
    if(!traverse_tree(virtualAddress, foundFrame)) {
        return 0;
    }
    
    uint64_t offset = get_offset(virtualAddress);
    uint64_t addr = foundFrame * PAGE_SIZE + offset;
    if (addr >= RAM_SIZE) return 0;
    PMwrite(addr, value);
    
    return 1;
}


bool is_frame_in_use(uint64_t frame_to_check, uint64_t frames_in_tree[], uint64_t num_frames_in_tree) {
    for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
        uint64_t parent_frame = frames_in_tree[i];

        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            word_t value;
            if (parent_frame >= NUM_FRAMES) {
                return false;
            }
            uint64_t addr = parent_frame * PAGE_SIZE + index;
            if (addr >= RAM_SIZE) return false;
            PMread(addr, &value);


            if ((uint64_t) value == frame_to_check) {
                return true;
            }
        }
    }
    return false;
}


bool initialize_new_frame(uint64_t &currentFrame, uint64_t virtualAddress, int level, uint64_t index) {
    uint64_t newFrame = find_unused_frame_or_evict(get_page_number(virtualAddress));
    if (newFrame == 0) {
        return false;
    }

    if (level == TABLES_DEPTH - 1) {
        // אנחנו מעלים עמוד מהדיסק
        uint64_t page_number = get_page_number(virtualAddress);

        // טוענים את תוכן העמוד מהדיסק לזיכרון
        PMrestore(newFrame, page_number);

    } else {
        // זה פריים של טבלת עמודים – נאתחל ל-0
        for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
            uint64_t addr = newFrame * PAGE_SIZE + i;
            if (addr >= RAM_SIZE) return false;
            PMwrite(addr, 0);
        }
    }

    // רושמים בטבלה שמצביעה לפריים החדש
    uint64_t addr = currentFrame * PAGE_SIZE + index;
    if (addr >= RAM_SIZE) return false;
    PMwrite(addr, newFrame);
    currentFrame = newFrame;

    return true;
}


uint64_t get_page_number_from_frame_with_map(uint64_t frame, const uint64_t parent_of[]) {
    uint64_t page_number = 0;
    uint64_t current_frame = frame;

    for (int level = TABLES_DEPTH - 1; level >= 0 && current_frame != 0; --level) {
        uint64_t parent = parent_of[current_frame];

        // סרוק את טבלת ההורה כדי למצוא באיזה אינדקס הוא מצביע ל־current_frame
        word_t value;
        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            PMread(parent * PAGE_SIZE + index, &value);
            if ((uint64_t)value == current_frame) {
                page_number |= (index << (level * LEVEL_BITS));
                break;
            }
        }

        current_frame = parent;
    }

    return page_number;
}



void scan_tree(uint64_t currentFrame,
               uint64_t frames_in_tree[],
               uint64_t& num_frames_in_tree,
               uint64_t parent_of[])
{
    if (currentFrame >= NUM_FRAMES) return;

    frames_in_tree[num_frames_in_tree++] = currentFrame;

    for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
        word_t value;
        uint64_t physicalAddress = currentFrame * PAGE_SIZE + index;
        if (physicalAddress >= RAM_SIZE) continue;

        PMread(physicalAddress, &value);

        if (value != 0 && (uint64_t)value < NUM_FRAMES) {
            bool already_in_tree = false;
            for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
                if (frames_in_tree[i] == (uint64_t)value) {
                    already_in_tree = true;
                    break;
                }
            }

            if (!already_in_tree) {
                parent_of[(uint64_t)value] = currentFrame;
                scan_tree((uint64_t)value, frames_in_tree, num_frames_in_tree, parent_of);
            }
        }
    }
}






void update_distance_for_evict(uint64_t frame, uint64_t page_to_insert,
                               uint64_t& max_distance, uint64_t& frame_to_evict,
                               uint64_t& parent_frame_of_candidate, uint64_t& index_in_parent) {
    if (!is_leaf_frame(frame)) {
        return;  // לא נרצה לפנות טבלאות עמודים
    }

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

    // סריקת העץ
    uint64_t frames_in_tree[NUM_FRAMES];
    uint64_t parent_of[NUM_FRAMES];
    for (uint64_t i = 0; i < NUM_FRAMES; ++i) {
        parent_of[i] = 0;
    }
    uint64_t num_frames_in_tree = 0;
    scan_tree(0, frames_in_tree, num_frames_in_tree, parent_of);

    for (uint64_t frame = 1; frame < NUM_FRAMES; frame++) {
        bool in_use = is_frame_in_use(frame, frames_in_tree, num_frames_in_tree);
        if (!in_use) {
            return frame;  // פריים פנוי
        }

        if (frame > max_frame_index) {
            max_frame_index = frame;
        }

        if (is_leaf_frame(frame)) {
            // נחשב את מספר העמוד בעזרת המפה
            uint64_t page = get_page_number_from_frame_with_map(frame, parent_of);

            uint64_t diff = (page_to_insert > page) ? (page_to_insert - page) : (page - page_to_insert);
            uint64_t distance = (NUM_PAGES - diff < diff) ? (NUM_PAGES - diff) : diff;

            if (distance > max_distance) {
                max_distance = distance;
                frame_to_evict = frame;

                // נמצא את ההורה והאינדקס (כמו קודם)
                for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
                    word_t val;
                    PMread(parent_of[frame] * PAGE_SIZE + index, &val);
                    if ((uint64_t)val == frame) {
                        parent_frame_of_candidate = parent_of[frame];
                        index_in_parent = index;
                        break;
                    }
                }
            }
        }
    }

    // הקצאת פריים חדש אם אפשר
    if (max_frame_index + 1 < NUM_FRAMES) {
        return max_frame_index + 1;
    }

    // אם אין מועמד חוקי
    if (frame_to_evict == 0) {
        return 0;
    }

    // פינוי
    uint64_t evicted_page_number = get_page_number_from_frame_with_map(frame_to_evict, parent_of);
    PMevict(frame_to_evict, evicted_page_number);
    PMwrite(parent_frame_of_candidate * PAGE_SIZE + index_in_parent, 0);

    return frame_to_evict;
}





std::pair<uint64_t, uint64_t> find_parent_and_index(uint64_t child_frame) {
    for (uint64_t frame = 0; frame < NUM_FRAMES; ++frame) {
        if (frame >= NUM_FRAMES) {
            break;
        }
        for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
            word_t value;
            uint64_t addr = frame * PAGE_SIZE + index;
            if (addr >= RAM_SIZE) continue; // או return {0, 0}; אם אתה רוצה לעצור
            PMread(addr, &value);
            if ((uint64_t)value == child_frame) {
                return {frame, index}; 
            }
        }
    }

    return {0, 0}; 
}




