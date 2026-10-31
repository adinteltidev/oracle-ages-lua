-- Explicit, single-step event transitions. C owns the existing actor state,
-- substate, script PC/return address, timers, flags and all physical effects.
-- A wait keeps that state; a completed decision advances the existing machine.
return {
  compare=function(a,b) if a==b then return 0 elseif a<b then return 1 else return 2 end end,
  zero=function(value) return value==0 end,
  bit_zero=function(value,mask) return (value & mask)==0 end,
  carry=function(value) return value==1 end,
  table_index=function(index) return index end,
  transition=function(current,requested) return requested end,
  next_state=function(current) return (current+1) & 255 end,
  dialogue=function(low,high) return low | (high<<8) end,
  choose_linked=function(linked) return linked~=0 end,
  script_ready=function(active) return active==0 or (active & 128)~=0 end,
  script_ended=function(opcode) return opcode==0 end,
  condition=function(fact) return fact~=0 end,
}
