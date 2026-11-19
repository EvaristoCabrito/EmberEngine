# Birolho1 — prompts para os vídeos que ainda faltam (2026-11-19)

O que o Birolho1 (classe `birolhoLegs2`) tem hoje, tirado do `BirolhoIdlewalkUpSidewaysattCast` + o novo `Birolho_reacts_to_hit_and_..._1080p`:
idle flutuando, abrir os membros, andar de frente, andar de lado (esquerda; a direita é espelhada), chicote de tentáculo, conjuração, dano e queda. A morte já está no vídeo novo de dano (a partir de ~5 s ele cai, pousa e fica parado), então não precisa de prompt.

O que falta de verdade (só o que o Birolho1 não tem; um prompt para cada):

1. **Andar de costas** — não tem andar para cima (o BirolhoLegs tem).
2. **Fechar os membros** — depois de andar ele volta ao idle flutuando num corte seco; o vídeo mostraria as pernas entrando no corpo.

Não pedi idle sobre as pernas (ele já tem o idle flutuando), nem Ataque 2 (ele tem o chicote de tentáculo), nem morte (já está no vídeo novo).

Valem as regras do `VEO_PROMPTS.md`, com estas adaptações para o Birolho (ele é escuro; os vídeos antigos em fundo preto deram recorte difícil):
fundo e chão **cinza médio liso**, o mesmo do vídeo novo de dano; câmera parada; 1920×1080 (16:9, porque os braços abertos ficam largos);
24 fps; a figura inteira, **braços e tentáculos incluídos**, sempre dentro do quadro com margem de pelo menos 8 % em cada lado; sem
projéteis, faíscas, poeira, fumaça ou objetos entrando no quadro (o vídeo novo de dano trouxe um projétil que tive de disfarçar na hora do
recorte: não repetir).

## Bloco igual em todos os vídeos

```
Full-body shot of the Birolho, a round, ball-shaped, rotting dark grey-green creature about the size of a large boulder: one wide mouth
full of sharp crooked teeth across the front, several large bulging bloodshot eyes on the upper half, a fringe of thin dark tendrils
around the body, scabby warty skin. When it walks it has two thick, short, dark brown-grey legs with clawed feet and two
long arms with clawed hands that come out from the sides and underside of the body; when it floats it has none of them. It is in front of a seamless plain medium-grey studio
backdrop and floor. Soft, even, neutral studio lighting from the front and both sides, no colored light, only a faint soft contact shadow
under it. Locked-off static camera at chest height, no camera movement, no zoom, no cuts. Horizontal 16:9 frame. The whole creature, every
limb, claw and tendril, stays inside the frame at all times with a clear margin of empty backdrop on every side. Its body (the ball) is
about 45% of the frame's height. Realistic, photographic, sharp focus, natural colors, exactly the same creature, skin and colors in every
video. Nothing else is in the frame: no projectiles, no sparks, no dust, no smoke, no other objects or creatures.
Audio: only sound effects of the creature itself (growls, wet and fleshy sounds, footsteps, claws on the floor); no narration, no voice-over,
no speech, no dialogue, no singing, no music, no background ambience.
```

Tempo: 4 s cada, o mínimo que dá um loop de andar e uma transição, para não gerar quadros à toa.

## 1. Andar de costas (4 s, loop)

```
[BLOCK] The Birolho walks in place AWAY from the camera, seen from behind (we see the back of its round body, its arms and its legs, never its
mouth or eyes), as if on a treadmill: its body stays in the center of the frame while the two legs move in a steady, heavy, natural
stalking rhythm at a constant pace for the whole video. It is already walking, on its legs, in the first frame and keeps walking until
the last frame, never slowing down, never stopping, never turning around. The last frame is the same step of the walk cycle as the first
frame, so the loop is seamless.
```
Por quê: só o trecho de passo constante vira loop (o BirolhoWalkandATT2 troca de ângulo a cada ~2 s; 4 s dá uns dois ciclos de passo).

## 2. Fechar os membros (4 s, uma vez)

Usar como imagem de referência (primeiro quadro) um quadro do fim do andar de frente: de pé nas pernas, **braços caídos junto ao corpo** (o vídeo anterior abriu os braços e ignorou a referência; repetir isso no texto, em maiúsculas, já está no prompt).

```
[BLOCK] The first frame is exactly the reference image: the Birolho standing on its two legs with its two arms HANGING DOWN at its sides, close to the body (the arms are never spread or raised, in any frame), in the center of the frame. It stays floating in
exactly the same place for the whole video: the body does not move across the frame and does not rise or sink, only the little natural
bob of a floating creature. Smoothly, in one continuous motion, the two arms fold in and the two legs pull up and retract into the
underside of the round body, until it is the plain floating ball with no legs and no arms, tendrils stirring. The video ends in that plain
floating pose, still and calm, held for the last second.
```
Por quê: é o contrário do "abrir os membros"; hoje a volta é um corte seco. Ele fica flutuando no mesmo lugar, só fecha os membros.
