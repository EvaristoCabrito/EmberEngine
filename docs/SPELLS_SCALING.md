# Escala das magias

Gerado por `tools/scripts/spells/tracks.py` (a tabela `kSpellTracks` em `engine/world/Spells.cpp` é a fonte; o texto do Como jogar sai do mesmo script).

Regras do usuário (2026-10-20): baseado no AD&D 2ª edição, sem copiar os números (o Ember tem atributos, HP e teto de nível
próprios); cada magia tem função e números únicos. A magia é aprendida no primeiro nível do tier dela para a classe e escala por
15 níveis: o nível em que é aprendida e os 14 seguintes, **uma melhoria pequena por nível, nunca duas no mesmo nível**. O alcance
nunca passa de 12 hexes (a visão do grupo). A área existe só nas magias de área: começa em 3 hexes (um corpo de tamanho 3) e cresce
um anel por vez, duas ou três vezes; o Veneno Cáustico tem a maior área do jogo (4 anéis), a Bola de Fogo a seguinte (3 anéis).
O Ember Ashes I para no nível 30; uma magia aprendida depois do 16 continua a escala no Ember Ashes II (teto 60), que já está
programado: os níveis além do 30 só não se alcançam ainda.

Familiar Titã é Tier 5 (nível 11); o Cão Zumbi é Tier 6 do Necromante (nível 15, a promoção) (o usuário, 2026-10-20).

### Míssil Mágico

Escala do nível 1 ao 15, uma melhoria pequena por nível: alcance 5 → 12; mísseis 1 → 5; peso do MAG 1,15 → 1,3.

Nível a nível: 2: +1 hex de alcance, 3: +1 mísseis, 4: +0,05 peso do MAG, 5: +1 hex de alcance, 6: +1 hex de alcance, 7: +1 mísseis, 8: +1 hex de alcance, 9: +0,05 peso do MAG, 10: +1 mísseis, 11: +1 hex de alcance, 12: +1 hex de alcance, 13: +0,05 peso do MAG, 14: +1 mísseis, 15: +1 hex de alcance.

### Relâmpago

Escala do nível 3 ao 17, uma melhoria pequena por nível: dano 2D8 → 5D8; alcance 5 → 12; peso do MAG 2 → 2,4.

Nível a nível: 4: +1 hex de alcance, 5: +0,1 peso do MAG, 6: +1 dado, 7: +1 hex de alcance, 8: +1 hex de alcance, 9: +0,1 peso do MAG, 10: +1 hex de alcance, 11: +1 dado, 12: +0,1 peso do MAG, 13: +1 hex de alcance, 14: +1 hex de alcance, 15: +1 dado, 16: +0,1 peso do MAG, 17: +1 hex de alcance.

### Frost

Escala do nível 5 ao 19, uma melhoria pequena por nível: dano 1D6 → 4D6; comprimento 2 → 8; peso do MAG 1,1 → 1,35.

Nível a nível: 6: +1 comprimento, 7: +0,05 peso do MAG, 8: +1 dado, 9: +1 comprimento, 10: +0,05 peso do MAG, 11: +1 comprimento, 12: +1 dado, 13: +0,05 peso do MAG, 14: +1 comprimento, 15: +0,05 peso do MAG, 16: +1 comprimento, 17: +1 dado, 18: +0,05 peso do MAG, 19: +1 comprimento.

### Sopro Venenoso

Escala do nível 3 ao 17, uma melhoria pequena por nível: dano 1D4 → 3D8; comprimento 1 → 3; peso do MAG 1,05 → 1,37.

Nível a nível: 4: +0,04 peso do MAG, 5: +1 comprimento, 6: +0,04 peso do MAG, 7: +1 dado, 8: +0,04 peso do MAG, 9: dado maior, 10: +0,04 peso do MAG, 11: +0,04 peso do MAG, 12: +1 comprimento, 13: +0,04 peso do MAG, 14: +1 dado, 15: +0,04 peso do MAG, 16: dado maior, 17: +0,04 peso do MAG.

### Bola de Fogo

Escala do nível 6 ao 20, uma melhoria pequena por nível: dano 2D6 → 6D6; alcance 5 → 12; área 3 hexes → 3 anéis.

Nível a nível: 7: +1 hex de alcance, 8: +1 dado, 9: +1 anel, 10: +1 hex de alcance, 11: +1 hex de alcance, 12: +1 dado, 13: +1 hex de alcance, 14: +1 anel, 15: +1 dado, 16: +1 hex de alcance, 17: +1 hex de alcance, 18: +1 anel, 19: +1 dado, 20: +1 hex de alcance.

### Veneno Cáustico

Escala do nível 11 ao 25, uma melhoria pequena por nível: dano 1D10 → 3D10; alcance 7 → 12; área 3 hexes → 4 anéis; peso do MAG 1,5 → 1,8.

Nível a nível: 12: +1 hex de alcance, 13: +1 anel, 14: +0,1 peso do MAG, 15: +1 dado, 16: +1 hex de alcance, 17: +1 anel, 18: +1 hex de alcance, 19: +0,1 peso do MAG, 20: +1 anel, 21: +1 hex de alcance, 22: +1 dado, 23: +0,1 peso do MAG, 24: +1 anel, 25: +1 hex de alcance.

