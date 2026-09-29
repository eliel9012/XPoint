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

O parser exibe texto, título e links relativos/absolutos. CSS inline aceita
somente `display:none`, `white-space:pre|normal` e
`text-transform:uppercase|lowercase|none` (transformação ASCII). Não interpreta
seletores, folhas `<style>`, fontes, cores ou layout. `white-space:pre` preserva
espaços e quebras, mas o widget ainda ajusta linhas à largura da tela. Tags
têm limite de 255 bytes; a pilha de elementos tem 64 entradas. Além desse
limite, o parser continua exibindo texto e ignora novos estilos até sair da
profundidade excedente. Texto fica limitado a 8191 bytes.

JavaScript mínimo: até oito blocos `<script>` sem `src` ou `type`, com no máximo
255 bytes cada, podem conter exclusivamente uma chamada
`document.write('texto literal')` (aspas simples ou duplas, espaços e `;`
opcionais; escapes `\\`, aspas, `\n`, `\r`, `\t`). A saída é texto simples, não
HTML, e fica limitada a 160 bytes por bloco. Qualquer outra sintaxe é ignorada
por inteiro; não existem `eval`, rede, variáveis, laços, DOM ou temporizadores.
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
