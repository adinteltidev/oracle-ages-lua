# Migração de gameplay para Lua: sete fases

Atualização: 30/10/2026. Implementação experimental das fases 1–5 disponível.
O jogador validou as versões cumulativas da fase 4 no Windows e Switch e
confirmou funcionamento e estabilidade. A pendência de validação manual dessas
plataformas está atendida para a fase 4. A fase 5 possui implementação e
validação local; a confirmação dos novos pacotes Windows/Switch está pendente.
Fase 6 possui integração experimental em validação local; a aceitação completa
depende das verificações registradas em [LUA-PHASE-6.md](LUA-PHASE-6.md).
Fase 7 ainda não iniciada.

[Fase 5: itens, lojas e validação](LUA-PHASE-5.md).
[Fase 4: inventário, implementação e evidências](LUA-PHASE-4.md).
[Confirmação de Windows/Switch pelo jogador](LUA-PHASE-4-PLAYER-VALIDATION.md).

Continua pendente a regressão completa de Seasons com ROM própria. Essa
verificação mantém a validação comum da migração em aberto; as opções
experimentais continuam OFF por padrão.

Os relatórios de fases anteriores preservam seus resultados e pendências da
época. O primeiro lote está em [fase 1, inventário](LUA-PHASE-1.md), e sua
implementação em [evidências da fase 1](LUA-PHASE-1-IMPLEMENTATION.md).
A confirmação posterior das plataformas está registrada acima.

## Objetivo e organização

Migrar os sete grupos de regras identificados na análise do projeto, nesta ordem:

| Fase | Grupo | Primeira entrega |
| --- | --- | --- |
| 1 | Puzzles com blocos | Decisões e transições de `pushblockTrigger.c` |
| 2 | Cubo colorido | Visibilidade e seleção de paleta em `coloredCubeFlame.c` |
| 3 | Alavancas e lava | Regras e transições de `leverLavaFiller.c` |
| 4 | Portas | Decisões de abertura e fechamento |
| 5 | Itens e lojas | Disponibilidade, compra e recompensas |
| 6 | NPCs e eventos | Condições de diálogo, ações e progressão |
| 7 | Inimigos | Decisões de comportamento e transições da IA |

O destino é um jogo híbrido: regras de gameplay em Lua e serviços de execução em C.
As primeiras entregas da tabela são pontos de entrada de cada fase; migrar apenas
um exemplo não conclui uma categoria inteira. Inventariar as demais regras do grupo
e executar lotes pequenos até cumprir o escopo registrado para a fase.

Trabalhar no repositório `adinteltidev/oracle-ages-lua`, na branch existente
`port/lua-gameplay`, cujo remoto local é `lua`. O código está atualmente no checkout
`work/oracle-switch/source`. Não trabalhar na `main` nem enviar mudanças ao remoto
`origin` do projeto C por engano. Registrar o commit-base real antes de cada lote.

Este documento detalha as ondas de gameplay do `LUA-PORT-PLAN.md`. As condições de
equivalência, persistência e desempenho daquele plano continuam válidas. Não há
etapa adicional numerada: a preparação técnica pertence à fase 1 e os testes e o
empacotamento fazem parte de cada fase.

## Ponto de partida

- Há duas decisões Lua de `switchTileToggler.c` integradas ao jogo de Ages.
- O host atual aceita dois bytes e devolve um booleano. Ainda não oferece uma
  interface geral para objetos, estados, inventário ou eventos.
- Existem testes diferenciais da interação e replay curto. O replay curto não
  alcança a interação migrada; a cobertura positiva vem dos testes dirigidos.
- Foi gerado e testado localmente um aplicativo macOS Intel C + Lua. Existem
  compilações experimentais para Windows e Switch; a execução do piloto nesses
  dois sistemas e as medições completas de desempenho continuam pendentes.
- Antes de ampliar o piloto, concluir as verificações pendentes de comportamento,
  replay completo e desempenho previstas no plano anterior. Registrar obstáculos
  e continuar apenas com trabalho que não dependa da verificação em falta.