### Relâmpago Maior

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 3D12 → 6D12; alcance 7 → 12; peso do MAG 3 → 3,6.

Nível a nível: 16: +0,1 peso do MAG, 17: +1 hex de alcance, 18: +1 dado, 19: +0,1 peso do MAG, 20: +1 hex de alcance, 21: +0,1 peso do MAG, 22: +1 hex de alcance, 23: +1 dado, 24: +0,1 peso do MAG, 25: +1 hex de alcance, 26: +0,1 peso do MAG, 27: +1 dado, 28: +1 hex de alcance, 29: +0,1 peso do MAG.

### Cura Menor

Escala do nível 1 ao 15, uma melhoria pequena por nível: dano 1D6 → 4D6; alcance 2 → 6; peso do MAG 1 → 1,35.

Nível a nível: 2: +0,05 peso do MAG, 3: +1 hex de alcance, 4: +1 dado, 5: +0,05 peso do MAG, 6: +0,05 peso do MAG, 7: +1 hex de alcance, 8: +1 dado, 9: +0,05 peso do MAG, 10: +1 hex de alcance, 11: +0,05 peso do MAG, 12: +0,05 peso do MAG, 13: +1 dado, 14: +1 hex de alcance, 15: +0,05 peso do MAG.

### Bênção

Escala do nível 3 ao 17, uma melhoria pequena por nível: área 3 hexes → 2 anéis; acerto % 1 → 8; rodadas 3 → 8.

Nível a nível: 4: +1 acerto %, 5: +1 rodadas, 6: +1 acerto %, 7: +1 anel, 8: +1 rodadas, 9: +1 acerto %, 10: +1 acerto %, 11: +1 rodadas, 12: +1 acerto %, 13: +1 rodadas, 14: +1 anel, 15: +1 acerto %, 16: +1 rodadas, 17: +1 acerto %.

### Cura Média

Escala do nível 3 ao 17, uma melhoria pequena por nível: dano 2D6 → 5D6; alcance 3 → 5; peso do MAG 1,6 → 2,05.

Nível a nível: 4: +0,05 peso do MAG, 5: +1 dado, 6: +0,05 peso do MAG, 7: +0,05 peso do MAG, 8: +1 hex de alcance, 9: +0,05 peso do MAG, 10: +1 dado, 11: +0,05 peso do MAG, 12: +0,05 peso do MAG, 13: +0,05 peso do MAG, 14: +1 hex de alcance, 15: +0,05 peso do MAG, 16: +1 dado, 17: +0,05 peso do MAG.

### Mãos Flamejantes

Escala do nível 5 ao 19, uma melhoria pequena por nível: dano 1D4 → 4D8; comprimento 1 → 3; peso do MAG 1,05 → 1,4.

Nível a nível: 6: +0,05 peso do MAG, 7: +1 dado, 8: +0,05 peso do MAG, 9: +1 comprimento, 10: dado maior, 11: +0,05 peso do MAG, 12: +1 dado, 13: +0,05 peso do MAG, 14: +0,05 peso do MAG, 15: +1 comprimento, 16: dado maior, 17: +0,05 peso do MAG, 18: +1 dado, 19: +0,05 peso do MAG.

### Expulsar Mortos-Vivos

Escala do nível 6 ao 20, uma melhoria pequena por nível: dano 4D6 → 9D6; área 3 hexes → 2 anéis; peso do MAG 1,6 → 2,3.

Nível a nível: 7: +0,1 peso do MAG, 8: +1 dado, 9: +0,1 peso do MAG, 10: +1 anel, 11: +1 dado, 12: +0,1 peso do MAG, 13: +1 dado, 14: +0,1 peso do MAG, 15: +0,1 peso do MAG, 16: +1 dado, 17: +1 anel, 18: +0,1 peso do MAG, 19: +1 dado, 20: +0,1 peso do MAG.

### Curar Doença

Escala do nível 6 ao 20, uma melhoria pequena por nível: alcance 2 → 4.

Nível a nível: 10: +1 hex de alcance, 17: +1 hex de alcance.

### Raio Divino

Escala do nível 8 ao 22, uma melhoria pequena por nível: dano 2D4 → 5D4; alcance 6 → 12; área 3 hexes → 2 anéis; peso do MAG 1,4 → 1,7.

Nível a nível: 9: +1 hex de alcance, 10: +1 dado, 11: +0,1 peso do MAG, 12: +1 hex de alcance, 13: +1 anel, 14: +1 hex de alcance, 15: +1 dado, 16: +0,1 peso do MAG, 17: +1 hex de alcance, 18: +1 anel, 19: +1 hex de alcance, 20: +1 dado, 21: +0,1 peso do MAG, 22: +1 hex de alcance.

### Círculo de Cura

Escala do nível 9 ao 23, uma melhoria pequena por nível: dano 1D8 → 4D8; alcance 4 → 10; área 3 hexes → 2 anéis; peso do MAG 1,2 → 1,5.

Nível a nível: 10: +1 hex de alcance, 11: +1 dado, 12: +0,1 peso do MAG, 13: +1 hex de alcance, 14: +1 anel, 15: +1 hex de alcance, 16: +1 dado, 17: +0,1 peso do MAG, 18: +1 hex de alcance, 19: +1 anel, 20: +1 hex de alcance, 21: +1 dado, 22: +0,1 peso do MAG, 23: +1 hex de alcance.

