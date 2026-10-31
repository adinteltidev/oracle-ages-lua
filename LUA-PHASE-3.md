# Fase 3 — alavancas e lava

30/10/2026. Branch `port/lua-gameplay`, base `e94017b`, alterações locais.

**Implementação experimental do inventário concluída; aceitação física aberta.**
O avanço foi solicitado pelo jogador. As pendências físicas das fases anteriores
continuam registradas. A integração nova é optativa, OFF por padrão, com perfil
separado. Fases 4–7 não foram iniciadas. Não houve commit/push nesta rodada.

## Inventário e limites

O controlador é `interactionCoded8`, seus helpers `toggleLavaSource`/`updateTile`
e a seleção de minisscript em `src/game/ages/interactions/leverLavaFiller.c`.
A desmontagem local registra seis mecanismos, todos tratados nesta fase:

| Subid | Sala | Intervalo entre grupos |
| --- | --- | --- |
| 0 | D4, primeira sala de lava | 4 frames |
| 1 | D4, duas salas antes da chave do chefe | 6 frames |
| 2 | D4, uma sala antes da chave do chefe | 6 frames |
| 3 | D8, sala de lava com bloco de chave | 6 frames |
| 4 | D8, outra sala de lava | 6 frames |
| 5 | Hero's Cave | 6 frames |

`lever.c` produz a distância e o bit de extensão completa: física de agarrar,
velocidade, limites de movimento, Link e animação permanecem em C. Esse arquivo
não é outro controlador de lava. Os outros consumidores de distância localizados
foram o puzzle de chave de D6 (`miscPuzzles`, subid 00) e o obstáculo móvel em
`miscellaneous1.c`: recompensas/portas e colisão/esmagamento pertencem às fases
5/4 e 7, respectivamente. Não foram contabilizados como regras migradas aqui.
Os mecanismos de lava de Seasons conservam o caminho C.

Os dados continuam na ROM do jogador. A tabela de velocidades tem oito entradas,
mas há seis scripts de sala; os testes cobrem a inicialização das oito entradas
e os seis scripts completos. Não foi criado um clamp de subid nem outra seleção
para substituir os acessos originais fora desse domínio.

## Regras migradas

`gameplay/ages/levers_lava.lua` fornece nove regras tipadas pelo host em
`src/script/levers_lava.h`:

| Regra | Entrada | Decisão |
| --- | --- | --- |
| `fully_pulled` | byte da distância | bit 7 ligado |
| `released` | byte da distância | distância zero |
| `timer_elapsed` | resultado zero do decremento original | atualizar grupo neste passo |
| `has_tile` | token original do script | continuar grupo / reconhecer separador ou fim |
| `source_found` | resultado da busca original em C | escolher o primeiro tipo de fonte encontrado |
| `source_segment` | índice relativo após SUB de 8 bits | continuar no intervalo de 12 tiles de fonte |
| `source_delta` | fonte visual vazia ou cheia | somar 250 (-6) ou 6 |
| `next_state` | estado 0–4 | sucessor válido: 1, 2, 3, 4, 1 |
| `floor_tile` | estado 2/4 e byte do RNG original | tile seco 01 ou lava 61–64 |

C mantém leituras, ALU, flags, contadores, busca de tiles, escrita dos buffers,
som e interpretação dos dados. A fonte é procurada na ordem original c3, c6,
c9, cc. O sinal de busca é passado a Lua após cada chamada; leituras não são
antecipadas. A fonte visual é alterada antes dos grupos de chão como no C.

O estado 1 aguarda extensão completa e inicia o atraso de 30 frames; o estado 2
troca os grupos por chão seco; o estado 3 aguarda distância exatamente zero;
o estado 4 repõe lava por grupos e retorna ao estado 1. C calcula o RNG uma vez
por tile no ponto original; Lua recebe seu byte, sem gerar aleatoriedade própria.
O contador zero conserva o wrap original para 255.

Na troca numérica de estado, o wrapper reproduz os efeitos da rotina C de
incremento, valida a resposta Lua e só então grava o estado. Retornos inválidos
param o motor com regra/diagnóstico, sem repetir silenciosamente a operação em C.
Efeitos já aplicados antes da falha não são desfeitos. Nenhum novo campo foi
introduzido no GB, nos objetos ou nos saves; Lua permanece sem progresso durável.

## Ativação e identidade

```sh
cmake -S . -B build-lua-phase3 -DORACLES_SDL=OFF \
  -DORACLES_LUA_GAMEPLAY=ON -DORACLES_LUA_BLOCK_PUZZLES=ON \
  -DORACLES_LUA_COLORED_CUBE=ON -DORACLES_LUA_LEVERS_LAVA=ON
cmake --build build-lua-phase3 --target test_lua_levers_lava test_lua_levers_lava_rom
ctest --test-dir build-lua-phase3 --output-on-failure -R '^lua_levers_lava'
```

A opção nova exige Lua ON e pode ser ligada isoladamente das integrações
anteriores. Os pacotes desta rodada habilitam as quatro opções. O Switch recebe
explicitamente a definição Lua no arquivo do controlador; todos os scripts de
build aceitam `ORACLES_LUA_LEVERS_LAVA`.

Cinco módulos incorporados: `switch_tiles`, `block_puzzles`, `block_rules`,
`colored_cube`, `levers_lava`. SHA256 conjunto:
`58bb6adb1f8b94893ca135e8c4c89723a2934da6070beebe31224008fc431789`.
O sandbox conserva o limite de 256 KiB, o limite de instruções e ambientes
separados sem bibliotecas padrão/arquivos/rede. Logs e replays expõem os nove
contadores novos. Os perfis são `oracles-lua-phase3` no desktop e
`sdmc:/switch/oracle-ages-lua-phase3/` no Switch.

