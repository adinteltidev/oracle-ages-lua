# Fase 5 — itens e lojas

30/10/2026. Branch `port/lua-gameplay`, base `fb4c18a`, alterações locais.

Implementação experimental de regras de itens, lojas e recompensas em
`gameplay/ages/items.lua` e `shops.lua`. A opção `ORACLES_LUA_ITEMS_SHOPS` é OFF
por padrão e exige `ORACLES_LUA_GAMEPLAY`. A validação manual que o jogador
registrou para Windows/Switch corresponde à fase 4: os pacotes novos da fase 5
precisam de uma nova rodada. A regressão completa de Seasons continua pendente
por falta de ROM própria. Fases 6 e 7 permanecem pendentes.

## Inventário e fronteira

O inventário verificável está em `gameplay/ages/items-shops-inventory.json`.
Abrange 22 índices de produto da loja comum, sete formas exibidas pela loja
Tokay, comparações de progresso/jogo conectado, tesouros coletáveis, drops,
limites e upgrades, compra/cancelamento e a recompensa da chave de chefe de D6
(`miscPuzzles` 00). As formas Tokay 2/3 de pá são transformações de 0/1; não são
índices iniciais da tabela original de dois tesouros.

O catálogo da ROM de Ages possui 99 posições de ID, duas sem recurso de objeto
(13/40 em hexadecimal), 97 IDs válidos e 237 registros contando subids. O teste
versiona somente contagens; as tabelas, textos e gráficos são lidos da ROM do
jogador. Os modos de aplicação 0–15 incluem flags, inventário, contadores,
upgrades, saúde, sementes, rúpias e anéis.

Lua avalia comparações, igualdade a zero, bits, carry, saldo disponível e as
seleções de ID, modo, preço e resultado. O roteamento original dos estados e
scripts continua em C, assim como tabelas da ROM, leituras/ALU/ciclos, débito,
entrega, flags, armazenamento do inventário, RNG, física e apresentação.
As respostas Lua controlam os flags ou valores consumidos pelo ramo seguinte.
Não se trata de traduzir todo o sistema de armas nem todos os eventos NPC.
As sequências de história e escolhas de diálogo pertencem à fase 6; seus
serviços de dinheiro e tesouros já atravessam as decisões desta fase.

As 15 regras possuem contratos tipados em `src/script/items_shops.h`. Booleanos
exigem booleanos; comparações retornam 0/1/2; modo é 0–15; ID de objeto é 0–98
sem as entradas nulas 13/40; preço é um seletor 0–20; resultado de compra é 1
ou 255. O índice de tabela não pode aumentar além da entrada original. Dados
originais fora do domínio de um seletor preservam o caminho original C; uma
resposta Lua inválida interrompe o motor com a regra e o motivo.

## Dinheiro, ordem e retomada

O seletor de preço não é uma quantia em rúpias. Os 21 seletores apontam para a
tabela original. Saldo e limites de sementes usam BCD: são preservados os bytes,
carry/DAA e a ordem da comparação original, inclusive para bytes não canônicos.
Não foi trocada a comparação exata de capacidade por `>=`.

A confirmação apenas escolhe o resultado e o estado do item. O débito e a
entrega ocorrem em `shopItemState3`; depois o item passa para estado 5. Atualizar
esse estado ou recarregar o save não executa novamente o estado 3. Scripts,
`var3a`, estado, saldo, inventário e flags existentes registram o progresso.
Lua não mantém transações ou progresso persistentes e não muda o formato de save.

Para tesouros substituíveis, a ordem é: guardar localização e ID/subid originais,
consultar Lua, aplicar o mod existente que corresponde aos valores originais e
executar os efeitos C. O mod prevalece sobre a escolha base Lua; mudar o ID em
Lua começa no subid 0 e mantém o modo de spawn/localização original. Não existe
uma segunda seleção Lua depois do mod que desfaça sua decisão.

Falhas param antes de continuar pelo ponto da decisão. Não há fallback silencioso
nem rollback das operações C legítimas anteriores: por exemplo, a ordem original
de um débito anterior à seleção da recompensa é conservada.

## Verificação e build

