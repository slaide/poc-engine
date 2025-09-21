--[[
  FPS Camera Controller Script

  This script demonstrates a first-person camera controller similar to
  those found in FPS games. It responds to WASD for movement, mouse for
  looking around, and mouse wheel for zoom/field of view adjustment.

  Usage:
    Load this script in your engine initialization and call the update
    functions from your main loop.
]]

require("poc_engine")

-- Camera configuration
local config = {
  movement_speed = 5.0,      -- units per second
  mouse_sensitivity = 0.15,  -- mouse look sensitivity
  zoom_speed = 2.0,          -- mouse wheel zoom speed
  initial_fov = 75.0,        -- starting field of view
  min_fov = 10.0,           -- minimum zoom
  max_fov = 120.0           -- maximum zoom
}

-- Camera state
local camera_state = {
  position = {x = 0, y = 2, z = 5},
  yaw = -90.0,    -- looking down negative Z
  pitch = 0.0,
  fov = config.initial_fov,

  -- Input tracking
  keys_down = {},
  last_mouse_x = 0,
  last_mouse_y = 0,
  first_mouse = true
}

-- Initialize the FPS camera
function init_fps_camera(aspect_ratio: number)
  print("Initializing FPS camera controller...")

  -- Create camera
  local success = POC.camera_create_fps(
    camera_state.position,
    camera_state.yaw,
    camera_state.pitch,
    aspect_ratio
  )

  if success then
    POC.camera_set_fov(config.initial_fov)
    print("✓ FPS camera initialized")
    print("  Controls: WASD = move, Mouse = look, Wheel = zoom")
    print("  Hold Shift = move faster, Space = up, Ctrl = down")
  else
    print("✗ Failed to initialize FPS camera")
  end
end

-- Update camera movement based on input
function update_fps_camera(delta_time: number)
  POC.camera_update(delta_time)
end

-- Process keyboard input for movement
function process_keyboard_input(key: integer, action: integer, delta_time: number)
  -- Track key states for smooth movement
  if action == KEY_ACTION.PRESS or action == KEY_ACTION.REPEAT then
    camera_state.keys_down[key] = true
  elseif action == KEY_ACTION.RELEASE then
    camera_state.keys_down[key] = false
  end

  -- Pass to camera for processing
  POC.camera_process_keyboard(key, action, delta_time)

  -- Handle special keys
  if key == KEY.ESCAPE and action == KEY_ACTION.PRESS then
    print("ESC pressed - camera controls disabled")
    -- In a real application, this might toggle mouse capture
  end
end

-- Process mouse movement for camera look
function process_mouse_movement(mouse_x: number, mouse_y: number)
  -- Initialize mouse position on first movement
  if camera_state.first_mouse then
    camera_state.last_mouse_x = mouse_x
    camera_state.last_mouse_y = mouse_y
    camera_state.first_mouse = false
    return
  end

  -- Calculate mouse delta
  local x_offset = mouse_x - camera_state.last_mouse_x
  local y_offset = camera_state.last_mouse_y - mouse_y  -- Reversed for correct Y direction

  camera_state.last_mouse_x = mouse_x
  camera_state.last_mouse_y = mouse_y

  -- Apply sensitivity
  x_offset = x_offset * config.mouse_sensitivity
  y_offset = y_offset * config.mouse_sensitivity

  -- Update camera orientation
  camera_state.yaw = camera_state.yaw + x_offset
  camera_state.pitch = camera_state.pitch + y_offset

  -- Constrain pitch to prevent camera flipping
  if camera_state.pitch > 89.0 then
    camera_state.pitch = 89.0
  elseif camera_state.pitch < -89.0 then
    camera_state.pitch = -89.0
  end

  -- Apply to camera
  POC.camera_set_rotation(camera_state.yaw, camera_state.pitch, 0.0)
end

-- Process mouse scroll for zoom/FOV
function process_mouse_scroll(scroll_y: number)
  -- Adjust field of view for zoom effect
  camera_state.fov = camera_state.fov - (scroll_y * config.zoom_speed)

  -- Clamp FOV to reasonable bounds
  if camera_state.fov < config.min_fov then
    camera_state.fov = config.min_fov
  elseif camera_state.fov > config.max_fov then
    camera_state.fov = config.max_fov
  end

  POC.camera_set_fov(camera_state.fov)

  print(string.format("FOV: %.1f", camera_state.fov))
end

-- Get current camera position (for debugging)
function get_camera_info(): string
  local pos = POC.camera_get_position()
  return string.format("Pos: (%.1f, %.1f, %.1f) Yaw: %.1f Pitch: %.1f FOV: %.1f",
    pos.x, pos.y, pos.z, camera_state.yaw, camera_state.pitch, camera_state.fov)
end

-- Set camera position (useful for teleporting or respawning)
function set_camera_position(x: number, y: number, z: number)
  camera_state.position = {x = x, y = y, z = z}
  POC.camera_set_position(camera_state.position)
  print(string.format("Camera moved to (%.1f, %.1f, %.1f)", x, y, z))
end

-- Reset camera to initial state
function reset_camera()
  camera_state.position = {x = 0, y = 2, z = 5}
  camera_state.yaw = -90.0
  camera_state.pitch = 0.0
  camera_state.fov = config.initial_fov
  camera_state.first_mouse = true

  POC.camera_set_position(camera_state.position)
  POC.camera_set_rotation(camera_state.yaw, camera_state.pitch, 0.0)
  POC.camera_set_fov(camera_state.fov)

  print("Camera reset to initial position")
end

-- Export functions for use by the main application
return {
  init = init_fps_camera,
  update = update_fps_camera,
  process_keyboard = process_keyboard_input,
  process_mouse_movement = process_mouse_movement,
  process_mouse_scroll = process_mouse_scroll,
  get_info = get_camera_info,
  set_position = set_camera_position,
  reset = reset_camera,
  config = config
}