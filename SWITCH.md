# Nintendo Switch — port C experimental

Alvo: Nintendo Switch original, ARM64, via homebrew `.nro`. Branch `port/switch`,
base `a828c7665bf349454eae9684299a3f79c11ac41d`. O núcleo continua nativo em C;
o alvo não compila `src/core/cpu.c`. A interface usa SDL2 e libnx, preservando
os alvos SDL3 de desktop. O C++ vem das bibliotecas gráficas do SDK, não de uma
reescrita da lógica do jogo.

## Estado e limites

Implementados: build separado, pilhas alinhadas das fibras com sentinela,
vídeo com escala inteira, PCM estéreo 48 kHz, controles, importação de ROM
validada por SHA1, SRAM, quicksave/reload em ponto seguro e executor de replay
com comparação estrita de checkpoints e reloads periódicos.

A sentinela detecta sobrescrita na volta da fibra; não equivale a uma página
protegida. Cada uma das quatro pilhas conserva 1 MiB. A montagem AArch64 é a
existente na base; a alocação foi separada de `mmap`/`mprotect` para este alvo.
O formato de estado mantém suas checagens de versão/tamanho; assertions no
build verificam tamanho de GB/APU e offsets selecionados contra a base desktop.
A compatibilidade funcional ARM64 ainda precisa dos replays no aparelho.

**O usuário confirmou que a build anterior à correção de quicksave funcionou no Nintendo Switch.** Esse relato
confirma a execução no console, mas não detalha os replays, desempenho, áudio,
controles, HOME/suspensão ou persistência. Os testes específicos abaixo
continuam pendentes de resultados e logs.
A interface ainda não tem seletor arbitrário de ROM, configuração visual,
remapeamento de controles. O quicksave/quickload agora mostram mensagens na tela. Ela aceita
`sdmc:/switch/oracle-ages-native/ages.gbc`; mensagens de erro ficam no console/log.
Mods Lua, sincronização HTTP, atualizações, widescreen e xBRZ ficam fora do alvo.
O primeiro teste deve usar memória completa de aplicação; modo Album não foi
validado. A SRAM é gravada a cada 600 frames, na perda de foco quando notificada
e na saída normal. Suspensão e substituição de arquivos no SD exigem testes reais.

## Build com o SDK devkitPro instalado

Instale o grupo `switch-dev`, `switch-sdl2` e suas dependências pelo gerenciador
oficial devkitPro. São necessários CMake >= 3.25 e o SDK apropriado ao sistema
host. Depois, na raiz do projeto:

```sh
export DEVKITPRO=/opt/devkitpro
sh tools/build_switch.sh
```

O script usa `cmake/Switch.cmake`, devkitA64 GCC, libnx e os packagers oficiais.
Saída: `build-switch/oracles-switch.nro`. Este caminho está configurado, mas
não foi executado com GCC neste Mac.

## Build LLVM utilizado neste Mac

O endpoint de pacotes `pkg.devkitpro.org` retornou HTTP 403. A alternativa usada
foi extrair as bibliotecas alvo da imagem oficial
`devkitpro/devkita64@sha256:82575ea78651b530b2e232bb3799cfd1fe331514e053d5f724bb4b28191fb79d`
e compilar com LLVM nativo do host. Os compiladores Linux dentro da imagem
não são executados no macOS. `platform/switch/sdk-lock.json` registra os digests,
versões e hashes das bibliotecas. Esse caminho LLVM deve ser considerado
experimental até ser executado no hardware.

```sh
python3 tools/fetch_switch_sdk.py --destination ../sdk --downloads ../sdk-downloads
python3 tools/fetch_switch_host_tools.py --destination ../host-tools
export DEVKITPRO="$(cd ../sdk/opt/devkitpro && pwd)"
export ORACLES_SWITCH_TOOLS="$(cd ../host-tools && pwd)"
export ORACLES_LLVM=/caminho/para/llvm/bin
sh tools/build_switch_llvm.sh
python3 tools/audit_switch_binary.py build-switch-llvm \
  --llvm-bin "$ORACLES_LLVM" --output build-switch-llvm/audit.json
python3 tools/package_switch.py build-switch-llvm Oracles-Switch-experimental.zip \
  --audit build-switch-llvm/audit.json
```

LLVM deve incluir clang, clang++, LLD, llvm-ar, llvm-ranlib e llvm-nm. O build
local usa LLVM 23.1.4 do toolchain LLVM-MinGW já instalado, com configuração
Windows desabilitada explicitamente e alvo `aarch64-none-elf`.
A configuração usa TLS emulado para o código compilado por Clang, pois o
`-mtp=soft` do GCC devkitA64 não é aceito pelo Clang. As bibliotecas do SDK
conservam sua própria ABI; `libgcc` fornece o suporte de TLS emulado.
O caminho LLVM seleciona as variantes PIC do runtime e inclui `libsysbase`.
O script de link libnx recebe um cabeçalho ELF PT_TLS exigido pelo LLD,
conservando os símbolos de inicialização da libnx. São emitidas relocations
RELA relativas, auditadas antes do empacotamento. Esses pontos também
precisam de validação no console.

