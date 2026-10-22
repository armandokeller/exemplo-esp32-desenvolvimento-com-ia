# Botoes-MQTT: Controle e Telemetria com ESP32 e ESP-IDF v6.1

Sistema embarcado de alta confiabilidade desenvolvido para o microcontrolador **ESP32 DevKit v1** utilizando o framework oficial **ESP-IDF v6.1**. O projeto integra controle de periféricos (LEDs e botões com debounce assíncrono), provisionamento dinâmico de rede sem fio (**Captive Portal Web**) com persistência em memória flash (**NVS**), rotina de redefinição de fábrica de rede via botões no boot e mensageria bidirecional via protocolo **MQTT**.

---

## 1. Características do Sistema

- **Framework:** ESP-IDF v6.1 com suporte ao componente gerenciado `espressif/mqtt`.
- **Arquitetura Modular:** Separação estrita de responsabilidades em módulos desacoplados com event loops e filas FreeRTOS.
- **Provisionamento Wi-Fi Autônomo:**
  - SoftAP com SSID dinâmico baseado no endereço MAC físico: `ESP32-Botoes-XXXX`.
  - Servidor DNS UDP (porta 53) redirecionando todo o tráfego HTTP para o IP `192.168.4.1`.
  - Servidor Web HTTP com varredura assíncrona de redes Wi-Fi locais (`/scan`) e persistência segura na NVS (`/connect`).
  - Máquina de estados resiliente: reconexão automática em até 3 tentativas com intervalos de 5 segundos antes de retornar ao Captive Portal.
- **Factory Reset de Rede no Boot:** Ao inicializar a placa mantendo ambos os botões pressionados, as credenciais da NVS são apagadas e o Captive Portal é aberto imediatamente.
- **Mensageria MQTT Assíncrona:**
  - Conexão não bloqueante com o broker público `broker.hivemq.com:1883`.
  - Client ID exclusivo baseado no endereço MAC: `ESP32-Botoes-<MAC>`.
  - Controle remoto de LEDs por tópicos dedicados (`/aula/led/vermelho` e `/aula/led/verde`).
  - Telemetria de botões físicos que solicitam a alternância via broker sem acionamento local direto (respeitando rigorosamente os princípios de sistemas IoT distribuídos).

---

## 2. Pinagem e Mapa de Hardware

| Periférico | Pino (GPIO) | Direção | Configuração Elétrica | Função no Sistema |
| :--- | :---: | :---: | :--- | :--- |
| **LED Vermelho** | `GPIO 14` | Saída | Resistor de 470 Ω em série para GND | Indicador acionado exclusivamente via MQTT (`ON`/`OFF`) |
| **LED Verde** | `GPIO 13` | Saída | Resistor de 470 Ω em série para GND | Indicador acionado exclusivamente via MQTT (`ON`/`OFF`) |
| **Botão S1** | `GPIO 23` | Entrada | Pull-down interno (Ativo em VCC) | Solicita alternância do LED Vermelho via broker MQTT |
| **Botão S2** | `GPIO 22` | Entrada | Pull-down interno (Ativo em VCC) | Solicita alternância do LED Verde via broker MQTT |
| **S1 + S2 no Boot** | `GPIO 23 + 22` | Entradas | Mantidos em VCC durante a inicialização | **Reset de Fábrica:** apaga credenciais salvas e abre o Captive Portal |

---

## 3. Diagramas de Sequência de Funcionamento

### 3.1 Provisionamento Wi-Fi via Captive Portal
Quando o dispositivo não possui credenciais salvas na NVS ou falha ao conectar após 3 tentativas de 5 segundos, o modo Captive Portal é iniciado automaticamente:

```mermaid
sequenceDiagram
    autonumber
    actor User as Usuário (Celular/PC)
    participant AP as ESP32 SoftAP & DNS (192.168.4.1)
    participant Web as Servidor HTTP ESP32
    participant NVS as Memória Flash NVS
    participant STA as Driver Wi-Fi Station (STA)

    User->>AP: Conecta ao Wi-Fi "ESP32-Botoes-XXXX"
    User->>AP: Consulta DNS qualquer (ex: www.google.com)
    AP-->>User: Redireciona DNS para 192.168.4.1
    User->>Web: GET / (Abre o navegador)
    Web-->>User: Retorna página HTML com interface de configuração
    User->>Web: GET /scan (Varredura de redes)
    Web-->>User: JSON com lista de redes Wi-Fi detectadas
    User->>Web: POST /connect (SSID + Senha)
    Web->>NVS: Grava credenciais na partição "wifi_cfg"
    Web-->>User: Retorna confirmação de sucesso
    Web->>STA: Encerra SoftAP e inicia modo Station
    STA->>STA: Conecta ao ponto de acesso e obtém IP
```

