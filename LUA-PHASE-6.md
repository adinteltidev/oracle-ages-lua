# Fase 6 — NPCs e eventos

Implementação de 30/10/2026; conclusão registrada em 31/10/2026.
Branch `port/lua-gameplay`, commit de implementação `73e7f75`.

**Status: fase 6 concluída para Ages.** O jogador confirmou que Windows e Switch
estão funcionando e autorizou o encerramento da fase. A confirmação posterior e
seu escopo estão em [validação do jogador](LUA-PHASE-6-PLAYER-VALIDATION.md).

Integração experimental de decisões de NPCs e eventos em Lua. A opção
`ORACLES_LUA_NPCS_EVENTS` é OFF por padrão e exige `ORACLES_LUA_GAMEPLAY`.
A regressão completa de Seasons com ROM própria continua como pendência comum
da migração, separada do encerramento desta fase de Ages.

## Inventário e regras

`gameplay/ages/events/npc-event-inventory.json` classifica 282 unidades de
interações de Ages/comuns: 193 recebem políticas de NPCs/eventos, `miscPuzzles`
recebe apenas os subids de evento 07/11/12/13, e as demais mantêm os serviços de
física, apresentação ou mecanismos de outras fases. Há 130 IDs de proprietário
no despacho de scripts, mais os quatro subids explicitamente selecionados de 72.
Portas, puzzles e lojistas das fases anteriores conservam seus próprios domínios.

O inventário inclui também sete controladores globais de cenas: as seis unidades
de cutscenes e o ator da cena de reviver Ganon. Essas decisões funcionam sem
proprietário NPC ativo; não são contadas como visitas a NPCs. Movimento de Link/
companheiros, apresentação e IA de combate continuam em C.

O inventário identifica unidades de código, não o número de personagens ou de
conversas aprovadas. Helpers do banco 15 podem pertencer à mesma família. A
cobertura de regras, o trajeto do replay e os cenários dirigidos são evidências
separadas; uma entrada no inventário não é tratada como visita a todas as cenas.

`gameplay/ages/npcs/rules.lua` contém comparações e predicados de disponibilidade,
progresso e flags. A senhora do passado tem regras explícitas para existência
após a flag de progresso e escolha entre os quatro roteiros de estágio. Symmetry
tem regras para a existência do subid 0c após a Tuni Nut e a retomada do roteiro
nos estados 1/2 durante a restauração. As demais unidades usam os predicados nos
pontos originais de decisão: igualdade/ordem, zero, bits e carry de serviços C.

`gameplay/ages/events/flow.lua` fornece as decisões dos scripts: comparações de
memória/flags/escolhas, seleção de índice de tabela, passagem de estado/subestado,
incremento com wrap de byte, texto, jogo conectado, espera por texto e término.
São 20 regras tipadas em `src/script/npcs_events.h`, incluindo as regras de NPCs.

## Fronteira e persistência

As condições escritas em C e as condições executadas pelo interpretador dos
scripts da ROM passam pelas novas decisões Lua. A integração não contabiliza
apenas carregar o endereço de um script como conversão de seu evento. O programa
e os endereços dos scripts continuam sendo recursos da ROM; C mantém o despacho
de comandos, leituras/ALU/ciclos, movimento, animação, colisão, som e os efeitos
sobre flags, inventário e controles. Não houve transcrição integral dos roteiros
para novos textos Lua.

Lua governa as saídas de decisão consumidas pelo ramo ou escrita seguinte. As
máquinas de estados usam os campos existentes do ator, subestado, PC/retorno do
script, timers e flags. `transition` aceita o estado atual ou o solicitado;
`next_state`, o atual ou seu sucessor original de byte. Não é salva uma pilha
Lua, corrotina ou novo progresso global. GB/SRAM/formato de save permanecem iguais.

O domínio do índice de tabela não pode aumentar além da entrada original. A
senhora do passado tem uma entrada no subid 0 e quatro no subid 1; não é criado
um clamp para trocar um recurso original danificado por outro. Texto deve ser o
ID fornecido pela ROM ou o ID 0000, sem construir endereços arbitrários. O texto
ligado/não ligado usa o fato original do estado do jogo conectado.

A identidade do proprietário é lida do objeto ativo a cada decisão, preservando
isolamento entre fibers. As chamadas C e os retornos dos scripts propagam falhas
antes de continuar. Uma resposta inválida interrompe o motor com regra e motivo,
sem fallback silencioso e sem rollback dos efeitos C legítimos anteriores.
O host de mods e sua ordem de substituição de tesouros não foram modificados.

