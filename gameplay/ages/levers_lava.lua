-- Controller policy only. C owns counters, minisscript reads, RNG and effects.
return {
  fully_pulled = function(distance) return (distance & 0x80) ~= 0 end,
  released = function(distance) return distance == 0 end,
  timer_elapsed = function(zero) return zero == 1 end,
  has_tile = function(token) return token ~= 0 end,
  source_found = function(zero) return zero == 1 end,
  source_segment = function(relative_tile) return relative_tile < 12 end,
  source_delta = function(empty) if empty == 1 then return 0xfa end return 6 end,
  next_state = function(state) if state == 4 then return 1 end return state + 1 end,
  floor_tile = function(state, random_byte)
    if state == 2 then return 1 end
    return 0x61 + (random_byte & 3)
  end,
}
