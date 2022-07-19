#version 450 core

in vec3 pix;


layout (location = 0) out vec4 modulate_color;

uniform sampler2D gAlbedo;
uniform sampler2D gEmission;
uniform sampler2D gIllumination;
uniform sampler2D gNormalAndLinearZ;


void  main(){
    vec2 uv = pix.xy * 0.5 + 0.5;
    float depth = texture2D(gNormalAndLinearZ,uv).a;
    if(depth == 1.0f){
        modulate_color = vec4(texture2D(gIllumination,uv).xyz,1.0);
    }else{
        modulate_color = vec4(texture2D(gIllumination,uv).xyz * texture2D(gAlbedo,uv).xyz + texture2D(gEmission,uv).xyz,1.0);
    }
}