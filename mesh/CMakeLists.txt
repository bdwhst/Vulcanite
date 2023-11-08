set(headers
    "Cluster.h"
    "MeshHandler.h"
)

set(sources
    "meshTest.cpp"
    "Cluster.cpp"
    "MeshHandler.cpp"
)

source_group(Headers FILES ${headers})
source_group(Sources FILES ${sources})

#include_directories(external/OpenMesh/src)
#include_directories(external/metis/include)
find_package(metis CONFIG REQUIRED)
find_package(openmesh REQUIRED)

set(APPLICATION_NAME mesh)

# Set the path to OpenMesh (change this to your actual path)
# set(OpenMesh_DIR "../external/OpenMesh/build")

find_package(OpenMesh REQUIRED)

add_executable(${APPLICATION_NAME} ${sources})
target_include_directories(${APPLICATION_NAME} PRIVATE ${OPENMESH_INCLUDE_DIRS})
target_link_libraries(${APPLICATION_NAME} 
  ${OPENMESH_LIBRARIES}
  metis
)