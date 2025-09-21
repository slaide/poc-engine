--[[
  Vec3 utility module for POC Engine

  Provides convenience functions and constants for working with 3D vectors.
  Built on top of the core Math.vec3 bindings from C.
]]

local Vec3 = {}

-- Common vector constants
Vec3.ZERO = Math.vec3(0, 0, 0)
Vec3.ONE = Math.vec3(1, 1, 1)
Vec3.UP = Math.vec3(0, 1, 0)
Vec3.DOWN = Math.vec3(0, -1, 0)
Vec3.FORWARD = Math.vec3(0, 0, -1)  -- OpenGL convention: -Z is forward
Vec3.BACK = Math.vec3(0, 0, 1)
Vec3.LEFT = Math.vec3(-1, 0, 0)
Vec3.RIGHT = Math.vec3(1, 0, 0)

-- Shorthand constructor
function Vec3.new(x, y, z)
    return Math.vec3(x or 0, y or 0, z or 0)
end

-- Create from table
function Vec3.from_table(t)
    return Math.vec3(t[1] or t.x or 0, t[2] or t.y or 0, t[3] or t.z or 0)
end

-- Create random unit vector
function Vec3.random_unit()
    local theta = math.random() * 2 * math.pi  -- Azimuth angle
    local phi = math.acos(2 * math.random() - 1)  -- Polar angle

    local x = math.sin(phi) * math.cos(theta)
    local y = math.sin(phi) * math.sin(theta)
    local z = math.cos(phi)

    return Math.vec3(x, y, z)
end

-- Create random vector in range
function Vec3.random_range(min_val, max_val)
    local range = max_val - min_val
    return Math.vec3(
        min_val + math.random() * range,
        min_val + math.random() * range,
        min_val + math.random() * range
    )
end

-- Clamp vector components to range
function Vec3.clamp(v, min_val, max_val)
    return Math.vec3(
        math.max(min_val, math.min(max_val, v.x)),
        math.max(min_val, math.min(max_val, v.y)),
        math.max(min_val, math.min(max_val, v.z))
    )
end

-- Spherical linear interpolation
function Vec3.slerp(a, b, t)
    local dot = a:dot(b)

    -- If vectors are nearly parallel, use linear interpolation
    if math.abs(dot) > 0.9995 then
        return a:lerp(b, t):normalize()
    end

    -- Calculate angle between vectors
    local angle = math.acos(math.abs(dot))
    local sin_angle = math.sin(angle)

    local factor_a = math.sin((1 - t) * angle) / sin_angle
    local factor_b = math.sin(t * angle) / sin_angle

    return (a * factor_a + b * factor_b):normalize()
end

-- Calculate reflection vector
function Vec3.reflect(incident, normal)
    return incident - normal * (2 * incident:dot(normal))
end

-- Calculate refraction vector
function Vec3.refract(incident, normal, eta)
    local cos_i = -incident:dot(normal)
    local sin_t2 = eta * eta * (1.0 - cos_i * cos_i)

    if sin_t2 >= 1.0 then
        return nil  -- Total internal reflection
    end

    local cos_t = math.sqrt(1.0 - sin_t2)
    return incident * eta + normal * (eta * cos_i - cos_t)
end

-- Project vector onto another vector
function Vec3.project(a, b)
    local b_length_sq = b:dot(b)
    if b_length_sq == 0 then
        return Vec3.ZERO
    end
    return b * (a:dot(b) / b_length_sq)
end

-- Reject vector from another vector (orthogonal component)
function Vec3.reject(a, b)
    return a - Vec3.project(a, b)
end

-- Angle between two vectors in radians
function Vec3.angle(a, b)
    local dot = a:dot(b)
    local lengths = a:length() * b:length()
    if lengths == 0 then
        return 0
    end
    return math.acos(math.max(-1, math.min(1, dot / lengths)))
end

-- Signed angle between two vectors around an axis
function Vec3.signed_angle(from, to, axis)
    local unsigned_angle = Vec3.angle(from, to)
    local cross_product = from:cross(to)
    local sign = cross_product:dot(axis)
    return sign >= 0 and unsigned_angle or -unsigned_angle
end

-- Convert to string for debugging
function Vec3.to_string(v)
    return string.format("vec3(%.3f, %.3f, %.3f)", v.x, v.y, v.z)
end

-- Check if two vectors are approximately equal
function Vec3.approximately_equal(a, b, tolerance)
    tolerance = tolerance or 1e-6
    return Vec3.distance(a, b) <= tolerance
end

-- Create basis vectors from a forward direction
function Vec3.create_basis(forward)
    forward = forward:normalize()

    -- Choose an arbitrary up vector that's not parallel to forward
    local up = math.abs(forward:dot(Vec3.UP)) < 0.99 and Vec3.UP or Vec3.RIGHT

    local right = forward:cross(up):normalize()
    up = right:cross(forward):normalize()

    return forward, right, up
end

return Vec3