# Boi Morto-vivo (Undead Ox)

36 quadros por animação, como os sprites normais do jogo. Extraídos dos três vídeos de referência (640x360, 24fps, câmera fixa) com rembg
(isnet-general-use), máscara limpa (sem fantasmas nem fumaça solta no chão). Todos no mesmo
quadro de 640x404, na mesma linha do chão (y=397) e na mesma escala: o vídeo "Idle and ATT"
é a referência (escala 1); os outros dois foram reescalados pela altura do boi parado
(walk/cast 1,112, hit/death 1,18) e alinhados pelos pés. 44px livres no topo para o coice
do hit e o orbe da magia.

Vídeo "Idle and ATT" (boi olhando para a direita):

    1.png .. 36.png      parado — quadros 1-19, tocados ida e volta (ping-pong) para o loop
                         fechar sem salto
    atk-1 .. atk-36      ataque, investida de cabeça baixa, montado para as 3 etapas do jogo:
                         abaixa a cabeça 1-18 (quadros 113-143), investida/impacto 19-27
                         (146-162), recuperação 28-36 (165-211, termina parado)

Vídeo "walking and casting magic" (olhando para a direita):

    move-1 .. move-36    andando para a direita — quadros 16-71 (dois passos; o 72 repete a
                         pose do 16, então o loop fecha). O jogo espelha ao andar para a esquerda
    cast-1 .. cast-36    magia (Veneno Cáustico) — quadros 130-212: costelas acendem em verde,
                         sopro, orbe verde na frente da cabeça, dissipa. O rembg apagava o orbe;
                         ele foi recuperado por chave de verde + brilho sobre o fundo cinza do
                         vídeo original

Vídeo "hit reaction and death" (no vídeo o boi olha para a ESQUERDA, em 3/4; espelhado para
olhar à direita como o resto):

    hit-1 .. hit-36      reação ao golpe — quadros 12-60 (empina a cabeça e volta)
    death-1 .. death-36  morte — quadros 113-186 (cai para frente e fica deitado de lado)
