# Ancient Golem

Extraídos do vídeo de referência (192 frames, 24fps, câmera fixa), 320x320 RGBA — mesma
escala (0,833) e mesma altura dos pés do recorte anterior, agora centralizado no quadro.

    1.png .. 32.png      parado, flutuando (loop; frames 11-44)
    move-1 .. move-32    deslocando (frames 66-97) — no vídeo ele vai para a esquerda; os
                         quadros foram espelhados para olhar à direita, a convenção de
                         caminhada do jogo (o jogo espelha de volta ao andar para a esquerda)
    atk-1 .. atk-32      carga (frames 100-133) e descarga/retorno (frames 160-191)

O disparo em si (frames 134-159 do vídeo) não entrou: o clarão ilumina a sala inteira e o
golem vira borrão, então não há como separar os dois. Se o feixe for preciso, ele tem que
ser uma camada de efeito à parte, não o sprite.

Recorte: rembg (modelo isnet-general-use).
