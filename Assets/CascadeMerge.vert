#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;

//out vec2 fromProbeCoord;
out vec2 toProbeCoordInput;
out vec2 toPixelCoordInput;

uniform vec2 cascadeTextureDimensions;
uniform vec2 worldTextureDimensions;
 
// { offset, scale }
uniform vec2 toHorizontalTransform; 

uniform ivec2 mergeFromProbeResolution;
uniform ivec2 mergeToProbeResolution;

uniform ivec2 mergeFromAngleResolution;
uniform ivec2 mergeToAngleResolution;

void main()
{
    // 0.0f -> 1.0f transformed coordinates
    float toTransformedTexCoordX = texCoord.x * toHorizontalTransform.y + toHorizontalTransform.x; // Then transform to match the requested horizontal cascade slice    
    
	gl_Position = vec4(position, 0.0f, 1.0f);
    gl_Position.x = (toTransformedTexCoordX * 2.0f) - 1.0f; // Remap to -1.0f -> 1.0f

    toProbeCoordInput = texCoord * mergeToProbeResolution;
    toPixelCoordInput = vec2(toTransformedTexCoordX, texCoord.y) * cascadeTextureDimensions;
}