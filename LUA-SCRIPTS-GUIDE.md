# Guia dos scripts Lua de gameplay

Referência conferida no código após o commit `9e1c1ab`, branch `port/lua-gameplay`.

## Localização e funcionamento

Os **11 módulos, com 120 funções**, ficam em `gameplay/ages/`. Eles são usados
pelas integrações habilitadas nos pacotes C + Lua da fase 7. O projeto continua
híbrido: os grafos de despacho, a execução física e os recursos originais não
foram integralmente reescritos em Lua.

A cada ponto integrado, o motor C lê os valores necessários, chama uma função
Lua, valida seu único resultado e aplica a decisão no ponto original. Lua recebe
valores explícitos, não ponteiros para o jogo. Uma regra pode retornar um booleano,
um estado, um índice/ID ou uma intenção de movimento. Cada chamada tem seu próprio
significado; `true` não representa universalmente a mesma ação em todos os módulos.

Cada arquivo retorna uma tabela de funções (`return { nome = function(...) ... end }`).
C registra essas funções na inicialização e as chama quando o controlador do jogo
precisa da regra. Esses arquivos não possuem um loop principal independente.

Os nomes abaixo são as **assinaturas Lua**. Os adaptadores C podem usar argumentos
`a`, `b`, etc.; o tipo, a ordem e o sentido Lua estão descritos nas tabelas.

## Como ler os valores

- A maior parte das entradas são bytes inteiros de **0 a 255**, copiados por C.
- Indicadores de serviço normalmente chegam como **0 ou 1**. Em Lua, `0` é
  verdadeiro em uma condição simples; por isso o código usa `== 0`, `== 1` ou `~= 0`.
- `state` é o estado interno do controlador; `subid` é a variante de um objeto.
- `tile` é um quadrado/ID do mapa; `flags` agrupa marcadores em bits.
- `mask` seleciona quais bits interessam. `&` faz E bit a bit, `|` faz OU,
  e `<<` desloca bits. `0x80` vale 128; `0x7f` vale 127; `0xff` vale 255.
- O retorno deve ter exatamente o tipo esperado: booleano Lua ou inteiro Lua.
  `true` e o número `1` não são intercambiáveis nessas interfaces.
- O host C restringe os números a domínios específicos. Lua não pode retornar
  qualquer estado, ID ou endereço. Uma resposta inválida interrompe o motor.

## Catálogo completo das funções

### switch_tiles.lua — Alternância de tiles e interruptores

