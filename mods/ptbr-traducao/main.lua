-- =============================================================
--  Mod: Traducao PT-BR — Oracle of Ages
--  Alterna entre Ingles e PT-BR com o botao SELECT (enquanto
--  uma caixa de texto estiver na tela).
--
--  Como adicionar traducoes:
--    1. Rode o jogo e abra um dialogo.
--    2. O ID do texto aparece no canto superior esquerdo (modo debug).
--    3. Adicione a entrada na tabela TEXTOS abaixo.
--
--  Limitacoes da fonte:
--    Apenas ASCII 0x20-0x7E e os dois glifos especiais (coracao,
--    cursor). Caracteres com acento NAO sao renderizados.
--    Convencoes usadas aqui:
--      a~ = a nasal (ex: "macan~" = "macann")
--      c, = c cedilha  (ex: "c,hao" = "chao")
--    Ou simplesmente use transliteracao direta.
-- =============================================================

-- ---- Enderecos de memoria (Ages) ----------------------------
-- Fonte: src/game/ram.h e src/game/gb.h
local W_TEXT_IS_ACTIVE   = 0xcba0  -- != 0 quando ha caixa de texto
local W_TEXT_INDEX_L     = 0xcba2  -- byte baixo do indice do dialogo
local W_TEXT_INDEX_H     = 0xcba3  -- byte alto  do indice do dialogo
local W_TEXTBOX_POS      = 0xcbac  -- 0 = caixa embaixo, 1 = caixa em cima
local W_KEYS_JUST_PRESSED = 0xc482 -- teclas recem pressionadas (borda de subida)
local SELECT_BIT          = 0x04   -- JOY_SELECT (src/game/gb.h)

-- ---- Cores RGB555 -------------------------------------------
local function rgb(r, g, b) return r + g * 32 + b * 1024 end
local COR_FUNDO   = rgb(0,  0,  8)   -- azul muito escuro (fundo do overlay)
local COR_TEXTO   = rgb(31, 31, 31)  -- branco
local COR_AVISO   = rgb(31, 27, 0)   -- amarelo
local COR_LABEL   = rgb(14, 14, 31)  -- azul claro (indicador de idioma)
local COR_BORDA   = rgb(10, 10, 20)  -- borda do fundo

-- ---- Estado persistente -------------------------------------
local ptbr_ativo = oracles.load("ptbr_ativo")
if ptbr_ativo == nil then ptbr_ativo = true end  -- PT-BR ligado por padrao

-- ---- Configuracoes visuais ----------------------------------
-- A caixa de texto do jogo ocupa as 3 linhas de baixo (y=96..143)
-- ou as 3 linhas de cima (y=16..47). Cada linha mede 16px de altura.
-- O overlay cobre exatamente essa area e escreve por cima.
local BOX_H     = 48   -- altura da caixa de texto original em pixels
local BOX_Y_BOT = 96   -- topo da caixa quando esta embaixo (pos=0)
local BOX_Y_TOP = 16   -- topo da caixa quando esta em cima  (pos=1)
local PAD       = 4    -- margem interna em pixels

-- ---- Tabela de traducoes ------------------------------------
-- Chave: (textIndexH * 256 + textIndexL) — ID de 16 bits do dialogo.
-- Use o modo debug (veja abaixo) para descobrir o ID de cada dialogo.
-- Cada entrada e uma lista de linhas (maximo ~18 chars por linha
-- para caber na largura padrao de 160px com gliphos de 8px).
--
-- NOTA: Adicione suas traducoes aqui! Os exemplos abaixo sao
-- ilustrativos — ajuste os IDs conforme o que voce vir no debug.
local TEXTOS = {
  -- Exemplo de formato:
  -- [0x0001] = { "LINHA 1 DO DIALOGO", "LINHA 2 DO DIALOGO" },

  -- Tela de abertura / introducao
  [0x0000] = { "ERA UMA VEZ..." },
  [0x0001] = { "LINK ACORDOU SENTINDO", "UM CHAMADO MISTERIOSO." },
  [0x0002] = { "NAVI: EI! ACORDA!" },

  -- Aldeoes e NPCs basicos
  [0x0010] = { "OLA! COMO VAI?" },
  [0x0011] = { "NAO SEI PARA ONDE", "LINKUS FOI." },
  [0x0012] = { "ESTA E A ALDEIA", "DE LYNNA." },

  -- Ralph / Nayru / personagens principais
  [0x0020] = { "RALPH: NAYRU!" },
  [0x0021] = { "NAYRU: O QUE FOI,", "RALPH?" },
  [0x0022] = { "ONOX: HAHAHA!" },

  -- Lojas
  [0x0100] = { "O QUE VOCE QUER?" },
  [0x0101] = { "COMPRAR?", "  SIM  NAO" },
  [0x0102] = { "VOLTE SEMPRE!" },

  -- Itens e tesouros
  [0x0200] = { "LINK ENCONTROU", "UMA ESPADA!" },
  [0x0201] = { "LINK OBTEVE UM", "ESCUDO!" },
  [0x0202] = { "LINK ENCONTROU", "UMA BOMBA!" },
  [0x0203] = { "LINK OBTEVE O", "BASTAO DE MAGICA!" },

  -- Essencias
  [0x0300] = { "VOCE OBTEVE A", "ESSENCIA DO TEMPO!" },
}

