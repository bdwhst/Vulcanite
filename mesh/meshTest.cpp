#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <metis.h>
#include <iostream>

#include <tinygltf/tiny_gltf.h>

#include <fstream>

#include "Mesh.h"
#include <chrono>

int main(){
    // Start the timer
    auto start_time = std::chrono::high_resolution_clock::now();

    //TriMesh * mesh = new TriMesh();
    //Mesh * mesh = new Mesh("../../assets/models/obj/teapot.obj");
    //if (!OpenMesh::IO::read_mesh(*mesh, "../../assets/models/obj/bunny.obj")) {
    //    std::cerr << "Error loading mesh." << std::endl;
    //    return 1;
    //}

    //TriMesh::FaceHandle fh;  // Replace with your actual FaceHandle
    //for (TriMesh::FaceIter f_it = mesh->faces_begin(); f_it != mesh->faces_end(); ++f_it) {
    //    TriMesh::FaceHandle fh = *f_it;
    //    // Access and manipulate the face using fh
    //}
    //Mesh* mesh = new Mesh("../../assets/models/obj/dragon.obj");
    //Mesh * mesh = new Mesh("../../assets/models/teapot.gltf");

    // Stop the timer
    auto end_time = std::chrono::high_resolution_clock::now();
    // Calculate the duration in seconds
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Time taken: " << duration.count() << " seconds" << std::endl;

    start_time = std::chrono::high_resolution_clock::now();

    // Stop the timer
    end_time = std::chrono::high_resolution_clock::now();
    // Calculate the duration in seconds
    duration = end_time - start_time;
    std::cout << "Time taken: " << duration.count() << " seconds" << std::endl;
    return 0;
}