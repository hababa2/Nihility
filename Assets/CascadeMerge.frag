#version 450

#define M_PI 3.1415926535897932384626433832795

in vec2 toProbeCoordInput;
in vec2 toPixelCoordInput;

out vec4 color;

uniform bool doMerge;
uniform bool bilinearInterpolation;
uniform bool enableBilinearFix;

uniform vec2 mergeToIntervalMinMax;

uniform float mergeFromLeftPositionX;

uniform ivec2 mergeFromProbeResolution;
uniform ivec2 mergeToProbeResolution;

uniform ivec2 mergeFromAngleResolution;
uniform ivec2 mergeToAngleResolution;

uniform sampler2D cascadeTexture;
uniform vec2 cascadeTextureDimensions;

uniform sampler2D worldTexture;
uniform vec2 worldTextureDimensions;

// https://playtechs.blogspot.com/2007/03/raytracing-on-grid.html?m=1
// Return rgb value is the colour of the first solid pixel
// The alpha value is 0 if it hit something, 1 if it didnt
// This function is most certainly the bottleneck for performance and could be swapped out for a more efficient approach e.g. raymarching
vec4 Raycast(vec2 from, vec2 to)
{
    vec2 delta = abs(to - from);
    ivec2 current = ivec2(floor(from));

    int n = 1;
    ivec2 inc = ivec2(0, 0);
    float error = 0;

    if (delta.x == 0)
    {
        inc.x = 0;
        error = 99999999.0f;
    }
    else if (to.x > from.x)
    {
        inc.x = 1;
        n += int(floor(to.x)) - current.x;
        error = (floor(from.x) + 1 - from.x) * delta.y;
    }
    else
    {
        inc.x = -1;
        n += current.x - int(floor(to.x));
        error = (from.x - floor(from.x)) * delta.y;
    }

    if (delta.y == 0)
    {
        inc.y = 0;
        error = -99999999.0f;
    }
    else if (to.y > from.y)
    {
        inc.y = 1;
        n += int(floor(to.y)) - current.y;
        error -= (floor(from.y) + 1 - from.y) * delta.x;
    }
    else
    {
        inc.y = -1;
        n += current.y - int(floor(to.y));
        error -= (from.y - floor(from.y)) * delta.x;
    }

    vec3 color = vec3(0.0f, 0.0f, 0.0f);

    for (; n > 0; --n)
    {
        if (any(lessThan(current, vec2(0, 0))) || any(greaterThanEqual(current, worldTextureDimensions)))
            return vec4(color, 1.0f); // 1 means it hit nothing

        vec4 sampledColour = texture(worldTexture, current / (worldTextureDimensions - 1));
        color = sampledColour.rgb;

        if (sampledColour.a == 1.0f)
            return vec4(color, 0.0f); // 0 means it hit something

        if (error > 0)
        {
            current.y += inc.y;
            error -= delta.x;
        }
        else
        {
            current.x += inc.x;
            error += delta.y;
        }
    }
    
    return vec4(color.rgb, 1.0f); // 1 means it hit nothing
}

vec4 MergeRadiance(vec4 near, vec4 far)
{   
    vec3 radiance = near.rgb + (far.rgb * near.a);
    return vec4(radiance, near.a * far.a);
}

// Weights for smooth merging between cascades
// All 4 numbers add up to 1
const vec4 ProbeIndexToBilinearWeights[4] = { 
    vec4(0.5625f, 0.1875f, 0.1875f, 0.0625f), 
    vec4(0.1875f, 0.5625f, 0.0625f, 0.1875f), 
    vec4(0.1875f, 0.0625f, 0.5625f, 0.1875f), 
    vec4(0.0625f, 0.1875f, 0.1875f, 0.5625f) };

// A naive approach to figuring out the weights of the next cascades probe as it assumes everything scales the same (2x)
// This is likely not mathmaticallly true on non power of 2 canvas sizes however I have not noticed any issues visually yet..
vec4 CalculateBilinearWeightsFromProbeCoordinate(ivec2 probeCoordinates)
{
    probeCoordinates -= 1; // - 1 because edges (e.g. 0, 0) are in the top right of their quadrant rather than the bottom left
    probeCoordinates %= 2; // Modulos with 2 so that we get the local coordinate within each quadrant

    // Use pre calculated weights, these are always the same for 2x probe resolution scaling
    int index = probeCoordinates.y * 2 + probeCoordinates.x;
    return ProbeIndexToBilinearWeights[index];
}

// Will return out of bounds coordinates in the bottom left sides
// This is correct because of how the next cascades probe are laid out inbetween the previous ones
ivec2 ProbeCoordinateToBottomLeftOfNextCascade(ivec2 probeCoordinate)
{
    return ivec2(floor((probeCoordinate - 1) * 0.5f));
}

vec2 ProbeCoordinateToWorldCenterPosition(ivec2 probeResolution, ivec2 coords)
{
    vec2 probeSpacing = worldTextureDimensions / probeResolution;
    return probeSpacing * (coords + 0.5f);
}

