# 🔧 Controle de PWM para LED e Display LCD I2C – ESP32S3 (ESP-IDF + Wokwi)

## 🎯 Objetivo

Ampliar o projeto das atividades anteriores para introduzir o uso de **saídas PWM** e **comunicação serial (I2C)**, utilizando o **ESP32S3** com o **framework ESP-IDF**, em simulação no ambiente **Wokwi**.

Conceitos abordados:

- Geração de sinal PWM
- Controle de brilho de LED
- Transmissão de dados via I2C
- Uso de interrupções com botões
- Debounce por software (sem `delay`)

---

## 📦 Material Necessário

- ESP32S3
- 4 LEDs (para exibição do contador binário)
- 2 botões (push buttons)
- 1 LED adicional (para controle de brilho via PWM)
- 1 Display LCD com interface I2C
- Conta no Wokwi: https://wokwi.com/

---

## 📝 Passos da Atividade

### 1. Atualize o diagrama da Atividade 04/05 incluindo:

- ✅ 1 LED adicional para PWM
- ✅ 1 Display LCD (interface I2C)

### 2. Atualize o esquemático
- Conecte corretamente os novos módulos (LED e LCD)
- Utilize GPIOs disponíveis e registre a nova pinagem no código

### 3. Desenvolvimento do Código

Crie um projeto no ESP-IDF com as seguintes funcionalidades:

---

## 🔢 Parte A – Contador Binário com Botões

- **4 LEDs** exibem o valor binário do contador de 4 bits
- **Botão A**: incrementa o contador com passo atual (padrão: +1)
- **Botão B**: decrementa o contador com passo atual (padrão: -1)
- O contador é **circular** (valor entre 0x0 e 0xF)

> ⚠️ O contador deve sempre permanecer dentro do intervalo **[0x0, 0xF]**

---

## 💡 Parte A – PWM: Controle de Brilho do LED

- Utilize o **driver PWM (LEDC)** do ESP-IDF
- O **duty cycle** do PWM deve ser proporcional ao valor atual do contador:

| Valor do Contador | Brilho PWM |
|-------------------|------------|
| `0x0`             | Mínimo     |
| `0xF`             | Máximo     |

---

## 🖥️ Parte B – LCD I2C: Exibição do Contador

- Configure o barramento **I2C** e inicialize o display
- Exiba os dados conforme abaixo:

```
Linha 1: Valor em hexadecimal → "HEX: 0xA"
Linha 2: Valor em decimal     → "DEC: 10"
```

> O display deve ser **atualizado sempre que o contador mudar**

---

## 🧠 Requisitos Técnicos

- Uso de **interrupções de GPIO** para os botões
- **Debounce via software** com `esp_timer` (não usar `vTaskDelay`)
- Código estruturado com funções dedicadas a cada parte:
  - `init_gpio()`
  - `update_leds()`
  - `set_pwm_duty()`
  - `update_lcd_display()`
  - `ISR de botões`

---

## 🔄 Regras do Contador

- Contador de 4 bits:
  - Intervalo: `0x0` a `0xF` (0 a 15 decimal)
- Incremento/decremento por padrão: **±1**
- Ao ultrapassar `0xF`, volta para `0x0`, e vice-versa (circular)

---

## 📎 Observações

- Use um resistor em série com o LED PWM.
- A barra do símbolo do LED representa o **cátodo (-)**; o outro lado é o **ânodo (+)**.
- O resistor pode ser conectado **em qualquer lado do LED**, sendo mais comum no ânodo.

---

## ✅ Resultado Esperado

- Sistema responsivo e sem atrasos perceptíveis
- Controle preciso com botões e display LCD funcional
- Brilho do LED PWM variando conforme o valor exibido

