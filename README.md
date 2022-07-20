# path-tracing-svgf
an OpenGL based path tracer with SVGF(Spatiotemporal Variance-Guided Filter)  
more detail about SVGF see [paper link here](http://behindthepixels.io/assets/files/hpg17_svgf.pdf)
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
## TODO
1、add some documents to full explain how to implement it  
2、add Cmake file  
3、refactor code, now it's a little bit messy
