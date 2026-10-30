# Fase 1 — inventário implementado e validação local

30/10/2026. Branch `port/lua-gameplay`, base `970dbe0`, alterações locais.

**Implementação experimental do inventário concluída; aceitação da fase aberta.**
O jogador informou que ainda não testou Windows/Switch. Também falta regressão
completa de Seasons com ROM própria. Esses itens impedem declarar a fase validada
ou promover as opções experimentais a padrão. Fases 2–7 não foram iniciadas.

## Regras integradas

`gameplay/ages/block_rules.lua` acrescenta 32 regras ao gatilho de blocos e aos
interruptores existentes. `src/script/block_rules.def/.h` define funções C
concretas, com parâmetros explícitos por regra e resultado validado; não há API
genérica para executar comandos de memória.

| Grupo | Regras Lua | Integração real |
| --- | --- | --- |
| Cubo/permissão/assentamento (5) | `cube_absent`, `cube_active`, `cube_match`, `block_has_replacement`, `block_finishes_puzzle` | `pushblock.c` |
| Sincronizador (3) | `sync_awake`, `sync_eligible`, `sync_next_state` | `pushblockSynchronizer.c`, incluindo helpers e transições 0–2 |
| Água (4) | `water_exists`, `water_initial_side`, `water_direction_match`, `water_advance` | `waterPushblock.c`, inicialização e substates/timers |
| Painéis (5) | `panel_opened`, `panel_trigger`, `panel_right`, `panel_move_tick`, `panel_advance` | `stonePanel.c` |
| Parser de soluções (3) | `tiles_end`, `tiles_new_group`, `tiles_match` | `miscPuzzles_verifyTilesAtPositions`; subids 01/03/0c/1a e demais consumidores |
| Puzzles do cubo (3) | `puzzle_cube_position`, `puzzle_cube_color`, `puzzle_cube_valid_color` | `miscPuzzles.c`, subids 02/14 |
| Painéis/escada (3) | `panels_torch_count`, `panels_all_triggers`, `panels_wait_opened` | subids 0d/15 |
| Estátua (1) | `statue_placed` | subid 18 |
| Reinício de sala (5) | `trap_route_open`, `trap_skip_corner`, `trap_solid_tile`, `trap_all_checked`, `trap_advance` | subid 1f |

C mantém física, movimento, animação, colisão, leitura das propriedades da ROM,
temporizadores e os efeitos mecânicos nos pontos originais. Condições genéricas
de recompensa, flags de itens e serviços compartilhados permanecem nas fases
correspondentes. A chama/paleta do cubo pertence à fase 2.

Leituras, ALU, flags, ciclos e ordem de escrita originais foram preservados. As
decisões Lua comandam os ramos/transições; o parser também devolve a flag Z
correspondente. Falhas param o motor com diagnóstico, inclusive em helpers.

## Runtime e persistência

Três módulos incorporados: `switch_tiles`, `block_puzzles`, `block_rules`.
Ambientes separados, sem bibliotecas padrão, arquivos, rede ou RNG. Limites de
256 KiB de memória e instruções por chamada; booleanos e transições inteiras
validados por tipo, quantidade e faixa. Lua não armazena progresso persistente.
Layout `GB`, saves e RNG não mudaram.

SHA256 das fontes incorporadas:
`ad4a8fa264af3e1781d7a8710b6685ab3717b1c01fa06abf004a0dcb7bc4fe2a`.

Ambas as opções continuam **OFF por padrão**. Builds expandidos usam
`ORACLES_LUA_GAMEPLAY=ON` e `ORACLES_LUA_BLOCK_PUZZLES=ON`, com perfil separado
`oracles-lua-phase1` no desktop e `oracle-ages-lua-phase1` no SD. Seasons ignora
essas regras. O arquivo nativo do controle C compilado com flags OFF não tem
referências à API `gameplay_`.

## Evidências locais

- Suíte final: **11/11 testes aprovados**, incluindo menus, quatro slots e
  quicksave, contratos anteriores e novos, cubo e sincronizador.
- Novo contrato: **398.857 decisões tipadas**, todas as 32 regras com cobertura,
  mais erros de tipo/faixa/quantidade, limites de memória/instruções e isolamento.
- Novo teste com ROM: **7.673 comparações dirigidas**, referência C real versus
  hook Lua real, cobrindo as 32 regras. Inclui 27 cenários de save/load com VM
  recriada, interrupções por falhas e guarda de Seasons.
- Cenas gráficas usam o scheduler real, comparam GB completo a cada frame e hash
  da amostra completa; casos diretos comparam GB e as 16 amostras. O teste restaura
  também o cache externo do renderizador entre C e Lua.
- Sincronizador: **7.200 comparações completas**, mais retomada dos três estados.
  O fixture antigo isolado foi substituído pelo hook de produção real.
