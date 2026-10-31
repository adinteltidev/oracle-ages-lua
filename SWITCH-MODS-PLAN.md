# Plano — carregador de mods externos no Nintendo Switch

31/10/2026. Base: `port/lua-gameplay`, commit `9e1c1ab`.
Status: implementação local aplicada na branch `feat/switch-mod-loader`.
Testes, builds e pacote documentados em [SWITCH-MODS-IMPLEMENTATION.md](SWITCH-MODS-IMPLEMENTATION.md).
O aceite das medições e dos testes no Switch real permanece pendente.

Ao aplicar, criar a branch `feat/switch-mod-loader` a partir de `port/lua-gameplay`,
preservando os documentos locais pendentes. A fase 7 de gameplay permanece como
base de comparação. As etapas abaixo formam entregas sequenciais e revisáveis.

## Objetivo e resultado esperado

O mesmo pacote `mod.ini` + `main.lua` aceito no desktop poderá ser instalado
manualmente no SD, descoberto pelo frontend Switch e executado usando a API
`oracles` versões 1 e 2. Isso inclui leitura do jogo, callbacks, overlays,
persistência, opções e as alterações permitidas de memória/recompensas.

Na versão Lua da fase 7, a instalação será:

```text
sdmc:/switch/oracle-ages-lua-phase7/mods/
  meu-mod/
    mod.ini
    main.lua
```

O caminho deve ser derivado de `DATA`, para acompanhar também os demais perfis
existentes. A primeira versão será para Ages, jogo já atendido pelo frontend
Switch atual. O filtro `games=` continuará sendo respeitado.

A instalação inicial será por cópia de pastas no SD. Catálogo online, download,
instalação automática de ZIPs e substituição dinâmica dos onze módulos internos
de gameplay ficam fora desta entrega.

## Situação verificada no código

- O carregador está em `src/mods/{mods,api,sandbox,overlay,treasures}.c`.
- O frontend desktop, `src/platform/app.c`, chama `mods_init`, monta `ModsFrame`,
  executa `mods_frame`, desenha o overlay, aplica a fila de escritas e liga
  `features.treasure_swap` a `mods_treasure_swap`.
- O build Switch retorna de `CMakeLists.txt` após incluir
  `platform/switch/targets.cmake`, antes da criação de `oraclesmods` e das
  dependências desktop. O NRO não liga esse carregador.
- `src/platform/switch_main.c` já possui o loop de jogo, snapshot `GBSample`,
  textura SDL2, tratamento de pausa/suspensão, menu e caminhos de dados.
- `oraclesmods` depende hoje de `oraclesui`, ícones e Lua; `oraclesui` agrega
  serviços além dos recursos mínimos necessários aos mods.
- A API de desenho opera em coordenadas do jogo. No Switch atual, será 160 × 144,
  com escala aplicada depois, e não a resolução física da TV.
- O host atual aceita até 32 mods, com teto de 8 MiB por VM; o callback tem limites
  de instruções e prazo de 4 ms. Somar os limites de cada mod não garante 60 FPS.
- A persistência atual usa `store.ini` na pasta do próprio mod. O Switch dispõe
  de `switch_storage_sync`, `switch_storage_commit` e recuperação por `.bak`,
  apropriados para substituição recuperável no SD. Isso não equivale a garantia
  de atomicidade diante de perda de energia.

## Etapa 1 — compatibilidade e referência

**Trabalho**

1. Registrar o comportamento do NRO da fase 7 sem mods: replay, tempo de CPU,
   áudio, memória, saves, menu e suspensão.
2. Usar o toolchain Switch já fixado para testar os requisitos reais do host:
   `dirent`, `stat/lstat`, abertura de arquivos, `fdopen`, `O_NOFOLLOW`, relógio
   monotônico e funções da biblioteca padrão usadas pelo sandbox.
3. Definir adaptações pequenas de plataforma para os recursos indisponíveis,
   com contratos explícitos. Não substituir silenciosamente uma verificação
   indisponível por sucesso ou uma flag de abertura por zero.
4. Adicionar a opção experimental `ORACLES_SWITCH_MODS`, inicialmente OFF. Ela
   será independente das opções de migração de gameplay; o runtime Lua já existe
   como biblioteca compartilhável no build.

**Entrega/aceite:** relatório das dependências, build de prova e referência
reproduzível sem mods. As diferenças de filesystem/relógio ficam conhecidas antes
da integração ao loop.

## Etapa 2 — compartilhar o host e suas dependências

**Trabalho**

1. Refatorar a configuração CMake para disponibilizar o mesmo `oraclesmods` aos
   frontends desktop e Switch, antes do retorno antecipado apropriado.
