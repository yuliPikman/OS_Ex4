#pragma once
#include "PhysicalMemory.h"


struct EvictionCandidate {
    uint64_t frame = -1;
    uint64_t parent = -1;
    uint64_t index_in_parent = 0;
};



bool is_leaf_frame(uint64_t frame);


uint64_t find_unused_frame_or_evict(uint64_t page_to_insert);




int find_free_frame(const uint64_t frames_in_tree[], uint64_t num_frames_in_tree);


int find_next_unused_frame(const uint64_t frames_in_tree[], uint64_t num_frames_in_tree);


uint64_t evict_best_frame(uint64_t page_to_insert,
                          const uint64_t frames_in_tree[],
                          uint64_t num_frames_in_tree,
                          const uint64_t parent_of[],
                          const uint64_t page_of[],
                          const uint64_t depth_of[]);



EvictionCandidate find_best_frame_to_evict(uint64_t page_to_insert,
                                           const uint64_t parent_of[],
                                           const uint64_t page_of[],
                                           const uint64_t depth_of[]);


uint64_t perform_eviction(const EvictionCandidate& candidate,
                          const uint64_t page_of[]);
