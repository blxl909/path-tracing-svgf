#version 330 core

in vec3 pix;//[-1,1]

layout (location = 0) out vec4 fragColor_swapBuffer0;//final color
//layout (location = 1) out vec4 fragColor_swapBuffer1;//final color
//layout (location = 2) out vec4 fragColor_for_next_frame_input;//final color

uniform sampler2D normal_depth;
uniform sampler2D variance_computed_color;


uniform int screen_width;//resolution width
uniform int screen_height;//resolution wheight
uniform int step_size;
//uniform int iteration; //current a trous iteration [0,max_iteration)

uniform float sigma_z;//depth 
uniform float sigma_n;//normal
uniform float sigma_l;//illumination

#define epsilon 1e-6f
#define INF     114514.0


// Gaussian kernel weights:
// 1/16  1/8  1/16
// 1/8   1/4  1/8
// 1/16  1/8  1/16
float Gaussian_weight(float x,int n){
    return x * pow(2,n);
}

float luminance(float r,float g,float b){
    return 0.2125*r+0.7154*g+0.0721*b;
}

float edge_stopping_weights(
	int delta_x,
	int delta_y,
	vec2 center_depth_gradient,
	float center_depth,
	float depth,
	vec3 center_normal,
	vec3 normal,
	float center_luminance_direct,
	float luminance_direct,
	float luminance_denom_direct
) {
	// ∇z(p)·(p−q) (Actually the negative of this but we take its absolute value)
	float d =
		center_depth_gradient.x * float(delta_x) +
		center_depth_gradient.y * float(delta_y);

	float ln_w_z = abs(center_depth - depth) / (sigma_z * abs(d) + epsilon);

	float w_n = pow(max(0.0f, dot(center_normal, normal)), sigma_n);

	float w_l_direct   = w_n * exp(-abs(center_luminance_direct   - luminance_direct)   * luminance_denom_direct   - ln_w_z);
	
	//return clamp(w_l_direct,0.0f,100.0f);
    return w_l_direct;
}

void  main(){

    vec2 uv = pix.xy*0.5+0.5;

    float variance_blurred_direct   = 0.0f;

    float inv_screen_width = 1.0f/screen_width;
    float inv_screen_height = 1.0f/screen_height;

    // Filter Variance using a 3x3 Gaussian Blur
	for (int j = -1; j <= 1; j++) {

		for (int i = -1; i <= 1; i++) {
			vec2 tap_uv=vec2(clamp(uv.x+inv_screen_width*i,0.0f,1.0f),clamp(uv.y+inv_screen_height*j,0.0f,1.0f));

			// Read the Variance of Direct Illumination
			// The Variance is stored in the alpha channel (w coordinate)
			float variance_direct   = texture2D(variance_computed_color,tap_uv).w;

			float kernel_weight = Gaussian_weight(0.25f, -(abs(i) + abs(j)));

			variance_blurred_direct   += variance_direct   * kernel_weight;
		}
	}




    // Precompute denominators that are loop invariant
	//公式中的高斯项
	float luminance_denom_direct   = sqrt(sigma_l * sigma_l * max(0.0f, variance_blurred_direct)+ epsilon);

    vec4 center_colour_direct   = texture2D(variance_computed_color,uv).rgba;

    float center_luminance_direct   = luminance(center_colour_direct.x,   center_colour_direct.y,   center_colour_direct.z);

    vec4 center_normal_and_depth = texture2D(normal_depth,uv).rgba;

    vec3 center_normal = center_normal_and_depth.rgb;
	float  center_depth  = center_normal_and_depth.a;



    //|| uv.x > (1.0f - inv_screen_width) || uv.y > (1.0f - inv_screen_height)
    if (center_depth == INF ){
        
        gl_FragData[0]=center_colour_direct;
        
        return;
    }


    float gradient_uv_x=uv.x + inv_screen_width;
    float gradient_uv_y=uv.y + inv_screen_height;
    if(gradient_uv_x>1.0f||gradient_uv_y>1.0f){

        gl_FragData[0]=center_colour_direct;
        return;
    }

    

    vec2 center_depth_gradient = vec2(
        texture2D(normal_depth,vec2(gradient_uv_y,uv.y)).a-center_depth,
        texture2D(normal_depth,vec2(uv.x,gradient_uv_y)).a-center_depth
	);

    

    float  sum_weight_direct   = 1.0f;
	vec4   sum_colour_direct   = center_colour_direct;

    int radius = 2;

    
    float kernelWeights[3] = float[]( 3.0/8.0, 1.0 / 4.0, 1.0 / 16.0 );
    
    for (int j = -radius; j <= radius; j++) {
        
		//tap_uv.y = uv.y + j * step_size * inv_screen_height;

		//if (tap_uv.y < 0.0f || tap_uv.y > 1.0f) continue;

		for (int i = -radius; i <= radius; i++) {
			//tap_uv.x = uv.x + i * step_size * inv_screen_width;
            vec2 tap_uv=vec2(uv.x + i * step_size * inv_screen_width,uv.y + j * step_size * inv_screen_height);

			if (tap_uv.x < 0.0f || tap_uv.x > 1.0f||tap_uv.y < 0.0f || tap_uv.y > 1.0f||i == 0 && j == 0) continue;
			
			vec4 colour_direct   =  texture2D(variance_computed_color,tap_uv).rgba;

			float luminance_direct   = luminance(colour_direct.x,   colour_direct.y,   colour_direct.z);

			vec4 normal_and_depth = texture2D(normal_depth,tap_uv).rgba;

			vec3 normal = normal_and_depth.xyz;
			float depth = normal_and_depth.a;

			float w_direct = edge_stopping_weights(
				i * step_size,
                j * step_size,
				center_depth_gradient,
				center_depth, depth,
				center_normal, normal,
				center_luminance_direct, 
				luminance_direct, 
				luminance_denom_direct
			);
            
            float cofi=kernelWeights[abs(i)] * kernelWeights[abs(j)];

			sum_weight_direct   += w_direct*cofi;

            // Filter Colour using the weights, filter Variance using the square of the weights
			sum_colour_direct   += vec4(w_direct,w_direct,w_direct,w_direct * w_direct)  * colour_direct*cofi;
            
		}
	}

    
    
    //some assertion
    if(sum_weight_direct < epsilon){
        gl_FragData[0]=center_colour_direct;
        return;
    }
    
    float inv_sum_weight_direct   = 1.0f / sum_weight_direct;
    
    // Normalize
    sum_colour_direct   *= inv_sum_weight_direct;

    // Alpha channel contains Variance, and needs to be divided by the square of the weights
	sum_colour_direct  .w *= inv_sum_weight_direct;
    

   gl_FragData[0]=sum_colour_direct;
   //gl_FragData[0]=vec4(1/0,1/0,1,1);
    

    

	
}