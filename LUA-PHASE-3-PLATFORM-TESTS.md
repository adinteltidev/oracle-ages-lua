# Testar C + Lua — fase 3, alavancas e lava

30/10/2026. Branch `port/lua-gameplay`, base `e94017b`, alterações locais.
As opções Lua, blocos, cubo colorido e alavancas/lava estão ON; Lua incorporado.
Identidade das fontes:
`58bb6adb1f8b94893ca135e8c4c89723a2934da6070beebe31224008fc431789`.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase3-Intel.dmg`, copie **Oracles Lua Phase3.app** e
importe sua ROM de Ages USA/Australia pelo menu. Perfil:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase3/`.
Assinatura ad hoc, sem notarização; fontes/licenças acompanham o DMG.

## Windows x64

1. Extraia `Oracles-Windows-Lua-Phase3.zip` inteiro.
2. Entre na pasta `Oracles-Windows-Lua-Phase3` e execute `Iniciar-Oracles-Lua.cmd`.
3. Importe sua ROM e teste controles, áudio, menu, slots e retomada.

Executável: `Oracles-Lua-Fase3.exe`. Perfil e log:
`%APPDATA%\oracles-decomp\oracles-lua-phase3\runtime.log`.

Contratos, cenas dirigidas e replay curto:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
```

Replays completos, normal e com recargas:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

Esperar `PASS`. O contrato novo verifica 1.547 decisões. As cenas exercitam os
seis scripts de sala, ciclos completos, temporizadores e saves. Os logs ficam
em `validation`: `levers-lava-contract.log`, `levers-lava-scenes.log`, `replay.log`
e os contratos anteriores. Nenhum desses executáveis exige Python/Lua instalados.

O replay curto compara 333 checkpoints por modo; o completo deve comparar
4.825 por modo e zero pulados. Exit 77 é teste não executado.

## Nintendo Switch

1. Extraia `Oracles-Switch-Lua-Phase3.zip` e copie a pasta `switch` para a raiz do SD.
2. Coloque sua ROM em `sdmc:/switch/oracle-ages-lua-phase3/ages.gbc`.
3. Abra `oracles-lua-phase3.nro` em modo aplicação com memória completa, conforme
   `SWITCH.md`. Título: **Oracle of Ages - Lua phase 3**, com ícone **recomp**.
4. **X** inicia o replay curto com recargas; **Y**, o completo. **Plus** cancela/sai.
   Mantenha os três arquivos da subpasta `tests`.
5. **A** inicia o jogo. **L + R** abre/fecha o menu; **ZL** faz quicksave e **ZR**
   quickload. Teste quatro slots, fechamento/reabertura e suspensão/retomada.

Logs no perfil da fase 3: `runtime.log`, `replay-result.txt`, `replay-observed.txt`.
No completo esperar `status=PASS`, `checkpoints=4825`, `expected=4825` e recargas
positivas. O NRO não executa os fixtures dirigidos do desktop.

## Cenas que precisam de teste real

Teste as três salas de D4, duas de D8 e a sala da Hero's Cave controladas pelo
filler. Puxe parcialmente, puxe até o fim, segure, solte até a posição inicial e
repita. Confira ordem/ritmo dos grupos, fonte nas orientações superior/inferior,
chão seco/lava e os sons. Saia/retorne, salve/carregue durante a atualização e
reinicie o aplicativo. No Switch repita portátil/dock quando disponíveis.

Os nove contadores aparecem como `ages/levers_lava.*`. Um replay natural que não
visite essas salas pode ter zero chamadas: isso não é prova específica da fase.
Os fixtures dirigidos têm cobertura positiva das nove regras e usam os scripts
reais, mas não substituem a partida nas seis salas e a aceitação de áudio/cadência.

Registre build/pacote, plataforma, modo portátil/dock, resultado e logs. A
validação real do Windows/Switch e a regressão completa de Seasons permanecem
pendentes. `BUILD.json` e os checksums identificam os arquivos distribuídos.
ZIPs/DMG incluem fontes e licenças; não incluem ROM ou saves pessoais.
