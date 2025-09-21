--[[
  Input Handler Script

  This script provides a centralized input handling system that can
  coordinate between different camera controllers and other game systems.
  It demonstrates how to create a flexible input mapping system using Lua.

  Usage:
    Load this script early in initialization and route all input events
    through it. It will dispatch to the appropriate handlers based on
    the current mode.
]]

require("poc_engine")

-- Input modes
local InputMode = {
  FPS_CAMERA = 1,
  ORBIT_CAMERA = 2,
  FREE_CAMERA = 3,
  UI_MENU = 4
}

-- Input state
local input_state = {
  current_mode = InputMode.FPS_CAMERA,
  mouse_captured = false,
  key_bindings = {},
  mouse_sensitivity = 1.0,
  controllers = {}
}

-- Initialize input system
function init_input_system()
  print("Initializing input handler...")

  -- Set up default key bindings
  setup_default_keybindings()

  -- Initialize mouse state
  input_state.mouse_captured = true
  print("✓ Input system initialized")
  print("  Press TAB to switch camera modes")
  print("  Press F1 to toggle mouse capture")
end

-- Set up default key bindings
function setup_default_keybindings()
  local bindings = input_state.key_bindings

  -- Movement keys (common across camera types)
  bindings[KEY.W] = "move_forward"
  bindings[KEY.S] = "move_backward"
  bindings[KEY.A] = "move_left"
  bindings[KEY.D] = "move_right"
  bindings[KEY.Q] = "move_up"
  bindings[KEY.E] = "move_down"
  bindings[KEY.SPACE] = "jump_or_up"
  bindings[KEY.LEFT_SHIFT] = "run_or_down"

  -- Camera mode switching
  bindings[KEY.TAB] = "switch_camera_mode"
  bindings[KEY.F1] = "toggle_mouse_capture"

  -- Special actions
  bindings[KEY.R] = "reset_camera"
  bindings[KEY.T] = "toggle_auto_rotate"
  bindings[KEY.F] = "frame_target"
  bindings[KEY.ESCAPE] = "escape_menu"

  print("Default key bindings loaded")
end

-- Register a camera controller
function register_controller(mode: integer, controller: table)
  input_state.controllers[mode] = controller
  print(string.format("Registered controller for mode %d", mode))
end

-- Set the current input mode
function set_input_mode(mode: integer)
  if input_state.controllers[mode] then
    input_state.current_mode = mode
    local mode_names = {"FPS", "Orbit", "Free", "UI Menu"}
    print(string.format("Switched to %s camera mode", mode_names[mode] or "Unknown"))
  else
    print(string.format("No controller registered for mode %d", mode))
  end
end

-- Get current input mode
function get_input_mode(): integer
  return input_state.current_mode
end

-- Process keyboard input
function process_keyboard_input(key: integer, action: integer, delta_time: number)
  local binding = input_state.key_bindings[key]

  -- Handle global key bindings first
  if binding and action == KEY_ACTION.PRESS then
    if binding == "switch_camera_mode" then
      switch_camera_mode()
      return
    elseif binding == "toggle_mouse_capture" then
      toggle_mouse_capture()
      return
    elseif binding == "escape_menu" then
      handle_escape_key()
      return
    end
  end

  -- Route to current controller
  local controller = input_state.controllers[input_state.current_mode]
  if controller and controller.process_keyboard then
    controller.process_keyboard(key, action, delta_time)
  end

  -- Log key events for debugging
  if action == KEY_ACTION.PRESS then
    local action_name = binding or "unbound"
    print(string.format("Key pressed: %d (%s)", key, action_name))
  end
end

-- Process mouse movement
function process_mouse_movement(mouse_x: number, mouse_y: number)
  if not input_state.mouse_captured then
    return
  end

  -- Apply global mouse sensitivity
  mouse_x = mouse_x * input_state.mouse_sensitivity
  mouse_y = mouse_y * input_state.mouse_sensitivity

  -- Route to current controller
  local controller = input_state.controllers[input_state.current_mode]
  if controller and controller.process_mouse_movement then
    if input_state.current_mode == InputMode.ORBIT_CAMERA then
      -- For orbit camera, pass additional drag state
      controller.process_mouse_movement(mouse_x, mouse_y, true)
    else
      controller.process_mouse_movement(mouse_x, mouse_y)
    end
  end
