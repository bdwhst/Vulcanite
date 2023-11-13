#include "utils.h"

void getTriangleAABB(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, glm::vec3& pMin, glm::vec3& pMax) {
	pMin = glm::min(p0, glm::min(p1, p2));
	pMax = glm::max(p0, glm::max(p1, p2));
}