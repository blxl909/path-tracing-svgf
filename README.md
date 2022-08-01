# path-tracing-svgf
an OpenGL based path tracer with SVGF(Spatiotemporal Variance-Guided Filter)  
more detail about SVGF see [paper link here](http://behindthepixels.io/assets/files/hpg17_svgf.pdf)  
this work is based on the project [EzRT](https://github.com/AKGWSB/EzRT) and [Falcor](https://github.com/NVIDIAGameWorks/Falcor) 
## result
### input
SVGF algorithm takes in one sample per pixel image input use normal path tracing (with next event estimation)  
![1spp_input](https://github.com/blxl909/path-tracing-svgf/blob/master/result/1spp_input.png)
### output
SVGF will reconstruct image,here is the result  
![svgf_output](https://github.com/blxl909/path-tracing-svgf/blob/master/result/svgf_output.png)
### compare to the accumulate color
the accumulate color after 133 iterations  
![acc_color](https://github.com/blxl909/path-tracing-svgf/blob/master/result/accumulate_output.png)
## Build
### requirement
* Windows 10 x64
* Visual Studio 2019
* cmake
* vcpkg
### third party library
* GLFW
* glad
* glm
* hdrloader
* imgui
### build step
you can build these third party library yourself or use vcpkg, hdrloader and imgui have been already included in this project.  
  
if use vcpkg:  
`.\vcpkg install glfw3:x64-windows`  
`.\vcpkg install glad:x64-windows`  
`.\vcpkg install glm:x64-windows`  
`.\vcpkg integrate install`  
then visual studio will automatically link these libraries.  
  
after that, come to source code folder, use command:  
`cmake ./`to generate visual studio solution.
set project 'path_tracer_svgf' as start project，build on x64 and Realease mode.(Debug mode would be really slow).  
## TODO
- [ ] add some documents to fully explain how to implement this algorithm  
- [x] add cmake file  
- [ ] refactor code, now it's a little bit messy
