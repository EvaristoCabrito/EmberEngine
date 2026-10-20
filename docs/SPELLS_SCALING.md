# Escala das magias (alcance e área)

Gerado de `engine/world/Spells.cpp` (`kReachScale`, `kAreaScale`) por `scaling_doc.py`; o código é a fonte.

Regras do usuário (2026-10-20): baseado no AD&D 2ª edição, sem copiar os números (o Ember tem atributos, HP e teto de nível 30
próprios); cada magia tem função e números únicos; o alcance cresce com o nível de quem lança, desde o nível em que a magia é
aprendida (o primeiro uso do tier dela na classe), nunca acima de 12 hexes (a visão do grupo); a área de efeito existe só nas
magias de área, começa em 3 hexes (um corpo de tamanho 3) e cresce de anel em anel duas ou três vezes; o Veneno Cáustico tem a
maior área do jogo (4 anéis), a Bola de Fogo a seguinte (3 anéis); o Raio Divino é sempre menor que a Bola de Fogo.

Dano que mudou com o nível (baseado no AD&D, sem copiar): Míssil Mágico +1 míssil a cada 3 níveis, até 5 no 13; Bola de Fogo
+1D6 a cada 2 níveis desde o 6, até 8D6; Relâmpago +1D8 a cada 4 níveis desde o 3, até 5D8; Veneno Menor abaixo do Veneno
Cáustico em todos os números (alcance 5, ⌊MAG/2⌋×1,0+1D6 no alvo, ×0,6+1D4 no respingo, veneno fraco).

