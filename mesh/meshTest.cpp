#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <metis.h>
#include <iostream>

#include <fstream>

#include "Mesh.h"
#include <chrono>

int main(){
    // Start the timer
    auto start_time = std::chrono::high_resolution_clock::now();

    //TriMesh * mesh = new TriMesh();
    ////Mesh * mesh = new Mesh("../../assets/models/obj/teapot.obj");
    //if (!OpenMesh::IO::read_mesh(*mesh, "../../assets/models/obj/dragon.obj")) {
    //    std::cerr << "Error loading mesh." << std::endl;
    //    return 1;
    //}

    Mesh * mesh = new Mesh("../../assets/models/obj/dragon.obj");

    // Stop the timer
    auto end_time = std::chrono::high_resolution_clock::now();
    // Calculate the duration in seconds
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Time taken: " << duration.count() << " seconds" << std::endl;

    delete mesh;
    return 0;
}