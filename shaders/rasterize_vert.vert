#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;


out vec3 FragPos;
out vec3 Normal;
out vec4 preScreenPosition;
out vec4 nowScreenPosition;


uniform mat4 view;
uniform mat4 projection;

uniform mat4 pre_viewproj;
uniform int screen_width;//resolution width
uniform int screen_height;//resolution wheight
uniform uint frameCounter;


void main()
{
    vec4 worldPos = vec4(aPos, 1.0);

    gl_Position = projection * view * worldPos;

    preScreenPosition = pre_viewproj * worldPos;
    nowScreenPosition = projection * view * worldPos;

    FragPos = worldPos.xyz;
    Normal = aNormal;

}