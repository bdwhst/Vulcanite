#include "Cluster.h"
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <metis.h>
typedef OpenMesh::TriMesh_ArrayKernelT<>  MyMesh;


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


    // 定义图的结构
    idx_t nvtxs = 6; // 顶点数量
    idx_t ncon = 1; // 权重的数量
    idx_t xadj[7] = { 0, 2, 5, 7, 9, 12, 14 }; // 每个顶点的邻接顶点索引开始位置
    idx_t adjncy[14] = { 1, 3, 0, 2, 4, 1, 3, 2, 4, 5, 1, 2, 5, 3 }; // 邻接顶点的索引

    // 分区参数
    idx_t nparts = 2; // 想要分成的区域数量
    idx_t objval; // 分区后边的切割数量
    idx_t part[6]; // 存储每个顶点的区域索引

    // 调用METIS进行分区
    int ret = METIS_PartGraphKway(&nvtxs, &ncon, xadj, adjncy,
        NULL, NULL, NULL, &nparts,
        NULL, NULL, NULL, &objval, part);

    // 检查是否成功
    if (ret == METIS_OK) {
        printf("Partitioning succeeded!\n");
        printf("Edge cut: %d\n", objval);
        printf("Partitions: ");
        for (idx_t i = 0; i < nvtxs; i++) {
            printf("%d ", part[i]);
        }
        printf("\n");
    }
    else {
        printf("Partitioning failed!\n");
    }

    return 0;
}