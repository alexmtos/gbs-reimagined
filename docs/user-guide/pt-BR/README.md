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

## O botão giratório e a telinha

A telinha da placa funciona sempre, mesmo quando a TV não mostra nada. A tela inicial diz o que a
placa está fazendo (por exemplo "Escalando" e "240p > 1080p"), a entrada e a frequência dela, o
clock de saída e o endereço da placa na sua rede.

| Faça isto | Para |
|---|---|
| Aperte o botão | abrir o menu, entrar num item, confirmar um valor |
| Gire | andar pelo menu ou mudar um valor |
| Gire segurando o botão apertado | mudar um número de dez em dez |
| Segure por cerca de um segundo | voltar (e, editando, desfazer a mudança) |

Os ajustes de imagem mudam enquanto você gira, para você acompanhar na TV. A resolução de saída e
o frame lock só mudam quando você aperta, para a imagem não passar por todos os modos no caminho.
A telinha volta para o início depois de 30 segundos sem uso e se apaga depois do tempo definido
em **Sistema → Proteção de tela**; qualquer giro ou toque a acende, e uma mudança de sinal também.

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

Na primeira vez, a placa não conhece a sua rede, então ela abre uma rede própria.

1. Ligue a placa. Ela abre uma rede Wi-Fi chamada **GBS-** seguida de quatro letras e números
   únicos da sua placa (por exemplo `GBS-EAC4`). A senha é **`qqqqqqqq`** (oito letras q) — a
   mesma do projeto GBS-Control original.
2. Conecte o celular ou o computador a essa rede. A página do GBS Reimagined abre sozinha; se não
   abrir, acesse **http://192.168.4.1** no navegador.
3. Vá na aba **Rede**, toque em **Procurar redes**, escolha a sua rede, digite a senha dela e toque
   em **Conectar**. A sua senha vai direto para a placa.
4. A placa entra na sua rede e fecha a rede própria cerca de um minuto depois (antes, se o celular
   sair dela). Volte o celular para a sua rede de sempre e abra **http://gbs.local**. Alguns
   celulares Android não entendem endereços `.local`: use o endereço mostrado na aba **Rede** ou o
   que o seu roteador mostra para a placa.

Se a placa não conseguir falar com a sua rede por 30 segundos (roteador desligado, ou você trocou
a senha), ela abre a rede própria de novo para você corrigir, e continua tentando a sua rede a cada
dois minutos enquanto ninguém estiver conectado nela.

## Mudanças que pedem confirmação

Algumas mudanças podem deixar a TV sem imagem — a resolução de saída e o frame lock, por exemplo.
Quando você faz uma delas pela página web ou pelo console serial, um aviso pede que você
toque em **Manter**. Se você não responder em 20 segundos, a placa volta ao que era antes: uma
TV apagada se resolve sozinha. O tempo de espera pode ser mudado em **Sistema → Confirmar em**.

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
| `factory-reset` | tudo volta ao padrão (digite duas vezes) |

## Em breve neste guia

1. Instalando o GBS Reimagined pelo navegador
2. Escolhendo a resolução de saída para a sua tela
3. Ajustes de imagem
4. Solução de problemas, pela mensagem que a placa mostra
5. Atualizando e restaurando
