#version 330 core
layout (location = 0) out vec4 clipPos;
layout (location = 1) out vec4 gNormal;



in vec3 FragPos;
in vec3 Normal;



void main()
{    
    // store the fragment position vector in the first gbuffer texture
    gNormal =vec4 (Normal,1.0);
    // also store the per-fragment normals into the gbuffer
    clipPos =vec4( FragPos,1.0);

}