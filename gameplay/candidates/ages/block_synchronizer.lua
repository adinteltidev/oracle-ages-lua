-- Candidate only: test fixtures load this; game hooks never do.
return {
  awake = function(enabled)
    -- Any nonzero enabled byte starts synchronization, not only the value 1.
    return enabled ~= 0
  end,
  eligible_tile = function(tile)
    -- Somaria blocks are excluded, but the scan must still continue past them.
    return tile ~= 0xda
  end,
}
