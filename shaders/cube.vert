#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 ambient_color;
    float _pad1;
    vec3 diffuse_color;
    float _pad2;
    vec3 specular_color;
    float shininess;
    vec3 light_pos;
    float _pad3;
    vec3 view_pos;
    float _pad4;
    vec4 render_params;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Transform vertex to world space
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // Transform normal to world space (should use normal matrix for non-uniform scaling)
    fragNormal = mat3(ubo.model) * inNormal;

    // Pass through texture coordinates
    fragTexCoord = inTexCoord;

    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * worldPos;
}
