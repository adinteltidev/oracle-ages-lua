-- Phase 1 policies. C supplies values at the original branch points and applies
-- all effects, including CPU arithmetic, flags, timings, writes and service calls.
return {
  cube_absent = function(position) return position == 0 end,
  cube_active = function(color) return (color & 0x80) ~= 0 end,
  cube_match = function(block_index, cube_index) return block_index == cube_index end,
  block_has_replacement = function(tile) return tile ~= 0 end,
  block_finishes_puzzle = function(rotated_properties) return (rotated_properties & 1) ~= 0 end,
  sync_awake = function(enabled) return enabled ~= 0 end,
  sync_eligible = function(tile) return tile ~= 0xda end,
  sync_next_state = function(current)
    if current == 1 then return 2 end
    return 1
  end,
  water_exists = function(subid, flooding_flag)
    if subid == 0 then return flooding_flag == 0 end
    return flooding_flag ~= 0
  end,
  water_initial_side = function(subid) return subid == 0 end,
  water_direction_match = function(direction, required) return direction == required end,
  water_advance = function(elapsed) return elapsed == 1 end,
  panel_opened = function(room_flag_40) return room_flag_40 ~= 0 end,
  panel_trigger = function(triggers) return (triggers & 0x80) ~= 0 end,
  panel_right = function(subid) return subid ~= 0 end,
  panel_move_tick = function(rotated_frame) return (rotated_frame & 0x80) ~= 0 end,
  panel_advance = function(elapsed) return elapsed == 1 end,
  tiles_end = function(token) return token == 0 end,
  tiles_new_group = function(position) return position == 0xff end,
  tiles_match = function(actual, expected) return actual == expected end,
  puzzle_cube_position = function(actual, expected) return actual == expected end,
  puzzle_cube_color = function(masked, expected) return masked == expected end,
  puzzle_cube_valid_color = function(color) return color < 3 end,
  panels_torch_count = function(count) return count == 1 end,
  panels_all_triggers = function(triggers) return triggers == 7 end,
  panels_wait_opened = function(triggers) return (triggers & 0x80) == 0 end,
  statue_placed = function(tile) return tile == 0x2a end,
  trap_route_open = function(collision) return collision == 0 end,
  trap_skip_corner = function(remaining) return (remaining & 1) ~= 0 end,
  trap_solid_tile = function(tile) return tile ~= 0 end,
  trap_all_checked = function(remaining) return remaining == 0 end,
  trap_advance = function(elapsed) return elapsed == 1 end,
}
