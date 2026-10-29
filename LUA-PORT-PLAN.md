# Plano de migração gradual de C para Lua

Data: 28/10/2026.

Complemento de 29/10/2026: [migração de gameplay em sete fases](LUA-MIGRATION-PHASES.md).
Esse documento detalha a expansão para blocos, cubo colorido, alavancas/lava,
portas, itens/lojas, NPCs/eventos e inimigos, preservando os critérios deste plano.

## Objetivo e isolamento

Migrar progressivamente regras do jogo para Lua, conservando em C a infraestrutura
de plataforma e as partes sensíveis à temporização e ao desempenho. Começar por
uma prova de conceito de uma rotina real de Ages; ampliar somente após demonstrar
equivalência de comportamento, save/load e desempenho.

- Repositório: `adinteltidev/oracle-ages-native`.
- Branch exclusiva: **`port/lua-gameplay`**.
- Base: `origin/main`, commit **`e7b829f`**, incluindo os menus e a versão exibida
  em Windows, macOS e Switch.
- A execução do piloto está descrita em `LUA-GAMEPLAY.md`: duas decisões da
  interação de tiles agora são executadas em Lua no build optativo. Não há
  commit, push ou release novos nesta implementação.
- Toda implementação, documentação e testes desta migração devem ocorrer nessa
  branch. A `main` permanece com a implementação C atual até a aceitação do port.
- Antes de implementar, atualizar a branch com mudanças posteriores da `main`,
  preservando a referência C usada para comparação. Registrar o commit-base de
  cada rodada de medição, em vez de comparar builds de origens diferentes.

## Escopo inicial

| Permanece em C inicialmente | Migra gradualmente para Lua |
| --- | --- |
| SDL3 desktop; SDL2/libnx Switch; arquivos e controles | Decisões de NPCs e interações |
| Memória, registradores e operações de 8/16 bits | Regras de itens e objetos |
| Temporização, interrupções, PPU/APU e apresentação | Eventos e máquinas de estados de gameplay |
| Extração de recursos da ROM do jogador | Regras e tabelas próprias independentes de dados da ROM |
| Fibras nativas e suporte a save/load | Estado de gameplay explicitamente serializável, quando necessário |

A primeira entrega não troca o motor inteiro por Lua, não substitui o frontend
por LÖVE e não depende de LuaJIT. Lua 5.4.9, já presente no repositório, será a
base inicial comum às três plataformas. Seasons permanece no caminho C durante
o piloto de Ages; o que for compartilhado precisa conservar seu comportamento.

## Arquitetura proposta

Usar a mesma distribuição Lua já incorporada ao projeto, separando a biblioteca
da linguagem de seu host de mods. O host de gameplay deve ser próprio: os mods
atuais são chamados pelo frontend e sua API/budget não equivalem ao motor do jogo.
Conservar a API pública e o isolamento dos mods existentes.

Estrutura sugerida, a confirmar no primeiro marco de implementação:

```text
src/script/                 # host C, bindings, diagnóstico e estado do gameplay
gameplay/ages/              # rotinas Lua do jogo, pertencentes ao projeto
tests/                     # equivalência, determinismo, persistência e orçamento
```

Criar uma opção de build **proposta** `ORACLES_LUA_GAMEPLAY`, inicialmente OFF.
Os builds e runners C continuam funcionando sem carregar scripts. Criar um alvo
híbrido separado, inclusive para o runner de replay, em vez de ligar Lua
incondicionalmente ao núcleo C de referência.

O despacho deve selecionar C ou Lua para cada rotina migrada. Uma rotina executa
uma implementação por vez; manter a C como referência de testes. Revisar chamadas
internas que chegam por `CALL_C`/`TAIL` e `hook_run`, além de `hook_dispatch`, para
que a nova seleção não seja contornada por outro ponto de entrada.

A API entre C e Lua deve operar em decisões ou operações agrupadas, evitando uma
chamada C/Lua para cada acesso a um byte. Os scripts recebem dados explícitos e
devolvem decisões/comandos validados. No piloto, o wrapper C conserva a ordem
das operações, registradores, flags e temporização necessários à equivalência.
Isso precisa ser provado, e não presumido por consumir o mesmo total de ciclos.

## Marcos de execução e condições de avanço

### 1. Referência C e contrato da prova de conceito