## Contrato comum a todas as fases

1. **Inventário:** listar funções, estados e subtipos abrangidos, dependências,
   entradas diretas e indiretas e cenas capazes de exercitar cada regra. Separar
   itens pendentes e exclusões justificadas; nenhum deles conta como migrado.
2. **Autoridade:** Lua recebe valores explícitos e retorna uma decisão validada.
   C aplica efeitos nos pontos originais, conservando registradores, flags,
   memória, ciclos e ordem das operações. Não antecipar leituras que poderiam
   mudar após uma interrupção ou chamada intermediária.
3. **Granularidade:** agrupar decisões quando for semanticamente seguro; evitar
   chamar Lua para cada byte. Não transformar a interface em execução arbitrária
   de comandos de memória nem entregar ponteiros de `GB` aos scripts.
4. **Persistência:** manter inicialmente todo estado durável no estado C já salvo.
   Não guardar progresso em globals, closures ou corrotinas Lua. Se uma regra
   exigir novos dados persistentes, definir antes esquema versionado,
   serialização, compatibilidade e migração de saves; isso bloqueia sua integração
   até os testes correspondentes passarem.
5. **Aritmética e aleatoriedade:** reproduzir explicitamente limites de 8/16 bits,
   sinal, carry e valores sentinela. O RNG continua sendo o original em C, com a
   mesma ordem e quantidade de chamadas; passar seus resultados para Lua.
6. **Referência:** preservar o caminho C e a opção `ORACLES_LUA_GAMEPLAY=OFF`.
   Comparar C e C + Lua do mesmo commit e configuração, com seleção por regra
   nos testes. Não rodar as duas implementações sobre o mesmo objeto já alterado.
7. **Falhas:** validar tipo, faixa e quantidade das saídas antes de aplicá-las.
   Limitar memória e instruções e registrar módulo/regra no diagnóstico. Após
   efeitos já aplicados, parar com diagnóstico; não refazer silenciosamente em C.
8. **Escopo compartilhado:** Ages usa as regras migradas; Seasons mantém o caminho
   C. Rotinas comuns precisam de testes de regressão de Seasons antes de integrar.
9. **Distribuição:** scripts incorporados ao aplicativo, sem exigir instalação de
   Lua. Manter builds experimentais identificados e caches separados. ROMs,
   saves e recursos extraídos do jogador ficam fora do Git e dos pacotes.

## Fase 1 — Puzzles com blocos

**Objetivo:** migrar as condições e transições dos puzzles com blocos, começando
pela interação `interactionCode13` em
`src/game/common/interactions/pushblockTrigger.c`.

**Trabalho:**

1. Registrar a referência C e concluir a validação pendente do piloto anterior.
   Inventariar também os puzzles de blocos em `pushblock.c` e `miscPuzzles.c`,
   distinguindo a regra do puzzle da física de empurrar um bloco.
2. Ampliar `src/script/gameplay.h/.c` para múltiplos módulos e regras nomeadas,
   entradas inteiras explicitamente tipadas e saídas booleanas/inteiras validadas.
   Preservar as duas regras existentes. Definir tipos concretos de entrada/saída
   por regra; uma tabela genérica sem contrato não basta.
3. Incorporar os novos módulos pelo CMake nos três alvos. Rever especificamente
   as definições de compilação do Switch, hoje limitadas ao arquivo do piloto.
4. Criar `gameplay/ages/block_puzzles.lua` para decidir quando liberar o bloco,
   reconhecer a mudança de tile e avançar/concluir os estados do puzzle.
5. Conservar criação do objeto, alterações do tile, contagem de inimigos,
   temporizadores, efeitos de memória e remoção do objeto no wrapper C.
6. Migrar as demais regras do inventário em lotes depois da primeira interação.

**Testes específicos:** condição ainda não cumprida/cumprida, igualdade e limites
da contagem, bloco imóvel/movido, atraso final, múltiplos objetos, saída e retorno
à sala e save/load em cada estado. Verificar chamadas diretas e despacho de hooks.
Adicionar testes de contrato, limites, falhas e isolamento entre os módulos.

