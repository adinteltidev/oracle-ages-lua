-- The upper bit controls the flame. Lower bits select the ROM palette entry.
-- Preserve the original mask for every byte, including unexpected colors.
return {
  present = function(position) return position ~= 0 end,
  visible = function(color) return (color & 0x80) ~= 0 end,
  palette_offset = function(color) return color & 0x7f end,
}