---

### 3.2 Factory Reset de Rede no Boot (S1 + S2 Pressionados)
Se o usuário ligar ou resetar a placa segurando simultaneamente os botões S1 e S2:

```mermaid
sequenceDiagram
    autonumber
    actor User as Usuário
    participant Boot as ESP32 Bootloader / app_main
    participant GPIO as Controlador GPIO
    participant NVS as Memória Flash NVS
    participant Portal as Captive Portal SoftAP

    User->>Boot: Segura S1 e S2 e aperta botão EN (Reset)
    Boot->>GPIO: Inicializa GPIOs com pull-down
    Boot->>GPIO: gpio_controller_are_both_buttons_pressed()
    GPIO-->>Boot: Retorna TRUE (ambos os botões em nível alto)
    Boot->>NVS: storage_nvs_clear_wifi_credentials()
    NVS-->>Boot: Credenciais apagadas com sucesso
    Boot->>GPIO: Pisca LEDs Vermelho e Verde 5x rapidamente (feedback visual)
    Boot->>Portal: Inicia Captive Portal diretamente
    Portal-->>User: SSID "ESP32-Botoes-XXXX" disponível para nova configuração
```

---

### 3.3 Controle Remoto dos LEDs via MQTT
Controle dos LEDs físicos através de comandos externos recebidos do broker:

```mermaid
sequenceDiagram
    autonumber
    actor Client as Cliente Externo (App / Dashboard)
    participant Broker as HiveMQ Broker (broker.hivemq.com:1883)
    participant MQTT as Serviço MQTT ESP32
    participant LED as GPIOs dos LEDs (14 e 13)

    MQTT->>Broker: Subscrição em /aula/led/vermelho e /aula/led/verde
    Broker-->>MQTT: Confirmação de subscrição (SUBACK)

    Client->>Broker: Publica "ON" em /aula/led/vermelho
    Broker->>MQTT: Entrega mensagem (MQTT_EVENT_DATA: "ON")
    MQTT->>LED: gpio_controller_set_led(LED_RED, LED_STATE_ON)
    LED-->>LED: Pino GPIO 14 vai para nível alto (LED Vermelho acende)

    Client->>Broker: Publica "OFF" em /aula/led/vermelho
    Broker->>MQTT: Entrega mensagem (MQTT_EVENT_DATA: "OFF")
    MQTT->>LED: gpio_controller_set_led(LED_RED, LED_STATE_OFF)
    LED-->>LED: Pino GPIO 14 vai para nível baixo (LED Vermelho apaga)
```

---

### 3.4 Telemetria dos Botões e Alternância Indireta (RF3, RF4 e RF5)
Demonstração de como os botões operam de forma 100% desacoplada dos LEDs físicos, utilizando a rota do broker para garantir sincronismo:

```mermaid
sequenceDiagram
    autonumber
    actor User as Usuário
    participant Button as Botão S1 (GPIO 23)
    participant Task as Tarefa FreeRTOS (button_monitor_task)
    participant MQTT as Serviço MQTT ESP32
    participant Broker as HiveMQ Broker
    participant LED as LED Vermelho (GPIO 14)

    User->>Button: Pressiona o botão físico S1
    Button->>Task: Interrupção (ISR) envia evento para fila FreeRTOS
    Task->>Task: Validação de 30ms + consulta estado atual do LED
    Note over Task,MQTT: RF5: O pino do LED NÃO é alterado aqui localmente!
    Task->>MQTT: mqtt_service_publish_led_toggle(LED_RED)
    MQTT->>Broker: Publica "ON" em /aula/led/vermelho
    Task->>Button: Aguarda usuário soltar o botão (while pin == 1)
    Task->>Task: Aguarda 80ms e drena repiques mecânicos da fila

    Note over Broker,MQTT: O broker recebe a publicação e a retransmite aos inscritos
    Broker->>MQTT: Entrega "ON" no tópico /aula/led/vermelho
    Note over MQTT,LED: RF3: O LED só é acionado ao receber a mensagem do broker!
    MQTT->>LED: gpio_controller_set_led(LED_RED, LED_STATE_ON)
    LED-->>LED: LED Vermelho acende
```

---

## 4. Tópicos e Formato de Mensagens MQTT

