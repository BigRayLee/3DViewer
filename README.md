# Project Title

A lightweight 3D viewer can render massive mesh surface model without LOD popping and mesh cracks.

## Description

The viewer is developed based on C++, OpenGL, GLSL.
The overview is as follows (The colorization of the different LODs):
![image]( https://github.com/BigRayLee/3DViewer/blob/master/pic/overview.png)

* HLOD construction
* Real-time rendering

## Getting Started

### Dependencies

* System: Linux
* Libraray: GLFW, GLAD, OpenGL, Pthread,  

  Mesh simplification is based on [MeshOptimizer](https://github.com/zeux/meshoptimizer) 

### Installing

* In-core mode  

  make 

* out-of-core mode (TODO)

### Executing program

* In-core mode   

  ./bin/viewer model_filepath

## Relate publications
* [View-dependent Adaptive HLOD: real-time interactive rendering of multi-resolution models.](https://doi.org/10.1145/3626495.3626507) In European Conference on Visual Media Production (CVMP ’23).

* [Real-world large-scale terrain model reconstruction and real-time rendering.](https://doi.org/10.1145/3611314.3615901) In The 28th International ACM Conference on 3D Web Technology (Web3D ’23).

* [A lightweight 3D viewer: real-time rendering of multi-scale 3D surface models.] In Proceedings of 27th ACM SIGGRAPH Symposium on Interactive 3D Graphics and Games (i3D 2023 posters). [Paper](https://i3dsymposium.org/2023/posters/rui_li_paper.pdf) [Poster](https://i3dsymposium.org/2023/posters/rui_li_poster.pdf) [Fast-forward](https://youtu.be/67qO-GjGcIE?si=kDYSd3N9g8Pky8ce) (Best Poster Paper Award sponsor by Intel)

* [Adaptive real-time interactive rendering of gigantic multi-resolution models.](https://doi.org/10.1145/3550082.3564170) In SIGGRAPH Asia 2022 Posters (SA '22).

* Real-time visualization of multi-resolution mesh based on dynamical subdivision. In Proceedings of 26th ACM SIGGRAPH Symposium on Interactive 3D Graphics and Games (i3D 2023 posters).  [Poster](https://i3dsymposium.org/2022/posters.html) [Fast-forward](https://youtu.be/DOXm6VvQGuo?si=O63NUIkSclwZPCnF)

## Demo video
* [3DEspace]{https://drive.google.com/file/d/1QoIlu-2RYoWaYcheJXtbeWpXiwQP505v/view}: an adaptive lightweight 3D viewer. Proceedings IEEE International Conference on Computer Vision (ICCV), Paris, France, 2023


