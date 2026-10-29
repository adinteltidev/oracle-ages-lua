# Fase 1 — validação de cobertura e desempenho

Data: 29/10/2026. Branch: `port/lua-gameplay`; base `b04e0d2`.
Estado: validação local do primeiro lote concluída; fase 1 ainda em andamento.
Esta rodada não migra novas regras.

## O que foi acrescentado

- O replay informa o avanço a cada 25.000 chamadas de frame, checkpoints
  comparados/pulados, número de recargas e cobertura acumulada dos blocos.
  `TAS_RUN=normal|reload|both` permite repetir apenas o trecho necessário.
  `TAS_GAMEPLAY=c` seleciona o controle C dentro do binário híbrido para diagnóstico;
  `TAS_GAMEPLAY=lua` mantém as decisões Lua habilitadas. O padrão é Lua quando
  esse suporte foi compilado. Essa seleção não é usada como referência de tempo.
- O host Lua expõe heap atual, pico e quantidade de alocações/reallocações
  bem-sucedidas. São contadores de diagnóstico; não alteram o estado salvo.
- `benchmark-gameplay`, em `tools/benchmark_gameplay.c`, mede frames completos
  do motor sem SDL, a partir de uma sala alcançada por entradas TAS naturais.
- `tools/benchmark_gameplay.py` compara dois builds Release com configurações
  equivalentes: C puro, sem runtime Lua, e C + Lua com blocos ativados. Registra
  configuração, hashes dos binários/fontes e dados do computador.

## Cenários e método

O preparador usa as entradas TAS até o primeiro estado jogável conhecido na sala
`00:8a`, solta os controles e espera um ponto seguro das fibras. O estado privado
é criado uma vez e usado pelos dois executáveis. O runner o remove ao terminar.
Nenhum save do jogador é lido ou sobrescrito.

| Cenário | Objetivo |
| --- | --- |
| `idle` | Custo do motor na sala, sem injetar objetos; não comprova execução Lua |
| `blocks` | Quatro interações `13` injetadas em slots livres, percorrendo inicialização, liberação, movimento e atraso no despacho normal do motor |
| `blocks-reload` | Mesmo cenário, com recargas em pontos seguros e comprovação de save durante o estado de atraso |

Os blocos são uma **cena controlada com alterações de RAM para teste**. A sala foi
alcançada naturalmente, mas os puzzles não foram descobertos nem resolvidos por
entradas naturais nesse cenário. As edições apontam explicitamente para o banco
WRAM das interações, sem depender do banco selecionado no instante da captura.

Na verificação, os executáveis produzem hashes por frame do estado GB completo
(com registradores, memória, ciclos e APU) e das 16 amostras. Endereços de processo,
ponteiros de callbacks e referências a fibras/recursos são normalizados; a pilha
nativa de uma fibra não é serializada nem comparada. Exigir hashes iguais e
cobertura positiva dos quatro estados no build Lua. O cenário de recarga precisa
salvar pelo menos uma vez com blocos ativos no estado 3.

As medições são **execuções separadas**, sem hashes por frame, saves ou comparação
de estados no trecho cronometrado. O estímulo artificial é aplicado antes do
cronômetro: mede-se o trabalho do motor por frame, não o driver do teste. Aquecer
por 160 frames e medir 2.000 frames por execução, em três repetições por modo e
cenário. Alternar a ordem C/Lua entre repetições para reduzir viés térmico/de ordem.

Registrar CPU da thread e tempo de parede: média, mediana, P95, P99 e máximo.
Comparar a mediana das médias de CPU das três execuções. O relatório marca se o
acréscimo observado fica dentro da meta proposta de 10%; isso não aprova sozinho
uma versão para distribuição. A medição deve ocorrer sem compilação ou replay
concorrentes. O sistema operacional e outros aplicativos ainda podem causar ruído.

O pico de heap inclui a inicialização da VM; as alocações medidas são a diferença
entre o fim do aquecimento e o fim da execução. Não há medição separada de pausas
internas do GC nem de memória total do processo. Os contadores não são um perfil
completo do coletor.

## Como reproduzir no macOS Intel

Na raiz do código, com CMake/Ninja disponíveis:

```sh
cmake -S . -B build-validation-c -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DORACLES_SDL=OFF -DORACLES_LUA_GAMEPLAY=OFF -DORACLES_LUA_BLOCK_PUZZLES=OFF
cmake -S . -B build-validation-lua -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DORACLES_SDL=OFF -DORACLES_LUA_GAMEPLAY=ON -DORACLES_LUA_BLOCK_PUZZLES=ON
cmake --build build-validation-c --target benchmark-gameplay --parallel 2
cmake --build build-validation-lua --target benchmark-gameplay test_native_ages --parallel 2

# Terminar o replay antes de medir desempenho.
TAS_FRAMES=289518 TAS_GAMEPLAY=lua build-validation-lua/test_native_ages
python3 tools/benchmark_gameplay.py \
  build-validation-c/benchmark-gameplay build-validation-lua/benchmark-gameplay \
  --output reports/lua-phase-1-validation
```

