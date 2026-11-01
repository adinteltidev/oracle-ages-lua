-- Requires a build with the on_input/run extension (November 2026 or newer).
assert(oracles.on_input and oracles.run, "Atualize o aplicativo para usar o mod Corrida")
oracles.on_input(function(input)
  oracles.run(input.run)
end)
