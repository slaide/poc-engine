-- Test script to verify camera direction vectors are accessible
print("=== Testing Camera Direction Vectors ===")

-- Create a camera
local camera = POC.create_camera("fps", 16/9)
if not camera then
    print("❌ Failed to create camera")
    return
end

print("✓ Camera created successfully")

-- Test direction vectors exist
print("Testing vector access...")
local front = camera.front
local right = camera.right
local up = camera.up

if front and right and up then
    print("✓ All direction vectors accessible")
    print("  Front:", front.x, front.y, front.z)
    print("  Right:", right.x, right.y, right.z)
    print("  Up:", up.x, up.y, up.z)

    -- Test vector math operations
    local scaled_forward = front * 2.0
    print("  Forward * 2:", scaled_forward.x, scaled_forward.y, scaled_forward.z)

    -- Test vector addition
    local combined = front + right
    print("  Front + Right:", combined.x, combined.y, combined.z)

    print("✓ Camera direction vectors and math work!")
else
    print("❌ Some direction vectors not accessible")
    print("  front:", front)
    print("  right:", right)
    print("  up:", up)
end