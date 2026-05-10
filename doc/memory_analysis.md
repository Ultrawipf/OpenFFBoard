# OpenFFBoard Memory Analysis (STM32F407VG & STM32F411RE)

## Overview
This document provides a systematic analysis of the memory footprint of core OpenFFBoard classes, assuming a maximum configuration of **2 axes**. It highlights risks related to FreeRTOS heap exhaustion, heap fragmentation, and thread stack overflows, and details the implemented 78KB heap strategy to ensure system stability.

## Target Architecture Context

Both the **STM32F407VG** and **STM32F411RE** share the same primary memory constraint: **128KB of general SRAM**. 
To maintain architectural consistency across targets, the FreeRTOS heap is centrally managed within this 128KB limit, without relying on the F407's isolated CCMRAM.

## Memory Consumption Profile (Verified 2-Axis Setup)

| Component / Thread | Permanent Memory (Heap) | Thread Stack Allocation | Temporary Spikes (Heap) | Overflow Risks & Observations |
| :--- | :--- | :--- | :--- | :--- |
| **TMC4671** (x2 axes) | **~12.6 KB** (2x `data_cogging`) | **3.0 KB** (2x 384-word threads) | **17.3 KB** (Calibration) | `data_cogging` (5.7KB per axis) is permanent. `CoggingCalibData` (17.3KB) is dynamically allocated *only* during calibration. |
| **Axes & HidFFB** | **~13.0 KB** (Effects Array) | **0.5 KB** (FFB Main Thread) | **~5.1 KB** (Biquad Filters) | Array of 40 `FFB_Effect` is large (~12KB). Dynamic Biquad allocation is necessary for render quality. |
| **Command Interfaces** | ~1.0 KB | **4.3 KB** (CMD Main + CDC) | **~5.8 KB** (via `help`) | **[FIXED]** `help` command consumes ~5.8KB peak. Stack rebalancing recovered 1.7KB for the heap. |
| **BISS-C Encoder** | < 1.0 KB | **0.5 KB** (128-word thread) | - | **[CRITICAL FIX]** Original 64-word stack had only 60 bytes of headroom. Increased to 128 words. |

**Total Estimated Permanent Heap Usage (Base + Stacks):** ~42.0 KB.
**Total Available Heap:** **78.0 KB** (`configTOTAL_HEAP_SIZE`).
**Idle Heap Free:** ~36.0 KB.

### Peak Stress Scenarios
1. **Help Command:** Consumes ~5.8KB. Remaining heap: ~30.2KB. (Status: **STABLE**)
2. **Cogging Calibration:** Consumes **17.3KB**. Remaining heap: ~18.7KB. (Status: **SAFE**)
   - *Note: Minimum ever free heap (High Water Mark) observed during Stress: ~18.3KB.*

## Optimized Stack Strategy (Rebalancing)

Empirical analysis using `sys.tasklist` (Stack High Water Mark) allowed for precision rebalancing, recovering **1.7KB** of heap while securing critical threads:

1. **BISSENC:** Increased from 64 to **128 words** (Prevented imminent stack overflow).
2. **CDCCMD:** Reduced from 512 to **384 words** (Recovered 512 bytes).
3. **TMC Thread:** Reduced from 512 to **384 words** (Recovered 1024 bytes for 2 axes).
4. **CMD_MAIN:** Reduced from 700 to **600 words** (Recovered 400 bytes).

## Linker & SRAM Constraints

The FreeRTOS heap was finalized at **78KB** to accommodate the 21.6KB SRAM overflow initially encountered at 100KB.
- **Total SRAM:** 128KB.
- **FreeRTOS Heap:** 78KB.
- **Static `.bss` / `.data`:** ~49KB.
- **Result:** ~127KB used. Fits within the 128KB hardware limit with minimal overhead for the system stack and interrupts.

## Recommendations
- **Avoid concurrent calibration:** Do not run Cogging calibration while generating massive help strings to maintain the 18KB safety buffer.
- **Monitor BISSENC:** If new features are added to the BISS-C driver, re-verify the 128-word stack headroom.
