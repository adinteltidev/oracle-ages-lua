-- C supplies physical facts, target geometry and the original RNG result.
-- This module owns branch decisions and movement/state intentions, without IO.
return {
  compare = function(a,b) if a==b then return 0 elseif a<b then return 1 else return 2 end end,
  zero = function(value) return value==0 end,
  bit_zero = function(value,mask) return (value & mask)==0 end,
  condition = function(flags, mask) return (flags & mask) ~= 0 end,
  transition = function(current, requested) return requested end,
  angle = function(current, requested) return requested end,
  direction = function(current, requested) return requested end,
  speed = function(current, requested) return requested end,
  octorok_attack = function(random, mask) return (random & mask) == 0 end,
  octorok_aim = function(subid) return subid >= 2 end,
}
