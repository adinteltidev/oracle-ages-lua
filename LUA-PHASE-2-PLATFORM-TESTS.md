# Testar C + Lua — fase 2, cubo colorido

30/10/2026. Branch `port/lua-gameplay`, base `5829dad` com alterações locais.
As opções de Lua, puzzles com blocos e chama do cubo estão ON. Lua incorporado,
sem instalação adicional. A versão experimental usa perfil próprio da fase 2.

Identidade das fontes Lua:
`9281cb160e0ea7bc404f3b775cf0d35341af9bab17c6e96531d3e38036aecef8`.

## macOS Intel

Abra `Oracles-macOS-Lua-Phase2-Intel.dmg` e copie **Oracles Lua Phase2.app**.
Importe sua ROM de Ages USA/Australia pelo menu. Perfil padrão:
`~/Library/Application Support/oracles-decomp/oracles-lua-phase2/`.
Assinatura ad hoc; sem notarização. Fontes, licenças e `BUILD.json` acompanham o DMG.

## Windows x64

1. Extraia `Oracles-Windows-Lua-Phase2.zip` inteiro.
2. Entre na pasta `Oracles-Windows-Lua-Phase2` e execute `Iniciar-Oracles-Lua.cmd`.
3. Importe sua ROM e teste controles, áudio, menu, quatro slots e retomada.

Executável: `Oracles-Lua-Fase2.exe`. Dados/log em
`%APPDATA%\oracles-decomp\oracles-lua-phase2\`.

Para os contratos, cenas dirigidas e replay curto:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
```

Para os replays completos, normal e com recargas:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

Esperar `PASS`. Os testes dirigidos fazem 2.180 comparações e o contrato novo
verifica 768 entradas. O replay completo deve comparar 4.825 checkpoints por
modo, zero pulados; o curto compara 333 por modo. Exit 77 é teste não executado.
Logs ficam na subpasta `validation`: `colored-cube-contract.log`,
`colored-cube-scenes.log`, `replay.log`, além dos contratos anteriores.

O teste local de 250 cores inesperadas usa subprocessos e Python; não é chamado
pelo `.cmd`. O contrato de todos os bytes e as cenas válidas não exigem Python.

## Nintendo Switch

1. Extraia `Oracles-Switch-Lua-Phase2.zip` e copie `switch` para a raiz do SD.
2. Coloque sua ROM em `sdmc:/switch/oracle-ages-lua-phase2/ages.gbc`.
3. Abra `oracles-lua-phase2.nro` em modo aplicação com memória completa, conforme
   `SWITCH.md`. O título é **Oracle of Ages - Lua phase 2**.
4. **X** inicia o replay curto com recargas; **Y**, o completo. **Plus** cancela/sai.
   Conserve os três arquivos da subpasta `tests`.
5. **A** inicia o jogo. **L + R** abre/fecha o menu; **ZL** faz quicksave;
   **ZR**, quickload. Teste também os quatro slots e retomada após suspensão.

Arquivos para conferir em `sdmc:/switch/oracle-ages-lua-phase2/`:
`runtime.log`, `replay-result.txt`, `replay-observed.txt`.
No completo, esperar `status=PASS`, `checkpoints=4825`, `expected=4825` e reloads
positivos. O NRO não executa os fixtures dirigidos do desktop.

## Cenas e critérios

Nas salas da chama do cubo, teste as três cores, tochas apagadas/acesas, mover o
cubo, sair e voltar à sala, salvar/carregar, fechar/reabrir e suspender/retomar.
No Switch, repita em portátil/dock quando disponíveis. Confira a chama, cores,
animação, áudio e os outros elementos desenhados sobre o cenário.

Os logs mostram `ages/colored_cube.present`, `.visible`, `.palette_offset`.
O replay natural pode ter **zero chamadas dessas regras**: isso não comprova a
chama. Use as cenas dirigidas e uma partida real para a cobertura específica.

Registre plataforma, pacote/build, resultado, modo portátil/dock e logs. Builds
e auditorias locais não confirmam execução no Windows/Switch. A aceitação dessas
plataformas e a regressão completa de Seasons continuam pendentes.

ZIPs incluem `BUILD.json`, `SHA256SUMS.txt`, fontes em `Sources/` e licenças.
O DMG também traz fontes e manifesto. Nenhum inclui ROM ou saves do jogador.
Os pacotes anteriores continuam disponíveis; não houve release automática.
