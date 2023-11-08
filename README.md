# Vulcanite

### Dependencies
- metis
- assimp
- OpenMesh

### Build

For now, we only test on Windows.

For windows users, we recommend that you use [vcpkg](https://github.com/microsoft/vcpkg) to install dependencies.

If you have successfully installed all dependencies through vcpkg, then this program should be able to run normally. 

TODO: Windows, not using vcpkg

TODO: Linux 

### TODOs

- [ ] CPU Side
	- [ ] Mesh simplification
		- [ ] Lock edge on cluster group boundaries
	- [ ] Cluster & Cluster group
		- [ ] Learn about METIS (or other available algorithms for triangle clustering)
		- [ ] Is forming cluster group exactly the same as forming cluster?
	- [ ] Core Algorithm
		Given cluster & cluster group
		- [ ] Mesh simplification (lock boundaries of cluster group)
		- [ ] Re-Cluster
		- [ ] Recalculate cluster groups
		- [ ] Maintain a LOD BVH Tree for each level
	- [ ] Nanite Mesh Exporter
		- [ ] Mesh LOD
		- [ ] BVH Tree
		- [ ] Data Compression
	
- [ ]  GPU Side
	- [ ] Runtime LOD
	
	- [ ] Soft ras
	
	- [ ] Hard ras
	
		- [ ] Mesh shader
	  
	- [ ] Customized depth test
	
	- [ ] Tile based deferred materials
	
	- [ ] Shadowmap culling


### Links

- [A Deep Dive into Nanite Virtualized Geometry - YouTube](https://www.youtube.com/watch?v=eviSykqSUUw)

- [A Macro View of Nanite – The Code Corsair (elopezr.com)](https://www.elopezr.com/a-macro-view-of-nanite/)

- [Mesh_shading_SIG2019.pptx (live.com)](https://view.officeapps.live.com/op/view.aspx?src=https%3A%2F%2Fadvances.realtimerendering.com%2Fs2019%2FMesh_shading_SIG2019.pptx&wdOrigin=BROWSELINK)

- [The Visibility Buffer (jcgt.org)](https://jcgt.org/published/0002/02/04/paper.pdf)

- [Journey to Nanite (highperformancegraphics.org)](https://www.highperformancegraphics.org/slides22/Journey_to_Nanite.pdf)

- [Karis Nanite Talk SIG2021](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf)

- [GAMES104_Lecture22.pdf (myqcloud.com)](https://games-1312234642.cos.ap-guangzhou.myqcloud.com/course/GAMES104/GAMES104_Lecture22.pdf)


### Log

11.8

> We will not use OpenMesh to load mesh anymore. Consider using assimp.
Two reasons mainly:
1. OpenMesh is too slow.
	- 50 seconds to load `dragon.obj`. 800k faces
	- 6 seonds to load `bunny.obj`. 60k faces
2. If we want to use half-edge data structure, that would require us to maintain this structure after every mesh simplification. Gonna bring a lot of problem.

A better solution
- Use `assimp` to load obj
	- Note: `assimp` is also not very fast when loading meshes.
		- 15 seconds to load `dragon.obj`. 800k faces
		- 1.2 seonds to load `bunny.obj`. 60k faces
- Construct adjacency graph 