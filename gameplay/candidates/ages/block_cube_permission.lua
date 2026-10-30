-- Candidate only: not embedded in or called by the distributed game.
-- Preserve the three original read/branch boundaries when integrating later.
local no_cube = function(position) return position == 0 end
local color_active = function(color) return (color & 0x80) ~= 0 end
local tile_matches = function(tile, color)
  -- SUB on the original CPU wraps at eight bits; color excludes its active bit.
  return ((tile - 0x2c) & 0xff) == (color & 0x7f)
end

return {
  no_cube = no_cube,
  color_active = color_active,
  tile_matches = tile_matches,
  -- Complete truth table for tests, using supplied values, never fetching RAM.
  -- Production must not prefetch color/tile to call this before their reads.
  allows = function(position, color, tile)
    return no_cube(position) or (color_active(color) and tile_matches(tile, color))
  end,
}
