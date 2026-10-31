# Integração do carregador de mods no Switch

31/10/2026. Branch `feat/switch-mod-loader`, base `9e1c1ab` mais as alterações
locais desta entrega. Implementação local pronta; aceite no aparelho pendente.
Não houve commit, push, publicação de release ou execução remota do Actions.

## Entregas do plano

| Etapa | Resultado |
| --- | --- |
| 1 | SDK inspecionado, build ON/OFF auditado e referências locais registradas. Referência de áudio/foco no console pendente. |
| 2 | Host compartilhado, fonte/ícones mínimos, bridge sem SDL; desktop recompilado. Motor e TAS sem host externo. |
| 3 | DATA/mods, filtro Ages, isolamento, stores síncronos com temporário/sync/backup/recuperação. |
| 4 | Snapshot apresentado, bancos 0/1/3, HRAM, scroll/origem, paletas/OAM e composição lógica 160×144. |
| 5 | API 2, recompensas/escritas, reset de transientes no load e replay sem host. |
| 6 | L+R → Settings → Mods, seleção/configuração persistida antes do Lua; aplicação na próxima sessão. |
| 7 | Regressões e medições locais feitas; meta de desempenho, portátil/dock, áudio e suspensão no Switch ainda pendentes. |
| 8 | Scripts/workflow/manifestos, documentação e empacotamento com exemplos opcionais. Actions validado localmente; execução remota depende do push. |

## Evidência local

- Regressões anteriores de gameplay Lua: 22/22 testes passaram, incluindo fixtures de Ages.
- Gate final do host/frontend: 8/8 testes passaram, sem skips, em `final-tests.xml`.
  O adaptador reúne 12 casos dirigidos, incluindo políticas antes de main.lua,
  registro dinâmico de opções, falha isolada, limite agregado e config inválida.
- GUI SDL2: 720p/1080p, lista longa, ID de 63 caracteres, erro visível e rolagem
  do item selecionado com seus detalhes. Capturas inspecionadas visualmente.
- Fonte/texto/ícones: teste novo com a ROM de Ages passou. Dois testes antigos
  específicos de Seasons (`mods_rom`, `treasure_swap`) foram pulados por falta
  dessa ROM; não contam como aprovados. As asserções de API 2/recompensas também
  são cobertas pelo host e adaptador de Ages.
- Replay normal: 289.518 frames, 4.825 checkpoints, zero divergências/skips.
- Replay com saves: 289.518 frames, 4.825 checkpoints, 285 recargas, zero skips.
- Replay curto: 20.000 frames em cada passagem, 333 checkpoints; 19 recargas.
- Ferramentas de CI: 7 testes Python passaram; actionlint e lint de gameplay passaram.
- NRO ON/OFF auditados, sem ROM embutida ou interpretador de instruções Game Boy.
  Os marcadores de host não aparecem no NRO OFF; o runner headless não contém
  símbolos de inicialização/frame/store do host externo.
- Builds de regressão macOS Intel e Windows x64 passaram. O PE foi auditado.
  Isso não constitui execução deste build em Windows nem teste do NRO no console.
- O timeout global da suíte de mods passou de 400 para 600 s após uma execução
  atingir 400 s durante builds concorrentes. Os limites de produção e os alarmes
  individuais dos testes não mudaram; a repetição final passou.

Evidências detalhadas locais: `reports/switch-mods/`. Builds, ROMs, estados,
relatórios e capturas ficam fora do Git.

## Compatibilidade do SDK e limites

O SDK fixado oferece dirent/stat/lstat/fdopen/fsync e O_NOFOLLOW. A inspeção de
`fsdev_devoptab` confirmou que stat e lstat apontam para fsdev_stat no SD sem
symlinks. O relógio libnx implementa CLOCK_MONOTONIC (ID 4) com o contador ARM;
a falha de relógio é recusada antes de executar Lua. O NRO ligou essas funções.
Isso verifica cabeçalhos/implementações/link, sem simular a execução do aparelho.

Mantidos 32 pacotes e 8 MiB por VM, carga 2 s/50 milhões de instruções e callback
4 ms/1 milhão. O teto agregado de 16 MiB é provisório. Não há garantia global de
tempo para 32 callbacks: as medições e a calibração no console continuam pendentes.

## Benchmark no computador

Duas rodadas em ordem alternada, 2.400 frames medidos mais 120 de aquecimento
por cenário, usando estado privado da sala. CPU por CLOCK_PROCESS_CPUTIME_ID;
sem áudio e sem pacing. O JSON preserva média/mediana/P95/P99 por rodada, tempos
do adaptador, chamadas, heap, I/O e os marcadores de ambiente.

| Cenário | CPU média (ms) | Diferença entre execuções | Pico Lua (bytes) |
| --- | ---: | ---: | ---: |
| empty | 3.1725 | +0.00% | 0 |
| overlay | 3.7163 | +17.14% | 38,403 |
| reader | 3.6448 | +14.89% | 39,856 |
| api2 | 3.5559 | +12.08% | 35,319 |
| multiple | 3.0854 | -2.75% | 112,304 |

Houve variação relevante entre rodadas com cargas concorrentes e mudanças de
frequência/temperatura. Uma rodada preliminar apresentou ~4,4% para o conjunto;
o ensaio final acima também tem resultados negativos e variações maiores que
10%. Esses deltas não isolam o custo do host e **não aprovam a meta de 10%**.
Os dados brutos foram mantidos, sem escolher apenas as rodadas favoráveis.
A aceitação depende de medições no Switch em condições controladas.

## Identificação do artefato

NRO: 34,119,728 bytes.
SHA256: `8ba94a2a2df5a3a7eb07c66a3b0c91443276ac9b99730d6a0aa9989157a649f5`.
Perfil: `sdmc:/switch/oracle-ages-lua-phase7/`.
Módulos internos: `2af4a10e44dbe5f2a045f3a57368a07b81d7024885efab329407f1aded49d6b8`.
O pacote final inclui manifesto desta compilação, fontes, licenças, checksums e
exemplos fora da pasta de mods instalada. A ROM não acompanha a entrega.

## Aceite pendente no Switch

1. Testar este NRO com a pasta mods vazia e salvar o relatório de referência.
2. Instalar overlay-info, room-reader, api2-demo e o conjunto, em sessões novas.
   Copiar mods-performance.json de cada sessão; o relatório é substituído ao sair.
3. Conferir L+R, A/B/direcional, persistência das escolhas antes de main.lua,
   ZL/ZR, quatro slots e SRAM; usar save descartável com o demo API 2.
4. Testar portátil e dock, áudio, fechamento e suspensão/retomada. Conferir que
   o menu externo não avança callbacks e que o replay X/Y não toca stores.
5. Medir a meta de overhead de 10%, cadência ~59,73 FPS e heap; ajustar o perfil
   agregado se necessário. Registrar problemas com runtime.log e métricas.

A confirmação de Windows/Switch da fase 6 não se estende a este novo NRO.
