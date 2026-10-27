# Port Windows x64 em C

Base: `b670ec74a325d58d2d41abed12719a8bd710423c`, o port Mac Intel validado.
A branch `port/windows-x64` mantém o núcleo e a lógica em C. O scaler xBRZ
continua sendo a dependência C++ que já existia. A plataforma usa SDL 3.4.16 estática.
Não há interpretador de CPU na biblioteca nativa nem ROM embutida no executável.

O destino é **Windows 11 x64**. Os testes deste trabalho usam o PE Windows sob
Wine 11.0 no macOS Intel; isso não certifica Explorer, DPI, dispositivos ou drivers
de uma máquina Windows real. O pacote ainda precisa desse gate de aceitação.

## Construção no Mac

São necessários Python 3, CMake 3.25+ e Ninja. O toolchain autocontido escolhido é
[LLVM-MinGW 20261020](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261020),
Clang/LLD 23.1.4, UCRT, x64 Windows. O arquivo macOS universal é fixado pelo SHA-256
`e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e`.
Essa rota usa MinGW com LLVM; o toolchain GCC upstream continua disponível.

```sh
export ORACLES_LLVM_MINGW="$(python3 tools/fetch_windows_toolchain.py .tools)"
tools/build_windows_llvm.sh
python3 tools/audit_windows_binary.py build-win-x64/Oracles.exe \
  --llvm-bin "$ORACLES_LLVM_MINGW/bin" --output build-win-x64/pe-audit.json
```

O script compila somente os alvos nativos e os testes selecionados. `ORACLES_CMAKE`,
`ORACLES_NINJA`, `ORACLES_JOBS` e `ORACLES_SDL_SOURCE` permitem usar ferramentas e
fontes já preparadas. Na ausência da última, CMake baixa a SDL 3.4.16 com o hash
fixado em `CMakeLists.txt`. Nenhum cache CMake do Mac deve ser reutilizado no Windows.

`Oracles.exe` é o produto gráfico. `oracles-native-run.exe` e os testes são
ferramentas de verificação; o executável com interpretador não é o produto.
O verificador de biblioteca aceita `--ar` para usar `llvm-ar` do toolchain e
reconhece objetos `.obj` do Windows, além de `.o`.

## Testes sob Wine

O [Wine macOS 11.0_1](https://github.com/Gcenx/macOS_Wine_builds/releases/tag/11.0_1)
usado localmente fica fora do projeto. Seu arquivo tem SHA-256
`b50dc50ec7f41d58b115a6b685d4d1315ba3c797bd3aa0f49213f2703cb82388`.
Use um prefixo privado e mantenha a ROM em `roms/`, que é ignorado pelo Git.

```sh
export WINEPREFIX="$PWD/.wine-prefix"
export ORACLES_WINE='/caminho/Wine Stable.app/Contents/Resources/wine/bin/wine'
export ORACLES_WINDOWS_SOURCE_ROOT="Z:$PWD"
tools/build_windows_llvm.sh
python3 tools/validate_windows_core.py build-win-x64 \
  'roms/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc' \
  'build-win-x64/validação João' --executor "$ORACLES_WINE" --wine-paths --full-replay
```

A unidade Ages compara 333 checkpoints em 20.000 frames e repete o trecho com
recargas de estado. O cenário de movimento alcança naturalmente a sala 00:8A,
X=72→91, bloqueio em X=124, Y=56 e vida 12. O replay completo exige todos os
4.825 checkpoints de `tas/ages.ref`; hashes adicionais são contabilizados à parte.
Retorno 77 de um teste significa ausência de uma entrada, não aprovação.

O layout selecionado do estado bruto v1 é conferido por `tools/state_layout.c`.
Mac e Windows apresentaram GB=194.656 bytes, Apu=65.788 bytes e os mesmos offsets
verificados. O header do estado de boot original é mantido e validado; não foi
afrouxado para aceitar um arquivo incompatível. A compatibilidade funcional ainda
depende dos replays e dos testes de recarga, não apenas de `sizeof`.

Para a interface, `tests/test_app_ages_resume.py` aceita `--executor`, `--wine-paths`
e `--check-shortcuts`. `ORACLES_TEST_HEADLESS=1` aplica os drivers dummy dentro do
processo e descarta PCM na saída do teste, evitando foco e espera pelo dispositivo.
O [Wine filtra as variáveis SDL de vídeo/áudio do host](https://github.com/wine-mirror/wine/blob/wine-11.0/dlls/ntdll/unix/env.c),
por isso os hints explícitos são necessários. Esse teste não certifica áudio real.
É necessário o módulo `pypng` no Python de teste. O cenário
reabre uma sala naturalmente alcançada em dois processos, verifica SRAM e testa
Ctrl+S/Ctrl+R. Somente nesse teste controlado, a vida é temporariamente alterada
para 10; carregar o slot deve restaurar 12. A configuração `--shortcut-modifier Cmd`
permite exercitar o comportamento Mac. ROM, cache, estados e capturas ficam locais.

## Ajustes de plataforma

- Ctrl+S/Ctrl+R no Windows; Cmd+S/Cmd+R preservados no Mac. F11 em ambos, Cmd+F só no Mac.
- Arquivo temporário exclusivo via `GetTempPathW`/`GetTempFileNameW` ou `mkstemp`,
  com limpeza após o teste. O headless também deixou de depender de `/tmp`.
- Manifesto UTF-8 nos executáveis, inclusive testes/headless. O alvo Windows 11
  atende à API utilizada. Caminhos com acentos e espaços são casos de verificação.
- Auditoria PE32+ x64: subsistema gráfico, ícone/manifesto, ausência de CLR,
  imports de sistema e nenhuma DLL privada de SDL ou runtime requerida.

## Empacotamento e gate Windows

```sh
python3 tools/package_windows.py build-win-x64/Oracles.exe Oracles-Windows-x64.zip \
  --audit build-win-x64/pe-audit.json --sdl-source /caminho/SDL3-3.4.16 \
  --llvm-root "$ORACLES_LLVM_MINGW"
```

O ZIP contém aplicativo, guia em português e licenças, sem ROM, cache ou saves.
Os fontes e evidências são entregues separadamente. No Windows preparado, também
é possível compilar via MSYS2 UCRT64 com o CMake upstream e os testes selecionados.
Não misture objetos e bibliotecas de CRTs diferentes.

Antes de declarar validação Windows, testar o pacote extraído fora da árvore de
desenvolvimento, sem compilador no PATH: abertura pelo Explorer, importação pelo
seletor, caminhos Unicode, DPI 100%/150%, F11/foco/minimização, teclado/controle,
áudio real e salvamento após fechar/reabrir. Seasons, campanha manual completa,
sincronização externa e mods de terceiros não são atestados por esta entrega.
