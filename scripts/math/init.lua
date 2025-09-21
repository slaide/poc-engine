--[[
  Math module initialization for POC Engine

  This file loads and exports all math utility modules.
  Usage:
    local Math = require("math.init")
    local vec = Math.Vec3.new(1, 2, 3)
    local mat = Math.Mat4.identity()
    local transform = Math.Transform.new({x=0, y=0, z=0})
]]

local math_utils = {}

-- Load utility modules
math_utils.Vec3 = require("math.vec3")
math_utils.Mat4 = require("math.mat4")
math_utils.Transform = require("math.transform")

-- Common math constants and functions
math_utils.PI = math.pi
math_utils.TWO_PI = 2 * math.pi
math_utils.HALF_PI = math.pi / 2
math_utils.DEG_TO_RAD = math.pi / 180
math_utils.RAD_TO_DEG = 180 / math.pi

-- Utility functions
function math_utils.degrees(radians)
    return radians * math_utils.RAD_TO_DEG
end

function math_utils.radians(degrees)
    return degrees * math_utils.DEG_TO_RAD
end

function math_utils.clamp(value, min_val, max_val)
    return math.max(min_val, math.min(max_val, value))
end

function math_utils.lerp(a, b, t)
    return a + (b - a) * t
end

function math_utils.smoothstep(edge0, edge1, x)
    local t = math_utils.clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
end

function math_utils.smootherstep(edge0, edge1, x)
    local t = math_utils.clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)
end

-- Fast approximations
function math_utils.fast_sin(x)
    -- Fast sine approximation using Taylor series (limited accuracy)
    local x2 = x * x
    return x * (1 - x2 / 6 + x2 * x2 / 120)
end

function math_utils.fast_cos(x)
    -- Fast cosine approximation
    local x2 = x * x
    return 1 - x2 / 2 + x2 * x2 / 24
end

-- Approximate equality comparison
function math_utils.approximately_equal(a, b, tolerance)
    tolerance = tolerance or 1e-6
    return math.abs(a - b) <= tolerance
end

-- Wrap angle to [-pi, pi] range
function math_utils.wrap_angle(angle)
    while angle > math.pi do
        angle = angle - math_utils.TWO_PI
    end
    while angle < -math.pi do
        angle = angle + math_utils.TWO_PI
    end
    return angle
end

-- Wrap angle to [0, 2*pi] range
function math_utils.wrap_angle_positive(angle)
    while angle >= math_utils.TWO_PI do
        angle = angle - math_utils.TWO_PI
    end
    while angle < 0 do
        angle = angle + math_utils.TWO_PI
    end
    return angle
end

-- Sign function
function math_utils.sign(x)
    if x > 0 then return 1
    elseif x < 0 then return -1
    else return 0 end
end

-- Round to nearest integer
function math_utils.round(x)
    return math.floor(x + 0.5)
end

-- Random float in range [min, max]
function math_utils.random_range(min_val, max_val)
    return min_val + math.random() * (max_val - min_val)
end

-- Random integer in range [min, max] (inclusive)
function math_utils.random_int_range(min_val, max_val)
    return math.random(min_val, max_val)
end

return math_utils