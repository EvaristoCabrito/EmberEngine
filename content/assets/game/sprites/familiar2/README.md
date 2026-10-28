# Familiar 2

Extraídos dos dois vídeos de referência (24fps, câmera fixa) com rembg (isnet-general-use),
36 quadros por animação, no mesmo quadro de 1400x704 e na mesma escala dos recortes antigos de
12 quadros (o tamanho na tela não mudou).

Vídeo "movement and attack" (escala 0,975, uma âncora única para parado/ataque/magia, então a
figura não treme nem salta ao trocar de animação):

    1.png .. 36.png      parado, de frente — quadros 197-236 (loop fechado, 40 quadros
                         reamostrados para 36)
    atk-1 .. atk-36      ataque — quadros 164-199, sem pular nenhum: recolhe (1-18), golpe dos
                         tentáculos no impacto (19-27 = quadros 182-190), volta ao parado
                         (28-36)
    cast-1 .. cast-36    magia — quadros 110-145, sem pular nenhum: já de frente, carrega o
                         brilho e termina na descarga elétrica, que é quando o feitiço sai no jogo

Vídeo "walking left and right" (escala 1,097 — a criatura é menor nesse vídeo):

    move-1 .. move-36            andando para a direita — quadros 161-200 (loop fechado, 40
                                 quadros reamostrados para 36), deslocamento do vídeo removido
    move-left-1 .. move-left-36  andando para a esquerda — quadros 37-72, idem, sem pular nenhum

A âncora comum deixa os pés parados ~20px acima da base do quadro, para caber o tentáculo que
avança na direção da câmera na magia e no ataque; o jogo compensa com footOffset (engine.ts).
