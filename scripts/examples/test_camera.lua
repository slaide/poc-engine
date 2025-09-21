--[[
  Object-Oriented Lua Camera Test Script

  This script demonstrates the new object-oriented POC Engine Lua bindings
  for camera control using userdata objects with metatables.
]]

print("=== POC Engine Lua Camera Test (Userdata API) ===")

-- Test basic engine functions
local current_time = POC.get_time()
print(string.format("Engine time: %.3f seconds", current_time))

-- Test camera creation
print("\nCreating FPS camera...")
local camera, error_msg = POC.create_camera("fps", 16/9)

if camera then
    print("✓ FPS camera created successfully")

    -- Test camera position access
    print(string.format("Initial position: (%.1f, %.1f, %.1f)", camera.position.x, camera.position.y, camera.position.z))

    -- Test camera rotation access
    print(string.format("Initial rotation: yaw=%.1f, pitch=%.1f", camera.yaw, camera.pitch))

    -- Test setting position properties
    camera.position.x = 1.0
    camera.position.y = 3.0
    camera.position.z = 7.0
    print(string.format("New position: (%.1f, %.1f, %.1f)", camera.position.x, camera.position.y, camera.position.z))

    -- Test setting rotation properties
    camera.yaw = -90.0
    camera.pitch = 15.0
    print(string.format("New rotation: yaw=%.1f, pitch=%.1f", camera.yaw, camera.pitch))

    -- Test FOV setting
    camera.fov = 65.0
    print(string.format("✓ FOV set to %.1f degrees", camera.fov))

    -- Test camera update
    camera:update(0.016) -- 60 FPS delta time
    print("✓ Camera updated")

    -- Test binding camera to context
    POC.bind_camera(camera)
    print("✓ Camera bound to rendering context")

    success = true
else
    print("✗ Failed to create FPS camera: " .. (error_msg or "unknown error"))
    success = false
end

print("\n=== Camera test completed ===")

-- Return a simple module for the application to use
return {
    camera = camera,
    test_passed = success,
    message = success and "Camera test passed!" or "Camera test failed!"
}