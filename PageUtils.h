#pragma once


#include "PhysicalMemory.h"


#define LEVEL_BITS ((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / TABLES_DEPTH)


uint64_t get_page_number(uint64_t virtualAddress);



uint64_t get_offset(uint64_t virtualAddress);



uint64_t get_page_mapping(uint64_t frame);



uint64_t get_index_in_level(uint64_t virtualAddress, int level);



uint64_t get_page_number_from_frame_with_map(uint64_t frame, const uint64_t parent_of[]);



uint64_t get_page_number_from_frame(uint64_t frame);