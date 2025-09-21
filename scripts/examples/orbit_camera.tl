--[[
  Orbit Camera Controller Script

  This script implements an orbit camera that rotates around a target point.
  It's useful for object inspection, 3D modeling applications, or cinematic
  camera movements. The camera maintains a fixed distance from the target
  while allowing rotation and zoom.

  Usage:
    Load this script and set a target object or point to orbit around.
    Use mouse drag for rotation and mouse wheel for zoom.
]]

require("poc_engine")

-- Camera configuration
local config = {
  orbit_speed = 1.0,        -- rotation speed multiplier
  zoom_speed = 0.5,         -- mouse wheel zoom speed
  min_distance = 1.0,       -- minimum zoom distance
  max_distance = 50.0,      -- maximum zoom distance
  initial_distance = 10.0,  -- starting distance from target
  smooth_factor = 8.0,      -- movement smoothing (higher = smoother)
  auto_rotate_speed = 0.2   -- automatic rotation speed (degrees/second)
}

-- Camera state
local camera_state = {
  target = {x = 0, y = 0, z = 0},  -- point to orbit around
  distance = config.initial_distance,
  yaw = 0.0,      -- horizontal rotation
  pitch = 20.0,   -- vertical rotation (slightly above target)

  -- Smoothed movement
  target_yaw = 0.0,
  target_pitch = 20.0,
  target_distance = config.initial_distance,

  -- Input tracking
  mouse_dragging = false,
  last_mouse_x = 0,
  last_mouse_y = 0,
  auto_rotate = false
}

-- Initialize the orbit camera
function init_orbit_camera(aspect_ratio: number, target_x: number, target_y: number, target_z: number)
  print("Initializing orbit camera controller...")

  -- Set target position
  camera_state.target = {x = target_x or 0, y = target_y or 0, z = target_z or 0}

  -- Create camera
  local success = POC.camera_create_orbit(
    camera_state.target,
    camera_state.distance,
    camera_state.yaw,
    camera_state.pitch,
    aspect_ratio
  )

  if success then
    print("✓ Orbit camera initialized")
    print(string.format("  Target: (%.1f, %.1f, %.1f)",
      camera_state.target.x, camera_state.target.y, camera_state.target.z))
    print("  Controls: Mouse drag = rotate, Wheel = zoom")
    print("  Press 'R' = toggle auto-rotate, 'T' = set new target")
  else
    print("✗ Failed to initialize orbit camera")
  end
end

-- Update camera with smooth interpolation
function update_orbit_camera(delta_time: number)
  -- Handle auto-rotation
  if camera_state.auto_rotate then
    camera_state.target_yaw = camera_state.target_yaw + (config.auto_rotate_speed * delta_time)
  end

  -- Smooth interpolation toward target values
  local lerp_factor = 1.0 - math.exp(-config.smooth_factor * delta_time)

  camera_state.yaw = camera_state.yaw + (camera_state.target_yaw - camera_state.yaw) * lerp_factor
  camera_state.pitch = camera_state.pitch + (camera_state.target_pitch - camera_state.pitch) * lerp_factor
  camera_state.distance = camera_state.distance + (camera_state.target_distance - camera_state.distance) * lerp_factor

  -- Calculate camera position from spherical coordinates
  local rad_yaw = math.rad(camera_state.yaw)
  local rad_pitch = math.rad(camera_state.pitch)

  local x = camera_state.target.x + camera_state.distance * math.cos(rad_yaw) * math.cos(rad_pitch)
  local y = camera_state.target.y + camera_state.distance * math.sin(rad_pitch)
  local z = camera_state.target.z + camera_state.distance * math.sin(rad_yaw) * math.cos(rad_pitch)

  -- Update camera position and make it look at target
  POC.camera_set_position({x = x, y = y, z = z})

  -- Calculate look direction (camera to target)
  local look_yaw = math.deg(math.atan2(camera_state.target.z - z, camera_state.target.x - x))
  local look_pitch = math.deg(math.asin((camera_state.target.y - y) / camera_state.distance))

  POC.camera_set_rotation(look_yaw, look_pitch, 0.0)
  POC.camera_update(delta_time)
end

