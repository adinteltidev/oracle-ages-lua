# Testar C + Lua — fase 4, portas

30/10/2026. Branch `port/lua-gameplay`, base `b4ee740`, alterações locais.
As opções Lua, blocos, cubo colorido, alavancas/lava e portas estão ON; Lua incorporado.
Identidade das fontes:
`470ac2b8b5316096754e6f40237b69d87979c5201b97dc57fa972fe549d512dd`.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase4-Intel.dmg`, copie **Oracles Lua Phase4.app** e
importe sua ROM de Ages USA/Australia pelo menu. Perfil:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase4/`.
Assinatura ad hoc, sem notarização; fontes/licenças acompanham o DMG.

## Windows x64

1. Extraia `Oracles-Windows-Lua-Phase4.zip` inteiro.
2. Entre na pasta `Oracles-Windows-Lua-Phase4` e execute `Iniciar-Oracles-Lua.cmd`.
3. Importe sua ROM e teste controles, áudio, menu, slots e retomada.

Executável: `Oracles-Lua-Fase4.exe`. Perfil e log:
`%APPDATA%\oracles-decomp\oracles-lua-phase4\runtime.log`.

Contratos, cenas dirigidas e replay curto:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
```

Replays completos, normal e com recargas:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

Esperar `PASS`. O contrato novo verifica 200.470 decisões canônicas de 28 regras.
As cenas de portas verificam 5.921 casos, todas as famílias do inventário,
sequências de frames, consumo de chaves, gatilhos e save/load durante animação.
Os logs ficam em `validation`: `doors-contract.log`, `doors-scenes.log`,
`replay.log` e os contratos/cenas anteriores. Não é necessário instalar Lua/Python.

O replay curto compara 333 checkpoints por modo; o completo deve comparar
4.825 por modo e zero pulados. Exit 77 é teste não executado.

## Nintendo Switch

1. Extraia `Oracles-Switch-Lua-Phase4.zip` e copie a pasta `switch` para a raiz do SD.
2. Coloque sua ROM em `sdmc:/switch/oracle-ages-lua-phase4/ages.gbc`.
3. Abra `oracles-lua-phase4.nro` em modo aplicação com memória completa, conforme
   `SWITCH.md`. Título: **Oracle of Ages - Lua phase 4**, com ícone **recomp**.
4. **X** inicia o replay curto com recargas; **Y**, o completo. **Plus** cancela/sai.
   Mantenha os três arquivos da subpasta `tests`.
5. **A** inicia o jogo. **L + R** abre/fecha o menu; **ZL** faz quicksave e **ZR**
   quickload. Teste quatro slots, fechamento/reabertura e suspensão/retomada.

Logs no perfil da fase 4: `runtime.log`, `replay-result.txt`, `replay-observed.txt`.
No completo esperar `status=PASS`, `checkpoints=4825`, `expected=4825` e recargas
positivas. O NRO não executa os fixtures dirigidos do desktop.

## Roteiro de regressão manual

Teste portas por interruptor, inimigos e tochas, fechaduras com/sem chave pequena,
porta de chefe e portas/portões de carrinho nas duas orientações. Confira abrir,
atravessar, fechar, som e colisão. Permaneça no tile durante o fechamento, saia e
retorne à sala e faça save/load durante abertura e fechamento. No Switch repita
portátil/dock quando disponíveis. Teste também as salas de blocos, cubo colorido
e lava das fases anteriores para detectar regressões.

Em 30/10/2026, o jogador confirmou funcionamento e estabilidade nas versões
Windows e Switch. Veja o [registro da validação](LUA-PHASE-4-PLAYER-VALIDATION.md).
O roteiro acima permanece disponível para regressões futuras; não foi presumida
a execução de cada cena ou dos replays no relato. A regressão completa de
Seasons com ROM própria continua pendente.