2. Separar/reutilizar os recursos mínimos de fonte da ROM e ícones de itens,
   evitando arrastar o frontend SDL3, sincronização HTTP ou o conjunto inteiro
   de filtros desktop para o NRO.
3. Ligar o host somente ao frontend. `oraclesnative`, os runners headless e a
   verificação TAS continuam sem capacidade de descobrir mods externos.
4. Preservar os contratos e testes desktop da API 1/2 e manter VMs de mods
   separadas da VM dos onze módulos de gameplay.

**Entrega/aceite:** NRO compila e liga o host; testes existentes de mods no desktop
passam. Inspeção das dependências confirma a separação de frontend e motor.

## Etapa 3 — descoberta, sessão e armazenamento no SD

**Trabalho**

1. Criar/usar `DATA/mods/` e reutilizar o parser de `mod.ini`, validação de IDs,
   `games=ages`, compatibilidade de API e ordem alfabética.
2. Integrar `mods_init` ao início da sessão e `mods_shutdown` a todas as saídas,
   inclusive falha de inicialização. Ausência de mods deve ser operação normal.
3. Preservar o isolamento por mod: um `main.lua` inválido ou um estouro de limite
   desabilita o pacote responsável, com ID e motivo no diagnóstico.
4. Adaptar `oracles.store/load` à gravação recuperável no SD: temporário,
   sincronização, instalação/backup e recuperação. Preservar tipo dos valores,
   limites, resultado da operação e confinamento da escrita à pasta do mod.
5. Manter a semântica síncrona da API atual. Uma fila assíncrona de gravações
   exigiria um contrato próprio e não será introduzida implicitamente.

**Entrega/aceite:** exemplos válidos são encontrados; pacotes inválidos têm
resultado explicado; `store/load` funciona após fechar/reabrir, com recuperação
verificada em falhas de escrita. Pasta ausente/vazia conserva o jogo normal.

## Etapa 4 — snapshot, callbacks e overlay

**Trabalho**

1. Extrair/reutilizar a montagem de `ModsFrame` em um adaptador compartilhável,
   sem dependência de SDL2/SDL3. Usar o snapshot da imagem apresentada, mantendo
   WRAM dos bancos corretos, HRAM, sala/grupo/dimensões, OAM, paletas e ROM.
2. Reutilizar as regras existentes para scroll/origem da sala. Em menus internos,
   transições e cenas sem origem válida, retornar os mesmos valores ausentes
   documentados no desktop, em vez de inventar coordenadas.
3. Executar `on_frame` uma vez por frame apresentado em gameplay e
   `on_room_change` na ordem atual. O menu externo e a suspensão não executam
   callbacks de gameplay.
4. Carregar a fonte da ROM; suportar `draw_text`, `draw_rect` e `draw_icon`.
5. Compor a cobertura do overlay na imagem lógica antes de enviá-la à textura
   SDL2. Escala e desenho do menu/status continuam no frontend.
6. Tratar retomada de foco sem acumular callbacks atrasados. Não rodar Lua dentro
   do callback de ciclo de vida do sistema.

**Entrega/aceite:** o mesmo mod de leitura/overlay funciona no desktop e Switch;
sala, câmera, sprites, coordenadas e desenho refletem a imagem apresentada.

## Etapa 5 — alterações de gameplay da API 2

**Trabalho**

1. Conectar `features.treasure_swap` a `mods_treasure_swap` durante a sessão e
   removê-lo no encerramento, preservando ordem de substituição e validação.
2. Aplicar a fila de `mods_take_writes` ao GB vivo no mesmo ponto relativo ao
   desenho usado no desktop. Os callbacks leem o snapshot, não memória em mudança.
3. Exigir `api=2` e `changes_game=yes` para os mesmos recursos de escrita e troca
   de recompensas já permitidos no desktop.
4. No quickload/load de slot, descartar referências/filas antigas e reconstruir
   o snapshot. A VM do mod segue a semântica desktop: seu estado Lua não é
   automaticamente rebobinado junto com GB, e não entra no arquivo de save.
5. Manter o modo de replay de referência com o host externo desligado, mesmo se
   existirem mods no SD. O caminho de replay não deve ler ou escrever seus stores.

**Entrega/aceite:** mod API 2 altera um caso controlado de recompensa/memória;
mod sem a declaração não obtém essas operações; sem mods o replay segue idêntico.

## Etapa 6 — acesso pelo menu do Switch

**Trabalho**

