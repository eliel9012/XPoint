# X4 Pro — validação física pendente

Build e testes host **não** homologam o aparelho. Este roteiro exige um X4 Pro com
USB e microSD. Não grave um dispositivo de terceiros ou com dados importantes sem
confirmar a porta e fazer cópia do cartão.

## Preparar

1. Copie os livros e ajustes do microSD para um local seguro. Não compartilhe o
   arquivo de ajustes: ele pode conter credenciais.
2. Rode `pio run -e x4pro-ptbr` e registre tamanho do app/RAM estática.
3. Conecte **somente** o X4 Pro por USB. Identifique a porta com
   `pio device list`; confirme que o nome/VID/PID correspondem ao aparelho.
4. Grave com `pio run -e x4pro-ptbr -t upload --upload-port PORTA_CONFIRMADA`.
   Abra o serial a 115200 baud: `pio device monitor -p PORTA_CONFIRMADA -b 115200`.

## Percurso e dados a registrar

| Etapa | Ação | Evidência esperada |
| --- | --- | --- |
| Boot | Ligar/desligar e acordar 3 vezes | Biblioteca inicial; sem loop, panic ou falha de PSRAM |
| Controles | Toque, botões físicos, Home, Voltar, teclado e acentos por toque longo | Ação e retorno coerentes; nenhuma tela sem saída |
| EPUB | Abrir `test/language/RTL/RTL_test.epub`, um EPUB português com capa e outro sem, um EPUB grego; trocar pelo menos 20 páginas em cada, variar a fonte | Texto, pontuação/acentos e direção hebraica corretos; fallback de título onde não há capa |
| Memória | Antes/depois de cada etapa, usar Ajustes → Sistema → Terminal → `memory`; guardar linhas `MEM` do serial a cada 10 s | DRAM livre/mínima/maior bloco e PSRAM livre/mínima/maior bloco; sem OOM ou queda contínua entre abrir/fechar livros |
| Rede/TLS | Ajustar relógio, conectar Wi-Fi real e buscar Weather e SKY | Certificado verificado, respostas/erro úteis, token nunca aparece no serial |
| Navegador | Abrir página HTTP/HTTPS simples, seguir links, rolar até fim e voltar | Texto/links dentro do subconjunto documentado; sem travar/estourar heap |
| E-ink | Página, menu, descanso e despertar repetidos | Refresh legível, sem ghosting anormal ou toque fora da área |
| OTA | **Não executar** antes de configurar a chave de assinatura e validar recuperação via USB | Manifesto assinado, rejeição de assinatura inválida e rollback demonstrados |

Guardar logs com hora e versão/commit do firmware, mas **redigir SSID, senhas,
tokens e URLs privadas** antes de compartilhar. Falha de qualquer etapa mantém a
versão como experimental. O relatório final deve trazer valores mínimos reais de
DRAM/PSRAM e o maior bloco livre, não apenas o tamanho do binário.
