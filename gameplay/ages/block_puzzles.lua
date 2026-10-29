-- Pure decisions for INTERAC_PUSHBLOCK_TRIGGER (interactionCode13).
-- C supplies values at the original branch points and applies all effects.
return {
  next_state = function(state, a, b)
    if state == 0 then return 1 end
    if state == 1 then
      -- a = subid/threshold, b = remaining enemy count; original unsigned CP.
      if a >= b then return 2 end
      return 1
    end
    if state == 2 then
      -- a = original tile, b = current tile at the remembered position.
      if a ~= b then return 3 end
      return 2
    end
    -- state 3: a = whether the C counter decrement produced zero.
    if a == 1 then return 4 end -- terminal action: clear count and delete
    return 3
  end,
}