end

-- Process mouse scroll
function process_mouse_scroll(scroll_y: number)
  -- Route to current controller
  local controller = input_state.controllers[input_state.current_mode]
  if controller and controller.process_mouse_scroll then
    controller.process_mouse_scroll(scroll_y)
  end
end

-- Update input system each frame
function update_input_system(delta_time: number)
  -- Update current controller
  local controller = input_state.controllers[input_state.current_mode]
  if controller and controller.update then
    controller.update(delta_time)
  end
end

-- Switch between camera modes
function switch_camera_mode()
  local next_mode = input_state.current_mode + 1
  if next_mode > InputMode.FREE_CAMERA then
    next_mode = InputMode.FPS_CAMERA
  end

  -- Skip modes that don't have controllers
  while next_mode ~= input_state.current_mode do
    if input_state.controllers[next_mode] then
      set_input_mode(next_mode)
      break
    end
    next_mode = next_mode + 1
    if next_mode > InputMode.FREE_CAMERA then
      next_mode = InputMode.FPS_CAMERA
    end
  end
end

-- Toggle mouse capture (for debugging or UI access)
function toggle_mouse_capture()
  input_state.mouse_captured = not input_state.mouse_captured
  local status = input_state.mouse_captured and "captured" or "released"
  print("Mouse " .. status)

  -- In a real implementation, this would call into the window system
  -- to actually capture or release the mouse cursor
end

-- Handle escape key
function handle_escape_key()
  if input_state.current_mode == InputMode.UI_MENU then
    -- Close UI menu, return to previous camera mode
    set_input_mode(InputMode.FPS_CAMERA)
  else
    -- Open UI menu or quit
    print("Escape pressed - opening menu or quitting")
    -- In a real application, this might open a pause menu
  end
end

-- Set mouse sensitivity
function set_mouse_sensitivity(sensitivity: number)
  input_state.mouse_sensitivity = sensitivity
  print(string.format("Mouse sensitivity set to %.2f", sensitivity))
end

-- Get current mouse sensitivity
function get_mouse_sensitivity(): number
  return input_state.mouse_sensitivity
end

-- Bind a key to an action
function bind_key(key: integer, action: string)
  input_state.key_bindings[key] = action
  print(string.format("Bound key %d to action '%s'", key, action))
end

-- Get key binding
function get_key_binding(key: integer): string
  return input_state.key_bindings[key] or "unbound"
end

-- Print current input state for debugging
function print_input_state()
  print("=== Input System State ===")
  print(string.format("Current mode: %d", input_state.current_mode))
  print(string.format("Mouse captured: %s", tostring(input_state.mouse_captured)))
  print(string.format("Mouse sensitivity: %.2f", input_state.mouse_sensitivity))

  local controller_count = 0
  for mode, _ in pairs(input_state.controllers) do
    controller_count = controller_count + 1
  end
  print(string.format("Registered controllers: %d", controller_count))

  -- Print current controller info if available
  local controller = input_state.controllers[input_state.current_mode]
  if controller and controller.get_info then
    print("Controller info: " .. controller.get_info())
  end
  print("========================")
end

-- Export the input handler interface
return {
  -- Core functions
  init = init_input_system,
  update = update_input_system,

  -- Input processing
  process_keyboard = process_keyboard_input,
  process_mouse_movement = process_mouse_movement,
  process_mouse_scroll = process_mouse_scroll,

  -- Mode management
  register_controller = register_controller,
  set_mode = set_input_mode,
  get_mode = get_input_mode,
  switch_mode = switch_camera_mode,

  -- Configuration
  set_mouse_sensitivity = set_mouse_sensitivity,
  get_mouse_sensitivity = get_mouse_sensitivity,
  bind_key = bind_key,
  get_key_binding = get_key_binding,

  -- Utilities
  toggle_mouse_capture = toggle_mouse_capture,
  print_state = print_input_state,

  -- Constants
  InputMode = InputMode
}