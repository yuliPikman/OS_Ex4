#include "PhysicalMemory.h"
#include <iostream>
#include "TreeUtils.h"
#include "PageUtils.h"
#include "FrameUtils.h"

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

                    // איפוס תמידי לפני השחזור כדי להימנע מערכים ישנים בזיכרון
                    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
                        PMwrite(newFrame * PAGE_SIZE + i, 0);
                    }

                    PMrestore(newFrame, page_number);
                }
                else {
                // איפוס טבלת עמודים חדשה
                for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
                    PMwrite(newFrame * PAGE_SIZE + i, 0);
                }
            }

            // עדכון הקישור בעץ
            PMwrite(currentFrame * PAGE_SIZE + index, newFrame);
            currentFrame = newFrame;
        } else {
            currentFrame = nextFrame;
        }
    }

    frame_found = currentFrame;
    return true;
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
