# G27 Wheel Rim Buttons - SPI Bus Conflict Report & Solution

## 🎯 Summary

The **G27 steering wheel rim buttons** (8 buttons via 74HC165) worked correctly when used **alone** on SPI2, but **failed when used simultaneously** with the G27 shifter on the same SPI bus. This document explains the root cause and the solution implemented using a separate SPI port.

---

## 📋 Hardware Setup

### G27 Wheel Rim
| Function | Pin (Original) | Description |
|----------|----------------|-------------|
| SCK | PB13 | SPI2 Clock |
| MISO | PB14 | SPI2 Data In |
| CS/Latch | PD8 | SPI2_SS2 (Wheel Rim CS) |
| VCC | 3.3V | Power |
| GND | GND | Ground |

The wheel rim contains:
- **1x 74HC165** shift register (8 buttons)
- **1x HC595AG** shift register (LEDs - not used)

### G27 Shifter (for reference)
| Function | Pin | Description |
|----------|-----|-------------|
| SCK | PB13 | SPI2 Clock (shared) |
| MISO | PB14 | SPI2 Data In (shared) |
| CS/Latch | PB12 | SPI2_SS1 (Shifter CS) |

---

## ✅ What Worked

### Wheel Rim Alone on CS2
When the wheel rim was connected **without the shifter**, using SPI Buttons with:
- `btnnum = 8`
- `mode = 1` (74HC165)
- `cs = 2` (PD8)

**Result:** ✅ All 8 buttons worked perfectly

### Shifter Alone on CS1
When the shifter was connected **without the wheel rim**, using ShifterAnalog G27 mode:
- `mode = 2` (G27 H-pattern)
- `cspin = 1` (PB12)

**Result:** ✅ All gears + reverse + 12 buttons worked perfectly

---

## ❌ What Didn't Work

### Both Devices on Same SPI Bus
When both devices were connected simultaneously:

| Configuration | Shifter | Wheel Rim |
|--------------|---------|-----------|
| ShifterAnalog CS1 + SPI Buttons CS2 | ❌ Buttons stopped working | ❌ Buttons stopped working |
| Only ShifterAnalog active | ❌ Buttons not reading | - |
| Only SPI Buttons active | - | ✅ Working |
| Wheel rim physically disconnected | ✅ Working | - |

**Key observation:** Simply having the wheel rim **physically connected** to the MISO line caused the shifter buttons to stop working, even with SPI Buttons disabled!

---

## 🔍 Root Cause: 74HC165 Has No Tri-State Output

### The Problem

The **74HC165 shift register does NOT have a tri-state (high-impedance) output**. This is a critical hardware limitation.

#### Normal SPI Device Behavior
Most SPI devices have tri-state outputs:
- When CS is **inactive** → Output goes **high-impedance** (disconnected)
- When CS is **active** → Output drives the MISO line

#### 74HC165 Behavior
The 74HC165 is different:
- When CS (SH/LD) is **inactive** → Output **still drives** the MISO line
- When CS is **active** → Output drives the MISO line

```
┌─────────────────────────────────────────────────────────────┐
│                    SPI BUS CONTENTION                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   74HC165 #1 (Shifter)     74HC165 #2 (Wheel Rim)          │
│        ┌───┐                    ┌───┐                       │
│   Q7 ──┤   ├──┐            Q7 ──┤   ├──┐                   │
│        └───┘  │                 └───┘  │                   │
│               │                        │                   │
│               └────────┬───────────────┘                   │
│                        │                                   │
│                        ▼                                   │
│                   MISO (PB14)  ← CONFLICT!                 │
│                                                             │
│   Both chips ALWAYS drive MISO, regardless of CS state.    │
│   This creates electrical contention and corrupted data.   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Why It Seemed to Work Sometimes

When only one device was actively being read, the other device's output might coincidentally not interfere. But as soon as both are polled (even at different times), the constant output driving causes:
- Data corruption
- Unpredictable readings
- One device "winning" over the other

---

## ✅ Solution: Use Separate SPI Bus

Since the 74HC165 chips cannot share a MISO line, the solution is to use a **completely separate SPI peripheral** for the wheel rim.

### New Hardware Configuration

| Device | Function | Pin | SPI Bus |
|--------|----------|-----|---------|
| **Shifter** | SCK | PB13 | SPI2 |
| **Shifter** | MISO | PB14 | SPI2 |
| **Shifter** | CS | PB12 | SPI2_SS1 |
| **Wheel Rim** | SCK | PC10 | SPI3 |
| **Wheel Rim** | MISO | PC11 | SPI3 |
| **Wheel Rim** | CS | PA15 | SPI3_SS1 |
| **Wheel Rim** | VCC | 3.3V | - |
| **Wheel Rim** | GND | GND | - |

```
┌─────────────────────────────────────────────────────────────┐
│                 SEPARATE SPI BUSES (SOLUTION)               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   SPI2 Bus (Shifter)              SPI3 Bus (Wheel Rim)     │
│   ┌─────────────┐                 ┌─────────────┐          │
│   │  74HC165    │                 │  74HC165    │          │
│   │  (x2)       │                 │  (x1)       │          │
│   └──────┬──────┘                 └──────┬──────┘          │
│          │                               │                  │
│   SCK ───┼─── PB13              SCK ────┼─── PC10          │
│   MISO ──┼─── PB14              MISO ───┼─── PC11          │
│   CS ────┼─── PB12              CS ─────┼─── PA15          │
│          │                               │                  │
│          ▼                               ▼                  │
│   ShifterAnalog                  SPI_Buttons_3             │
│   (G27 Mode)                     (8 buttons)               │
│                                                             │
│   ✅ No contention - completely separate MISO lines        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🛠️ Firmware Modifications

