--[[
  Mat4 utility module for POC Engine

  Provides convenience functions for working with 4x4 matrices.
  Built on top of the core Math.mat4 bindings from C.
]]

local Mat4 = {}

-- Common matrix constants
Mat4.IDENTITY = Math.mat4.identity()

-- Shorthand constructor
function Mat4.new()
    return Math.mat4.new()
end

function Mat4.identity()
    return Math.mat4.identity()
end

-- Create translation matrix
function Mat4.translation(x, y, z)
    local mat = Mat4.identity()
    if type(x) == "table" then
        -- Assume it's a vec3-like object
        return mat:translate(x)
    else
        local vec = Math.vec3(x or 0, y or 0, z or 0)
        return mat:translate(vec)
    end
end

-- Create rotation matrix from Euler angles (degrees)
function Mat4.rotation_euler(x, y, z)
    local mat = Mat4.identity()
    if type(x) == "table" then
        -- Assume it's a vec3-like object with rotation in degrees
        return mat:rotate(x)
    else
        local euler = Math.vec3(x or 0, y or 0, z or 0)
        return mat:rotate(euler)
    end
end

-- Create rotation matrix around X axis
function Mat4.rotation_x(angle_degrees)
    local mat = Mat4.identity()
    local rotation = Math.vec3(angle_degrees, 0, 0)
    return mat:rotate(rotation)
end

-- Create rotation matrix around Y axis
function Mat4.rotation_y(angle_degrees)
    local mat = Mat4.identity()
    local rotation = Math.vec3(0, angle_degrees, 0)
    return mat:rotate(rotation)
end

-- Create rotation matrix around Z axis
function Mat4.rotation_z(angle_degrees)
    local mat = Mat4.identity()
    local rotation = Math.vec3(0, 0, angle_degrees)
    return mat:rotate(rotation)
end

-- Create scale matrix
function Mat4.scaling(x, y, z)
    local mat = Mat4.identity()
    if type(x) == "table" then
        -- Assume it's a vec3-like object
        return mat:scale(x)
    else
        local scale_vec = Math.vec3(x or 1, y or x or 1, z or x or 1)  -- Uniform scaling if only x provided
        return mat:scale(scale_vec)
    end
end

-- Create transformation matrix (TRS - Translation, Rotation, Scale)
function Mat4.transformation(translation, rotation, scale)
    local t_mat = translation and Mat4.translation(translation) or Mat4.identity()
    local r_mat = rotation and Mat4.rotation_euler(rotation) or Mat4.identity()
    local s_mat = scale and Mat4.scaling(scale) or Mat4.identity()

    -- Apply in order: Scale, Rotate, Translate
    return t_mat * r_mat * s_mat
end

-- Create look-at view matrix
function Mat4.look_at(eye, center, up)
    -- This would need to be implemented in the C math bindings
    -- For now, return identity as placeholder
    error("Mat4.look_at not yet implemented - needs C binding support")
end

-- Create perspective projection matrix
function Mat4.perspective(fov_degrees, aspect_ratio, near_plane, far_plane)
    -- This would need to be implemented in the C math bindings
    -- For now, return identity as placeholder
    error("Mat4.perspective not yet implemented - needs C binding support")
end

-- Create orthographic projection matrix
function Mat4.orthographic(left, right, bottom, top, near_plane, far_plane)
    -- This would need to be implemented in the C math bindings
    -- For now, return identity as placeholder
    error("Mat4.orthographic not yet implemented - needs C binding support")
end

-- Decompose matrix into translation, rotation, scale
function Mat4.decompose(mat)
    -- This would need to be implemented in the C math bindings
    -- For now, return defaults as placeholder
    error("Mat4.decompose not yet implemented - needs C binding support")
end

-- Convert to string for debugging
function Mat4.to_string(mat)
    -- This would need access to matrix elements from C
    return "mat4(...)"  -- Placeholder
end

-- Helper function to create transform from position, rotation, scale
function Mat4.create_transform(pos, rot, scale)
    local transform = Math.transform()

    if pos then
        transform.position.x = pos.x or pos[1] or 0
        transform.position.y = pos.y or pos[2] or 0
        transform.position.z = pos.z or pos[3] or 0
    end

    if rot then
        transform.rotation.x = rot.x or rot[1] or 0
        transform.rotation.y = rot.y or rot[2] or 0
        transform.rotation.z = rot.z or rot[3] or 0
    end

    if scale then
        if type(scale) == "number" then
            transform.scale.x = scale
            transform.scale.y = scale
            transform.scale.z = scale
        else
            transform.scale.x = scale.x or scale[1] or 1
            transform.scale.y = scale.y or scale[2] or 1
            transform.scale.z = scale.z or scale[3] or 1
        end
    end

    return transform
end

return Mat4