Arquivo: [gameplay/ages/switch_tiles.lua](gameplay/ages/switch_tiles.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `changed(previous, current)` | Compara o valor anterior e o atual; retorna `true` se forem diferentes. |
| `alternate(mask, switches)` | Calcula `mask & switches`; retorna `true` se algum bit selecionado estiver ativo. |

### block_puzzles.lua — Progressão do controlador de puzzle de blocos

Arquivo: [gameplay/ages/block_puzzles.lua](gameplay/ages/block_puzzles.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `next_state(state, a, b)` | Retorna um inteiro de estado: `0 → 1`; em `1`, avança para `2` quando `a >= b` (limiar/subid versus inimigos restantes); em `2`, avança para `3` se o tile original `a` diferir do atual `b`; em `3`, retorna `4` se `a == 1` indicar que o temporizador terminou. Nos outros casos válidos mantém o estado. C só fornece estados de entrada 0–3. |

### block_rules.lua — Regras de blocos, sincronização, água, painéis e armadilhas

Arquivo: [gameplay/ages/block_rules.lua](gameplay/ages/block_rules.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `cube_absent(position)` | Retorna `true` se a posição do cubo for zero, usada como ausência. |
| `cube_active(color)` | Retorna `true` se o bit `0x80` da cor estiver ativo. |
| `cube_match(block_index, cube_index)` | Compara os índices do bloco e do cubo; retorna igualdade. |
| `block_has_replacement(tile)` | Retorna `true` se o ID do tile substituto não for zero. |
| `block_finishes_puzzle(rotated_properties)` | Testa o bit 0 das propriedades já rotacionadas em C; retorna se está ativo. |
| `sync_awake(enabled)` | Retorna `true` para qualquer byte de habilitação diferente de zero. |
| `sync_eligible(tile)` | Retorna `true` se o tile não for `0xda`, o bloco de Somaria excluído da sincronização. |
| `sync_next_state(current)` | Retorna `2` quando o estado atual é `1`; caso contrário retorna `1`. |
| `water_exists(subid, flooding_flag)` | Para subid 0, retorna se a flag de inundação é zero; nos demais, se é diferente de zero. |
| `water_initial_side(subid)` | Retorna `true` para subid 0, selecionando o lado inicial correspondente. |
| `water_direction_match(direction, required)` | Compara a direção recebida com a direção requerida; retorna igualdade. |
| `water_advance(elapsed)` | Retorna `true` se C informou término do temporizador por meio de `elapsed == 1`. |
| `panel_opened(room_flag_40)` | Retorna se o bit/valor de abertura da sala, já extraído em C, é diferente de zero. |
| `panel_trigger(triggers)` | Retorna se o bit `0x80` dos gatilhos está ativo. |
| `panel_right(subid)` | Retorna `true` para subid diferente de zero, selecionando o lado direito. |
| `panel_move_tick(rotated_frame)` | Testa o bit `0x80` do contador de frames já rotacionado em C; retorna se deve executar este passo. |
| `panel_advance(elapsed)` | Retorna `true` quando `elapsed == 1`, indicando avanço permitido. |
| `tiles_end(token)` | Retorna `true` para token 0, o terminador da sequência. |
| `tiles_new_group(position)` | Retorna `true` para posição `0xff`, a marca de novo grupo. |
| `tiles_match(actual, expected)` | Compara tile atual e esperado; retorna igualdade. |
| `puzzle_cube_position(actual, expected)` | Compara posição atual e esperada do cubo; retorna igualdade. |
| `puzzle_cube_color(masked, expected)` | Compara a cor já mascarada em C com a esperada; retorna igualdade. |
| `puzzle_cube_valid_color(color)` | Retorna `true` se a cor for menor que 3. |
| `panels_torch_count(count)` | Retorna `true` se a contagem de tochas for exatamente 1. |
| `panels_all_triggers(triggers)` | Retorna `true` se o byte de gatilhos for exatamente 7. |
| `panels_wait_opened(triggers)` | Retorna `true` enquanto o bit `0x80` dos gatilhos estiver desligado. |
| `statue_placed(tile)` | Retorna `true` se o tile for `0x2a`, o marcador esperado. |
| `trap_route_open(collision)` | Retorna `true` se o valor de colisão for zero. |
| `trap_skip_corner(remaining)` | Retorna `true` se o número de posições restantes for ímpar. |
| `trap_solid_tile(tile)` | Retorna `true` se o valor do tile for diferente de zero. |
| `trap_all_checked(remaining)` | Retorna `true` se não restarem posições para verificar. |
| `trap_advance(elapsed)` | Retorna `true` se C informou `elapsed == 1`. |

### colored_cube.lua — Existência, visibilidade e paleta da chama do cubo

Arquivo: [gameplay/ages/colored_cube.lua](gameplay/ages/colored_cube.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `present(position)` | Retorna `true` quando a posição é diferente de zero. |
| `visible(color)` | Retorna `true` se o bit `0x80` da cor estiver ativo. |
| `palette_offset(color)` | Remove o bit superior com `color & 0x7f`; devolve o índice numérico 0–127 usado no acesso à paleta da ROM. |

### levers_lava.lua — Alavanca e preenchimento de lava

Arquivo: [gameplay/ages/levers_lava.lua](gameplay/ages/levers_lava.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `fully_pulled(distance)` | Testa o bit `0x80` da distância; retorna se a alavanca atingiu a condição de puxada completa. |
| `released(distance)` | Retorna `true` se a distância for zero. |
| `timer_elapsed(zero)` | Retorna `true` se a flag de término fornecida por C for 1. O decremento já aconteceu em C. |
| `has_tile(token)` | Retorna `true` para token diferente de zero. |
| `source_found(zero)` | Retorna `true` se a condição zero fornecida por C for 1. |
| `source_segment(relative_tile)` | Retorna `true` se a posição relativa do tile for menor que 12. |
| `source_delta(empty)` | Se `empty == 1`, retorna `0xfa` (250, representação de -6 em byte); caso contrário retorna 6. |
| `next_state(state)` | Retorna 1 depois do estado 4; nos demais estados válidos retorna `state + 1`. |
| `floor_tile(state, random_byte)` | No estado 2 retorna o tile 1; nos demais retorna `0x61 + (random_byte & 3)`, isto é, um ID de `0x61` a `0x64`. O número aleatório vem de C. |

### doors.lua — Portas, chaves e portões

Arquivo: [gameplay/ages/doors.lua](gameplay/ages/doors.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `switch_hook_busy(value)` | Retorna `true` se o estado recebido do Switch Hook for 2. |
| `palette_busy(value)` | Retorna `true` se a operação de paleta estiver ativa, com valor não zero. |
| `open_collision(collided)` | Retorna `true` se houve colisão, representada por 1. |
| `close_collision(collided)` | Retorna `true` se não houve colisão, representada por 0. |
| `somaria_tile(tile)` | Retorna `true` para o tile `0xda`. |
| `same_position(a, b)` | Compara duas posições codificadas em bytes; retorna igualdade. |
| `timer_elapsed(zero)` | Retorna `true` se C informou término com valor 1. |
| `trigger_active(masked)` | Retorna `true` se o valor já mascarado em C não for zero. |
| `tile_matches(actual, expected)` | Compara tile atual e esperado; retorna igualdade. |
| `collision_clear(collision)` | Retorna `true` se a colisão recebida for zero. |
| `minecart_riding(riding)` | Retorna `true` se C informou que Link está no carrinho, com valor 1. |
| `minecart_collided(collided)` | Retorna `true` se a colisão do carrinho for 1. |
| `torches_ready(required, actual)` | Compara a quantidade requerida e a atual de tochas; retorna igualdade. |
| `gate_open(masked)` | Retorna `true` se o valor mascarado for zero. |
| `gate_switch(masked)` | Retorna `true` se o valor mascarado não for zero. |
| `gate_is_open(bit_set)` | Retorna `true` se o bit de abertura normalizado for 1. |
| `gate_animation_done(advanced_parameter)` | Retorna `true` se o parâmetro de animação já incrementado em C tiver voltado a zero. |
| `gate_state(animation)` | Retorna o inteiro `(animation & 1) + 1`, portanto 1 ou 2. |
| `script_state(requested)` | Devolve o estado proposto por C, sem alteração; o host exige valor de 0 a 3. |
| `no_enemies(count)` | Retorna `true` quando a quantidade de inimigos é zero. |
| `link_clear(collided)` | Retorna `true` se o indicador de colisão de Link for zero. |
| `valid_dungeon(index)` | Retorna `true` se o índice da dungeon não for o sentinela `0xff`. |
| `boss_door(tile_parameter)` | Retorna `true` se o parâmetro do tile for maior ou igual a `0x40`. |
| `small_key(count)` | Retorna `true` se a quantidade de chaves pequenas não for zero. |
| `boss_key(owned)` | Retorna `true` se o indicador de posse da chave do chefe for 1. |
| `reserved_free(enabled)` | Retorna `true` se o registro reservado estiver desabilitado, com valor zero. |
| `overworld_locked(masked)` | Retorna `true` se o valor mascarado for zero. |
| `overworld_key(owned)` | Retorna `true` se o indicador de posse da chave do mundo externo for 1. |

### items.lua — Decisões auxiliares de itens e recompensas

Arquivo: [gameplay/ages/items.lua](gameplay/ages/items.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `compare(a,b)` | Compara `a` e `b`: retorna o inteiro `0` para iguais, `1` para `a < b`, `2` para `a > b`. |
| `zero(value)` | Retorna `true` quando o valor é zero; caso contrário, `false`. |
| `bit_zero(value,mask)` | Aplica a máscara de bits ao valor; retorna `true` se `(value & mask) == 0`. |
| `carry(value)` | Retorna `true` apenas para a entrada normalizada `1`, que representa carry ativo. |
| `reward_id(id)` | Devolve o ID recebido, sem alterá-lo. C valida e entrega a recompensa. |
| `parameter_mode(mode)` | Devolve o modo de parâmetro recebido. A escolha/aplicação dos efeitos continua nos serviços C. |

### shops.lua — Compras e seleção de recursos de loja

Arquivo: [gameplay/ages/shops.lua](gameplay/ages/shops.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `compare(a,b)` | Compara `a` e `b`: retorna o inteiro `0` para iguais, `1` para `a < b`, `2` para `a > b`. |
| `zero(value)` | Retorna `true` quando o valor é zero; caso contrário, `false`. |
| `bit_zero(value,mask)` | Aplica a máscara de bits ao valor; retorna `true` se `(value & mask) == 0`. |
| `carry(value)` | Retorna `true` apenas para a entrada normalizada `1`, que representa carry ativo. |
| `reward_id(id)` | Devolve o ID recebido, sem alterá-lo. C valida e entrega a recompensa. |
| `price_index(index)` | Devolve o índice de preço já preparado em C; não calcula uma nova tabela de preços. |
| `table_index(index)` | Devolve o índice de tabela recebido. |
| `purchase_outcome(outcome)` | Devolve o código de resultado da compra já preparado em C. |
| `affordable(low,high,cost_low,cost_high)` | Recebe saldo e custo em dois bytes cada. Retorna `true` se o byte alto do saldo for maior que o do preço, ou se forem iguais e o byte baixo do saldo for maior/igual. Os valores seguem o BCD compactado original; não debita o saldo. |

### npcs/rules.lua — Disponibilidade de NPCs e seleção de roteiro

Arquivo: [gameplay/ages/npcs/rules.lua](gameplay/ages/npcs/rules.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `compare(a,b)` | Compara `a` e `b`: retorna o inteiro `0` para iguais, `1` para `a < b`, `2` para `a > b`. |
| `zero(value)` | Retorna `true` quando o valor é zero; caso contrário, `false`. |
| `bit_zero(value,mask)` | Aplica a máscara de bits ao valor; retorna `true` se `(value & mask) == 0`. |
| `carry(value)` | Retorna `true` apenas para a entrada normalizada `1`, que representa carry ativo. |
| `old_lady_available(subid,saved,progress)` | No subid 0, retorna `saved == 0`; no outro subtipo integrado, retorna `progress < 4`. `saved` é a flag de progresso/resgate consultada em C, não um arquivo de save. |
| `old_lady_script(subid,progress)` | No subid 0 retorna índice 0; no subid 1 devolve o progresso como índice do roteiro original. C só aceita índices válidos para esse NPC. |
| `symmetry_available(subid,nut)` | Retorna `true` para subid diferente de 12 ou quando o indicador de reparo/Tuni Nut é não zero. |
| `symmetry_restart(state,flags)` | Retorna `true` somente no estado 2 com o bit 0 de `flags` ativo. |

### events/flow.lua — Condições e progressão de eventos

Arquivo: [gameplay/ages/events/flow.lua](gameplay/ages/events/flow.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `compare(a,b)` | Compara `a` e `b`: retorna o inteiro `0` para iguais, `1` para `a < b`, `2` para `a > b`. |
| `zero(value)` | Retorna `true` quando o valor é zero; caso contrário, `false`. |
| `bit_zero(value,mask)` | Aplica a máscara de bits ao valor; retorna `true` se `(value & mask) == 0`. |
| `carry(value)` | Retorna `true` apenas para a entrada normalizada `1`, que representa carry ativo. |
| `table_index(index)` | Devolve o índice recebido. C valida a saída antes de acessar a tabela original. |
| `transition(current,requested)` | Devolve o estado solicitado por C. O estado atual também é recebido para permitir a política de manter a execução parada. |
| `next_state(current)` | Incrementa o estado com limite de byte: `(current + 1) & 255`; depois de 255 retorna 0. |
| `dialogue(low,high)` | Combina os dois bytes do ID: `low \| (high << 8)`. Retorna um inteiro de 16 bits, não uma string nem um roteiro de conversa. |
| `choose_linked(linked)` | Retorna `true` se o indicador de jogo conectado não for zero. |
| `script_ready(active)` | Retorna `true` quando o estado de texto é zero ou seu bit `0x80` está ativo. |
| `script_ended(opcode)` | Retorna `true` para opcode 0, o terminador considerado pelo interpretador. |
| `condition(fact)` | Retorna `true` se o fato numérico recebido for diferente de zero. |

### enemies/ai.lua — Decisões condicionais e intenções de inimigos

Arquivo: [gameplay/ages/enemies/ai.lua](gameplay/ages/enemies/ai.lua).

| Função e entradas | Processamento e saída |
| --- | --- |
| `compare(a,b)` | Compara `a` e `b`: retorna o inteiro `0` para iguais, `1` para `a < b`, `2` para `a > b`. |
| `zero(value)` | Retorna `true` quando o valor é zero; caso contrário, `false`. |
| `bit_zero(value,mask)` | Aplica a máscara de bits ao valor; retorna `true` se `(value & mask) == 0`. |
| `condition(flags, mask)` | Recebe flags aritméticas e uma máscara; retorna `true` se algum bit selecionado estiver ativo. |
| `transition(current, requested)` | Devolve o estado solicitado. C valida que é o estado atual ou a transição solicitada originalmente. |
| `angle(current, requested)` | Devolve o ângulo proposto por C, que já calculou a geometria/alvo. O host permite o valor atual, a proposta original ou ângulos de 0 a 31; sentinelas originais são preservados quando recebidos. |
| `direction(current, requested)` | Devolve a direção proposta. O host aceita a atual, a proposta original ou direções de 0 a 3. |
| `speed(current, requested)` | Devolve a velocidade proposta. O host aceita a atual, a proposta original ou 0 para parar; C executa o movimento. |
| `octorok_attack(random, mask)` | Retorna `(random & mask) == 0`. Usa o resultado do RNG de C para autorizar o ramo que prepara o ataque; projétil e temporizador são aplicados por C. |
| `octorok_aim(subid)` | Retorna `true` para subid maior ou igual a 2, indicando a variante que mira no alvo. |

## Exemplos de entrada → processamento → saída

**Mudança de tile:** `changed(0x2c, 0x2d)` compara dois IDs diferentes e devolve
`true`. C executa o ramo correspondente; a função não escreve o mapa.

**Puzzle:** `next_state(1, 2, 1)` em `block_puzzles.lua` recebe estado 1, limiar 2 e
um inimigo restante. Como `2 >= 1`, devolve estado 2. C registra esse avanço.

**Paleta:** em `colored_cube.lua`, `color = 0x82` produz `visible(color) == true`
e `palette_offset(color) == 2`. C acessa a paleta da ROM e faz a apresentação.

**Loja:** `affordable(0x50, 0x01, 0x80, 0x00)` compara saldo BCD de 150 com preço
BCD de 80 e devolve `true`. Debitar dinheiro e entregar o item são efeitos de C.

**Evento:** `dialogue(0x0a, 0x18)` devolve `0x180a`, o ID usado para buscar o texto
original da ROM. Não devolve o conteúdo da conversa.

**Octorok:** `octorok_attack(4, 3)` resulta em `true`, pois `4 & 3 == 0`.
`octorok_attack(6, 3)` resulta em `false`, pois `6 & 3 == 2`. O ramo autorizado
prepara o ataque; o disparo efetivo continua sujeito ao controlador e timer C.

As chamadas acima são exemplos conceituais da interface. O ambiente de gameplay
não expõe `require`, `dofile`, IO ou bibliotecas globais para carregar módulos.

## Por que algumas regras apenas devolvem a entrada?

`reward_id(id)`, `price_index(index)`, `transition(current, requested)`, `angle`,
`direction` e `speed` conservam, na política atual, o valor já proposto por C.
São pontos de decisão integrados e validados, mas não representam por si só uma
nova seleção de recompensas, nova tabela de preços, toda a máquina de estados
ou uma nova IA completa escrita em Lua. Os predicados e regras específicas fazem
os cálculos mostrados nas tabelas; os grafos C continuam organizando sua execução.

O texto dos diálogos, por exemplo, permanece na ROM. Lua auxilia na disponibilidade,
seleção de índice e progressão; não contém as falas completas de todos os NPCs.

## Alguns limites impostos pelo adaptador C

O que a função Lua calcula e o que C aceita são duas partes da interface:

- Recompensas de itens/lojas: IDs 0–98, excluindo `0x13` e `0x40`, sem recurso
  correspondente de objeto de Ages nessa interface.
- Modo de parâmetro de item: 0–15; índice de preço de loja: 0–20.
- Resultado de compra: apenas os códigos 1 ou 255.
- Índice de tabela de loja/evento: não pode exceder o índice originalmente recebido.
- Diálogo de evento: o ID original combinado ou o ID zero autorizado pelo host.
- Estado de evento/inimigo: atual ou solicitado; `next_state` de evento aceita
  manter o atual ou seu sucessor de byte.

Esses limites não transformam todos os valores dentro deles em recursos novos.
O conteúdo e os efeitos precisam existir nos serviços e dados originais do jogo.

## Integração, persistência e edição

- [src/script/gameplay.c](src/script/gameplay.c): carrega módulos, chama funções,
  valida tipos/domínios, conta chamadas e controla os limites da VM.
- [src/script/gameplay.h](src/script/gameplay.h): interface de carregamento usada
  também pelos testes.
- [src/script/enemies.h](src/script/enemies.h) e os demais headers de
  `src/script/`: adaptadores entre o retorno Lua e os controladores C.
- [src/game/common/enemies/octorok.c](src/game/common/enemies/octorok.c): exemplo
  de controlador nativo que consulta as regras e aplica o ramo de ataque.
- [cmake/LuaGameplay.cmake](cmake/LuaGameplay.cmake): lista os onze módulos e
  incorpora os textos Lua nos executáveis durante o build.

As funções de gameplay não guardam inventário, avanço de evento ou posição em
estado durável Lua. C mantém os campos do jogo, saves/SRAM, temporizadores,
movimento, colisão, dano, áudio e desenho. O RNG é o original de C: seus valores
são entradas Lua quando necessários.

Para editar uma regra, altere seu `.lua` de origem e **recompile**. Os pacotes
atuais usam scripts incorporados; mudar o arquivo no repositório não modifica
sozinho um `.exe`, `.app` ou `.nro` já gerado. Arquivos como
`build-macos-lua/ages_enemies_ai_lua.c` são textos Lua convertidos em arrays C pelo
build; a fonte editável continua sendo `gameplay/ages/enemies/ai.lua`.

Os módulos são de Ages. Seasons conserva o caminho C; compilar a aplicação C + Lua
não gera uma ROM de Game Boy Color.

## Outros arquivos que podem causar confusão

- [gameplay/candidates/ages/block_cube_permission.lua](gameplay/candidates/ages/block_cube_permission.lua):
  protótipo de permissão de empurrar bloco conforme presença/cor/posição do cubo;
  expõe `no_cube`, `color_active`, `tile_matches` e a combinação `allows`.
- [gameplay/candidates/ages/block_synchronizer.lua](gameplay/candidates/ages/block_synchronizer.lua):
  protótipo de sincronização com `awake(enabled)` e `eligible_tile(tile)`.
- Os dois `candidates` são usados por fixtures de teste e **não são incorporados
  nem chamados pelos hooks do jogo distribuído**. As regras integradas estão nos
  onze módulos listados acima.
- Os `.json` de inventário em `gameplay/ages/` documentam unidades/famílias e
  integração; não são scripts Lua de gameplay.
- `tools/bizhawk/lua/` contém ferramentas de inspeção/diagnóstico para o emulador,
  não os módulos de gameplay carregados pelo port nativo.

Uma ordem prática de leitura é `switch_tiles.lua`, `colored_cube.lua`,
`block_puzzles.lua`, `shops.lua`, `npcs/rules.lua`, `events/flow.lua` e `enemies/ai.lua`.
Depois, os módulos maiores de blocos e portas mostram como os predicados são agrupados.
