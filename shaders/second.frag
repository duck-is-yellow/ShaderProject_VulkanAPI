#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColour; // Colour Output from Subpass 1
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth; // Depth Output from Subpass 1 

layout(location = 0) out vec4 colour;

void main()
{
	int xHalf = 201;
	if(gl_FragCoord.x > xHalf)
	{
		float lowerBound = 0.99;
		float upperBound = 1;

		float depth = subpassLoad(inputDepth).r;
		float depthColourScale = 1.0f - ( (depth - lowerBound) / (upperBound - lowerBound) );
		colour = vec4(depthColourScale , 0.0f, 0.0f, 1.0f);
	}
	else
	{
		colour = subpassLoad(inputColour).rgba;
	}
}