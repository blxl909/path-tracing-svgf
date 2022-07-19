#version 450 core

in vec3 pix;

layout (location = 0) out vec4 filtered_color_moments;//xyz for color, w for variance

uniform sampler2D gIllumination;
uniform sampler2D gMoments_HistoryLength;
uniform sampler2D gNormalAndLinearZ;
uniform sampler2D gNormalDepthFwidth;

uniform float gPhiColor;
uniform float gPhiNormal;
//uniform float gPhiDepth;

uniform float inv_screen_width;
uniform float inv_screen_height;

float luminance(vec3 color){
    return 0.2125*color.r+0.7154*color.g+0.0721*color.b;
}


float computeWeight(
    float depthCenter, float depthP, float phiDepth,
    vec3 normalCenter, vec3 normalP, float phiNormal,
    float luminanceIllumCenter, float luminanceIllumP, float phiIllum)
{
    float weightNormal  = pow(clamp(dot(normalCenter, normalP),0.0,1.0), phiNormal);
    float weightZ       = (phiDepth == 0) ? 0.0f : abs(depthCenter - depthP) / phiDepth;
    float weightLillum  = abs(luminanceIllumCenter - luminanceIllumP) / phiIllum;

    float weightIllum   = exp(0.0 - max(weightLillum, 0.0) - max(weightZ, 0.0)) * weightNormal;

    return weightIllum;
}



void  main(){

	vec2 uv = pix.xy * 0.5 + 0.5;
	float h = texture2D(gMoments_HistoryLength,uv).z;

	if (h < 4.0) // not enough temporal history available
    {
        float sumWIllumination   = 0.0;
        vec3 sumIllumination   = vec3(0.0, 0.0, 0.0);
        vec2 sumMoments  = vec2(0.0, 0.0);

        vec4 illuminationCenter = texture2D(gIllumination,uv).xyzw;
        float lIlluminationCenter = luminance(illuminationCenter.rgb);

        float zCenter = texture2D(gNormalAndLinearZ,uv).w;

        if (zCenter == 1.0)
        {
            // current pixel does not a valid depth => must be envmap => do nothing
			filtered_color_moments = illuminationCenter;
            return;
        }

        vec3 nCenter = texture2D(gNormalAndLinearZ,uv).xyz;
        float phiLIllumination   = gPhiColor;
        float phiDepth     =  max(texture2D(gNormalDepthFwidth,uv).y, 1e-8) * 3.0;

        // compute first and second moment spatially. This code also applies cross-bilateral
        // filtering on the input illumination.
        int radius = 3;

        for (int yy = -radius; yy <= radius; yy++)
        {
            for (int xx = -radius; xx <= radius; xx++)
            {
				//-----------------------------
                vec2 p     = uv + vec2(xx * inv_screen_width,yy * inv_screen_height);
                bool inside = p.x >= 0.0 && p.x <= 1.0 && p.y >= 0.0 && p.y <= 1.0;

                if (inside)
                {
                    vec3 illuminationP = texture2D(gIllumination,p).xyz;
                    vec2 momentsP      = texture2D(gMoments_HistoryLength,p).xy;
                    float lIlluminationP = luminance(illuminationP.rgb);
					vec4 normalDepthP  = texture2D(gNormalAndLinearZ,p).rgba;
                    float zP = normalDepthP.a;
                    vec3 nP = normalDepthP.rgb;

                    float w = computeWeight(
                        zCenter, zP, phiDepth * length(vec2(xx, yy)),
                        nCenter, nP, gPhiNormal,
                        lIlluminationCenter, lIlluminationP, phiLIllumination);

                    sumWIllumination += w;
                    sumIllumination  += illuminationP * w;
                    sumMoments += momentsP * w;
                }
            }
        }

        // Clamp sum to >0 to avoid NaNs.
        sumWIllumination = max(sumWIllumination, 1e-6f);

        sumIllumination   /= sumWIllumination;
        sumMoments  /= sumWIllumination;

        // compute variance using the first and second moments
        float variance = sumMoments.g - sumMoments.r * sumMoments.r;

        // give the variance a boost for the first frames
        variance *= 4.0 / h;

		filtered_color_moments = vec4(sumIllumination,variance.r);
    }
    else
    {
        // do nothing, pass data unmodified
		filtered_color_moments = texture2D(gIllumination,uv).rgba;
    }
}

// #version 450 core
// // this pass culculate the real variance 
// //the reproject pass only culculate the variance during time
// //but if history is lowwer than 4 ,time variance is untrustable, so we culculate the variance in space
// //besides, this pass also add a cross bilateral fileter (use w weight in paper) to calculate the color and varience when history is lowwer than 4
// //the real variance is store in the w channel in output
// in vec3 pix;     //[-1,1]

// layout (location = 0) out vec4 fragColor; //xyz for color, w for variance

// uniform sampler2D reprojected_color;
// uniform sampler2D reprojected_moment_history;//x for variance,  y for history, z for first moment, w for second moment
// uniform sampler2D normal_depth;

// //now fix in code 
// //TODO: use imGUI to modify it 
// uniform float sigma_z;//depth 
// uniform float sigma_n;//normal
// uniform float sigma_l;//illumination

