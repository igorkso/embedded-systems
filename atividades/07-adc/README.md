# 🌡️ Sistema DAQ com Alerta Sonoro via NTC – ESP32 + ESP-IDF

Este projeto propõe o desenvolvimento de um sistema de aquisição de dados (DAQ) usando o ESP32 e o framework **ESP-IDF** para monitorar temperatura com um **sensor NTC**. O sistema emite um **alerta sonoro via PWM** quando a temperatura ultrapassa um limite configurável, exibindo os valores em um **display LCD I2C** e sinalizando aproximação via **LEDs**.

---

## 🎯 Objetivo

- Ler a temperatura com um sensor **NTC**;
- Permitir ajuste da **temperatura de alarme** com botões físicos;
- Gerar **alarme sonoro via PWM** quando necessário;
- Mostrar dados em um **display LCD I2C**;
- Indicar aproximação do limite via **LEDs**.

---

## 📦 Componentes Necessários

- ESP32 (ESP32S3)
- Sensor de temperatura NTC
- 2 botões (push buttons)
- 1 buzzer (saída PWM)
- 1 Display LCD com interface I2C
- 4 LEDs
- Resistores para botões e LEDs

---

## 🧱 Estrutura Funcional

### 🔘 Parte A – Botões: Ajuste de Temperatura de Alarme

- **Botão A**: Incrementa a temperatura de alarme em **+5°C** por acionamento
- **Botão B**: Decrementa a temperatura de alarme em **–5°C** por acionamento
- Temperatura de alarme inicial (default): **25°C**

> ⚠️ O debounce deve ser tratado por software (sem usar `delay`)  
> ✅ O uso de **interrupções (ISR)** para os botões é obrigatório

---

### 🔊 Parte B – PWM: Alarme Sonoro

- Utilize o **driver PWM (LEDC)** do ESP-IDF
- O buzzer será **ativado** quando a temperatura **ultrapassar o limite configurado**
- O buzzer só será **desligado** quando a temperatura **voltar a ser menor** que o limite

---

### 🖥️ Parte C – LCD I2C

- Configure o **barramento I2C**
- Mostre os dados no display LCD:

```
Linha 1: Temperatura NTC → "Temp: 27.0 C"
Linha 2: Alarme Config.  → "Alarme: 30 C"
```

- O display deve ser **atualizado sempre que os valores mudarem**

---

### 💡 Parte D – LEDs: Indicação de Aproximação

- Lógica de proximidade entre a temperatura do NTC e o valor de alarme:

| Diferença (°C) | Ação                  |
|----------------|------------------------|
| ≤ 20           | 1 LED aceso            |
| ≤ 15           | 2 LEDs acesos          |
| ≤ 10           | 3 LEDs acesos          |
| ≤ 2            | 4 LEDs acesos          |
| ≥ limite       | 4 LEDs piscando        |

- Os **4 LEDs devem continuar piscando** enquanto a temperatura estiver **acima do limite**

---

## 🔄 Resumo das Regras

- Temperatura lida do NTC em tempo real
- Buzzer e LEDs reagem dinamicamente
- Botões ajustam o alarme em incrementos de 5°C
- Sem uso de `delay` para debounce
- Uso obrigatório de interrupções para GPIOs dos botões

---

## 📎 Licença

MIT ou compatível com seu projeto.
