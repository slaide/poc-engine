--[[
  Transform utility module for POC Engine

  Provides convenience functions for working with transforms (position, rotation, scale).
  Built on top of the core Math.transform bindings from C.
]]

local Transform = {}

-- Shorthand constructor
function Transform.new(position, rotation, scale)
    local transform = Math.transform()

    if position then
        transform.position.x = position.x or position[1] or 0
        transform.position.y = position.y or position[2] or 0
        transform.position.z = position.z or position[3] or 0
    end

    if rotation then
        transform.rotation.x = rotation.x or rotation[1] or 0
        transform.rotation.y = rotation.y or rotation[2] or 0
        transform.rotation.z = rotation.z or rotation[3] or 0
    end

    if scale then
        if type(scale) == "number" then
            -- Uniform scaling
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

-- Create transform at position
function Transform.at_position(x, y, z)
    if type(x) == "table" then
        return Transform.new(x)
    else
        return Transform.new({x = x or 0, y = y or 0, z = z or 0})
    end
end

-- Create transform with rotation
function Transform.with_rotation(x, y, z)
    if type(x) == "table" then
        return Transform.new(nil, x)
    else
        return Transform.new(nil, {x = x or 0, y = y or 0, z = z or 0})
    end
end

-- Create transform with scale
function Transform.with_scale(x, y, z)
    if type(x) == "number" and not y and not z then
        return Transform.new(nil, nil, x)  -- Uniform scale
    elseif type(x) == "table" then
        return Transform.new(nil, nil, x)
    else
        return Transform.new(nil, nil, {x = x or 1, y = y or 1, z = z or 1})
    end
end

-- Copy transform
function Transform.copy(transform)
    return Transform.new(
        {x = transform.position.x, y = transform.position.y, z = transform.position.z},
        {x = transform.rotation.x, y = transform.rotation.y, z = transform.rotation.z},
        {x = transform.scale.x, y = transform.scale.y, z = transform.scale.z}
    )
end

-- Set position
function Transform.set_position(transform, x, y, z)
    if type(x) == "table" then
        transform.position.x = x.x or x[1] or 0
        transform.position.y = x.y or x[2] or 0
        transform.position.z = x.z or x[3] or 0
    else
        transform.position.x = x or 0
        transform.position.y = y or 0
        transform.position.z = z or 0
    end
end

-- Set rotation
function Transform.set_rotation(transform, x, y, z)
    if type(x) == "table" then
        transform.rotation.x = x.x or x[1] or 0
        transform.rotation.y = x.y or x[2] or 0
        transform.rotation.z = x.z or x[3] or 0
    else
        transform.rotation.x = x or 0
        transform.rotation.y = y or 0
        transform.rotation.z = z or 0
    end
end

-- Set scale
function Transform.set_scale(transform, x, y, z)
    if type(x) == "number" and not y and not z then
        -- Uniform scale
        transform.scale.x = x
        transform.scale.y = x
        transform.scale.z = x
    elseif type(x) == "table" then
        transform.scale.x = x.x or x[1] or 1
        transform.scale.y = x.y or x[2] or 1
        transform.scale.z = x.z or x[3] or 1
    else
        transform.scale.x = x or 1
        transform.scale.y = y or 1
        transform.scale.z = z or 1
    end
end

-- Translate by offset
function Transform.translate(transform, x, y, z)
    if type(x) == "table" then
        transform.position.x = transform.position.x + (x.x or x[1] or 0)
        transform.position.y = transform.position.y + (x.y or x[2] or 0)
        transform.position.z = transform.position.z + (x.z or x[3] or 0)
    else
        transform.position.x = transform.position.x + (x or 0)
        transform.position.y = transform.position.y + (y or 0)
        transform.position.z = transform.position.z + (z or 0)
    end
end

-- Rotate by offset
function Transform.rotate(transform, x, y, z)
    if type(x) == "table" then
        transform.rotation.x = transform.rotation.x + (x.x or x[1] or 0)
        transform.rotation.y = transform.rotation.y + (x.y or x[2] or 0)
        transform.rotation.z = transform.rotation.z + (x.z or x[3] or 0)
    else
        transform.rotation.x = transform.rotation.x + (x or 0)
        transform.rotation.y = transform.rotation.y + (y or 0)
        transform.rotation.z = transform.rotation.z + (z or 0)
    end
