#include "FrameUtils.h"
#include "TreeUtils.h"




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

