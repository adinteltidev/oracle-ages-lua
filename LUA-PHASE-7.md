# Fase 7 — decisões de IA dos inimigos

31/10/2026. Branch `port/lua-gameplay`, base `73e7f75`, alterações locais.
**Status: implementação e validação local concluídas.** A confirmação de
funcionamento da fase 6 no Windows/Switch permanece registrada para aqueles
pacotes. Os novos pacotes da fase 7 precisam de validação própria nos aparelhos.

## Escopo e fronteira

O inventário em `gameplay/ages/enemies/inventory.json` classifica 142 arquivos:
99 unidades com decisões de IA, três serviços compartilhados de estado, um
despachante que delega para `kingMoblinMinionMain.c`, seis serviços sem IA de
combate e 33 implementações exclusivas de Seasons, mantidas em C.
O catálogo dirigido contém 101 IDs de despacho; arquivos auxiliares podem
pertencer à mesma família. Essas contagens não representam um percentual do jogo.

`gameplay/ages/enemies/ai.lua` fornece dez regras: condições de serviço,
comparação de valores, zero, teste de bits, transição de estado, ângulo, direção,
velocidade, decisão de ataque e mira do Octorok. As decisões condicionais dos
controladores recebem valores explícitos, incluindo distâncias/limiares, flags,
contadores e resultados do RNG original. C consome a decisão Lua no ramo ou na
escrita original. O Octorok decide em Lua se fica parado ou ataca com base no
resultado aleatório e sua máscara, e se mira no alvo conforme seu subtipo.

C conserva o grafo de despacho dos controladores, leituras/ALU/ciclos físicos,
cálculo geométrico do alvo, movimento, colisão, dano, animação e criação efetiva
de projéteis. A integração não é uma transcrição integral dos controladores C
para roteiros Lua. Helpers de endereços da ROM, suas tabelas e carry de ponteiros
permanecem em C; Lua não recebe ponteiros de GB nem comandos de memória.

Os serviços comuns submetem estado/subestado e intenções de movimento ao host
validado. Estado admite o valor atual ou solicitado; ângulo/direção admitem seus
valores nativos e os sentinelas originais; velocidade permite manter, aplicar a
solicitação ou parar. Respostas inválidas interrompem a execução com regra e
motivo. Serviços C já executados antes da falha conservam seus efeitos legítimos.

`ORACLES_LUA_ENEMIES` é OFF por padrão e exige `ORACLES_LUA_GAMEPLAY`.
Seasons conserva seu caminho C. A sequência e o número de chamadas ao RNG em C
são mantidos; Lua não possui RNG, IO ou bibliotecas globais. Todo estado durável
continua nos campos originais de GB/SRAM/save. Não há corrotinas ou formato novo
de serialização. Contadores de diagnóstico por regra e proprietário ficam fora
do estado emulado e não constituem progresso persistente.

## Verificação

Os contratos cobrem todos os pares de bytes nas comparações, máscaras e intenções,
além de tipos, domínios, instruções, isolamento e alocações. Os casos dirigidos
comparam GB completo e amostras de saída, incluindo quatro sementes de RNG,
retomada nos estados 8–11 e falhas nas dez regras em hooks nativos reais.

O catálogo executa 80 frames por ID, com o mesmo cenário/semente em C e Lua.
Componentes recebem dados de um objeto relacionado; Smog começa no subid 5,
seu controlador de sala, em vez de assumir subid 0 para todos os inimigos.
Cenas sintéticas de inicialização não comprovam a execução de cada fase de
combate de todos os chefes. Replays e contadores registram separadamente o que
foi visitado; lutas/ramificações não visitadas continuam no roteiro manual.

O relatório `reports/lua-phase-7.md` reúne os resultados finais de contratos,
643 casos dirigidos, regressões anteriores, replays completos, comparação do
caminho C, traces, medições e auditorias de distribuição. As medições locais
não aprovam desempenho no Switch ou execução no Windows.

## Resultados locais já verificados

- 22 testes de regressão passaram, incluindo 12 dependentes da ROM própria;
  zero falhas ou testes pulados.
- 643 casos dirigidos; 101 IDs × 80 frames, 16 cenários de combate × 240 frames,
  quatro sementes, 64 saves/recriações da VM e estados 8–11.
- Dez regras com cobertura dirigida positiva, provas de autoridade e injeção de
  respostas inválidas em hooks reais.
- 103 arquivos nativos existentes com código de máquina idêntico à base com
  Lua OFF. Traces de 960 frames coincidem em C / Lua OFF / Lua ON.
- Cena com oito Octorok/Keese/Rope: nove medições alternadas, 480 frames de
  aquecimento e 1.600 medidos. Mediana das médias: C 2,222 ms, OFF 2,234 ms,
  ON 2,297 ms; acréscimo de 3,37%, dentro do orçamento de 10%. Sem alocações
  Lua nos frames medidos. P95/P99 e rodadas completos ficam nos JSONs locais.
- Contrato observado: heap 38.748 bytes, pico 50.219 bytes, limite 256 KiB.
  Os tempos são do macOS Intel i5-5350U, sem compiladores/regressões concorrentes;
  os replays foram suspensos durante a cronometragem.

Os dois replays completos de 289.518 frames passaram, normal e com retomada:
4.825 checkpoints cada, zero pulados e 285 reloads na rodada de recargas.
Os novos pacotes ainda aguardam validação do jogador no Windows/Switch.

## Build e distribuição

```sh
cmake -S . -B build-lua -DORACLES_LUA_GAMEPLAY=ON \
  -DORACLES_LUA_BLOCK_PUZZLES=ON -DORACLES_LUA_COLORED_CUBE=ON \
  -DORACLES_LUA_LEVERS_LAVA=ON -DORACLES_LUA_DOORS=ON \
  -DORACLES_LUA_ITEMS_SHOPS=ON -DORACLES_LUA_NPCS_EVENTS=ON \
  -DORACLES_LUA_ENEMIES=ON -DORACLES_VERSION=
cmake --build build-lua --target test_lua_enemies test_lua_enemies_rom
ctest --test-dir build-lua -R '^lua_enemies' --output-on-failure
```

As oito opções são propagadas aos scripts e ao workflow experimental. O CI público
executa contratos sem ROM; gameplay dependente da ROM própria permanece local.
Os três pacotes têm perfil próprio, fontes correspondentes, licenças e checksums,
sem ROM ou saves pessoais. As atualizações estáveis ficam desativadas.

SHA256 dos onze módulos incorporados:
`2af4a10e44dbe5f2a045f3a57368a07b81d7024885efab329407f1aded49d6b8`.
Roteiro: [LUA-PHASE-7-PLATFORM-TESTS.md](LUA-PHASE-7-PLATFORM-TESTS.md).
Seasons completo continua como pendência comum, dependente de ROM própria.
