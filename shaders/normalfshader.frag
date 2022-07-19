#version 450 core
layout (location = 0) out vec4 worldPos;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 velocity;
layout (location = 3) out vec4 gNormalLinearZFwidth;



in vec3 FragPos;
in vec3 Normal;
in vec4 preScreenPosition;
in vec4 nowScreenPosition;



void main()
{    
    float linearZ = gl_FragCoord.z / gl_FragCoord.w;
    gNormal =vec4 (Normal,linearZ);
    
    worldPos =vec4( FragPos,1.0);

    vec2 newPos = ((nowScreenPosition.xy / nowScreenPosition.w) * 0.5 + 0.5);
    vec2 prePos = ((preScreenPosition.xy / preScreenPosition.w) * 0.5 + 0.5);
    velocity.xy = newPos - prePos;
    velocity.w = 1.0f;

   
    gNormalLinearZFwidth = vec4(length(fwidth(Normal)),max(abs(dFdx(linearZ)), abs(dFdy(linearZ))),linearZ,1.0);
    

}