| Magia | Alcance (hexes) | Área de efeito |
|---|---|---|
| Míssil Mágico | 5 no nível 1, +1 por nível, até 12 (no 8) | alvo único / linha |
| Relâmpago | 5 no nível 3, +1 por nível, até 12 (no 10) | alvo único / linha |
| Bola de Fogo | 5 no nível 6, +1 por nível, até 12 (no 13) | 3 hexes ao aprender, 1 anel no 8, 2 anéis no 13, 3 anéis no 18 |
| Veneno Cáustico | 7 no nível 11, +1 a cada 2 níveis, até 12 (no 21) | 3 hexes ao aprender, 1 anel no 14, 2 anéis no 18, 3 anéis no 22, 4 anéis no 26 |
| Relâmpago Maior | 7 no nível 15, +1 por nível, até 12 (no 20) | alvo único / linha |
| Cura Menor | 2 no nível 1, +1 a cada 3 níveis, até 6 (no 13) | alvo único / linha |
| Cura Média | 3 no nível 3, +1 a cada 5 níveis, até 5 (no 13) | alvo único / linha |
| Curar Doença | 2 no nível 6, +1 a cada 5 níveis, até 4 (no 16) | alvo único / linha |
| Raio Divino | 6 no nível 8, +1 a cada 2 níveis, até 12 (no 20) | 3 hexes ao aprender, 1 anel no 16, 2 anéis no 24 |
| Mãos Curativas | 1 no nível 11, +1 a cada 6 níveis, até 3 (no 23) | alvo único / linha |
| Círculo de Cura | 4 no nível 9, +1 a cada 2 níveis, até 10 (no 21) | 3 hexes ao aprender, 1 anel no 13, 2 anéis no 21 |
| Julgamento | 6 no nível 14, +1 a cada 3 níveis, até 10 (no 26) | 3 hexes ao aprender, 1 anel no 20, 2 anéis no 26 |
| Força Fantasmagórica | 6 no nível 2, +1 por nível, até 12 (no 8) | alvo único / linha |
| Teia dos Sonhos | 7 no nível 3, +1 a cada 2 níveis, até 12 (no 13) | 3 hexes ao aprender, 1 anel no 9, 2 anéis no 15 |
| Troca de Lugar | 6 no nível 3, +1 a cada 2 níveis, até 10 (no 11) | alvo único / linha |
| Imagem Espelhada | 6 no nível 7, +1 a cada 3 níveis, até 9 (no 16) | alvo único / linha |
| Detonar Invocação | 6 no nível 9, +1 a cada 2 níveis, até 10 (no 17) | 3 hexes ao aprender, 1 anel no 15, 2 anéis no 23 |
| Pesadelo | 6 no nível 13, +1 a cada 2 níveis, até 12 (no 25) | 3 hexes ao aprender, 1 anel no 19, 2 anéis no 27 |
| Invocar Familiar | o do ponto de invocação, +1 a cada 2 níveis desde o 1, até 8 | alvo único / linha |
| Familiar Maior | o do ponto de invocação, +1 a cada 2 níveis desde o 3, até 8 | alvo único / linha |
| Familiar Radiante | o do ponto de invocação, +1 a cada 2 níveis desde o 6, até 8 | alvo único / linha |
| Familiar Titã | o do ponto de invocação, +1 a cada 2 níveis desde o 8, até 8 | alvo único / linha |
| Cão Zumbi | o do ponto de invocação, +1 a cada 2 níveis desde o 11, até 8 | alvo único / linha |
| Marca da Ruína | 6 no nível 15, +1 a cada 2 níveis, até 11 (no 25) | alvo único / linha |
| Praga Rastejante | 6 no nível 15, +1 a cada 4 níveis, até 9 (no 27) | alvo único / linha |
| Dreno de Vida | 5 no nível 16, +1 a cada 5 níveis, até 7 (no 26) | alvo único / linha |
| Mortalha de Sombras | 6 no nível 17, +1 por nível, até 12 (no 23) | 3 hexes ao aprender, 1 anel no 21, 2 anéis no 25 |
| Hemorragia | 5 no nível 18, +1 a cada 3 níveis, até 9 (no 30) | alvo único / linha |
| Silêncio Profano | 6 no nível 19, +1 a cada 3 níveis, até 10 | 3 hexes ao aprender, 1 anel no 23, 2 anéis no 28 |
| Peste Negra | 7 no nível 21, +1 a cada 4 níveis, até 9 (no 29) | 3 hexes ao aprender, 1 anel no 24, 2 anéis no 27 |
| Maldição da Decadência | 6 no nível 22, +1 a cada 4 níveis, até 8 (no 30) | alvo único / linha |
| Colheita de Almas | 6 no nível 23, +1 por nível, até 12 (no 29) | 3 hexes ao aprender, 1 anel no 26, 2 anéis no 29 |
| Vazio Devorador | 7 no nível 25, +1 a cada 2 níveis, até 11 | alvo único / linha |
| Coluna de Fogo | 6 no nível 15, +1 a cada 3 níveis, até 11 (no 30) | 3 hexes ao aprender, 1 anel no 20, 2 anéis no 26 |
| Lança de Gelo | 7 no nível 15, +1 a cada 2 níveis, até 12 (no 25) | alvo único / linha |
| Tempestade Elétrica | 6 no nível 16, +1 a cada 2 níveis, até 12 (no 28) | 3 hexes ao aprender, 1 anel no 21, 2 anéis no 28 |
| Meteoro | 8 no nível 17, +1 por nível, até 12 (no 21) | 3 hexes ao aprender, 1 anel no 21, 2 anéis no 25 |
| Zero Absoluto | 6 no nível 18, +1 a cada 3 níveis, até 10 (no 30) | 3 hexes ao aprender, 1 anel no 22, 2 anéis no 26 |
| Cadeia de Raios | 7 no nível 19, +1 a cada 2 níveis, até 12 (no 29) | alvo único / linha |
| Fúria dos Céus | 8 no nível 23, +1 a cada 2 níveis, até 12 | alvo único / linha |
| Brasa Primordial | 7 no nível 25, +1 a cada 2 níveis, até 10 | 3 hexes ao aprender, 1 anel no 27, 2 anéis no 30 |
| Tiro Longo | 7 no nível 1, +1 a cada 2 níveis, até 12 (no 11) | alvo único / linha |
| Tiro Sangrento | 6 no nível 5, +1 a cada 2 níveis, até 12 (no 17) | alvo único / linha |
| Tiro Múltiplo | 6 no nível 7, +1 a cada 2 níveis, até 12 (no 19) | alvo único / linha |
| Chuva de Flechas | 6 no nível 7, +1 a cada 2 níveis, até 12 (no 19) | 3 hexes ao aprender, 1 anel no 13, 2 anéis no 21 |
| Tempestade de Flechas | 7 no nível 13, +1 a cada 2 níveis, até 12 (no 23) | 3 hexes ao aprender, 1 anel no 17, 2 anéis no 25 |
| Flecha Envenenada | o do ponto de invocação, +1 a cada 3 níveis desde o 2, até 12 | alvo único / linha |
| Flecha Explosiva | o do ponto de invocação, +1 a cada 3 níveis desde o 10, até 12 | alvo único / linha |
| Tiro Paralisante | o do ponto de invocação, +1 a cada 3 níveis desde o 12, até 12 | alvo único / linha |
| Tiro Mortal | o do ponto de invocação, +1 a cada 3 níveis desde o 15, até 12 | alvo único / linha |
| Veneno Menor (plantas) | 5 no nível 8, +1 a cada 4 níveis, até 7 (no 16) | 3 hexes ao aprender, 1 anel no 16, 2 anéis no 24 |
| Bênção | em volta de quem lança | 3 hexes ao aprender, 1 anel no 11, 2 anéis no 19 |
| Expulsar Mortos-Vivos | em volta de quem lança | 3 hexes ao aprender, 1 anel no 14, 2 anéis no 22 |
| Aura de Proteção | em volta de quem lança | 3 hexes ao aprender, 1 anel no 20, 2 anéis no 25 |
| Presença Intimidante | em volta de quem lança | 3 hexes ao aprender, 1 anel no 21, 2 anéis no 27 |
| Erupção | em volta de quem lança | 3 hexes ao aprender, 1 anel no 24, 2 anéis no 28 |
