# GBS Reimagined — guia do usuário

Para quem tem uma RetroScaler GBS-C e quer jogar seus consoles em uma TV moderna ou em um monitor.
Não é preciso saber programar.

> Instale pelo navegador em **https://alexmtos.github.io/gbs-reimagined/**: veja
> [Instalando pelo navegador](#instalando-pelo-navegador).

## O que a placa faz

A GBS-C recebe o vídeo analógico de um console ou computador e o transforma em uma imagem que a sua
tela aceita: HDMI para TVs modernas, ou VGA / RGBS para monitores e CRTs. O GBS Reimagined é o
software que roda no pequeno computador da placa e decide como essa conversão é feita.

Você pode controlá-lo de três formas:

- **Pelo celular ou computador** na mesma rede Wi-Fi, em uma página web com todas as opções
  explicadas.
- **Pelo botão giratório e pela telinha** da própria placa — isso sempre funciona, mesmo quando a TV
  não mostra nada.
- **Por um terminal serial USB**, para usuários avançados e chaveadores de consoles.

A página web e a telinha da placa falam inglês ou português do Brasil. Escolha o idioma no alto da
página web, ou em **Sistema → Idioma** no menu da placa; os dois mudam juntos.

Na página web, um ajuste que não tem efeito no momento (por exemplo a força das scanlines com as
scanlines desligadas) fica esmaecido e diz por quê, e **Padrão**, ao lado de um ajuste mudado,
devolve o valor padrão.

## Instalando pelo navegador

Você precisa da placa, de um computador com **Chrome** ou **Edge** (celulares e tablets não
instalam) e de um cabo USB que transmita dados (alguns cabos só carregam). A instalação substitui o
firmware que a placa tem agora, por exemplo o GBS-Control; os ajustes dele não passam para o novo.

1. Ligue a placa como de costume e conecte a porta micro-USB dela ao computador. A telinha acende
   como sempre.
2. Abra **https://alexmtos.github.io/gbs-reimagined/** e clique em **Instalar**. Escolha a porta
   chamada **USB-SERIAL CH340** (ou parecida) e clique em **Conectar**. A janela do instalador abre
   e oferece **Install GBS Reimagined**. Se nenhuma porta aparecer, veja [Se a instalação não
   funcionar](#se-a-instalação-não-funcionar).
3. A janela do instalador é em inglês. Escolha **Install GBS Reimagined** (instalar). Na primeira
   vez, marque **Erase device** (apagar o dispositivo), para não sobrar nada do firmware anterior,
   e escolha **Next** (próximo) e **Install**. Uma barra de progresso avança; não desconecte a
   placa até a janela dizer **Installation complete!** (instalação concluída).
4. Escolha **Next**. Se você marcou **Erase device**, a janela agora pede o seu Wi-Fi: escolha a sua
   rede, digite a senha e escolha **Connect** (conectar). A senha vai direto do navegador para a
   placa. Quando a placa entra na rede, o formulário fecha e aparece **Visit Device** (visitar o
   dispositivo), que abre a página da placa. Para fazer isso depois, escolha **Skip** (pular) e
   siga [Conectando ao seu Wi-Fi](#conectando-ao-seu-wi-fi). Se a janela não pedir, a placa
   manteve a rede que já conhecia, ou veja [Conectando ao seu Wi-Fi](#conectando-ao-seu-wi-fi).
5. Conecte o console a uma das entradas de vídeo da placa (SCART, as entradas RGBS, VGA ou
   componente) e a tela à saída HDMI dela, e depois ligue os dois. A placa procura o console em
   todas as entradas sozinha. Em alguns segundos a telinha diz **Escalando** e a tela mostra o
   jogo. A placa começa na resolução **Auto**: 1280x960 para consoles de 60 Hz, 1280x1024 para os
   de 50 Hz. Se a tela não mostrar nada, veja [Quando algo está errado](troubleshooting.md).

### Se a instalação não funcionar

- **Nenhuma porta na lista:** tente outro cabo e outra porta USB. No Windows, o chip USB da placa,
  um WCH CH340, pode precisar de driver: ele se chama CH341SER e vem do fabricante do chip, a WCH
  (wch-ic.com).
- **"Failed to open serial port"** (falha ao abrir a porta serial): feche todos os outros programas
  que usam a porta (Arduino IDE, PlatformIO, um monitor serial) e tente de novo.
- **A instalação parou no meio:** clique em **Instalar** e comece de novo. O carregador do próprio
  chip, com quem o instalador conversa, nunca é gravado, então uma instalação pela USB sempre pode
  ser repetida.
- **A placa não entra na sua rede:** confira a senha e se a rede é de 2,4 GHz. Clique em
  **Instalar** de novo; a janela encontra a placa e oferece **Connect to Wi-Fi** (conectar ao
  Wi-Fi) ou **Change Wi-Fi** (trocar o Wi-Fi).
- **A janela não pediu o seu Wi-Fi:** clique em **Instalar** de novo; a janela encontra a placa e
  oferece **Connect to Wi-Fi** (conectar ao Wi-Fi). Ou use a rede própria da placa, como em
  [Conectando ao seu Wi-Fi](#conectando-ao-seu-wi-fi).

## O botão giratório e a telinha

A telinha da placa funciona sempre, mesmo quando a TV não mostra nada. A tela inicial diz o que a
placa está fazendo (por exemplo "Escalando" e "240p > 1080p"), a entrada e a frequência dela, o
clock de saída e o endereço da placa na sua rede.

| Faça isto | Para |
|---|---|
| Aperte o botão | abrir o menu, entrar num item, confirmar um valor |
| Gire | andar pelo menu ou mudar um valor |
| Gire segurando o botão apertado | mudar um número de dez em dez |
| Segure por cerca de um segundo | voltar (e, editando, desfazer a mudança); na tela inicial, abrir os perfis |
| Aperte duas vezes rápido sobre um ajuste | devolver o valor padrão desse ajuste |

Os ajustes de imagem mudam enquanto você gira, para você acompanhar na TV. A resolução de saída e
o frame lock só mudam quando você aperta, para a imagem não passar por todos os modos no caminho.
A telinha volta para o início depois de 30 segundos sem uso e se apaga depois do tempo definido
em **Sistema → Proteção de tela**; qualquer giro ou toque a acende, e uma mudança de sinal também.

**Rede**, no menu, mostra como a placa está conectada. Aperte o botão nessa opção para ver um QR
code que a câmera do celular lê: na rede própria da placa, ele entra nessa rede; na sua, ele abre a
página web. A telinha fica mais escura enquanto o código aparece, e o código fica por um minuto;
aperte de novo, ou segure, para voltar.

**Modo de segurança.** Se algum ajuste deixar você sem imagem e sem a página web, reinicie a placa
e, **enquanto a primeira tela mostra "Segure: modo seguro"**, aperte e segure o botão por cinco
segundos. A placa então começa com os ajustes de fábrica e saída 480p, sem mudar os seus ajustes
salvos; reinicie normalmente para voltar a eles. (Não segure o botão enquanto liga a placa na
tomada: nesse momento o mesmo botão coloca o chip em modo de atualização.)

## Chaves e botões da própria placa

Alguns controles ficam ligados direto no caminho do vídeo; o firmware não consegue ver a posição
deles, então confira-os quando algo parecer errado. Isto é o que cada um fez na nossa bancada (um
console nas entradas RGBS da frente, saída HDMI):

| Nome na placa | O que faz |
|---|---|
| Knobs R / G / B | Diminuem o nível da entrada vermelha, verde ou azul. **Deixe no mínimo** (girados até o fim para baixo) para o sinal completo; girar um deles para cima escurece aquela cor. Úteis se as cores de um console parecerem estouradas mesmo depois de **Calibrar níveis**. |
| HDMI ON / OFF | Desliga a saída HDMI. Se a placa diz que está escalando mas a TV pelo HDMI não mostra nada, confira esta chave. |
| RGBS Sync | Muda como o sincronismo das entradas RGBS da frente é captado. Deixe solto, a não ser que a imagem role ou caia com a sua fonte. |
| Sync on Green | Tira o sincronismo do sinal verde: para fontes componente (YPbPr) e RGsB. |
| RGBHV / RGBS | Não fez diferença com as entradas RGBS da frente; ela diz respeito à entrada VGA. |
| Compatibility mode | Não fez diferença na entrada nem na saída HDMI; pode dizer respeito à saída VGA/RGBS. |
| Power | Liga a placa. |

## Conectando ao seu Wi-Fi

O instalador pede a sua rede logo depois de instalar. Se você pulou essa etapa, ou se a placa não
alcança a rede que conhece, ela abre uma rede própria. A placa só usa redes de 2,4 GHz: o chip de
Wi-Fi dela, um ESP8266, não tem rádio de 5 GHz.

1. Ligue a placa. Ela abre uma rede Wi-Fi chamada **GBS-** seguida de quatro letras e números
   únicos da sua placa (por exemplo `GBS-EAC4`). A senha é **`qqqqqqqq`** (oito letras q) — a
   mesma do projeto GBS-Control original.
2. Conecte o celular ou o computador a essa rede. Com o celular, dá para não digitar nada: na
   placa, aperte o botão, escolha **Rede** e aperte de novo. A telinha mostra um QR code; aponte a
   câmera do celular para ele (o app de câmera comum lê na maioria dos celulares) e aceite a rede
   que ela oferece. A página do GBS Reimagined abre
   sozinha; se não abrir, acesse **http://192.168.4.1** no navegador.
3. Vá na aba **Rede**, toque em **Procurar redes**, escolha a sua rede, digite a senha dela e toque
   em **Conectar**. A sua senha vai direto para a placa.
4. A placa entra na sua rede e fecha a rede própria cerca de um minuto depois (antes, se o celular
   sair dela). Volte o celular para a sua rede de sempre e abra **http://gbs.local**. Alguns
   celulares Android não entendem endereços `.local`: use o endereço mostrado na aba **Rede** ou o
   que o seu roteador mostra para a placa. Na placa, **Rede** e um aperto mostram esse endereço
   como um QR code, que abre a página pela câmera do celular.

Se a placa não conseguir falar com a sua rede por 30 segundos (roteador desligado, ou você trocou
a senha), ela abre a rede própria de novo para você corrigir, e continua tentando a sua rede a cada
dois minutos enquanto ninguém estiver conectado nela.

Para trocar de rede depois, use a aba **Rede**, ou a página do instalador: clique em **Instalar**,
escolha a porta da placa e depois **Change Wi-Fi** (trocar o Wi-Fi).

## Mudanças que pedem confirmação

Algumas mudanças podem deixar a TV sem imagem — a resolução de saída e o frame lock, por exemplo.
Quando você faz uma delas pela página web ou pelo console serial, um aviso pede que você
toque em **Manter**. Se você não responder em 20 segundos, a placa volta ao que era antes: uma
TV apagada se resolve sozinha. O tempo de espera pode ser mudado em **Sistema → Confirmar em**.
Carregar um perfil que muda um desses ajustes pede confirmação do mesmo jeito, e desfazer devolve
tudo o que o perfil mudou.

## Escolhendo a resolução de saída para a sua tela

A **Resolução de saída** fica na aba **Saída** e no menu da placa. **Auto**, o padrão, é o ponto de
partida: 1280x960 para consoles de 60 Hz e 1280x1024 para os de 50 Hz. Se a sua tela não mostrar
nada depois de uma mudança feita na página web, a placa volta sozinha depois de 20 segundos (veja
[Mudanças que pedem confirmação](#mudanças-que-pedem-confirmação)). Uma mudança feita pelo botão
vale quando você aperta e não é desfeita: se a tela continuar escura, escolha outra resolução na
placa, ou ligue-a no modo de segurança (veja
[O botão giratório e a telinha](#o-botão-giratório-e-a-telinha)).

| A sua tela | Experimente | Por quê |
|---|---|---|
| Uma TV pela HDMI | **1080p** | O que as TVs esperam. A imagem 4:3 ganha faixas pretas nas laterais; a **Escala inteira** deixa as linhas do 240p uniformes |
| Um monitor de PC | **Auto**: **1280x960** ou **1280x1024** | Jogos 240p saem exatamente quatro vezes mais altos em 1280x960; em 1280x1024 (consoles de 50 Hz) a imagem 4:3 ganha faixas finas em cima e embaixo |
| Uma TV ou monitor que não aceita 1080p | **720p** | Jogos 240p saem exatamente três vezes mais altos |
| Uma tela ou upscaler que pede uma resolução baixa | **480p / 576p** | Jogos 240p saem exatamente duas vezes mais altos em 480p, e os 288p em 576p |
| Um CRT na saída VGA / RGBS da placa | **15 kHz (CRT)** | Feito para um CRT. Ainda não testado com um na bancada do projeto, e uma tela HDMI pode não mostrar nada |
| Um monitor que prefere 1024x768, 800x600, 1600x1200 ou 1920x1200 | os modos **(exp.)** | Veja [Formato, posição e tamanho da imagem](#formato-posição-e-tamanho-da-imagem) |

Na bancada do projeto, a saída HDMI da placa deu imagem em 1280x960, 1280x1024, 1080p, 720p e
480p / 576p com todas as fontes testadas (um console 240p e um 480i em RGB, um Wii em 480p no
componente, um PS2 PAL), com o frame lock acompanhando o console. Foi conferido com uma placa de
captura, não em todo tipo de TV ([notas de bancada, M1, em inglês](../../bench/M1.md)).

O **Frame lock** (na mesma aba) faz a saída acompanhar exatamente a atualização do console: sem
trancos nem cortes na imagem. Deixe ligado; desligue só se a sua tela recusar a imagem com ele. Com
um console PAL, o Auto manda 50 Hz: se a sua tela só aceita 60 Hz, ligue **Forçar 60 Hz**.

## Formato, posição e tamanho da imagem

Na aba **Saída**:

- **Proporção** — o formato da imagem.
  - **4:3**, o padrão, mostra os consoles como foram feitos para ser vistos: com faixas pretas nas
    laterais em **1080p** e **720p**, ocupando a tela em **1280x960** e **480p / 576p**, e com
    faixas pretas finas em cima e embaixo em **1280x1024**, cuja tela é um pouco mais alta que 4:3.
  - **16:9** é para jogos com modo widescreen (GameCube, PS2, Wii): a imagem ocupa a largura em
    1080p e 720p; em 1280x960 e 1280x1024 ganha faixas pretas em cima e embaixo, para continuar
    correta num monitor 4:3 ou 5:4. Em 480p / 576p fica igual a 4:3.
  - **Tela cheia** ocupa a tela, seja qual for o formato dela, esticando a imagem.
  - **8:7** é um pouco mais estreito que 4:3 (seis sétimos da largura), o formato que muitos
    jogadores preferem para consoles que desenham 256 pixels por linha, como o Super Nintendo e o
    Nintendinho: os pixels deles ficam quadrados. Em **1280x1024** ele ocupa a altura toda da tela,
    onde o 4:3 tem faixas finas em cima e embaixo.
- **Posição horizontal** e **Posição vertical** — movem a imagem, para consoles que a colocam um
  pouco fora do centro. Cada passo vertical é uma linha da imagem do console. Onde a imagem chega à
  borda da tela, ela é cortada ali.
- **Escala inteira** — mostra cada linha do console com o mesmo número de linhas na tela (para
  jogos 240p: quatro em 1080p, onde a imagem fica normalmente 4,5 vezes mais alta), para scanlines e
  pixels saírem iguais. A imagem fica um pouco menor, com bordas pretas, e a **Altura** não tem
  efeito enquanto ela estiver ligada. Para jogos 240p não muda nada em 1280x960, 1280x1024, 720p e
  480p, que já os mostram em múltiplos exatos; para consoles PAL vale em todas as saídas. O
  múltiplo é contado nas linhas 240p (ou 288p) do console, então um menu 480i só fica exato em
  1080p e 1280x960.
- **Largura** e **Altura** — aumentam ou diminuem a imagem, cerca de meio por cento por passo. São
  ajustes avançados: ligue **Mostrar ajustes avançados**, no fim da página. É o mesmo ajuste que
  **Mostrar avançado** na aba **Sistema**: fica ligado, e o menu da placa também os mostra. Uma
  imagem maior que a tela é cortada nas bordas.

Algumas combinações não têm efeito, porque o scaler não consegue fazê-las sem defeito:

- em **15 kHz (CRT)** só a posição funciona, e o **Passthrough** não usa nenhum desses ajustes
  (ele aparece na lista de resoluções com **Mostrar ajustes avançados** ligado: ainda não foi
  verificado numa placa);
- em **1080p** a imagem não pode ficar mais estreita do que no início;
- em **1280x960**, **1280x1024** e **480p / 576p** ela não pode ficar mais larga do que no início,
  e em 480p / 576p também não pode ficar mais baixa.

Em 1080p, consoles PAL e a **Escala inteira** precisam de uma imagem mais estreita do que o scaler
consegue fazer sozinho: a placa passa então a desenhar cada linha em passos mais finos, o que a sua
tela não percebe, e a imagem sai na forma exata.

As resoluções de saída marcadas com **(exp.)** — 1024x768, 800x600, 1600x1200 e 1920x1200 — são para
monitores e TVs que preferem essas resoluções, como um monitor de PC 1920x1200 ou uma tela antiga
1024x768. Elas são calculadas a partir dos padrões de vídeo, e não tiradas do conjunto testado do
scaler, e foram experimentadas numa placa e numa TV. Se a sua tela não mostrar nada depois de
escolher uma delas pela página web ou pelo console serial, a placa volta à resolução anterior se
você não confirmar (veja [Mudanças que pedem confirmação](#mudanças-que-pedem-confirmação)). Com um
console PAL esses modos rodam a 50 Hz para manter o movimento suave; a saída HDMI da placa pode
então mostrar a imagem um pouco estreita e deslocada para a direita. **Forçar 60 Hz** corrige a
forma (com um pequeno tranco), ou ajuste a posição e o tamanho.

Forçar 60 Hz (na aba **Saída**) é para telas que não aceitam 50 Hz: um console PAL passa a ser
mostrado a 60 Hz. O frame lock não consegue segui-lo assim, então o movimento dá pequenos trancos
(um quadro em cinco aparece duas vezes); deixe desligado se a sua tela aceita 50 Hz. Em **480p** ele
mostra 480 das 576 linhas do console, então uma faixa em cima e outra embaixo da imagem ficam
cortadas; as outras resoluções mostram todas ([notas de bancada, B-05, em
inglês](../../bench/B05.md)).

## Ajustes de imagem

Na aba **Imagem**, e em **Imagem** no menu da placa. Eles mudam enquanto você gira, para você ver
na TV, e cada perfil guarda os seus.

- **Brilho**, **Contraste** e **Saturação** vão de -32 a +32; 0 mostra o console como ele envia.
  O brilho clareia ou escurece a imagem toda, o contraste aumenta ou diminui a diferença entre
  escuro e claro, a saturação deixa as cores mais fortes ou mais fracas. Se os brancos parecem
  estourados ou as cores lavadas, experimente antes os [Níveis de entrada](#níveis-de-entrada)
  (ajustes avançados, na aba **Entrada**): eles corrigem o próprio sinal.
- **Nitidez** realça os contornos: **Desligado**, o padrão, mostra a imagem como foi amostrada;
  **Baixa**, **Média** e **Alta** realçam cada vez mais.
- **Padrão**, ao lado de um ajuste mudado na página web, ou um toque duplo na placa, devolve o valor
  padrão.

## Scanlines e desentrelaçador

Na aba **Imagem**:

- **Scanlines** escurecem o espaço entre as linhas do console, como num CRT. Funcionam em jogos
  240p e 288p e pausam sozinhas enquanto o console manda 480i ou 576i (menus, alguns jogos),
  voltando quando ele retorna a 240p. A **Força das scanlines** vai de 1 (suave) a 8 (faixas
  pretas); linhas mais fortes deixam a imagem mais escura — cerca de um sexto no 4, o padrão, e
  cerca de dois quintos no 8.
- As linhas ficam perfeitamente iguais quando a saída é um múltiplo inteiro da imagem do console:
  **1280x960**, **720p** ou **480p** para jogos 240p. Em **1080p** as faixas alternam um pouco de
  espessura, o que algumas pessoas notam em telas grandes.
- **Desentrelaçador** (ajustes avançados) escolhe como imagens 480i e 576i são reconstruídas.
  **Adaptativo** (ao movimento), o padrão, mantém detalhes parados, como texto, nítidos e estáveis.
  **Bob** nunca mostra efeito pente em movimento rápido, mas faz linhas finas tremularem em imagens
  paradas.

## Amostragem e sync (avançado)

Na aba **Entrada**, com **Mostrar ajustes avançados** ligado:

- **Qualidade do sync** mostra quanto do último segundo a placa ficou travada nas linhas do
  console. O ideal é 100 %; qualquer valor menor significa que a imagem está, ou está prestes a
  ficar, instável.
- **Limiar de sync** separa o sincronismo da imagem nos consoles que mandam o sync no verde ou no Y
  do vídeo componente. Auto encontra sozinho um valor que funciona; escolha um número só se a
  imagem rolar ou cair, e acompanhe a qualidade do sync enquanto ajusta. Se a tela apagar, espere:
  a placa volta ao valor anterior em 20 segundos, a não ser que você confirme.
- **Fase de amostragem** escolhe em que ponto de cada pixel a entrada é amostrada. Faz pouca
  diferença em consoles 240p; experimente em fontes 480p ou de computador se detalhes finos
  tremularem.
- **Filtro passa-baixa** filtra a entrada antes da amostragem. Valores menores suavizam a imagem
  e podem acalmar uma fonte ruidosa; Auto escolhe conforme o tipo de fonte. Este também pede
  confirmação.

## Níveis de entrada

Alguns consoles mandam um sinal mais forte do que a placa espera, o que deixa os brancos chapados e
"estourados" e as cores lavadas. Duas ferramentas ajudam (ligue **Mostrar ajustes avançados**, aba
**Entrada**):

- **Calibrar níveis** — mostre uma tela totalmente branca no console (o 240p Test Suite tem uma;
  qualquer tela toda branca serve) e toque no botão. Leva alguns segundos e as cores podem piscar
  enquanto isso. A placa guarda o resultado para aquela entrada (RGB ou componente). Se a tela não
  for branca o bastante, ela avisa e não muda nada.
- **Ganho automático** — durante o jogo, reduz o nível da entrada sempre que as partes mais claras
  da imagem estouram. Só escurece, e recomeça a cada novo console conectado.

## Perfis

Um perfil guarda tudo o que está nas páginas **Saída**, **Entrada** e **Imagem**, para cada console
ter a sua imagem. Os ajustes de Sistema, a calibração de níveis e o Wi-Fi são os mesmos para todos
os perfis. Há 16 posições.

- **Salvar:** ajuste a imagem para um console e, na página **Perfis**, escolha uma posição, dê um
  nome e toque em **Salvar**. Marque **Carregar sozinho quando esta fonte aparecer** para a placa
  escolher este perfil sempre que esse console voltar. Salvar sobre uma posição ocupada pede
  confirmação: toque em **Substituir**.
- **Carregar:** toque em **Carregar** ao lado de um perfil, ou na placa: segure o botão giratório
  na tela inicial (ou abra **Perfis** no menu), escolha o perfil e **Carregar**.
- **Mudanças depois de carregar** ficam sem salvar: a página e a telinha da placa mostram o perfil
  como *alterado* (um `*` depois do nome, na placa) até você tocar em **Salvar aqui** nele.
  Carregá-lo de novo devolve os valores salvos.
- **Carregar sozinho:** quando a imagem de um novo console se firma, a placa carrega o perfil
  marcado para ele. Ela procura primeiro um salvo de um console na mesma entrada mandando o mesmo
  tipo de imagem (por exemplo 240p/480i, ou 480p) com o mesmo número de linhas, com folga de duas;
  depois com a mesma entrada e tipo de imagem; depois só com a mesma entrada. Um console que alterna
  entre 240p e 480i (ou 288p e 576i) nunca muda de perfil. Dois consoles que mandam exatamente o
  mesmo tipo de sinal na mesma entrada não podem ser distinguidos: carregue os perfis deles à mão ou
  use um seletor (abaixo).
- **Perfil inicial** (na página **Perfis**; na placa, em **Sistema**): *Último usado* começa de onde
  você parou, com as mudanças não salvas, a não ser que o perfil de outro console combine com a
  primeira imagem; *Auto pela entrada* sempre carrega o perfil que combina; um perfil numerado é
  carregado ao ligar.
- **Exportar e importar:** **Exportar** salva um perfil num arquivo; escolha uma posição e importe
  o arquivo nela, nesta placa ou em outra.
- **Cópia de segurança** (na aba **Sistema**): **Baixar uma cópia** salva todos os ajustes, a
  calibração de níveis e todos os perfis num arquivo (nunca a senha do Wi-Fi). **Restaurar uma
  cópia** pede confirmação, devolve o que o arquivo contém e diz o que não conseguiu; perfis em
  posições que o arquivo não menciona ficam como estão. Os ajustes de saída restaurados esperam o
  **Manter**, como qualquer mudança que possa apagar a tela. A restauração de fábrica mantém os
  perfis.
- **Seletores de console:** os comandos que seletores mandam para scalers RetroTINK funcionam aqui
  também: `SVS NEW INPUT=3` ou `remote prof3` carrega o perfil 3; `SVS CURRENT INPUT=3` carrega, a
  não ser que ele já esteja em uso e sem alterações. Mande-os de um computador ou de uma ponte pela
  porta USB da placa, ou pela rede para `http://gbs.local/api/svs`, por exemplo `curl -d "remote
  prof3" http://gbs.local/api/svs`. Ligar um seletor direto nos pinos seriais internos da placa não
  é suportado: essa linha também leva as mensagens da própria placa.

## Atualizando o firmware

Versões novas mantêm seus ajustes, perfis, calibração de níveis e Wi-Fi. Na página web, abra
**Sistema** → **Atualização do firmware**:

- **Procurar atualizações** procura uma versão mais nova no site do projeto, baixa, confere se
  chegou inteira e instala. O computador ou celular que mostra a página precisa estar na internet.
  Marque **Incluir versões beta** para receber também versões de teste.
- Ou instale a partir de um arquivo: baixe o arquivo terminado em **.ota.bin.gz** de uma versão
  publicada, escolha-o em **Atualizar a partir de um arquivo** e toque em **Instalar**. O outro
  arquivo, terminado só em **.bin**, é para o instalador pelo navegador, e a placa o recusa aqui.
- Ou pela USB, na página do instalador: clique em **Instalar**, escolha a porta da placa e depois
  **Update GBS Reimagined** (atualizar). Isso também mantém tudo.

O envio leva alguns segundos. Depois a placa reinicia sozinha, e a imagem e a página voltam em
cerca de 20 segundos. Deixe a placa ligada nesse tempo: se faltar energia enquanto ela copia a nova
versão, pode ser preciso instalar de novo pelo navegador, pela USB (escolha a mesma versão e não
apague a placa, para manter seus ajustes).

Antes de gravar qualquer coisa, a placa confere se o arquivo foi feito para ela, e recusa um feito
para outra placa. **Procurar atualizações** também recusa um download que chegou danificado. Se
uma atualização não for concluída, a placa continua com a versão que tinha.

## Quando algo está errado

A placa sempre diz o que está fazendo: na telinha dela, na aba **Status** e no `status` do
console. [Quando algo está errado](troubleshooting.md) lista cada mensagem com o que tentar, da
causa mais provável para a menos provável.

## Console serial (avançado)

Ligue a porta USB da placa a um computador e abra um terminal serial a **115200 baud**. Digite
`help` para ver a lista de comandos (o console é em inglês). Os mais úteis:

| Comando | O que faz |
|---|---|
| `status` | o que a placa está fazendo, com os números |
| `list` | todos os ajustes com o valor atual |
| `set picture.sharpness medium` | muda um ajuste (`get` lê um) |
| `output 1080p` / `input rgb` | atalhos para a resolução de saída e a entrada |
| `keep` / `revert` | responde a uma mudança que espera confirmação |
| `wifi` | o estado da rede; `wifi <nome> <senha>` conecta a uma rede |
| `calibrate` | calibra os níveis com uma tela branca |
| `profile` | os perfis salvos; `profile load 3`, `profile save 3 Mega Drive`, `profile source 3 on` |
| `factory-reset` | ajustes e níveis voltam ao padrão, os perfis ficam (digite duas vezes) |
