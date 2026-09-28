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

O parser exibe texto, título e links relativos/absolutos; ignora scripts, CSS,
imagens, formulários e JavaScript. O carregamento usa o callback streaming de
`HttpDownloader`, portanto não mantém o HTML bruto inteiro na RAM.