**Conclusão:** regras inventariadas resolvidas, cobertura Lua positiva por regra,
nenhuma divergência nos testes e aprovação da validação comum abaixo.

## Fase 2 — Cubo colorido

Implementação: [regras, testes e limites da rodada](LUA-PHASE-2.md).
Distribuição: [guia dos pacotes da fase 2](LUA-PHASE-2-PLATFORM-TESTS.md).
A opção `ORACLES_LUA_COLORED_CUBE` permanece OFF por padrão; pendências físicas
da fase 1 não foram consideradas resolvidas pelo avanço solicitado.

**Objetivo:** migrar as decisões da chama associada ao cubo em
`src/game/ages/interactions/coloredCubeFlame.c`.

**Trabalho:**

1. Mapear o significado dos bits de `wRotatingCubeColor`, da posição e da tabela
   de paletas lida da ROM do jogador.
2. Criar `gameplay/ages/colored_cube.lua` para decidir visibilidade e seleção da
   paleta; C fornece os dados necessários e conserva os acessos aos recursos.
3. Manter inicialização gráfica, escrita de OAM, animação e temporização em C.
   Não tratar esta fase como migração do renderizador ou da física do cubo.

**Testes específicos:** todas as cores válidas e combinações dos bits de controle,
ausência/presença do cubo, inicialização, mudança de cor, preservação dos outros
bits de OAM, troca de sala e save/load. Definir e testar também o tratamento dos
valores fora do domínio esperado, respeitando o comportamento da referência.

**Conclusão:** decisões de cor e visibilidade executadas em Lua com imagem,
estado e ciclos equivalentes nos casos cobertos e validação comum aprovada.

## Fase 3 — Alavancas e lava

Implementação: [inventário, regras e testes](LUA-PHASE-3.md).
Distribuição: [guia dos pacotes da fase 3](LUA-PHASE-3-PLATFORM-TESTS.md).
A integração usa `ORACLES_LUA_LEVERS_LAVA`, OFF por padrão, e mantém as
pendências físicas anteriores registradas. O avanço foi solicitado pelo jogador.

**Objetivo:** migrar as regras do mecanismo em
`src/game/ages/interactions/leverLavaFiller.c` e, em lotes, os mecanismos
equivalentes identificados no inventário.

**Trabalho:**

1. Mapear subtipos, distância da alavanca, estados, contadores, tipos de fonte,
   minisscripts e ordem de atualização dos tiles.
2. Criar `gameplay/ages/levers_lava.lua` para as condições de ativação, escolha de
   encher/esvaziar e transições. Conservar os contadores e o estado no lado C.
3. Manter busca e escrita dos tiles, espera entre alterações, sons e execução
   dos minisscripts em C. Cada retorno Lua precisa corresponder a um ponto de
   decisão original, sem reagrupar atualizações que ocorrem em frames diferentes.

**Testes específicos:** alavanca parcial/completa, cada subtipo e orientação de
fonte, enchimento/esvaziamento, limites da sequência, acionamento repetido,
temporizadores, abandono/retorno da sala e save/load no meio da atualização.
Comparar a sequência de tiles e o frame de cada mudança, além do resultado final.

**Conclusão:** todos os mecanismos do inventário resolvidos, sem diferenças de
cadência, som, mapa ou persistência e com validação comum aprovada.

## Fase 4 — Portas

**Objetivo:** migrar condições e transições das portas de Ages, começando pelo
controlador comum em `src/game/common/interactions/doorController.c`.

**Trabalho:**

1. Inventariar portas de interruptor, chave, chefe e carrinho, seus subtipos e os
   scripts/minisscripts que realmente escolhem quando abrir ou fechar.
2. Criar `gameplay/ages/doors.lua` para essas decisões. Migrar somente a condição
   dentro do script quando ela pertencer ao mecanismo de portas; a execução
   genérica de scripts e os eventos de história ficam para a fase 6.
3. Manter animação, colisão, contagem de portas fechadas, som, consumo efetivo de
   chaves e reposicionamento de Link em C, na ordem original.

