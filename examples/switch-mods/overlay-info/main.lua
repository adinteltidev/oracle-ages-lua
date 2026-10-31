local show = oracles.setting("Show overlay", true)
oracles.on_frame(function()
  if not show() then return end
  local r = oracles.room()
  oracles.draw_rect(2, 18, 130, 12, 0)
  oracles.draw_text(4, 20, r and ("Room " .. r.group .. ":" .. r.room) or "Recomp mods", 32767)
end)