### Ressurreição

Escala do nível 12 ao 26, uma melhoria pequena por nível: dano 2D8 → 5D10; alcance 1 → 4; peso do MAG 1,5 → 2,2.

Nível a nível: 13: +0,1 peso do MAG, 14: +1 hex de alcance, 15: +1 dado, 16: +0,1 peso do MAG, 17: +0,1 peso do MAG, 18: dado maior, 19: +1 hex de alcance, 20: +0,1 peso do MAG, 21: +1 dado, 22: +0,1 peso do MAG, 23: +0,1 peso do MAG, 24: +1 hex de alcance, 25: +1 dado, 26: +0,1 peso do MAG.

### Julgamento

Escala do nível 14 ao 28, uma melhoria pequena por nível: dano 2D8 → 5D8; alcance 6 → 10; área 3 hexes → 2 anéis; peso do MAG 1,8 → 2,4; peso contra mortos-vivos 1,5 → 1,75.

Nível a nível: 15: +1 hex de alcance, 16: +0,15 peso do MAG, 17: +1 dado, 18: +1 anel, 19: +1 hex de alcance, 20: +0,15 peso do MAG, 21: +1 dado, 22: +0,25 peso contra mortos-vivos, 23: +1 hex de alcance, 24: +0,15 peso do MAG, 25: +1 anel, 26: +1 dado, 27: +1 hex de alcance, 28: +0,15 peso do MAG.

### Mãos Curativas

Escala do nível 11 ao 25, uma melhoria pequena por nível: dano 1D8 → 4D8; alcance 1 → 3; peso do MAG 1,2 → 1,65.

Nível a nível: 12: +0,05 peso do MAG, 13: +1 dado, 14: +0,05 peso do MAG, 15: +0,05 peso do MAG, 16: +1 hex de alcance, 17: +0,05 peso do MAG, 18: +1 dado, 19: +0,05 peso do MAG, 20: +0,05 peso do MAG, 21: +0,05 peso do MAG, 22: +1 hex de alcance, 23: +0,05 peso do MAG, 24: +1 dado, 25: +0,05 peso do MAG.

### Aura de Proteção

Escala do nível 15 ao 29, uma melhoria pequena por nível: área 3 hexes → 2 anéis; proteção % 20 → 36; rodadas 3 → 7.

Nível a nível: 16: +2 proteção %, 17: +1 rodadas, 18: +2 proteção %, 19: +1 anel, 20: +2 proteção %, 21: +1 rodadas, 22: +2 proteção %, 23: +2 proteção %, 24: +1 rodadas, 25: +2 proteção %, 26: +1 anel, 27: +2 proteção %, 28: +1 rodadas, 29: +2 proteção %.

### Presença Intimidante

Escala do nível 15 ao 29, uma melhoria pequena por nível: área 3 hexes → 2 anéis; intimidação % 20 → 38; rodadas 3 → 6.

Nível a nível: 16: +2 intimidação %, 17: +1 rodadas, 18: +2 intimidação %, 19: +2 intimidação %, 20: +1 anel, 21: +2 intimidação %, 22: +2 intimidação %, 23: +1 rodadas, 24: +2 intimidação %, 25: +2 intimidação %, 26: +1 anel, 27: +2 intimidação %, 28: +1 rodadas, 29: +2 intimidação %.

### Ira Divina

Escala do nível 18 ao 32, uma melhoria pequena por nível: dano 1D10 → 6D12; alcance 4 → 12.

Nível a nível: 19: +1 hex de alcance, 20: +1 dado, 21: +1 hex de alcance, 22: +1 dado, 23: +1 hex de alcance, 24: dado maior, 25: +1 hex de alcance, 26: +1 dado, 27: +1 hex de alcance, 28: +1 hex de alcance, 29: +1 dado, 30: +1 hex de alcance, 31: +1 dado, 32: +1 hex de alcance.

### Invocar Familiar

Escala do nível 1 ao 15, uma melhoria pequena por nível: alcance 2 → 8.

Nível a nível: 3: +1 hex de alcance, 5: +1 hex de alcance, 7: +1 hex de alcance, 10: +1 hex de alcance, 12: +1 hex de alcance, 14: +1 hex de alcance.

### Força Fantasmagórica

Escala do nível 2 ao 16, uma melhoria pequena por nível: dano 1D4 → 3D12; alcance 6 → 12; peso do MAG 1 → 1,2.

Nível a nível: 3: +1 hex de alcance, 4: dado maior, 5: +1 dado, 6: +1 hex de alcance, 7: +0,1 peso do MAG, 8: dado maior, 9: +1 hex de alcance, 10: +1 hex de alcance, 11: dado maior, 12: +1 dado, 13: +1 hex de alcance, 14: +0,1 peso do MAG, 15: dado maior, 16: +1 hex de alcance.

### Teia dos Sonhos

Escala do nível 3 ao 17, uma melhoria pequena por nível: alcance 7 → 12; área 3 hexes → 2 anéis; chance de sono % 25 → 60.

Nível a nível: 4: +5 chance de sono %, 5: +1 hex de alcance, 6: +5 chance de sono %, 7: +1 anel, 8: +1 hex de alcance, 9: +5 chance de sono %, 10: +1 hex de alcance, 11: +5 chance de sono %, 12: +5 chance de sono %, 13: +1 hex de alcance, 14: +1 anel, 15: +5 chance de sono %, 16: +1 hex de alcance, 17: +5 chance de sono %.

