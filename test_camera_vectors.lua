-- Test script to verify camera direction vectors are accessible
print("Testing camera direction vectors...")

-- Create a camera
local camera = POC.create_camera("fps", 16/9)
if not camera then
    print("❌ Failed to create camera")
    return
end

-- Test direction vectors
print("Camera direction vectors:")
print("  Front:", camera.front.x, camera.front.y, camera.front.z)
print("  Right:", camera.right.x, camera.right.y, camera.right.z)
print("  Up:", camera.up.x, camera.up.y, camera.up.z)

-- Test vector math
local forward = camera.front
local scaled_forward = forward * 2.0
print("  Forward * 2:", scaled_forward.x, scaled_forward.y, scaled_forward.z)

print("✓ Camera direction vectors work!")