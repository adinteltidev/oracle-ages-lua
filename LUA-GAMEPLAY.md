# Piloto C + Lua de Ages

Branch: `port/lua-gameplay`. Implementação experimental; desativada por padrão.

Expansão iniciada: [fase 1 — puzzles com blocos](LUA-PHASE-1.md).
A integração de `interactionCode13` é optativa, com
`ORACLES_LUA_GAMEPLAY=ON` e `ORACLES_LUA_BLOCK_PUZZLES=ON`; a segunda opção
continua OFF por padrão, aguardando a validação da fase. O host agora incorpora
dois módulos em ambientes separados e aceita também transições inteiras validadas.
As evidências do piloto original abaixo não comprovam a migração dos novos puzzles.

## O que foi migrado

`gameplay/ages/switch_tiles.lua` decide se o estado do interruptor mudou e qual
alternativa de tile deve ser selecionada. A interação real `interactionCode78`
consulta essas funções nos pontos correspondentes de execução, incluindo quando
é chamada diretamente por outra rotina C. As decisões de gameplay são executadas
em Lua; temporização, flags, movimentação de memória e atualização do tile ainda
são executadas pelo wrapper C. Não é uma conversão da interação inteira.

Ages usa Lua no build experimental. Seasons continua no caminho C. Não foi
alterado o layout de GB nem o formato dos saves. O script é puro, sem estado de
jogo escondido em globals, closures ou corrotinas; contadores são apenas diagnóstico.

## Build

```sh
cmake -S . -B build-lua -DORACLES_SDL=OFF -DORACLES_LUA_GAMEPLAY=ON
cmake --build build-lua --target test_lua_gameplay test_lua_gameplay_rom test_native_ages
ctest --test-dir build-lua -R '^(lua_gameplay|lua_gameplay_rom|native_ages)$' --output-on-failure
```

Com `ORACLES_SDL=ON`, o aplicativo desktop usa o mesmo caminho experimental.
Nos scripts `build_windows_llvm.sh`, `build_macos_intel.sh` e
`build_switch_llvm.sh`, exportar `ORACLES_LUA_GAMEPLAY=ON`. O padrão desses
scripts continua OFF. Na configuração CMake direta do SDK, usar
`-DORACLES_LUA_GAMEPLAY=ON`. Os scripts são incorporados ao binário: não é
necessário instalar Lua nem copiar `main.lua` para o SD. O piloto Switch usa a
pasta separada `/switch/oracle-ages-lua/`: colocar a ROM em `ages.gbc` nessa
pasta. Não compartilha nem sobrescreve os saves de `oracle-ages-native`.
No desktop, usar `--cache /caminho/para/cache-lua-piloto` para isolar os dados.

Voltar ao C: configurar outra pasta com `-DORACLES_LUA_GAMEPLAY=OFF`. Para
comparação nos testes, `gameplay_enable(false)` seleciona o controle C no mesmo
binário; não é um interruptor de gameplay exposto ao jogador.

## Runtime

Lua 5.4.9 já vendorizado é compilado em uma biblioteca compartilhada pelos dois
hosts: o de mods existente e o novo host de gameplay. As APIs dos mods não foram
substituídas. O host experimental não abre bibliotecas padrão nem oferece acesso
a arquivos, rede ou funções C que modifiquem o jogo. Aceita retornos booleanos
nas decisões de interruptores e transições inteiras nas de blocos.
Tem teto de heap de 256 KiB e limite aproximado de 10.000 instruções
por chamada; esses são limites do piloto, não uma promessa de orçamento para um
port completo. Falhas param a execução do jogo com diagnóstico, sem refazer a
rotina em C depois de efeitos já aplicados. A VM é única e usada pelo thread do
jogo; não chamar a API concorrentemente.

## Evidências e limites

- `test_lua_gameplay`: todos os 65.536 pares de bytes em cada decisão, limite de
  execução, validação do retorno e seleção do controle C.
- `test_lua_gameplay_rom`: exercita a interação real em 512 combinações com ROM
  local; compara a estrutura GB inteira, incluindo registradores, relógios,
  memória, framebuffer e APU, e exige cobertura positiva das duas funções Lua.
- `test_native_ages`: compara os checkpoints do replay e inclui reloads de estado;
  imprime contadores de cobertura. Se o replay não alcançar a interação, isso
  deve aparecer como zero e não é prova de cobertura do trecho migrado.
- O workflow experimental testa o runtime sem ROM em Linux, macOS e Windows.
  Os testes com ROM são locais, não recebem a ROM pelo GitHub.

O desempenho do teste das decisões não representa o tempo total de um frame.
Medições comparativas de gameplay, replay completo, execução nativa no Windows e
validação real do NRO no Switch continuam sendo critérios antes de ampliar a
migração. Não liberar este experimento como substituição de uma versão estável
sem concluir essas verificações.

## Resultado desta rodada (28/10/2026)

- 131.072 decisões comparadas com o controle C: aprovadas, incluindo testes de
  execução limitada e retorno inválido. Heap medido no teste inicial: 6.701 bytes.
- 512 casos da interação real: estado GB e amostras iguais; cobertura Lua 512/240.
  O fixture foi corrigido para selecionar o banco ROM da interação antes da chamada.
- Replay Ages: 333 checkpoints em 20.000 frames, execução normal e com reloads,
  aprovados. Cobertura da rotina nesse replay: zero; a cobertura vem dos casos dirigidos.
- Regressão de quatro slots/continuação determinística: aprovada.
- Teste de mods completo: aprovado em reexecução isolada (261 segundos), após
  timeout de 400 segundos na execução concorrente às compilações. Os seis
  testes dirigidos de troca de tesouros também passaram.
- Runtime compilado para Windows x64; NRO ARM64 gerado. Ainda sem execução desta
  build no Windows real ou no console, sem benchmark completo de frames e sem
  replay completo de 289.518 frames. A migração não deve avançar de escala ainda.