// uniform int screen_width;//resolution width
// uniform int screen_height;//resolution wheight

// #define epsilon 1e-8f
// #define INF     114514.0

// //check later
// float edge_stopping_weights(
// 	int delta_x,
// 	int delta_y,
// 	vec2 center_depth_gradient,
// 	float center_depth,
// 	float depth,
// 	vec3 center_normal,
// 	vec3 normal,
// 	float center_luminance_direct,
// 	float luminance_direct,
// 	float luminance_denom_direct
// ) {
// 	// ∇z(p)·(p−q) (Actually the negative of this but we take its absolute value)
// 	float d =
// 		center_depth_gradient.x * float(delta_x) +
// 		center_depth_gradient.y * float(delta_y);

// 	float ln_w_z = abs(center_depth - depth) / (sigma_z * abs(d) + epsilon);

// 	float w_n = pow(max(0.0f, dot(center_normal, normal)), sigma_n);

// 	float w_l_direct   = w_n * exp(-abs(center_luminance_direct   - luminance_direct)   * luminance_denom_direct   - ln_w_z);
	
// 	return w_l_direct;
// }

// float luminance(float r,float g,float b){
//     return 0.2125*r+0.7154*g+0.0721*b;
// }


// void  main(){
//     vec2 uv = pix.xy*0.5+0.5;
// 	vec4 moment_history = texture2D(reprojected_moment_history,uv).rgba;
//     float history = moment_history.g;

// 	vec4 center_colour_direct    = texture2D(reprojected_color,uv).rgba;

//     // float s_coord = uv.x * float(screen_width);
//     // float t_coord = uv.y * float(screen_height);

//     // int x_coord = int(s_coord);
//     // int y_coord = int(t_coord);

//     float inv_screen_width = 1.0f/screen_width;
//     float inv_screen_height = 1.0f/screen_height;

//     if (history >= 4.0f || uv.x > (1.0f - inv_screen_width) || uv.y > (1.0f - inv_screen_height)) {
//         fragColor = center_colour_direct;
// 		fragColor.w = moment_history.r;
		
// 		return;
// 	}

//     float luminance_denom = 1.0f / sigma_l;

    
//     float center_luminance_direct   = luminance(center_colour_direct.x,   center_colour_direct.y,   center_colour_direct.z);
//     vec4 center_normal_and_depth = texture2D(normal_depth,uv).rgba;

//     vec3 center_normal = center_normal_and_depth.xyz;
//     float center_depth = center_normal_and_depth.a;

//     if (center_depth == 0.0f) {
// 		fragColor = center_colour_direct;
// 		fragColor.w = moment_history.r;
// 		return;
// 	}

    

//     vec2 center_depth_gradient = vec2(
//         texture2D(normal_depth,vec2(uv.x + inv_screen_width,uv.y)).a-center_depth,
//         texture2D(normal_depth,vec2(uv.x,uv.y + inv_screen_height)).a-center_depth
// 	);

//     float sum_weight_direct   = 1.0f;
//     vec4 sum_colour_direct   = center_colour_direct;

//     vec2 sum_moment = vec2(0);

//     int radius = 3; // 7x7 filter

    
//     for (int j = -radius; j <= radius; j++) {

// 		for (int i = -radius; i <= radius; i++) {
// 			vec2 tap_uv=vec2(uv.x + i * inv_screen_width,uv.y + j * inv_screen_height);
			
// 			if (tap_uv.x < 0.0f || tap_uv.x > 1.0f||tap_uv.y < 0.0f || tap_uv.y > 1.0f||i == 0 && j == 0) continue;

			
// 			vec4 colour_direct   =  texture2D(reprojected_color,tap_uv).rgba;
// 			vec2 moment          =  texture2D(reprojected_moment_history,tap_uv).zw;

// 			float luminance_direct   = luminance(colour_direct.x,   colour_direct.y,   colour_direct.z);

// 			vec4 normal_and_depth = texture2D(normal_depth,tap_uv).rgba;

// 			vec3 normal = normal_and_depth.xyz;
// 			float depth = normal_and_depth.a;
			

// 			float w_direct = edge_stopping_weights(
// 				i, j,
// 				center_depth_gradient,
// 				center_depth, depth,
// 				center_normal, normal,
// 				center_luminance_direct, 
// 				luminance_direct, 
// 				luminance_denom
// 			);

// 			sum_weight_direct   += w_direct;

// 			sum_colour_direct   += w_direct   * colour_direct;

//             //moment is vec2
// 			sum_moment += moment * w_direct;
// 		}
// 	}

//     sum_weight_direct   = max(sum_weight_direct,   1e-6f);
// 	sum_colour_direct   /= sum_weight_direct;
//     sum_moment /= sum_weight_direct;
//     float variance_direct   = max(0.0f, sum_moment.y - sum_moment.x * sum_moment.x);
//     // give the variance a boost for the first frames
//     //variance_direct*= 4.0 / history;
//     sum_colour_direct.w=variance_direct;

//     fragColor=sum_colour_direct;
	

// }