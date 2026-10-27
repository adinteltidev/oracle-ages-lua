# GitHub Actions: builds e releases

O workflow `CI` compila Windows x64, macOS Intel e Nintendo Switch, executa os
testes disponíveis sem ROM comercial e disponibiliza pacotes na seção Artifacts
da execução. Há disparos em pull requests, pushes em `main`, `port/**`, `ci/**`
e execução manual. CI antiga da mesma branch é cancelada quando há outra execução.

## Jobs e downloads

- Core: lint, testes dos helpers, build Linux e CTest, com relatório XML e resumo
  que distingue testes aprovados, falhos e omitidos.
- Windows: LLVM-MinGW UCRT 20261020 no Ubuntu, auditoria PE e ZIP; outro job em
  Windows 2022 executa UI, atalhos, CLI com ROM ausente e o launcher headless.
- macOS: runner Intel explícito, SDL3 vendored, testes, auditoria de assinatura
  ad-hoc/dependências e DMG. Não há notarização Apple.
- Switch: bibliotecas da imagem devkitPro fixada por digest, LLVM 23.1.4 com a mesma
  receita do port local, auditoria ELF/NRO, ZIP para SD e NRO separado.

Os artifacts `package-*` contêm os pacotes e manifestos `*-BUILD.json` do próprio
commit executado. Os artifacts de testes contêm apenas relatórios e executáveis
necessários ao smoke test; nenhum contém ROM comercial, cache de jogo ou SRAM.
Builds de CI ficam retidos por 7 dias; candidatos de release, por 30 dias.
Downloads do LLVM-MinGW e até 300 MB de objetos do núcleo Linux (ccache) são cacheados.
O SDK Switch completo não é mantido em cache.

Os manifestos históricos `SWITCH-VERIFICATION.json`/`WINDOWS-VERIFICATION.json`
na raiz não são usados como prova de uma nova build CI. Cada execução cria sua
própria evidência, identifica o commit, registra hashes dos pacotes e marca os
replays com ROM e testes físicos como não executados. No pacote Switch, o arquivo
`SWITCH-VERIFICATION.json` passa a conter o manifesto novo quando criado pela CI.

## Release em rascunho

O workflow `Release candidate` substitui o antigo gatilho automático por mudança
de `VERSION`. Alterar `VERSION` ou fazer push na `main` não publica uma release.

Para uma versão estável, depois de revisar e integrar os fontes na `main`:

```sh
git switch main
git pull --ff-only origin main
# Edite VERSION, faça commit/push e aguarde a CI antes de criar a tag.
git tag -a v1.0.3 -m "Oracle Ages Native 1.0.3"
git push origin v1.0.3
```

O exemplo pressupõe `VERSION` igual a `1.0.3`; use a versão real. A tag deve seguir
SemVer e seu commit deve estar contido na `main`. Tags inválidas ou divergentes de
`VERSION` são recusadas antes do build.

Quando o workflow estiver na branch padrão, a execução manual aceita candidatos
com sufixo, por exemplo `v1.0.2-rc.1` se `VERSION` for `1.0.2`:

```sh
gh workflow run release.yml --repo adinteltidev/oracle-ages-native --ref main -f tag=v1.0.2-rc.1
```

Após os builds e o smoke test Windows, a publicação baixa os mesmos artifacts da
execução, confere commit/hashes, gera `SHA256SUMS.txt` e cria **somente um draft**.
Ele não é marcado como latest. Uma release existente não é sobrescrita.
O fluxo básico usa o `GITHUB_TOKEN`; não precisa de PAT ou secrets de ROM.

Baixe esse candidato e teste o NRO no aparelho (incluindo X/Y, ZL/ZR e SRAM),
guardando os logs junto ao hash do executável. Publique o draft pela interface do
GitHub quando a versão estiver validada. Não recompile os pacotes entre teste e
publicação. Não há instalação automática no console ou publicação na eShop.

## Ativação e permissões

Antes da implementação, Actions estava desativado no repositório. Habilite em
Settings → Actions → General quando necessário. O workflow de release herdado
pode permanecer desativado enquanto este PR não for integrado, para impedir
que o arquivo antigo na `main` publique automaticamente. Após o merge, habilite
`release.yml` na aba Actions ou com:

```sh
gh workflow enable release.yml --repo adinteltidev/oracle-ages-native
```

Os jobs de build recebem apenas `contents: read`. Só o job que cria o draft
recebe `contents: write`. As actions são fixadas por commit SHA. PRs não recebem
secrets e não fazem publicação; `pull_request_target` não é utilizado.
O workflow reutilizável `build-platforms.yml` é compartilhado por CI e release.

## Verificação local

```sh
python3 -m unittest discover -s tests/ci -v
python3 tools/lint_game.py
actionlint .github/workflows/*.yml
bash -n tools/ci/build.sh tools/ci/create_draft.sh
```

O arquivo de fontes usa os blobs do commit, omite ROMs compiladas e registra
os commits dos submódulos de referência em `REFERENCE-SUBMODULES.json`, sem
exigir seu checkout. Esses submódulos não são necessários ao build nativo.

Os scripts de build também podem ser usados localmente, com CMake/Ninja e
compiladores apropriados no PATH: `bash tools/ci/build.sh switch|windows|macos`.
O build Switch extrai um SDK grande em `.tools/`; confira o espaço livre.
A variante GCC Switch é uma evolução separada: esta CI preserva o caminho LLVM
que foi usado na build local e não declara equivalência a um NRO GCC não testado.
