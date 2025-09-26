--[[
  Object Picker Example

  This script demonstrates how to use the POC Engine scene system with object picking.
  It creates a scene with multiple objects and allows clicking on them to identify them.
]]

local ObjectPicker = {}
ObjectPicker.__index = ObjectPicker

function ObjectPicker.new()
    local self = setmetatable({}, ObjectPicker)

    -- Key state tracking for continuous movement
    self.keys_pressed = {}

    -- Mouse tracking for camera look
    self.mouse_sensitivity = 0.1
    self.last_mouse_x = 0
    self.last_mouse_y = 0
    self.first_mouse = true

    -- Create scene
    self.scene = POC.create_scene()
    if not self.scene then
        error("Failed to create scene")
    end

    -- Create and setup camera
    local aspect_ratio = 16/9
    self.camera = POC.create_camera("fps", aspect_ratio)
    if not self.camera then
        error("Failed to create camera")
    end

    -- Position camera to see objects
    -- Camera at (5, 0, 0) looking down negative X toward objects along X axis
    -- Objects are at X=±2.5, Y=0, Z=0, so camera should look down -X axis from positive X
    self.camera.position.x = 5
    self.camera.position.y = 0
    self.camera.position.z = 0
    self.camera.yaw = 180  -- Look down negative X axis
    self.camera.pitch = 0

    -- Create scene objects
    self.objects = {}
    self:create_objects()

    -- Bind scene and camera
    POC.bind_scene(self.scene)
    POC.bind_camera(self.camera)

    self.fps_mode_enabled = false
    self:set_fps_mode(false, true)
    print("✓ Object Picker initialized")
    print("Edit mode active: click objects to select (unlit shading)")
    print("Press F to enter play mode for FPS controls and lighting")

    return self
end