SDK observado: devkitA64 r29.2, GCC 15.2.0, libnx 4.12.0, SDL2 2.28.5,
Mesa 20.1.0, libdrm_nouveau 1.0.1, switch-tools 1.13.1, switch-cmake 1.5.2.
A extração pode consumir vários GB; ela fica fora dos fontes.

## Verificações realizadas no host

O replay Ages no Mac passou em 20.000 frames com 333 checkpoints iguais à
referência, tanto na execução normal quanto com reloads periódicos, usando
a nova alocação das pilhas com sentinela (`ORACLES_SWITCH_FIBERS=1`).
Esse teste executa a montagem x86-64 do Mac; não executa a montagem ARM64
no Switch. O build ARM64 verifica os tamanhos e offsets selecionados de
GB/APU com assertions estáticas. A auditoria ELF/NRO verifica segmentos,
entrada, MOD0, metadados, ausência de imports dinâmicos obrigatórios,
relocations relativas e ausência de interpretador/ROM embutida.

## Uso e validação

Consulte `LEIA-ME-SWITCH.txt` para instalação, controles e arquivos no SD.
O pacote não inclui a ROM nem dados/saves do usuário. Os fixtures de teste
já versionados ficam em `tests/` no pacote; não são aplicados à partida normal.

Execute X para 20.000 frames / 333 checkpoints e depois Y para o replay
completo / 4.825 checkpoints. O executor compara cada frame/hash esperado,
rejeita referência vazia/truncada, verifica término e testa reloads periódicos.
Os testes usam SRAM isolada em memória e `replay-reload.state`, sem tocar
`quick.state` ou `sram.sav`. `+` cancela; cancelamento não é PASS.
Preserve `replay-result.txt`, `replay-observed.txt` e `runtime.log`, junto ao
modelo do console e ambiente de execução. Logs são substituídos no próximo teste.

Ainda necessários para aceitar o port: ambos os replays no aparelho,
partida manual, colisão/movimento, SRAM entre processos, quicksave/reload,
HOME/suspensão, reconexão de controles, SD sem espaço e sessão de 30 minutos
em portátil/dock. Compilar ou passar testes no Mac não substitui esses itens.

## Fontes técnicas e bibliotecas

- [libnx](https://github.com/switchbrew/libnx) e [exemplos](https://github.com/switchbrew/switch-examples).
- [SDL2 Switch](https://github.com/devkitPro/pacman-packages/tree/master/switch/SDL2).
- [CMake Switch](https://github.com/devkitPro/pacman-packages/tree/master/cmake/switch).
- [switch-tools](https://github.com/switchbrew/switch-tools).
- [Imagem devkitA64](https://hub.docker.com/r/devkitpro/devkita64).
- [Mesa Switch](https://github.com/devkitPro/pacman-packages/tree/master/switch/mesa),
  [libdrm_nouveau](https://github.com/devkitPro/libdrm_nouveau),
  [buildscripts GCC/newlib](https://github.com/devkitPro/buildscripts).
- O [port SDL3 oficial de Switch](https://wiki.libsdl.org/SDL3/README-switch)
  requer o acesso sob NDA indicado pelo projeto SDL; ele não é usado neste alvo.

Licenças: projeto GPLv3; SDL2 zlib; libnx ISC; Mesa/libdrm com avisos permissivos;
newlib com avisos próprios; runtime GCC/libstdc++ com GCC Runtime Library
Exception. Veja `platform/switch/licenses/` e as receitas/fontes upstream.
O ícone geométrico e seu SVG foram criados para este port e não usam arte do jogo.

## Correção do quicksave no SD

O usuário relatou que o ZL não salvava. A versão corrigida remove a dependência
de `rename` substituir um destino existente: grava e sincroniza um temporário,
move a versão anterior para `.bak` e instala a nova. Uma falha tenta restaurar
a anterior; se necessário, a próxima carga/inicialização recupera o backup
quando o arquivo principal estiver ausente. A mesma rotina protege a SRAM.
Esta sequência é recuperável, mas não é uma operação atômica contra perda de energia.

ZL mostra `SALVANDO...`, libera momentaneamente a entrada do jogo e aguarda
um frame seguro. Ao terminar mostra `ESTADO SALVO` ou `ERRO AO SALVAR`.
A espera termina após 600 frames se o jogo não chegar a um ponto seguro.
ZR mostra o resultado da carga e não aguarda que a cena atual estacione suas
fibras; a carga é feita no limite de frame do host, descartando as fibras antigas.
Os erros ficam em `runtime.log`, com errno e descrição.

A regressão `switch_storage` usa arquivos reais com rename sem sobrescrita
e falhas injetadas: primeiro save, saves repetidos, rollback, recuperação e
falha ao abrir temporário. `switch_quicksave` exercita um estado real de Ages,
sobrescreve o slot e verifica 120 frames idênticos após a carga, além da recuperação.
Esses testes são de host; a nova build ainda precisa de teste de ZL/ZR no console.
