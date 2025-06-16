#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <algorithm>
#include <iostream>



#define LEVEL_BITS ((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / TABLES_DEPTH)

struct Pair {
    uint64_t first;
    uint64_t second;
};



// Forward declarations
uint64_t find_unused_frame_or_evict(uint64_t page_to_insert);
Pair find_parent_and_index(uint64_t child_frame);
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
        Pair pair = find_parent_and_index(current_frame); 
        uint64_t parent_frame = pair.first;
        uint64_t index = pair.second;

        page_number |= (index << (level * LEVEL_BITS)); 
        
        current_frame = parent_frame; 
    }

    return page_number;
}


bool traverse_tree(uint64_t virtualAddress, uint64_t &frame_found) {
    uint64_t currentFrame = 0;

    for (int level = 0; level < TABLES_DEPTH; ++level) {
        uint64_t index = get_index_in_level(virtualAddress, level);
        word_t nextFrame = 0;
        PMread(currentFrame * PAGE_SIZE + index, &nextFrame);

        if (nextFrame == 0) {
            uint64_t newFrame = find_unused_frame_or_evict(get_page_number(virtualAddress));
            if (newFrame == 0) {
                return false;
            }

            if (level == TABLES_DEPTH - 1) {
                uint64_t page_number = get_page_number(virtualAddress);
                // std::cerr << "[RESTORE] Restoring page " << page_number
                //           << " to frame " << newFrame << std::endl;
                PMrestore(newFrame, page_number);
            } else {
                for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
                    
                    PMwrite(newFrame * PAGE_SIZE + i, 0);
                }
            }

            // עדכן את טבלת האב בפריים החדש
            PMwrite(currentFrame * PAGE_SIZE + index, newFrame);
            currentFrame = newFrame;
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
    // std::cerr << "[READ] virtualAddress=" << virtualAddress 
    //       << " page=" << get_page_number(virtualAddress) 
    //       << " foundFrame=" << foundFrame << std::endl;

    
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


bool is_frame_in_use(uint64_t frame_to_check,
                     const uint64_t frames_in_tree[],
                     uint64_t num_frames_in_tree,
                     const uint64_t parent_depth[])   // ← נשמור עומק של כל פריים
{
    for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
        uint64_t parent = frames_in_tree[i];

        /*  מדלגים על דפי-מידע  */
        if (parent_depth[parent] == TABLES_DEPTH) continue;

        for (uint64_t idx = 0; idx < PAGE_SIZE; ++idx) {
            word_t val;
            PMread(parent * PAGE_SIZE + idx, &val);
            if ((uint64_t)val == frame_to_check) {
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

    // עדכון תמידי של הקישור בטבלת האב

    PMwrite(currentFrame * PAGE_SIZE + index, newFrame);

    // עדכון currentFrame תמיד (גם אם זה leaf)
    currentFrame = newFrame;

    if (level == TABLES_DEPTH - 1) {
        uint64_t page_number = get_page_number(virtualAddress);
        // std::cerr << "[RESTORE] Restoring page " << page_number
        //           << " to frame " << newFrame << std::endl;
        PMrestore(newFrame, page_number);
    } else {
        // רק אם זו טבלת עמודים – אפס אותה
        for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
            PMwrite(newFrame * PAGE_SIZE + i, 0);
        }
    }

    return true;
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


void scan_tree(uint64_t currentFrame,
               uint64_t depth,
               uint64_t frames_in_tree[],
               uint64_t &num_frames_in_tree,
               uint64_t parent_of[],
               uint64_t page_of[],
               uint64_t depth_of[],
               uint64_t path = 0)
{
    if (currentFrame >= NUM_FRAMES) return;

    frames_in_tree[num_frames_in_tree++] = currentFrame;
    page_of[currentFrame] = path;
    depth_of[currentFrame] = depth;

    if (depth == TABLES_DEPTH) return;

    for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
        word_t child;
        PMread(currentFrame * PAGE_SIZE + index, &child);
        if (child != 0 && (uint64_t)child < NUM_FRAMES) {
            parent_of[(uint64_t)child] = currentFrame;
            scan_tree((uint64_t)child, depth + 1,
                      frames_in_tree, num_frames_in_tree,
                      parent_of, page_of, depth_of,
                      path | (index << ((TABLES_DEPTH - 1 - depth) * LEVEL_BITS)));
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

        Pair parent_index = find_parent_and_index(frame);
        parent_frame_of_candidate = parent_index.first;
        index_in_parent = parent_index.second;
    }
}


uint64_t find_unused_frame_or_evict(uint64_t page_to_insert) {
    uint64_t frames_in_tree[NUM_FRAMES];
    uint64_t parent_of[NUM_FRAMES] = {0};
    uint64_t page_of[NUM_FRAMES] = {0};
    uint64_t depth_of[NUM_FRAMES] = {0};
    uint64_t num_frames_in_tree = 0;

    scan_tree(0, 0, frames_in_tree, num_frames_in_tree, parent_of, page_of, depth_of);

    uint64_t max_distance = 0;
    uint64_t frame_to_evict = 0;
    uint64_t parent_frame_of_candidate = 0;
    uint64_t index_in_parent = 0;
    uint64_t max_frame_index = 0;

    for (uint64_t i = 1; i < NUM_FRAMES; ++i) {
        // בדוק אם הפריים בשימוש
        bool in_use = false;
        for (uint64_t j = 0; j < num_frames_in_tree; ++j) {
            if (frames_in_tree[j] == i) {
                in_use = true;
                break;
            }
        }

        if (!in_use) {
            return i;  // פריים פנוי
        }

        if (i > max_frame_index) {
            max_frame_index = i;
        }

        // נבדוק אם זה leaf
        if (depth_of[i] == TABLES_DEPTH) {
            uint64_t page = page_of[i];
            uint64_t diff = (page > page_to_insert) ? (page - page_to_insert) : (page_to_insert - page);
            uint64_t distance = (NUM_PAGES - diff < diff) ? (NUM_PAGES - diff) : diff;

            if (distance > max_distance) {
                max_distance = distance;
                frame_to_evict = i;

                // מצא את האינדקס בטבלת האב
                uint64_t parent = parent_of[i];
                for (uint64_t j = 0; j < PAGE_SIZE; ++j) {
                    word_t val;
                    PMread(parent * PAGE_SIZE + j, &val);
                    if ((uint64_t)val == i) {
                        parent_frame_of_candidate = parent;
                        index_in_parent = j;
                        break;
                    }
                }
            }
        }
    }

    if (max_frame_index + 1 < NUM_FRAMES) {
        return max_frame_index + 1;
    }

    if (frame_to_evict == 0) {
        return 0;  // מקרה קצה
    }

    uint64_t evicted_page = page_of[frame_to_evict];
    // std::cerr << "[EVICT] Evicting page " << evicted_page
    //           << " from frame " << frame_to_evict << std::endl;

    PMevict(frame_to_evict, evicted_page);
    PMwrite(parent_frame_of_candidate * PAGE_SIZE + index_in_parent, 0);

    return frame_to_evict;
}

Pair find_parent_and_index(uint64_t child_frame) {
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