vec2 AngleIndexToRayDirection(int angleIndex, ivec2 angleResolution)
{
    // t does not reach 1.0f as angleIndex is 0 based while resolution is not
    // This is on purpose so we don't raycast 0 & 360 angle
    float angleT = angleIndex / float(angleResolution.x * angleResolution.y);
    
    // Adding 45 degrees so that e.g. the 4 raycasts are offset rather than on the axis aligned
    float rayAngle = angleT * (M_PI * 2.0f) + (M_PI * 0.25f);
    vec2 rayDirection = vec2(cos(rayAngle), sin(rayAngle));
    return rayDirection;
}

vec4 SampleMergedRays(vec2 probeBottomLeftPosition, int startAngleIndex, ivec2 angleResolution, int sampleCount)
{
    vec4 radiance = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Loop over every direction to sample in the from cascade
    for (int directionOffset = 0; directionOffset < sampleCount; ++directionOffset)
    {
        int angleIndex = startAngleIndex + directionOffset;

        ivec2 angleCoord = ivec2(
            angleIndex % angleResolution.x,
            floor(angleIndex / float(angleResolution.x)));

        vec2 samplePosition = probeBottomLeftPosition + angleCoord;
        vec4 sampleRadiance = texture(cascadeTexture, samplePosition / cascadeTextureDimensions);

        radiance += sampleRadiance;
    }

    return radiance / sampleCount;
}

vec4 MergeCascade(ivec2 probeCoord, int angleIndex)
{
    vec2 probeCenterWorldPosition = ProbeCoordinateToWorldCenterPosition(mergeToProbeResolution, probeCoord);
    vec2 rayDirection = AngleIndexToRayDirection(angleIndex, mergeToAngleResolution);
    vec2 from = probeCenterWorldPosition + rayDirection * mergeToIntervalMinMax.x;

    vec4 currentCascadesRadiance = vec4(0, 0, 0, 0);

    if (!enableBilinearFix || !doMerge)
    {
        vec2 to = probeCenterWorldPosition + rayDirection * mergeToIntervalMinMax.y;
        currentCascadesRadiance = Raycast(from, to);
    }

    // We do not merge anything for our first merge as there is nothing to merge
    // So we just output the normal raycast
    if (!doMerge)
        return currentCascadesRadiance;


    vec4 finalRadiance = vec4(0, 0, 0, 0);
    ivec2 fromBottomLeftProbeCoord = ProbeCoordinateToBottomLeftOfNextCascade(probeCoord);

    vec4 weights = vec4(0.25f, 0.25f, 0.25f, 0.25f);

    if (bilinearInterpolation)
        weights = CalculateBilinearWeightsFromProbeCoordinate(probeCoord);

    for (int yOffset = 0; yOffset < 2; ++yOffset)
    {
        for (int xOffset = 0; xOffset < 2; ++xOffset)
        {
            ivec2 probeCoordinate = fromBottomLeftProbeCoord + ivec2(xOffset, yOffset);
            
            // If probe coordinate is out of bounds (happens at the edges of the screen) then we assume empty/black and skip
            // Could clamp here instead, or perhaps some environmental colour
            if (any(lessThan(probeCoordinate, ivec2(0, 0))) || any(greaterThanEqual(probeCoordinate, mergeFromProbeResolution)))
                continue;

            if (enableBilinearFix)
            {
                // For bilinear fix we do another raycast from the same from position but to the general starting position of the next cascades rays
                // This reduces empty gaps when merging and smooths over most light leaks
                vec2 fromProbeCenterWorldPosition = ProbeCoordinateToWorldCenterPosition(mergeFromProbeResolution, fromBottomLeftProbeCoord + ivec2(xOffset, yOffset));
                vec2 to = fromProbeCenterWorldPosition + rayDirection * mergeToIntervalMinMax.y;

                currentCascadesRadiance = Raycast(from, to);
            }

            vec2 probeBottomLeftPosition = probeCoordinate * mergeFromAngleResolution;
            probeBottomLeftPosition.x += mergeFromLeftPositionX;

            vec4 sampledRadiance = SampleMergedRays(probeBottomLeftPosition, angleIndex * 2, mergeFromAngleResolution, 2);
            finalRadiance += MergeRadiance(currentCascadesRadiance, sampledRadiance) * weights[yOffset * 2 + xOffset];
        }
    }

    return finalRadiance;
}

void main(void)
{
    // Get pixel index
    ivec2 angleCoord = ivec2(toPixelCoordInput);
    angleCoord %= mergeToAngleResolution;

    int toAngleIndex = angleCoord.y * mergeToAngleResolution.x + angleCoord.x;
    ivec2 toProbeCoord = ivec2(floor(toProbeCoordInput));

    color = MergeCascade(toProbeCoord, toAngleIndex);
}
