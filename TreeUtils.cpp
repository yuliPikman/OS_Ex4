#include "PhysicalMemory.h"
#include <iostream>
#include "TreeUtils.h"
#include "PageUtils.h"
#include "FrameUtils.h"

void reset_frame(uint64_t frame) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame * PAGE_SIZE + i, 0);
    }
}


bool handle_missing_entry(uint64_t virtualAddress, uint64_t currentFrame,
                          int level, uint64_t index) {
    uint64_t newFrame = find_unused_frame_or_evict(get_page_number(virtualAddress));
    std::cout << "Allocated frame " << newFrame << " at level " << level << " for VA " << virtualAddress << std::endl;
    
    if (newFrame == 0) {
        return false;
    }

    reset_frame(newFrame);

    if (level == TABLES_DEPTH - 1) {
        PMrestore(newFrame, get_page_number(virtualAddress));
    }

    PMwrite(currentFrame * PAGE_SIZE + index, newFrame);
    return true;
}



bool traverse_tree(uint64_t virtualAddress, uint64_t &frame_found) {
    uint64_t currentFrame = 0;

    for (int level = 0; level < TABLES_DEPTH; ++level) {
        uint64_t index = get_index_in_level(virtualAddress, level);
        word_t nextFrameWord = 0;
        PMread(currentFrame * PAGE_SIZE + index, &nextFrameWord);
        uint64_t nextFrame = static_cast<uint64_t>(nextFrameWord);

        if (nextFrame == 0) {
            if (!handle_missing_entry(virtualAddress, currentFrame, level, index)) {
                return false;
            }
                // ⬇️ קרא מחדש את מה שהוקצה
            PMread(currentFrame * PAGE_SIZE + index, &nextFrameWord);
            nextFrame = static_cast<uint64_t>(nextFrameWord);
        }
        currentFrame = nextFrame;
    }

    frame_found = currentFrame;
    return true;
}




void process_current_frame(uint64_t currentFrame,
                           uint64_t depth,
                           uint64_t frames_in_tree[],
                           uint64_t &num_frames_in_tree,
                           uint64_t page_of[],
                           uint64_t depth_of[],
                           uint64_t path)
{
    frames_in_tree[num_frames_in_tree++] = currentFrame;
    page_of[currentFrame] = path;
    depth_of[currentFrame] = depth;
}

void scan_tree(uint64_t currentFrame,
               uint64_t depth,
               uint64_t frames_in_tree[],
               uint64_t &num_frames_in_tree,
               uint64_t parent_of[],
               uint64_t page_of[],
               uint64_t depth_of[],
               uint64_t path)
{
    if (currentFrame >= NUM_FRAMES) return;

    process_current_frame(currentFrame, depth, frames_in_tree, num_frames_in_tree, page_of, depth_of, path);

    if (depth == TABLES_DEPTH) return;

    for (uint64_t index = 0; index < PAGE_SIZE; ++index) {
        word_t child;
        PMread(currentFrame * PAGE_SIZE + index, &child);
        if (child != 0 && (uint64_t)child < NUM_FRAMES) {
            parent_of[(uint64_t)child] = currentFrame;
            uint64_t new_path = path | (index << ((TABLES_DEPTH - 1 - depth) * LEVEL_BITS));
            scan_tree((uint64_t)child, depth + 1,
                      frames_in_tree, num_frames_in_tree,
                      parent_of, page_of, depth_of, new_path);
        }
    }
}



bool initialize_new_frame(uint64_t &currentFrame, uint64_t virtualAddress, int level, uint64_t index) {
    uint64_t newFrame = find_unused_frame_or_evict(get_page_number(virtualAddress));
    if (newFrame == 0) {
        return false;
    }


    PMwrite(currentFrame * PAGE_SIZE + index, newFrame);

    currentFrame = newFrame;

    if (level == TABLES_DEPTH - 1) {
        uint64_t page_number = get_page_number(virtualAddress);
        PMrestore(newFrame, page_number);
    } else {
        for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
            PMwrite(newFrame * PAGE_SIZE + i, 0);
        }
    }

    return true;
}



Pair find_parent_and_index(uint64_t child_frame) {
    for (uint64_t frame = 0; frame < NUM_FRAMES; ++frame) {
        // if (frame >= NUM_FRAMES) {
        //     break;
        // }
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