end

-- Scale by factor
function Transform.scale_by(transform, x, y, z)
    if type(x) == "number" and not y and not z then
        -- Uniform scale
        transform.scale.x = transform.scale.x * x
        transform.scale.y = transform.scale.y * x
        transform.scale.z = transform.scale.z * x
    elseif type(x) == "table" then
        transform.scale.x = transform.scale.x * (x.x or x[1] or 1)
        transform.scale.y = transform.scale.y * (x.y or x[2] or 1)
        transform.scale.z = transform.scale.z * (x.z or x[3] or 1)
    else
        transform.scale.x = transform.scale.x * (x or 1)
        transform.scale.y = transform.scale.y * (y or 1)
        transform.scale.z = transform.scale.z * (z or 1)
    end
end

-- Get forward vector (assuming Y-up, -Z forward convention)
function Transform.get_forward(transform)
    -- This uses the transform's rotation to calculate forward direction
    -- For now, return a default forward vector
    -- TODO: Implement proper rotation-based forward calculation
    return Math.vec3(0, 0, -1)
end

-- Get right vector
function Transform.get_right(transform)
    -- This uses the transform's rotation to calculate right direction
    -- For now, return a default right vector
    -- TODO: Implement proper rotation-based right calculation
    return Math.vec3(1, 0, 0)
end

-- Get up vector
function Transform.get_up(transform)
    -- This uses the transform's rotation to calculate up direction
    -- For now, return a default up vector
    -- TODO: Implement proper rotation-based up calculation
    return Math.vec3(0, 1, 0)
end

-- Interpolate between two transforms
function Transform.lerp(from, to, t)
    local result = Transform.new()

    -- Interpolate position
    result.position.x = from.position.x + (to.position.x - from.position.x) * t
    result.position.y = from.position.y + (to.position.y - from.position.y) * t
    result.position.z = from.position.z + (to.position.z - from.position.z) * t

    -- Interpolate rotation (simple linear interpolation)
    result.rotation.x = from.rotation.x + (to.rotation.x - from.rotation.x) * t
    result.rotation.y = from.rotation.y + (to.rotation.y - from.rotation.y) * t
    result.rotation.z = from.rotation.z + (to.rotation.z - from.rotation.z) * t

    -- Interpolate scale
    result.scale.x = from.scale.x + (to.scale.x - from.scale.x) * t
    result.scale.y = from.scale.y + (to.scale.y - from.scale.y) * t
    result.scale.z = from.scale.z + (to.scale.z - from.scale.z) * t

    return result
end

-- Convert to string for debugging
function Transform.to_string(transform)
    return string.format(
        "Transform{pos=(%.2f,%.2f,%.2f), rot=(%.1f,%.1f,%.1f), scale=(%.2f,%.2f,%.2f)}",
        transform.position.x, transform.position.y, transform.position.z,
        transform.rotation.x, transform.rotation.y, transform.rotation.z,
        transform.scale.x, transform.scale.y, transform.scale.z
    )
end

-- Check if two transforms are approximately equal
function Transform.approximately_equal(a, b, tolerance)
    tolerance = tolerance or 1e-6

    -- Check position
    if math.abs(a.position.x - b.position.x) > tolerance or
       math.abs(a.position.y - b.position.y) > tolerance or
       math.abs(a.position.z - b.position.z) > tolerance then
        return false
    end

    -- Check rotation
    if math.abs(a.rotation.x - b.rotation.x) > tolerance or
       math.abs(a.rotation.y - b.rotation.y) > tolerance or
       math.abs(a.rotation.z - b.rotation.z) > tolerance then
        return false
    end

    -- Check scale
    if math.abs(a.scale.x - b.scale.x) > tolerance or
       math.abs(a.scale.y - b.scale.y) > tolerance or
       math.abs(a.scale.z - b.scale.z) > tolerance then
        return false
    end

    return true
end

return Transform