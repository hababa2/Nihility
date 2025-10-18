#version 450
#extension GL_EXT_nonuniform_qualifier : require

#define SAMPLE_TYPE 0

layout(push_constant) uniform constants
{
    vec2 worldTextureDimensions;
    vec2 cascadeTextureDimensions;
    ivec2 cascade0AngleResolution;
    ivec2 cascade0ProbeResolution;
    int cascadeTextureIndex;
} Globals;

layout (set = 1, binding = 10) uniform sampler2D globalTextures[];

layout (location = 0) in vec2 texcoord;

layout (location = 0) out vec4 fragColor;

vec4 BilinearWeights(vec2 ratio)
{
    return vec4(
        (1.0f - ratio.x) * (1.0f - ratio.y),
        ratio.x * (1.0f - ratio.y),
        (1.0f - ratio.x) * ratio.y,
        ratio.x * ratio.y
    );
}

vec2 WorldPositionToProbeCoordinate(vec2 worldPosition, ivec2 probeResolution)
{
    vec2 probeSpacing = Globals.worldTextureDimensions / probeResolution;
    return (worldPosition / probeSpacing) - 0.5f;
}

void main(void)
{
    vec2 worldPosition = Globals.worldTextureDimensions * texcoord; //TODO: Use camera
    vec2 probeCoordinate = WorldPositionToProbeCoordinate(worldPosition, Globals.cascade0ProbeResolution);

#if SAMPLE_TYPE == 0 //Point
    ivec2 bottomLeftProbeCoordinate = ivec2(round(probeCoordinate));
    vec4 weights = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    int probeSampleAmount = 1;
#elif SAMPLE_TYPE == 1 //Average
    ivec2 bottomLeftProbeCoordinate = ivec2(floor(probeCoordinate));
    vec4 weights = vec4(0.25f, 0.25f, 0.25f, 0.25f);
    int probeSampleAmount = 2;
#elif SAMPLE_TYPE == 2 //Bilinear Interpolation
    ivec2 bottomLeftProbeCoordinate = ivec2(floor(probeCoordinate));
    vec4 weights = BilinearWeights(fract(probeCoordinate));
    int probeSampleAmount = 2;
#endif
    
    vec4 combinedRadiance = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int probeOffsetY = 0; probeOffsetY < probeSampleAmount; ++probeOffsetY)
    {
        for (int probeOffsetX = 0; probeOffsetX < probeSampleAmount; ++probeOffsetX)
        {
            ivec2 probeCoordinate = bottomLeftProbeCoordinate + ivec2(probeOffsetX, probeOffsetY);
            
            if (any(lessThan(probeCoordinate, ivec2(0, 0))) || any(greaterThanEqual(probeCoordinate, Globals.cascade0ProbeResolution)))
                continue;

            ivec2 probePositionOffsetBottomLeft = probeCoordinate * Globals.cascade0AngleResolution;

            for (int directionOffsetY = 0; directionOffsetY < Globals.cascade0AngleResolution.y; ++directionOffsetY)
            {
                for (int directionOffsetX = 0; directionOffsetX < Globals.cascade0AngleResolution.x; ++directionOffsetX)
                {
                    vec2 samplePosition = probePositionOffsetBottomLeft + vec2(directionOffsetX, directionOffsetY);

                    vec4 radiance = texture(globalTextures[nonuniformEXT(Globals.cascadeTextureIndex)], samplePosition / Globals.cascadeTextureDimensions);
                    combinedRadiance += radiance * weights[probeOffsetY * 2 + probeOffsetX];
                }
            }
        }
    }

    fragColor = combinedRadiance / (Globals.cascade0AngleResolution.x * Globals.cascade0AngleResolution.y);

    fragColor.rgb = pow(fragColor.rgb, 1 / vec3(2.2f, 2.2f, 2.2f));
}