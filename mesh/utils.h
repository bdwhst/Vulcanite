#pragma once
#include <iostream>
#include <cassert>
#include <glm/glm.hpp>

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

#define TEST(condition, message) \
    do { \
        if ((condition)) { \
            std::cerr << "Test Passed: " << message << " " << #condition << "\n"; \
        } \
        else {std::abort();} \
    } while (0)

#define LOG(message) \
	do { \
		std::cerr << message << "\n"; \
	} while (0)

void getTriangleAABB(const glm::vec3 & p0, const glm::vec3 & p1, const glm::vec3 & p2, glm::vec3 & pMin, glm::vec3 & pMax);
