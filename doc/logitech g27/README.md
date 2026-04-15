# Logitech G27 Support for OpenFFBoard

# Suporte ao Logitech G27 para OpenFFBoard

---

## 📋 Overview / Visão Geral

This folder contains documentation and guides for integrating a **Logitech G27 Racing Wheel** with **OpenFFBoard** firmware.

Esta pasta contém documentação e guias para integrar um **Volante Logitech G27** com o firmware **OpenFFBoard**.

---

## 🛒 Hardware Requirements / Requisitos de Hardware

### Essential / Essencial

| Component | Description | Example |
|-----------|-------------|---------|
| **STM32F407 Board** | Main controller | [STM32F407VET6 (AliExpress)](http://pt.aliexpress.com/item/1005006882009420.html) |
| **H-Bridge Driver** | Motor driver for FFB | BTS7960 43A |
| **Power Supply** | 12-24V for motor | 12V 5A minimum |
| **USB Cable** | Micro USB or USB-C | For STM32 connection |

### From G27 / Do G27

| Component | What to use |
|-----------|-------------|
| **Motor** | Original G27 DC motor |
| **Encoder** | Original optical encoder (600 PPR) or upgrade to MT6835 |
| **Pedals** | Original 3-axis potentiometers |
| **Shifter** | Original H-pattern with 2x 74HC165 |
| **Wheel Rim** | Original buttons with 1x 74HC165 |

### Optional Upgrades / Upgrades Opcionais

| Component | Description |
|-----------|-------------|
| **MT6835 Encoder** | High resolution magnetic encoder (65535 CPR) |
| **3D Printed Adapter** | [Thingiverse 1270001](https://www.thingiverse.com/thing:1270001) |

---

## ⚠️ Important Notes / Notas Importantes

### STM32F407VET6 Board Fix

> **EN:** Generic STM32F407VET6 boards require a **10kΩ pull-up resistor between PA9 and 5V** for the F407VG firmware to boot correctly.
>
> **PT:** Placas genéricas STM32F407VET6 requerem um **resistor pull-up de 10kΩ entre PA9 e 5V** para o firmware F407VG iniciar corretamente.

### Wheel Rim + Shifter Together

> **EN:** The G27 wheel rim and shifter both use 74HC165 shift registers which **cannot share the same SPI bus** due to lack of tri-state output. This fork adds **SPI3 support** to allow both to work simultaneously.
>
> **PT:** O aro e o câmbio do G27 usam 74HC165 que **não podem compartilhar o mesmo barramento SPI** devido à falta de saída tri-state. Este fork adiciona **suporte a SPI3** para ambos funcionarem simultaneamente.

---

## 📄 Documentation / Documentação

### For Users / Para Usuários

| Document | Description |
|----------|-------------|
| [**G27_COMPLETE_WIRING_GUIDE.md**](G27_COMPLETE_WIRING_GUIDE.md) | Complete wiring guide for all G27 components / Guia completo de ligações |

### Technical Reports / Relatórios Técnicos

| Document | Description |
|----------|-------------|
| [**G27_SHIFTER_ISSUE_REPORT.md**](G27_SHIFTER_ISSUE_REPORT.md) | Analysis of ShifterAnalog bugs and fixes / Análise dos bugs do câmbio |
| [**G27_WHEEL_RIM_BUTTONS_REPORT.md**](G27_WHEEL_RIM_BUTTONS_REPORT.md) | SPI bus conflict analysis and SPI3 solution / Conflito SPI e solução |

---

## 🔧 Quick Start / Início Rápido

### 1. Flash Firmware / Gravar Firmware

```bash
# Using DFU mode (USB)
# 1. Hold BOOT0 button while connecting USB
# 2. Use STM32CubeProgrammer or dfu-util
dfu-util -a 0 -s 0x08000000 -D OpenFFBoard_F407VG.bin
```

### 2. Wire Components / Conectar Componentes

See [G27_COMPLETE_WIRING_GUIDE.md](G27_COMPLETE_WIRING_GUIDE.md) for detailed pinout.

| Component | Interface | Main Pins |
|-----------|-----------|-----------|
| Motor | PWM | PE9, PE11 |
| Encoder | ABN | PA0, PA1 |
| Pedals | Analog | PA2, PA3, PA6 |
| Shifter | SPI2 | PB12, PB13, PB14 |
| Wheel Rim | SPI3 | PA15, PC10, PC11 |

### 3. Configure / Configurar

Using OpenFFBoard Configurator:

1. **Motor Driver**: PWM mode, Torque Curve 0.60-0.70
2. **Encoder**: Local ABN, CPR 2400 (or 65535 for MT6835)
3. **Analog**: 3 axes for pedals
4. **Shifter**: ShifterAnalog, Mode G27 H-pattern
5. **Wheel Rim**: SPI Buttons 3, 8 buttons, Mode 74HC165

---

## 🎮 Final Result / Resultado Final

| Component | Status | Windows Output |
|-----------|--------|----------------|
| Motor | ✅ Working | Force Feedback enabled |
| Encoder | ✅ Working | Steering axis |
| Pedals | ✅ Working | 3 axes (throttle, brake, clutch) |
| Shifter Gears | ✅ Working | Buttons 1-7 |
| Shifter Buttons | ✅ Working | Buttons 8-19 |
| Wheel Rim | ✅ Working | Buttons 20-27 |

**Total: 27 buttons + 7 gears + 3 pedal axes + FFB**

---

## 🐛 Troubleshooting / Solução de Problemas

### Firmware doesn't boot / Firmware não inicia

- **Cause**: Missing PA9 pull-up resistor on generic boards
- **Solution**: Solder 10kΩ between PA9 and 5V

### Shifter buttons not working / Botões do câmbio não funcionam

- **Cause**: Original firmware SPI timing bug
- **Solution**: Use this fork with the ShifterAnalog fix

### Wheel rim buttons not working with shifter / Botões do aro não funcionam com câmbio

- **Cause**: 74HC165 bus contention on shared SPI
- **Solution**: Use SPI3 for wheel rim (this fork)

### Motor doesn't move / Motor não gira

- Check H-bridge connections and power supply
- Verify PWM pin configuration in Configurator
- Check encoder is reading position correctly

---

## 📚 References / Referências

- [OpenFFBoard Wiki](https://github.com/Ultrawipf/OpenFFBoard/wiki)
- [OpenFFBoard Discord](https://discord.gg/gHtnEcP)
- [Original Project](https://github.com/Ultrawipf/OpenFFBoard)

---

*Last updated: April 2026*
