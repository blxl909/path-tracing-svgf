#version 330 core
layout (location = 0) out vec4 clipPos;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 velocity;



in vec3 FragPos;
in vec3 Normal;
in vec4 preScreenPosition;
in vec4 nowScreenPosition;


void main()
{    
    // store the fragment position vector in the first gbuffer texture
    gNormal =vec4 (Normal,gl_FragCoord.z);
    // also store the per-fragment normals into the gbuffer
    clipPos =vec4( FragPos,1.0);

    vec2 newPos = ((nowScreenPosition.xy / nowScreenPosition.w) * 0.5 + 0.5);
    vec2 prePos = ((preScreenPosition.xy / preScreenPosition.w) * 0.5 + 0.5);
    velocity.xy = newPos - prePos;
    velocity.w = 1.0f;
    

}