## Verificação local

- Contrato das 20 regras: pares de bytes, flags, IDs de texto, estados/subestados,
  limites de execução/memória, tipos/domínios e ausência de alocações no caminho
  quente. Heap/pico observado: 47.632 bytes, limite 256 KiB.
- 2.717 casos dirigidos com comparação de GB e amostras completos. Incluem todos
  os IDs inventariados de proprietário e isolamento das fases anteriores,
  transições nos 256 valores de byte e comandos reais do interpretador.
  Incluem três estados globais de cenas × 256 valores sem proprietário ativo,
  além de autoridade para reter estado e falha antes da escrita.
- 44 cenários de NPCs × 80 frames por rodada: senhora do passado em ambos os
  subids e oito estágios de progresso, todos os treze subids de Symmetry antes/
  depois do reparo, menina e homem do passado. Conversas e escolhas se repetem.
  A rodada com retomada fez 264 operações save/load e recriação da VM.
- Seis cenários de abertura do portão Maku/torre negra × 100 frames com comparação
  C/Lua, incluindo progresso e disponibilidade da Maku Seed.
- Respostas válidas alternativas demonstram autoridade sobre existência,
  estado, texto e ramificação. Respostas inválidas de cada uma das 20 regras
  são injetadas em hooks reais e interrompem a execução.
- Os 203 arquivos nativos alterados geram código de máquina idêntico ao commit
  base quando Lua está desligado. A auditoria considera a versão final dos
  macros e das regras semânticas.

As conversas e ramificações não visitadas por esses cenários ou pelo replay não
são contadas como execução em hardware. O relatório local
`reports/lua-phase-6.md` registra os resultados finais dos replays, regressões,
medições, auditorias e pacotes. Fase 7 permanece pendente.

## Build e distribuição

```sh
cmake -S . -B build-lua -DORACLES_LUA_GAMEPLAY=ON \
  -DORACLES_LUA_BLOCK_PUZZLES=ON -DORACLES_LUA_COLORED_CUBE=ON \
  -DORACLES_LUA_LEVERS_LAVA=ON -DORACLES_LUA_DOORS=ON \
  -DORACLES_LUA_ITEMS_SHOPS=ON -DORACLES_LUA_NPCS_EVENTS=ON -DORACLES_VERSION=
cmake --build build-lua --target test_lua_npcs_events test_lua_npcs_events_rom
ctest --test-dir build-lua -R '^lua_npcs_events' --output-on-failure
```

As sete opções são propagadas aos scripts de plataforma e ao workflow
experimental. O CI padrão conserva Lua OFF; fixtures com ROM não são simulados
no CI público. Os pacotes da fase 6 usam perfil próprio, incluem fontes
correspondentes/licenças/checksums e não incluem ROM ou saves pessoais.
Atualização de releases estáveis permanece desativada nos builds experimentais.

SHA256 dos dez módulos Lua incorporados:
`7737d9b8f3d27271d3d97bc1e048d827bf8505dc9426691b71a91a3b81fe75e7`.
Roteiro e pacotes: [LUA-PHASE-6-PLATFORM-TESTS.md](LUA-PHASE-6-PLATFORM-TESTS.md).

## Medição de CPU e distribuição local

Os traces de 960 frames na cena com quatro NPCs coincidiram entre build C,
build Lua com integração OFF e Lua ON, normalizando apenas ponteiros do processo.
A medição separada usou nove rodadas, três por variante em ordem alternada,
480 frames de aquecimento e 1.600 medidos. A medição final ocorreu após o término
dos replays e das compilações. Não houve hashing nem I/O de save no intervalo
medido.

Mediana das médias de CPU: 2,809 ms no build C, 2,469 ms com integração OFF e
2,479 ms com Lua ON. A diferença observada de −11,74% ficou dentro do orçamento de
10%; zero alocações Lua nos frames medidos. Essa variação local não comprova
aceleração nem mede desempenho de Windows/Switch ou de todas as cutscenes.
P95/P99 e as rodadas completas ficam no relatório de evidências.

Os builds macOS Intel, Windows x64 e Switch foram produzidos. O Windows mantém
manifesto UTF-8, sem CLR ou DLL privada. O NRO tem título/perfil da fase 6, com o
ícone **recomp**. Auditorias verificam ausência de interpretador da CPU Game Boy
e de cabeçalho de ROM incorporado. O fluxo de manifesto/empacotamento do CI foi
exercitado localmente; isso não é uma execução do workflow no GitHub.
