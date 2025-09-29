--[[
  POC Engine Teal Type Definitions

  This file provides type-safe Teal definitions for the POC Engine API.
  It allows Teal scripts to have compile-time type checking when interacting
  with the engine's Lua bindings.

  Usage:
    local pocengine = require("poc_engine")

  Example:
    local camera_pos: Vec3 = {x = 0, y = 1, z = 5}
    POC.camera_set_position(camera_pos)
]]

-- Basic types

---@alias Vec3 {x: number, y: number, z: number}
---@alias Mat4 {number}  -- Array of 16 numbers

-- Enums

---@alias CameraType
---| "FIRST_PERSON"  # FPS-style camera
---| "ORBIT"         # Orbit around target
---| "FREE"          # Free-flying camera

---@alias KeyCode
---| "W"
---| "A"
---| "S"
---| "D"
---| "Q"
---| "E"
---| "SPACE"
---| "LEFT_SHIFT"
---| "ESCAPE"

---@alias KeyAction
---| "PRESS"
---| "RELEASE"
---| "REPEAT"

-- Global constants tables that are registered by the engine
global CAMERA_TYPE: {
  FIRST_PERSON: integer,
  ORBIT: integer,
  FREE: integer
}

global KEY: {
  W: integer,
  A: integer,
  S: integer,
  D: integer,
  Q: integer,
  E: integer,
  SPACE: integer,
  LEFT_SHIFT: integer,
  ESCAPE: integer
}

global KEY_ACTION: {
  PRESS: integer,
  RELEASE: integer,
  REPEAT: integer
}

-- POC Engine API
global POC: {
  -- Core engine functions
  get_time: function(): number,
  sleep: function(seconds: number),

  -- Camera creation functions
  camera_create: function(camera_type: integer, aspect_ratio: number): boolean,
  camera_create_fps: function(position: Vec3, yaw: number, pitch: number, aspect_ratio: number): boolean,
  camera_create_orbit: function(target: Vec3, distance: number, yaw: number, pitch: number, aspect_ratio: number): boolean,

  -- Camera property functions
  camera_set_position: function(position: Vec3),
  camera_set_rotation: function(yaw: number, pitch: number, roll: number),
  camera_set_fov: function(fov: number),
  camera_set_vertical_fov: function(fov: number),
  camera_set_horizontal_fov: function(fov: number),

  -- Camera query functions
  camera_get_position: function(): Vec3,
  camera_get_rotation: function(): number, number, number,  -- yaw, pitch, roll
  camera_get_view_matrix: function(): Mat4,
  camera_get_projection_matrix: function(): Mat4,
  camera_get_vertical_fov: function(): number,
  camera_get_horizontal_fov: function(): number,

  -- Camera input processing
  camera_process_keyboard: function(key: integer, action: integer, delta_time: number),
  camera_process_mouse_movement: function(mouse_x: number, mouse_y: number, constrain_pitch: boolean),
  camera_process_mouse_scroll: function(scroll_y: number),

  -- Camera update
  camera_update: function(delta_time: number)
}

-- Helper functions for creating Vec3 objects
local Vec3 = {
  new = function(x: number, y: number, z: number): Vec3
    return {x = x, y = y, z = z}
  end,

  zero = function(): Vec3
    return {x = 0, y = 0, z = 0}
  end,

  up = function(): Vec3
    return {x = 0, y = 1, z = 0}
  end,

  forward = function(): Vec3
    return {x = 0, y = 0, z = -1}
  end,

  right = function(): Vec3
    return {x = 1, y = 0, z = 0}
  end
}

-- Input handling utilities
local Input = {
  -- Check if a key is being held down (for movement)
  is_key_down = function(key: integer): boolean
    -- This would need to be implemented by tracking key states
    -- For now, this is just a type definition
    return false
  end,

  -- Get mouse delta since last frame
  get_mouse_delta = function(): number, number
    -- This would need to be implemented by tracking mouse state
    return 0, 0
  end
}

-- Camera controller utilities
local CameraController = {
  -- Create an FPS controller that responds to standard WASD + mouse input
  create_fps_controller = function(camera_speed: number, mouse_sensitivity: number): {
    update: function(delta_time: number),
    process_input: function(key: integer, action: integer),
    process_mouse: function(x: number, y: number)
  }
    return {
      update = function(delta_time: number)
        -- FPS camera update logic would go here
      end,

      process_input = function(key: integer, action: integer)
        POC.camera_process_keyboard(key, action, delta_time or 0.016)
      end,

      process_mouse = function(x: number, y: number)
        POC.camera_process_mouse_movement(x, y, true)
      end
    }
  end,

  -- Create an orbit controller that responds to mouse drag for rotation
  create_orbit_controller = function(orbit_speed: number, zoom_speed: number): {
    update: function(delta_time: number),
    process_mouse: function(x: number, y: number),
    process_scroll: function(scroll_y: number)
  }
    return {
      update = function(delta_time: number)
        POC.camera_update(delta_time)
      end,

      process_mouse = function(x: number, y: number)
        POC.camera_process_mouse_movement(x, y, true)
      end,

      process_scroll = function(scroll_y: number)
        POC.camera_process_mouse_scroll(scroll_y)
      end
    }
  end
}

-- Export the utility modules
return {
  Vec3 = Vec3,
  Input = Input,
  CameraController = CameraController
}