### Familiar Maior

Escala do nível 3 ao 17, uma melhoria pequena por nível: alcance 2 → 8.

Nível a nível: 5: +1 hex de alcance, 7: +1 hex de alcance, 9: +1 hex de alcance, 12: +1 hex de alcance, 14: +1 hex de alcance, 16: +1 hex de alcance.

### Troca de Lugar

Escala do nível 3 ao 17, uma melhoria pequena por nível: alcance 6 → 10.

Nível a nível: 5: +1 hex de alcance, 9: +1 hex de alcance, 12: +1 hex de alcance, 16: +1 hex de alcance.

### Lança de Osso

Escala do nível 5 ao 19, uma melhoria pequena por nível: dano 1D8 → 4D12; comprimento 3 → 7; peso do MAG 1,3 → 1,8.

Nível a nível: 6: +0,1 peso do MAG, 7: +1 comprimento, 8: +1 dado, 9: dado maior, 10: +0,1 peso do MAG, 11: +1 comprimento, 12: +1 dado, 13: +0,1 peso do MAG, 14: +1 comprimento, 15: +0,1 peso do MAG, 16: dado maior, 17: +1 dado, 18: +1 comprimento, 19: +0,1 peso do MAG.

### Familiar Radiante

Escala do nível 6 ao 20, uma melhoria pequena por nível: alcance 2 → 8.

Nível a nível: 8: +1 hex de alcance, 10: +1 hex de alcance, 12: +1 hex de alcance, 15: +1 hex de alcance, 17: +1 hex de alcance, 19: +1 hex de alcance.

### Imagem Espelhada

Escala do nível 7 ao 21, uma melhoria pequena por nível: alcance 6 → 10; cópias 2 → 4; chance de errar % 33 → 48; rodadas 3 → 6.

Nível a nível: 8: +3 chance de errar %, 9: +1 hex de alcance, 10: +1 rodadas, 11: +1 cópias, 12: +3 chance de errar %, 13: +1 hex de alcance, 14: +1 rodadas, 15: +3 chance de errar %, 16: +1 hex de alcance, 17: +3 chance de errar %, 18: +1 cópias, 19: +1 rodadas, 20: +1 hex de alcance, 21: +3 chance de errar %.

### Detonar Invocação

Escala do nível 9 ao 23, uma melhoria pequena por nível: dano 2D8 → 5D8; alcance 6 → 10; área 3 hexes → 2 anéis; peso do MAG 1,8 → 2,4.

Nível a nível: 10: +0,12 peso do MAG, 11: +1 hex de alcance, 12: +1 dado, 13: +1 anel, 14: +0,12 peso do MAG, 15: +1 hex de alcance, 16: +1 dado, 17: +0,12 peso do MAG, 18: +1 hex de alcance, 19: +0,12 peso do MAG, 20: +1 anel, 21: +1 dado, 22: +1 hex de alcance, 23: +0,12 peso do MAG.

### Familiar Titã (T5)

Escala do nível 11 ao 25, uma melhoria pequena por nível: alcance 2 → 8.

Nível a nível: 13: +1 hex de alcance, 15: +1 hex de alcance, 17: +1 hex de alcance, 20: +1 hex de alcance, 22: +1 hex de alcance, 24: +1 hex de alcance.

### Elo de Sacrifício

Escala do nível 11 ao 25, uma melhoria pequena por nível: dano que a invocação recebe % 100 → 58.

Nível a nível: 12: -3 dano que a invocação recebe %, 13: -3 dano que a invocação recebe %, 14: -3 dano que a invocação recebe %, 15: -3 dano que a invocação recebe %, 16: -3 dano que a invocação recebe %, 17: -3 dano que a invocação recebe %, 18: -3 dano que a invocação recebe %, 19: -3 dano que a invocação recebe %, 20: -3 dano que a invocação recebe %, 21: -3 dano que a invocação recebe %, 22: -3 dano que a invocação recebe %, 23: -3 dano que a invocação recebe %, 24: -3 dano que a invocação recebe %, 25: -3 dano que a invocação recebe %.

### Pesadelo

Escala do nível 13 ao 27, uma melhoria pequena por nível: dano 2D8 → 4D8; alcance 6 → 12; área 3 hexes → 2 anéis; peso do MAG 1,5 → 1,8; chance de medo % 25 → 35; turnos de medo 1 → 2.

Nível a nível: 14: +1 hex de alcance, 15: +1 anel, 16: +1 dado, 17: +1 hex de alcance, 18: +0,15 peso do MAG, 19: +1 hex de alcance, 20: +10 chance de medo %, 21: +1 turnos de medo, 22: +1 hex de alcance, 23: +1 anel, 24: +1 hex de alcance, 25: +1 dado, 26: +0,15 peso do MAG, 27: +1 hex de alcance.

### Cão Zumbi (Necromante, T6)

Escala do nível 15 ao 29, uma melhoria pequena por nível: alcance 2 → 8.

Nível a nível: 17: +1 hex de alcance, 19: +1 hex de alcance, 21: +1 hex de alcance, 24: +1 hex de alcance, 26: +1 hex de alcance, 28: +1 hex de alcance.

