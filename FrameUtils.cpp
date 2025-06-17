#include "FrameUtils.h"
#include "TreeUtils.h"
#include "PageUtils.h"
#include <iostream>




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



uint64_t find_unused_frame_or_evict(uint64_t page_to_insert,
                                    const uint64_t frames_in_tree[],
                                    uint64_t num_frames_in_tree,
                                    const uint64_t parent_of[],
                                    const uint64_t page_of[],
                                    const uint64_t depth_of[]) {
    // ננסה קודם למצוא מסגרת פנויה
    int free = find_free_frame(frames_in_tree, num_frames_in_tree);
    if (free != 0) return free;

    // אם אין, ננסה למצוא מסגרת לא בשימוש בטבלאות (אך לא בהכרח ריקה)
    int next = find_next_unused_frame(frames_in_tree, num_frames_in_tree);
    if (next != 0) return next;

    // אם אין, נדרש לפנות מסגרת
    EvictionCandidate candidate = evict_best_frame(page_to_insert,
                                                   frames_in_tree,
                                                   num_frames_in_tree,
                                                   parent_of,
                                                   page_of,
                                                   depth_of);

    uint64_t frame = perform_eviction(candidate, page_of);
    reset_frame(frame);  // איפוס אחרי פינוי

    return frame;
}




int find_free_frame(const uint64_t frames_in_tree[], uint64_t num_frames_in_tree) {
    for (uint64_t i = 1; i < NUM_FRAMES; ++i) {
        bool in_use = false;
        for (uint64_t j = 0; j < num_frames_in_tree; ++j) {
            if (frames_in_tree[j] == i) {
                in_use = true;
                break;
            }
        }
        if (!in_use) return i;
    }
    return 0;
}

int find_next_unused_frame(const uint64_t frames_in_tree[], uint64_t num_frames_in_tree) {
    uint64_t max_frame_index = 0;
    for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
        if (frames_in_tree[i] > max_frame_index) {
            max_frame_index = frames_in_tree[i];
        }
    }
    if (max_frame_index + 1 < NUM_FRAMES) {
        return max_frame_index + 1;
    }
    return 0;
}






void update_distance_for_evict(uint64_t frame, uint64_t page_to_insert,
                               uint64_t& max_distance, uint64_t& frame_to_evict,
                               uint64_t& parent_frame_of_candidate, uint64_t& index_in_parent,
                               const uint64_t parent_of[]) {
    uint64_t page = get_page_number_from_frame_with_map(frame, parent_of);

    uint64_t diff = (page > page_to_insert) ? (page - page_to_insert) : (page_to_insert - page);
    uint64_t distance = (diff < NUM_PAGES - diff) ? diff : (NUM_PAGES - diff);

    std::cout << "Frame: " << frame << ", page: " << page << ", page_to_insert: " << page_to_insert
              << ", diff: " << diff << ", distance: " << distance << ", max_distance: " << max_distance << std::endl;

    if (distance >= max_distance) {
        max_distance = distance;
        frame_to_evict = frame;

        parent_frame_of_candidate = parent_of[frame];
        index_in_parent = get_index_in_parent(parent_frame_of_candidate, frame);

        std::cout << "Updated eviction candidate: frame_to_evict = " << frame_to_evict << std::endl;
    }
}




EvictionCandidate evict_best_frame(uint64_t page_to_insert,
                                   const uint64_t frames_in_tree[],
                                   uint64_t num_frames_in_tree,
                                   const uint64_t parent_of[],
                                   const uint64_t page_of[],
                                   const uint64_t depth_of[]) {
    uint64_t max_distance = 0;
    uint64_t frame_to_evict = 0;
    uint64_t parent_frame_of_candidate = 0;
    uint64_t index_in_parent = 0;

    for (uint64_t i = 0; i < num_frames_in_tree; ++i) {
        uint64_t frame = frames_in_tree[i];
        if (frame == 0) continue; // אסור לפנות את השורש

        if (depth_of[frame] == TABLES_DEPTH) {
            update_distance_for_evict(frame, page_to_insert,
                                      max_distance, frame_to_evict,
                                      parent_frame_of_candidate, index_in_parent,
                                      parent_of);
            std::cout << "Frame to evict in evict_best_frame (after): " << frame_to_evict << std::endl;
        }
    }

    if (frame_to_evict == 0) {
        std::cerr << "[ERROR] No valid frame found for eviction!\n";
        exit(1);
    }

    return {frame_to_evict, parent_frame_of_candidate, index_in_parent};
}






// EvictionCandidate find_best_frame_to_evict(uint64_t page_to_insert,
//                                            const uint64_t parent_of[],
//                                            const uint64_t page_of[],
//                                            const uint64_t depth_of[]) {
//     uint64_t max_distance = 0;
//     EvictionCandidate candidate = {0, 0, 0};

//     for (uint64_t i = 1; i < NUM_FRAMES; ++i) {
//         if (depth_of[i] == TABLES_DEPTH) {
//             uint64_t page = page_of[i];
//             uint64_t diff = (page > page_to_insert) ? (page - page_to_insert) : (page_to_insert - page);
//             uint64_t distance = (diff < NUM_PAGES - diff) ? diff : (NUM_PAGES - diff);

//             if (distance > max_distance) {
//                 max_distance = distance;
//                 candidate.frame = i;
//                 candidate.parent = parent_of[i];
//             }
//         }
//     }

//     if (candidate.frame != 0 && candidate.parent != 0) {
//         for (uint64_t j = 0; j < PAGE_SIZE; ++j) {
//             word_t val;
//             PMread(candidate.parent * PAGE_SIZE + j, &val);
//             if ((uint64_t)val == candidate.frame) {
//                 candidate.index_in_parent = j;
//                 break;
//             }
//         }
//     }
    

//     return candidate;
// }

uint64_t perform_eviction(const EvictionCandidate& candidate,
                          const uint64_t page_of[]) {
    if (candidate.frame == 0) return 0;
    PMevict(candidate.frame, page_of[candidate.frame]);
    PMwrite(candidate.parent * PAGE_SIZE + candidate.index_in_parent, 0);
    return candidate.frame;
}