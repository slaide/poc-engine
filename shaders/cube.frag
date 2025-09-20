#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Uniform buffer (must match vertex shader)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 ambient_color;
    vec3 diffuse_color;
    vec3 specular_color;
    float shininess;
    vec3 light_pos;
    vec3 view_pos;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the interpolated normal
    vec3 normal = normalize(fragNormal);

    // Calculate lighting vectors
    vec3 lightDir = normalize(ubo.light_pos - fragWorldPos);
    vec3 viewDir = normalize(ubo.view_pos - fragWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);

    // Ambient component
    vec3 ambient = ubo.ambient_color;

    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * ubo.diffuse_color;

    // Specular component
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ubo.shininess);
    vec3 specular = spec * ubo.specular_color;

    // Combine components
    vec3 result = ambient + diffuse + specular;

    outColor = vec4(result, 1.0);
}