Os testes `lua_items_shops` e `lua_items_shops_rom` verificam contratos, todos os
saldos 0–999 contra os 21 seletores, catálogo, capacidade, BCD/upgrades, modos de
aplicação, inventário, estoque, confirmação/cancelamento e prioridade dos mods.
Os fixtures dirigidos usam a ROM própria e comparam o GB completo e amostras
entre C com Lua desativada e os mesmos mecanismos com Lua ativa.
O relatório local `reports/lua-phase-5.md` registra resultados finais,
replays, persistência, desempenho, auditorias e pacotes.

```sh
cmake -S . -B build-lua -DORACLES_LUA_GAMEPLAY=ON \
  -DORACLES_LUA_BLOCK_PUZZLES=ON -DORACLES_LUA_COLORED_CUBE=ON \
  -DORACLES_LUA_LEVERS_LAVA=ON -DORACLES_LUA_DOORS=ON \
  -DORACLES_LUA_ITEMS_SHOPS=ON -DORACLES_VERSION=
cmake --build build-lua --target test_lua_items_shops test_lua_items_shops_rom
ctest --test-dir build-lua -R '^lua_items_shops' --output-on-failure
```

Scripts de build, manifesto, smoke tests e workflow Lua recebem a opção nova.
O CI padrão permanece com Lua OFF; CI público não dispõe da ROM comercial para
executar esses fixtures. Os empacotadores de fases anteriores recusam builds da
fase 5. `tools/package_lua_phase5.py` verifica identidade dos oito módulos,
opções, perfis privados, auditorias e vínculos nativos com Lua; inclui fontes
correspondentes, licenças e checksums.

SHA256 dos oito módulos incorporados:
`4965b8fa088ab9116c3f3ecc1a336e2a799f8e562ba567fdde9f991011dadf23`.
Contrato local: heap/pico de 38.006 bytes, limite de 256 KiB, zero alocações
no caminho quente. Pacotes e roteiro: [LUA-PHASE-5-PLATFORM-TESTS.md](LUA-PHASE-5-PLATFORM-TESTS.md).

## Evidências locais desta rodada

- Contrato: 460.265 decisões canônicas, além do laço de alocação e dos testes
  negativos. Quinze regras com cobertura positiva nos hooks reais; tipos,
  domínios, IDs sem recurso, limites da VM e respostas inválidas são verificados.
- 32.021 casos dirigidos: inclui os 32 cenários adicionais de alavanca, progresso,
  tentativa inicial e RNG da recompensa de chefe de D6. C e Lua preservam os
  estados, saldos, inventário, flags e amostras completos.
- Sete famílias × 64 frames em duas rodadas: normal e com 42 operações de
  save/load e recriação da VM. Compras não cobram novamente; Tokay desconta
  exatamente dez sementes, mantendo dez do saldo inicial de vinte.
- Dois replays de 289.518 frames, 4.825 checkpoints por modo, zero pulados ou
  divergentes; a rodada com retomadas fez 285 recargas.
- Os dez arquivos nativos alterados produzem código de máquina idêntico à base
  `fb4c18a` com Lua OFF. Traces de 960 frames coincidem entre build C, build Lua
  com integração OFF e Lua ON, normalizando apenas ponteiros do processo.
- Nove medições de CPU, três por variante em ordem alternada, 480 frames de
  aquecimento e 1.600 medidos, na cena de estoque/compra/coleta: mediana das médias
  de 2,387 ms no build C e 2,331 ms com Lua. Diferença observada de −2,33%, dentro
  do orçamento de 10%, sem evidência de regressão nessa cena. Essa variação não
  comprova aceleração e não mede desempenho de Windows/Switch. Zero alocações Lua
  nos frames medidos; hashing e I/O de save permanecem fora da cronometragem.
- Builds macOS Intel, Windows x64 e Switch concluídos. Auditorias de vínculos,
  UTF-8/dependências no PE, estrutura NRO, ícone/título e ausência de interpretador
  da CPU Game Boy/cabeçalho de ROM incorporado passaram. O fluxo de manifesto e
  empacotamento do CI também foi exercitado localmente com a opção nova.

As validações locais não substituem a confirmação dos novos pacotes nas duas
plataformas nem a regressão completa de Seasons. Os pacotes experimentais usam
perfis separados e não habilitam o atualizador de releases estáveis.
