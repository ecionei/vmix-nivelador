# Nivelador vMix

Plugin VST3 (Windows 64 bits) que mantém o áudio num nível constante:
sobe o que está baixo, abaixa o que está alto e nunca deixa estourar.

## Instalar

1. Feche o vMix.
2. Copie a pasta **`Nivelador vMix.vst3`** para
   `C:\Program Files\Common Files\VST3\`
3. Abra o vMix (testado para a versão 27, 64 bits).

## Onde colocar no vMix

| Onde | Modo | Para quê |
|---|---|---|
| Input **MESA DE SOM** | Voz (microfones) | Deixa todos os apresentadores e repórteres ao vivo no mesmo volume |
| **Master** | Programa (master) | Nivela vídeos de matérias, comerciais, vinhetas e trilhas, e protege a saída contra estouro |

Para adicionar: no Audio Mixer, clique na engrenagem do input (ou do Master),
abra a parte de efeitos **VST3** e escolha **Nivelador vMix**. Clique no
plugin para abrir a tela dele.

## Controles

- **Alvo**: volume final desejado, em LUFS. Botões prontos:
  - **Internet -14**: YouTube, Facebook, Instagram.
  - **vMix atual -18**: parecido com o "-6 de pico" que vocês usam hoje.
  - **TV -23**: padrão de emissora de TV.
- **Modo**:
  - **Voz**: reage mais rápido (ideal para microfones).
  - **Programa**: reage mais devagar e de forma mais natural (ideal para o Master).
- **Ganho entrada**: ajuste fixo antes do nivelador, se o sinal chegar muito baixo.
- **Subida máx.**: quanto o plugin pode aumentar, no máximo (padrão 12 dB).
- **Corte máx.**: quanto pode abaixar, no máximo (padrão 20 dB).
- **Gate silêncio**: abaixo desse nível o plugin entende que é silêncio ou ruído
  e **não sobe o ganho** (padrão -45 LUFS). Se o chiado do estúdio estiver subindo,
  aumente para -40.
- **Teto**: nível máximo de pico na saída (padrão -1 dBTP). Nada passa disso.
- **Nivelador / Limitador**: liga e desliga cada parte.

## Medidores

- **Entrada**: volume que está chegando.
- **Saída**: volume que está saindo (último 1 segundo). O traço branco é o alvo. Verde = no alvo.
- **Ganho**: quanto o plugin está subindo (azul) ou abaixando (amarelo), já somando o controle rápido.
- **Limitador**: quanto o limitador está segurando os picos.

## Detalhes técnicos

- Medição de loudness ITU-R BS.1770 (filtro K), em blocos de 50 ms, com gate:
  pausas e silêncio não entram na média.
- Janela de medição: 0,8 s (Voz) ou 3 s (Programa).
- Controle rápido (desde a versão 1.1): sons curtos e fortes, como batidas de
  vinheta, são abaixados em cerca de 5 ms quando passam do alvo (3 dB de folga
  no modo Voz, 2 dB no modo Programa) e voltam aos poucos.
- Limitador com lookahead de 1,5 ms e detecção de pico entre amostras (8x).
- Atraso total de cerca de 2 ms (imperceptível, não atrapalha a sincronia com o vídeo).