### Fortalecer Invocações

Escala do nível 15 ao 29, uma melhoria pequena por nível: atributos % 25 → 37; rodadas 3 → 5.

Nível a nível: 16: +1 atributos %, 17: +1 atributos %, 18: +1 atributos %, 19: +1 rodadas, 20: +1 atributos %, 21: +1 atributos %, 22: +1 atributos %, 23: +1 atributos %, 24: +1 atributos %, 25: +1 atributos %, 26: +1 rodadas, 27: +1 atributos %, 28: +1 atributos %, 29: +1 atributos %.

### Marca da Ruína

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 1D6 → 2D8; alcance 6 → 11; peso do MAG 1,15 → 1,3; dano extra % 20 → 35; turnos 3 → 4.

Nível a nível: 16: +1 hex de alcance, 17: +5 dano extra %, 18: +0,05 peso do MAG, 19: +1 hex de alcance, 20: +1 turnos, 21: +1 dado, 22: +1 hex de alcance, 23: +5 dano extra %, 24: +0,05 peso do MAG, 25: dado maior, 26: +1 hex de alcance, 27: +5 dano extra %, 28: +0,05 peso do MAG, 29: +1 hex de alcance.

### Praga Rastejante

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 1D8 → 3D10; alcance 6 → 11; peso do MAG 1,3 → 1,5; espalha 1 → 2; Veneno Maior → Veneno Mortal.

Nível a nível: 16: +1 hex de alcance, 17: +0,05 peso do MAG, 18: +1 dado, 19: +1 hex de alcance, 20: +0,05 peso do MAG, 21: veneno mais forte, 22: +1 hex de alcance, 23: +1 espalha, 24: dado maior, 25: +0,05 peso do MAG, 26: +1 hex de alcance, 27: +1 dado, 28: +0,05 peso do MAG, 29: +1 hex de alcance.

### Dreno de Vida

Escala do nível 16 ao 30, uma melhoria pequena por nível: dano 2D6 → 3D10; alcance 5 → 7; peso do MAG 1,6 → 1,95; cura % 50 → 100.

Nível a nível: 17: +0,05 peso do MAG, 18: +1 hex de alcance, 19: +0,05 peso do MAG, 20: dado maior, 21: +25 cura %, 22: +0,05 peso do MAG, 23: +0,05 peso do MAG, 24: +1 dado, 25: +0,05 peso do MAG, 26: +1 hex de alcance, 27: dado maior, 28: +0,05 peso do MAG, 29: +25 cura %, 30: +0,05 peso do MAG.

### Mortalha de Sombras

Escala do nível 17 ao 31, uma melhoria pequena por nível: dano por turno 1D6 → 2D6; alcance 6 → 12; área 3 hexes → 2 anéis; redução % 20 → 35; rodadas 2 → 4.

Nível a nível: 18: +1 hex de alcance, 19: +5 redução %, 20: +1 anel, 21: +1 hex de alcance, 22: +1 rodadas, 23: +1 hex de alcance, 24: +5 redução %, 25: +1 dado por turno, 26: +1 hex de alcance, 27: +1 anel, 28: +1 hex de alcance, 29: +1 rodadas, 30: +5 redução %, 31: +1 hex de alcance.

### Hemorragia

Escala do nível 18 ao 32, uma melhoria pequena por nível: dano 2D6 → 3D6; dano por turno 2D6 → 3D8; alcance 5 → 9; peso do MAG 1,4 → 1,7; duração 1D6 → 2D6.

Nível a nível: 19: +0,05 peso do MAG, 20: +1 hex de alcance, 21: +0,05 peso do MAG, 22: +1 dado por turno, 23: +1 hex de alcance, 24: +0,05 peso do MAG, 25: dado por turno maior, 26: +1 dado de duração, 27: +0,05 peso do MAG, 28: +1 hex de alcance, 29: +1 dado, 30: +0,05 peso do MAG, 31: +1 hex de alcance, 32: +0,05 peso do MAG.

### Silêncio Profano

Escala do nível 19 ao 33, uma melhoria pequena por nível: dano 1D8 → 2D10; alcance 6 → 10; área 3 hexes → 2 anéis; peso do MAG 1,1 → 1,3; turnos 2 → 4.

Nível a nível: 20: +1 hex de alcance, 21: +0,05 peso do MAG, 22: +1 anel, 23: +1 turnos, 24: +1 hex de alcance, 25: +0,05 peso do MAG, 26: +1 dado, 27: dado maior, 28: +1 hex de alcance, 29: +0,05 peso do MAG, 30: +1 anel, 31: +1 turnos, 32: +1 hex de alcance, 33: +0,05 peso do MAG.

### Peste Negra

Escala do nível 21 ao 35, uma melhoria pequena por nível: dano 2D6 → 4D10; alcance 7 → 9; área 3 hexes → 2 anéis; peso do MAG 1,2 → 1,45; Veneno Mortal → Veneno Letal.

Nível a nível: 22: +0,05 peso do MAG, 23: +1 hex de alcance, 24: +1 anel, 25: +1 dado, 26: +0,05 peso do MAG, 27: dado maior, 28: +0,05 peso do MAG, 29: veneno mais forte, 30: +1 hex de alcance, 31: +0,05 peso do MAG, 32: +1 anel, 33: +1 dado, 34: dado maior, 35: +0,05 peso do MAG.

