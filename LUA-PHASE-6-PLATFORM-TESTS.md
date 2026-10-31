# Testar C + Lua — fase 6, NPCs e eventos

Branch `port/lua-gameplay`, base `40cf271`, alterações locais. Lua incorporado;
as sete opções cumulativas estão ON. Esta fase tem perfil próprio. A validação
física da fase 5 também permanece pendente, conforme o jogador informou.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase6-Intel.dmg`, copie **Oracles Lua Phase6.app** e
importe sua ROM de Ages USA/Australia. Perfil:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase6/`.
Assinatura ad hoc; sem notarização. Fontes e licenças acompanham o DMG.

## Windows x64

Extraia `Oracles-Windows-Lua-Phase6.zip` e execute `Iniciar-Oracles-Lua.cmd`
na pasta homônima. Executável `Oracles-Lua-Fase6.exe`. Perfil:
`%APPDATA%\oracles-decomp\oracles-lua-phase6\`.

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

O primeiro comando executa contratos, cenas e replay curto; o segundo usa os
replays completos. Esperar `PASS`. Logs na subpasta `validation` do perfil:
`npcs-events-contract.log`, `npcs-events-scenes.log`, `replay.log` e testes anteriores.
Exit 77 é teste não executado. Não é necessário instalar Lua ou Python.

## Nintendo Switch

Extraia `Oracles-Switch-Lua-Phase6.zip`; copie `switch` à raiz do SD. Coloque sua
ROM em `sdmc:/switch/oracle-ages-lua-phase6/ages.gbc`. Abra
`oracles-lua-phase6.nro` em modo aplicação com memória completa, conforme `SWITCH.md`.
Título **Oracle of Ages - Lua phase 6**, ícone **recomp**.

**X** inicia replay curto com recargas; **Y**, completo; **Plus**, sair/cancelar.
Mantenha os três arquivos de `tests`. No completo esperar `status=PASS`,
`checkpoints=4825`, `expected=4825` e recargas positivas no `replay-result.txt`.
O NRO não executa os fixtures dirigidos do desktop.

**A** inicia o jogo; **L + R** abre/fecha o menu; **ZL** faz quicksave e **ZR**
quickload. Perfil e logs ficam na pasta da fase 6.

## Roteiro manual

Converse repetidamente com a senhora do passado antes/depois de D2/D4 e de salvar
Nayru. Em Symmetry, teste moradores antes/depois do reparo da Tuni Nut, incluindo
repetir conversas e sair/voltar à sala. Confira escolhas afirmativa/negativa,
progresso em passado/presente, jogo conectado e textos após mudar flags.

Teste eventos de Nayru, Ralph, Impa, Maku, personagens ligados e a abertura da
torre negra/portão Maku. Confira que os controles voltam após as cenas e que
recompensas únicas não são entregues de novo ao conversar ou reentrar.

Faça save/load antes de escolher uma resposta, durante um texto/movimento de
cena, antes/depois de uma recompensa e após fechar/reabrir o aplicativo. Teste
slots 1–4, SRAM, áudio, menu e suspensão/retomada. No Switch, repita portátil/dock
quando disponíveis. Refaça também puzzles/lojas das fases anteriores.

A execução destes novos pacotes no Windows e Switch precisa de confirmação.
O relato anterior de estabilidade é da fase 4; as fases 5 e 6 não herdam essa
aprovação. Seasons completo continua pendente de ROM própria.