function ObjectPicker:create_objects()
    -- Load different meshes for different objects
    local golden_mesh = POC.load_mesh("models/cube.obj")
    local red_mesh = POC.load_mesh("models/cube_red.obj")

    if not golden_mesh then
        print("Warning: Could not load cube.obj mesh")
        return
    end

    if not red_mesh then
        print("Warning: Could not load cube_red.obj mesh, using golden cube for both")
        red_mesh = golden_mesh
    end

    -- Create two cubes at different positions with different meshes
    local cube_data = {
        {id = 1, name = "GoldenCube", position = {-2.5, 0, 0}, color = "golden", mesh = golden_mesh},
        {id = 2, name = "RedCube", position = {2.5, 0, 0}, color = "red", mesh = red_mesh}
    }

    for _, data in ipairs(cube_data) do
        -- Create scene object
        local obj = POC.create_scene_object(data.name, data.id)
        if obj then
            -- Set mesh (using POC API functions)
            POC.scene_object_set_mesh(obj, data.mesh)

            -- Set position
            POC.scene_object_set_position(obj, data.position[1], data.position[2], data.position[3])

            -- Add to scene
            if POC.scene_add_object(self.scene, obj) then
                table.insert(self.objects, {
                    object = obj,
                    id = data.id,
                    name = data.name,
                    color = data.color
                })
                print(string.format("✓ Created %s (%s) at (%.1f, %.1f, %.1f)",
                      data.name, data.color, data.position[1], data.position[2], data.position[3]))
            else
                print(string.format("✗ Failed to add %s to scene", data.name))
            end
        else
            print(string.format("✗ Failed to create scene object %s", data.name))
        end
    end

    print(string.format("Scene created with %d objects", #self.objects))
end

function ObjectPicker:process_mouse_button(button, pressed, x, y, width, height)
    -- Handle left mouse button clicks for picking (only when FPS mode not active)
    if self.fps_mode_enabled then
        return
    end

    if button == MOUSE_BUTTON.LEFT and pressed == 1 then
        print(string.format("Picking at screen coordinates: (%.1f, %.1f) in %dx%d window", x, y, width, height))

        -- Perform object picking
        local hit = POC.pick_object(x, y, width, height)

        if hit and hit.hit then
            -- Find the object data for additional info
            local object_data = nil
            for _, obj_data in ipairs(self.objects) do
                if obj_data.id == hit.object_id then
                    object_data = obj_data
                    break
                end
            end

            if object_data then
                print(string.format("🎯 HIT: %s (%s, ID=%d) at distance %.2f",
                      hit.object_name, object_data.color, hit.object_id, hit.distance))
                print(string.format("   Hit point: (%.2f, %.2f, %.2f)",
                      hit.point.x, hit.point.y, hit.point.z))
            else
                print(string.format("🎯 HIT: %s (ID=%d) at distance %.2f",
                      hit.object_name, hit.object_id, hit.distance))
            end
        else
            print("❌ No object hit")
        end
    end
end

function ObjectPicker:process_keyboard(key, pressed)
    local was_pressed = self.keys_pressed[key]

    -- Handle ESC key to quit
    if key == KEY.ESCAPE and pressed then
        self:set_fps_mode(false, true)
        POC.quit_application()
        return
    end

    if key == KEY.F and pressed and not was_pressed then
        self:set_fps_mode(not self.fps_mode_enabled, false)
    end

    -- Track key states for continuous movement
    self.keys_pressed[key] = pressed
end

function ObjectPicker:process_mouse_movement(x, y, delta_x, delta_y)
    if not self.fps_mode_enabled then
        self.last_mouse_x = x
        self.last_mouse_y = y
        self.first_mouse = true
        return
    end

    -- Handle first mouse movement
    if self.first_mouse then
        self.last_mouse_x = x
        self.last_mouse_y = y
        self.first_mouse = false
        return
    end

    -- Use provided deltas if available (when cursor is locked), otherwise calculate from absolute position
    local xoffset, yoffset
    if delta_x and delta_y then
        -- Use the delta coordinates provided by the platform layer
        xoffset = delta_x
        yoffset = -delta_y  -- Reversed since y-coordinates go from bottom to top
    else
        -- Calculate mouse offset from absolute position (fallback for unlocked cursor)
        xoffset = x - self.last_mouse_x
        yoffset = self.last_mouse_y - y  -- Reversed since y-coordinates go from bottom to top
    end

    self.last_mouse_x = x
    self.last_mouse_y = y

    -- Apply mouse sensitivity
    xoffset = xoffset * self.mouse_sensitivity
    yoffset = yoffset * self.mouse_sensitivity

    -- Update camera orientation
    self.camera.yaw = self.camera.yaw + xoffset
    self.camera.pitch = self.camera.pitch + yoffset

    -- Constrain pitch to avoid flipping
    if self.camera.pitch > 89.0 then
        self.camera.pitch = 89.0
    elseif self.camera.pitch < -89.0 then
        self.camera.pitch = -89.0
    end
end

function ObjectPicker:process_mouse_scroll(scroll_y)
    -- Adjust FOV for zoom
    self.camera.fov = self.camera.fov - scroll_y * 2.0
    if self.camera.fov < 10.0 then
        self.camera.fov = 10.0
    elseif self.camera.fov > 90.0 then
        self.camera.fov = 90.0
    end
end

function ObjectPicker:update(delta_time)
    if not self.fps_mode_enabled then
        return
    end

    local move_speed = 2.5 -- units per second
    local movement_delta = move_speed * delta_time

    local move_x, move_y, move_z = 0.0, 0.0, 0.0

    local function add_horizontal(vec, sign)
        local vx = vec.x
        local vz = vec.z
        local len = math.sqrt(vx * vx + vz * vz)
        if len > 0.0001 then
            vx = vx / len
            vz = vz / len
            move_x = move_x + vx * sign
            move_z = move_z + vz * sign
        end
    end

    if self.keys_pressed[KEY.W] then
        add_horizontal(self.camera.front, 1.0)
    end
    if self.keys_pressed[KEY.S] then
        add_horizontal(self.camera.front, -1.0)
    end
    if self.keys_pressed[KEY.A] then
        add_horizontal(self.camera.right, -1.0)
    end
    if self.keys_pressed[KEY.D] then
        add_horizontal(self.camera.right, 1.0)
    end

    if self.keys_pressed[KEY.Q] then
        move_y = move_y - 1.0
    end
    if self.keys_pressed[KEY.E] then
        move_y = move_y + 1.0
    end

    local length_sq = move_x * move_x + move_y * move_y + move_z * move_z
    if length_sq > 0.0001 then
        local length = math.sqrt(length_sq)
        move_x = move_x / length
        move_y = move_y / length
        move_z = move_z / length

        self.camera.position.x = self.camera.position.x + move_x * movement_delta
        self.camera.position.y = self.camera.position.y + move_y * movement_delta
        self.camera.position.z = self.camera.position.z + move_z * movement_delta
    end

    -- Update camera
    self.camera:update(delta_time)
end

function ObjectPicker:set_fps_mode(enabled, force)
    local changed = self.fps_mode_enabled ~= enabled

    if not changed and not force then
        return
    end

    self.fps_mode_enabled = enabled
    POC.set_cursor_mode(enabled, not enabled)
    self.first_mouse = true

    POC.set_play_mode(enabled)

    if enabled then
        print("Play mode enabled: lighting on, cursor locked")
    else
        print("Edit mode enabled: unlit shading, cursor visible")
    end
end

function ObjectPicker:on_window_focus(focused)
    if not focused then
        if self.fps_mode_enabled then
            self:set_fps_mode(false, true)
        end
        self.keys_pressed = {}
    else
        self.first_mouse = true
    end
end

function ObjectPicker:on_mouse_enter(x, y)
    self.last_mouse_x = x
    self.last_mouse_y = y
    self.first_mouse = true
end

-- Create global instance
local picker = ObjectPicker.new()

-- Export functions for the engine to call
function process_mouse_button(button, pressed, x, y, width, height)
    picker:process_mouse_button(button, pressed, x, y, width, height)
end

function process_keyboard(key, pressed)
    picker:process_keyboard(key, pressed)
end

function process_mouse_movement(x, y, delta_x, delta_y)
    picker:process_mouse_movement(x, y, delta_x, delta_y)
end

function process_mouse_scroll(scroll_y)
    picker:process_mouse_scroll(scroll_y)
end

function update(delta_time)
    picker:update(delta_time)
end

function set_fps_mode(enabled, force)
    picker:set_fps_mode(enabled, force)
end

function on_window_focus(focused)
    picker:on_window_focus(focused)
end

function on_mouse_enter(x, y)
    picker:on_mouse_enter(x, y)
end
