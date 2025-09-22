--[[
  FPS Camera Controller

  This script provides a complete FPS-style camera controller with mouse look
  and WASD movement controls. It demonstrates advanced usage of the POC Engine
  camera userdata system.
]]

local FPSCamera = {}
FPSCamera.__index = FPSCamera

-- Default settings
local DEFAULT_SETTINGS = {
    move_speed = 5.0,
    mouse_sensitivity = 0.1,
    max_pitch = 89.0,
    initial_position = {x = 0, y = 2, z = 5},
    initial_yaw = -90.0,
    initial_pitch = 0.0,
    fov = 45.0,
    mouse_look_mode = false  -- Enable/disable mouse look mode (true = always on, false = hold button)
}

function FPSCamera.new(settings)
    local self = setmetatable({}, FPSCamera)

    -- Merge settings with defaults
    self.settings = {}
    for k, v in pairs(DEFAULT_SETTINGS) do
        self.settings[k] = v
    end
    if settings then
        for k, v in pairs(settings) do
            self.settings[k] = v
        end
    end

    -- Create the POC camera object
    local aspect_ratio = 16/9  -- Could be made configurable
    self.camera = POC.create_camera("fps", aspect_ratio)
    if not self.camera then
        error("Failed to create FPS camera object")
    end

    -- Initialize camera properties
    self.camera.position.x = self.settings.initial_position.x
    self.camera.position.y = self.settings.initial_position.y
    self.camera.position.z = self.settings.initial_position.z
    self.camera.yaw = self.settings.initial_yaw
    self.camera.pitch = self.settings.initial_pitch
    self.camera.fov = self.settings.fov

    -- Key states for smooth movement
    self.keys = {
        forward = false,
        backward = false,
        left = false,
        right = false,
        up = false,
        down = false
    }

    -- Mouse state
    self.first_mouse = true
    self.last_mouse_x = 0
    self.last_mouse_y = 0
    self.mouse_look_active = false  -- Whether mouse look is currently active

    print("✓ FPS Camera Controller initialized")
    print(string.format("  Position: (%.1f, %.1f, %.1f)",
          self.camera.position.x, self.camera.position.y, self.camera.position.z))
    print(string.format("  Rotation: yaw=%.1f°, pitch=%.1f°",
          self.camera.yaw, self.camera.pitch))
    print(string.format("  Settings: speed=%.1f, sensitivity=%.2f, fov=%.1f°",
          self.settings.move_speed, self.settings.mouse_sensitivity, self.settings.fov))

    return self
end

function FPSCamera:bind()
    -- Bind this camera to the rendering context
    POC.bind_camera(self.camera)
    print("✓ FPS camera bound to rendering context")
end

function FPSCamera:update(delta_time)
    -- Handle movement based on current key states using proper vector math
    local velocity = self.settings.move_speed * delta_time
    local moved = false

    -- Get camera direction vectors for proper movement
    local forward = self.camera.front
    local right = self.camera.right
    local up = self.camera.up

    if self.keys.forward then
        -- Move forward in the direction the camera is facing
        local movement = forward * velocity
        self.camera.position.x = self.camera.position.x + movement.x
        self.camera.position.y = self.camera.position.y + movement.y
        self.camera.position.z = self.camera.position.z + movement.z
        moved = true
    end

    if self.keys.backward then
        -- Move backward opposite to camera direction
        local movement = forward * (-velocity)
        self.camera.position.x = self.camera.position.x + movement.x
        self.camera.position.y = self.camera.position.y + movement.y
        self.camera.position.z = self.camera.position.z + movement.z
        moved = true
    end

    if self.keys.left then
        -- Strafe left using camera's right vector
        local movement = right * (-velocity)
        self.camera.position.x = self.camera.position.x + movement.x
        self.camera.position.y = self.camera.position.y + movement.y
        self.camera.position.z = self.camera.position.z + movement.z
        moved = true
    end

    if self.keys.right then
        -- Strafe right using camera's right vector
        local movement = right * velocity
        self.camera.position.x = self.camera.position.x + movement.x
        self.camera.position.y = self.camera.position.y + movement.y
        self.camera.position.z = self.camera.position.z + movement.z
        moved = true
    end

    if self.keys.up then
        -- Move up along world Y-axis (not camera up vector)
        self.camera.position.y = self.camera.position.y + velocity
        moved = true
    end

    if self.keys.down then
        -- Move down along world Y-axis
        self.camera.position.y = self.camera.position.y - velocity
        moved = true
    end

    -- Debug position changes (disabled for cleaner output)
    -- if moved then
    --     print(string.format("Camera moved to: (%.2f, %.2f, %.2f)",
    --           self.camera.position.x, self.camera.position.y, self.camera.position.z))
    -- end

    -- Update the camera's internal matrices
    self.camera:update(delta_time)
end

function FPSCamera:process_keyboard(key, pressed)
    -- Map keys to movement states
    if key == KEY.W then
        self.keys.forward = pressed
    elseif key == KEY.S then
        self.keys.backward = pressed
    elseif key == KEY.A then
        self.keys.left = pressed
    elseif key == KEY.D then
        self.keys.right = pressed
    elseif key == KEY.SPACE then
        self.keys.up = pressed
    elseif key == KEY.SHIFT then
        self.keys.down = pressed
    elseif key == KEY.ESCAPE and pressed then
        -- Quit application when ESC is pressed
        print("ESC pressed - quitting application")
        POC.quit_application()
    end
