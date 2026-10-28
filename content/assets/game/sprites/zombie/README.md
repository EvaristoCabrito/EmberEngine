# Zombie

Extraídos dos vídeos de referência (24fps, câmera fixa) com rembg (isnet-general-use), depois
com a máscara limpa (figura sólida, sem fantasmas e sem fumaça no chão; os vãos entre os
braços continuam transparentes). Todos no mesmo quadro de 480x360, na mesma linha do chão
(y=353) e na mesma escala.

    1.png .. 32.png      parado — vídeo 1, 0,00–0,96 s, tocado para frente e para trás
                         (ping-pong) para o loop fechar sem salto. Uma âncora única para todos
                         os quadros (câmera fixa), então a figura não treme
    move-1 .. move-32    andando para a direita — vídeo 2, de 2,05 s a 3,9 s (caminhada de
                         lado, dois passos de verdade), espelhado. Sem mistura de quadros.
    move-left-1 .. 32    o mesmo trecho, sem espelhar (o jogo espelha move-* ao andar para a
                         esquerda; estes ficam de reserva, como no troll2)
    atk-1 .. atk-32      ataque — vídeo 2, a partir de 5,5 s, montado para as 3 etapas do jogo
                         (investida 0,20 s = quadros 1-16, impacto 0,18 s = 17-24, recuperação
                         0,16 s = 25-32). Começa de frente, como o parado, vira para a direita
                         e ataca; nunca olha para a esquerda, então não há troca de lado.

A caminhada do vídeo não é um ciclo perfeitamente fechado: o último quadro é o pé da frente
quase pousando e o primeiro é o pé já pousado, então a emenda lê como o próximo passo.
