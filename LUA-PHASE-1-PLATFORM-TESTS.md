# Testar a fase 1 C + Lua expandida

Atualização: 30/10/2026. Branch `port/lua-gameplay`, base `970dbe0` com alterações
locais. Inclui as 32 regras tipadas de `block_rules.lua`, além do gatilho de blocos
e dos interruptores anteriores. Ambas as opções Lua estão ON; Lua incorporado.

Fontes Lua SHA256:
`ad4a8fa264af3e1781d7a8710b6685ab3717b1c01fa06abf004a0dcb7bc4fe2a`.

A compilação/auditoria local passou; Windows e Switch **ainda não testados em
execução real**. A fase não tem aceitação final. [Evidências locais](LUA-PHASE-1-IMPLEMENTATION.md).

## macOS Intel

Abra `Oracles-macOS-Lua-Phase1-Expanded-Intel.dmg`, copie o app e importe sua ROM
pelo menu. O pacote usa o perfil `oracles-decomp/oracles-lua-phase1` do SDL, em
`~/Library/Application Support/`. Teste áudio, controles, quatro slots, retomada e
as salas com cubos, água, painéis e estátuas. Assinatura ad hoc; sem notarização.

## Windows x64

1. Extraia `Oracles-Windows-Lua-Phase1-Expanded.zip` inteiro.
2. Entre em `Oracles-Windows-Lua-Phase1` e execute `Iniciar-Oracles-Lua.cmd`.
3. Adicione sua ROM de Ages USA/Australia pelo menu e faça uma partida.
4. Teste controles, áudio, abertura do menu, os quatro slots e retomada após fechar.

O executável é `Oracles-Lua-Fase1.exe`. Os dados de teste ficam em:

```text
%APPDATA%\oracles-decomp\oracles-lua-phase1
```

Esse também é o perfil padrão do build com os blocos Lua ativados. `--cache`
continua permitindo um diretório explícito. O iniciador grava `runtime.log` nessa
pasta; o log inclui a identificação das fontes Lua e os contadores ao sair.

O validador executa o contrato de 32 regras e 7.673 comparações dirigidas contra
o C real, incluindo save/load e falhas, antes do replay. Para verificar o runtime
Lua e um replay curto, abra o Prompt de Comando na pasta
que contém os dois arquivos `.cmd` e execute, ajustando o caminho da sua ROM:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc"
```

Para os replays completos, normal e com recargas:

```bat
Validar-Lua.cmd "C:\Jogos\Ages.gbc" full
```

A verificação completa pode demorar vários minutos. O resultado deve ser `PASS`,
com **4.825 checkpoints em cada execução**, zero pulados e cobertura dos quatro
estados Lua. O teste curto deve comparar 333 checkpoints por execução; pode ter
cobertura zero dos blocos, pois não chega às cenas migradas.

Logs da verificação:

```text
%APPDATA%\oracles-decomp\oracles-lua-phase1\validation\lua-contract.log
%APPDATA%\oracles-decomp\oracles-lua-phase1\validation\block-rules-contract.log
%APPDATA%\oracles-decomp\oracles-lua-phase1\validation\block-rules-scenes.log
%APPDATA%\oracles-decomp\oracles-lua-phase1\validation\replay.log
```

O verificador também pode ser chamado diretamente, sem Python ou PowerShell:

```bat
Tests\test_lua_gameplay.exe
Tests\test_lua_block_rules.exe
Tests\test_lua_block_rules_rom.exe --rom "C:\Jogos\Ages.gbc" --tas-dir Tests
Tests\test_native_ages.exe --rom "C:\Jogos\Ages.gbc" --tas-dir Tests --frames 289518 --run both
```

Exit 77 significa teste não executado por falta de arquivos. O iniciador de
validação trata saídas diferentes de zero como falha ou teste não executado.

## Nintendo Switch

1. Extraia `Oracles-Switch-Lua-Phase1-Expanded.zip` e copie a pasta `switch` para a raiz do SD.
2. Coloque sua ROM de Ages USA/Australia neste caminho:

```text
sdmc:/switch/oracle-ages-lua-phase1/ages.gbc
```

3. Abra `oracles-lua-phase1.nro` pelo homebrew launcher que você já usa, em modo
   aplicação com memória completa, conforme `SWITCH.md`. Use os caminhos da fase 1
   listados aqui para este pacote.
4. No menu inicial, use **X** para o replay curto com recargas e **Y** para o
   replay completo. **Plus** cancela ou sai. Os arquivos da pasta `tests` precisam
   estar no SD para essas opções.
5. Use **A** para jogar. **L + R** abre/fecha o menu externo. Teste os quatro slots,
   o quicksave com **ZL**, o quickload com **ZR**, áudio, controles e retomada após
   suspensão, nos modos portátil e dock quando disponíveis.

O replay verifica checkpoints e recargas. A partida pelo botão A permite avaliar
imagem, áudio e controles. O NRO aparece como **Oracle of Ages - Lua phase 1**.
Os saves e logs desse build ficam em `oracle-ages-lua-phase1`.

Depois do replay, os arquivos para conferir são:

```text
sdmc:/switch/oracle-ages-lua-phase1/replay-result.txt
sdmc:/switch/oracle-ages-lua-phase1/replay-observed.txt
sdmc:/switch/oracle-ages-lua-phase1/runtime.log
```

No replay completo, esperar `status=PASS`, `checkpoints=4825`, `expected=4825`,
recargas positivas e contadores `lua_blocks` positivos nos quatro estados.
O código de recarga espera as amostras pendentes serem comparadas antes do load.

## Identificar os builds e as evidências

`BUILD.json` identifica o commit-base, alterações locais, hashes dos binários e
fontes Lua, as opções ativadas e os perfis de dados. `SHA256SUMS.txt` permite
verificar os arquivos de cada ZIP. Os ZIPs incluem fontes correspondentes em
`Sources/`, licenças e avisos das dependências. Não incluem ROM ou saves do jogador.

Os artefatos têm nomes específicos para a fase 1. As opções continuam OFF por
padrão no projeto. Os resultados do primeiro lote estão em
`LUA-PHASE-1-VALIDATION.md`; a rodada expandida está em
`LUA-PHASE-1-IMPLEMENTATION.md`; os resultados em hardware devem ser registrados
separadamente, com plataforma, modo portátil/dock, build e logs.

O replay natural não visita todas as regras: contadores zero nas demais não
indicam falha nem comprovam cobertura. Os testes dirigidos locais cobrem as 32;
o Switch não executa esses fixtures dirigidos no NRO. É preciso testar também
as salas migradas em uma partida real.

Após testar, os logs acima permitem confirmar o resultado desta etapa e decidir
sobre os próximos lotes da migração. Não há commit, push ou publicação automática.