end

function FPSCamera:process_mouse_button(button, pressed)
    -- Handle mouse look activation with left mouse button
    if button == MOUSE_BUTTON.LEFT then
        if not self.settings.mouse_look_mode then
            -- Convert C boolean (1/0) to Lua boolean
            local is_pressed = pressed ~= 0
            self.mouse_look_active = is_pressed

            -- Reset first mouse when entering/exiting mouse look
            if is_pressed then
                self.first_mouse = true
                print("Mouse look activated (hold left mouse button)")
            else
                print("Mouse look deactivated")
            end
        end
    end
end

function FPSCamera:process_mouse_movement(mouse_x, mouse_y)
    -- Only process mouse look if enabled
    local should_look = self.settings.mouse_look_mode or self.mouse_look_active

    if not should_look then
        return
    end

    if self.first_mouse then
        self.last_mouse_x = mouse_x
        self.last_mouse_y = mouse_y
        self.first_mouse = false
        return
    end

    local offset_x = mouse_x - self.last_mouse_x
    local offset_y = self.last_mouse_y - mouse_y  -- Reversed since y-coordinates go from bottom to top

    self.last_mouse_x = mouse_x
    self.last_mouse_y = mouse_y

    offset_x = offset_x * self.settings.mouse_sensitivity
    offset_y = offset_y * self.settings.mouse_sensitivity

    self.camera.yaw = self.camera.yaw + offset_x
    self.camera.pitch = self.camera.pitch + offset_y

    -- Constrain pitch to prevent camera flipping
    if self.camera.pitch > self.settings.max_pitch then
        self.camera.pitch = self.settings.max_pitch
    elseif self.camera.pitch < -self.settings.max_pitch then
        self.camera.pitch = -self.settings.max_pitch
    end
end

function FPSCamera:process_mouse_scroll(scroll_y)
    -- Adjust FOV for zoom effect
    self.camera.fov = self.camera.fov - scroll_y * 2.0

    -- Constrain FOV
    if self.camera.fov < 1.0 then
        self.camera.fov = 1.0
    elseif self.camera.fov > 120.0 then
        self.camera.fov = 120.0
    end
end

function FPSCamera:get_position()
    return {
        x = self.camera.position.x,
        y = self.camera.position.y,
        z = self.camera.position.z
    }
end

function FPSCamera:get_rotation()
    return {
        yaw = self.camera.yaw,
        pitch = self.camera.pitch
    }
end

function FPSCamera:set_position(x, y, z)
    self.camera.position.x = x
    self.camera.position.y = y
    self.camera.position.z = z
end

function FPSCamera:set_rotation(yaw, pitch)
    self.camera.yaw = yaw
    self.camera.pitch = pitch

    -- Constrain pitch
    if self.camera.pitch > self.settings.max_pitch then
        self.camera.pitch = self.settings.max_pitch
    elseif self.camera.pitch < -self.settings.max_pitch then
        self.camera.pitch = -self.settings.max_pitch
    end
end

function FPSCamera:toggle_mouse_look()
    self.settings.mouse_look_mode = not self.settings.mouse_look_mode
    if self.settings.mouse_look_mode then
        print("Mouse look mode: Always On")
        self.first_mouse = true  -- Reset mouse tracking
    else
        print("Mouse look mode: Hold Button")
        self.mouse_look_active = false  -- Deactivate hold mode
    end
end

function FPSCamera:set_mouse_look_mode(enabled)
    self.settings.mouse_look_mode = enabled
    if enabled then
        print("Mouse look mode: Always On")
        self.first_mouse = true
    else
        print("Mouse look mode: Hold Button")
        self.mouse_look_active = false
    end
end

-- Create and return a default FPS camera controller
local fps_camera = FPSCamera.new()
fps_camera:bind()

print("=== FPS Camera Controller loaded ===")
print("Controls:")
print("  WASD - Move forward/left/backward/right")
print("  Space - Move up")
print("  Shift - Move down")
if fps_camera.settings.mouse_look_mode then
    print("  Mouse - Look around (always on)")
else
    print("  Hold Left Mouse Button - Look around")
end
print("  Mouse wheel - Zoom in/out")

-- Create global functions that can be called from C
function update(delta_time)
    fps_camera:update(delta_time)
end

function process_keyboard(key, pressed)
    fps_camera:process_keyboard(key, pressed)
end

function process_mouse_movement(mouse_x, mouse_y)
    fps_camera:process_mouse_movement(mouse_x, mouse_y)
end

function process_mouse_button(button, pressed)
    fps_camera:process_mouse_button(button, pressed)
end

function process_mouse_scroll(scroll_y)
    fps_camera:process_mouse_scroll(scroll_y)
end

function get_position()
    return fps_camera:get_position()
end

function get_rotation()
    return fps_camera:get_rotation()
end

-- Also return the table for module-style usage if needed
return {
    camera_controller = fps_camera,
    update = update,
    process_keyboard = process_keyboard,
    process_mouse_movement = process_mouse_movement,
    process_mouse_button = process_mouse_button,
    process_mouse_scroll = process_mouse_scroll,
    get_position = get_position,
    get_rotation = get_rotation
}