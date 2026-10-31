-- Pure policy. C supplies bytes from the original ALU/resource read points.
-- No inventory, debit, flags, RNG, animation or progress is stored in Lua.
return {
  compare = function(a,b) if a==b then return 0 elseif a<b then return 1 else return 2 end end,
  zero = function(value) return value==0 end,
  bit_zero = function(value,mask) return (value & mask)==0 end,
  carry = function(value) return value==1 end,
  reward_id = function(id) return id end,
  parameter_mode = function(mode) return mode end,
}
