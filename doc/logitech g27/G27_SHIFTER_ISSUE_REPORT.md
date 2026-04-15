# G27 Shifter - ShifterAnalog G27 Mode Bug Report & Fix

## 🎯 Summary

The **G27 Shifter buttons did not work** when using the official **ShifterAnalog "G27 Shifter H-pattern" mode** on a generic [STM32F407VET6 board](http://pt.aliexpress.com/item/1005006882009420.html) running OpenFFBoard firmware. After code analysis and modifications, the issue was identified and fixed.

---

## 📋 Hardware Setup

| Function | Pin | Description |
|----------|-----|-------------|
| SCK | PB13 | SPI2 Clock |
| MISO | PB14 | SPI2 Data In |
| CS/Latch | PB12 | SPI2_SS1 (Shifter Chip Select) |
| VCC | 3.3V | Power |
| GND | GND | Ground |

The G27 shifter contains:
- **2x 74HC165** shift registers (16 buttons total)
- **Analog axes** for X/Y position (gear detection)

---

## 🔴 Original Problem

### Symptoms
- ✅ **Analog axes worked** - Gear positions detected correctly
- ❌ **Buttons did NOT work** - No button presses registered in G27 mode
- ✅ **Same hardware worked with SPI Buttons** - When configured as standalone "SPI Buttons" with 16 buttons, mode 74HC165, CS 1, all buttons worked

---

## 🔍 Root Cause Analysis

After analyzing the original code, **5 bugs** were identified in `ShifterAnalog.cpp`:

### Bug 1: Wrong SPI Clock Configuration

**Original Code:**
```cpp
spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;
spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;
```

**Problem:** The 74HC165 shift register requires specific SPI timing. The original configuration didn't match the chip's requirements.

**Fix:**
```cpp
// 74HC165 configuration: sample on falling edge (phase 2, polarity high)
spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
```

---

### Bug 2: Missing CS Polarity Configuration

**Original Code:**
```cpp
// No cspol configuration
```

**Problem:** The CS (chip select) polarity was not explicitly set, causing incorrect latch behavior for the 74HC165.

**Fix:**
```cpp
// CS polarity LOW (active low) for 74HC165 mode
spiConfig.cspol = false;
```

---

### Bug 3: Asynchronous Read Without Waiting for Data

**Original Code:**
```cpp
bool ShifterAnalog::G27ShifterButtonClient::getReverseButton() {
    external_spi.receive_DMA(reinterpret_cast<uint8_t*>(&buttonStates), sizeof(buttonStates), this);
    return buttonStates & 0x02;  // Returns BEFORE DMA completes!
}
```

**Problem:** The function returned the button state **immediately after starting** the DMA transfer, not after it completed. This meant it always read stale/zero data.

**Fix:** Created a separate `startRead()` function and read from buffer after completion:
```cpp
void ShifterAnalog::G27ShifterButtonClient::startRead() {
    // Use synchronous read - waits for completion
    external_spi.receive(spi_buf, bytesToRead, this, 2); // 2ms timeout
}

bool ShifterAnalog::G27ShifterButtonClient::getReverseButton() {
    // Read from buffer AFTER DMA completed
    uint16_t buttonStates = (uint16_t)spi_buf[0] | ((uint16_t)spi_buf[1] << 8);
    return buttonStates & 0x02;
}
```

---

### Bug 4: readButtons() Didn't Trigger SPI Read

**Original Code:**
```cpp
uint8_t ShifterAnalog::readButtons(uint64_t* buf){
    updateAdc();
    updateReverseState();  // This triggered DMA but didn't wait
    calculateGear();
    *buf = 0;
    auto numUserButtons{getUserButtons(buf)};  // Read stale data
    // ...
}
```

**Problem:** The read sequence was wrong - it didn't wait for SPI data to be ready before processing.

**Fix:**
```cpp
uint8_t ShifterAnalog::readButtons(uint64_t* buf){
    updateAdc();

    if (g27ShifterButtonClient) {
        // Synchronous read - waits for bus and reads immediately
        g27ShifterButtonClient->startRead();

        // Now process the freshly read data
        updateReverseState();
        getUserButtons(buf);
    } else {
        updateReverseState();
        *buf = 0;
    }

    calculateGear();
    // ...
}
```

---

### Bug 5: CS Pin Index Off-by-One Error

**Original Code:**
```cpp
void ShifterAnalog::setMode(ShifterMode newMode) {
    // ...
    g27ShifterButtonClient = std::make_unique<G27ShifterButtonClient>(external_spi.getFreeCsPins()[0]);
}

void ShifterAnalog::setCSPin(uint8_t new_cs_pin_num) {
    // ...
    g27ShifterButtonClient->updateCSPin(*external_spi.getCsPin(cs_pin_num));  // Wrong index!
}
```

**Problem:**
1. `setMode()` always used the first free CS pin instead of the configured `cs_pin_num`
2. `setCSPin()` used `cs_pin_num` directly, but `getCsPin()` is 0-indexed while `cs_pin_num` is 1-indexed

**Fix:**
```cpp
void ShifterAnalog::setMode(ShifterMode newMode) {
    // ...
    // Use the configured CS pin (cs_pin_num is 1-indexed, getCsPin uses 0-indexed)
    OutputPin* csPin = external_spi.getCsPin(cs_pin_num - 1);
    if (csPin == nullptr) {
        // Fallback to first free pin if configured pin is invalid
        auto& freePins = external_spi.getFreeCsPins();
        if (!freePins.empty()) {
            csPin = &freePins[0];
        }
    }
    if (csPin != nullptr) {
        g27ShifterButtonClient = std::make_unique<G27ShifterButtonClient>(*csPin);
    }
}

void ShifterAnalog::setCSPin(uint8_t new_cs_pin_num) {
    // ...
    // cs_pin_num is 1-indexed, getCsPin uses 0-indexed
    OutputPin* newPin = external_spi.getCsPin(cs_pin_num - 1);
    if (newPin != nullptr) {
        g27ShifterButtonClient->updateCSPin(*newPin);
    }
}
```

---

## 📁 Modified Files

### ShifterAnalog.h

```diff
  class G27ShifterButtonClient : public SPIDevice {
      G27ShifterButtonClient(OutputPin& csPin);

      static constexpr int numUserButtons{12};
+     static constexpr int bytesToRead{2}; // 16 buttons = 2 bytes

      uint16_t getUserButtons();
      bool getReverseButton();
- private:
-     uint16_t buttonStates{0};
+     void startRead();
+     void spiRxCompleted(SPIPort* port) override;
+
+     uint8_t spi_buf[2]{0}; // Buffer for SPI read
  };
```

### ShifterAnalog.cpp

**G27ShifterButtonClient Constructor:**
```diff
  ShifterAnalog::G27ShifterButtonClient::G27ShifterButtonClient(OutputPin& csPin)
      :SPIDevice(external_spi,csPin) {
      spiConfig.peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
      spiConfig.peripheral.FirstBit = SPI_FIRSTBIT_LSB;
-     spiConfig.peripheral.CLKPhase = SPI_PHASE_1EDGE;
-     spiConfig.peripheral.CLKPolarity = SPI_POLARITY_LOW;
+     spiConfig.peripheral.CLKPhase = SPI_PHASE_2EDGE;
+     spiConfig.peripheral.CLKPolarity = SPI_POLARITY_HIGH;
+     spiConfig.cspol = false;
  }
```

**New startRead() function:**
```cpp
void ShifterAnalog::G27ShifterButtonClient::startRead() {
    external_spi.receive(spi_buf, bytesToRead, this, 2); // 2ms timeout
}
```

**getReverseButton() - Read from buffer instead of triggering DMA:**
```diff
  bool ShifterAnalog::G27ShifterButtonClient::getReverseButton() {
-     external_spi.receive_DMA(reinterpret_cast<uint8_t*>(&buttonStates), sizeof(buttonStates),this);
+     uint16_t buttonStates = (uint16_t)spi_buf[0] | ((uint16_t)spi_buf[1] << 8);
      return buttonStates & 0x02;
  }
```

**getUserButtons() - Read from buffer:**
```diff
  uint16_t ShifterAnalog::G27ShifterButtonClient::getUserButtons() {
+     uint16_t buttonStates = (uint16_t)spi_buf[0] | ((uint16_t)spi_buf[1] << 8);
      return buttonStates >> 4;
  }
```

**readButtons() - Proper read sequence:**
```diff
  uint8_t ShifterAnalog::readButtons(uint64_t* buf){
      updateAdc();

-     updateReverseState();
+     if (g27ShifterButtonClient) {
+         g27ShifterButtonClient->startRead();
+         updateReverseState();
+         getUserButtons(buf);
+     } else {
+         updateReverseState();
+         *buf = 0;
+     }
+
      calculateGear();

-     *buf = 0;
-     auto numUserButtons{getUserButtons(buf)};
-
      if(gear > 0){
+         uint64_t numUserButtons = g27ShifterButtonClient ? g27ShifterButtonClient->numUserButtons : 0;
          *buf |= 1 << (gear - 1 + numUserButtons);
      }

      return this->btnnum;
  }
```

**setMode() - Use configured CS pin:**
```diff
  void ShifterAnalog::setMode(ShifterMode newMode) {
      // ...
-     g27ShifterButtonClient = std::make_unique<G27ShifterButtonClient>(external_spi.getFreeCsPins()[0]);
+     OutputPin* csPin = external_spi.getCsPin(cs_pin_num - 1);
+     if (csPin == nullptr) {
+         auto& freePins = external_spi.getFreeCsPins();
+         if (!freePins.empty()) {
+             csPin = &freePins[0];
+         }
+     }
+     if (csPin != nullptr) {
+         g27ShifterButtonClient = std::make_unique<G27ShifterButtonClient>(*csPin);
+     }
  }
```

**setCSPin() - Fix off-by-one error:**
```diff
  void ShifterAnalog::setCSPin(uint8_t new_cs_pin_num) {
      // ...
      if (g27ShifterButtonClient) {
-         g27ShifterButtonClient->updateCSPin(*external_spi.getCsPin(cs_pin_num));
+         OutputPin* newPin = external_spi.getCsPin(cs_pin_num - 1);
+         if (newPin != nullptr) {
+             g27ShifterButtonClient->updateCSPin(*newPin);
+         }
      }
  }
```

---

## ✅ Result After Fix

| Feature | Status |
|---------|--------|
| Gear 1-6 detection | ✅ Working |
| Reverse gear | ✅ Working |
| 12 user buttons | ✅ Working |
| CS pin configuration | ✅ Working |

---

## 📊 Test Environment

| Parameter | Value |
|-----------|-------|
| Board | Generic STM32F407VET6 |
| Shifter | Logitech G27 H-Pattern |
| Connection | PB12 (CS), PB13 (SCK), PB14 (MISO) |
| Voltage | 3.3V |

---

## 🇧🇷 Resumo em Português

### Problema
Os **botões do câmbio G27 não funcionavam** no modo oficial "G27 Shifter H-pattern" do ShifterAnalog.

### Causa (5 bugs encontrados)
1. **Configuração SPI errada** - CLKPhase e CLKPolarity incorretos para o 74HC165
2. **Polaridade do CS não definida** - cspol não estava configurado
3. **Leitura assíncrona sem esperar dados** - getReverseButton() retornava antes do DMA completar
4. **readButtons() não disparava leitura SPI** - sequência de leitura incorreta
5. **Erro de índice no CS pin** - off-by-one error (1-indexed vs 0-indexed)

### Solução
Foram feitas correções em `ShifterAnalog.cpp` e `ShifterAnalog.h`:
- Corrigida configuração SPI (CLKPhase, CLKPolarity, cspol)
- Criada função `startRead()` com leitura síncrona
- Corrigida sequência de leitura em `readButtons()`
- Corrigido índice do CS pin (cs_pin_num - 1)

### Resultado
Após as correções, o câmbio G27 funciona perfeitamente:
- ✅ 6 marchas + ré
- ✅ 12 botões extras
- ✅ Configuração do CS pin funcional

---

*Report Date: January 2026*
*Board: Generic STM32F407VET6*
*Affected Feature: ShifterAnalog - G27 Shifter H-pattern mode*
