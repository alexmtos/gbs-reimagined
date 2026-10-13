# GBS Reimagined — guia do usuário

Para quem tem uma RetroScaler GBS-C e quer jogar seus consoles em uma TV moderna ou em um monitor.
Não é preciso saber programar.

> **Este firmware ainda não está pronto para instalar.** Este guia cresce a cada etapa; a primeira
> versão instalável chega com a etapa M1 ([roadmap, em inglês](../../roadmap.md)).

## O que a placa faz

A GBS-C recebe o vídeo analógico de um console ou computador e o transforma em uma imagem que a sua
tela aceita: HDMI para TVs modernas, ou VGA / RGBS para monitores e CRTs. O GBS Reimagined é o
software que roda no pequeno computador da placa e decide como essa conversão é feita.

Você poderá controlá-lo de três formas:

- **Pelo celular ou computador** na mesma rede Wi-Fi, em uma página web com todas as opções
  explicadas.
- **Pelo botão giratório e pela telinha** da própria placa — isso sempre funciona, mesmo quando a TV
  não mostra nada.
- **Por um terminal serial USB**, para usuários avançados e chaveadores de consoles.

## Chaves e botões da própria placa

Alguns controles são ligados diretamente ao caminho do vídeo; o firmware não consegue ver a
posição deles, então confira-os quando algo parecer errado. A função exata de cada um ainda está
sendo confirmada na bancada.

| Nome impresso na placa | Tipo |
|---|---|
| HDMI ON / OFF | chave deslizante |
| RGBHV / RGBS | chave deslizante |
| Compatibility mode | chave deslizante |
| RGBS Sync | botão com trava |
| Sync on Green | botão com trava |
| R / G / B | potenciômetros |
| Power | botão com trava |

## Em breve neste guia

1. Instalando o GBS Reimagined pelo navegador
2. Primeiro uso e conexão ao seu Wi-Fi
3. Escolhendo a resolução de saída para a sua tela
4. Ajustes de imagem
5. Solução de problemas, pela mensagem que a placa mostra
6. Atualizando e restaurando