### 1. Created `SPI_Buttons_3` Class

**File:** `SPIButtons.h`
```cpp
class SPI_Buttons_3 : public SPI_Buttons {
public:
    SPI_Buttons_3();

    const ClassIdentifier getInfo() override;
    static ClassIdentifier info;
    static bool isCreatable();

    void restoreFlash() override;
    uint8_t readButtons(uint64_t* buf) override;  // Custom read for SPI3
    std::string getHelpstring() override {return "SPI 3 Button (SPI3)";}
};
```

### 2. Custom `readButtons()` for SPI3

The standard DMA-based read had timing issues with SPI3. Implemented synchronous read with manual CS pulse:

**File:** `SPIButtons.cpp`
```cpp
uint8_t SPI_Buttons_3::readButtons(uint64_t* buf){
    // Return last buffer immediately (non-blocking)
    memcpy(buf, this->spi_buf, std::min<uint8_t>(this->bytes, 8));
    process(buf);

    if(spiPort.isTaken())
        return this->conf.numButtons;

    SPI_HandleTypeDef* hspi = spiPort.getPortHandle();

    // Get CS pin (PA15)
    OutputPin* cs_pin = spiPort.getCsPin(this->conf.cs_num > 0 ? this->conf.cs_num - 1 : 0);

    if(cs_pin != nullptr) {
        // Pulse CS to latch parallel data into 74HC165
        cs_pin->write(false);  // CS LOW - load parallel data
        for(volatile int i = 0; i < 10; i++) {}  // Small delay
        cs_pin->write(true);   // CS HIGH - enable shift
    }

    // Synchronous SPI read
    HAL_SPI_Receive(hspi, spi_buf, bytes, 10);

    return this->conf.numButtons;
}
```

### 3. Hardcoded Configuration for Wheel Rim

```cpp
void SPI_Buttons_3::restoreFlash(){
    ButtonSourceConfig config;
    config.numButtons = 8;              // 8 buttons on wheel rim
    config.mode = SPI_BtnMode::PISOSR;  // 74HC165 mode
    config.cs_num = 1;                  // CS pin 1 (PA15)
    config.spi_speed = 2;               // Slow speed
    config.invert = false;
    config.cutRight = false;

    setConfig(config);
    this->conf.cs_num = 1;
    this->btnnum = 8;
}
```

### 4. SPI3 Port Configuration

**File:** `cpp_target_config.cpp`
```cpp
#ifdef EXT3_SPI_PORT
static const std::vector<OutputPin> ext3_spi_cspins{
    OutputPin(*SPI3_SS1_GPIO_Port, SPI3_SS1_Pin),  // PA15 - CS1
    OutputPin(*SPI3_SS2_GPIO_Port, SPI3_SS2_Pin),  // PD2  - CS2
    OutputPin(*SPI3_SS3_GPIO_Port, SPI3_SS3_Pin)   // PD3  - CS3
};
extern SPI_HandleTypeDef EXT3_SPI_PORT;
SPIPort ext3_spi{hspi3, ext3_spi_cspins, 42000000, true};
#endif
```

### 5. SPI3 Hardware Init

**File:** `main.c`
```c
static void MX_SPI3_Init(void)
{
    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    // ...
}
```

---

## 📁 Modified Files Summary

| File | Changes |
|------|---------|
| `SPIButtons.h` | Added `SPI_Buttons_3` class with custom `readButtons()` override |
| `SPIButtons.cpp` | Implemented `SPI_Buttons_3` with synchronous read + manual CS pulse |
| `cpp_target_config.cpp` | Defined SPI3 CS pins (PA15, PD2, PD3) |
| `main.c` | Configured SPI3 hardware (CLKPolarity, CLKPhase, BaudRate) |
| `stm32f4xx_hal_msp.c` | Adjusted SPI3 DMA/IRQ priorities |
| `OpenFFBoard_F407VG.ioc` | Updated SPI3 CubeMX configuration |