**Testes específicos:** cada família de porta, requisitos presentes/ausentes,
ausência e consumo único de chave, gatilhos, Link sobre o tile ao fechar,
reentrada na sala e save/load durante a animação. Validar Seasons no caminho C.

**Conclusão:** inventário de condições e transições atendido; abrir, atravessar
e fechar conserva o comportamento original e passa na validação comum.

## Fase 5 — Itens e lojas

**Objetivo:** migrar regras de disponibilidade, compra e recompensas. Candidatos
iniciais: `src/game/common/interactions/shopItem.c`, `shopkeeper.c` e
`treasureInteraction.c`.

**Trabalho:**

1. Inventariar produtos, preços e suas representações numéricas, inventário,
   progressão, recompensas únicas e condições de jogo conectado que afetem Ages.
2. Criar `gameplay/ages/items.lua` e `gameplay/ages/shops.lua`. Lua decide oferta,
   elegibilidade, resultado da compra e identificador da recompensa, usando
   recursos/tabelas fornecidos por C a partir da ROM do jogador.
3. C valida a decisão e aplica débito, entrega e flags na ordem original.
   Definir como impedir entrega ou cobrança duplicadas ao retomar uma operação.
4. Preservar os pontos de integração dos mods de tesouros; definir explicitamente
   a ordem de decisão Lua, alteração pelo mod e aplicação C antes de integrar.
5. Expandir por família de item; não incluir física de projéteis ou o sistema
   inteiro de uso de armas apenas por serem chamados de itens.

**Testes específicos:** dinheiro insuficiente/exato/sobrando, inventário cheio,
item já obtido, limites e representação do saldo, coleta repetida, cancelamento,
save/load durante compra/entrega e regressão dos mods existentes.

**Conclusão:** todas as regras inventariadas executam em Lua e mantêm saldo,
inventário e flags equivalentes, sem duplicação, com validação comum aprovada.

## Fase 6 — NPCs e eventos

**Objetivo:** migrar seleção de diálogos, ações e progressão dos eventos de Ages.
Começar por um NPC simples, como `pastOldLady.c`, antes de ampliar para
`symmetryNpc.c`, outros personagens e sequências de história.

**Trabalho:**

1. Inventariar NPCs, flags, eras, escolhas de diálogo e dependências entre eventos.
   Distinguir regras de escolha em C de regras hoje executadas pelos scripts da
   ROM. Migrar ambas quando fizerem parte do escopo, sem contar a simples chamada
   de um script original como conversão de todo o evento.
2. Criar módulos em `gameplay/ages/npcs/` e `gameplay/ages/events/`. Retornar IDs
   de texto, ações e estados validados; os textos continuam sendo obtidos da ROM.
3. Representar sequências como máquinas de estados explícitas. Usar estado C
   existente quando suficiente; caso contrário, resolver a persistência
   versionada antes de integrar. Não salvar pilhas de corrotinas Lua.
4. Conservar apresentação de texto, movimento, animação, sons e bloqueio de
   controles em C. Migrar eventos pequenos antes de cenas longas de história.

**Testes específicos:** cada ramificação, escolhas e cancelamentos, antes/depois
de flags de progresso, passado/presente, conversa repetida, entrada/saída de sala,
retomada no meio de evento e impossibilidade de receber recompensa duas vezes.

**Conclusão:** o inventário possui cobertura das ramificações e continuidade
determinística após reload; eventos não deixam controles bloqueados e a validação
comum está aprovada. Exclusões de cenas permanecem visíveis como pendências.

## Fase 7 — Inimigos

**Objetivo:** migrar decisões de IA de Ages, começando por inimigos simples,
como os de `src/game/common/enemies/octorok.c`, e ampliando por famílias.

**Trabalho:**

1. Inventariar famílias, subtipos, estados, RNG, frequência de atualização e
   comportamentos compartilhados. Deixar chefes e sequências complexas para os
   últimos lotes da própria fase, após medir os inimigos simples.
