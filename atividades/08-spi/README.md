# 📦 QAD com SDCard — ESP32 + ESP-IDF

## 🎯 Objetivo

Expandir o projeto da **Atividade 7** para incluir a funcionalidade de gravação de dados em **cartão SD**, utilizando a interface **SPI** do ESP32. Os conceitos abordados incluem:

- Comunicação SPI
- Gravação em arquivos
- Leitura de temperatura via NTC
- Controle de LEDs, buzzer (PWM) e display LCD via I2C
- Manipulação de GPIOs com interrupção

---

## 🧰 Material Necessário

- ESP32-S3 (ESP-IDF)
- 4 LEDs (para exibição do estado de alarme)
- 2 botões (push buttons)
- 1 Buzzer (controle por PWM)
- 1 Display LCD com interface I2C
- 1 NTC (Sensor de temperatura analógico)
- 1 Cartão Micro SD (com adaptador)
- Conta no Wokwi: https://wokwi.com/

---

## 🔧 Passos da Atividade

### 1. Diagrama de Blocos Atualizado

- O ESP32 se conecta aos sensores, atuadores e periféricos de comunicação.
- Entradas: SW1, SW2 e Sensor NTC
- Saídas: LEDs, buzzer e LCD
- Comunicação SPI com SDCard
- Alimentação dos periféricos via fonte 5V

### 2. Esquemático Atualizado

- Inserir e conectar o módulo MicroSD ao barramento SPI
- Garantir a alimentação correta do display e SDCard (5V)
- Usar resistores e pinos compatíveis com o projeto base da Atividade 7

### 3. Desenvolvimento do Código

Crie um programa no **ESP-IDF** que:

- Lê a temperatura do NTC
- Exibe os valores no display LCD
- Sinaliza visualmente com LEDs
- Emite som via buzzer
- E **grava todas as leituras em um arquivo no SDCard**

---

## 🧩 Funcionalidades por Parte

### 🔘 Parte A – Botões

- **Botão A (GPIO4):** incrementa o valor da temperatura de alarme em **+5°C**
- **Botão B (GPIO1):** decrementa o valor da temperatura de alarme em **–5°C**

> 🔁 O debounce deve ser tratado por software, sem `delay`.  
> ⚠️ **Uso obrigatório de interrupções** para os botões.

---

### 🔊 Parte B – Buzzer com PWM

- O buzzer será ativado quando **NTC ≥ temperatura de alarme**
- O som é gerado via PWM com duty fixo
- O buzzer é desligado automaticamente quando a temperatura volta abaixo do limite

---

### 🖥️ Parte C – LCD I2C

- Conectado via **SDA (GPIO10)** e **SCL (GPIO18)**
- **Linha 1:** temperatura lida do NTC (ex: `NTC: 28`)
- **Linha 2:** temperatura de alarme atual (ex: `Al: 30`)
- O display deve ser atualizado **a cada mudança de estado**

---

### 💡 Parte D – LEDs

- **1 LED** aceso quando `∆T ≤ 20°C`
- **2 LEDs** acesos quando `∆T ≤ 15°C`
- **3 LEDs** acesos quando `∆T ≤ 10°C`
- **4 LEDs** acesos quando `∆T ≤ 2°C`
- **4 LEDs piscando** quando `NTC ≥ temperatura de alarme`
- Os LEDs **continuam piscando** enquanto a temperatura estiver acima do limite

---

### 💾 Parte E – SDCard

- Conectado via **SPI**:
  - MOSI: GPIO36
  - MISO: GPIO2
  - SCK:  GPIO35
  - CS:   GPIO38
- As leituras do NTC (temperatura) e temperatura de alarme são **gravadas em log.txt**
- Cada linha do arquivo deve ter o formato:

  ```
  NTC: 28, Alarme: 30
  ```

---

## ✅ Observações

- Debounce por software usando `esp_timer_get_time()`
- O uso de interrupções evita polling constante dos botões
- Todos os periféricos devem estar alimentados adequadamente (5V para SD e LCD)

---