### Maldição da Decadência

Escala do nível 22 ao 36, uma melhoria pequena por nível: dano 2D8 → 3D8; dano por turno 1D8 → 2D10; alcance 6 → 8; peso do MAG 1,5 → 1,7; teto % 30 → 50; turnos 3 → 4; peso por turno 0,5 → 0,7.

Nível a nível: 23: +0,05 peso do MAG, 24: +1 hex de alcance, 25: +10 teto %, 26: +0,1 peso por turno, 27: +0,05 peso do MAG, 28: +1 turnos, 29: +1 dado, 30: +1 dado por turno, 31: dado por turno maior, 32: +0,05 peso do MAG, 33: +1 hex de alcance, 34: +10 teto %, 35: +0,1 peso por turno, 36: +0,05 peso do MAG.

### Colheita de Almas

Escala do nível 23 ao 37, uma melhoria pequena por nível: dano 3D6 → 4D8; alcance 6 → 12; área 3 hexes → 2 anéis; peso do MAG 1,7 → 1,9; alma % 10 → 14.

Nível a nível: 24: +1 hex de alcance, 25: +1 anel, 26: +0,1 peso do MAG, 27: +1 hex de alcance, 28: +2 alma %, 29: +1 hex de alcance, 30: +1 dado, 31: dado maior, 32: +1 hex de alcance, 33: +1 anel, 34: +1 hex de alcance, 35: +0,1 peso do MAG, 36: +2 alma %, 37: +1 hex de alcance.

### Vazio Devorador

Escala do nível 25 ao 39, uma melhoria pequena por nível: dano 3D8 → 4D10; alcance 7 → 11; peso do MAG 2,2 → 2,4; dano extra % 15 → 25; resistência retirada 30 → 40.

Nível a nível: 26: +1 hex de alcance, 27: +0,05 peso do MAG, 28: +5 dano extra %, 29: +5 resistência retirada, 30: +1 hex de alcance, 31: +0,05 peso do MAG, 32: +1 dado, 33: dado maior, 34: +1 hex de alcance, 35: +0,05 peso do MAG, 36: +5 dano extra %, 37: +5 resistência retirada, 38: +1 hex de alcance, 39: +0,05 peso do MAG.

### Coluna de Fogo

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 2D8 → 3D10; chão em chamas 1D6 → 2D6; alcance 6 → 11; área 3 hexes → 2 anéis; peso do MAG 2 → 2,3; rodadas 1 → 2.

Nível a nível: 16: +1 hex de alcance, 17: +0,1 peso do MAG, 18: +1 anel, 19: +1 hex de alcance, 20: +1 dado, 21: +1 rodadas, 22: +1 hex de alcance, 23: +0,1 peso do MAG, 24: +1 dado de fogo, 25: dado maior, 26: +1 hex de alcance, 27: +1 anel, 28: +0,1 peso do MAG, 29: +1 hex de alcance.

### Lança de Gelo

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 2D10 → 3D12; alcance 7 → 12; peso do MAG 2,4 → 2,65; dano após o 1º % 50 → 100.

Nível a nível: 16: +1 hex de alcance, 17: +0,05 peso do MAG, 18: +25 dano após o 1º %, 19: +1 hex de alcance, 20: +0,05 peso do MAG, 21: +1 dado, 22: +1 hex de alcance, 23: +0,05 peso do MAG, 24: dado maior, 25: +1 hex de alcance, 26: +0,05 peso do MAG, 27: +25 dano após o 1º %, 28: +1 hex de alcance, 29: +0,05 peso do MAG.

### Tempestade Elétrica

Escala do nível 16 ao 30, uma melhoria pequena por nível: dano 2D12 → 3D12; dano por turno 1D6 → 2D6; alcance 6 → 12; área 3 hexes → 2 anéis; bônus do eco 4 → 6; peso do MAG 1,8 → 2,1.

Nível a nível: 17: +1 hex de alcance, 18: +0,1 peso do MAG, 19: +1 anel, 20: +1 hex de alcance, 21: +1 dado, 22: +1 hex de alcance, 23: +0,1 peso do MAG, 24: +1 dado por turno, 25: +1 hex de alcance, 26: +2 bônus do eco, 27: +1 hex de alcance, 28: +1 anel, 29: +0,1 peso do MAG, 30: +1 hex de alcance.

### Meteoro

Escala do nível 17 ao 31, uma melhoria pequena por nível: dano 4D6 → 5D8; dano do anel 2D6 → 3D6; alcance 8 → 12; área 3 hexes → 2 anéis; peso do MAG 3 → 3,3; peso do anel 2 → 2,4.

Nível a nível: 18: +1 hex de alcance, 19: +0,1 peso do MAG, 20: +1 anel, 21: +0,2 peso do anel, 22: +1 hex de alcance, 23: +1 dado, 24: +0,1 peso do MAG, 25: +1 dado no anel, 26: dado maior, 27: +1 hex de alcance, 28: +1 anel, 29: +0,2 peso do anel, 30: +0,1 peso do MAG, 31: +1 hex de alcance.

### Zero Absoluto

