# Testar a fase 1 C + Lua no Windows e no Nintendo Switch

Build experimental baseado em `f8eb6b2`, com alterações locais de preparação dos
pacotes. As duas opções `ORACLES_LUA_GAMEPLAY` e `ORACLES_LUA_BLOCK_PUZZLES`
estão ativadas. Lua está incorporado; não é necessário instalá-lo.

Esta rodada prepara a validação em outras plataformas do primeiro lote dos
puzzles com blocos. A fase 1 continua em andamento. A execução real no Windows e
no Switch será confirmada pelos testes abaixo; compilação e auditoria de arquivos
são resultados separados.

## Windows x64

1. Extraia `Oracles-Windows-Lua-Phase1.zip` inteiro.
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

Para verificar o runtime Lua e um replay curto, abra o Prompt de Comando na pasta
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
%APPDATA%\oracles-decomp\oracles-lua-phase1\validation\replay.log
```

O verificador também pode ser chamado diretamente, sem Python ou PowerShell:

```bat
Tests\test_lua_gameplay.exe
Tests\test_native_ages.exe --rom "C:\Jogos\Ages.gbc" --tas-dir Tests --frames 289518 --run both
```

Exit 77 significa teste não executado por falta de arquivos. O iniciador de
validação trata saídas diferentes de zero como falha ou teste não executado.

## Nintendo Switch

1. Extraia `Oracles-Switch-Lua-Phase1.zip` e copie a pasta `switch` para a raiz do SD.
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
verificar os arquivos de cada ZIP. Ambos incluem fontes correspondentes em
`Sources/`, licenças e avisos das dependências. Não incluem ROM ou saves do jogador.

Os artefatos têm nomes específicos para a fase 1. As opções continuam OFF por
padrão no projeto. Os replays e medições já realizados no macOS estão em
`LUA-PHASE-1-VALIDATION.md`; os resultados em hardware devem ser registrados
separadamente, com plataforma, modo portátil/dock, build e logs.

Após testar, os logs acima permitem confirmar o resultado desta etapa e decidir
sobre os próximos lotes da migração. Não há commit, push ou publicação automática.
