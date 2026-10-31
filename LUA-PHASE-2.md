# Fase 2 — chama do cubo colorido

30/10/2026. Branch `port/lua-gameplay`, commit-base `5829dad`, alterações locais.

**Implementação experimental concluída; aceitação das plataformas aberta.**
O avanço foi solicitado pelo jogador. As pendências de execução real da fase 1
continuam registradas e não são consideradas aprovadas por esta fase. A nova
integração tem opção própria OFF por padrão e perfil separado. Fases 3–7 não
foram iniciadas. Não houve commit, push nem publicação nesta rodada.

## Inventário e contrato

O escopo desta fase é `interactionCode1a` e seu helper `updateColor`, em
`src/game/ages/interactions/coloredCubeFlame.c`. A orientação, física e movimento
em `coloredCube.c` continuam em C, como previsto no plano.

A documentação da desmontagem local (`include/wram.s`) identifica cores 0 =
vermelho, 1 = amarelo, 2 = azul; o bit 7 é ligado quando as tochas acendem.
`dungeonEvents.s` confirma a escrita desse bit. A tabela da ROM USA/Australia
em banco 08, endereço 4a6c, contém índices de paleta **2, 3, 1**.

| Regra em `gameplay/ages/colored_cube.lua` | Entrada | Saída | Ponto original |
| --- | --- | --- | --- |
| `present` | posição, byte completo | booleano: posição diferente de zero | antes do retorno que impede inicializar sem cubo |
| `visible` | cor, byte completo | booleano: bit 7 ligado | após RLCA original, antes de escolher visível/invisível |
| `palette_offset` | cor, byte completo | inteiro 0–127: cor AND 7f | após AND original, antes de calcular o endereço da tabela |

O host define três funções tipadas em `src/script/colored_cube.h`; valida tipo,
faixa e quantidade de retornos antes de aplicar a decisão. C mantém ALU, flags,
ciclos, leitura da tabela, escrita do OAM, inicialização gráfica e animação nos
pontos originais. Não há leitura antecipada de recursos nem tabela de paletas
copiada para Lua. O teste de autoridade usa respostas válidas diferentes para
demonstrar que o hook realmente segue Lua.

O índice original é mascarado para **todos os 256 bytes**, sem limitar a cor a
0–2. Para as 250 combinações fora das cores normais, a referência nativa pode
atingir dados vizinhos ou sua proteção de leitura de código: **224 terminam com
exit 5**, e 26 concluem. O teste em subprocessos confirma igualdade exata de
saída e diagnóstico C/Lua nesses casos. Isso não os transforma em cores válidas.

O OAM continua calculado como `(flags & f8) | byte_da_ROM`, preservando o
comportamento do C inclusive se um byte vizinho tiver bits superiores ligados.
Nenhum novo campo foi adicionado ao GB, aos objetos ou aos saves. Lua não guarda
progresso, não recebe ponteiros e não altera RNG. O sandbox mantém os limites de
memória/instruções e os ambientes isolados das fases anteriores.

## Ativação

```sh
cmake -S . -B build-lua-phase2 -DORACLES_SDL=OFF \
  -DORACLES_LUA_GAMEPLAY=ON -DORACLES_LUA_BLOCK_PUZZLES=ON \
  -DORACLES_LUA_COLORED_CUBE=ON
cmake --build build-lua-phase2 --target test_lua_colored_cube test_lua_colored_cube_rom
ctest --test-dir build-lua-phase2 --output-on-failure -R '^lua_colored_cube'
```

`ORACLES_LUA_COLORED_CUBE` exige `ORACLES_LUA_GAMEPLAY=ON`; pode ser testada
independentemente dos blocos. Os pacotes desta rodada habilitam as três opções.
A opção nova é compilada no runtime e consultada apenas pelo hook da chama;
os builds C e Seasons não chamam as decisões. O Switch recebe explicitamente a
definição Lua nesse arquivo. Todos os scripts de build aceitam a nova opção.

Os quatro módulos incorporados são `switch_tiles`, `block_puzzles`, `block_rules`
e `colored_cube`. SHA256 conjunto:
`9281cb160e0ea7bc404f3b775cf0d35341af9bab17c6e96531d3e38036aecef8`.
Logs desktop, headless e replay do Switch mostram os três contadores novos.

## Testes locais

- **768 casos de contrato:** todos os bytes de posição/cor para as três regras;
  contadores positivos, saídas booleanas e índice inteiro no domínio original.
- Retornos inválidos, floats, nil, múltiplos retornos, ausência de retorno,
  limite de instruções, limite de memória, VM inutilizada e isolamento de
  ambientes: testados. Saídas do chamador permanecem intactas em falhas.
- **2.180 casos dirigidos:** os seis bytes de cor normais, todos os 256 valores
  de OAM, cubo ausente, chamada direta/dispatcher, chama apagada, inicialização,
  trocas de cor, recriação do objeto para simular reentrada e save/load dos
  estados não inicializado/inicializado com VM recriada.
