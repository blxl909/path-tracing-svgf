#version 330 core

in vec3 pix;

out vec4 fragColor;

uniform sampler2D in_texture;



void  main(){
    vec4 color=texture2D(in_texture,pix.xy*0.5+0.5).rgba;

    fragColor=color;


}