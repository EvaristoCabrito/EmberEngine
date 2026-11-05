# Tempo global e relogios

Tempo e um modulo de numeros em CPU (`engine/world/Tempo.h`). Alimenta os parametros dos efeitos existentes, sem substituir os perfis regionais. O consumidor recebe coberturas baixa/media/alta, forma, crescimento, alturas, precipitacao, raios, vento, umidade, nevoa, neblina, temperatura e molhado. Os efeitos existentes leem cobertura/altura/vento, chuva/raios, umidade/nevoa seca e multiplicadores da neblina regional.

A mesma semente, caos e instante usam aritmetica inteira de ponto fixo e produzem exatamente os mesmos valores. A avaliacao consulta um numero fixo de eventos e uma janela fixa de chuva anterior; nao percorre o tempo desde o inicio da campanha. Interpolacao entre minutos e cache ficam em Timeline. Mudancas de semente/caos/estado forcado transitam durante dez minutos de jogo. Saltos usam o estado calculado diretamente no destino. O ano tem 365 dias; o pico de verao fica no dia 172.

## Controles

Tempo comeca LIGADO em campanhas novas e antigas. Na campanha, TEMPO mostra apenas o estado atual, sem controles. O botao de controles e previsao existe somente no Modo teste; o editor mantem suas ferramentas de depuracao. Desligar continua disponivel por escolha manual. `--weather-seed N` habilita o calculo com a semente escolhida; `--weather-chaos X` aceita 0 a 1; `--weather off` o desliga. A ordem dos argumentos respeita a ultima escolha de modo. O painel Tempo global fornece modo, semente, caos, estados forcados e previsao de 72 horas para todas as grandezas. O editor tem uma previsao global de teste no Cenario. Estados forcados nao sao persistidos na campanha.

## Relogio do jogo

- Cada movimento no mapa-mundi e no submundo: 6 horas, inclusive na grade menor.
- Cada rodada completa de combate: 1 minuto. O turno individual de cada personagem nao adiciona minutos.
- Exploracao e dialogos: 1 segundo real = 1 segundo no calendario, com fracao de minuto preservada entre mapas e saves.
- Dia e HH:MM aparecem no mapa-mundi. Nao foi acrescentado relogio a interface de combate.
- Party mostra `Total campaign time: Xh YYmin`: horas reais jogadas, independentes de viagens e descanso. O save conserva os segundos. Saves antigos sem esse dado comecam do zero; horas anteriores nao podem ser reconstruidas.

## Duracao de venenos e doencas

| Potencia | Identificador | Duracao em dias | Duracao em horas | Duracao em minutos |
| --- | --- | ---: | ---: | ---: |
| Menor | lesser | 2 | 48 | 2880 |
| Comum | poison | 4 | 96 | 5760 |
| Maior | greater | 6 | 144 | 8640 |
| Mortal | deadly | 10 | 240 | 14400 |
| Letal | lethal | 20 | 480 | 28800 |

As flags antigas continuam legiveis. Os prazos absolutos ficam em heroPoisonTimers e heroDiseaseTimers. Saves antigos recebem prazos na primeira migracao, pois nao registravam o momento de infeccao. Snapshots de combate guardam minutos restantes, potencia e tipo. A expiracao da doenca restaura os atributos anteriores sem curar automaticamente um veneno independente. Doencas existentes de mordida e podridao mantem a potencia menor; inflictDisease aceita potencia e tipo explicitamente, sem inventar novas potencias para inimigos existentes.

## Validacao sem GPU

Compilar TempoCheck e TurnClockCheck pelo CMake. Rodar TempoCheck e TempoCheck jump em processos separados: os digests de 60 dias devem ser identicos. O teste cobre limites por minuto, frentes quentes com aviso de 6-8 horas, saltos, transicoes e independencias de taxa de quadros. TurnClockCheck cobre contagem por rodada, expiracao por tempo de exploracao, restauracao dos atributos, potencias, persistencia das horas reais e passos de 6 horas.
