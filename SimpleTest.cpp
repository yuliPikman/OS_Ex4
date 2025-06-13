#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>
#include <iostream>

int main(int argc, char **argv) {
    VMinitialize();
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        printf("writing to %llu\n", (long long int) i);
        VMwrite(5 * i * PAGE_SIZE, i);
    }

    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        word_t value;
        VMread(5 * i * PAGE_SIZE, &value);
        printf("reading from %llu %d\n", (long long int) i, value);
        std::cout << uint64_t(value) << std::endl;        
        std::cout << i << std::endl;
        assert(uint64_t(value) == i);
    }
    printf("success\n");

    return 0;
}
