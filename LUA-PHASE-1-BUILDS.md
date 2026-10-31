# Fase 1: pacotes de teste para Windows e Nintendo Switch

Data: 29/10/2026. Branch: `port/lua-gameplay`. Commit-base: `f8eb6b2`.
Esta continuação prepara os builds experimentais com as alterações locais
descritas abaixo. Não representa uma release publicada nem encerra a fase 1.

## Comportamento e reprodução

Os dois builds ativam `ORACLES_LUA_GAMEPLAY=ON` e
`ORACLES_LUA_BLOCK_PUZZLES=ON`. As opções continuam OFF por padrão.
A identidade das fontes Lua é
`6e6f9d18d78f8759ee04f90578c48cd5cc6daf2e4b70f501e0f3259c18aeaf12`.

- Perfil padrão separado no desktop: `oracles-decomp/oracles-lua-phase1` dentro
  do diretório de preferências do SDL; `--cache` pode substituí-lo.
- Perfil do Switch: `sdmc:/switch/oracle-ages-lua-phase1/`. Título do NRO:
  `Oracle of Ages - Lua phase 1`.
- Logs de execução registram identidade Lua, cobertura dos quatro estados de
  blocos e uso de memória. O replay do Switch registra a cobertura no resultado.
- As recargas periódicas do Switch e CLI esperam a fila de amostras esvaziar,
  preservando os checkpoints que o load descartaria. A mesma condição já havia
  sido validada no harness de replay da rodada anterior.
- O verificador Ages aceita `--rom`, `--tas-dir`, `--frames` e `--run`.
  Os caminhos padrão continuam disponíveis; argumentos permitem mover o pacote
  para outro computador. O SHA1 da ROM Ages é conferido antes de usá-la.
- Os executáveis auxiliares do Windows incluem o manifesto UTF-8. O pacote
  contém iniciadores `.cmd` para o jogo e testes, com logs no perfil experimental.

Os scripts existentes `tools/build_windows_llvm.sh` e
`tools/build_switch_llvm.sh` geram os binários. O empacotador
`tools/package_lua_phase1.py` exige os dois builds e suas auditorias, confere
flags, hashes, identificação dos módulos e vínculo real da interação de blocos
com Lua. Ele acrescenta fontes correspondentes da árvore de trabalho, licenças,
fixtures de teste e checksums. ROM e saves pessoais não entram nos pacotes.

`BUILD.json` identifica os binários e as fontes empacotadas; `WORKTREE.json`
dentro das fontes registra o commit-base e o hash de cada arquivo, incluindo
alterações ainda não commitadas. Os hashes dos ZIPs ficam no manifesto externo
`oracle-lua-phase1-builds.json`.

## Validação desta continuação

- macOS Intel: app recompilado; teste `app_menus_ages` aprovado (45,33 s).
- Verificador portátil executado fora da árvore de fontes, com fixtures em um
  caminho contendo espaços e acento. Replay normal e com recargas aprovados:
  **333 checkpoints em cada**, zero pulados. Os argumentos prevaleceram sobre
  variáveis de ambiente de teste. Fixture ausente retornou 77, conforme esperado.
- CLI nativa macOS recompilada: `--reload-every 1013` em 20.000 frames gerou
  os mesmos **333 checkpoints**, comparados integralmente com a referência.
- A cobertura de blocos do replay curto pode ser zero. A evidência positiva e o
  replay completo da rodada anterior estão em
  [LUA-PHASE-1-VALIDATION.md](LUA-PHASE-1-VALIDATION.md).
- Switch ARM64: compilação e auditoria estrutural aprovadas; título e caminho
  experimental confirmados no NRO. Sem interpretador de CPU GB e sem cabeçalho
  de ROM incorporado. Os símbolos indefinidos são somente os fracos previstos
  pelo libnx.
- Windows x64: app, CLI, verificador de replay e teste Lua compilados.
  Auditoria PE32+ aprovada: aplicativo gráfico nativo, sem CLR, com ícone e
  manifesto UTF-8; somente DLLs de sistema na tabela de importação.

Os testes desta continuação não medem desempenho durante compilação concorrente.
A execução em Windows real/VM e em Switch físico ainda deve ser realizada.
As instruções e os resultados esperados estão em
[LUA-PHASE-1-PLATFORM-TESTS.md](LUA-PHASE-1-PLATFORM-TESTS.md).
