# Testar C + Lua — fase 7, inimigos

Branch `port/lua-gameplay`, base `73e7f75`, alterações locais. Lua incorporado;
as oito opções cumulativas estão ON. Perfil próprio da fase 7. A confirmação
anterior de Windows/Switch se refere à fase 6; estes pacotes têm código novo.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase7-Intel.dmg`, copie **Oracles Lua Phase7.app** e
importe sua ROM de Ages USA/Australia. Perfil:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase7/`.
Assinatura ad hoc; sem notarização. Fontes, licenças e metadados acompanham o DMG.

## Windows x64

Extraia `Oracles-Windows-Lua-Phase7.zip`. Na pasta `Oracles-Windows-Lua-Phase7`,
execute `Iniciar-Oracles-Lua.cmd` para abrir `Oracles-Lua-Fase7.exe`.
Perfil: `%APPDATA%\oracles-decomp\oracles-lua-phase7\`.

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

O primeiro comando executa contratos, cenas e replay curto. O segundo usa os
replays completos. Esperar `PASS`; exit 77 significa teste não executado.
Logs na subpasta `validation` do perfil, incluindo `enemies-contract.log`,
`enemies-scenes.log`, `replay.log` e as verificações cumulativas anteriores.
Lua e Python não precisam ser instalados pelo jogador.

## Nintendo Switch

Extraia `Oracles-Switch-Lua-Phase7.zip` e copie `switch` à raiz do SD.
ROM: `sdmc:/switch/oracle-ages-lua-phase7/ages.gbc`.
Abra `oracles-lua-phase7.nro` em modo aplicação com memória completa, conforme
`SWITCH.md`. Título **Oracle of Ages - Lua phase 7**, ícone **recomp**.

**X** inicia replay curto com recargas; **Y**, completo; **Plus**, sair/cancelar.
Mantenha os três arquivos de `tests`. No completo esperar `status=PASS`,
`checkpoints=4825`, `expected=4825` e recargas positivas no `replay-result.txt`.
O NRO executa o replay; os fixtures dirigidos são executáveis do desktop.

**A** inicia o jogo; **L + R** abre/fecha o menu; **ZL** faz quicksave e **ZR**
quickload. Perfil e logs ficam na pasta separada da fase 7.

## Roteiro manual de combate e regressão

Teste Octoroks antes/depois da aproximação, disparo, parede, buraco e scent seed.
Confira também Keese, Rope, Moblin, Stalfos, inimigos com escudo e voadores,
múltiplos inimigos, dano, knockback, morte, drops e projéteis.

Teste fases/partes dos chefes de Ages, em especial Smog, Octogon, Ramrock e seus
braços, Veran e as lutas conectadas contra Twinrova/Ganon. Confira mudanças de
fase, criação de componentes, recompensas únicas e liberação de controles.
Os testes locais não equivalem a uma aprovação manual de todas essas lutas.

Faça save/load durante patrulha, ataque, dano e mudança de fase. Confira slots
1–4, SRAM, fechar/reabrir, trocar de sala, áudio, menu e suspensão/retomada.
No Switch, repita portátil/dock quando disponíveis. Refaça puzzles, portas,
lojas e eventos anteriores. Registre pacote, plataforma e cenas verificadas.

A execução destes novos pacotes no Windows e no Switch ainda precisa de
confirmação. A regressão completa de Seasons requer uma ROM própria.