- C real e Lua real comparados por GB completo e 16 amostras nos casos diretos.
  Cenas com gráficos rodam no scheduler real e comparam GB e amostra completa
  em cada frame. O cache externo do renderizador é restaurado entre os modos.
- **250 cores inesperadas em 500 subprocessos:** a proteção original é exercida
  sem matar a suíte; exit/stdout/stderr idênticos entre os dois caminhos.
- Traces de **160 frames** com estímulo de chama: build C, caminho C com runtime
  desativado e Lua iguais, incluindo vídeo/áudio e estado normalizado. Cobertura
  Lua no trecho: presença 2, visibilidade 160, paleta 92.
- O teste de guarda de Seasons comprova ausência de consultas às regras; não
  substitui regressão completa com ROM própria de Seasons.

Os casos de falha do hook imprimem diagnósticos intencionais e verificam que não
há escrita de OAM nem avanço do estado após um retorno inválido. O teste de
domínio inesperado copia o executável antes de iniciar os subprocessos, evitando
interferência de um rebuild concorrente.

`tests/test_lua_colored_cube_rom.c` aceita `--rom ARQUIVO --tas-dir PASTA` e
prepara a cena pelos fixtures públicos TAS. A ROM própria fica fora do Git.
O modo `--invalid-color BYTE --mode c|lua` é usado pelo teste de subprocessos.
ROM ausente retorna 77 e conta como teste não executado.

## Desempenho e distribuição

O benchmark existente recebeu `--scenario colored_cube`: acende/apaga e muda
cores, reinicializa as chamas periodicamente e conta chamadas reais. `--lua off`
permite referência C no mesmo binário. Traces e medições são modos separados;
o trecho cronometrado não calcula hashes nem compara estados.

```sh
build-macos-lua/benchmark-gameplay --prepare /caminho/privado/scene.state
build-macos-lua/benchmark-gameplay --state /caminho/privado/scene.state \
  --scenario colored_cube --blocks 4 --frames 1600 --warmup 160 \
  --lua on --output /caminho/privado/lua.json
```

O workflow experimental executa o contrato sem ROM em Linux/macOS/Windows e
inclui a nova opção nos artefatos multiplataforma de execução manual. O smoke do
Windows inclui o executável do contrato. CI/release padrão continuam com Lua
OFF; nenhuma execução remota foi realizada nesta rodada.

O [guia de pacotes](LUA-PHASE-2-PLATFORM-TESTS.md) descreve os perfis, controles
e validação. Os pacotes carregam fontes correspondentes do working tree,
licenças, identidade dos scripts e checksums; não incluem ROM ou saves pessoais.
O empacotador da fase 1 recusa builds da fase 2, evitando misturar as identidades.

Logs/XML, traces, medições, auditorias e manifesto local ficam em
`reports/lua-phase2/`, ignorado pelo Git. Resultados finais da rodada são
registrados junto aos pacotes em `oracle-lua-phase2-evidence.json`.

## Aceitação pendente

Faltam os testes no Windows e no Switch real, portátil/dock, com áudio,
suspensão/retomada, saves e cenas da chama, além da regressão completa de Seasons.
Compilação cruzada, testes headless e um replay que não visita a chama não
substituem essa aceitação. A fase permanece experimental até registrar esses
resultados, incluindo GC/cadência nos alvos físicos.

## Resultados adicionais desta rodada

As três suítes novas e as 11 regressões anteriores passaram: **14 testes**.
O hook C otimizado, com Lua OFF, tem código de máquina idêntico ao commit
`5829dad`. Lint: zero problemas; seis testes das ferramentas CI aprovados.

O replay normal final conferiu **4.825 checkpoints em 289.518 frames**, sem
pulados, e executou as regras da chama: presença **4**, visibilidade **800**,
paleta **292**. O resultado final do modo com recargas é registrado no manifesto
de evidências desta rodada ao lado dos pacotes.

CPU local Release/x86_64: três pares C/Lua, quatro chamas, 1.600 frames medidos
e 160 de aquecimento por processo. Ordem alternada; replay suspenso durante
as medições, sem compilações nem comparador/hash ativos na região medida.

| Par | Média C/Lua (ms) | Mediana C/Lua (ms) | P95 C/Lua (ms) | P99 C/Lua (ms) | Acréscimo médio |
| --- | --- | --- | --- | --- | --- |
| 1 | 1.848/1.884 | 1.779/1.807 | 2.273/2.303 | 2.470/2.834 | +1.99% |
| 2 | 1.995/1.962 | 1.805/1.803 | 3.614/3.145 | 3.780/3.783 | -1.65% |
| 3 | 1.974/1.885 | 1.817/1.809 | 3.085/2.304 | 3.733/2.761 | -4.49% |

Todos os pares ficaram dentro do orçamento inicial de 10% nestas condições.
Heap/pico Lua: **21.803 bytes**, com **zero alocações no trecho medido**; cobertura
por processo Lua: **80 / 6.400 / 3.320**. Isso não mede GC/cadência do aplicativo
completo em Windows/Switch nem comprova ganho quando uma execução fica abaixo
do C. Os perfis e os tempos absolutos permanecem nas evidências locais.
