#version 450 core

in vec3 pix;

layout (location = 0) out vec4 filtered_color_moments;//xyz for color, w for variance

uniform sampler2D gIllumination;
uniform sampler2D gMoments_HistoryLength;
uniform sampler2D gNormalAndLinearZ;
uniform sampler2D gNormalDepthFwidth;

uniform float gPhiColor;
uniform float gPhiNormal;

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