1. Adicionar uma seção/aba **Mods** ao menu já aberto por **L + R**.
2. Exibir nome/ID, estado habilitado/desabilitado e motivo de falha.
3. Permitir selecionar mods e configurações suportadas usando direcional, A e B.
4. Persistir a seleção habilitado/desabilitado em configuração do frontend,
   respeitada antes da execução de `main.lua`.
5. Aplicar alterações na próxima inicialização da sessão, com indicação no menu.
   Evitar descarregar/recarregar código arbitrário no meio de um combate.
6. Reutilizar o status existente para falhas, com detalhes no log do perfil.

**Entrega/aceite:** gerenciamento utilizável com controle, seleção persistente,
sem conflito com L+R, ZL/ZR ou navegação dos slots.

## Etapa 7 — regressões, limites e desempenho

**Trabalho**

1. Rodar a suíte de mods existente e testes específicos do adaptador: descoberta,
   snapshots, ordem de callbacks, troca de sala, overlays, fila de escrita,
   recompensas, falha isolada, stores e limpeza da sessão.
2. Cobrir pasta vazia/ausente, API incompatível, ID inválido, Lua com erro,
   memória/instruções excedidas, erro no relógio, SD sem escrita e store inválido.
3. Testar quicksave/quickload, slots, SRAM, pausa, fechamento e suspensão/retomada.
4. Comparar zero mods, overlay simples, leitor de sala/sprites, mod API 2 e vários
   mods simultâneos. Medir CPU média/mediana/P95/P99, chamadas, Lua heap agregado,
   atrasos de frame, I/O e underruns de áudio.
5. Meta inicial: overhead dentro de 10% para o conjunto de referência, sem perda
   da cadência original de aproximadamente 59,73 FPS. Registrar resultados no
   Switch real; os números do macOS não aprovam o console.
6. Definir o perfil agregado de memória/tempo a partir dessas medições. Os atuais
   32 × 8 MiB e prazos individuais não são tratados como orçamento global.
   Se houver limite específico do Switch, documentar diagnóstico e compatibilidade;
   não omitir callbacks silenciosamente para aparentar manter o FPS.
7. Rodar os replays curto e completo de 289.518 frames sem mods, com comparação
   de checkpoints, e regressões das fases de gameplay anteriores.

**Entrega/aceite:** contratos e referência sem mods passam; exemplos têm resultado
medido; configuração dos limites está documentada; validação real portátil/dock,
áudio e suspensão é registrada separadamente dos testes locais.

## Etapa 8 — CI, documentação e pacote

**Trabalho**

1. Propagar a opção de build aos scripts Switch e workflow experimental; incluir
   feature de mods no manifesto, versão do host/API e fonte correspondente.
2. Executar testes do host/adaptador sem ROM no CI; produzir NRO auditado sem
   interpretar o build como aprovação no aparelho.
3. Empacotar instruções, fontes, licenças, checksums e exemplos de mods API 1/2.
   Exemplos que alteram gameplay ficarão em uma pasta de exemplos para instalação
   explícita, sem ativação automática no jogo.
4. Atualizar `MODDING.md`, `SWITCH.md` e a documentação dos perfis experimentais.
5. Gerar um relatório com artefatos, testes, métricas e pendências. O pacote será
   identificado como build com suporte a mods, sem publicar release automaticamente.

**Entrega/aceite:** o jogador copia uma pasta de mod válida para `DATA/mods/`,
abre a sessão, vê o pacote listado e consegue usá-lo. O mesmo mod funciona nas
plataformas declaradas quando só usa a API documentada. A conclusão completa
exige a confirmação dos testes do novo NRO no Switch real.

## Ordem e pontos de integração

Ordem: **1 → 2 → 3 → 4 → 5 → 6 → 7 → 8**. A configuração do CI pode ser preparada
com as primeiras etapas, mas o pacote final depende das regressões e medições.

Arquivos principais existentes:

- `CMakeLists.txt`, `platform/switch/targets.cmake`, `platform/switch/menu.cmake`;
- `src/platform/switch_main.c`, `switch_menu*.c/.h`, `switch_storage.c/.h`;
- `src/platform/app.c` como referência de integração desktop;
- `src/mods/*`, `src/ui/ui.c/.h`, `src/wide/item.c` e helpers de snapshot/geometria;
- `tools/build_switch*.sh`, `tools/package_switch.py`, `tools/package_lua_phase7.py`;
- workflows, manifestos e testes em `tests/`.

Novos adaptadores e testes devem formar unidades próprias, por exemplo um bridge
de snapshot/composição independente de SDL e um backend de storage para SD.
Evitar duplicar o host Lua ou o parser de manifesto em código exclusivo do Switch.