Escala do nível 18 ao 32, uma melhoria pequena por nível: dano 3D8 → 4D10; alcance 6 → 10; área 3 hexes → 2 anéis; peso do MAG 2,2 → 2,5.

Nível a nível: 19: +0,05 peso do MAG, 20: +1 hex de alcance, 21: +1 anel, 22: +0,05 peso do MAG, 23: +1 hex de alcance, 24: +0,05 peso do MAG, 25: +1 dado, 26: dado maior, 27: +0,05 peso do MAG, 28: +1 hex de alcance, 29: +0,05 peso do MAG, 30: +1 anel, 31: +1 hex de alcance, 32: +0,05 peso do MAG.

### Cadeia de Raios

Escala do nível 19 ao 33, uma melhoria pequena por nível: dano 3D10 → 4D10; alcance 7 → 12; alcance do salto 3 → 4; saltos 3 → 5; peso do MAG 2,8 → 2,95; perda por salto 0,15 → 0,1.

Nível a nível: 20: +1 hex de alcance, 21: +0,05 peso do MAG, 22: +1 saltos, 23: -0,03 perda por salto, 24: +1 hex de alcance, 25: +1 dado, 26: +1 hex de alcance, 27: +0,05 peso do MAG, 28: +1 alcance do salto, 29: +1 hex de alcance, 30: +1 saltos, 31: -0,03 perda por salto, 32: +0,05 peso do MAG, 33: +1 hex de alcance.

### Erupção

Escala do nível 21 ao 35, uma melhoria pequena por nível: dano 4D8 → 5D10; chão em chamas 1D8 → 2D10; área 3 hexes → 2 anéis; peso do MAG 2,6 → 2,9; rodadas 2 → 4.

Nível a nível: 22: +0,05 peso do MAG, 23: +1 anel, 24: +1 rodadas, 25: +0,05 peso do MAG, 26: +1 dado, 27: +0,05 peso do MAG, 28: +1 dado de fogo, 29: dado maior, 30: +0,05 peso do MAG, 31: dado de fogo maior, 32: +0,05 peso do MAG, 33: +1 anel, 34: +1 rodadas, 35: +0,05 peso do MAG.

### Vendaval Glacial

Escala do nível 22 ao 36, uma melhoria pequena por nível: dano 4D6 → 5D10; comprimento 5 → 8; peso do MAG 2,4 → 2,75; empurrão 1 → 2.

Nível a nível: 23: +0,05 peso do MAG, 24: +1 comprimento, 25: +0,05 peso do MAG, 26: dado maior, 27: +0,05 peso do MAG, 28: +1 dado, 29: +0,05 peso do MAG, 30: +1 comprimento, 31: +1 empurrão, 32: +0,05 peso do MAG, 33: dado maior, 34: +0,05 peso do MAG, 35: +1 comprimento, 36: +0,05 peso do MAG.

### Fúria dos Céus

Escala do nível 23 ao 37, uma melhoria pequena por nível: dano 3D12 → 4D12; alcance 8 → 12; raios 4 → 5; bônus 0 → 4; peso do MAG 2,6 → 3.

Nível a nível: 24: +1 hex de alcance, 25: +0,1 peso do MAG, 26: +1 bônus, 27: +1 raios, 28: +1 hex de alcance, 29: +0,1 peso do MAG, 30: +1 bônus, 31: +1 dado, 32: +1 hex de alcance, 33: +0,1 peso do MAG, 34: +1 bônus, 35: +1 hex de alcance, 36: +0,1 peso do MAG, 37: +1 bônus.

### Brasa Primordial

Escala do nível 25 ao 39, uma melhoria pequena por nível: dano 4D10 → 5D12; chão em chamas 2D8 → 3D10; alcance 7 → 10; área 3 hexes → 2 anéis; bônus 0 → 1; peso do MAG 3,4 → 3,7; rodadas 2 → 3.

Nível a nível: 26: +1 hex de alcance, 27: +0,1 peso do MAG, 28: +1 anel, 29: +1 dado, 30: +1 dado de fogo, 31: +1 rodadas, 32: +1 hex de alcance, 33: +0,1 peso do MAG, 34: dado maior, 35: dado de fogo maior, 36: +1 bônus, 37: +1 anel, 38: +1 hex de alcance, 39: +0,1 peso do MAG.

### Tiro Longo

Escala do nível 1 ao 15, uma melhoria pequena por nível: dano 1D8 → 4D12; alcance 7 → 12; bônus no arco 0 → 4.

Nível a nível: 2: +1 hex de alcance, 3: +1 bônus no arco, 4: +1 dado, 5: dado maior, 6: +1 hex de alcance, 7: +1 bônus no arco, 8: +1 hex de alcance, 9: +1 dado, 10: +1 bônus no arco, 11: +1 hex de alcance, 12: dado maior, 13: +1 dado, 14: +1 bônus no arco, 15: +1 hex de alcance.

### Flecha Envenenada

Escala do nível 2 ao 16, uma melhoria pequena por nível: dano 1D4 → 3D8; hexes além do arco 0 → 4; peso do arco 1 → 1,15; Veneno Menor → Veneno Mortal.

Nível a nível: 3: +1 hex além do arco, 4: veneno mais forte, 5: +0,05 peso do arco, 6: +1 dado, 7: dado maior, 8: +1 hex além do arco, 9: veneno mais forte, 10: +0,05 peso do arco, 11: +1 hex além do arco, 12: +1 dado, 13: dado maior, 14: veneno mais forte, 15: +0,05 peso do arco, 16: +1 hex além do arco.

