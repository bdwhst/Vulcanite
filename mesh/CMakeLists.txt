set(headers
    "Cluster.h"
    "MeshHandler.h"
    "Mesh.h"
    "utils.h"
)

set(sources
    "meshTest.cpp"
    "Cluster.cpp"
    "MeshHandler.cpp"
    "Mesh.cpp"
)

list(SORT headers)
list(SORT sources)

source_group(Headers FILES ${headers})
source_group(Sources FILES ${sources})

#include_directories(external/OpenMesh/src)
#include_directories(external/metis/include)
find_package(metis CONFIG REQUIRED)
find_package(openmesh REQUIRED)
find_package(assimp CONFIG REQUIRED)

set(APPLICATION_NAME mesh)

# Set the path to OpenMesh (change this to your actual path)
# set(OpenMesh_DIR "../external/OpenMesh/build")

find_package(OpenMesh REQUIRED)

add_executable(${APPLICATION_NAME} ${sources} ${headers})
target_include_directories(${APPLICATION_NAME} PRIVATE ${OPENMESH_INCLUDE_DIRS})
target_link_libraries(${APPLICATION_NAME} 
PRIVATE assimp::assimp
  ${OPENMESH_LIBRARIES}
  metis
)