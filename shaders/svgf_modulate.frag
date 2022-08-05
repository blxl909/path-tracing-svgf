#version 450 core

in vec3 pix;


layout (location = 0) out vec4 modulate_color;

uniform sampler2D gAlbedo;
uniform sampler2D gEmission;
uniform sampler2D gIllumination;
uniform sampler2D gNormalAndLinearZ;

vec3 tonrMapping(in vec3 c, float limit){
    float luminance=0.3*c.x+0.6*c.y+0.1*c.z;
    return c*1.0/(1.0+luminance/limit);
}

void  main(){
    vec2 uv = pix.xy * 0.5 + 0.5;
    float depth = texture2D(gNormalAndLinearZ,uv).a;
    vec3 color = texture2D(gIllumination,uv).xyz;
    //color=tonrMapping(color,1.5);
    //color=pow(color,vec3(1.0/2.2));
    if(depth == 1.0f){
        modulate_color = vec4(color,1.0);
    }else{
        modulate_color = vec4(color * texture2D(gAlbedo,uv).xyz + texture2D(gEmission,uv).xyz,1.0);
    }
    
}