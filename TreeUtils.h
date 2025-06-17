#pragma once

#include "PhysicalMemory.h"

struct Pair {
    uint64_t first;
    uint64_t second;
};


void reset_frame(uint64_t frame);



bool handle_missing_entry(uint64_t virtualAddress, uint64_t currentFrame,
                          int level, uint64_t index);


bool traverse_tree(uint64_t virtualAddress, uint64_t &frame_found);


void process_current_frame(uint64_t currentFrame,
                           uint64_t depth,
                           uint64_t frames_in_tree[],
                           uint64_t &num_frames_in_tree,
                           uint64_t page_of[],
                           uint64_t depth_of[],
                           uint64_t path);


void scan_tree(uint64_t currentFrame,
               uint64_t depth,
               uint64_t frames_in_tree[],
               uint64_t &num_frames_in_tree,
               uint64_t parent_of[],
               uint64_t page_of[],
               uint64_t depth_of[],
               uint64_t path = 0);



bool initialize_new_frame(uint64_t &currentFrame, uint64_t virtualAddress, int level, uint64_t index);



Pair find_parent_and_index(uint64_t child_frame);