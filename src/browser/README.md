# Browser core MVP

Núcleo sem UI para um navegador experimental de e-ink. Não altera `ActivityManager`,
`HomeActivity`, `platformio.ini` ou o simulador.

## API de integração

```cpp
#include "browser/BrowserCore.h"

xpoint::browser::BrowserCore browser;
if (browser.open("https://example.org")) {
  const auto& page = browser.document();
  // page.title, page.text, page.links[i].url/text
}
browser.goBack();
browser.goForward();
browser.bookmarks().add(page.url.c_str(), page.title.c_str());
```

`BrowserHtmlParser` aceita chunks arbitrários e não depende de fronteiras de
tags. `BrowserHistory`, `BrowserBookmarks` e `BrowserCacheIndex` usam somente
vetores de capacidade fixa. `IBrowserCacheStorage` permite que a integração
grave o payload em SD, LittleFS ou PSRAM sem obrigar o núcleo a alocar memória.

O parser exibe texto, título e links relativos/absolutos. CSS inline e em até
dois blocos `<style>` (sem `type` ou com `type="text/css"`) aceita somente
`display:none`, `white-space:pre|normal` e
`text-transform:uppercase|lowercase|none` (transformação ASCII). Cada bloco
tem no máximo 511 bytes. No total, até oito regras são guardadas. Cada seletor
tem no máximo 31 bytes e deve ser um único nome de tag, `.classe` ou `#id`.
`#id` vence `.classe`, que vence tag; empate favorece regra posterior; estilo
inline vence todos. `display:none` herdado não pode ser revertido.

Comentários CSS são aceitos entre regras. Seletores compostos, listas,
pseudoclasses, blocos aninhados (`@media` etc.), folhas externas, variáveis,
fontes, cores e layout não são interpretados. Um bloco de declarações com
aspas, funções, comentários ou barras é ignorado por inteiro. Um `<` fora do fechamento
de `<style>` ou bloco acima do limite faz ignorar o bloco inteiro. Regras só
afetam elementos encontrados depois do fechamento do `<style>`; não há recálculo
de elementos anteriores nem DOM geral. `white-space:pre` preserva espaços e
quebras, mas o widget ainda ajusta linhas à largura da tela. Tags têm limite de
255 bytes; pilha de elementos tem 64 entradas. Além desse limite, parser
continua exibindo texto e ignora novos estilos até sair da profundidade
excedente. Texto fica limitado a 8191 bytes.

Reconhecimento literal de script: até oito blocos `<script>` sem `src` ou
`type`, com no máximo 255 bytes cada, podem conter exclusivamente uma chamada
`document.write('texto literal')` (aspas simples ou duplas, espaços e `;`
opcionais; escapes `\\`, aspas, `\n`, `\r`, `\t`). A saída é texto simples, não
HTML, e fica limitada a 160 bytes por bloco. Qualquer outra sintaxe é ignorada
por inteiro; não há execução de JavaScript arbitrário, `eval`, rede, variáveis,
laços, DOM ou temporizadores.
Cada bloco percorre no máximo seu buffer fixo, sem execução de código da página.
Essa extensão é viável para mensagens estáticas, mas não executa aplicações web.

Botões Próximo/Anterior leem o texto por páginas, com uma linha de
sobreposição; no fim, a navegação passa para os links. Anterior no primeiro
link volta ao texto. Gestos verticais percorrem o texto e, ao chegar ao limite,
rolam a lista de links. O widget de texto aceita `topLine` e é medido com
`textAreaMeasure` na largura e fonte reais. A limitação é que gestos não
distinguem onde começaram: mesmo sobre a lista, rolam o texto primeiro quando
há mais linhas nessa direção. O carregamento usa o callback streaming de
`HttpDownloader`, portanto não mantém o HTML bruto inteiro na RAM.
