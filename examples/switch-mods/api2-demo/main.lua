-- Install explicitly in a disposable save profile.
-- Demonstrates a valid reward mapping and the guarded write queue.
oracles.swap_treasure(0, 1, 0x28, 0, 0x05, 0)
local demo = oracles.setting("Queue identity write", false)
oracles.on_frame(function()
  if demo() then oracles.wr(0xc6a0, oracles.rd(0xc6a0)) end
end)