1. Registrar versões de ferramentas, configuração, plataforma e hash da ROM
   fornecida pelo jogador. Não copiar ROM ou saves para Git/CI.
2. Executar a referência C e registrar comportamento, custo de CPU, memória,
   áudio, hashes e save/load nas cenas de interesse.
3. Examinar as dependências da rotina candidata antes de escolhê-la.
   `src/game/common/interactions/switchTileToggler.c` é um candidato concreto:
   reage ao estado de um interruptor e troca um tile. Não é escolha definitiva;
   verificar se suas dependências e pontos de entrada permitem um piloto isolado.
4. Definir entradas, saídas, efeitos sobre memória, caminhos condicionais,
   temporização e casos de inicialização/atualização da rotina escolhida.

**Avanço:** existe um caso reproduzível que alcança a rotina, com resultados C
conhecidos. Um replay que nunca executa o trecho não serve como prova da migração.

### 2. Runtime Lua compartilhado e desligável

1. Separar o target da biblioteca Lua do host `oraclesmods` sem mudar o contrato
   dos mods existentes.
2. Criar o host de gameplay e conectar o ponto de execução optativo aos alvos
   Windows, macOS e Switch, incluindo o runner de testes.
3. Definir limites de memória e execução a partir da medição. Medir alocações e
   pausas do coletor; evitar criar tabelas e closures sem necessidade a cada frame.
4. Carregar apenas os scripts previstos pelo build. Distribuí-los como recursos
   do aplicativo ou incorporados ao binário, sem exigir Lua instalado no sistema.
   Não introduzir recarga de scripts no meio de uma partida nesta fase.
5. Informar erros e a identificação da versão dos scripts no diagnóstico.
   Scripts de gameplay não precisam de rede nem de execução de programas externos.

**Avanço:** os três alvos compilam e o runtime inicia/encerra corretamente. Com
a opção OFF, os testes C continuam passando e o jogo não depende de scripts.

### 3. Uma rotina realmente executada em Lua

1. Migrar a decisão da rotina selecionada, mantendo os serviços de baixo nível
   em C. Sua execução deve substituir o caminho C correspondente, não apenas
   observar ou alterar o resultado como um mod.
2. Usar inicialmente uma função Lua pura: entradas explícitas e retorno
   validado, sem estado persistente em globals/upvalues/corrotinas.
3. Cobrir inicialização, estado inalterado, mudança de estado e valores-limite.
   Testar aritmética de 8/16 bits e conversões quando relevantes.
4. Comparar a execução C com a híbrida sobre estados equivalentes e manter um
   contador de cobertura que comprove chamadas à implementação Lua.

**Avanço:** a rotina foi executada em Lua e produz os mesmos efeitos funcionais
e de temporização observáveis da referência nos casos cobertos. Conservar esse
piloto pequeno até validar a sua arquitetura nas três plataformas.

### 4. Determinismo e saves antes de ampliar o port

1. Rodar os testes diferenciais da rotina e o replay de Ages de 20.000 frames;
   depois, o replay completo disponível, até 289.518 frames/4.825 checkpoints.
   Registrar quantas vezes a rotina migrada foi alcançada em cada replay.
2. Complementar os hashes existentes com verificação dos registradores, ciclos,
   efeitos de memória, framebuffer e áudio relevantes. Não tratar um hash que
   cobre apenas parte do estado como prova universal de equivalência.
3. Verificar save/load repetido, os quatro slots, retomada após fechar o app,
   troca de salas, pausa, reinício e continuidade determinística após carregar.
4. Durante o piloto sem estado Lua persistente, preservar o layout de `GB` e o
   formato de estado atual sempre que possível. Testar estados anteriores.
5. Antes de introduzir estado permanente em Lua, definir um formato de dados
   explícito, versionado e determinístico. Não tentar salvar a VM, ponteiros,
   closures ou a pilha de uma corrotina como um dump de memória.
6. Se for necessário novo formato, implementar leitura/migração das versões
   suportadas ou rejeição explicada, sem sobrescrever os estados originais.
   Manter a SRAM separada dos quicksaves e verificar sua compatibilidade.

**Avanço:** não há divergência conhecida nos testes executados, a cobertura Lua
é positiva e save/load restaura todo o estado necessário, inclusive o que vier
a existir fora de `GB`.

### 5. Desempenho e execução real nas três plataformas

