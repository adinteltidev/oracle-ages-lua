# Mods externos no Nintendo Switch (experimental)

Host versão 1, API Lua 1/2. Base C + Lua fase 7. Build com
`ORACLES_SWITCH_MODS=ON`; a opção continua OFF por padrão.

## Instalar e abrir

1. Extraia a pasta `switch/` do ZIP na raiz do SD. Faça backup dos seus saves.
2. Mantenha sua própria `ages.gbc` em
   `sdmc:/switch/oracle-ages-lua-phase7/`. A ROM não acompanha o pacote.
3. Copie uma pasta de mod com `mod.ini` e `main.lua` para:

   ```text
   sdmc:/switch/oracle-ages-lua-phase7/mods/meu-mod/mod.ini
   sdmc:/switch/oracle-ages-lua-phase7/mods/meu-mod/main.lua
   ```

4. Abra o NRO e escolha **A: Play**. Mods novos são habilitados por padrão.
5. Durante o jogo, pressione **L + R → Settings → Mods**.
   Direcional seleciona; **A** alterna o pacote; **Esquerda/Direita** ajustam
   opções; **B** volta. Nome, ID, estado atual, próxima seleção e erro aparecem
   na lista. **Minus** volta à aba Ages.
6. A seleção e as opções são gravadas em `mods.cfg`. Para aplicar, saia da sessão
   feche o NRO, abra-o novamente e escolha Play. Não há recarga de código no meio da partida.

Os exemplos de overlay/leitura aceitam ambos os jogos. Os exemplos ficam em `Examples/switch-mods/`, fora da pasta instalada no SD.
Comece por `overlay-info`. `api2-demo` altera uma recompensa: instale-o apenas
se desejar esse teste e use uma cópia descartável do save. A API completa e
seus limites estão em [MODDING.md](MODDING.md).

O caminho é derivado do perfil DATA: os outros perfis usam sua própria pasta.
Esta implementação Switch executa Ages e Seasons e respeita o campo `games`.
Seasons usa `DATA/seasons/mods/`, ou `DATA/seasons-eu/mods/` para a edição
europeia. Seus stores e configurações ficam separados de Ages; veja
[SWITCH-SEASONS.md](SWITCH-SEASONS.md). Os onze módulos
internos de gameplay continuam incorporados e separados das VMs dos mods.

## Saves, falhas e persistência

- ZL salva e ZR carrega; os quatro slots e SRAM continuam com o mesmo formato.
- Carregar GB limpa snapshot/overlay/fila de escritas. O estado Lua de cada mod
  continua vivo, sem rebobinar com o save, como no desktop. Ele não entra no save.
- `oracles.store` continua síncrono e retorna nada; falhas geram erro Lua.
  Cada store usa temporário, sincronização, backup `.bak` e recuperação.
  Isso reduz a perda de dados em falhas; não garante atomicidade após perda de energia.
- Configuração de seleção inválida impede a execução de todos os mods; corrija
  `mods.cfg` antes de abrir novamente. O motivo fica em `runtime.log`.
- Erros desabilitam somente o mod responsável; consulte a lista e `runtime.log`.
- Os modos **X/Y replay** mantêm o host externo desligado, sem acessar stores.
- Menu externo e perda de foco não executam callbacks de gameplay.
- Opções persistidas são restauradas no registro, antes de o getter voltar ao Lua,
  inclusive no corpo inicial e em opções registradas dinamicamente em callbacks.

## Limites e métricas

Mantidos: 32 pacotes, 8 MiB por VM, carga de 50 milhões de instruções/2 s,
callback de 1 milhão/4 ms. O perfil Switch acrescenta um teto agregado
**provisório de 16 MiB** de heap Lua. Um erro de relógio impede a execução Lua.
O teto agregado pode tornar um conjunto de mods desktop incompatível; falha de
alocação fica visível no mod responsável. Nenhum callback é omitido para simular FPS.

Ao sair da sessão, `mods-performance.json` registra frames, chamadas, heap máximo,
latência média/mediana/P95/P99 do trabalho de frame e do adaptador, frames atrasados,
underruns e I/O de escrita. Os tempos são intervalos de trabalho medidos por relógio
monotônico, não contadores de CPU do processador. O histograma tem resolução de
0,01 ms e overflow a partir de 20 ms. O relatório exclui sua própria gravação de I/O.
Os prazos são verificados em pontos de execução Lua e não cancelam uma operação
bloqueada de SD. Não grave por frame em mods; SD é síncrono e pode causar atraso audível.

A meta de overhead de 10%, cadência ~59,73 FPS, portátil/dock, áudio e retomada
**ainda exigem medição deste NRO no Switch real**. Builds e testes macOS não
aprovam essa etapa. O limite agregado será calibrado com essas medições.

## Compilar e CI

Use o SDK fixado em `platform/switch/sdk-lock.json` e os mesmos comandos de
[SWITCH.md](SWITCH.md), acrescentando `ORACLES_SWITCH_MODS=ON` ao ambiente:

```sh
ORACLES_SWITCH_MODS=ON sh tools/build_switch_llvm.sh
# ou, com devkitA64 no host:
ORACLES_SWITCH_MODS=ON sh tools/build_switch.sh
```

Mantenha também as oito opções `ORACLES_LUA_*` ON para usar o perfil fase 7.
No GitHub Actions, execute **Experimental Lua gameplay**, marcando **switch-mods**.
O workflow produz artefatos de teste, sem criar release automaticamente. O manifesto
identifica host/API, feature e checksum do NRO. O pacote inclui fontes e licenças.

Testes sem ROM: `mods`, `mods_clock_failure`, `switch_mods`, `switch_storage`.
O último teste do adaptador é POSIX e roda nos jobs Linux/macOS. O replay e o
motor headless não ligam o host externo. A ferramenta `benchmark-switch-mods`
permite comparar os mesmos pacotes localmente; o relatório é apenas referência
para a validação no console.

Para repetir o benchmark local em um diretório novo:

```sh
cmake --build build-macos-lua --target benchmark-switch-mods
python3 tools/benchmark_switch_mods.py \
  --runner build-macos-lua/benchmark-switch-mods \
  --rom "roms/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc" \
  --state reports/lua-phase6/bench.state \
  --output reports/switch-mods/novo-benchmark
```

Use um estado privado de sala normal, criado pela opção `--prepare` de
`benchmark-gameplay`, caso não tenha o fixture local da fase 6. As duas rodadas
comparam zero mods, overlay, leitor, API 2 e os três juntos. A ferramenta roda
sem áudio e sem limitar FPS; os zeros de underrun/atraso desse ensaio não são
medições do aparelho. Os percentis de CPU usam `CLOCK_PROCESS_CPUTIME_ID`.