-- ---- Estado interno -----------------------------------------
local ultimo_id    = -1    -- ID do ultimo dialogo exibido (para debug)
local ultimo_ativo = false -- se havia texto ativo no frame anterior

-- ---- Funcao auxiliar: salva o estado do idioma --------------
local function salvar_estado()
  oracles.store("ptbr_ativo", ptbr_ativo)
end

-- ---- Handler principal (por frame) -------------------------
oracles.on_frame(function()
  local screen_w = oracles.screen()

  -- 1. Le o estado atual do jogo
  local ativo  = oracles.rd(W_TEXT_IS_ACTIVE)
  local id_l   = oracles.rd(W_TEXT_INDEX_L)
  local id_h   = oracles.rd(W_TEXT_INDEX_H)
  local pos    = oracles.rd(W_TEXTBOX_POS)       -- 0=baixo, 1=cima
  local teclas = oracles.rd(W_KEYS_JUST_PRESSED) -- borda de subida: nao precisa de anti-repeat

  -- 2. Detecta pressao de SELECT para alternar idioma (so quando ha texto ativo)
  if (teclas & SELECT_BIT) ~= 0 and ativo ~= 0 then
    ptbr_ativo = not ptbr_ativo
    salvar_estado()
    oracles.log("ptbr-traducao: idioma -> " .. (ptbr_ativo and "PT-BR" or "EN"))
  end

  -- 3. Indicador de idioma no canto superior direito (sempre visivel)
  local label = ptbr_ativo and "PT-BR" or "EN"
  local label_x = screen_w - (#label * 8) - 2
  oracles.draw_rect(label_x - 2, 0, (#label * 8) + 4, 10, COR_LABEL)
  oracles.draw_text(label_x, 1, label, COR_TEXTO)

  -- 4. Se nao ha texto ativo, nao faz mais nada
  if ativo == 0 then
    ultimo_ativo = false
    ultimo_id    = -1
    return
  end

  -- 5. Calcula o ID de 16 bits do dialogo atual
  local id = id_h * 256 + id_l

  -- 6. Mostra ID no canto superior esquerdo (modo debug)
  --    Descomente as linhas abaixo para descobrir IDs de novos dialogos:
  -- local hex = string.format("ID:%04X", id)
  -- oracles.draw_rect(0, 0, #hex * 8 + 4, 10, COR_AVISO)
  -- oracles.draw_text(2, 1, hex, COR_FUNDO)

  -- 7. Se PT-BR desativado, apenas mostra indicador e sai
  if not ptbr_ativo then return end

  -- 8. Busca traducao na tabela
  local traducao = TEXTOS[id]
  if not traducao then return end

  -- 9. Calcula a posicao do overlay (espelha a posicao da caixa do jogo)
  local box_y = (pos == 0) and BOX_Y_BOT or BOX_Y_TOP

  -- 10. Desenha fundo opaco sobre a caixa original
  oracles.draw_rect(0,     box_y - 1,  screen_w, BOX_H + 2, COR_BORDA)
  oracles.draw_rect(PAD/2, box_y,      screen_w - PAD, BOX_H, COR_FUNDO)

  -- 11. Escreve cada linha da traducao (max 3 linhas de 16px)
  for i, linha in ipairs(traducao) do
    if i > 3 then break end  -- cabe no maximo 3 linhas
    oracles.draw_text(PAD, box_y + (i - 1) * 16, linha, COR_TEXTO)
  end

  -- 12. Rodape: indicador de acao (pressione SELECT para alternar)
  local dica = "\x1b SELECT:IDIOMA"  -- \x1b e o glifo de cursor da fonte do jogo
  oracles.draw_text(PAD, box_y + BOX_H - 10, dica, COR_AVISO)

  ultimo_ativo = true
  ultimo_id    = id
end)

oracles.log("ptbr-traducao v1.0 carregado — PT-BR: " .. tostring(ptbr_ativo))
