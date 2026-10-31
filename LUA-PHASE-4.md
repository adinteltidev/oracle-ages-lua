# Fase 4 — portas

30/10/2026. Branch `port/lua-gameplay`, base `b4ee740`, alterações locais.

A implementação experimental do inventário está concluída. Em 30/10/2026, o
jogador confirmou que as versões Windows e Switch estão funcionando e estáveis;
a pendência de validação manual nessas plataformas está atendida, conforme o
[registro do jogador](LUA-PHASE-4-PLAYER-VALIDATION.md). Continua pendente a
regressão completa de Seasons com uma ROM própria. A opção nova é OFF por padrão;
as fases 5–7 continuam sem implementação. Não houve commit ou push nesta rodada.

## Inventário resolvido

| Mecanismo | Implementação/variante | Decisões em Lua |
| --- | --- | --- |
| Controlador comum `1e` | subid 00, abertura simples; 01–03, scripts vazios originais | inicialização/retorno de estado, pausas e abertura |
| Interruptores | subids 04–07, quatro orientações | gatilho mascarado, tile de porta e colisão da passagem |
| Inimigos | subids 08–0b, quatro orientações | saltar quando não há inimigos e aguardar a limpeza |
| Porta de carrinho | subids 0c–0f, quatro orientações | reconhecer trilhos 5d/5e, montaria, contato e passagem livre |
| Fecha depois de entrar | subids 10–13, quatro orientações | esperar Link sair, fechar e preservar XOR de CFC0 em C |
| Tochas | 14: cima/2; 15: esquerda/2; 16: baixo/1; 17: esquerda/1 | igualdade exata da quantidade acesa, sem transformar em `>=` |
| Portão de carrinho `1b` | direções 0 e 2; oito bits de interruptor | abrir/fechar, mudança de interruptor, término da animação e estado seguinte |
| Chave pequena/chefe | `checkAndDecKeyCount`, `nextToKeyDoor`; quatro orientações/tipos da tabela original | dungeon válido, tipo de chave, disponibilidade, reserva da interação |
| Fechaduras do overworld | `nextToOverworldKeyhole`, tabela original de salas/chaves | já aberta e posse da chave de entrada |
| Segurança da passagem | helpers do controlador e banco 15 | bloco de Somaria, colisão, temporizador e coincidência do tile de respawn |

Os 24 subids do controlador são lidos da tabela original, incluindo os três
scripts vazios. Os dados de animação, direções, scripts, máscaras e tiles continuam
na ROM do jogador; não foi criado um clamp que troque acessos fora do domínio
original por outro recurso.

`miscPuzzles` 00 é o puzzle/recompensa da chave de chefe de D6: ele produz um
gatilho, consumido pelas portas já migradas; sua escolha aleatória/recompensa é
fase 5. O subid 07 é a sequência de tochas/cutscene de retração de parede, fase 6.
Os subids 08/09 espelham bits de abertura em salas adjacentes: a propagação de
persistência permanece no serviço C, como `setRoomFlagsForUnlockedKeyDoor`.
Os subids 11–13 conduzem cenas de abertura de entradas. `blackTowerDoorHandler`
e `makuGateOpening` também conduzem eventos de história; essas sequências ficam
para a fase 6. Não foram contabilizadas como portas comuns portadas. Sprites de
chave, geometria de túneis e paredes quebráveis continuam nos serviços C.

## Fronteira C/Lua

`gameplay/ages/doors.lua` define 28 regras; `src/script/doors.def` e `doors.h`
fornecem chamadas tipadas, nomes e contadores. Booleanos exigem booleano Lua.
`gate_state` retorna inteiro 1–2; `script_state`, inteiro 0–3. Floats, nil,
tabelas, múltiplos retornos e estados fora da faixa são rejeitados. Bytecode de
porta danificado que solicita outro estado conserva o caminho original C.

As condições de `jumpIfNoEnemies`, `checkNoEnemies` e
`checkNotCollidedWithLink_ignoreZ`, e a gravação de estado, só usam Lua quando o
proprietário atual do script é uma interação `1e` no banco de objetos. Outros
atores e comandos de subestado mantêm o interpretador de scripts C. A seleção
usa a identidade do ator no ponto da decisão, sem variável global de contexto
que possa vazar entre fibers. O banco 0 propaga falhas antes de executar o próximo
comando ou salvar o endereço do script.

Leituras, ALU, flags, ciclos, decrementar temporizadores, animação, colisão,
contagem de portas, efeitos sonoros, consumo de chave e respawn permanecem em C,
na ordem original. As respostas são consultadas no ponto da decisão; os flags
retornados que representam a decisão também refletem a resposta Lua. O teste de
slot reservado ocupado preserva a ordem original: a verificação/débito da chave
ocorre antes da reserva da interação. Não foi redesenhada essa regra nesta fase.

