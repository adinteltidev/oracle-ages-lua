# Fase 1: preparação da permissão de blocos ligada ao cubo

Data: 29/10/2026. Branch: `port/lua-gameplay`. Commit-base: `73c4fd7`.
Registro histórico: candidato testado antes da integração.

**Atualização 30/10/2026:** as regras foram promovidas para
`gameplay/ages/block_rules.lua` e integradas ao hook real, junto ao assentamento.
Veja [implementação atual](LUA-PHASE-1-IMPLEMENTATION.md). O candidato abaixo
permanece como referência de testes; não é o módulo carregado pelo jogo.

O plano permite preparar contratos e testes enquanto a execução do lote anterior
no Windows e no Switch continua pendente. Esta rodada não amplia o gameplay
ativado e não inicia a fase 2, que trata da chama e das paletas do cubo.

## Regra e contrato

A referência é `pushblock_checkRotatingCubePermitsPushing`, função privada em
`src/game/common/interactions/pushblock.c` (`interactionCode14`, Ages).
Carry definido na saída significa que o bloco não pode ser empurrado.

O candidato está em `gameplay/candidates/ages/block_cube_permission.lua`.
Entradas são inteiros de oito bits fornecidos por C; cada função retorna um
booleano, validado pelo host nos testes.

| Função | Entrada | Significado de true |
| --- | --- | --- |
| `no_cube` | Posição do cubo | Não existe cubo ativo; permitir sem consultar cor ou tile |
| `color_active` | Byte de cor | Bit 7 definido; continuar para a comparação do bloco |
| `tile_matches` | Índice original do tile e cor | `((tile - 0x2c) & 0xff) == (cor & 0x7f)` |
| `allows` | Posição, cor e tile já fornecidos | Resultado completo da tabela de verdade, usado pelos testes |

As três primeiras funções representam os pontos de decisão originais. Uma
integração futura deve preservar essas fronteiras: ler a cor somente após a
posição não zero e ler o tile somente após o teste do bit 7. O índice original
precisa ser capturado antes do SUB em C, para não subtrair duas vezes.
`allows` não autoriza antecipar leituras para reunir três argumentos.

Não há ponteiros de `GB`, efeitos em memória, bibliotecas padrão, estado Lua
durável ou RNG. C continuará responsável por leituras, flags, registradores,
consumo de ciclos, remoção do objeto, movimento, som e assentamento do bloco.

## Isolamento dos testes

`oracles_cube_candidate_fixture` incorpora o candidato exclusivamente nos dois
executáveis de teste. O adaptador em `tests/lua_cube_candidate.c` usa a interface
de substituição de fontes do host existente, com seus limites de memória e
instruções. O nome `changed` dessa interface serve apenas de adaptador no teste;
o módulo de interruptores distribuído não foi alterado.

O candidato não está na lista de módulos de `cmake/LuaGameplay.cmake`, não é
carregado pela VM de produção e nenhum hook passou a chamá-lo. Os hashes das
bibliotecas nativas/macOS, do aplicativo Windows e do NRO Switch existentes
permaneceram iguais após a compilação dos testes e dos alvos macOS.

A comparação com a referência inclui o arquivo C original dentro do teste,
renomeando somente sua entrada pública naquela unidade de compilação. Assim,
o teste chama a função privada real sem modificar sua visibilidade ou o código
de produção. Ele compara a permissão com o carry retornado; **não representa
comparação do estado completo de um jogo já integrado a esta regra Lua**.

## Evidências desta rodada

- macOS Intel: **263.936 verificações** sem ROM, cobrindo os 256 valores de
  posição/cor, todos os 65.536 pares tile/cor e a permissão completa com posição
  ausente, presente e 255. Inclui wrap da subtração, igualdade, bit 7 e cores
  fora das normalmente usadas pelo jogo.
- macOS Intel com a ROM Ages canônica: **18.432 resultados da função C real**
  comparados com o candidato, em três posições, 256 cores, 12 tiles de fronteira
  e dois valores iniciais de flags. Nenhuma divergência.
- Regressão dirigida: `lua_gameplay`, `lua_gameplay_rom` e `lua_blocks_rom`, além
  dos dois novos testes: **cinco testes aprovados**, sem divergências. Os
  resultados dessa rodada ficam no relatório local.
- Windows x64: os dois novos executáveis compilaram por cross-compilation.
  Sem execução no Windows nesta rodada.
- Workflow experimental ampliado para compilar/executar o novo contrato sem
  ROM nos três sistemas. Ainda sem execução remota desta alteração.

Hash SHA256 do candidato:
`3fce0e16ace448d51d21700d640e10211f79f46d8958bcdf30c1d3fd92942604`.
A identidade dos módulos do jogo continua
`6e6f9d18d78f8759ee04f90578c48cd5cc6daf2e4b70f501e0f3259c18aeaf12`.

Para reproduzir, com as duas opções Lua ON:

```sh
cmake --build build-macos-lua --target test_lua_cube_candidate test_lua_cube_candidate_rom
ctest --test-dir build-macos-lua --output-on-failure -R '^lua_cube_candidate(_rom)?$'
```

O segundo teste requer a ROM local e retorna 77 quando ela falta. O primeiro
não depende da ROM e integra o CI. Evidências locais ignoradas pelo Git:
`reports/lua-phase1-cube-candidate/`.

## Próximo passo

Executar os [pacotes anteriores no Windows e no Switch](LUA-PHASE-1-PLATFORM-TESTS.md).
Depois de resolver essa condição, integrar as decisões nos pontos originais e
comparar estado completo, vídeo/áudio, ciclos, continuação de saves e desempenho.
O candidato não conta como gameplay migrado enquanto essa integração estiver
pendente. As demais regras do inventário e a fase 1 completa continuam abertas.
