# Exemplo de uso de MQTT com ESP32
Este projeto demonstra como utilizar o protocolo MQTT com o microcontrolador ESP32, permitindo o controle de LEDs através de mensagens publicadas em tópicos específicos.

## Hardware
- ESP32 (devkitv1)
- LEDs (vermelho e verde)
- Botões (S1 e S2)
  
### Ligação
O led vermelho está ligado ao pino 14, com um resistor de 470 ohms em série para gnd.
O led verde está ligado ao pino 13, com um resistor de 470 ohms em série para gnd.
O botão S1  está ligado entre o pino 23 e VCC, ajustar resistor interno de pull-down no código.
O botão S2  está ligado entre o pino 22 e VCC, ajustar resistor interno de pull-down no código.

## MQTT

### Dados Broker MQTT
 - Host:	broker.hivemq.com
 - TCP Port:	1883
 - Websocket Port:	8000
 - TLS TCP Port:	8883
 - TLS Websocket Port:	8884

### Tópicos MQTT

|Topico|Valores aceitos|Descrição|
|------|----------------|---------|
|/aula/led/vermelho|ON ou OFF|Estado do led vermelho|
|/aula/led/verde|ON ou OFF|Estado do led verde|

## Requisitos

### Requisitos funcionais
- RF1: O sistema deve ser capaz se conectar a uma rede Wi-Fi utilizando SSID e senha ou Captive Portal (preferencial).
- RF2: O sistema deve ser capaz de se conectar a um broker MQTT e publicar/assinar tópicos.
- RF3: O sistema deve ser capaz de controlar o estado dos LEDs (vermelho e verde) através de mensagens MQTT.
- RF4: O sistema deve ser capaz de enviar mensagens MQTT quando os botões S1 e S2 forem pressionados, indicando o estado atual dos LEDs.
- RF5: O sistema NÃO deve mudar o estado dos LEDs quando os botões S1 e S2 forem pressionados, apenas enviar mensagens MQTT.
- RF6: O sistema deve apresentar mensagens de log no console, indicando eventos importantes, como conexão/desconexão da rede Wi-Fi, conexão/desconexão do broker MQTT, estado dos LEDs e eventos de pressionamento dos botões.
- RF7: O sistema deve permitir esquecer as credenciais de rede Wi-Fi salvas na memória flash (NVS) e entrar imediatamente no modo Captive Portal caso seja inicializado com ambos os botões (S1 e S2) pressionados simultaneamente durante o boot.
  
### Requisitos não funcionais
- RNF1: O sistema deve ser implementado utilizando o ESP-IDF na versão 6.1.
- RNF2: O sistema deve ser modular, com funções bem definidas e separadas em arquivos diferentes.
- RNF3: O sistema deve utilizar bibliotecas oficiais do ESP-IDF sempre que possível, evitando bibliotecas de terceiros, a menos que seja estritamente necessário. As assinaturas e APIs do ESP-IDF v6.1 devem ser validadas consultando o servidor MCP `espressif-documentation` (ferramenta `search_espressif_sources`).
- RNF4: O sistema deve ser capaz de reconectar automaticamente à rede Wi-Fi e ao broker MQTT em caso de falha na conexão, com tentativas de reconexão limitadas a 3 tentativas com intervalos de 5 segundos. Em caso de falha, o sistema deve voltar a criar a rede Wi-Fi para que o usuário possa reconfigurar as credenciais.
- RNF5: O sistema deve ser capaz de salvar as credenciais da rede Wi-Fi na memória flash do ESP32 para que ele possa se reconectar automaticamente à rede Wi-Fi na próxima inicialização.
- RNF6: O sistema deve ser capaz de lidar com falhas de conexão ao broker MQTT, implementando uma lógica de reconexão com tentativas limitadas e intervalos de 5 segundos entre as tentativas. Em caso de falha, o sistema deve notificar o usuário e permitir que ele reconfigure as credenciais da rede Wi-Fi, caso necessário.
- RNF7: Caso use o modo Captive Portal, o SSID da rede SoftAP gerada pelo ESP32 deve conter um identificador único baseado no MAC/Chip ID (ex: `ESP32-Botoes-XXXX`) para evitar conflito com múltiplos dispositivos no mesmo ambiente. A interface deve ser uma página web simples carregada de arquivo HTML, onde um campo apresenta um dropdown com as redes Wi-Fi disponíveis (realizar escaneamento, e ter botão para atualizar varredura) e outro campo para inserir a senha da rede selecionada. O usuário deve ser capaz de selecionar a rede desejada e inserir a senha correspondente, e o sistema deve salvar essas informações na memória flash do ESP32 para reconexão automática na próxima inicialização.
- RNF8: O tratamento das mensagens recebidas via MQTT deve ser feito de forma assíncrona, garantindo que o sistema continue funcionando normalmente enquanto aguarda mensagens.
- RNF9: Utilize máquinas de estado para gerenciar o fluxo do programa, garantindo que cada estado tenha uma única responsabilidade, encapsulada em funções específicas, e que as transições entre estados sejam claras e bem definidas, por exemplo, no tratamento de eventos de conexão/desconexão da rede Wi-Fi, conexão/desconexão do broker MQTT, e eventos de pressionamento dos botões e tratar mensagens recebidas via MQTT.