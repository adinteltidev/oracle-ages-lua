# Testar C + Lua — fase 5, itens e lojas

30/10/2026. Branch `port/lua-gameplay`, base `fb4c18a`, alterações locais.
As seis opções cumulativas de Lua estão ON; Lua está incorporado.
SHA256 dos scripts: `4965b8fa088ab9116c3f3ecc1a336e2a799f8e562ba567fdde9f991011dadf23`.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase5-Intel.dmg`, copie **Oracles Lua Phase5.app** e
importe sua ROM de Ages USA/Australia. Perfil privado:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase5/`.
Assinatura ad hoc, sem notarização. Fontes e licenças acompanham o DMG.

## Windows x64

Extraia `Oracles-Windows-Lua-Phase5.zip`, abra a pasta homônima e execute
`Iniciar-Oracles-Lua.cmd`. Executável: `Oracles-Lua-Fase5.exe`.
Perfil: `%APPDATA%\oracles-decomp\oracles-lua-phase5\`.

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

O primeiro comando roda contratos, cenas dirigidas e replay curto; o segundo
executa os dois replays completos. Esperar `PASS`. Logs na subpasta `validation`
do perfil, incluindo `items-shops-contract.log` e `items-shops-scenes.log`.
O replay completo compara 4.825 checkpoints por modo e zero pulados.
Exit 77 indica teste não executado. Não é necessário instalar Lua ou Python.

## Nintendo Switch

Extraia `Oracles-Switch-Lua-Phase5.zip` e copie `switch` à raiz do SD.
Coloque sua ROM em `sdmc:/switch/oracle-ages-lua-phase5/ages.gbc` e abra
`oracles-lua-phase5.nro` em modo aplicação com memória completa, conforme
`SWITCH.md`. Título **Oracle of Ages - Lua phase 5**, ícone **recomp**.

**X** inicia replay curto com recargas; **Y**, completo; **Plus**, sair/cancelar.
Mantenha os três arquivos da subpasta `tests`. No completo, esperar `status=PASS`,
`checkpoints=4825`, `expected=4825` e recargas positivas em `replay-result.txt`.
O Switch não roda os fixtures dirigidos do desktop.

**A** inicia o jogo; **L + R** abre/fecha o menu; **ZL** faz quicksave e **ZR**
quickload. Teste os slots e suspensão/retomada. Logs ficam no perfil da fase 5.

## Roteiro manual

- Lojas: cancelar, faltar dinheiro, saldo exato, saldo sobrando, item já possuído,
  bombas/sementes no limite, upgrades de escudo/caixa de anéis e estoque renovado.
- Tokay: devolver/trocar pá, pena e bracelete, pagar dez sementes, faltar sementes,
  recusar a troca e comprar escudo. Conferir recurso entregue e flags de compra.
- Coletar rúpias, saúde, sementes, anéis, upgrades e recompensa única. Reentrar
  na sala e verificar que a recompensa única não pode ser obtida novamente.
- Quicksave/quickload antes da confirmação, durante a entrega/texto e depois da
  compra. O saldo e o inventário devem ser os mesmos e sem duplicação de entrega.
- Repetir menu, controles, áudio e os puzzles de blocos/cubo/lava/portas anteriores.

A confirmação anterior do jogador é da fase 4. A execução destes novos pacotes
no Windows e no Switch ainda precisa de confirmação. Seasons completo continua
pendente de ROM própria; essas pendências não são registradas como testes feitos.