Falhas Lua interrompem o motor, com regra e motivo, antes de prosseguir pelo
respectivo ponto de decisão. Não há repetição silenciosa pelo caminho C nem
rollback das operações C legítimas que precedem esse ponto. Nenhum estado Lua
persistente ou campo novo foi adicionado a GB, SRAM ou formato de save.

## Verificação

- Contrato: 200.470 decisões canônicas, todos os pares de bytes das três regras
  binárias, todas as entradas de byte, erros de tipo/faixa/contagem, limites de
  execução/memória, isolamento de módulos e ausência de alocações no caminho quente.
- ROM própria USA/Australia: 5.921 casos dirigidos com comparação do GB completo
  e buffers de amostras entre C desabilitando Lua e os mesmos hooks com Lua ativa.
  Todas as 28 regras têm cobertura positiva nos mecanismos reais.
- 22 famílias × 120 frames comparados por rodada, com pixels/áudio e estado;
  rodada normal e outra com 56 operações save/load e recriação da VM durante
  abertura/fechamento. Reentrada na sala, Link no tile ao fechar, Somaria,
  temporizador zero/255, requisitos ausentes/presentes, débito de chave e posse
  de chave de chefe também foram exercitados.
- Respostas inválidas de cada regra são injetadas nos hooks reais. Respostas
  válidas alternativas demonstram autoridade Lua sobre espera por inimigos,
  passagem livre, consumo de chave e estados de portas/portões.

- Os 16 testes `lua_*` passaram, incluindo nove testes com ROM. Os dois replays
  completos verificaram 4.825 checkpoints em 289.518 frames cada, nenhum pulado;
  o segundo realizou 285 recargas. A cobertura natural complementa os fixtures,
  sem substituir as famílias que o trajeto não visita.
- Com Lua OFF no build, os seis arquivos nativos alterados geram o mesmo código
  de máquina de `HEAD` (`b4ee740`), usando as flags do controle C. Traces de 960
  frames também coincidem entre build C, build Lua com integração desativada e Lua
  ativa, normalizando apenas ponteiros do processo.
- Desempenho local, cena privada com cinco mecanismos, três rodadas por variante
  em ordem alternada, 480 frames de aquecimento e 1.600 medidos: mediana das médias
  de CPU de 1,954 ms no build C e 1,967 ms com Lua, diferença de 0,68% ante orçamento
  de 10%. Sem hashing no intervalo cronometrado e zero alocações Lua nos frames
  medidos. É uma medição local do executável Intel, não desempenho de Windows/Switch nem FPS.
- Compilação cruzada Windows x64 e Switch concluída. Windows possui manifesto
  UTF-8, sem CLR nem DLL privada; Switch não contém interpretador da CPU Game Boy
  nem cabeçalho de ROM incorporado. O caminho Lua permanece experimental.

Identidade SHA256 dos seis módulos incorporados:
`470ac2b8b5316096754e6f40237b69d87979c5201b97dc57fa972fe549d512dd`.
Heap do contrato: 30.752 bytes; pico de inicialização 34.150 bytes, limite 256 KiB.
Resultados de replays completos, desempenho e auditorias: ver o relatório final
local `reports/lua-phase-4.md` e as evidências no diretório de outputs.

## Build e pacotes

Ative explicitamente as cinco opções do build experimental:

```sh
cmake -S . -B build-lua -DORACLES_LUA_GAMEPLAY=ON \
  -DORACLES_LUA_BLOCK_PUZZLES=ON -DORACLES_LUA_COLORED_CUBE=ON \
  -DORACLES_LUA_LEVERS_LAVA=ON -DORACLES_LUA_DOORS=ON -DORACLES_VERSION=
cmake --build build-lua --target test_lua_doors test_lua_doors_rom
ctest --test-dir build-lua -R '^lua_doors' --output-on-failure
```

Scripts de build, workflow experimental, manifesto e smoke tests recebem
`ORACLES_LUA_DOORS`. O CI padrão continua com as opções Lua OFF. Os testes
comerciais que exigem ROM não são simulados pelo CI público sem ROM.

`tools/package_lua_phase4.py` exige as opções, identidade dos módulos, perfis,
auditorias e vínculos de cada arquivo nativo com suas regras Lua. Inclui fontes
correspondentes do working tree, licenças e checksums. Empacotadores antigos
recusam identificar builds da fase 4 como uma fase anterior.

Os novos pacotes são `Oracles-Windows-Lua-Phase4.zip`,
`Oracles-Switch-Lua-Phase4.zip` e `Oracles-macOS-Lua-Phase4-Intel.dmg`.
Eles não incluem ROM nem saves pessoais. Instruções e aceitação pendente estão
em [LUA-PHASE-4-PLATFORM-TESTS.md](LUA-PHASE-4-PLATFORM-TESTS.md).