| Tópico | Sentido | Valores Aceitos | Comportamento |
| :--- | :---: | :---: | :--- |
| `/aula/led/vermelho` | **RX / TX** | `"ON"` ou `"OFF"` | **RX:** Atualiza LED Vermelho.<br>**TX:** Enviado ao pressionar S1 com a alternância desejada. |
| `/aula/led/verde` | **RX / TX** | `"ON"` ou `"OFF"` | **RX:** Atualiza LED Verde.<br>**TX:** Enviado ao pressionar S2 com a alternância desejada. |

- **Broker:** `broker.hivemq.com`
- **Porta:** `1883` (TCP plaintext)
- **Client ID:** `ESP32-Botoes-<MAC>` (ex: `ESP32-Botoes-246F28600520`)

---

## 5. Estrutura Modular do Código

```
botoes/
├── CMakeLists.txt              # Configuração do projeto CMake
├── dependencies.lock           # Bloqueio de versões de dependências
├── AGENTS.md                   # Diretrizes técnicas e regras de desenvolvimento
├── ESPECIFICACAO.md            # Especificação de requisitos da disciplina
├── PRD.md                      # Product Requirements Document
├── PLANEJAMENTO.md             # Plano de execução e acompanhamento detalhado
├── README.md                   # Este documento com guia e diagramas
└── main/
    ├── CMakeLists.txt          # Fontes e componentes do módulo principal
    ├── idf_component.yml       # Dependência do componente gerenciado espressif/mqtt
    ├── main.c                  # Ponto de entrada (app_main), tarefas e orquestração
    ├── gpio_controller.h       # Interface de acionamento de LEDs e botões
    ├── gpio_controller.c       # Implementação com debounce por software e ISR
    ├── storage_nvs.h           # Interface de persistência de credenciais Wi-Fi
    ├── storage_nvs.c           # Implementação com NVS Flash e autoformatação
    ├── wifi_manager.h          # Interface do gerenciador Wi-Fi (STA + retry FSM)
    ├── wifi_manager.c          # FSM de conexão Wi-Fi e fallback para Captive Portal
    ├── captive_portal.h        # Interface do SoftAP, DNS e Web Server
    ├── captive_portal.c        # Servidores HTTP (esp_http_server) e DNS (UDP 53)
    ├── captive_portal_html.h   # Interface Web responsiva embutida
    ├── mqtt_service.h          # Interface do cliente MQTT assíncrono
    └── mqtt_service.c          # Subscrições, publicações e tratamento assíncrono
```

---

## 6. Instruções de Compilação e Gravação

### Pré-requisitos
- **ESP-IDF v6.1** instalado e configurado no ambiente.
- Cabo USB conectado à porta serial do ESP32 (ex: `/dev/cu.usbserial-0001` no macOS ou `/dev/ttyUSB0` no Linux).

### Passos de Execução

1. **Carregar o ambiente do ESP-IDF:**
   ```bash
   source $HOME/.espressif/v6.1/esp-idf/export.sh
   ```

2. **Compilar o projeto:**
   ```bash
   idf.py build
   ```

3. **Gravar na placa e abrir o monitor serial:**
   ```bash
   idf.py -p /dev/cu.usbserial-0001 flash monitor
   ```
   *(Para sair do monitor serial, utilize o atalho `Ctrl + ]`)*

---

## 7. Como Utilizar

1. **Primeira Inicialização:**
   - Ao ligar a placa pela primeira vez, o autoteste piscará os LEDs brevemente.
   - O ESP32 criará a rede Wi-Fi `ESP32-Botoes-XXXX`.
   - Conecte seu dispositivo a essa rede e abra `http://192.168.4.1` no navegador.
   - Selecione a sua rede Wi-Fi local, informe a senha e clique em **Conectar**.
   - O ESP32 salvará as credenciais na flash e conectará automaticamente.

2. **Operação Normal:**
   - Com o Wi-Fi ativo, o ESP32 conecta ao broker HiveMQ e subscreve aos tópicos.
   - Pressione **S1** para alternar o LED Vermelho via broker MQTT.
   - Pressione **S2** para alternar o LED Verde via broker MQTT.
   - Envie `"ON"` ou `"OFF"` a partir de qualquer cliente MQTT (HiveMQ Web Client, MQTT Explorer, app móvel) para controlar os LEDs remotamente.

3. **Esquecer a Rede (Factory Reset):**
   - Segure os botões **S1** e **S2** simultaneamente.
   - Pressione brevemente o botão **EN (Reset)** da placa ESP32.
   - Os LEDs piscarão 5 vezes rápido indicando que as credenciais foram apagadas e o Captive Portal será reaberto imediatamente.
