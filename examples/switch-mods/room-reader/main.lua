local count = 0
oracles.on_frame(function()
  local room = oracles.room()
  local camera = oracles.camera()
  local sprites = oracles.sprites()
  count = count + 1
  -- Read only. Log infrequently to avoid SD/console work each frame.
  if count % 600 == 0 then oracles.log("Reader frames: " .. count) end
end)
