#version 450

layout(location = 0) out uint InstanceIndex;

void main()
{
    InstanceIndex = gl_InstanceIndex;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}