#include "Cluster.h"
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <metis.h>
typedef OpenMesh::TriMesh_ArrayKernelT<>  MyMesh;


#include <iostream>

#include <fstream>


int main(){
    MyMesh mesh;

    // ��Ӷ���
    MyMesh::VertexHandle vhandle[3];
    vhandle[0] = mesh.add_vertex(MyMesh::Point(-1, -1, 0));
    vhandle[1] = mesh.add_vertex(MyMesh::Point(1, -1, 0));
    vhandle[2] = mesh.add_vertex(MyMesh::Point(0, 1, 0));

    // ���һ����
    std::vector<MyMesh::VertexHandle>  face_vhandles;
    face_vhandles.clear();
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[2]);
    mesh.add_face(face_vhandles);

    // �������ж���
    for (MyMesh::VertexIter v_it = mesh.vertices_begin(); v_it != mesh.vertices_end(); ++v_it)
    {
        MyMesh::Point point = mesh.point(*v_it);
        std::cout << "Vertex: " << *v_it << "  Point: " << point << std::endl;
    }


    // ����ͼ�Ľṹ
    idx_t nvtxs = 6; // ��������
    idx_t ncon = 1; // Ȩ�ص�����
    idx_t xadj[7] = { 0, 2, 5, 7, 9, 12, 14 }; // ÿ��������ڽӶ���������ʼλ��
    idx_t adjncy[14] = { 1, 3, 0, 2, 4, 1, 3, 2, 4, 5, 1, 2, 5, 3 }; // �ڽӶ��������

    // ��������
    idx_t nparts = 2; // ��Ҫ�ֳɵ���������
    idx_t objval; // ������ߵ��и�����
    idx_t part[6]; // �洢ÿ���������������

    // ����METIS���з���
    int ret = METIS_PartGraphKway(&nvtxs, &ncon, xadj, adjncy,
        NULL, NULL, NULL, &nparts,
        NULL, NULL, NULL, &objval, part);

    // ����Ƿ�ɹ�
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