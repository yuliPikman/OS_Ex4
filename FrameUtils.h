#pragma once
#include "PhysicalMemory.h"




bool is_leaf_frame(uint64_t frame);


uint64_t find_unused_frame_or_evict(uint64_t page_to_insert);