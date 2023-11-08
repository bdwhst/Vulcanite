#pragma once
#include <iostream>
#include <cassert>

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion failed: " << #condition << "\n" \
                      << "File: " << __FILE__ << "\n" \
                      << "Line: " << __LINE__ << "\n" \
                      << "Message: " << message << "\n"; \
            std::abort(); \
        } \
    } while (0)
