set(headers
    "Cluster.h"
    "ClusterGroup.h"
    "MeshHandler.h"
    "Mesh.h"
    "NaniteMesh.h"
    "Graph.h"
    "utils.h"
    "Instance.h"
)

set(sources
    "meshTest.cpp"
    "Cluster.cpp"
    "ClusterGroup.cpp"
    "Mesh.cpp"
    "MeshHandler.cpp"
    "NaniteMesh.cpp"
    "Graph.cpp"
    "utils.cpp"
    "Instance.cpp"
)

list(SORT headers)
list(SORT sources)

source_group(Headers FILES ${headers})
source_group(Sources FILES ${sources})

#include_directories(external/OpenMesh/src)
#include_directories(external/metis/include)
find_package(metis CONFIG REQUIRED)
find_package(OpenMesh REQUIRED)
find_package(assimp CONFIG REQUIRED)

set(APPLICATION_NAME mesh)

# Set the path to OpenMesh (change this to your actual path)
# set(OpenMesh_DIR "../external/OpenMesh/build")

# add_executable(${APPLICATION_NAME} ${sources} ${headers})

file(GLOB MESH_SRC ${sources} ${headers})
file(GLOB MESH_HEADERS ${headers})

add_library(${APPLICATION_NAME} STATIC ${MESH_SRC})

target_include_directories(${APPLICATION_NAME} PRIVATE ${OPENMESH_INCLUDE_DIRS})
target_link_libraries(${APPLICATION_NAME} 
PRIVATE assimp::assimp
  ${OPENMESH_LIBRARIES}
  metis
)