# Validação do jogador — fase 4

Data do registro: 30/10/2026. Branch `port/lua-gameplay`.

O jogador informou nesta conversa:

> A pouco eu validei as versoes de windows e switch e estao funcionando e estaveis

A confirmação se refere, pelo contexto, às versões C + Lua da fase 4 entregues
nesta conversa, que também incluem as fases anteriores.

| Plataforma | Resultado informado | Origem |
| --- | --- | --- |
| Windows | Funcionando e estável | Validação manual relatada pelo jogador |
| Nintendo Switch | Funcionando e estável | Validação manual relatada pelo jogador |

A pendência de validação manual dessas duas plataformas está atendida. O relato
não especifica versões do sistema, hashes dos arquivos executados, cenas
individuais ou execução dos replays automáticos; esses resultados específicos
não foram presumidos. Os testes automatizados locais continuam documentados em
[LUA-PHASE-4.md](LUA-PHASE-4.md).

Permanece pendente a regressão completa de Seasons com uma ROM própria, conforme
[LUA-MIGRATION-PHASES.md](LUA-MIGRATION-PHASES.md). A implementação da fase 4 está
concluída, mas a validação comum da migração ainda tem essa pendência.

Os ZIPs/DMG já entregues não foram reconstruídos ou alterados: preservam-se os
pacotes aos quais o relato se refere. Seus manifestos registram as verificações
existentes no momento da geração. Este documento acrescenta a confirmação
posterior, sem atribuí-la ao CI nem a novas execuções de testes pelo agente.
