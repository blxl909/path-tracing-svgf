#version 450 core

in vec3 pix;

layout (location = 0) out vec4 filteredIllumination;

uniform sampler2D gIllumination;
uniform sampler2D gNormalAndLinearZ;
uniform sampler2D gNormalDepthFwidth;

uniform int       gStepSize;
uniform float     gPhiColor;
uniform float     gPhiNormal;

uniform float inv_screen_width;
uniform float inv_screen_height;

// computes a 3x3 gaussian blur of the variance, centered around
// the current pixel
float computeVarianceCenter(vec2 ipos)
{
    float sum = 0.0f;

    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    const int radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            const vec2 p = ipos + vec2(xx * inv_screen_width, yy * inv_screen_height);
            const float k = kernel[abs(xx)][abs(yy)];
            sum += texture2D(gIllumination,ipos).a * k;
        }
    }

    return sum;
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

float luminance(vec3 color){
    return 0.2125*color.r+0.7154*color.g+0.0721*color.b;
}

void main(){

	vec2 ipos = pix.xy * 0.5 + 0.5;

    const float epsVariance      = 1e-10;
    const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    const vec4 illuminationCenter = texture2D(gIllumination,ipos).rgba;
    const float lIlluminationCenter = luminance(illuminationCenter.rgb);

	// variance, filtered using 3x3 gaussin blur
    const float var = computeVarianceCenter(ipos);

	vec4 normalDepthCenter = texture2D(gNormalAndLinearZ,ipos).rgba;
	float zCenter = normalDepthCenter.w;

    if (zCenter == 1.0)
    {
        // current pixel does not a valid depth => must be envmap => do nothing
		filteredIllumination = illuminationCenter;
        return;
    }

    vec3 nCenter = normalDepthCenter.rgb;

	const float phiLIllumination   = gPhiColor * sqrt(max(0.0, epsVariance + var.r));
    const float phiDepth     = max(texture2D(gNormalDepthFwidth,ipos).y, 1e-8) * gStepSize;

    float sumWIllumination   = 1.0;
    vec4  sumIllumination  = illuminationCenter;

	for (int yy = -2; yy <= 2; yy++)
    {
        for (int xx = -2; xx <= 2; xx++)
        {
            const vec2 p     = ipos + vec2(xx * inv_screen_width, yy * inv_screen_height) * gStepSize;
            const bool inside = p.x >= 0.0 && p.x <= 1.0 && p.y >= 0.0 && p.y <= 1.0;

            const float kernel = kernelWeights[abs(xx)] * kernelWeights[abs(yy)];

            if (inside && (xx != 0 || yy != 0)) // skip center pixel, it is already accumulated
            {
                const vec4 illuminationP = texture2D(gIllumination,p).rgba;
                const float lIlluminationP = luminance(illuminationP.rgb);

				const vec4 normalDepthP  = texture2D(gNormalAndLinearZ,p).rgba;
                const float zP = normalDepthP.a;
                const vec3 nP = normalDepthP.rgb;

                // compute the edge-stopping functions
                const float w = computeWeight(
                    zCenter.x, zP, phiDepth * length(vec2(xx, yy)),
                    nCenter, nP, gPhiNormal,
                    lIlluminationCenter, lIlluminationP, phiLIllumination);

                const float wIllumination = w * kernel;

                // alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula
                sumWIllumination  += wIllumination;
                sumIllumination   += vec4(wIllumination.xxx, wIllumination * wIllumination) * illuminationP;
            }
        }
    }

	filteredIllumination = vec4(sumIllumination / vec4(sumWIllumination.xxx, sumWIllumination * sumWIllumination));

}