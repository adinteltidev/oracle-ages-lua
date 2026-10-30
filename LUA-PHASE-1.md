# Fase 1: puzzles com blocos — primeiro lote

Data: 29/10/2026. Branch: `port/lua-gameplay`.
Referência inicial: `ed4b912` (antes desta rodada; o plano de sete fases estava
como alteração local). Estado: em implementação, fase ainda não concluída.

Continuação a partir de `b04e0d2`: [validação de cobertura e desempenho](LUA-PHASE-1-VALIDATION.md).
Continuação a partir de `f8eb6b2`: [pacotes de teste Windows/Switch](LUA-PHASE-1-BUILDS.md).
Continuação a partir de `73c4fd7`: [candidato da permissão de blocos ligada ao cubo](LUA-PHASE-1-CUBE-CANDIDATE.md), ainda sem integração ao jogo.
Os resultados abaixo descrevem o primeiro lote; a continuação registra as novas
evidências sem atribuí-las retrospectivamente aos testes anteriores.

## Escopo e inventário inicial

| Rotina/grupo | Regras e dependências | Estado |
| --- | --- | --- |
| `common/interactions/pushblockTrigger.c` / `interactionCode13` | Inicializar; aguardar contagem de inimigos atingir o limiar; detectar mudança do tile; concluir após decremento do contador | Implementado e aprovado nos testes dirigidos locais; hardware pendente |
| `common/interactions/pushblock.c` / `interactionCode14` | Permissão ligada ao cubo, propriedades e condições de assentamento; movimento/colisão ficam em C | Candidato da permissão preparado e testado, sem integração; propriedades/assentamento pendentes |
| `ages/interactions/pushblockSynchronizer.c` / `interactionCodebd` | Sincronização de blocos e estado do puzzle | Pendente |
| `ages/interactions/waterPushblock.c` / `interactionCode9e` | Gatilhos de inundação e layouts de salas; efeitos em C | Pendente |
| `ages/interactions/stonePanel.c` | Gatilho e transições dos painéis de pedra | Pendente |
| `ages/interactions/miscPuzzles.c`, subids `01`, `0c` | Soluções por posicionamento de objetos com switch hook; dependem da verificação de tiles | Pendente |
| Mesmo arquivo, subids `02`, `03`, `14`, `15` | Posições/cores do cubo e condição da recompensa; a chama visual pertence à fase 2 | Pendente |
| Mesmo arquivo, subid `0d` | Escada após deslocar painéis | Pendente |
| Mesmo arquivo, subids `18`, `1a` | Estátuas/blocos posicionados para chave/baú | Pendente |
| Mesmo arquivo, subid `1f` | Detecção de Link preso e reinício de sala no puzzle de D5 | Pendente |
| Mesmo arquivo, helper `miscPuzzles_verifyTilesAtPositions` | Comparações de tiles e formato de dados da ROM; compartilhado com outros puzzles | Pendente; definir contrato sem antecipar leituras |

O inventário é inicial: deve ser conferido por chamadas e cenas antes de afirmar
cobertura de toda a categoria. Água, cubos e painéis entram aqui pelas regras dos
puzzles de blocos, sem converter a física, o renderizador ou o sistema genérico de
scripts. Subids de alavancas, portas, recompensas genéricas e eventos de história
continuam nos grupos das fases correspondentes.

## Primeiro lote

`gameplay/ages/block_puzzles.lua` fornece `next_state(state, a, b)`:

| Estado de entrada | Dados observados | Decisão Lua |
| --- | --- | --- |
| 0 | Nenhum | Inicializar no estado 1 |
| 1 | Subid/limiar e inimigos restantes | Continuar em 1 ou liberar para 2 quando limiar >= contagem |
| 2 | Tile original e tile atual | Continuar em 2 ou avançar para 3 se mudou |
| 3 | Resultado zero do decremento original do contador | Aguardar em 3 ou concluir com ação terminal 4 |

O valor 4 é uma ação terminal, não um novo estado persistente do objeto. O wrapper
C zera a contagem e remove a interação como antes. Ele preserva os pontos de
leitura, a ordem de escrita, os registradores, flags e ciclos. Não foi criada
memória persistente Lua nem alterado `GB` ou o formato dos saves.

O host agora carrega múltiplos módulos em ambientes separados na mesma VM,
registra regras por módulo/nome, valida inteiros/transições e a quantidade de
retornos, limita memória/instruções e conserva a API anterior dos interruptores.
A inicialização que pode alocar está dentro de uma chamada protegida. Os módulos
continuam sem bibliotecas padrão, arquivos, rede ou RNG próprio.

As regras têm contadores de cobertura por estado. O build identifica o conjunto
de fontes Lua por SHA256, gerado pelo CMake a partir dos hashes dos dois módulos;
substituições usadas pelos testes não alteram essa identidade do build.

## Ativação e limites

A integração dos blocos usa uma opção separada, **OFF por padrão**:

```sh
cmake -S . -B build-lua-phase1 -DORACLES_SDL=OFF \
  -DORACLES_LUA_GAMEPLAY=ON -DORACLES_LUA_BLOCK_PUZZLES=ON
cmake --build build-lua-phase1 --target \
  test_lua_gameplay test_lua_gameplay_rom test_lua_blocks_rom test_native_ages
ctest --test-dir build-lua-phase1 --output-on-failure \
  -R '^(lua_gameplay|lua_gameplay_rom|lua_blocks_rom|native_ages)$'
```

