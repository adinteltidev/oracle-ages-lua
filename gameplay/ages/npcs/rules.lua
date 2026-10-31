-- NPC policy receives ROM/progress/physical facts; no globals or persistent Lua state.
return {
  compare=function(a,b) if a==b then return 0 elseif a<b then return 1 else return 2 end end,
  zero=function(value) return value==0 end,
  bit_zero=function(value,mask) return (value & mask)==0 end,
  carry=function(value) return value==1 end,
  old_lady_available=function(subid,saved,progress)
    if subid==0 then return saved==0 end
    return progress<4
  end,
  old_lady_script=function(subid,progress) if subid==0 then return 0 else return progress end end,
  symmetry_available=function(subid,nut) return subid~=12 or nut~=0 end,
  symmetry_restart=function(state,flags) return state==2 and (flags & 1)~=0 end,
}
