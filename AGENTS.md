# Projeto de sistemas embarcados com ESP32

## Microcontrolador e bibliotecas

O microcontrolador utilizado é um ESP32, no devkitv1, e está sendo utilizado o ESP-IDF na versão 6.1.

## Regras gerais sobre o código

- No inicio do projeto, caso não seja especificado, solicite a forma com a qual o ESP deve se conectar à rede Wi-Fi:
  - Se for via SSID e senha, adicione as variáveis na configuração do projeto. (para usar com o menuconfig, e informe ao usuário como ele deve configurar)
  - Se for via Captive Portal, implemente a funcionalidade de Captive Portal, e informe ao usuário como ele deve configurar. Considere que o Captive Portal deve ser implementado de forma que, ao conectar-se à rede Wi-Fi do ESP32, o usuário seja redirecionado para uma página web onde ele possa inserir as credenciais da rede Wi-Fi desejada. Informe o IP padrão do ESP32 e como acessar a página de configuração. Considere salvar as credenciais na memória flash do ESP32 para que ele possa se reconectar automaticamente à rede Wi-Fi na próxima inicialização. Em caso de falha na conexão (em 3 tentativas com intervalos de 5 segundos), o ESP32 deve voltar a criar a rede Wi-Fi para que o usuário possa reconfigurar as credenciais.
- SEMPRE consulte a documentação oficial do ESP-IDF (conferindo a versão) utilizando o servidor MCP da Espressif:
  - **Servidor MCP:** `espressif-documentation`
  - **Ferramenta de busca:** `search_espressif_sources` (via `call_mcp_tool`)
  - **Parâmetros:**
    - `query`: busca semântica objetiva com indicação da versão quando relevante (ex: `"esp_mqtt_client_init ESP-IDF v6.1"`, `"wifi station configuration ESP-IDF v6.1"`)
    - `language`: `"en"` (padrão) ou `"cn"`
  - **Feedback (opcional):** ferramenta `give_feedback` informando o `interaction_id` retornado na busca.
- Sempre que possível, utilize as bibliotecas oficiais do ESP-IDF para implementar funcionalidades, evitando bibliotecas de terceiros, a menos que seja estritamente necessário.
- Implemente o código de forma modular, separando funcionalidades em arquivos diferentes e utilizando funções bem definidas.
- Siga o princípio de responsabilidade única, garantindo que cada função ou módulo tenha uma única responsabilidade.
- Evite muitos comentários no código, mas utilize comentários claros e concisos quando necessário para explicar trechos complexos. O código deve ser autoexplicativo sempre que possível.
  