# Mod Corrida 2x

Implementação local de 01/11/2026, baseada no commit `5fa0d4b`.

O mod distribuído em `examples/mods/corrida` usa as novas funções Lua
`oracles.on_input` e `oracles.run`. São extensões aditivas da API 2; é necessário
usar os executáveis atualizados. A pasta do mod sozinha não atualiza o motor.

## Instalação e uso

Feche o jogo e instale o aplicativo atualizado. Copie a pasta `corrida` do ZIP
`Mod-Corrida-2x.zip` para a pasta de mods do perfil ativo. Reabra o jogo.

- macOS fase 7: `~/Library/Application Support/oracles-decomp/oracles-lua-phase7/mods/corrida/`.
- Windows fase 7: `%APPDATA%\oracles-decomp\oracles-lua-phase7\mods\corrida\`.
- Switch Ages fase 7: `sdmc:/switch/oracle-ages-lua-phase7/mods/corrida/`.
- Switch Seasons US: `sdmc:/switch/oracle-ages-lua-phase7/seasons/mods/corrida/`.
- Switch Seasons EU: `sdmc:/switch/oracle-ages-lua-phase7/seasons-eu/mods/corrida/`.

Modo portátil e `--cache` usam o respectivo diretório de dados, seguido de `mods/corrida`.

Segure **Shift esquerdo** ou o **ombro direito** no computador; use **L3**
(clique do analógico esquerdo) no Switch. No computador, Settings → Controls →
Run (mod) permite remapear. Soltar o botão restaura a velocidade normal. Ao
pausar no computador, a ação é limpa: solte e pressione novamente para correr.
Remover a pasta e reiniciar desinstala o mod.

A corrida dobra a velocidade normal do terreno: piso comum, grama e
escadas/trepadeiras. Pegasus não acumula e não há consumo de sementes. A tabela
original de velocidades mantém as direções, diagonais e colisões, incluindo seu
arredondamento para posições fracionárias. Gelo, água, saltos, montarias, recuo
por dano, menus, transições e visão lateral mantêm o comportamento original.

## Implementação

A ação foi adicionada ao fim da lista de controles para manter a compatibilidade
com configurações antigas. O Lua recebe `{run=boolean}` antes de cada frame de
jogo normal, sob o sandbox e os limites já existentes. Apenas mods API 2 com
`changes_game=yes` podem solicitar corrida. Pedidos de mods combinam por OR,
não multiplicação, e são condicionados ao botão físico pressionado. Um callback
que falha não contribui; sem mod, não há corrida. Escritas de RAM não são
permitidas nesse callback. Pausa, troca de jogo e encerramento limpam o efeito.

O motor aplica o pedido em `updateLinkSpeed_withParam_hook`, depois da escolha
da velocidade original e antes do movimento com colisões. O pedido não integra
o estado serializado do Game Boy nem altera os onze scripts internos de Lua.

## Validação

- Suíte completa do host de mods aprovada, incluindo permissões, erro após um
  pedido de corrida, botão solto e encerramento da sessão.
- Controles/UI aprovados após incluir a ação sem criar um botão de toque Android.
- Teste do aplicativo macOS com eventos SDL confirmou Shift pressionado/solto
  chegando ao Lua e solicitando corrida sem erro.
- Replays de Ages e Seasons com corrida desativada: 20.000 frames em cada modo
  normal/reload, 333 checkpoints por modo, zero skips e 19 recargas no modo reload.
- Adaptador de mods do Switch aprovado; verificações Python do CI aprovadas.
- Testes de deslocamento real e restauração de velocidade: resultados em
  `reports/run-mod/movement-final.xml` (ROMs US fornecidas pelo usuário).
  Ages percorreu 3 pixels andando e 6 correndo na mesma janela de seis frames;
  Seasons percorreu 5 e 10 pixels. Soltar o botão restaurou o índice de velocidade
  original nos dois jogos. Essas amostras dependem do terreno e da fase do frame.
- Builds macOS Intel, Windows x64 e Switch concluídas; binários auditados.

Os testes físicos de teclado/controle, diagonais/colisões em diferentes salas e
terrenos e o novo NRO em hardware Switch ainda precisam de validação manual.
A edição europeia não foi jogada nesta validação. Logs ficam no diretório
ignorado `reports/run-mod/`. Nenhuma ROM acompanha os pacotes.

## Correção da primeira versão macOS

O primeiro motor bloqueava a corrida quando o inventário estava desativado
(`wMenuDisabled`), mesmo em trechos com caminhada livre. Essa condição foi
removida; menus realmente abertos, diálogos e bloqueios de movimento continuam
impedindo a corrida. A regressão cobre inventário desativado e objetos bloqueados.
Uma leitura do estado salvo do jogador confirmou 6 pixels andando e 12 correndo,
com restauração da velocidade ao soltar o botão, sem gravar no save original.
O instalador corrigido é `Oracles-macOS-Lua-Phase7-Corrida-Fix.dmg`.
Após a correção, o usuário confirmou o funcionamento no macOS em
`/Applications/Oracles.app` usando Shift esquerdo. Essa confirmação cobre o uso
manual no Mac; não encerra a validação de controles, terrenos e outras plataformas.