-- Process keyboard input for special controls
function process_keyboard_input(key: integer, action: integer)
  if action == KEY_ACTION.PRESS then
    if key == KEY.R then
      camera_state.auto_rotate = not camera_state.auto_rotate
      local status = camera_state.auto_rotate and "enabled" or "disabled"
      print("Auto-rotate " .. status)

    elseif key == KEY.T then
      print("Click to set new target position...")
      -- In a real application, this might enable target selection mode

    elseif key == KEY.SPACE then
      reset_orbit_camera()

    elseif key == KEY.ESCAPE then
      print("ESC pressed - orbit camera controls disabled")
    end
  end
end

-- Process mouse movement for camera rotation
function process_mouse_movement(mouse_x: number, mouse_y: number, is_dragging: boolean)
  if not is_dragging and not camera_state.mouse_dragging then
    camera_state.last_mouse_x = mouse_x
    camera_state.last_mouse_y = mouse_y
    return
  end

  camera_state.mouse_dragging = is_dragging

  if is_dragging then
    -- Calculate mouse delta
    local x_offset = mouse_x - camera_state.last_mouse_x
    local y_offset = mouse_y - camera_state.last_mouse_y

    -- Apply orbit speed
    x_offset = x_offset * config.orbit_speed * 0.1
    y_offset = y_offset * config.orbit_speed * 0.1

    -- Update target rotation
    camera_state.target_yaw = camera_state.target_yaw + x_offset
    camera_state.target_pitch = camera_state.target_pitch - y_offset

    -- Constrain pitch to prevent flipping
    if camera_state.target_pitch > 89.0 then
      camera_state.target_pitch = 89.0
    elseif camera_state.target_pitch < -89.0 then
      camera_state.target_pitch = -89.0
    end

    -- Disable auto-rotate when manually controlling
    camera_state.auto_rotate = false
  end

  camera_state.last_mouse_x = mouse_x
  camera_state.last_mouse_y = mouse_y
end

-- Process mouse scroll for zoom
function process_mouse_scroll(scroll_y: number)
  -- Adjust target distance
  camera_state.target_distance = camera_state.target_distance - (scroll_y * config.zoom_speed)

  -- Clamp distance to bounds
  if camera_state.target_distance < config.min_distance then
    camera_state.target_distance = config.min_distance
  elseif camera_state.target_distance > config.max_distance then
    camera_state.target_distance = config.max_distance
  end

  print(string.format("Distance: %.1f", camera_state.target_distance))
end

-- Set a new target position
function set_target(x: number, y: number, z: number)
  camera_state.target = {x = x, y = y, z = z}
  print(string.format("Orbit target set to (%.1f, %.1f, %.1f)", x, y, z))
end

-- Get current camera information
function get_camera_info(): string
  return string.format("Target: (%.1f, %.1f, %.1f) Distance: %.1f Yaw: %.1f Pitch: %.1f",
    camera_state.target.x, camera_state.target.y, camera_state.target.z,
    camera_state.distance, camera_state.yaw, camera_state.pitch)
end

-- Reset camera to initial state
function reset_orbit_camera()
  camera_state.yaw = 0.0
  camera_state.pitch = 20.0
  camera_state.distance = config.initial_distance
  camera_state.target_yaw = 0.0
  camera_state.target_pitch = 20.0
  camera_state.target_distance = config.initial_distance
  camera_state.auto_rotate = false

  print("Orbit camera reset to initial position")
end

-- Frame a target object by setting appropriate distance
function frame_target(object_size: number)
  local ideal_distance = object_size * 2.5  -- Reasonable framing distance
  camera_state.target_distance = math.max(ideal_distance, config.min_distance)
  camera_state.target_distance = math.min(camera_state.target_distance, config.max_distance)

  print(string.format("Camera framed target (size: %.1f, distance: %.1f)",
    object_size, camera_state.target_distance))
end

-- Animate to a specific view
function animate_to_view(yaw: number, pitch: number, distance: number, duration: number)
  -- Set target values for smooth interpolation
  camera_state.target_yaw = yaw
  camera_state.target_pitch = pitch
  camera_state.target_distance = distance

  print(string.format("Animating to view: yaw=%.1f, pitch=%.1f, distance=%.1f",
    yaw, pitch, distance))
end

-- Export functions for use by the main application
return {
  init = init_orbit_camera,
  update = update_orbit_camera,
  process_keyboard = process_keyboard_input,
  process_mouse_movement = process_mouse_movement,
  process_mouse_scroll = process_mouse_scroll,
  set_target = set_target,
  get_info = get_camera_info,
  reset = reset_orbit_camera,
  frame_target = frame_target,
  animate_to_view = animate_to_view,
  config = config
}