---

## ✅ Final Result

| Component | Status | Button Range |
|-----------|--------|--------------|
| G27 Shifter (SPI2) | ✅ Working | Buttons 1-19 (gears + 12 buttons) |
| G27 Wheel Rim (SPI3) | ✅ Working | Buttons 20-27 (8 buttons) |

---

## 💡 Alternative Solutions (Not Implemented)

If SPI3 pins are not available, other options include:

1. **External Tri-State Buffer** (e.g., 74HC125)
   - Add a buffer IC between each 74HC165 and the shared MISO line
   - Enable/disable buffer with CS signal
   - Allows sharing single MISO line

2. **Directly daisy-chain the 74HC165s**
   - Connect Q7' (serial out) of shifter's last 74HC165 to DS (serial in) of wheel rim's 74HC165
   - Read all buttons in one long SPI transaction
   - Requires firmware changes to handle 24 bits (3 bytes)

---

## 🔑 Key Takeaway

**74HC165 shift registers cannot share an SPI MISO line** due to lack of tri-state output. Each 74HC165 (or group of daisy-chained 74HC165s) requires either:
- A **dedicated SPI bus**, or
- An **external tri-state buffer** on the MISO line

---

# 🇧🇷 Relatório em Português

## Resumo

Os **botões do aro do volante G27** (8 botões via 74HC165) funcionavam corretamente quando usados **sozinhos** no SPI2, mas **falhavam quando usados simultaneamente** com o câmbio G27 no mesmo barramento SPI.

---

## O Que Funcionou

### Aro Sozinho no CS2
- Configuração: SPI Buttons, 8 botões, modo 74HC165, CS 2 (PD8)
- **Resultado:** ✅ Todos os 8 botões funcionaram

### Câmbio Sozinho no CS1
- Configuração: ShifterAnalog modo G27, CS 1 (PB12)
- **Resultado:** ✅ Todas as marchas + ré + 12 botões funcionaram

---

## O Que Não Funcionou

### Ambos no Mesmo Barramento SPI
Quando ambos estavam conectados:
- Apenas conectar fisicamente o aro já fazia os botões do câmbio pararem
- Os dois dispositivos não conseguiam coexistir no mesmo MISO

---

## Causa Raiz: 74HC165 Não Tem Saída Tri-State

O **74HC165 sempre mantém sua saída ativa**, mesmo quando o CS está inativo. Isso causa conflito elétrico quando dois 74HC165 compartilham a mesma linha MISO.

```
Dispositivo SPI normal:
  CS inativo → Saída em alta impedância (desconectada)
  CS ativo   → Saída conectada

74HC165:
  CS inativo → Saída CONTINUA conectada ❌
  CS ativo   → Saída conectada
```

Quando dois 74HC165 estão na mesma linha MISO, ambos tentam controlar a linha simultaneamente, causando:
- Corrupção de dados
- Leituras imprevisíveis
- Um dispositivo "vencendo" sobre o outro

---

## Solução: Usar Barramento SPI Separado

### Nova Configuração de Hardware

| Dispositivo | Função | Pino | Barramento |
|-------------|--------|------|------------|
| **Câmbio** | SCK | PB13 | SPI2 |
| **Câmbio** | MISO | PB14 | SPI2 |
| **Câmbio** | CS | PB12 | SPI2_SS1 |
| **Aro** | SCK | PC10 | SPI3 |
| **Aro** | MISO | PC11 | SPI3 |
| **Aro** | CS | PA15 | SPI3_SS1 |
| **Aro** | VCC | 3.3V | - |
| **Aro** | GND | GND | - |

---

## Modificações no Firmware

1. **Criada classe `SPI_Buttons_3`** para usar o SPI3
2. **Implementada leitura síncrona** com pulso manual do CS (o DMA tinha problemas de timing no SPI3)
3. **Configuração hardcoded** para 8 botões, modo 74HC165, CS 1 (PA15)
4. **Configurado SPI3** com os parâmetros corretos (CLKPolarity, CLKPhase, BaudRate)

---

## Resultado Final

| Componente | Status | Botões no Windows |
|------------|--------|-------------------|
| Câmbio G27 (SPI2) | ✅ Funcionando | Botões 1-19 |
| Aro G27 (SPI3) | ✅ Funcionando | Botões 20-27 |

---

## Lição Aprendida

**Registradores 74HC165 não podem compartilhar a linha MISO do SPI** devido à falta de saída tri-state. Cada 74HC165 (ou grupo em cadeia) precisa de:
- Um **barramento SPI dedicado**, ou
- Um **buffer tri-state externo** (como 74HC125) na linha MISO

---

*Report Date: January 2026*
*Board: Generic STM32F407VET6*
*Components: G27 Wheel Rim (8 buttons) + G27 Shifter*