## Validação local

- **1.547 decisões de contrato:** todos os bytes de distância, token, índice
  relativo e RNG, ambas as flags de timer/busca, os cinco sucessores e dois deltas.
- Retornos inválidos por regra, tipo/quantidade/faixa, floats, nil, limite de
  instruções/memória, ponteiro de saída nulo e isolamento: testados.
- Referência C real versus hook Lua real, por chamada direta e dispatcher:
  inicialização, distâncias parciais/completas, 0–255 nos contadores, todos os
  grupos/separadores/fins dos seis scripts e quatro orientações de fonte.
- Todos os bits/ciclos/memória e as 16 amostras são comparados nos casos diretos.
  O teste também verifica as posições de cada grupo e os valores de chão escritos.
- Cada script executa dois ciclos completos no scheduler, com comparação do GB
  e da amostra completa em cada frame, tanto sem recarga como com VM recriada
  durante drenagem e reposição. Os pares coincidiram em 747/743/671/647/695/719
  frames por subid (dois ciclos por cena), totalizando **8.444 frames comparados**.
- Abandono da atualização e recriação dos objetos de sala na mesma VM: seis casos
  verificam que um novo acionamento começa pelo script/atraso inicial.
- Erros em helpers e nas nove regras: interrupção verificada. Os testes de
  autoridade alteram respostas válidas e confirmam que o C segue Lua, inclusive
  seleção de tile/delta e parada após o primeiro segmento da fonte.
- Com Lua OFF, o código de máquina otimizado do controlador é **idêntico** ao
  commit-base `e94017b`. O caminho C de Seasons não recebe consultas novas.

As cenas usam scripts reais da ROM sobre uma cena preparada pelos fixtures TAS;
não são prova de apresentação/áudio subjetivo nas seis salas reais de dungeon.
As paradas de falha impressas nos logs são intencionais e verificadas por asserções.

O teste aceita `--rom ARQUIVO --tas-dir PASTA`, sem estado privado pré-existente.
ROM ausente retorna 77 e significa teste não executado. Os resultados finais,
contagens, traces, auditorias, replays e medições ficam em `reports/lua-phase3/`
e em `oracle-lua-phase3-evidence.json` ao lado dos pacotes.

## Desempenho e distribuição

O benchmark recebeu `--scenario levers_lava`, com um controlador, dados originais,
fonte visual e ciclos completos. Traces e tempo são modos distintos, sem hashes
nem comparador no trecho cronometrado. `--lua off` fornece o caminho C do mesmo
binário; o build C independente também pode executar a cena.

```sh
build-macos-lua/benchmark-gameplay --prepare /caminho/privado/scene.state
build-macos-lua/benchmark-gameplay --state /caminho/privado/scene.state \
  --scenario levers_lava --blocks 1 --frames 1600 --warmup 480 \
  --lua on --output /caminho/privado/lua.json
```

O workflow experimental testa o contrato sem ROM em Linux/macOS/Windows e ativa
a fase 3 nos builds manuais. O smoke do Windows inclui seu contrato. A CI/release
padrão permanece C; não foi executada CI remota nesta rodada. Os empacotadores
das fases anteriores recusam binários da fase 3.

O [guia de pacotes](LUA-PHASE-3-PLATFORM-TESTS.md) cobre os três sistemas. Os
artefatos incluem fontes correspondentes, licenças e identidade/checksums, sem
ROM nem saves pessoais. O ícone **recomp** do Switch foi preservado.

## Aceitação pendente

Ainda faltam execução real no Windows/Switch (portátil/dock, áudio, suspensão,
retomada, saves e salas de lava), regressão completa de Seasons com ROM própria
e confirmação de GC/cadência nas plataformas físicas. Builds cruzados e os testes
locais não substituem essa aceitação. A fase continua optativa até registrar
esses resultados; pendências anteriores não foram consideradas resolvidas.

## Resultados adicionais da rodada

As duas suítes novas e as 14 regressões anteriores passaram: **16 testes**.
O teste dirigido completou **6.586 casos**, mais as comparações de frames dos
ciclos completos. Seis testes das ferramentas CI passaram; lint sem problemas.

Traces de **960 frames** coincidiram entre C independente, caminho C com runtime
desativado e Lua. A cena exercitou as nove regras; a cobertura específica vem
dessas cenas, sem atribuir cobertura a um replay que não visite salas de lava.

CPU local Release/x86_64, três pares C/Lua, um controlador, 1.600 frames medidos
e 480 de aquecimento. Ordem alternada; ambos os replays suspensos durante a
medição, sem compilação, comparador ou hash no trecho cronometrado.

| Par | Média C/Lua (ms) | Mediana C/Lua (ms) | P95 C/Lua (ms) | P99 C/Lua (ms) | Acréscimo médio |
| --- | --- | --- | --- | --- | --- |
| 1 | 1.855/1.839 | 1.766/1.755 | 2.341/2.311 | 2.840/2.576 | -0.90% |
| 2 | 1.872/1.858 | 1.777/1.769 | 2.377/2.317 | 3.007/2.698 | -0.73% |
| 3 | 1.848/1.848 | 1.770/1.776 | 2.297/2.291 | 2.641/2.561 | -0.03% |

Heap/pico Lua: **27.184 bytes**, com **zero alocações no trecho medido**.
Os resultados estão dentro do orçamento inicial de CPU de 10% nestas condições.
São medições locais headless: não representam todas as salas, GC ou cadência do
aplicativo completo em Windows/Switch, nem demonstram ganho quando Lua fica
abaixo de C em uma execução. Perfis e cobertura detalhada estão nas evidências.