2. Criar módulos em `gameplay/ages/enemies/` para decidir patrulha, direção,
   perseguição, ataque e transições. Receber posição, estado, timers e resultados
   do RNG original; devolver intenção de ação, sem acessar a memória diretamente.
3. Manter integração de movimento, colisões, aplicação de dano, animação e
   criação efetiva de projéteis em C. Não pular frames de decisão nem alterar a
   ordem do RNG para compensar custo de execução.
4. Medir cenas com vários inimigos antes de expandir. Rever agrupamento das
   chamadas, alocações e limites da VM usando dados; preservar o caminho C das
   famílias ainda não migradas e identificar cada uma no inventário.

**Testes específicos:** sementes idênticas, limites de timers e posições,
patrulha/perseguição/ataque, paredes, buracos, dano/morte, projéteis, múltiplos
inimigos simultâneos, mudança de sala e save/load em estados de combate.

**Conclusão:** decisões de todas as famílias do escopo registradas como migradas
e exercitadas, incluindo os lotes complexos; RNG e continuação determinística
equivalentes e orçamento de frames aprovado, especialmente no Switch.

## Validação e entrega obrigatórias em cada fase

| Verificação | Condição para concluir |
| --- | --- |
| Contrato Lua | Tipos, faixas, limites de execução e memória, erros e isolamento testados |
| Equivalência dirigida | Referência C e híbrida com mesmas entradas; comparar memória, registradores, flags, ciclos, vídeo e áudio relevantes |
| Cobertura | Contadores por regra e cenas/casos que realmente a executem; zero chamadas não é aprovação |
| Replays | Replay curto e completo disponível de 289.518 frames, com resultado e cobertura registrados |
| Saves | Slots 1–4, SRAM, retomada após fechar, reload em cada estado migrado e continuação equivalente |
| Regressão | Piloto anterior, fases concluídas, mods e caminho C de Seasons preservados |
| Desempenho | C versus C + Lua do mesmo commit, mediana/P95/P99 de CPU por frame, heap, GC e chamadas; medir sem comparador ativo |
| Plataformas | Execução no macOS Intel e Windows x64; Switch real portátil e dock, incluindo áudio e suspensão/retomada |
| Distribuição | Scripts corretos incorporados, builds identificados, fontes/licenças e ausência de ROMs/saves |

Usar o orçamento proposto no plano anterior: acréscimo inicial de CPU de até 10%
sobre C, sem perda da cadência original de aproximadamente 59,73 frames/s. Esse
limite é uma meta a verificar, não resultado já demonstrado. Registrar também os
valores absolutos, as condições e o hardware; não elevar o limite silenciosamente.

Se um teste ou validação de hardware estiver indisponível, registrar como pendente.
Pode-se preparar contratos e casos dos próximos lotes, mas não declarar a fase
concluída nem ampliar a integração de gameplay sem resolver a condição de avanço.

Em cada fase, ampliar o workflow experimental para os módulos e arquivos tocados,
executar testes sem ROM no CI e gerar artefatos C + Lua dos três alvos. Os testes
com a ROM do jogador continuam locais; a ausência da ROM não conta como sucesso.
A geração de artefatos não publica uma release estável automaticamente.

Entregar um relatório por fase em `reports/lua-phase-N.md`, com inventário,
commit-base, decisões migradas, estado mantido em C, cobertura, testes executados,
medições por plataforma, artefatos e pendências. Incluir hash dos scripts no
diagnóstico do build. Código, testes e documentação devem formar lotes revisáveis;
commit, push e publicação serão feitos quando solicitados.

## Resultado esperado

Ao final das sete fases, as regras dos sete grupos inventariados estarão em Lua,
com os serviços e efeitos de baixo nível em C. Informar progresso por regras,
estados e famílias cobertas, sem estimar percentual do jogo apenas pelo número de
arquivos Lua. Um grupo com regras pendentes não deve ser anunciado como completo.

Não há prazo confiável para todo o escopo antes do inventário e das medições da
fase 1. Atualizar estimativas após cada fase usando o custo observado de migração,
cobertura e validação nas três plataformas.
