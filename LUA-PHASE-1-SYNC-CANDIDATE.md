# Sincronizador de blocos — integração experimental

Atualização: 30/10/2026. Branch `port/lua-gameplay`, base `970dbe0`.

O candidato `gameplay/candidates/ages/block_synchronizer.lua` permanece como
referência de testes. O jogo usa `gameplay/ages/block_rules.lua`: `sync_awake`,
`sync_eligible` e `sync_next_state`. O contrato de transição aceita estados 0–2,
produz 1 para 0/2 e 2 para 1 e rejeita qualquer outro resultado antes da escrita.
C conserva movimento, colisão, criação de objetos, ciclos, registradores e flags.

`test_lua_sync_candidate_rom` agora compara o **hook de produção real** com Lua
ativado/desativado. Não usa cópia isolada do hook nem checker de fixture.
São 7.200 comparações completas GB/amostras, mais save/load dos três estados com
VM recriada. `test_lua_block_rules_rom` acrescenta cobertura positiva das três
regras e verifica interrupção ao retornar valores inválidos, inclusive dentro
dos helpers que criam blocos.

```sh
cmake --build build-macos-lua --target test_lua_sync_candidate test_lua_sync_candidate_rom
ctest --test-dir build-macos-lua --output-on-failure -R '^lua_sync_candidate(_rom)?$'
```

Testes locais aprovados. Windows/Switch foram compilados e auditados, mas não
executados em hardware. Ver [relatório atual](LUA-PHASE-1-IMPLEMENTATION.md).
A fase 1 continua com aceitação aberta; fases 2–7 não começaram.
