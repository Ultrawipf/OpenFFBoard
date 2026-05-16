# Logitech G27 Complete Wiring Guide for OpenFFBoard STM32F407VET6

# Guia Completo de Ligações do Logitech G27 para OpenFFBoard STM32F407VET6

---

## ⚡ Required: PA9 Pull-up Resistor / Resistor Pull-up no PA9

> **EN:** On the generic **STM32F407VET6 board**, a **10kΩ pull-up resistor must be soldered between PA9 and 5V** for the main firmware (F407VG) to boot correctly. Without this resistor, only the F407VG_DISCO firmware works. This is required due to VBUS sensing differences.
>
> **PT:** Na placa genérica **STM32F407VET6**, um **resistor pull-up de 10kΩ deve ser soldado entre PA9 e 5V** para o firmware principal (F407VG) iniciar corretamente. Sem esse resistor, apenas o firmware F407VG_DISCO funciona. Isso é necessário devido a diferenças na detecção VBUS.

| Connection | Description |
|------------|-------------|
| **PA9** ↔ **5V** | 10kΩ resistor (pull-up for VBUS sensing) |

---

## 📋 Table of Contents / Índice

1. [Overview / Visão Geral](#overview--visão-geral)
2. [⚡ PA9 Pull-up Resistor (Required)](#-required-pa9-pull-up-resistor--resistor-pull-up-no-pa9)
3. [Motor Connection / Conexão do Motor](#motor-connection--conexão-do-motor)
4. [Encoder Connection / Conexão do Encoder](#encoder-connection--conexão-do-encoder)
5. [Pedals Connection / Conexão dos Pedais](#pedals-connection--conexão-dos-pedais)
6. [Shifter Connection / Conexão do Câmbio](#shifter-connection--conexão-do-câmbio)
7. [Wheel Rim Buttons / Botões do Aro](#wheel-rim-buttons--botões-do-aro)
8. [Complete Pinout Summary / Resumo Completo da Pinagem](#complete-pinout-summary--resumo-completo-da-pinagem)

---

## Overview / Visão Geral

### EN - English

This guide documents all connections made to integrate a **Logitech G27 Racing Wheel** with a generic **[STM32F407VET6 board](http://pt.aliexpress.com/item/1005006882009420.html)** running OpenFFBoard firmware. The G27 consists of:

- **Wheel Base**: DC motor with optical encoder
- **Shifter**: H-pattern with analog axes + 16 SPI buttons (2x 74HC165)
- **Wheel Rim**: 8 buttons (1x 74HC165) + LEDs (1x HC595)
- **Pedals**: 3 analog axes (throttle, brake, clutch)

### PT - Português

Este guia documenta todas as conexões feitas para integrar um **Volante Logitech G27** com uma placa genérica **[STM32F407VET6](http://pt.aliexpress.com/item/1005006882009420.html)** rodando firmware OpenFFBoard. O G27 consiste em:

- **Base do Volante**: Motor DC com encoder óptico
- **Câmbio**: H-pattern com eixos analógicos + 16 botões SPI (2x 74HC165)
- **Aro do Volante**: 8 botões (1x 74HC165) + LEDs (1x HC595)
- **Pedais**: 3 eixos analógicos (acelerador, freio, embreagem)

---

## Motor Connection / Conexão do Motor

### EN - Motor Wiring

The G27 uses a **DC motor** controlled via PWM through an H-bridge driver (like BTS7960).

| Function | OpenFFBoard Pin | Wire Color (typical) | Notes |
|----------|-----------------|---------------------|-------|
| PWM | PE11 | - | TIM1_CH2 PWM output |
| Direction | PE9 | - | Motor direction control |
| Enable | PE10 | - | Motor enable (DRV_BRAKE) |
| Motor + | BTS7960 M+ | Red/Orange | To H-bridge output |
| Motor - | BTS7960 M- | Black/Brown | To H-bridge output |

#### 🎮 G27 Motor Torque Curve Tuning

The G27 motors have specific characteristics that benefit from torque curve adjustment. For good force feedback response with minimal dead zone/backlash:

| Parameter | Recommended Value | Notes |
|-----------|-------------------|-------|
| Torque Curve | **0.60 ~ 0.70** | Reduces dead zone, improves response |

> **EN:** Setting the torque curve to 0.60-0.70 provides good FFB response with the G27 motors, reducing the "slack" feeling at center position.
>
> **PT:** Configurar a curva de torque em 0.60-0.70 proporciona boa resposta do FFB com os motores G27, reduzindo a sensação de "folga" na posição central.

### PT - Ligação do Motor

O G27 usa um **motor DC** controlado via PWM através de um driver H-bridge (como BTS7960).

| Função | Pino OpenFFBoard | Cor do Fio (típico) | Notas |
|--------|------------------|---------------------|-------|
| PWM | PE11 | - | Saída PWM TIM1_CH2 |
| Direção | PE9 | - | Controle de direção |
| Enable | PE10 | - | Habilitação (DRV_BRAKE) |
| Motor + | BTS7960 M+ | Vermelho/Laranja | Saída do H-bridge |
| Motor - | BTS7960 M- | Preto/Marrom | Saída do H-bridge |

---

## Encoder Connection / Conexão do Encoder

### EN - Encoder Wiring

The G27 originally uses an **optical quadrature encoder** (600 PPR) for position feedback.

| Function | OpenFFBoard Pin | G27 Wire | Notes |
|----------|-----------------|----------|-------|
| Encoder A | PA0 | Green | Quadrature signal A |
| Encoder B | PA1 | White | Quadrature signal B |
| Index (Z) | - | - | Not used on G27 |
| VCC | 5V | Red | Encoder power |
| GND | GND | Black | Ground |

#### Option A: Original G27 Encoder (600 PPR)
- Encoder Type: **ABN** (Quadrature)
- CPR (Counts Per Revolution): **2400** (600 PPR x4)

#### Option B: Magnetic Encoder Upgrade (MT6835) ⭐ Recommended

I replaced the original encoder with a **MT6835 ABZ magnetic encoder** mounted on the motor shaft. This provides much higher resolution and better FFB precision.

| Parameter | Value | Notes |
|-----------|-------|-------|
| Encoder | MT6835 ABZ | Magnetic, high resolution |
| CPR | **65535** | Maximum resolution |
| Gear Ratio | **11:180** | Motor gear:Wheel gear teeth |
| Mounting | [Thingiverse 1270001](https://www.thingiverse.com/thing:1270001) | 3D printed adapter (works with 555 motors too) |

The gear ratio 11:180 represents the relationship between the motor pinion (11 teeth) and the main wheel gear (180 teeth).

### PT - Ligação do Encoder

O G27 originalmente usa um **encoder óptico em quadratura** (600 PPR) para feedback de posição.

| Função | Pino OpenFFBoard | Fio G27 | Notas |
|--------|------------------|---------|-------|
| Encoder A | PA0 | Verde | Sinal de quadratura A |
| Encoder B | PA1 | Branco | Sinal de quadratura B |
| Index (Z) | - | - | Não usado no G27 |
| VCC | 5V | Vermelho | Alimentação do encoder |
| GND | GND | Preto | Terra |

#### Opção A: Encoder Original do G27 (600 PPR)
- Tipo do Encoder: **ABN** (Quadratura)
- CPR (Contagens Por Revolução): **2400** (600 PPR x4)

#### Opção B: Upgrade com Encoder Magnético (MT6835) ⭐ Recomendado

Substituí o encoder original por um **encoder magnético MT6835 ABZ** montado no eixo do motor. Isso proporciona resolução muito maior e melhor precisão do FFB.

| Parâmetro | Valor | Notas |
|-----------|-------|-------|
| Encoder | MT6835 ABZ | Magnético, alta resolução |
| CPR | **65535** | Resolução máxima |
| Relação de Engrenagens | **11:180** | Dentes engrenagem motor:aro |
| Montagem | [Thingiverse 1270001](https://www.thingiverse.com/thing:1270001) | Adaptador impresso 3D (funciona com motores 555 também) |

A relação 11:180 representa a proporção entre o pinhão do motor (11 dentes) e a engrenagem principal do aro (180 dentes).

---

## Pedals Connection / Conexão dos Pedais

### EN - Pedals Wiring

The G27 pedals use **3 potentiometers** for throttle, brake, and clutch. This connection follows the **standard OpenFFBoard wiki guide** - no modifications needed.

| Function | OpenFFBoard Pin | G27 Pedal Wire | Notes |
|----------|-----------------|----------------|-------|
| Throttle | Analog 1 (PA2) | - | Accelerator pedal |
| Brake | Analog 2 (PA3) | - | Brake pedal |
| Clutch | Analog 3 (PC3) | - | Clutch pedal |
| VCC | 3.3V | Red | Potentiometer power |
| GND | GND | Black | Ground |

> **Note:** This is the **standard connection** as documented in the [OpenFFBoard Wiki - Pinouts and Peripherals](https://github.com/Ultrawipf/OpenFFBoard/wiki/Pinouts-and-peripherals). No custom modifications were required.

#### Configuration / Configuração
- Analog Source: **Local Analog**
- Number of axes: **3**
- Calibrate each axis in the OpenFFBoard Configurator

### PT - Ligação dos Pedais

Os pedais do G27 usam **3 potenciômetros** para acelerador, freio e embreagem. Esta conexão segue o **guia padrão da wiki do OpenFFBoard** - nenhuma modificação necessária.

| Função | Pino OpenFFBoard | Fio Pedal G27 | Notas |
|--------|------------------|---------------|-------|
| Acelerador | Analog 1 (PA2) | - | Pedal do acelerador |
| Freio | Analog 2 (PA3) | - | Pedal do freio |
| Embreagem | Analog 3 (PC3) | - | Pedal da embreagem |
| VCC | 3.3V | Vermelho | Alimentação dos potenciômetros |
| GND | GND | Preto | Terra |

> **Nota:** Esta é a **conexão padrão** conforme documentado na [Wiki do OpenFFBoard - Pinouts and Peripherals](https://github.com/Ultrawipf/OpenFFBoard/wiki/Pinouts-and-peripherals). Nenhuma modificação customizada foi necessária.

---

## Shifter Connection / Conexão do Câmbio

### EN - Shifter Wiring

The G27 shifter has:
- **Analog axes** (X/Y) for gear position detection
- **2x 74HC165** shift registers for 16 buttons

| Function | OpenFFBoard Pin | G27 Connector Pin | Notes |
|----------|-----------------|-------------------|-------|
| VCC | 3.3V | 1 | Power (3.3V!) |
| SCK | PB13 | 2 | SPI Clock |
| MISO | PB14 | 4 | SPI Data |
| CS/Latch | PB12 | 6 | Chip Select (SPI2_SS1) |
| X Axis | PC0 (Analog 6) | 7 | Analog X position |
| Y Axis | PC1 (Analog 5) | 3 | Analog Y position |
| GND | GND | 9 | Ground |

#### Configuration / Configuração
- Mode: **G27 Shifter H-pattern** (ShifterAnalog mode 2)
- CS Pin: **1** (PB12)
- X Channel: **6** (PC0) - configurable via `shifter.xchan`
- Y Channel: **5** (PC1) - configurable via `shifter.ychan`

### PT - Ligação do Câmbio

O câmbio G27 tem:
- **Eixos analógicos** (X/Y) para detecção da marcha
- **2x 74HC165** registradores de deslocamento para 16 botões

| Função | Pino OpenFFBoard | Pino Conector G27 | Notas |
|--------|------------------|-------------------|-------|
| VCC | 3.3V | 1 | Alimentação (3.3V!) |
| SCK | PB13 | 2 | Clock SPI |
| MISO | PB14 | 4 | Dados SPI |
| CS/Latch | PB12 | 6 | Chip Select (SPI2_SS1) |
| Eixo X | PC0 (Analog 6) | 7 | Posição analógica X |
| Eixo Y | PC1 (Analog 5) | 3 | Posição analógica Y |
| GND | GND | 9 | Terra |

#### Configuração
- Modo: **G27 Shifter H-pattern** (ShifterAnalog modo 2)
- CS Pin: **1** (PB12)
- X Channel: **6** (PC0) - configurável via `shifter.xchan`
- Y Channel: **5** (PC1) - configurável via `shifter.ychan`

---

## Wheel Rim Buttons / Botões do Aro

> **Upstream PR vs Fork note:**
> - **PR #182 (upstream):** Contains only the ShifterAnalog G27 H-pattern fix. Wheel rim via SPI2 CS2 is the preferred upstream approach, but requires a **tri-state buffer** on MISO because the stock G27 74HC165 has no tri-state output.
> - **Tag `g27-full-working` (fork):** Contains SPI3 support for the wheel rim. Use this tag to build and flash firmware if you need both shifter + wheel rim working on stock G27 hardware today.

### EN - Wheel Rim Wiring

The G27 wheel rim has:
- **1x 74HC165** for 8 buttons
- **1x HC595AG** for LEDs (optional, not implemented)

> **Important:** The wheel rim buttons **cannot share the same SPI bus** with the shifter because the **stock G27 74HC165 lacks tri-state output**. The upstream preferred approach (SPI2 CS2) requires adding a tri-state buffer (e.g. 74HC125) on the MISO line. The fork workaround uses a **separate SPI port (SPI3)** — available in tag `g27-full-working`.

#### Option A: Upstream approach (SPI2 CS2, requires tri-state buffer)

| Function | OpenFFBoard Pin | Notes |
|----------|-----------------|-------|
| SCK | PB13 | Shared with shifter |
| MISO | PB14 | Via 74HC125 tri-state buffer |
| CS/Latch | PD8 | SPI2_SS2 |
| VCC | 3.3V | Power |
| GND | GND | Ground |

#### Option B: Fork workaround (SPI3, tag `g27-full-working`)

| Function | OpenFFBoard Pin | Wheel Rim Pin | Notes |
|----------|-----------------|---------------|-------|
| VCC | 3.3V | 1 | Power (3.3V!) |
| SCK | PC10 | 4 | SPI3 Clock |
| MISO | PC11 | 6 | SPI3 Data |
| CS/Latch | PA15 | 2 | SPI3_SS1 |
| GND | GND | 7 | Ground |
| MOSI | - | 3 | For LEDs (not used) |
| LED Latch | - | 5 | For LEDs (not used) |

#### Why the conflict? / Por que há conflito?

The 74HC165 shift register does **not have tri-state output**. When two 74HC165 chips share the same MISO line, they create electrical contention - both try to drive the line simultaneously. The upstream solution requires a buffer IC; the fork solution uses a separate SPI bus.

#### Configuration for Option B / Configuração para Opção B
- Class: **SPI Buttons 3** (fork only, uses SPI3)
- Buttons: **8**
- Mode: **74HC165** (PISOSR)
- CS Pin: **1** (PA15)

### PT - Ligação dos Botões do Aro

> **PR upstream vs Fork:**
> - **PR #182 (upstream):** Contém apenas o fix do câmbio ShifterAnalog. Aro via SPI2 CS2 é a abordagem preferida upstream, mas exige **buffer tri-state** no MISO porque o G27 stock não tem saída tri-state.
> - **Tag `g27-full-working` (fork):** Contém suporte a SPI3 para o aro. Use essa tag para compilar e gravar se precisar de câmbio + aro funcionando no hardware G27 stock hoje.

O aro do G27 tem:
- **1x 74HC165** para 8 botões
- **1x HC595AG** para LEDs (opcional, não implementado)

> **Importante:** Os botões do aro **não podem compartilhar o mesmo barramento SPI** com o câmbio porque o **G27 stock 74HC165 não tem saída tri-state**. A abordagem upstream (SPI2 CS2) requer adicionar um buffer tri-state (ex: 74HC125). O workaround fork usa **SPI3** — disponível na tag `g27-full-working`.

---

## Complete Pinout Summary / Resumo Completo da Pinagem

### STM32F407VET6 Pin Assignment / Atribuição de Pinos

```
┌─────────────────────────────────────────────────────────────┐
│            STM32F407VET6 - G27 CONNECTIONS                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ⚡ REQUIRED FOR F407VG FIRMWARE TO BOOT                    │
│  └── PA9 ────────── 5V (via 10kΩ resistor)                 │
│                                                             │
│  MOTOR (via BTS7960)                                        │
│  ├── PWM ────────── PE11                                    │
│  ├── Direction ──── PE9                                     │
│  └── Enable ─────── PE10 (DRV_BRAKE)                        │
│                                                             │
│  ENCODER                                                    │
│  ├── Encoder A ──── PA0                                     │
│  ├── Encoder B ──── PA1                                     │
│  ├── VCC ────────── 5V                                      │
│  └── GND ────────── GND                                     │
│                                                             │
│  PEDALS (Standard Wiki Connection)                          │
│  ├── Throttle ───── PA2 (Analog 1)                          │
│  ├── Brake ──────── PA3 (Analog 2)                          │
│  ├── Clutch ─────── PC3 (Analog 3)                          │
│  ├── VCC ────────── 3.3V                                    │
│  └── GND ────────── GND                                     │
│                                                             │
│  SHIFTER (SPI2 + Analog)                                    │
│  ├── SCK ────────── PB13                                    │
│  ├── MISO ───────── PB14                                    │
│  ├── CS ─────────── PB12 (SPI2_SS1)                        │
│  ├── X Axis ─────── PC0 (Analog 6)                          │
│  ├── Y Axis ─────── PC1 (Analog 5)                          │
│  ├── VCC ────────── 3.3V                                    │
│  └── GND ────────── GND                                     │
│                                                             │
│  WHEEL RIM BUTTONS (SPI3) - Separate bus required!          │
│  ├── SCK ────────── PC10                                    │
│  ├── MISO ───────── PC11                                    │
│  ├── CS ─────────── PA15 (SPI3_SS1)                        │
│  ├── VCC ────────── 3.3V                                    │
│  └── GND ────────── GND                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Summary Table / Tabela Resumo

| Component | Interface | Pins | Power | Notes |
|-----------|-----------|------|-------|-------|
| **VBUS Fix** | Resistor | PA9 → 5V | - | ⚡ **10kΩ required for F407VG firmware** |
| Motor | PWM | PE11 | External PSU | H-bridge required, torque curve 0.60-0.70 |
| Encoder (Original) | Quadrature | PA0, PA1 | 5V | ABN type, CPR 2400 |
| Encoder (MT6835) | Quadrature | PA0, PA1 | 5V | ABN type, CPR 65535, ratio 11:180 ⭐ |
| Pedals | Analog | PA2, PA3, PC3 | 3.3V | Standard wiki connection |
| Shifter | SPI2 + Analog | PB13, PB14, PB12, PC0, PC1 | 3.3V | G27 mode, X/Y analog axes |
| Wheel Rim | SPI3 | PC10, PC11, PA15 | 3.3V | Custom firmware required |

---

## 🔧 OpenFFBoard Configurator Settings / Configurações

### EN - Configuration Steps

1. **Motor Driver**
   - Type: PWM
   - PWM Pin: PE11
   - **Torque Curve: 0.60 ~ 0.70** (reduces dead zone)

2. **Encoder**
   - Type: Local ABN
   - **Option A (Original):** CPR: 2400
   - **Option B (MT6835):** CPR: 65535, Gear Ratio: 11:180

3. **Pedals (Local Analog)**
   - Analog Source: Local Analog
   - Axes: 3 (Throttle, Brake, Clutch)
   - Calibrate in configurator
   - ✅ Standard wiki connection - no modifications

4. **Shifter (Analog Shifter)**
   - Mode: G27 Shifter H-pattern (mode 2)
   - CS Pin: 1

5. **Wheel Rim (SPI Buttons 3)**
   - Buttons: 8
   - Mode: 74HC165
   - CS: 1
   - (Requires custom firmware with SPI_Buttons_3 class)

### PT - Passos de Configuração

1. **Driver do Motor**
   - Tipo: PWM
   - Pino PWM: PE11
   - **Curva de Torque: 0.60 ~ 0.70** (reduz zona morta)

2. **Encoder**
   - Tipo: Local ABN
   - **Opção A (Original):** CPR: 2400
   - **Opção B (MT6835):** CPR: 65535, Relação: 11:180

3. **Pedais (Local Analog)**
   - Fonte Analógica: Local Analog
   - Eixos: 3 (Acelerador, Freio, Embreagem)
   - Calibrar no configurador
   - ✅ Conexão padrão da wiki - sem modificações

4. **Câmbio (Analog Shifter)**
   - Modo: G27 Shifter H-pattern (modo 2)
   - CS Pin: 1

5. **Aro (SPI Buttons 3)**
   - Botões: 8
   - Modo: 74HC165
   - CS: 1
   - (Requer firmware customizado com classe SPI_Buttons_3)

---

## ⚠️ Warnings / Avisos

### EN - Important Warnings

1. **PA9 Resistor (STM32F407VET6)**: A **10kΩ pull-up resistor between PA9 and 5V is required** for the F407VG firmware to boot on generic STM32F407VET6 boards. Without it, only the DISCO firmware works.
2. **Voltage**: The shifter and wheel rim electronics work with **3.3V**. Do not apply 5V!
3. **SPI Bus Separation**: The wheel rim **must** use a separate SPI bus (SPI3) due to 74HC165 limitations.
4. **Firmware**: Wheel rim buttons via SPI3 require the fork firmware (tag `g27-full-working`). The upstream PR includes only the ShifterAnalog fix.

### PT - Avisos Importantes

1. **Resistor PA9 (STM32F407VET6)**: Um **resistor pull-up de 10kΩ entre PA9 e 5V é obrigatório** para o firmware F407VG iniciar em placas genéricas STM32F407VET6. Sem ele, apenas o firmware DISCO funciona.
2. **Tensão**: A eletrônica do câmbio e aro funciona com **3.3V**. Não aplique 5V!
3. **Separação do Barramento SPI**: O aro **deve** usar um barramento SPI separado (SPI3) devido às limitações do 74HC165.
4. **Firmware**: Os botões do aro requerem modificações customizadas no firmware (classe SPI_Buttons_3).

---

## 📊 Final Result / Resultado Final

| Component / Componente | Status | Windows Axes/Buttons |
|------------------------|--------|----------------------|
| Motor / Motor | ✅ Working / Funcionando | FFB enabled (torque curve 0.60-0.70) |
| Encoder / Encoder | ✅ Working / Funcionando | Steering axis (MT6835 65535 CPR) |
| Pedals / Pedais | ✅ Working / Funcionando | 3 axes (throttle, brake, clutch) |
| Shifter Gears / Marchas | ✅ Working / Funcionando | Buttons 1-7 (6 gears + reverse) |
| Shifter Buttons / Botões Câmbio | ✅ Working / Funcionando | Buttons 8-19 (12 buttons) |
| Wheel Rim / Aro | ✅ Working / Funcionando | Buttons 20-27 (8 buttons) |

**Total: 27 buttons + 7 gears (including reverse) + 3 pedal axes + FFB motor**

### Encoder Upgrade Notes / Notas do Upgrade do Encoder

> **EN:** The magnetic encoder MT6835 mounted with [this 3D printed adapter](https://www.thingiverse.com/thing:1270001) provides much better resolution and FFB feel compared to the original 600 PPR encoder. The adapter also works with 555 motors.
>
> **PT:** O encoder magnético MT6835 montado com [este adaptador impresso 3D](https://www.thingiverse.com/thing:1270001) proporciona resolução muito melhor e sensação do FFB comparado ao encoder original de 600 PPR. O adaptador também funciona com motores 555.

---

*Document Version: 1.1*
*Date: January 2026*
*Board: Generic STM32F407VET6*
*Hardware: Logitech G27 Racing Wheel*
*Encoder Upgrade: MT6835 ABZ Magnetic Encoder*