Para verificar apenas equivalência/cobertura, acrescentar `--verify-only` ao runner.
A ROM local deve continuar na pasta `roms/` já usada pelos testes. O script não
baixa ROMs nem envia os dados privados ao CI.

## Correção do teste de recarga

Na primeira execução desta rodada, o replay normal terminou com 4.825 checkpoints
coincidentes e cobertura dos blocos `3 / 77 / 489 / 60`. O teste de recarga
registrou `reference=194880 sample=194881`: um checkpoint foi deixado para trás.

A carga de estado zera a fila de amostras do host. O teste podia salvar enquanto
uma amostra dessa fila ainda aguardava comparação; o load descartava essa amostra.
O teste agora exige `sample_count == 0` e fibras estacionadas antes de salvar e
recarregar. Ele também reprova Ages se algum checkpoint for pulado, e informa o
intervalo efetivo de amostras e a quantidade de recargas. O benchmark aplica o
mesmo requisito nas verificações com saves. O formato de save e o carregamento
usados pelo aplicativo não foram modificados.

A execução antiga com recargas foi interrompida após o diagnóstico. A parte
normal já concluída foi preservada; a parte com recargas foi reiniciada do boot
com o teste corrigido. Os logs distinguem essas execuções.

## Resultados desta rodada

Resultados finais no macOS Intel (Intel Core i5-5350U, 1,80 GHz):

- Replay normal: **4.825 checkpoints coincidentes**, sem checkpoints pulados.
- Replay com o teste corrigido: **4.825 checkpoints coincidentes, 285 recargas e zero pulados**.
- Cada replay realizou 289.518 chamadas de `gb_run_frame`. A execução com recargas
  produziu 289.759 amostras, do índice 185 ao 289.943; uma chamada pode produzir
  amostras adicionais por callbacks durante operações longas do motor.
- Ambos os percursos alcançaram as quatro regras/estados dos blocos:
  **3 / 77 / 489 / 60 chamadas Lua**. As duas decisões antigas dos interruptores
  tiveram cobertura zero nesse percurso; não são consideradas cobertas por ele.
- Três testes dirigidos aprovados: `lua_gameplay`, `lua_gameplay_rom` e `lua_blocks_rom`.
- Verificação final separada: **320 hashes por cenário** iguais entre C puro e Lua
  em `idle`, `blocks` e `blocks-reload`. Cobertura Lua dos blocos: **16 / 48 / 48 / 480**
  em cada cenário com blocos. O cenário de recarga fez **24 loads**, incluindo
  saves com quatro blocos no estado de atraso.
- O objeto da ferramenta de benchmark compilou para Windows x64. Ainda sem
  execução no Windows. Lint do jogo: zero problemas.

Medição: três execuções de 2.000 frames por modo/cenário, após aquecimento de
160 frames, sem replay ou compilação concorrentes. Os valores C e Lua abaixo
são a mediana das médias de CPU por execução. O P99 é o maior entre as três
execuções Lua.

| Cena | C (ms/frame) | Lua (ms/frame) | Diferença observada | P99 Lua (ms) |
| --- | --- | --- | --- | --- |
| Sala sem blocos | 1.8861 | 1.9722 | +4.56% | 3.9324 |
| Quatro blocos | 1.7247 | 1.7508 | +1.51% | 3.7579 |

As duas comparações ficaram dentro da meta proposta de 10% neste computador.
Há variação relevante entre repetições, inclusive na sala sem chamadas Lua:
essas diferenças não isolam com precisão o custo do interpretador nem comprovam
uma taxa de frames do aplicativo completo. Os valores brutos de CPU e parede,
mediana/P95/P99/máximo de cada execução estão no JSON.

Na medição com blocos, cada execução Lua cobriu **100 / 300 / 300 / 3.000**
chamadas. Heap final da VM: **8.187 bytes**; pico: **8.305 bytes**, incluindo
inicialização; **zero alocações/reallocações novas** no trecho medido. Isso não
é uma medição individual de pausas do GC.

Evidências locais: `reports/lua-phase-1-next/evidence.json` agrega os hashes dos
dados de entrada, testes, replays e benchmark. O JSON de medição está em
`reports/lua-phase-1-next/measurement/summary.json`; logs e traços ficam ao lado.
Esses arquivos locais são ignorados pelo Git. Este resumo é versionável.

## Limites e sequência

Essa medição cobre CPU do motor sem apresentação SDL nem saída de áudio no
hardware. Não mede a experiência completa do aplicativo, suspensão, SD, dock ou
modo portátil. Resultados do macOS Intel não substituem testes no Windows e no
Switch reais. Troca de sala com esses puzzles, novas famílias do inventário e
regressão de Seasons continuam pendentes. A fase 1 permanece em andamento e a
opção `ORACLES_LUA_BLOCK_PUZZLES` continua OFF por padrão.

Alterações registradas em commit local a pedido do usuário. Não houve push ou
publicação nesta rodada.
