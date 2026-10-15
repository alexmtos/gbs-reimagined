# GBS Reimagined — quando algo está errado

Para quem usa o GBS Reimagined numa RetroScaler GBS-C. O resto do guia está em
[README.md](README.md).

A placa sempre diz o que está fazendo: na telinha dela, na aba **Status** e no `status` do
console. Procure a mensagem aqui.

**Sem sinal em RGB** (ou em Componente, ou Auto; na placa: *Sem sinal*) — a placa não ouve
nenhum console nessa entrada. Por alguns segundos depois de ligar a placa, isso é normal.

- Confira se o console está ligado e se o cabo está bem encaixado na placa.
- O ajuste **Entrada** (na aba **Entrada**, ou no menu da placa) deve estar em **Auto**, ou
  combinar com o cabo: **RGB** para SCART, as entradas RGBS e VGA; **Componente** para YPbPr.
- Cabos componente e com sync no verde precisam da chave **Sync on Green** da placa apertada (veja
  [Chaves e botões da própria placa](README.md#chaves-e-botões-da-própria-placa)).

**O sincronismo chega, mas não está estável** (*Sincronismo instável*) — a placa ouve o console, mas
perde o sinal o tempo todo.

- Confira o cabo e o encaixe, e as chaves **RGBS Sync** e **Sync on Green**.
- Para fontes componente e com sync no verde, experimente o **Limiar de sync** (aba **Entrada**,
  ajustes avançados) olhando a **Qualidade do sync**; veja
  [Amostragem e sync](README.md#amostragem-e-sync-avançado).

**Sinal encontrado, estabilizando** ou **Sincronizando com a fonte** — normal por alguns segundos
depois que um console é ligado ou muda de modo. Se não terminar, trate como sincronismo instável.

**Escalando 240p para 1280x960** (ou outro par), mas a tela não mostra nada:

- Confira a entrada selecionada na tela e a chave **HDMI ON / OFF** da placa.
- A tela pode não aceitar essa resolução. Na placa, abra o menu, **Saída**, **Resolução de saída**,
  escolha **1080p** ou **720p** e aperte o botão. Uma mudança feita na página web volta sozinha
  depois de 20 segundos, a menos que você toque em **Manter**.
- Se nada mais ajudar, ligue a placa no modo de segurança (veja
  [O botão giratório e a telinha](README.md#o-botão-giratório-e-a-telinha)).

**Escalando**, mas a imagem está errada: cores lavadas ou estouradas, veja
[Níveis de entrada](README.md#níveis-de-entrada); tamanho, formato ou posição, veja
[Formato, posição e tamanho da imagem](README.md#formato-posição-e-tamanho-da-imagem).

**O chip do scaler não está respondendo** (*Scaler não responde*) — o chip de vídeo da placa não
responde. Desligue a energia da placa por dez segundos e ligue de novo. Se a mensagem continuar, a
placa precisa de conserto: o firmware não funciona sem esse chip.

**Modo de segurança: os ajustes salvos não estão em uso e mudanças não são salvas** (na placa:
*MODO SEGURO*) — a placa foi ligada com o botão apertado (veja
[O botão giratório e a telinha](README.md#o-botão-giratório-e-a-telinha)). Reinicie normalmente
para voltar aos seus ajustes.

**A página web não abre:** use o endereço mostrado na tela inicial da placa (alguns celulares não
entendem `gbs.local`). Se a telinha mostrar a rede própria da placa (**GBS-** e quatro caracteres),
ela não conseguiu entrar na sua: veja [Conectando ao seu Wi-Fi](README.md#conectando-ao-seu-wi-fi).
