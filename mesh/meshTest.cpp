#include "Cluster.h"
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <metis.h>
typedef OpenMesh::TriMesh_ArrayKernelT<>  MyMesh;

//#include <metis.h>

#include <iostream>

#include <fstream>


int main(){
    MyMesh mesh;

    // 添加顶点
    MyMesh::VertexHandle vhandle[3];
    vhandle[0] = mesh.add_vertex(MyMesh::Point(-1, -1, 0));
    vhandle[1] = mesh.add_vertex(MyMesh::Point(1, -1, 0));
    vhandle[2] = mesh.add_vertex(MyMesh::Point(0, 1, 0));

    // 添加一个面
    std::vector<MyMesh::VertexHandle>  face_vhandles;
    face_vhandles.clear();
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[2]);
    mesh.add_face(face_vhandles);

    // 遍历所有顶点
    for (MyMesh::VertexIter v_it = mesh.vertices_begin(); v_it != mesh.vertices_end(); ++v_it)
    {
        MyMesh::Point point = mesh.point(*v_it);
        std::cout << "Vertex: " << *v_it << "  Point: " << point << std::endl;
    }

    return 0;
}