Apenas `ORACLES_LUA_GAMEPLAY=ON` mantém a integração anterior dos interruptores;
os blocos continuam em C. Com ambas OFF, o jogo continua no caminho C. Seasons
não chama as regras Lua dos blocos. Os quatro scripts de build aceitam
`ORACLES_LUA_BLOCK_PUZZLES=ON`, mas exigem também `ORACLES_LUA_GAMEPLAY=ON`.

O CI experimental executa testes de contrato sem ROM nos três sistemas e acompanha
os arquivos novos. Testes com ROM e comparação da interação real são locais.
A execução do CI remoto só poderá ser comprovada após um push autorizado.

## Evidências e critérios pendentes

Resultados locais do primeiro lote:

- **Seis testes aprovados:** `lua_gameplay`, `lua_gameplay_rom`, `lua_blocks_rom`,
  `native_ages`, `switch_quicksave` e `app_menus_ages`.
- **262.147 decisões** no teste sem ROM: 131.072 dos interruptores e 131.075 dos
  blocos, cobrindo todos os pares de bytes das condições de liberação e mudança
  de tile, inicialização e os dois resultados do atraso. Heap observado: 8.187
  bytes; o tempo desse teste não é um benchmark de frames.
- **896 casos da interação real de blocos**, pelos caminhos direto e despachado:
  estado GB e 16 amostras de vídeo iguais ao controle C. Cobertura Lua por estado:
  **128 / 128 / 128 / 512**. Inclui limites de contagem e os 256 valores iniciais
  do contador (inclusive o wrap de zero para 255).
- Ciclo completo do puzzle até a remoção, early-outs, save/load em cada estado
  com VM recriada e interrupção por retorno Lua inválido: aprovados. O fixture
  foi corrigido para definir `hActiveObjectType`, como faz o motor antes de
  chamar a interação; a asserção da posição lembrada detectou a omissão inicial.
- Interação anterior de interruptores: 512 casos, cobertura 512/240, aprovada.
- Replay curto: 333 checkpoints em 20.000 frames, normal e com reloads, aprovado.
  **Cobertura Lua zero** nos interruptores e blocos desse replay; a prova positiva
  do lote vem dos testes dirigidos, não desse percurso.
- Quatro slots, SRAM preservada no reinício, continuação de 120 frames e menus do
  aplicativo: aprovados nos testes locais.
- AddressSanitizer + UndefinedBehaviorSanitizer no teste do runtime: aprovados,
  incluindo falta de memória, limite de instruções, isolamento e retornos inválidos.
- Build do app e CLI macOS Intel: aprovado. Auditoria do arquivo nativo: 880
  objetos, sem interpretador de CPU GB; binários verificados sem cabeçalho da ROM.
- Runtime/teste sem ROM e objeto da interação compilados para Windows x64;
  runtime e objeto compilados para Switch ARM64. Símbolos confirmam chamadas à
  nova API nos dois objetos. **Não foram gerados nesta rodada novos pacotes
  Windows/NRO nem executados esses alvos.**
- O objeto C sem a opção experimental não referencia Lua e seu código de máquina
  otimizado (`clang -O2`, comparação do disassembly) é idêntico ao do commit-base.
  Lint do jogo: zero problemas. Sintaxe dos quatro scripts de build: aprovada.
- Identidade das fontes Lua incorporadas:
  `6e6f9d18d78f8759ee04f90578c48cd5cc6daf2e4b70f501e0f3259c18aeaf12`.

O replay completo da referência inicial terminou com exit 0: **4.825 checkpoints
em 289.518 frames**, e **4.824 checkpoints em 289.518 frames com recargas**. Todos
os checkpoints comparados coincidiram; o modo com recargas comparou um checkpoint
a menos, portanto não representa verificação de todos os checkpoints da execução
normal. A cobertura Lua foi **zero** nesse percurso. O teste usou o binário inicial,
antes das alterações deste lote: não atribuir seu resultado à nova integração dos
blocos. O replay completo com os blocos ativados ainda está pendente.

A integração optativa permite comparação dirigida sem promover o lote a padrão.
Ainda é necessário concluir as validações do piloto e as condições de avanço
previstas em `LUA-MIGRATION-PHASES.md`, incluindo Windows e Switch reais.

Permanecem pendentes os outros grupos do inventário, cenas naturais cobrindo cada
regra, troca de sala e múltiplos objetos, compatibilidade com Seasons no jogo,
benchmark comparativo de frames (mediana/P95/P99, heap e GC), distribuição dos três
alvos e validação física do Switch portátil/dock e do Windows. Compilar um alvo
não conta como executá-lo. Um replay com zero chamadas não comprova cobertura.

## Evidências locais

Resultados detalhados e logs estão em `reports/lua-phase-1/` (diretório ignorado
pelo Git, por conter artefatos de validação locais). O resumo acima é versionável
neste arquivo. O primeiro lote é registrado em commit local a pedido do usuário;
não houve push ou publicação nesta rodada.