- Replay final: **289.518 frames, 4.825 checkpoints em cada modo**, zero pulados;
  modo com recargas realizou **285 reloads**. Os quatro estados do gatilho Lua
  tiveram chamadas positivas. O percurso natural não visita todas as 32 regras;
  a cobertura delas vem dos testes dirigidos, não desse replay.
- Execução portátil fora do checkout, com caminhos contendo espaços e acentos,
  aprovou os 7.673 casos usando `--rom` e `--tas-dir` e preparação automática.
- Builds macOS Intel, Windows x64 e NRO ARM64 aprovados. Auditorias locais
  confirmaram ausência de interpretador de CPU GB e cabeçalho da ROM; Windows
  sem CLR/DLLs privadas. Compilação cruzada não comprova execução no aparelho.
- Lint do jogo: zero problemas. Seis testes das ferramentas CI aprovados;
  sintaxe YAML/shell/Python verificada. CI remoto ainda não executado nesta rodada.

Os testes de erro imprimem paradas intencionais; exit 0 e asserções confirmam
que a falha foi detectada e não continuou produzindo efeitos.

## Reprodução

Configure o build com as duas opções ON e uma ROM própria em `roms/`:

```sh
cmake --build build-macos-lua --target test_lua_block_rules test_lua_block_rules_rom test_lua_sync_candidate_rom
ctest --test-dir build-macos-lua --output-on-failure -R '^lua_'
build-macos-lua/test_native_ages --rom '/caminho/Ages.gbc' --tas-dir tas --frames 289518 --run both
```

O teste dirigido pode preparar sua cena a partir dos fixtures TAS sem estado
privado. `PHASE1_RULES_CASES=water` (ou cube/sync/panels/parser/misc) seleciona
um grupo. `PHASE1_TRACE=1` identifica cenas no diagnóstico. ROM ausente retorna
77; isso é teste não executado, não aprovação.

O modo `PHASE1_RULES_PROFILE=/caminho/resultado.json` mede CPU da thread dentro
de `gb_run_frame`, com comparador/hash/cópias por frame desativados. Executa os
mesmos cenários artificiais C e Lua; `PHASE1_LUA_FIRST=1` inverte a ordem. Mede
1.600 frames por modo em cenas de água/painéis. Não mede SDL/apresentação, uso do
SD ou cadência em Windows/Switch. Logs, XML, perfis e auditorias desta rodada
ficam em `reports/lua-phase1-expanded/`, ignorado pelo Git.

## Distribuição e pendências

[Guia de validação dos pacotes expandidos](LUA-PHASE-1-PLATFORM-TESTS.md).
ZIPs Windows/Switch incluem binários, fontes correspondentes do working tree,
licenças, manifesto e checksums; o DMG macOS também acompanha fontes/licenças.
Nenhum pacote inclui ROM ou saves do jogador. Os pacotes históricos são mantidos.

O workflow experimental mantém os testes de contrato sem ROM nos três sistemas.
Uma execução manual também chama o build multiplataforma com ambas as opções
ON e smoke dos contratos no Windows. O empacotador Switch respeita o perfil Lua.
CI/release padrão continuam com Lua OFF. Nada foi enviado/publicado nesta rodada.

Para encerrar a **aceitação** faltam: executar os pacotes no Windows e no Switch
real (portátil/dock, áudio, suspensão, saves e cenas migradas), registrar seus
logs, regressão completa de Seasons com ROM própria e avaliar o desempenho nas
plataformas reais, incluindo GC/cadência. O teste local de guarda de Seasons não
substitui essa regressão. A implementação optativa está pronta para essa rodada;
a fase ainda não pode ser declarada integralmente validada.

## CPU local da rodada expandida

Três execuções Release/x86_64, 1.600 frames por modo por execução, mesma cena,
ordem C/Lua alternada; comparador/hash/cópias por frame desativados. Tempos da
thread dentro de `gb_run_frame`; verificações fora da região medida.

| Execução | Média C/Lua (ms) | Mediana C/Lua (ms) | P95 C/Lua (ms) | P99 C/Lua (ms) | Acréscimo médio |
| --- | --- | --- | --- | --- | --- |
| 1 | 1.921/1.904 | 1.824/1.817 | 2.426/2.379 | 2.871/2.596 | -0.89% |
| 2 | 1.904/1.915 | 1.808/1.813 | 2.378/2.402 | 2.846/2.889 | +0.54% |
| 3 | 1.882/1.871 | 1.799/1.787 | 2.308/2.321 | 2.493/2.520 | -0.55% |

Heap e pico Lua: **20.005 bytes** nas três execuções. Os tempos medidos ficaram
abaixo do acréscimo inicial de 10% do plano nesses cenários; diferenças pequenas
não demonstram ganho de velocidade. Não são medição de todas as salas, de FPS do
aplicativo, nem de GC/cadência nos alvos Windows/Switch. Esses critérios continuam
pendentes de aceitação nas plataformas. Perfis: `profile-final-{1,2,3}.json`.