### Tiro Sangrento

Escala do nível 5 ao 19, uma melhoria pequena por nível: dano por turno 1D6 → 2D8; alcance 6 → 12; peso do arco 1,5 → 2,4.

Nível a nível: 6: +1 hex de alcance, 7: +0,15 peso do arco, 8: +1 hex de alcance, 9: +0,15 peso do arco, 10: +1 dado por turno, 11: +1 hex de alcance, 12: +0,15 peso do arco, 13: dado por turno maior, 14: +1 hex de alcance, 15: +0,15 peso do arco, 16: +1 hex de alcance, 17: +0,15 peso do arco, 18: +1 hex de alcance, 19: +0,15 peso do arco.

### Tiro Múltiplo

Escala do nível 7 ao 21, uma melhoria pequena por nível: dano 0 → 3D10; alcance 6 → 12; alvos 2 → 4.

Nível a nível: 8: +1 hex de alcance, 9: +1 dado, 10: dado maior, 11: +1 hex de alcance, 12: +1 alvos, 13: +1 hex de alcance, 14: +1 dado, 15: dado maior, 16: +1 hex de alcance, 17: +1 alvos, 18: +1 hex de alcance, 19: +1 dado, 20: dado maior, 21: +1 hex de alcance.

### Chuva de Flechas

Escala do nível 7 ao 21, uma melhoria pequena por nível: dano 1D4 → 2D6; alcance 6 → 12; área 3 hexes → 2 anéis; peso do arco 0,75 → 0,95.

Nível a nível: 8: +1 hex de alcance, 9: +0,05 peso do arco, 10: +1 anel, 11: +1 hex de alcance, 12: +0,05 peso do arco, 13: +1 hex de alcance, 14: +1 dado, 15: dado maior, 16: +1 hex de alcance, 17: +0,05 peso do arco, 18: +1 hex de alcance, 19: +1 anel, 20: +0,05 peso do arco, 21: +1 hex de alcance.

### Flecha Explosiva

Escala do nível 10 ao 24, uma melhoria pequena por nível: dano 2D6 → 4D10; hexes além do arco 0 → 4; peso do arco 1,3 → 1,75; dano nos vizinhos % 50 → 65.

Nível a nível: 11: +1 hex além do arco, 12: +0,15 peso do arco, 13: +5 dano nos vizinhos %, 14: +1 dado, 15: dado maior, 16: +1 hex além do arco, 17: +0,15 peso do arco, 18: +5 dano nos vizinhos %, 19: +1 hex além do arco, 20: +1 dado, 21: dado maior, 22: +0,15 peso do arco, 23: +5 dano nos vizinhos %, 24: +1 hex além do arco.

### Tiro Paralisante

Escala do nível 12 ao 26, uma melhoria pequena por nível: dano 1D8 → 2D8; hexes além do arco 0 → 4; peso do arco 1 → 1,3; MOV retirado % 50 → 75; turnos 2 → 3.

Nível a nível: 13: +5 MOV retirado %, 14: +1 hex além do arco, 15: +0,1 peso do arco, 16: +5 MOV retirado %, 17: +1 hex além do arco, 18: +1 dado, 19: +5 MOV retirado %, 20: +0,1 peso do arco, 21: +1 turnos, 22: +1 hex além do arco, 23: +5 MOV retirado %, 24: +0,1 peso do arco, 25: +1 hex além do arco, 26: +5 MOV retirado %.

### Tempestade de Flechas

Escala do nível 13 ao 27, uma melhoria pequena por nível: dano 1D6 → 2D10; alcance 7 → 12; área 3 hexes → 2 anéis; peso do arco 0,9 → 1,3.

Nível a nível: 14: +1 hex de alcance, 15: +0,1 peso do arco, 16: +1 anel, 17: dado maior, 18: +1 hex de alcance, 19: +0,1 peso do arco, 20: +1 hex de alcance, 21: +1 dado, 22: +0,1 peso do arco, 23: +1 hex de alcance, 24: +1 anel, 25: dado maior, 26: +0,1 peso do arco, 27: +1 hex de alcance.

### Tiro Mortal

Escala do nível 15 ao 29, uma melhoria pequena por nível: dano 2D8 → 3D12; hexes além do arco 2 → 6; peso do arco 2 → 2,6; executa abaixo de % do HP 25 → 31.

Nível a nível: 16: +1 hex além do arco, 17: +0,15 peso do arco, 18: +2 executa abaixo de % do HP, 19: dado maior, 20: +1 hex além do arco, 21: +0,15 peso do arco, 22: +2 executa abaixo de % do HP, 23: +1 dado, 24: +1 hex além do arco, 25: +0,15 peso do arco, 26: dado maior, 27: +2 executa abaixo de % do HP, 28: +1 hex além do arco, 29: +0,15 peso do arco.

### Veneno Menor (plantas)

Escala do nível 8 ao 22, uma melhoria pequena por nível: alcance 5 → 7; área 3 hexes → 2 anéis.

Nível a nível: 10: +1 hex de alcance, 14: +1 anel, 17: +1 hex de alcance, 21: +1 anel.
