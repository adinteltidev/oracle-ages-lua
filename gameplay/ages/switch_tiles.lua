-- Pure gameplay decisions. C preserves memory writes, CPU flags and cycle order.
return {
  changed = function(previous, current) return previous ~= current end,
  alternate = function(mask, switches) return (mask & switches) ~= 0 end,
}