Comparar C e C+Lua em builds de release equivalentes. Fazer a verificação de
correção e a medição de desempenho em execuções separadas, para não atribuir à
Lua o custo do comparador de estados.

Registrar tempo de CPU por frame, mediana/P95/P99, memória máxima e após GC,
alocações, chamadas C/Lua, áudio e atrasos. Como meta inicial proposta, limitar
o acréscimo de CPU do piloto a 10% sobre a referência; reavaliar esse orçamento
com os dados, sem relaxar silenciosamente o requisito de fluidez.

Preservar a cadência original de aproximadamente 59,73 frames/s, cerca de
16,74 ms por frame. Validar a carga total de gameplay contra esse orçamento,
sem confundir espera por apresentação com custo de execução.

No Switch, medir em hardware, com memória completa de aplicação, nos modos
portátil e dock, sem depender de JIT ou alterar clocks para mascarar regressões.
Verificar também áudio, controles, HOME/suspensão e SD. Compilação cruzada e
testes no Mac não substituem essa validação. Executar a aplicação no Windows
real, além da compilação cruzada, e no macOS suportado.

**Avanço:** o piloto mantém comportamento, fluidez e uso de memória aceitáveis
em todos os alvos. Caso falhe, ajustar a granularidade da API ou manter o trecho
em C; não iniciar uma conversão em massa para compensar um piloto inconclusivo.

### 6. CI, empacotamento e recuperação

1. Manter os builds C atuais como controle e acrescentar builds híbridos
   identificados separadamente, sem substituir artefatos de produção.
2. Executar testes sem ROM no CI: bindings, decisões com dados sintéticos,
   limites, erro de script e serialização. Registrar como pendentes/skipped os
   testes com ROM indisponível; não reportá-los como aprovados.
3. Validar que os scripts da versão correta entram nos pacotes Windows/macOS
   e no NRO/recursos do Switch; conservar licenças e fontes correspondentes.
4. Durante o piloto, só aplicar os resultados Lua depois de validá-los. Uma
   falha antes de qualquer efeito pode permitir cancelar a operação; depois de
   alterar o estado, não executar novamente a rotina C como fallback silencioso.
   Pausar e informar o erro, ou restaurar um checkpoint completo validado.

**Avanço:** há builds reproduzíveis, resultados identificáveis e um caminho de
recuperação testado. Esta branch não publica releases estáveis automaticamente.

### 7. Migração por ondas pequenas

Após aprovação do piloto, migrar pequenos conjuntos coesos de interações, NPCs,
itens e eventos. Para cada conjunto: contrato, implementação Lua, comparação C,
save/load, desempenho e teste dos três alvos. Deixar o agendador, a troca das
fibras e os subsistemas de vídeo/áudio em C até existir motivo e evidência para
reavaliá-los. Se corrotinas Lua se tornarem necessárias, tratar yield em
callbacks C e serialização como um marco próprio, não como troca mecânica de API.

Revalidar Seasons antes de migrar rotinas compartilhadas para produção. Definir
o que conta como gameplay migrado e reportar rotinas efetivamente executadas em
Lua; carregar a VM ou chamar o motor C por um script não prova essa migração.

## Entrega inicial e revisão

A primeira implementação estará concluída quando uma rotina real de Ages rodar
em Lua nas três plataformas, com cobertura demonstrada, equivalência nos testes,
save/load validado e medições comparáveis, mantendo o caminho C funcional.

Registrar o alcance e os limites do experimento. Somente então abrir/revisar o
PR da branch `port/lua-gameplay` e decidir entre ampliar a migração, manter a
solução híbrida pequena ou interromper o experimento. Não há prazo confiável
para o port integral antes dessas medições.

## Referências para a implementação

- `src/game/asm.h`, `src/hooks/hooks.h` e `src/rt/fibers.c`: contrato nativo.
- `src/mods/`, `MODDING.md` e `third_party/lua/`: integração Lua existente.
- `src/platform/setup.c`: formato atual dos estados.
- `tests/test_native_tas.c`, `tests/test_switch_quicksave.c` e `tas/`: regressões.
- [Lua 5.4 — API C e corrotinas](https://www.lua.org/manual/5.4/manual.html).
- [LuaJIT — plataformas e requisitos](https://luajit.org/status.html): uma
  alternativa futura a avaliar; não é pré-requisito do piloto homebrew.
