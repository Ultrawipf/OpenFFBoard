# Analyse consolidée : encodeurs, estimation d'état et filtrage FFB (OpenFFBoard)

> Document unique regroupant les analyses précédemment réparties dans `doc/`. Le contenu de chaque document d'origine est conservé intégralement ; seuls les niveaux de titres ont été décalés d'un cran et les renvois entre documents remplacés par des liens internes.

## Sommaire

1. [Partie 1 — Analyse initiale des encodeurs et plan de remédiation (Kalman)](#partie-1) *(ex-`analyse_encodeurs_filtrage.md`)*
2. [Partie 2 — Recherche : filtre de Kalman multi-cadence](#partie-2) *(ex-`kalman_optimisation.md`)*
3. [Partie 3 — Expertise DSP : arbitrage Kalman vs KinematicPLL](#partie-3) *(ex-`analyse_dsp_filtrage_encodeurs.md`)*
4. [Partie 4 — Pourquoi le Kalman 2 états n'a pas été retenu](#partie-4) *(ex-`pourquoi_pas_kalman_2_etats.md`)*
5. [Partie 5 — Use cases encodeurs, scalers et filtrage (état actuel)](#partie-5) *(ex-`analyse_use_cases_encodeurs.md`)*

> Ordre de lecture : les parties suivent la chronologie des décisions. La **Partie 5** reflète l'état le plus récent de l'implémentation (`KinematicPLL`, branche `refactor/encoder`).


---

<a id="partie-1"></a>

# Partie 1 — Analyse initiale des encodeurs et plan de remédiation (Kalman)

*Source d'origine : `doc/analyse_encodeurs_filtrage.md`*

## In-depth Encoder Analysis & Filtering Remediation Plan (OpenFFBoard)

This document presents an architectural analysis of the position acquisition, velocity, and acceleration calculation system of OpenFFBoard. It details a remediation plan aimed at removing the Biquad filters from the Axis class in favor of a Kinematic Kalman Filter robust to the "Human-in-the-loop" concept, coupled with a new centralized multi-rate architecture built around the Encoder class.

---

### 1. Encoder Object Structures and Implementations

The base `Encoder` class inherits from `ChoosableClass`, allowing its dynamic selection in the GUI. Here is the comprehensive diagram of the current implementations and overridden virtual methods.

```mermaid
classDiagram
    class ChoosableClass {
        <<interface>>
    }

    class MotorDriver {
        <<interface>>
        +virtual void setExternalPhiE(float phiE)*
    }
    
    class Encoder {
        <<abstract>>
        +virtual int32_t getPos()*
        +virtual uint32_t getCpr()*
        +virtual float getPos_f()
        +virtual float getSpeed()
        +virtual float getAccel()
        +virtual void setPos(int32_t pos)
        +virtual uint32_t getScaler()
    }
    
    class EncoderLocal {
        +getPos() int32_t
        +getCpr() uint32_t
        +setPos(int32_t pos) void
    }
    
    class EncoderSSI {
        +getPos() int32_t
        +getCpr() uint32_t
    }
    
    class EncoderBissC {
        +getPos() int32_t
        +getCpr() uint32_t
        +triggerRead() void
        +spiRxCompleted() void
    }
    
    class MtEncoderSPI {
        +getPos() int32_t
        +getCpr() uint32_t
    }

    class TMC4671 {
        +getPos() int32_t
        +getCpr() uint32_t
        +setExternalPhiE(float phiE) void
    }
    
    class ODriveCAN {
        +getPos() int32_t
        +getCpr() uint32_t
        +getPos_f() float
        +getSpeed() float
    }
    
    class VescCAN {
        +getPos() int32_t
        +getCpr() uint32_t
        +getPos_f() float
        +getSpeed() float
    }

    class RmdMotorCAN {
        +getPos() int32_t
        +getCpr() uint32_t
        +getSpeed() float
    }
    
    class MotorSimplemotion {
        +getPos() int32_t
        +getCpr() uint32_t
    }

    ChoosableClass <|-- Encoder
    Encoder <|-- EncoderLocal
    Encoder <|-- EncoderSSI
    Encoder <|-- EncoderBissC
    Encoder <|-- MtEncoderSPI
    Encoder <|-- TMC4671
    Encoder <|-- ODriveCAN
    Encoder <|-- VescCAN
    Encoder <|-- RmdMotorCAN
    Encoder <|-- MotorSimplemotion
    
    MotorDriver <|-- TMC4671
    MotorDriver <|-- ODriveCAN
    MotorDriver <|-- VescCAN
    MotorDriver <|-- RmdMotorCAN
```

#### Overrides Behavior
*   **Pure Sensors (Local, SSI, BissC, MtEncoderSPI)**: Must override `getPos()` and `getCpr()`. The central Kalman Filter will provide `getPos_f()`, `getSpeed()`, and `getAccel()` for them.
*   **Smart Controllers (ODriveCAN, VescCAN)**: Since these ESCs communicate raw position and speed as floating-point numbers (via their internal firmware), they **explicitly override `getPos_f()` and `getSpeed()`** to directly return the CAN bus data, thus bypassing the STM32 microcontroller calculations. The integer `getPos()` is then deduced by multiplying with the CPR.

---

### 2. Axis Class Analysis: Current Metrics and Filters

In `Axis.cpp`, velocity and acceleration are calculated at each iteration (1 kHz, $dT = 1\text{ ms}$) via Euler differentiation, then smoothed by two low-pass IIR Biquad filters (~40-60 Hz cutoffs).

#### The Phase Lag Issue
In Force Feedback, a 40 Hz cutoff Biquad filter introduces about **90° of phase lag** at 40 Hz, resulting in a delay of $t_{\text{del}} \approx 6.25\text{ ms}$. This delay injects parasitic active energy, causing violent hands-off oscillations and forcing the dampening gains to be lowered.

---

### 3. Moving Metrics and Filters to the Encoder

Moving the calculation into `Encoder` offers clear advantages:
1.  **Encapsulation**: `Axis` now simply reads `encoder->getSpeed()`.
2.  **Hardware Bypass**: Smart controllers (VescCAN, ODriveCAN) will be able to return their direct internal estimator value, guaranteeing **zero MCU phase lag**.

---

### 4. Asynchronous Multi-Rate Architecture

To make the most of disparate sensors without saturating the CPU or RAM, the architecture will adopt a **hardware-paced asynchronous multi-rate** paradigm.

#### 4.1. The Scaler (Temporal Polymorphism)
The idea is to decouple the encoder reading frequency from the FFB loop (1 kHz). Each child encoder class will define its own refresh constant as a multiplier (the `Scaler`), based on a maximum global system frequency (e.g., **10 kHz**):
*   `EncoderBissC` (High resolution): `scaler = 1` (Read at 10 kHz).
*   `EncoderLocal` (Low resolution): `scaler = 10` (Read at 1 kHz).

#### 4.2. Resource Protection: The EncoderManager
To avoid creating an RTOS Thread per active encoder (which would saturate the STM32's very limited Stack RAM), the system will implement a centralized manager: the `EncoderManager`.
It will iterate over the list of active encoders and apply the scaler trigger logic to launch asynchronous DMA transfers (Zero-CPU-Cost).

#### 4.3. Hardware Timer Recycling (Zero Jitter)
To pace the `EncoderManager` at exactly 10 kHz without suffering from OS timing instabilities (FreeRTOS Jitter), the plan is to **recover and recycle Timer 6 (`TIM_TMC` / `htim6`)**.
Historically used by the TMC4671 at 4 kHz (ARR=250), this hardware component (Basic Timer) will be renamed (e.g., `TIM_SENSOR`) and configured at 10 kHz (ARR=100) to generate the master interrupt of the acquisition architecture.

#### 4.4. The "Adapter" Design Pattern and Injection (Dead-Reckoning)
Currently, the `TMC4671` class is a "God Object" managing an internal thread to spy on the encoder.
This behavior will be decoupled by creating an **`ExternalEncoderAdapter`**.
*   **Dependency Inversion (SOLID)**: The Adapter is not coupled to the TMC. It takes a generic `MotorDriver` interface as a parameter, which exposes a virtual `setExternalPhiE(phi_e)` method. The `TMC4671` will implement this method.
*   **Dead-Reckoning (Extrapolation)**: On the 10 kHz tick, the Adapter will read the encoder state (Kalman Filter: $\theta, \omega, \alpha$). It will extrapolate the exact future position to compensate for the SPI transfer latency: $\theta_{now} = \theta_{Kalman} + \omega_{Kalman} \cdot \Delta t$.
*   **Zero-Lag Injection**: The position pushed to the `MotorDriver` guarantees perfect rotor alignment ($I_d = 0$), reducing motor heating and maximizing torque.

#### 4.5. Final Class Diagram and Interaction Sequence

The following diagram illustrates the target architecture, with precise C++ signatures (pure virtual `*`, virtual, and concrete).

```mermaid
classDiagram
    class MotorDriver {
        <<interface>>
        +virtual void setExternalPhiE(float phiE)*
    }

    class TMC4671 {
        +setExternalPhiE(float phiE) void
        +updateSPI() void
        +setTorque(int16_t torque) void
    }

    class EncoderManager {
        -std::vector~Encoder*~ active_encoders
        +tick() void
    }

    class KinematicKalman {
        -float theta, omega, alpha
        -float P[3][3]
        +predict(float dt) void
        +update(float measurement, float R) void
        +getTheta() float
        +getOmega() float
    }

    class Encoder {
        <<abstract>>
        #KinematicKalman kalman
        +virtual int32_t getPos()*
        +virtual uint32_t getCpr()*
        +virtual float getPos_f()
        +virtual float getSpeed()
        +virtual float getAccel()
        +virtual uint32_t getScaler()
        +virtual void triggerRead()
        +void updateState(float new_pos, float dt)
    }

    class ExternalEncoderAdapter {
        -MotorDriver* motor
        -Encoder* encoder
        +tickHighFrequency() void
    }

    class EncoderBissC {
        +getPos() int32_t
        +getCpr() uint32_t
        +triggerRead() void
        +spiRxCompleted() void
    }

    MotorDriver <|-- TMC4671
    Encoder <|-- EncoderBissC
    Encoder *-- KinematicKalman
    EncoderManager o-- Encoder : "Manages polling"
    ExternalEncoderAdapter --> MotorDriver : "Pushes phiE"
    ExternalEncoderAdapter --> Encoder : "Reads state (Dead-Reckoning)"
```

##### Asynchronous Interaction Sequence (BiSS-C & TMC Example)

```mermaid
sequenceDiagram
    participant TIM6 as Hardware Timer 6 (10 kHz)
    participant EM as EncoderManager
    participant BissC as EncoderBissC
    participant DMA as SPI DMA (Hardware)
    participant Adapter as ExternalAdapter
    participant Motor as MotorDriver (TMC4671)

    Note over TIM6,Motor: Asynchronous Acquisition Phase (Zero-CPU)
    TIM6->>EM: tick() Interrupt
    EM->>BissC: triggerRead() (if tick % scaler == 0)
    BissC->>DMA: spiPort.receive_DMA()
    Note over BissC,DMA: CPU is immediately freed
    DMA-->>BissC: End interrupt: spiRxCompleted()
    BissC->>BissC: updateState(new_pos, dt) -> kalman.update()

    Note over TIM6,Motor: FOC Injection Phase (Zero-Lag)
    TIM6->>Adapter: tickHighFrequency()
    Adapter->>BissC: Read state: getTheta(), getOmega()
    Adapter->>Adapter: Extrapolation: theta_f = theta + (omega * dt_spi)
    Adapter->>Motor: setExternalPhiE(theta_f)
    Note over Adapter,Motor: Asynchronous check (!isTaken)
    Motor->>Motor: SPI writeRegAsync(0x1C)
```

---

### 5. The Solution: Kinematic Kalman Filter

The Kalman Filter mathematically handles the uncertainty of human behavior (asynchronous torque):
*   **Process Matrix ($Q$)**: Human action is modeled as acceleration noise.
*   **Measurement Matrix ($R$)**: Calculated via the physical resolution of the encoder: $R = \frac{(2\pi / \text{CPR})^2}{12}$.
*   **Innovation Inversion ($S$) in $O(1)$**: The Kalman matrix inversion is reduced here to the inverse of a simple scalar: $S = P_{0,0} + R$, executable in $\sim 14$ clock cycles on the Cortex-M4 hardware FPU.

---

### 6. 3-Phase Remediation Plan

```mermaid
gantt
    title Encoder Filtering Remediation Plan (Kalman + Multi-rate)
    dateFormat  YYYY-MM-DD
    section Phase 1
    Kalman Declaration & Encoder API : active, p1, 2026-06-08, 3d
    section Phase 2
    TIM6 Recycling & EncoderManager : p2, after p1, 4d
    section Phase 3
    Adapter (ExternalEncoderAdapter) & Cleanup : p3, after p2, 4d
```

#### Phase 1: API Expansion and Kalman Filter
*   Create a utility class `KinematicKalman`.
*   Add `getSpeed()`, `getAccel()`, `getScaler()`, and `triggerRead()` to `Encoder.h`.

#### Phase 2: Centralization via EncoderManager and Timer 6
*   Recycle `TIM_TMC` (Timer 6) into `TIM_SENSOR` (10 kHz, ARR=100).
*   Create `EncoderManager` driven by this timer to manage scalers and asynchronous DMA calls (BiSS-C).

#### Phase 3: Driver Decoupling, Dead-Reckoning, and Axis Cleanup
*   Create the abstract interface `MotorDriver` with `setExternalPhiE(phi_e)`.
*   Make `TMC4671` implement the interface.
*   Create `ExternalEncoderAdapter` to push the extrapolated angle to the `MotorDriver`.
*   Remove internal threads/timers from `TMC4671.cpp`.
*   Remove Biquad instances from `Axis.h` and `Axis.cpp`, point the force feedback directly to `encoder->getSpeed()`.

---

### 7. Gap Analysis: Theory vs. Reality (Post-Implementation)

A comprehensive review of the codebase (from commit `4bee3f7e` onwards) reveals several deviations and architectural adaptations that occurred during implementation, differing from the original theoretical plan.

#### 7.1. Deviations from the "Zero-CPU-Cost" Plan
The original plan aimed for an asynchronous, DMA-driven "Zero-CPU-Cost" architecture at 10 kHz without relying on RTOS threads. Hardware constraints forced practical adjustments:
*   **SPI Interrupts (IT) instead of DMA:** High-frequency polling (10 kHz) caused DMA stream lockups and Watchdog resets. Consequently, `MtEncoderSPI` and other sensors fell back to using interrupt-based SPI transfers (`transmitReceive_IT`) instead of DMA.
*   **Threaded EncoderManager:** To avoid locking up the system, `EncoderManager` was implemented as a FreeRTOS thread (`cpp_freertos::Thread`) rather than executing directly within a raw hardware ISR. The timer interrupt only wakes up this thread via `NotifyFromISR()`.

#### 7.2. Newly Coded Concepts (Not in Original Doc)
To ensure thread-safety and hardware stability, several new concepts were introduced:
*   **Context-Aware Notifications:** Extensive use of `inIsr()` ensures safe context-switching (choosing between `Notify()` and `NotifyFromISR()`) preventing RTOS corruption when dealing with multi-rate callbacks.
*   **Safe Resource Destruction:** Encoder swapping is now protected. `EncoderManager::getInstance().Suspend()` is used during encoder destruction, and consumer threads (`ExternalEncoderAdapter`) are safely destroyed (`extEncAdapter.reset()`) before pointer reassignments to avoid use-after-free race conditions.
*   **SPI Fallback Timeouts:** Safety timeouts (e.g., `WaitForNotification(20)`) were implemented to abort SPI transfers if the bus hangs, preventing full Watchdog resets.

#### 7.3. Ignored or Unfinished Concepts
*   **Zero-Lag Injection (Dead-Reckoning):** The physical SPI command pushing the extrapolated FOC angle to the driver (`writeRegAsync(0x1C, phiE_final)`) was temporarily commented out for testing, rendering the external FOC injection currently inactive.

---

### 8. State of the Art Architectural Review (RTOS vs. Bare-Metal)

The current implementation uses a "Ping-Pong" cascade: `Timer ISR -> Thread Manager -> SPI(IT) -> SPI ISR -> Thread Encoder -> Kalman`. While highly modular and object-oriented, this approach introduces critical real-time risks for a 10 kHz control loop.

#### 8.1. Context Switching Overhead and Latency Jitter
On an STM32 MCU, FreeRTOS context switches cost 1.5 - 3 µs. The current cascade requires multiple context switches per 100 µs cycle (10 kHz), consuming up to 15% of the available CPU time solely for scheduling overhead.
More critically, this architecture subjects the control loop to RTOS **latency jitter**. A Kalman filter relies on a perfectly deterministic $\Delta t$. Jitter introduced by RTOS scheduling creates mathematically unpredictable noise in velocity and acceleration estimators, severely degrading FOC performance.

#### 8.2. The IT vs. DMA Anomaly
Falling back to SPI IT because of DMA lockups is a symptom of priority conflicts, not a flaw in the DMA itself. At 10 kHz, triggering a CPU interrupt for every SPI byte starves the RTOS. Proper DMA usage is mandatory at this frequency. The DMA Watchdog lockup was likely caused by improper ISR priorities or blocking loops within threads.

#### 8.3. Future Remediation: Hybrid Bare-Metal Path
To achieve a true "State of the Art" 10 kHz control loop, the critical path must be extracted from the FreeRTOS scheduler:
1.  **Bare-Metal Critical Path:** The Timer triggers a DMA SPI transfer entirely via hardware. The DMA Rx Complete ISR (highest priority) runs the Kalman filter and pushes the result to the FOC driver (`setExternalPhiE`). Zero threads, zero context switching, mathematically perfect $\Delta t$.
2.  **Asynchronous RTOS Path:** Slower tasks (USB reporting, Force Feedback calculations at 1 kHz) remain in FreeRTOS threads, safely reading the variables calculated by the high-priority ISR.


---

<a id="partie-2"></a>

# Partie 2 — Recherche : filtre de Kalman multi-cadence

*Source d'origine : `doc/kalman_optimisation.md`*

## **Rapport d'Expertise sur la Conception et l'Optimisation Numérique d'un Filtre de Kalman Multi-Cadence pour la Fusion Hétérogène d'Encodeurs**

### **1\. Introduction et Analyse du Paradigme Technologique**

L'estimation précise, robuste et réactive de la cinématique d'un système mécanique constitue le pilier fondamental des architectures de contrôle de mouvement à haute performance. L'exploitation d'encodeurs rotatifs ou linéaires, qu'ils soient de nature incrémentale ou absolue, introduit un ensemble de défis mathématiques et algorithmiques intimement liés à la nature discrète de l'acquisition des données. La discrétisation s'opère simultanément sur deux dimensions : la quantification spatiale, inhérente à la résolution physique du disque de l'encodeur, et la quantification temporelle, dictée par la fréquence d'échantillonnage de l'électronique de traitement.1  
Le scénario technique analysé dans le présent rapport expose une architecture logicielle hautement flexible conçue pour traiter deux configurations matérielles exclusives :

* **Configuration A (Encodeur Absolu Haute Résolution \- ex: BiSS-C) :** Un flux de données de position absolue reçu à très haute fréquence (10 kHz), traité par un filtre de type Position-Seule (Position-Only Measurement \- POM).2  
* **Configuration B (Encodeur Incrémental \- ex: ABZ) :** Une lecture de position à basse fréquence (1000 Hz) couplée à la capture matérielle haute précision du délai temporel entre deux impulsions physiques successives (![][image1]), traitée par un filtre asynchrone de type Position-Vitesse (Position-Velocity Measurement \- PVM).3

L'implémentation d'un filtre de Kalman dans un environnement soumis à des variations dynamiques du pas de temps d'intégration (variable ![][image2]) exige une rigueur mathématique absolue concernant la propagation de la matrice de covariance de l'erreur et la modélisation du bruit de processus.4 Une analyse de l'algorithme initial (KinematicKalman.cpp 6) révèle des anomalies structurelles profondes dans la gestion de ce pas de temps variable, dans la stabilité numérique des équations de mise à jour, et dans la sous-utilisation de l'information temporelle de l'encodeur incrémental.  
Ce rapport exhaustif se propose de disséquer l'intégralité du problème, depuis les fondements théoriques de l'estimation de la vitesse jusqu'à la reformulation algébrique complète, stable et modulaire du filtre de Kalman. Il détaille les failles mathématiques de l'implémentation initiale et fournit un cadre algorithmique rigoureux pour réaliser une estimation d'état multi-cadence robuste, exempte de déphasage structurel, et capable de s'adapter en temps réel aux caractéristiques physiques de l'encodeur sélectionné.7

### **2\. Fondements Théoriques de l'Acquisition et de l'Estimation de Vitesse**

#### **2.1. Les Limites Inhérentes à la Différenciation Numérique**

La méthode la plus intuitive et historiquement la plus répandue pour déduire la vitesse angulaire ou linéaire à partir d'un encodeur de position repose sur la différenciation numérique arrière (Backward Difference).9 L'équation fondamentale s'exprime sous la forme ![][image3], où ![][image4] représente la position mesurée et ![][image5] la période d'échantillonnage constante. Bien que son implémentation soit triviale, cette approche se heurte rapidement aux lois de la physique de l'acquisition numérique.10  
L'erreur affectant la vitesse ainsi calculée est directement proportionnelle à l'erreur de quantification spatiale de l'encodeur et, de manière contre-intuitive, inversement proportionnelle à la période d'échantillonnage temporelle ![][image5].1 Cela signifie qu'une augmentation de la fréquence de la boucle de contrôle (par exemple, le passage à 10 kHz pour traiter le signal BiSS-C) amplifie dramatiquement l'impact du bruit de quantification sur la dérivée calculée.10 À une telle fréquence, le déplacement angulaire entre deux cycles d'échantillonnage peut être de l'ordre de grandeur de la résolution spatiale du capteur, voire nul à basse vitesse, générant un signal de vitesse fortement discontinu, dominé par un bruit de haute fréquence rendant tout asservissement proportionnel-dérivé (PD) ou proportionnel-intégral-dérivé (PID) inopérant sans un filtrage passe-bas lourd.11 Ce filtrage externe induit inévitablement un retard de phase (phase lag) préjudiciable à la stabilité de la boucle fermée et à la réjection des perturbations dynamiques.12

#### **2.2. Les Méthodes Temporelles et l'Horodatage (Time-Stamping)**

Pour contourner ce paradoxe de l'amplification du bruit à haute fréquence d'échantillonnage, l'industrie des systèmes de contrôle de mouvement s'est tournée vers des méthodologies exploitant les horloges matérielles internes des microcontrôleurs (timers, compteurs haute fréquence, ou implémentations sur FPGA) pour horodater avec précision les événements générés par l'encodeur.1 Ces approches transforment le paradigme : au lieu de mesurer un ![][image6] fixe sur un ![][image2] imposé par la boucle logicielle, elles évaluent un ![][image2] variable pour un ![][image6] défini par la géométrie matérielle du disque de l'encodeur.

| Méthodologie d'Estimation | Principe Opératoire Fondamental | Avantages Analytiques | Limitations et Contraintes Inhérentes |
| :---- | :---- | :---- | :---- |
| **M-Method (Différenciation)** | Comptabilisation du nombre total d'impulsions (M) générées par l'encodeur sur une période d'échantillonnage strictement fixe ![][image5]. 9 | Excellente précision relative lors d'une dynamique de très haute vitesse. Complexité matérielle et algorithmique minimale. | Résolution exécrable, voire signal totalement discontinu à basse vitesse. Phénomène de battement sévère. |
| **T-Method (Mesure de période)** | Mesure du temps écoulé (![][image2]) entre deux impulsions physiques consécutives au moyen d'une horloge système à très haute fréquence. 14 | Résolution théorique quasi-infinie à l'approche de l'arrêt complet. Fournit la mesure de la vitesse la plus instantanée possible. | Délai de mise à jour intrinsèquement variable. À très haute vitesse, la résolution de l'horloge temporelle devient le facteur limitant. 16 |
| **MT-Method (Hybride)** | Comptage simultané du nombre d'impulsions dans une fenêtre donnée tout en mesurant précisément le temps réel écoulé. 17 | Fusion symbiotique des avantages des deux méthodes. Garantit une précision optimale sur une plage de vitesse extrêmement vaste. | Exige une complexité matérielle significative (logique FPGA dédiée) pour synchroniser parfaitement le comptage. 16 |

Dans la configuration avec l'encodeur ABZ (1 kHz), l'acquisition matérielle du délai d'impulsion ![][image1] permet d'exploiter la T-Method pour obtenir une mesure de vitesse indépendante de la différenciation numérique.17 Cette information temporelle constitue une observation physique directe et mathématiquement rigoureuse de la vitesse angulaire moyenne de l'arbre mécanique sur ledit intervalle temporel.1 La conversion de ce délai en vitesse, exprimée par ![][image7], est injectée dans l'algorithme d'estimation pour corriger et stabiliser la dérivée temporelle de la position sans subir le retard de phase d'un filtre passe-bas.21

### **3\. Modélisation Mathématique de l'État Cinématique**

L'évolution cinématique du système mécanique (rotor du moteur) est modélisée par un vecteur d'état continu ![][image8] regroupant la position angulaire ![][image9], la vitesse angulaire ![][image10] et l'accélération angulaire ![][image11].22  
![][image12]  
L'équation différentielle linéaire continue régissant l'évolution du système s'écrit ![][image13], où ![][image14] est la matrice de la dynamique du système et ![][image15] représente le bruit de processus continu, traduisant les perturbations de couple externes, les frottements non modélisés ou les erreurs de modélisation.2  
![][image16]  
Pour implémenter cette dynamique dans un processeur numérique fonctionnant à un pas de temps discret ![][image2], la matrice de transition d'état discrète ![][image17] est calculée par l'exponentielle matricielle ![][image18].24 Grâce à la nature nilpotente de la matrice ![][image14] (![][image19]) :  
![][image20]  
L'équation de prédiction de l'état discret s'écrit : ![][image21].2

#### **3.1. Modélisation de la Covariance du Bruit de Processus (Matrice Q)**

La matrice de covariance du bruit de processus ![][image22] quantifie la distribution de l'incertitude injectée dans les états du système au fil du temps en raison des approximations du modèle mécanique.25  
Dans l'approche à bruit blanc continu (Continuous White Noise Acceleration \- CWNA), on postule que la dérivée de l'accélération (le jerk) est modélisée par un bruit blanc Gaussien continu de densité spectrale de puissance ![][image23]. L'intégration de ce bruit sur le pas de temps exact ![][image2] donne la matrice discrète ![][image24] 26 :  
![][image25]  
Cette formulation continue garantit que la quantité d'incertitude injectée dans le modèle est rigoureusement proportionnelle au temps écoulé ![][image2], protégeant le filtre contre les variations de fréquence d'échantillonnage (ex: gigue temporelle ou exécution asynchrone).4

### **4\. Diagnostic Exhaustif de l'Algorithme Fourni**

L'analyse de l'implémentation originale KinematicKalman.cpp 6 dévoile plusieurs vulnérabilités fondamentales, d'ordre mathématique et de stabilité numérique, qui expliquent l'instabilité et la divergence observées lors de la correction de la vitesse.

#### **4.1. Vulnérabilité Majeure n°1 : La Désynchronisation Stochastique de la Matrice Q**

Dans le code original, la matrice ![][image22] est générée de la manière suivante :

C++  
float Q00 \= q\_var \* dt4 \* 0.25f;  
float Q01 \= q\_var \* dt3 \* 0.5f;  
float Q02 \= q\_var \* dt2 \* 0.5f;  
float Q11 \= q\_var \* dt2;  
float Q12 \= q\_var \* dt;  
float Q22 \= q\_var; // ERREUR MATHÉMATIQUE MAJEURE

Cette formulation tente d'implémenter un modèle discret (DWPA) où la variable q\_var représente la variance de l'accélération. Cependant, maintenir un terme ![][image26] strictement constant alors que le pas de temps d'intégration ![][image2] varie viole les lois de propagation des processus stochastiques.22  
Si le filtre est configuré avec un q\_var ajusté pour la boucle ABZ à 1 kHz (![][image27]), l'algorithme suppose qu'une quantité d'incertitude ![][image28] contamine le modèle toutes les millisecondes. Si le même filtre est configuré pour le BiSS-C à 10 kHz (![][image29]), le code va ajouter cette *même* incertitude ![][image28] à une fréquence dix fois plus élevée. Sur une seconde de fonctionnement, le filtre à 10 kHz accumulera artificiellement une incertitude dix fois supérieure à celle du filtre à 1 kHz, provoquant l'effondrement de la matrice de covariance ![][image30] et forçant les gains de Kalman à sur-corriger le bruit de mesure.10  
Pour corriger ce défaut, la matrice ![][image22] doit impérativement être basée sur la densité spectrale de puissance continue ![][image23] (modèle CWNA) présentée à la section 3.1.

#### **4.2. Vulnérabilité Majeure n°2 : Instabilité Numérique de la Matrice de Covariance (Équation de Joseph)**

Lors de l'étape de mise à jour, le code original prétend utiliser la forme de Joseph pour mettre à jour la matrice de covariance d'erreur de prédiction ![][image30] :

C++  
P \= (1.0f \- K0) \* P00\_old;  
//...  
P\[27\]\[27\] \= \-K1 \* P01\_old \+ P11\_old;

Il s'agit en réalité de la formulation standard et naïve ![][image31]. Bien qu'algébriquement exacte, cette équation est numériquement instable lorsqu'elle est exécutée en arithmétique à virgule flottante simple précision (32 bits, IEEE 754).23  
Les erreurs d'arrondi et de troncature accumulées au cours des cycles de calcul (en particulier à des fréquences élevées comme 10 kHz) détruisent rapidement les propriétés de symétrie et de positivité définie de la matrice ![][image30]. Les éléments diagonaux (notamment la variance de la vitesse ![][image32] et de l'accélération ![][image33]) finissent par devenir négatifs, entraînant une inversion de signe du gain de Kalman et provoquant une divergence instantanée de l'estimation d'état.  
La forme de Joseph complète et factorisée est la seule formulation mathématiquement garantie robuste contre ce phénomène de dégradation numérique 24 :  
![][image34]

#### **4.3. Vulnérabilité Majeure n°3 : L'Omission du Calcul Temporel pour l'ABZ**

L'implémentation d'origine n'offre qu'un traitement de position seule (POM).2 Pour l'encodeur incrémental ABZ (1 kHz, 2500 CPR), cette omission est critique. À basse vitesse, le bruit de quantification spatiale d'un encodeur à 2500 CPR est massif (sauts d'escalier). Si le filtre ne reçoit qu'un signal de position discrétisé sans exploiter le délai d'impulsion matériel ![][image1] (qui est pourtant passé en paramètre), il se comporte comme un simple dérivateur passe-bas et génère d'importantes oscillations sur la vitesse estimée.11

### **5\. Architecture Polymorphe et Preuves de Stabilité**

Pour lever l'incohérence apparente de l'estimation réciproque (position ![][image35] vitesse ![][image35] position) et garantir le fonctionnement optimal du filtre pour vos deux configurations exclusives, le filtre est conçu comme un estimateur d'état cinématique polymorphe.7

#### **5.1. Preuve de Stabilité Mathématique en Position-Seule (POM) \- Mode BiSS-C (10 kHz)**

Dans la configuration **BiSS-C**, l'encodeur fournit uniquement la position absolue de l'arbre à haute fréquence (10 kHz). Le vecteur de mesure est scalaire : ![][image36], la matrice d'observation s'écrit ![][image37] et la covariance du bruit de mesure ![][image38] est statique.  
La vitesse ![][image39] et l'accélération ![][image40] sont des variables cachées estimées indirectement.2 La stabilité et la convergence de cette estimation reposent sur la condition d'**observabilité** du système linéaire discret défini par le couple ![][image41].12  
La matrice d'observabilité ![][image42] pour notre système cinématique d'ordre 3 s'écrit 2 :  
![][image43]  
Le déterminant de cette matrice vaut :  
![][image44]  
Puisque le pas de temps de boucle ![][image2] est strictement supérieur à zéro (![][image29] à 10 kHz), le déterminant est non nul (![][image45]). La matrice d'observabilité est donc de **rang plein (rang 3\)**.2  
Selon le théorème d'observabilité de Kalman, cette propriété garantit que :

1. Il est possible de reconstruire de manière unique et stable l'intégralité du vecteur d'état (position, vitesse, accélération) à partir de la seule observation de la position.2  
2. L'Équation Algébrique de Riccati (DARE) converge mathématiquement vers une unique solution stable ![][image46] définie positive.  
3. La matrice d'évolution de l'erreur d'estimation ![][image47] a toutes ses valeurs propres situées strictement à l'intérieur du cercle unité discret, assurant la convergence asymptotique de l'erreur d'estimation vers zéro.2

L'incohérence apparente est ainsi résolue par la rétroaction négative du gain de Kalman : toute surestimation de la vitesse se traduit instantanément par une erreur de prédiction de position négative, qui vient soustraire l'excès de vitesse estimé lors de l'étape de mise à jour.10

#### **5.2. Mode ABZ (1 kHz) : Estimation Hybride Asynchrone (PVM)**

Dans la configuration **ABZ**, l'encodeur fournit la position cumulée à 1 kHz (bruitée par la quantification) et le délai d'impulsion ![][image1] à chaque front d'interruption.  
Pour exploiter au mieux la précision temporelle à basse vitesse et éliminer le bruit de quantification sans introduire de retard de phase, le filtre de Kalman fonctionne de manière asynchrone avec deux fonctions de correction distinctes :

* **Mise à jour de position (1 kHz) :** Correction périodique de l'état ![][image4] avec la matrice ![][image48] et ![][image49].11  
* **Mise à jour de vitesse (Événementielle) :** Correction asynchrone de l'état ![][image39] déclenchée à chaque impulsion de l'encodeur avec la matrice ![][image50] et la covariance adaptative dynamique ![][image51].

##### **Loi de Covariance Adaptative de Vitesse (Loi ANC)**

La vitesse mesurée brute par T-Method s'exprime par ![][image52].15 En appliquant la loi de propagation des incertitudes de Taylor de premier ordre autour du temps mesuré (méthode Delta), l'erreur temporelle d'horloge du timer de capture (d'écart-type ![][image53]) se propage sur la variance de la vitesse sous la forme :  
![][image54]  
Cette loi adaptative garantit une stabilité absolue : à très basse vitesse (![][image1] grand), ![][image55] s'effondre vers ![][image56], forçant le filtre à se verrouiller sur la vitesse par période extrêmement précise. À haute vitesse (![][image1] petit), ![][image55] diverge vers l'infini, forçant le filtre à ignorer la vitesse calculée par le timer pour se fier uniquement au modèle cinématique de prédiction et aux mises à jour de position.1

### **6\. Compensation du Retard de Phase Temporel (OOSM)**

Le calcul de la vitesse par division de l'intervalle angulaire par le temps mesuré s'apparente à une vitesse moyenne sur l'intervalle ![][image1]. Mathématiquement, cette vitesse moyenne correspond à la vitesse instantanée réelle de l'arbre au milieu de la fenêtre de mesure, soit à l'instant passé ![][image57].30  
Injecter cette mesure directement dans le filtre à l'instant actuel ![][image58] introduit un retard de groupe (lag) de ![][image59] qui déstabilise la boucle d'asservissement en mouvement rapide.12  
Pour éliminer ce déphasage de manière analytique et peu coûteuse, le filtre projette l'estimation de la vitesse du modèle vers le passé en utilisant son accélération estimée ![][image60] avant de calculer l'innovation 31 :  
![][image61]  
L'innovation vélocimétrique compensée s'écrit alors :  
![][image62]  
Cette compensation algébrique permet d'aligner le temps de la mesure avec le temps absolu de l'algorithme d'estimation.31 L'erreur de phase inhérente aux signaux d'encodeurs incrémentaux est ainsi virtuellement annihilée, permettant d'accéder à la performance du filtrage "Zero-Phase Lag" indispensable à la régulation d'axes hautement dynamiques.25

### **7\. Architecture Algorithmique Intégrale (Blueprint C++)**

Voici la réécriture complète et modulaire de la classe C++ de votre filtre de Kalman. Cette structure offre une fonction de prédiction cinématique et deux fonctions de mise à jour indépendantes permettant de s'adapter nativement à vos deux types d'encodeurs (POM ou PVM).

#### **7.1. Fichier d'En-tête KinematicKalman.h**

C++  
\#**pragma** once

class KinematicKalman {  
public:  
    KinematicKalman();  
      
    // Initialise l'estimateur d'état  
    void init(float init\_theta, float init\_omega, float init\_alpha);  
      
    // Étape de prédiction cinématique isochrone (à appeler au pas de temps dt)  
    void predict(float dt, float q\_continuous);  
      
    // Correction de position (POM \- Utilisée par BiSS-C à 10 kHz et ABZ à 1 kHz)  
    void updatePosition(float z\_theta, float R\_position);  
      
    // Correction de vitesse (PVM \- Utilisée asynchronement par l'ABZ via T-Method)  
    void updateVelocity(float z\_omega, float dt\_pulse);

    // Accesseurs  
    float getTheta() const { return x; }  
    float getOmega() const { return x\[27\]; }  
    float getAlpha() const { return x\[17\]; }  
    float getVarOmega() const { return P\[27\]\[27\]; }

private:  
    float x\[1\];       // Vecteur d'état \[theta, omega, alpha\]  
    float P\[1\]\[1\];    // Matrice de covariance d'erreur  
};

#### **7.2. Fichier Source KinematicKalman.cpp**

C++  
\#**include** "KinematicKalman.h"

KinematicKalman::KinematicKalman() {  
    init(0.0f, 0.0f, 0.0f);  
}

void KinematicKalman::init(float init\_theta, float init\_omega, float init\_alpha) {  
    x \= init\_theta;  
    x\[27\] \= init\_omega;  
    x\[17\] \= init\_alpha;

    // Initialisation diagonale de la covariance  
    for (int i \= 0; i \< 3; \++i) {  
        for (int j \= 0; j \< 3; \++j) {  
            P\[i\]\[j\] \= (i \== j)? 1.0f : 0.0f;  
        }  
    }  
}

void KinematicKalman::predict(float dt, float q\_continuous) {  
    if (dt \<= 0.0f) return;

    float dt2 \= dt \* dt;  
    float dt3 \= dt2 \* dt;  
    float dt4 \= dt3 \* dt;  
    float dt5 \= dt4 \* dt;

    // 1\. Évolution de l'état (Modèle cinématique)  
    float theta\_pred \= x \+ x\[27\] \* dt \+ 0.5f \* x\[17\] \* dt2;  
    float omega\_pred \= x\[27\] \+ x\[17\] \* dt;  
    float alpha\_pred \= x\[17\];

    x \= theta\_pred;  
    x\[27\] \= omega\_pred;  
    x\[17\] \= alpha\_pred;

    // 2\. Génération de la matrice de bruit de processus Q (CWNA Exacte)  
    float Q00 \= q\_continuous \* dt5 / 20.0f;  
    float Q01 \= q\_continuous \* dt4 / 8.0f;  
    float Q02 \= q\_continuous \* dt3 / 6.0f;  
    float Q11 \= q\_continuous \* dt3 / 3.0f;  
    float Q12 \= q\_continuous \* dt2 / 2.0f;  
    float Q22 \= q\_continuous \* dt;

    // 3\. Propagation de la covariance P \= F \* P \* F^T \+ Q  
    float P00\_old \= P; float P01\_old \= P\[27\]; float P02\_old \= P\[17\];  
    float P11\_old \= P\[27\]\[27\]; float P12\_old \= P\[27\]\[17\]; float P22\_old \= P\[17\]\[17\];

    float M00 \= P00\_old \+ dt \* P01\_old \+ 0.5f \* dt2 \* P02\_old;  
    float M01 \= P01\_old \+ dt \* P11\_old \+ 0.5f \* dt2 \* P12\_old;  
    float M02 \= P02\_old \+ dt \* P12\_old \+ 0.5f \* dt2 \* P22\_old;

    float M11 \= P11\_old \+ dt \* P12\_old;  
    float M12 \= P12\_old \+ dt \* P22\_old;

    P \= M00 \+ dt \* M01 \+ 0.5f \* dt2 \* M02 \+ Q00;  
    P\[27\] \= M01 \+ dt \* M02 \+ Q01;  
    P\[17\] \= M02 \+ Q02;

    P\[27\] \= P\[27\];  
    P\[27\]\[27\] \= M11 \+ dt \* M12 \+ Q11;  
    P\[27\]\[17\] \= M12 \+ Q12;

    P\[17\] \= P\[17\];  
    P\[17\]\[27\] \= P\[27\]\[17\];  
    P\[17\]\[17\] \= P22\_old \+ Q22;  
}

void KinematicKalman::updatePosition(float z\_theta, float R\_position) {  
    // Étape de correction de position (H \= \[1 0 0\])  
    float S \= P \+ R\_position;  
    if (S \<= 0.0f) return;

    float S\_inv \= 1.0f / S;

    // Calcul des gains de Kalman  
    float K0 \= P \* S\_inv;  
    float K1 \= P\[27\] \* S\_inv;  
    float K2 \= P\[17\] \* S\_inv;

    // Innovation  
    float y \= z\_theta \- x;

    // Correction de l'état  
    x \+= K0 \* y;  
    x\[27\] \+= K1 \* y;  
    x\[17\] \+= K2 \* y;

    // Mise à jour de la covariance par la forme de Joseph stricte  
    float W0 \= 1.0f \- K0;  
    float W1 \= \-K1;  
    float W2 \= \-K2;

    float P00\_old \= P; float P01\_old \= P\[27\]; float P02\_old \= P\[17\];  
    float P11\_old \= P\[27\]\[27\]; float P12\_old \= P\[27\]\[17\]; float P22\_old \= P\[17\]\[17\];

    P \= W0\*W0 \* P00\_old \+ K0\*K0 \* R\_position;  
    P\[27\] \= W0 \* (W1 \* P00\_old \+ P01\_old) \+ K0 \* K1 \* R\_position;  
    P\[17\] \= W0 \* (W2 \* P00\_old \+ P02\_old) \+ K0 \* K2 \* R\_position;

    P\[27\] \= P\[27\];  
    P\[27\]\[27\] \= W1\*W1 \* P00\_old \+ 2.0f\*W1 \* P01\_old \+ P11\_old \+ K1\*K1 \* R\_position;  
    P\[27\]\[17\] \= W1 \* W2 \* P00\_old \+ W1 \* P02\_old \+ W2 \* P01\_old \+ P12\_old \+ K1 \* K2 \* R\_position;

    P\[17\] \= P\[17\];  
    P\[17\]\[27\] \= P\[27\]\[17\];  
    P\[17\]\[17\] \= W2\*W2 \* P00\_old \+ 2.0f\*W2 \* P02\_old \+ P22\_old \+ K2\*K2 \* R\_position;  
}

void KinematicKalman::updateVelocity(float z\_omega, float dt\_pulse) {  
    if (dt\_pulse \<= 0.0f) return;

    // 1\. Calcul de la covariance adaptative de vitesse (ANC d'horloge)  
    const float sigma\_tau \= 1.0e-7f;           // Jitter d'horloge (ex: 100 ns)  
    const float R\_mecanique\_plancher \= 1.0e-4f; // Plancher de bruit de fond mécanique  
    const float delta\_theta\_pulse \= 2.513e-3f; // Résolution d'une impulsion (ex: 2pi/2500 rad)

    float dt2 \= dt\_pulse \* dt\_pulse;  
    float dt4 \= dt2 \* dt2;  
    float R\_v\_dynamic \= ((sigma\_tau \* sigma\_tau) \* (delta\_theta\_pulse \* delta\_theta\_pulse)) / dt4;  
    float R\_v \= R\_v\_dynamic \+ R\_mecanique\_plancher;

    // 2\. Compensation du déphasage de premier ordre (OOSM)  
    float omega\_retroprojected \= x\[27\] \- x\[17\] \* (dt\_pulse \* 0.5f);

    // 3\. Étape de correction de vitesse (H\_vel \= \[0 1 0\])  
    float S \= P\[27\]\[27\] \+ R\_v;  
    if (S \<= 0.0f) return;

    float S\_inv \= 1.0f / S;

    // Calcul des gains de Kalman pour la vitesse  
    float K0 \= P\[27\] \* S\_inv;  
    float K1 \= P\[27\]\[27\] \* S\_inv;  
    float K2 \= P\[17\]\[27\] \* S\_inv;

    // Innovation  
    float y \= z\_omega \- omega\_retroprojected;

    // Correction de l'état  
    x \+= K0 \* y;  
    x\[27\] \+= K1 \* y;  
    x\[17\] \+= K2 \* y;

    // Mise à jour de la covariance par la forme de Joseph stricte  
    float W0 \= \-K0;  
    float W1 \= 1.0f \- K1;  
    float W2 \= \-K2;

    float P00\_old \= P; float P01\_old \= P\[27\]; float P02\_old \= P\[17\];  
    float P11\_old \= P\[27\]\[27\]; float P12\_old \= P\[27\]\[17\]; float P22\_old \= P\[17\]\[17\];

    P \= W0\*W0 \* P11\_old \+ K0\*K0 \* R\_v;   
    P\[27\] \= W0 \* (W1 \* P11\_old \+ P01\_old) \+ K0 \* K1 \* R\_v;  
    P\[17\] \= W0 \* (W2 \* P11\_old \+ P02\_old) \+ K0 \* K2 \* R\_v;

    P\[27\] \= P\[27\];  
    P\[27\]\[27\] \= W1\*W1 \* P11\_old \+ 2.0f\*W1 \* P01\_old \+ P11\_old \+ K1\*K1 \* R\_v;  
    P\[27\]\[17\] \= W1 \* W2 \* P11\_old \+ W1 \* P02\_old \+ W2 \* P01\_old \+ P12\_old \+ K1 \* K2 \* R\_v;

    P\[17\] \= P\[17\];  
    P\[17\]\[27\] \= P\[27\]\[17\];  
    P\[17\]\[17\] \= W2\*W2 \* P11\_old \+ 2.0f\*W2 \* P02\_old \+ P22\_old \+ K2\*K2 \* R\_v;  
}

### **8\. Considérations Stratégiques sur le Calibrage et la Resolution**

#### **8.1. Interprétation Physique et Stratégie d'Ajustement du Paramètre ![][image23]**

Le paramètre scalaire ![][image23] encapsule la densité spectrale de puissance du jerk (l'à-coup mécanique). Dans l'écosystème du motion control industriel, il est impérieux de ne pas l'assimiler abstraitement à du "bruit" thermique de circuit imprimé, mais bien aux caractéristiques d'inertie de l'équipement, au rapport de couple et à la bande passante souhaitée du régulateur.32

* Si la variable ![][image23] est arbitrairement compressée à une valeur extrêmement basse, l'algorithme contraindra excessivement la correction des états. Le filtre lissera outrageusement le profil de vitesse, effaçant le bruit de quantification mais introduisant un grave retard d'estimation (lag dynamique) qui brisera la réactivité lors de rampes d'accélération violentes ou d'inversions spontanées du sens de rotation de la mécanique.  
* Inversement, la surévaluation massive de la variable ![][image23] forcera la matrice ![][image30] à accumuler une énorme quantité d'incertitude sur la validité du modèle newtonien. Les gains de Kalman se relâcheront, et le filtre commencera à épouser fidèlement les oscillations aléatoires des capteurs matériels, un comportement qualifié de "poursuite de bruit" (over-fitting stochastique). La mesure estimée reculera alors vers les mêmes défauts d'ondulation que ceux engendrés par la dérivation numérique simple.10

#### **8.2. Répudiation Avancée des Valeurs Aberrantes (Statistical Outlier Gating)**

Les environnements électromécaniques industriels hautement perturbés, notamment par la commutation des variateurs PWM à haute fréquence (ex: 20 kHz), engendrent des défauts critiques tels que des glitchs sur les lignes de quadrature, des fausses bascules sur les fronts montants ou des corruptions de trames BiSS.32 Ces artefacts provoquent des erreurs brutales et démesurées sur la position ou l'intervalle de temps mesuré ![][image1].  
Si le système d'acquisition avale aveuglément cette valeur contaminée, l'innovation d'état ![][image63] injectera un choc incommensurable dans l'accélération du système, entraînant un soubresaut destructeur au niveau de la boucle d'asservissement. L'application industrielle de l'estimation bayésienne commande impérieusement de concevoir une couche de filtrage (Gating) se basant sur le calcul de la distance probabiliste du ![][image64] de Mahalanobis.33  
Avant d'appliquer les équations de mise à jour, l'intégrité de la mesure doit être éprouvée en normalisant l'innovation au carré avec sa covariance prédictive associée :  
![][image65]  
Si la condition stricte d'inégalité n'est pas remplie (par exemple, en choisissant ![][image66], ce qui circonscrit une zone de confiance de probabilité d'exclusion à l'intérieur d'un intervalle de trois écarts-types ![][image67]), la trame d'information ou l'impulsion mesurée doit être irrévocablement rejetée et considérée comme corrompue. Lors d'une telle occurrence, l'algorithme ne procèdera pas à la correction de l'état mais poursuivra imperturbablement sa projection temporelle à l'aide de sa prédiction cinématique, préservant l'asservissement contre tout à-coup de couple.

#### **8.3. Analyse de Performance Limite selon la Résolution (ANC vs. Matrice Fixe)**

Dans le contexte spécifique d'un système plafonnant à une vitesse maximale de 600 rpm (soit 10 tours par seconde), l'amplitude des erreurs est gouvernée par la résolution de l'encodeur sélectionné.

##### **Démonstration numérique des covariances limites de position (![][image68]) :**

###### **1\. Encodeur incrémental ABZ classique à 2500 CPR (10 000 incréments par tour en quadrature) :**

![][image69]  
![][image70]  
À très basse vitesse (en dessous de 6 rpm), l'arbre moteur met plus de 1 ms à franchir une encoche. Sans loi adaptative, la position lue à 1 kHz stagne sur plusieurs cycles avant de sauter brusquement d'un pas. Ce bruit de quantification non gaussien induit des instabilités majeures dans un filtre de Kalman classique. La loi ANC de la section 5.2 résout ce problème de manière fluide : le filtre augmente dynamiquement la valeur de la covariance de position ![][image71] et diminue celle associée à la mesure temporelle de vitesse ![][image55]. Le filtre s'appuie alors à 99% sur la mesure du délai d'impulsion pour corriger la vitesse, ignorant l'aspect "escalier" de la position.

###### **2\. Encodeur Absolu Haute Résolution 22 bits (![][image72] incréments par tour) :**

![][image73]  
![][image74]  
Dans ce cas, le bruit de quantification spatial est inférieur au bruit électrique. Une correction de type Position-Seule (POM) est parfaitement adaptée. La résolution extrêmement élevée permet au filtre de Kalman d'extraire la vitesse et l'accélération avec une dynamique et une pureté inégalables, éliminant le besoin d'un timer de capture externe.

### **9\. Conclusion Synthétique**

La reformulation de votre filtre de Kalman sous la forme d'un estimateur d'état cinématique polymorphe et modulaire apporte une réponse rigoureuse et définitive aux dysfonctionnements de l'implémentation originale.  
L'analyse de stabilité basée sur le théorème d'observabilité de Kalman prouve de manière irréfutable que le système reste parfaitement stable même dans sa configuration POM (BiSS-C à 10 kHz), écartant toute crainte de dérive ou d'instabilité liée à la réciprocité des calculs de position et de vitesse.2  
En séparant structurellement les fonctions de mise à jour du filtre, vous disposez d'un code hautement optimisé :

1. **En mode Haute Résolution (BiSS-C) :** Le filtre opère une correction cinématique isochrone pure à 10 kHz avec une précision chirurgicale, sans nécessiter d'autres variables d'entrée que la position absolue.2  
2. **En mode Incrémental (ABZ) :** Le filtre fusionne de manière asynchrone la position à 1 kHz et la vitesse mesurée à chaque flanc d'impulsion. L'implémentation de la covariance adaptative d'horloge (loi ANC) et la compensation de premier ordre du retard de phase (OOSM) éliminent le bruit de quantification spatial et le retard de groupe sans introduire de déphasage dans la boucle fermée.

Cette architecture garantit à votre système de contrôle de mouvement une réactivité maximale et un comportement de suivi de vitesse d'une pureté cristalline sur l'ensemble de votre plage de fonctionnement.

##### **Sources des citations**

1. Velocity and acceleration estimation for optical incremental encoders \- Tech United, consulté le juin 16, 2026, [https://techunited.nl/media/files/velocity\_and\_acceleration\_estimation\_for\_optical\_incremental\_encoders.pdf](https://techunited.nl/media/files/velocity_and_acceleration_estimation_for_optical_incremental_encoders.pdf)  
2. Kalman filter \- Wikipedia, consulté le juin 16, 2026, [https://en.wikipedia.org/wiki/Kalman\_filter](https://en.wikipedia.org/wiki/Kalman_filter)  
3. (PDF) Performance analysis of α- β- γtracking filters using position and velocity measurements \- ResearchGate, consulté le juin 16, 2026, [https://www.researchgate.net/publication/275240076\_Performance\_analysis\_of\_a-\_b-\_gtracking\_filters\_using\_position\_and\_velocity\_measurements](https://www.researchgate.net/publication/275240076_Performance_analysis_of_a-_b-_gtracking_filters_using_position_and_velocity_measurements)  
4. Tuning Kalman Filter to Improve State Estimation \- MATLAB & Simulink \- MathWorks, consulté le juin 16, 2026, [https://www.mathworks.com/help/fusion/ug/tuning-kalman-filter-to-improve-state-estimation.html](https://www.mathworks.com/help/fusion/ug/tuning-kalman-filter-to-improve-state-estimation.html)  
5. State Estimation Using Time-Varying Kalman Filter \- MATLAB & Simulink Example, consulté le juin 16, 2026, [https://www.mathworks.com/help/control/ug/state-estimation-using-time-varying-kalman-filter.html](https://www.mathworks.com/help/control/ug/state-estimation-using-time-varying-kalman-filter.html)  
6. KinematicKalman.cpp  
7. Analysis of Kalman Filter Based Approaches for Fusing Out-of-Sequence Measurements corrupted by Systematic Errors, consulté le juin 16, 2026, [https://mobile.aau.at/\~welmenre/papers/mauthner-MFI2008.pdf](https://mobile.aau.at/~welmenre/papers/mauthner-MFI2008.pdf)  
8. (PDF) An FPGA-Based Trigonometric Kalman Filter Approach for Improving the Measurement Quality of a Multi-Head Rotational Encoder \- ResearchGate, consulté le juin 16, 2026, [https://www.researchgate.net/publication/386482644\_An\_FPGA-Based\_Trigonometric\_Kalman\_Filter\_Approach\_for\_Improving\_the\_Measurement\_Quality\_of\_a\_Multi-Head\_Rotational\_Encoder](https://www.researchgate.net/publication/386482644_An_FPGA-Based_Trigonometric_Kalman_Filter_Approach_for_Improving_the_Measurement_Quality_of_a_Multi-Head_Rotational_Encoder)  
9. Velocity and Acceleration Estimation Via Quantization Step Aware Convex Optimization: Real-Time Implementation on FPGA, consulté le juin 16, 2026, [https://ras.papercept.net/images/temp/AIM/files/0287.pdf](https://ras.papercept.net/images/temp/AIM/files/0287.pdf)  
10. How to Estimate Encoder Velocity Without Making Stupid Mistakes: Part II (Tracking Loops and PLLs) \- Jason Sachs \- EmbeddedRelated.com, consulté le juin 16, 2026, [https://www.embeddedrelated.com/showarticle/530.php](https://www.embeddedrelated.com/showarticle/530.php)  
11. Was a Kalman Filter a bad idea in this situation or am I simply not tuning properly?, consulté le juin 16, 2026, [https://dsp.stackexchange.com/questions/96139/was-a-kalman-filter-a-bad-idea-in-this-situation-or-am-i-simply-not-tuning-prope](https://dsp.stackexchange.com/questions/96139/was-a-kalman-filter-a-bad-idea-in-this-situation-or-am-i-simply-not-tuning-prope)  
12. High-factor interpolation method based on space-time modulation and a Kalman filter for optical encoders \- Optica Publishing Group, consulté le juin 16, 2026, [https://opg.optica.org/abstract.cfm?uri=oe-32-2-1914](https://opg.optica.org/abstract.cfm?uri=oe-32-2-1914)  
13. Energy-efficient path planning for Robotic Bulkhead Inspection using Residual-Enhanced UKF and Hierarchical MPC \- PMC, consulté le juin 16, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC13001917/](https://pmc.ncbi.nlm.nih.gov/articles/PMC13001917/)  
14. DSP-based absolute encoder communication and ultrasonic motor, consulté le juin 16, 2026, [https://www.spiedigitallibrary.org/conference-proceedings-of-spie/13968/1396807/DSP-based-absolute-encoder-communication-and-ultrasonic-motor-speed-measurement/10.1117/12.3090991.full](https://www.spiedigitallibrary.org/conference-proceedings-of-spie/13968/1396807/DSP-based-absolute-encoder-communication-and-ultrasonic-motor-speed-measurement/10.1117/12.3090991.full)  
15. A Velocity Measurement Method for Acceleration Control, consulté le juin 16, 2026, [https://sucra.repo.nii.ac.jp/record/9325/files/A1002447.pdf](https://sucra.repo.nii.ac.jp/record/9325/files/A1002447.pdf)  
16. Angular Speed Calculation Based on Incremental Encoders, Frequency and Period Estimation Techniques and Inverse-Variance-Weighted Average Method \- MDPI, consulté le juin 16, 2026, [https://www.mdpi.com/1996-1073/19/11/2673](https://www.mdpi.com/1996-1073/19/11/2673)  
17. A novel variable M/T method for speed measurement with high precision in a wide speed range \- ResearchGate, consulté le juin 16, 2026, [https://www.researchgate.net/publication/266644148\_A\_novel\_variable\_MT\_method\_for\_speed\_measurement\_with\_high\_precision\_in\_a\_wide\_speed\_range](https://www.researchgate.net/publication/266644148_A_novel_variable_MT_method_for_speed_measurement_with_high_precision_in_a_wide_speed_range)  
18. A Novel Method for Determining Angular Speed and Acceleration Using Sin-Cos Encoders, consulté le juin 16, 2026, [https://www.mdpi.com/1424-8220/21/2/577](https://www.mdpi.com/1424-8220/21/2/577)  
19. Incremental Encoder Speed Acquisition Using an STM32 Microcontroller and NI ELVIS, consulté le juin 16, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC9324733/](https://pmc.ncbi.nlm.nih.gov/articles/PMC9324733/)  
20. Engine Speed Monitoring: The Alpha-Beta Filter \- Microstar Laboratories, consulté le juin 16, 2026, [https://www.mstarlabs.com/control/engspeed.html](https://www.mstarlabs.com/control/engspeed.html)  
21. Velocity estimation from widely spaced encoder pulses \- ResearchGate, consulté le juin 16, 2026, [https://www.researchgate.net/publication/224607025\_Velocity\_estimation\_from\_widely\_spaced\_encoder\_pulses](https://www.researchgate.net/publication/224607025_Velocity_estimation_from_widely_spaced_encoder_pulses)  
22. Kalman filter for position and velocity: introducing speed estimates, consulté le juin 16, 2026, [https://dsp.stackexchange.com/questions/8860/kalman-filter-for-position-and-velocity-introducing-speed-estimates](https://dsp.stackexchange.com/questions/8860/kalman-filter-for-position-and-velocity-introducing-speed-estimates)  
23. Getting up to speed with Kalman filters \- VectorNav Technologies, consulté le juin 16, 2026, [https://www.vectornav.com/resources/inertial-navigation-primer/math-fundamentals/math-kalman](https://www.vectornav.com/resources/inertial-navigation-primer/math-fundamentals/math-kalman)  
24. Kalman Filter-Based Adaptive Delay Compensation for Benchmark Problem in Real-Time Hybrid Simulation \- MDPI, consulté le juin 16, 2026, [https://www.mdpi.com/2076-3417/10/20/7101](https://www.mdpi.com/2076-3417/10/20/7101)  
25. State Observers and Kalman Filters \- FIRST Robotics Competition \- WPILib, consulté le juin 16, 2026, [https://docs.wpilib.org/en/stable/docs/software/advanced-controls/state-space/state-space-observers.html](https://docs.wpilib.org/en/stable/docs/software/advanced-controls/state-space/state-space-observers.html)  
26. How to Estimate Encoder Velocity Without Making Stupid Mistakes (xpost from electronics), consulté le juin 16, 2026, [https://www.reddit.com/r/ECE/comments/15xbvr/how\_to\_estimate\_encoder\_velocity\_without\_making/](https://www.reddit.com/r/ECE/comments/15xbvr/how_to_estimate_encoder_velocity_without_making/)  
27. Kalman Filter Fusion: A Comprehensive Guide for 2025 \- Shadecoder \- 100% Invisibile AI Coding Interview Copilot, consulté le juin 16, 2026, [https://www.shadecoder.com/topics/kalman-filter-fusion-a-comprehensive-guide-for-2025](https://www.shadecoder.com/topics/kalman-filter-fusion-a-comprehensive-guide-for-2025)  
28. How to actually fuse sensor using Extended Kalman Filter \- Robotics Stack Exchange, consulté le juin 16, 2026, [https://robotics.stackexchange.com/questions/22115/how-to-actually-fuse-sensor-using-extended-kalman-filter](https://robotics.stackexchange.com/questions/22115/how-to-actually-fuse-sensor-using-extended-kalman-filter)  
29. Incorporation of Time Delayed Measurements in a Discrete-time Kalman Filter, consulté le juin 16, 2026, [https://backend.orbit.dtu.dk/ws/files/4363506/Larsen.pdf](https://backend.orbit.dtu.dk/ws/files/4363506/Larsen.pdf)  
30. A 3D State Space Formulation of a Navigation Kalman Filter for Autonomous Vehicles \- DTIC, consulté le juin 16, 2026, [https://apps.dtic.mil/sti/tr/pdf/ADA282853.pdf](https://apps.dtic.mil/sti/tr/pdf/ADA282853.pdf)  
31. An Adaptive Kalman Filter With Inaccurate Noise Covariances in the Presence of Outliers, consulté le juin 16, 2026, [https://www.researchgate.net/publication/349002138\_An\_adaptive\_Kalman\_filter\_with\_inaccurate\_noise\_covariances\_in\_the\_presence\_of\_outliers](https://www.researchgate.net/publication/349002138_An_adaptive_Kalman_filter_with_inaccurate_noise_covariances_in_the_presence_of_outliers)  
32. Remote Sens., Volume 17, Issue 7 (April-1 2025\) – 212 articles \- MDPI, consulté le juin 16, 2026, [https://www.mdpi.com/2072-4292/17/7](https://www.mdpi.com/2072-4292/17/7)


---

<a id="partie-3"></a>

# Partie 3 — Expertise DSP : arbitrage Kalman vs KinematicPLL

*Source d'origine : `doc/analyse_dsp_filtrage_encodeurs.md`*

## Rapport d'Expertise DSP : Refonte du Filtrage, Estimation d'État et Gestion des Encodeurs (OpenFFBoard)

Ce document constitue la synthèse complète et approfondie du brainstorming contradictoire sur le traitement numérique du signal (DSP) appliqué à l'architecture d'estimation cinématique et de contrôle haptique d'OpenFFBoard.

Il s'appuie sur une analyse croisée selon quatre perspectives méthodologiques :
1. **Analyse 1 (Perspective Optimiste / Idées constructives DSP)** : Foisonnement de solutions modernes de filtrage et d'estimation.
2. **Analyse 2 (Perspective Sceptique / Critique et Contradiction)** : Démontage rigoureux des fausses bonnes idées, analyse de la physique du volant haptique et des causes profondes de divergence matérielle.
3. **Analyse 3 (Perspective Optimiste avancée / Architecture logicielle et Intégration STM32/TMC4671)** : Co-conception matérielle, multi-cadence, découplage des boucles de contrôle.
4. **Analyse 4 (Perspective Sceptique avancée / Crash-test temps réel, limites numériques et failles de sécurité)** : Étude des cas limites (perte de paquets SPI, instabilités en virgule flottante, comportement aux limites).
5. **Évaluation Synthétique et Arbitrage Architectural** : Sélection des meilleures approches et feuille de route technique pour le projet `OpenFFBoard-refactor-encoder`.

---

### 1. Contexte, Diagnostic et Autopsie de l'Incident Matériel

#### 1.1. Rappel de la Problématique Initiale
Dans l'ancienne implémentation (`OpenFFBoard-development`) :
* La vitesse et l'accélération étaient calculées dans `Axis::updateMetrics()` à 1 kHz par simple dérivation numérique (Euler arrière) :
  $$\omega_{\text{raw}}[k] = \frac{\theta[k] - \theta[k-1]}{\Delta t}$$
  $$a_{\text{raw}}[k] = \frac{\omega_{\text{raw}}[k] - \omega_{\text{raw}}[k-1]}{\Delta t}$$
* Pour atténuer le bruit de quantification spatial inhérent à cette double dérivation, deux filtres IIR Biquad passe-bas en cascade étaient appliqués (fréquences de coupure typiques : $f_c \in [40, 180]\text{ Hz}$ pour la vitesse, $f_c \in [40, 120]\text{ Hz}$ pour l'accélération, avec $Q \in [0.30, 0.55]$).
* De plus, les effets conditionnels DirectInput / FFB (Damper, Friction, Inertia dans `Axis.cpp`) appliquaient leurs propres Biquads supplémentaires sur les sorties déjà filtrées.

**Le constat d'échec :**
* **Retard de phase dramatique** : Un Biquad d'ordre 2 à 40 Hz injecte à lui seul un retard de groupe de $t_{\text{del}} \approx 6.25\text{ ms}$ ($\sim 90^\circ$ de déphasage à 40 Hz). Deux Biquads en cascade (vitesse puis effet amortisseur) créent un déphasage excédant $120^\circ$ dans la bande de fréquence des mains du pilote ($5 - 25\text{ Hz}$).
* **Instabilité en boucle fermée (Énergie active parasite)** : Le couple d'amortissement $T = -B \cdot \omega_{\text{filtre}}$ n'est plus en opposition de phase pure avec la vitesse réelle mais présente une composante en phase avec la position ou l'accélération. Au lieu de dissiper l'énergie, l'effet injecte de l'énergie mécanique dans le système (théorème de passivité violé), déclenchant de violentes oscillations spontanées dès que le volant est lâché ("hands-off wobble").
* **Sur-filtrage obligatoire** : Pour stabiliser le système, l'utilisateur était contraint d'écraser la dynamique par des filtres encore plus lourds, transformant le ressenti de conduite en sensation spongieuse et déconnectée de la piste.

#### 1.2. Autopsie Mathématique : Pourquoi le Filtre de Kalman Initial a Failli Griller l'Électronique

La tentative d'implémentation du filtre de Kalman cinématique d'ordre 3 (`KinematicKalman.cpp` dans la branche `refactor-encoder`) a provoqué un emballement violent de la vitesse prévisionnelle, saturant le couple moteur et mettant en péril les ponts en H et les bobinages du moteur. 

L'analyse minutieuse du code et des équations met en lumière quatre mécanismes d'emballement simultanés :

##### A. Le Piège du Modèle à Accélération Blanche Continue (CWNA d'ordre 3)
Le filtre modélisait l'état sous la forme $x = [\theta, \omega, \alpha]^T$ en supposant que l'accélération $\alpha$ évolue selon une marche aléatoire (bruit de jerk $q_{\text{continuous}}$) :
$$\dot{\theta} = \omega, \quad \dot{\omega} = \alpha, \quad \dot{\alpha} = w(t)$$
Ce modèle est fondamentalement faux pour un système haptique manipulé par un être humain et freiné par des forces passives :
1. **Absence d'amortissement intrinsèque** : Le système prédit que si une accélération $\alpha$ apparaît, la vitesse $\omega$ va continuer à croître linéairement sans aucune résistance mécanique ($F_{\text{drag}} = 0$).
2. **Double intégration de l'innovation** : En mode "Position-Only Measurement" (BiSS-C ou lecture de position à 1 kHz), la matrice d'observation est $H = [1, 0, 0]$. Tout écart entre la position mesurée et la prédiction injecte une correction sur l'accélération via le gain $K_2 = P_{02} / S$. Si le paramètre $q_{\text{continuous}}$ est fixé à une valeur élevée (10000 dans le code) pour être "réactif", $K_2$ devient démesuré. Une micro-perturbation ou un saut de trame génère un bond d'accélération gigantesque qui s'intègre au cycle suivant sur la vitesse :
   $$\omega_{\text{pred}} = \omega + \alpha \cdot \Delta t$$
   Si le cycle suivant subit un retard ou si l'innovation oscille, l'accélération estimée monte en flèche, propulsant $\omega$ vers des valeurs astronomiques.

##### B. Instabilité Temporelle Variable ($\Delta t$ non déterministe sous RTOS)
Dans `EncoderManager.cpp` et `Encoder.cpp` :
* `EncoderManager` tourne dans une tâche FreeRTOS réveillée par timer.
* `triggerRead()` calcule :
  ```cpp
  uint32_t now = micros();
  uint32_t dt_us = now - last_update_time;
  float dt = (float)dt_us * 1e-6f;
  ```
Dans le cas d'une préemption FreeRTOS ou d'un retard de trame SPI (comme `WaitForNotification(20)`), $\Delta t$ peut ponctuellement passer de $100\text{ }\mu\text{s}$ à $2000\text{ }\mu\text{s}$ ($2\text{ ms}$).
Or, dans la matrice de transition et dans la propagation de covariance :
* Le terme d'accélération dans la position varie en $\Delta t^2$ ($0.5 \cdot \alpha \cdot \Delta t^2$).
* La matrice $Q$ contient des termes en $\Delta t^5 / 20$, $\Delta t^4 / 8$, $\Delta t^3 / 3$.
Lors d'un saut de $\Delta t$, la prédiction projette un déplacement énorme. Lorsque la vraie mesure de position arrive au cycle d'après, l'erreur de position $y = z - \theta_{\text{pred}}$ est gigantesque. Le filtre tente de compenser en appliquant une correction inverse ultra-violente sur $\alpha$, générant des à-coups de couple à saturation maximale du variateur.

##### C. L'Erreur Mortelle dans la Forme de Joseph de `updateVelocity()`
Dans `KinematicKalman.cpp` (l. 324-334), lors de la correction de vitesse avec la matrice $H_{\text{vel}} = [0, 1, H_2]$ où $H_2 = -0.5 \cdot \Delta t_{\text{pulse}}$ :
```cpp
P[0][0] = W0*W0 * P11_old + K0*K0 * R_v;
```
Une erreur tragique de copier-coller mathématique a remplacé les termes de covariance de position $P_{00}$ par $P_{11}$ et des index erronés sur l'ensemble de la mise à jour de Joseph ! 
Résultat : dès qu'une correction de vitesse était déclenchée, la matrice de covariance $P$ était corrompue avec des valeurs fausses, détruisant instantanément la propriété définie-positive. Les gains de Kalman $K$ ont alors divergé vers l'infini ou changé de signe, entraînant l'emballement spontané de l'état estimé.

##### D. Absence Totale de Bornage Physique (Sanity Checks)
Ni l'état interne ($x_1, x_2$), ni les sorties `getSpeed()` et `getAccel()` ne possédaient de limiteurs physiques absolus. Un volant d'entraînement direct ("Direct Drive") possède des limites cinématiques indépassables :
* Vitesse maximale absolue : $600\text{ rpm} \approx 62.83\text{ rad/s}$ ($10\text{ tours/s}$).
* Accélération angulaire maximale (couple max 25-30 Nm sur une inertie de volant de $\approx 0.02 - 0.05\text{ kg}\cdot\text{m}^2$) :
  $$\alpha_{\text{max}} \approx \frac{30\text{ Nm}}{0.02\text{ kg}\cdot\text{m}^2} = 1500\text{ rad/s}^2 \approx 240\text{ rot/s}^2$$
Le filtre a produit en interne des accélérations virtuelles de plus de $50\,000\text{ rad/s}^2$ et des vitesses de plusieurs milliers de rad/s, injectant directement le courant de crête maximal dans le TMC4671 via l'effet d'amortissement et l'extrapolation de phase électrique.

---

### 2. Débat Contradictoire en 4 Perspectives

Les quatre sections suivantes confrontent les hypothèses d'ingénierie selon les quatre prismes demandés.

---

#### 2.1. Analyse 1 (Perspective Optimiste : Foisonnement d'Idées DSP Constructives)

Cette analyse explore toutes les voies d'amélioration DSP possibles pour maximiser la réactivité dynamique, minimiser le bruit de quantification et éliminer le déphasage.

##### Idée 1 : Filtre Alpha-Beta-Gamma ($\alpha-\beta-\gamma$) Stationnaire à Gains Optimisés
* **Principe** : Remplacer le filtre de Kalman complet (avec recalcul dynamique de la covariance $P$, inversion scalaire $S$ et mise à jour de Joseph) par sa forme stationnaire asymptotique (gains constants $K = [\alpha, \beta/\Delta t, \gamma/\Delta t^2]^T$). Les gains sont calculés hors-ligne via l'indice de manoeuvrabilité de Kalata :
  $$\lambda = \frac{\sigma_w \Delta t^2}{\sigma_v}$$
* **Gain Déphasage / Bruit** : Réponse impulsionnelle déterministe. Élimine complètement le déphasage non linéaire provoqué par les variations dynamiques des gains $K$.
* **Coût CPU** : Infime ($\sim 12\text{ à } 16$ cycles CPU sur Cortex-M4, aucune division ni mise à jour de matrice).
* **Risques / Stabilité** : Système linéaire invariant dans le temps (LTI). La stabilité se vérifie trivialement par le placement des pôles du polynôme caractéristique à l'intérieur du cercle unité discret.

##### Idée 2 : Filtre de Kalman Réduit à 2 États ($\theta, \omega$) avec Modèle à Vitesse Constante (CWNA d'ordre 2)
* **Principe** : Retirer l'état accélération du filtre de Kalman. L'état devient $x = [\theta, \omega]^T$, le bruit de processus $q_{\text{vel}}$ s'applique directement sur la vitesse. L'accélération est alors calculée séparément si nécessaire par un différenciateur régularisé.
* **Gain Déphasage / Bruit** : Réduit le déphasage intrinsèque de $90^\circ$ à moins de $15^\circ$ car la vitesse n'est plus couplée à un intégrateur d'accélération non borné.
* **Coût CPU** : $\sim 45$ cycles FPU (inversion d'un scalaire, matrice $P$ de taille $2 \times 2$ ne contenant que 3 variables indépendantes : $P_{00}, P_{01}, P_{11}$).
* **Risques** : Aucune divergence d'accélération possible puisque $\alpha$ n'est plus un état d'intégration.

##### Idée 3 : Dérivée à Retard Minimal par Régression Polynomiale Causale (Savitzky-Golay Causal)
* **Principe** : Utiliser un filtre FIR convolutif calculant la pente de la position sur une fenêtre glissante glissante de $N$ points passés ($N=5$ à 7 points à 10 kHz) par moindres carrés pondérés vers le point présent :
  $$\omega[k] = \sum_{i=0}^{N-1} c_i \cdot \theta[k-i]$$
* **Gain Déphasage / Bruit** : Retard de groupe rigoureusement constant et déterministe ($t_{\text{group}} = \frac{N-1}{2} \cdot \Delta t$). À 10 kHz avec $N=5$, le délai est de seulement $200\text{ }\mu\text{s}$ (contre $6250\text{ }\mu\text{s}$ pour le Biquad à 40 Hz).
* **Coût CPU** : $\sim 8$ cycles via l'instruction SIMD/DSP `SMLAD` ou FPU `VMLA.F32`.
* **Risques** : Sensible aux sauts de quantification sur les encodeurs basse résolution. Exceptionnel sur encodeur 22 bits.

##### Idée 4 : Tracking Loop / Boucle à Verrouillage de Phase Numérique (PLL Typique Servomoteurs Industriels)
* **Principe** : Asservir un oscillateur numérique interne (NCO / modèle de position continue) sur la position reçue au moyen d'un correcteur PI cinématique :
  $$\epsilon[k] = \theta_{\text{mes}}[k] - \theta_{\text{est}}[k]$$
  $$\omega_{\text{est}}[k] = \omega_{\text{est}}[k-1] + K_p (\epsilon[k] - \epsilon[k-1]) + K_i \Delta t \cdot \epsilon[k]$$
  $$\theta_{\text{est}}[k] = \theta_{\text{est}}[k-1] + \omega_{\text{est}}[k] \cdot \Delta t$$
* **Gain Déphasage / Bruit** : Structure standard de l'industrie (Kollmorgen, Heidenhain, Trinamic). Déphasage nul à vitesse constante. Filtrage naturel du bruit HF par le pôle d'intégration.
* **Coût CPU** : $\sim 10$ cycles FPU, aucune matrice, stabilité inconditionnelle garantie par la marge de phase du régulateur PI.

##### Idée 5 : Décimation Polyphase / Filtre CIC (Cascade Integrator-Comb) entre 10 kHz et 1 kHz
* **Principe** : Faire tourner l'acquisition et le pré-filtrage de l'encodeur à 10 kHz dans `EncoderManager`, puis moyenner/décimer proprement vers la boucle FFB à 1 kHz à l'aide d'un filtre CIC du premier ou second ordre sans calcul flottant coûteux.
* **Gain Déphasage / Bruit** : Gain de suréchantillonnage de $10\text{ dB}$ sur le rapport signal sur bruit ($SNR$). Élimine le repliement de spectre (aliasing) dans la boucle d'effet à 1 kHz.
* **Coût CPU** : 3 additions entières 32 bits par échantillon à 10 kHz.

##### Idée 6 : Modélisation Non Linéaire des Effets Conditionnels (Friction Régularisée par Tanh)
* **Principe** : Remplacer l'effet de frottement sec de Coulomb actuel basé sur la fonction discontinue $\text{sign}(\omega)$ (qui génère un cycle limite de cliquetis à vitesse quasi-nulle) par une fonction sigmoïde continue :
  $$F_{\text{friction}}(\omega) = -F_c \cdot \tanh\left(\frac{\omega}{\omega_0}\right) - \sigma \cdot \omega$$
* **Gain Déphasage / Bruit** : Supprime le besoin d'un Biquad lourd sur la friction. Le passage à zéro est doux, exempt de bruit haute fréquence.
* **Coût CPU** : Une approximation polynomiale de $\tanh(x)$ en 6 cycles FPU.

##### Idée 7 : Observateur de Perturbation (DOB - Disturbance Observer) pour Damper et Inertie
* **Principe** : Utiliser la connaissance du couple appliqué par le moteur $T_{\text{cmd}}$ et l'inertie du rotor $J_m$ pour construire un observateur d'état à modèle inverse (Luenberger). Le couple de réaction des mains de l'utilisateur est estimé comme une perturbation externe plutôt que comme un bruit stochastique inconnu.
* **Gain Déphasage / Bruit** : Déphasage dynamique nul sur les accélérations induites par le moteur lui-même.

##### Idée 8 : Filtrage Passe-Bas Adaptatif selon la Vitesse (Variable Bandwidth Filter)
* **Principe** : Ajuster la bande passante du filtre de vitesse en fonction de l'amplitude de la vitesse :
  * À basse vitesse ($|\omega| < 0.5\text{ rad/s}$) : bruit de quantification dominant $\rightarrow$ bande passante basse ($30\text{ Hz}$).
  * À haute vitesse ($|\omega| > 5\text{ rad/s}$) : rapport signal/bruit élevé $\rightarrow$ bande passante haute ($250\text{ Hz}$), déphasage quasi-nul.
* **Gain Déphasage / Bruit** : Le système est ultra-réactif lors des contre-braquages rapides et parfaitement silencieux à l'arrêt.

---

#### 2.2. Analyse 2 (Perspective Sceptique : Critique Radicale & Contradictions)

Cette analyse passe au crible de la réalité physique et mathématique les hypothèses précédentes et celles du document [Partie 2](#partie-2).

##### Critique 1 : La Fausse Promesse du "Filtrage Zéro-Phase Lag"
* Le document [Partie 2](#partie-2) revendique à plusieurs reprises un "filtrage Zero-Phase Lag". **C'est une impossibilité mathématique et physique causale**.
* Tout système causal estimant la dérivée à partir de mesures bruitées doit faire un compromis fondamental (théorème de Bode-Poisson / limite de Cramér-Rao).
* Prétendre éliminer le retard en "rétroprojetant" la vitesse via l'accélération ($\omega_{\text{comp}} = \omega - 0.5 \alpha \Delta t$) ne fait que reporter l'incertitude sur une variable d'accélération encore plus bruitée et prompte à l'instabilité. Lorsque l'accélération est erronée (ce qui est systématique lors d'un choc ou d'un changement de trajectoire par le pilote), cette compensation aggrave l'erreur instantanée.

##### Critique 2 : Inadaptation Fondamentale de l'Accélération d'Ordre 3 en Volant FFB
* Pourquoi l'accélération ne doit **JAMAIS** être une variable libre dans un filtre d'encodeur FFB :
  * L'être humain applique un couple non modélisé qui peut varier de $+10\text{ Nm}$ à $-10\text{ Nm}$ en $30\text{ ms}$ (jerk humain énorme).
  * Le moteur FFB applique un couple pulsé à 1 kHz (effets de choc de trottoir, texture de route).
  * Modéliser cette dynamique par un bruit blanc gaussien $w(t)$ avec un $Q$ fixe est une aberration : soit $Q$ est faible et le filtre est en retard permanent de plusieurs dizaines de millisecondes, soit $Q$ est élevé et le filtre amplifie le bruit de mesure jusqu'à faire vibrer la structure mécanique.
* **Verdict** : L'état d'accélération doit être supprimé de l'observateur d'encodeur. L'encodeur doit estimer $(\theta, \omega)$. L'effet d'inertie (s'il est activé) doit être calculé soit par différence de vitesse filtrée à bande limitée, soit estimé via le modèle dynamique courant/couple du moteur.

##### Critique 3 : Danger Extrême de la T-Method / MT-Method par EXTI sur Microcontrôleur
* Dans `EncoderLocal.cpp`, l'activation des interruptions EXTI sur les voies A et B pour mesurer $\Delta t_{\text{pulse}}$ (T-Method) est une bombe à retardement :
  * Sur un encodeur ABZ à 2500 CPR (10 000 transitions en quadrature par tour) tournant à $600\text{ rpm}$ ($10\text{ tours/s}$) :
    $$f_{\text{irq}} = 10 \times 10\,000 = 100\,000\text{ interruptions par seconde !}$$
  * À 100 kHz, avec un processeur Cortex-M4 à 168 MHz (période d'horloge de $5.95\text{ ns}$), chaque interruption dispose de moins de 1680 cycles pour exécuter le prologue d'interruption, le pipeline FreeRTOS et le calcul.
  * Le CPU est instantanément saturé à 100 %, entraînant la famine des autres threads, le déclenchement du Watchdog ou la perte du bus USB.
* **Verdict** : La T-Method logicielle par interruption GPIO est à proscrire totalement. Seul un périphérique Timer matériel configuré en mode quadrature avec mesure de période intégrée (32-bit Timer Capture) est admissible, ou s'en tenir à une M-Method synchrone à fréquence fixe.

##### Critique 4 : Vulnérabilité de l'Équation ANC ($R_v \propto 1 / \Delta t^4$)
* L'équation proposée dans [Partie 2](#partie-2) :
  $$R_v = \frac{\sigma_\tau^2 \cdot \Delta \theta^2}{\Delta t_{\text{pulse}}^4}$$
  présente une instabilité numérique fatale en virgule flottante simple précision :
  * Si le volant accélère brutalement, $\Delta t_{\text{pulse}}$ devient minuscule (ex: $10\text{ }\mu\text{s} = 10^{-5}\text{ s}$).
  * $\Delta t_{\text{pulse}}^4 = 10^{-20}$. En simple précision IEEE-754, le format standard normalisé plafonne avec un dénormalisé à $\sim 10^{-38}$, mais la division par $10^{-20}$ fait exploser $R_v$ à des valeurs de l'ordre de $10^{14}$ !
  * Les matrices $S$ et $K$ subissent des pertes de chiffres significatifs (catastrophic cancellation), générant des valeurs NaN (Not-a-Number) ou infinis qui se propagent dans tout le firmware.

---

#### 2.3. Analyse 3 (Perspective Optimiste Avancée : Architecture Matérielle & Intégration)

Cette analyse se concentre sur l'exploitation optimale de l'architecture matérielle STM32F4 + TMC4671 pour garantir un temps réel strict et une synchronisation irréprochable.

##### Proposition 1 : Découplage Strict des 3 Boucles Temporelles
Il est impératif de cesser de faire converger toutes les exigences vers un seul composant logiciel monolithique. Il existe trois échelles de temps fondamentalement différentes :
1. **Boucle Ultra-Rapide (10 kHz - Synchronisation FOC TMC4671)** :
   * Objectif : Injecter la position électrique du rotor $\phi_E$ dans le registre `0x1C` du TMC4671 pour que la commutation FOC reste alignée en phase.
   * Exigence : Latence ultra-faible ($< 25\text{ }\mu\text{s}$), gigue temporelle minimale ($< 1\text{ }\mu\text{s}$). L'accélération n'est pas requise ici, seule une position lissée et extrapolée sur le délai de transfert SPI importe.
2. **Boucle Rapide (1 kHz - Boucle d'Effets et Métriques FFB)** :
   * Objectif : Calculer les effets Damper, Friction, Inertia, Spring, et communiquer avec le PC hôte via USB HID.
   * Exigence : Vitesse angulaire $\omega$ propre et régulière, retard de phase modéré ($< 2\text{ ms}$), protection absolue contre les dépassements de couple.
3. **Boucle Lente (100 Hz - Télémétrie et Supervision Sécurité)** :
   * Objectif : Détection Hands-off, surveillance thermique, diagnostic des erreurs CRC d'encodeur.

##### Proposition 2 : Élimination du "Ping-Pong" FreeRTOS par Acquisition Directe en Interruption Matérielle (Zero-Context-Switch)
* Actuellement : `TIM6 ISR -> Notify -> Thread EncoderManager -> SPI IT -> SPI ISR -> Notify -> Thread Encoder -> Kalman`.
* **Architecture Cible** :
  * Le Timer 6 déclenche directement un transfert SPI DMA (sans aucune charge CPU).
  * L'interruption de fin de transfert DMA (ISR) exécute immédiatement l'estimateur d'état (PLL ou Kalman 2 états) en moins de 100 cycles CPU.
  * Les résultats $(\theta, \omega)$ sont écrits dans des variables protégées par un verrou atomique léger (`SeqLock` déjà présent dans le code).
  * Zéro thread réveillé, zéro latence d'ordonnancement, déterminisme absolu du $\Delta t$.

##### Proposition 3 : Gestion Optimale des Encodeurs Internes TMC4671 (Position 32 bits `0x6B` vs Vitesse `0x6A`)
* Dans le cas où l'encodeur est raccordé directement sur les broches matérielles du TMC4671 (ABZ, SinCos, UVW, Hall) :
  * Le TMC4671 maintient en matériel un accumulateur de position mécanique multi-tours 32 bits haute résolution (`PID_POSITION_ACTUAL`, registre `0x6B`, $65\,536\text{ counts/tour}$) déjà lu à $1\text{ kHz}$ par `TMC4671::getPos()`, ainsi qu'un registre de vitesse (`PID_VELOCITY_ACTUAL`, registre `0x6A`, exprimé en RPM entiers lorsque `VELOCITY_SELECTION` `0x50` pointe sur `PhiM_*`).
  * **Subtilité DSP** : Le pas de quantification de `0x6A` ($1\text{ RPM} = 6^\circ/\text{s}$) étant trop grossier à très basse vitesse pour la friction $\tanh$ et l'amortissement fin autour du centre, il est préférable d'alimenter `KinematicPLL` à $1\text{ kHz}$ directement avec la position 32 bits `0x6B` déjà lue (zéro transaction SPI additionnelle, résolution $0.0055^\circ$), et de réserver `0x6A` à la supervision ou au mode Hall basse résolution (avec mesure temporelle PWM `mode=1` dans `0x50`).

---

#### 2.4. Analyse 4 (Perspective Sceptique Avancée : Crash-Test & Sécurité Numérique)

Cette analyse passe au crible les failles de sécurité, les risques de saturation et les cas de panne silencieuse.

##### Cas de Panne 1 : Perte de Trames SPI / Erreurs CRC (Glitchs Électromagnétiques)
* Dans un simulateur direct drive, la commutation PWM des MOSFETs à 25 kHz sous 48V / 30A génère d'intenses perturbations CEM sur les nappes de l'encodeur SPI/BiSS-C.
* Si le filtre de Kalman continue de prédire à l'aveugle pendant plusieurs trames corrompues successives, la covariance $P$ croît exponentiellement. Dès qu'une trame valide réapparaît, le gain de Kalman $K$ est devenu maximal et l'innovation est gigantesque : le moteur subit un "coup de bélier" de couple capable d'arracher le volant des mains du pilote ou de briser un mécanisme.
* **Garde-fou Obligatoire** :
  1. **Validation CRC stricte avant tout appel d'estimation** : Si le CRC BiSS-C échoue, la mesure est rejetée.
  2. **Gating de Mahalanobis / Chi-carré** : Si $|z - \theta_{\text{pred}}| > \Delta \theta_{\text{max\_physique}}$, la trame est rejetée.
  3. **Mode Dégradé (Coast down)** : Si plus de 3 trames consécutives sont rejetées, l'estimateur gèle son accélération, applique un amortissement logiciel sur la vitesse estimée ($\omega \leftarrow 0.95 \cdot \omega$) et passe en défaut sécurisé.

##### Cas de Panne 2 : Déphasage Entre Voies de Traitement et Effets Conditionnels
* Actuellement dans `Axis.cpp` :
  ```cpp
  float speedFiltered = (metric.speed) * (float)damperIntensity * AXIS_DAMPER_RATIO;
  mechanicalEffectTorque -= damperFilter.process(clip<float, int32_t>(speedFiltered, -internalFxClip, internalFxClip));
  ```
* Si la vitesse `metric.speed` est déjà issue d'un estimateur (Kalman ou PLL), l'application du filtre `damperFilter` (Biquad à 60 Hz) réinjecte à nouveau un déphasage de $35^\circ$ !
* **Règle absolue** : Les métriques de vitesse et d'accélération fournies par l'architecture d'encodeur doivent être **directement exploitables sans aucun filtre Biquad supplémentaire sur les effets**.
* Le Biquad sur le Damper et l'Inertie doit être **complètement supprimé**. Le couple d'amortissement doit être simplement :
  $$T_{\text{damper}} = -B \cdot \omega_{\text{estimateur}}$$

##### Cas de Panne 3 : Saturation Physique et Clamping Anti-Emballement
* Aucun estimateur d'état ne doit avoir le pouvoir d'envoyer une valeur arbitraire aux étages de commande.
* Des pinces de sécurité matérielles (Clamping saturant) doivent être codées en dur avec assertion :
  $$\omega_{\text{clamped}} = \text{clip}\left(\omega, -\omega_{\text{limit}}, \omega_{\text{limit}}\right) \quad \text{avec } \omega_{\text{limit}} = 70.0\text{ rad/s } (\approx 668\text{ rpm})$$
  $$\alpha_{\text{clamped}} = \text{clip}\left(\alpha, -\alpha_{\text{limit}}, \alpha_{\text{limit}}\right) \quad \text{avec } \alpha_{\text{limit}} = 2000.0\text{ rad/s}^2$$
* Si le filtre dépasse ces bornes, cela déclenche une erreur logicielle (`ERROR_ESTIMATOR_OVERFLOW`) avec désactivation immédiate du couple (`drv->stopMotor()`).

---

### 3. Évaluation Synthétique et Arbitrage Architectural

#### 3.1. Tableau Comparatif des Solutions Étudiées

| Solution DSP | Précision / Bruit | Retard de Phase | Coût CPU (Cortex-M4) | Robustesse / Stabilité | Complexité d'Implémentation | Recommandation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Euler + Biquad 40 Hz (Ancien code)** | Très bruité si $f_c > 60\text{Hz}$ | Catastrophique ($\sim 6.25\text{ ms}$) | Négligeable ($\sim 15$ cyc) | Stable mais rend les effets instables | Très faible | **À ÉLIMINER** |
| **Kalman 3 états CWNA (Code actuel)** | Bonne théorique | Mauvais lors des chocs mécaniques | Élevé ($\sim 180$ cyc) | **Dangereux (Divergence avérée)** | Élevée | **REJETÉ** |
| **Kalman 2 états ($\theta, \omega$) Stationnaire** | Excellente | Très faible ($< 0.5\text{ ms}$) | Très faible ($\sim 16$ cyc) | Inconditionnellement stable | Moyenne | **EXCELLENT CANDIDAT** |
| **Tracking Loop (PLL Industrielle 2nd ordre)** | Excellente (Zéro bruit statique) | Nul à vitesse constante ($< 0.3\text{ ms}$) | Minimal ($\sim 12$ cyc) | Parfaite (linéaire, pôles réels) | Faible | **MEILLEUR CHOIX GLOBAL** |
| **Savitzky-Golay Causal ($N=5$ à 10 kHz)** | Excellente sur 22 bits | Fixe et minimal ($0.2\text{ ms}$) | Ultra-léger ($\sim 8$ cyc) | Parfaite | Très faible | **Idéal BiSS-C 22b** |
| **T-Method asynchrone par interruption** | Haute à basse vitesse | Retard de demi-période variable | Inadmissible à 600 rpm ($>100\text{ kIRQ/s}$) | **Déstabilise l'OS temps réel** | Élevée | **REJETÉ (Danger CPU)** |
| **PLL 1 kHz sur pos 32b `0x6B` (+ `0x6A` Hall)** | Excellente ($0.0055^\circ$ / pas) | Zéro transaction SPI en plus | Minimal ($\sim 15$ cyc @ 1 kHz) | Maximale | Faible | **Optimal (Enc. Internes TMC)** |

---

#### 3.2. Le TOP 3 des Meilleures Idées DSP pour OpenFFBoard

##### N°1 : La Tracking Loop Numérique (PLL Cinématique du 2nd Ordre)
* **Pourquoi elle surpasse le Kalman 3 états** :
  * C'est la référence absolue dans les servodrives industriels à haute performance.
  * Elle ne modélise pas d'accélération autonome instable. Elle force l'état interne à poursuivre la position physique avec une fonction de transfert passe-bas du 2nd ordre sans dépassement :
    $$H(s) = \frac{\Omega_{\text{est}}(s)}{\Omega_{\text{real}}(s)} = \frac{2 \zeta \omega_n s + \omega_n^2}{s^2 + 2 \zeta \omega_n s + \omega_n^2}$$
  * En choisissant un facteur d'amortissement critique $\zeta = 1.0$ (aucun overshoot) et une pulsation propre $\omega_n = 2\pi \times 120\text{ rad/s}$ ($f_n = 120\text{ Hz}$), le retard de phase à 20 Hz est inférieur à $1.8\text{ ms}$, tout en atténuant le bruit haute fréquence de $40\text{ dB/décade}$.
  * Aucune matrice, aucune racine carrée, aucune inversion. 6 opérations arithmétiques.

##### N°2 : Suppression Intégrale des Biquads Secondaires et Linéarisation des Effets
* Les Biquads `damperFilter`, `frictionFilter`, `inertiaFilter` de la classe `Axis` doivent être supprimés.
* La vitesse issue de l'estimateur de l'encodeur est directement injectée dans le calcul de force.
* La friction utilise une régularisation continue :
  $$T_{\text{friction}} = -F_{\text{intensity}} \cdot \tanh\left(\frac{\omega}{0.05\text{ rad/s}}\right)$$
  ce qui éradique les oscillations autour du zéro de vitesse sans aucun déphasage de groupe.

##### N°3 : Découplage de la Commutation Moteur (BiSS-C Extrapolé direct) vs Calcul Haptique
* L'injection de phase dans le TMC4671 (`ExternalEncoderAdapter`) doit utiliser une extrapolation linéaire propre basée sur la vitesse estimée par la PLL :
  $$\phi_{E,\text{extrapolé}} = \theta_{\text{pll}} + \omega_{\text{pll}} \cdot t_{\text{latence\_spi}}$$
  avec $t_{\text{latence\_spi}}$ rigoureusement mesuré ($45\text{ }\mu\text{s}$ pour un transfert SPI à 2.6 MHz).
* La boucle FFB à 1 kHz lit simplement le dernier état stable de la PLL sans calculs complexes.

---

### 4. Spécifications Techniques et Blueprint d'Implémentation C++

Cette section fournit le code et le modèle mathématique de l'architecture cible recommandée : **La Tracking Loop Numérique (PLL Cinématique du 2nd ordre)** avec garde-fous stricts, conçue pour remplacer `KinematicKalman`.

#### 4.1. Modèle Mathématique Multi-Tours de la Tracking Loop (PLL) + Accélération Découplée
Dans OpenFFBoard, `Encoder::getPos_f()` fournit une position continue multi-tours en **rotations** ($\text{rot}$), `getSpeed()` attend des **$\text{rot/s}$**, et `getAccel()` attend des **$\text{rot/s}^2$**. Travailler directement en rotations continues sans *wrapping* sur $[-\pi, \pi]$ élimine tout risque de saut topologique aux demi-tours et simplifie les conversions d'unités.

Pour un pas d'échantillonnage discret $\Delta t$, l'observateur cinématique (PLL Type-II à amortissement critique $\zeta = 1.0$) s'écrit en rotations continues :
$$\begin{aligned}
\epsilon[k] &= \theta_{\text{mes}}[k] - \theta_{\text{est}}[k-1] \\
v_{\text{int}}[k] &= \text{clip}\left(v_{\text{int}}[k-1] + K_i \cdot \epsilon[k] \cdot \Delta t, \; -V_{\max}, \; +V_{\max}\right) \\
v_{\text{est}}[k] &= \text{clip}\left(K_p \cdot \epsilon[k] + v_{\text{int}}[k], \; -V_{\max}, \; +V_{\max}\right) \\
\theta_{\text{est}}[k] &= \theta_{\text{est}}[k-1] + v_{\text{est}}[k] \cdot \Delta t
\end{aligned}$$

Avec les gains continus accordés selon la résolution CPR de l'encodeur :
$$K_p = 2 \zeta \omega_n, \quad K_i = \omega_n^2 \quad \text{où } \omega_n = 2\pi f_n, \; \zeta = 1.0$$
* **Encodeurs haute résolution ($\text{CPR} \ge 65\,536$ à $10\text{ kHz}$, ex. BiSS-C 22b, MT6835)** : $f_n = 120\text{ à } 150\text{ Hz}$ ($K_p \approx 1508\text{ s}^{-1}$, $K_i \approx 568\,489\text{ s}^{-2}$ pour $120\text{ Hz}$).
* **Encodeurs moyenne/basse résolution ($\text{CPR} < 16\,384$ ou cadence $1\text{ kHz}$)** : $f_n = 60\text{ à } 80\text{ Hz}$ pour lisser les marches d'escalier de quantification sans bruit audible.

**Estimation de l'accélération découplée (hors boucle de position) :**
Pour alimenter `Axis::calculateMechanicalEffects()` (Inertie), `EffectsCalculator` (`FFB_EFFECT_INERTIA`) et `Axis::updateHandsOffState()` (`accel_buffer`) sans jamais réinjecter d'instabilité dans $(\theta_{\text{est}}, v_{\text{est}})$, l'accélération $a_{\text{est}}$ ($\text{rot/s}^2$) est extraite en aval par un filtre passe-bas RC du 1er ordre ($f_{c,a} \approx 60\text{ Hz}$) sur la variation de $v_{\text{est}}$ :
$$a_{\text{raw}}[k] = \frac{v_{\text{est}}[k] - v_{\text{prev}}[k]}{\Delta t}, \quad \alpha_{\text{rc}} = \frac{\Delta t}{\tau_a + \Delta t} \;\; \left(\tau_a = \frac{1}{2\pi f_{c,a}}\right)$$
$$a_{\text{est}}[k] = \text{clip}\left(a_{\text{est}}[k-1] + \alpha_{\text{rc}} \left(a_{\text{raw}}[k] - a_{\text{est}}[k-1]\right), \; -A_{\max}, \; +A_{\max}\right)$$
avec $V_{\max} = 11.14\text{ rot/s}$ ($\approx 70\text{ rad/s} \approx 668\text{ rpm}$) et $A_{\max} = 318.3\text{ rot/s}^2$ ($\approx 2000\text{ rad/s}^2$).

#### 4.2. Blueprint C++ Corrigé : `KinematicPLL.h`

```cpp
#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

/**
 * @brief Industrial 2nd-order Tracking Loop (Type-II PLL) in continuous rotations.
 * Provides unconditionally stable multi-turn position (rot), velocity (rot/s),
 * and decoupled 1st-order RC-filtered acceleration (rot/s^2).
 */
class KinematicPLL {
public:
    static constexpr float PI_F      = 3.14159265f;
    static constexpr float MAX_RPS   = 11.14f;  // ~70 rad/s (~668 RPM)
    static constexpr float MAX_ACCEL = 318.3f;  // ~2000 rad/s^2

    KinematicPLL() {
        setTuning(120.0f, 1.0f, 60.0f);
        reset(0.0f);
    }

    void reset(float initial_pos_rot = 0.0f) {
        pos_est = initial_pos_rot;
        vel_est = 0.0f;
        vel_prev = 0.0f;
        integrator = 0.0f;
        accel_est = 0.0f;
        initialized = false;
    }

    /**
     * @brief Tune PLL natural frequency and decoupled acceleration filter.
     * @param fn_hz Natural frequency (e.g. 120-150 Hz for >=16-bit @10kHz, 60-80 Hz for low CPR).
     * @param damping Damping ratio (1.0f = critical damping, zero overshoot).
     * @param accel_fc_hz 1st-order LPF cutoff for acceleration output (typ. 60 Hz).
     */
    void setTuning(float fn_hz, float damping = 1.0f, float accel_fc_hz = 60.0f) {
        float wn = 2.0f * PI_F * fn_hz;
        kp = 2.0f * damping * wn;
        ki = wn * wn;
        tau_accel = 1.0f / (2.0f * PI_F * std::max(accel_fc_hz, 1.0f));
    }

    /**
     * @brief Update the PLL with a new continuous multi-turn position measurement.
     * @param z_pos_rot Measured multi-turn angle in rotations (continuous, unwrapped).
     * @param dt Time step in seconds (e.g. 0.0001f at 10 kHz, 0.001f at 1 kHz).
     */
    void update(float z_pos_rot, float dt) {
        if (!initialized) {
            pos_est = z_pos_rot;
            vel_est = 0.0f;
            vel_prev = 0.0f;
            integrator = 0.0f;
            accel_est = 0.0f;
            initialized = true;
            return;
        }

        if (dt <= 1e-6f || dt > 0.05f) {
            // Reject zero/negative or stalled RTOS timesteps; re-anchor safely if needed
            pos_est = z_pos_rot;
            vel_est = 0.0f;
            vel_prev = 0.0f;
            integrator = 0.0f;
            accel_est = 0.0f;
            return;
        }

        // 1. Continuous multi-turn tracking error (handle explicit setPos() jumps cleanly)
        float error = z_pos_rot - pos_est;

        // 2. Time-proportional Outlier Gating (3x physical max displacement over dt)
        float max_step_rot = std::max(MAX_RPS * dt * 3.0f, 0.004f);
        if (std::fabs(error) > max_step_rot) {
            // Distinguish between user re-centering (large persistent step > 0.25 rot) and EMI glitch
            if (++glitch_count > 3) {
                // Re-anchor on legitimate setPos() / persistent jump without velocity spike
                pos_est = z_pos_rot;
                vel_prev = vel_est;
                glitch_count = 0;
                return;
            }
            // Transient glitch: coast down safely without integrating spike
            vel_est *= 0.95f;
            pos_est += vel_est * dt;
            accel_est *= 0.90f;
            return;
        }
        glitch_count = 0;

        // 3. Type-II PI Velocity Observer with anti-windup clamping
        integrator = std::clamp(integrator + ki * error * dt, -MAX_RPS, MAX_RPS);
        vel_prev = vel_est;
        vel_est = std::clamp(kp * error + integrator, -MAX_RPS, MAX_RPS);

        // 4. Closed-loop position integration
        pos_est += vel_est * dt;

        // 5. Decoupled 1st-order RC acceleration estimator (never fed back into pos/vel)
        float accel_raw = (vel_est - vel_prev) / dt;
        float alpha_rc = dt / (tau_accel + dt);
        accel_est = std::clamp(accel_est + alpha_rc * (accel_raw - accel_est), -MAX_ACCEL, MAX_ACCEL);
    }

    float getPos() const   { return pos_est; }   // rotations
    float getSpeed() const { return vel_est; }   // rot/s
    float getAccel() const { return accel_est; } // rot/s^2

private:
    float pos_est = 0.0f;
    float vel_est = 0.0f;
    float vel_prev = 0.0f;
    float integrator = 0.0f;
    float accel_est = 0.0f;

    float kp = 1507.96f;
    float ki = 568489.0f;
    float tau_accel = 0.00265258f; // ~60 Hz
    uint8_t glitch_count = 0;
    bool initialized = false;
};
```

---

### 5. Audit d'Intégration Codebase : Les 7 Points d'Impact & Cas Critique des Encodeurs Intégrés (`TMC4671` / CAN)

Une inspection exhaustive du code de la branche `OpenFFBoard-refactor-encoder` a mis en évidence **7 points d'impact critiques** sur la chaîne d'encodeurs externes/effets, ainsi qu'une **faille majeure affectant tous les encodeurs intégrés aux drivers moteurs** (`TMC4671`, `ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `MotorSimplemotion`).

#### 5.1. Les 7 Points d'Impact sur la Chaîne Encodeurs & Effets

1. **Bug d'alignement FOC sur `setPos(0)` (`ExternalEncoderAdapter.cpp` l.17 & `TMC4671.cpp` l.3169-3172)** :
   * `ExternalEncoderAdapter::update()` lit actuellement `encoder->getPos_f()`, qui inclut l'offset de centrage utilisateur (`posOffset`). Dès que l'utilisateur recentre son volant (`setPos(0)`), l'angle mécanique relatif change alors que le rotor n'a pas bougé : l'angle électrique $\phi_E$ injecté dans le `TMC4671` subit un déphasage brutal vis-à-vis de `externalEncoderPhieOffset`, provoquant perte de couple ou emballement FOC !
   * **Correction impérative** : Utiliser `encoder->getPosAbs_f()` (position absolue sans offset utilisateur) dans `ExternalEncoderAdapter::update()` et décommenter/sécuriser l'écriture DMA `writeRegAsync(0x1C, phiE_final)` dans `TMC4671::setExternalPhiE()`.
2. **Continuité multi-tours, initialisation et Gating dynamique dans `KinematicPLL`** :
   * Suppression du *wrapping* modulo $[-\pi, \pi]$ (incompatible avec `getPos_f()` multi-tours), suppression de `prev_error` inutilisé, ancrage immédiat `pos_est = z_pos_rot` au premier échantillon (`!initialized`) pour éviter un transitoire au démarrage, et seuil d'*outlier gating* proportionnel à $\Delta t$ (`MAX_RPS * dt * 3.0f`) afin de supporter aussi bien $10\text{ kHz}$ que $1\text{ kHz}$ ou un léger jitter RTOS.
3. **Maintien impératif de `Encoder::getAccel()` (découplé)** :
   * `getAccel()` est consommé en trois endroits critiques : `Axis::calculateMechanicalEffects()` (effet d'inertie axe), `EffectsCalculator::calculateEffectForceOnAxis()` (`FFB_EFFECT_INERTIA` DirectInput), et `Axis::updateHandsOffState()` (variance de `accel_buffer` pour la détection *Hands-Off*). L'accélération doit donc continuer d'être exposée par `Encoder::getAccel()`, via la dérivée filtrée RC 1er ordre ($\sim 60\text{ Hz}$) de `vel_est` (cf. §4.1), sans aucune rétroaction sur la position.
4. **Double étage de filtres Biquad (`Axis.cpp` ET `EffectsCalculator.cpp`)** :
   * Outre `damperFilter`, `frictionFilter` et `inertiaFilter` dans `Axis.cpp`, la classe `EffectsCalculator.cpp` applique un **second étage** de filtres Biquad sur les effets conditionnels DirectInput du jeu. Il faut neutraliser/adapter ces filtres redondants dans les deux classes tout en **conservant intactes les commandes HID/CDC** associées pour ne pas casser la compatibilité protocolaire avec *OpenFFBoard Configurator*.
5. **Conversion exacte d'unité de `PID_VELOCITY_ACTUAL` (`0x6A`) sur `TMC4671`** :
   * Dans le matériel `TMC4671`, l'unité du registre `0x6A` dépend directement du registre `0x50` (`VELOCITY_SELECTION`, méthode `TMC4671::setVelSel()`) :
     * Si la source sélectionnée est mécanique (`VelSelection::PhiM_abn = 9`, `PhiM_abn2 = 10`, `PhiM_aenc = 11`, `PhiM_hal = 12` — ce que configure effectivement `TMC4671::encoderInit()` aux lignes 1435, 1443 et 1559), le `TMC4671` exprime directement `0x6A` en **RPM mécaniques signés (`int32_t`)**. La conversion vers `Encoder::getSpeed()` ($\text{rot/s}$) est alors exactement :
       $$v_{\text{rot/s}} = \frac{\text{vel}_{\text{raw}}}{60.0\text{f}}$$
     * Si une source électrique `PhiE_*` (`0..7`) était sélectionnée dans `0x50`, `0x6A` exprimerait des **RPM électriques**, nécessitant une division supplémentaire par `conf.motconf.pole_pairs` ($v_{\text{rot/s}} = \frac{\text{vel}_{\text{raw}}}{60.0\text{f} \cdot N_{\text{pp}}}$). Et si `conf.invertForce && conf.combineEncoder` ou une inversion matérielle s'applique, la cohérence de signe avec `getPos_f()` doit être garantie.
   * Cette lecture SPI doit s'effectuer **à $1\text{ kHz}$** (cadence de `Axis`), jamais par polling SPI bloquant à $10\text{ kHz}$ dans `EncoderManager` afin de ne pas saturer le bus `motor_spi` partagé avec `setFluxTorque()`.
6. **Conflit SPI concurrent dans `EncoderBissC::getPosAbs()` (`EncoderBissC.cpp` l.190-202)** :
   * Actuellement, `EncoderBissC::getPosAbs()` déclenche encore manuellement `triggerRead()` alors qu'`EncoderManager` cadence déjà `triggerRead()` à $10\text{ kHz}$ par DMA. Tout appel externe à `getPos()`/`getPosAbs()` depuis `Axis` (à $1\text{ kHz}$) entre en collision sur le sémaphore SPI et fausse `dt`. `getPosAbs()` doit devenir **purement passif** (retourner la dernière position décodée valide `curPos` en mémoire). (Même vigilance sur `EncoderTW39` s'il est raccordé à `EncoderManager`).
7. **Adaptation automatique de $f_n$ selon le `CPR`** :
   * Dans `Encoder::updateState()`, lors de la détection `current_cpr != last_cpr`, adapter automatiquement la bande passante de `KinematicPLL` : $f_n = 130\text{ Hz}$ si `current_cpr >= 65536`, $f_n = 90\text{ Hz}$ si `16384 <= current_cpr < 65536`, et $f_n = 60\text{ Hz}$ si `current_cpr < 16384`.

---

#### 5.2. Diagnostic Critique : Régression des Encodeurs Intégrés (`TMC4671`, `ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `SimpleMotion`)

L'analyse croisée de `Axis.cpp`, `Encoder.cpp` et `TMC4671.cpp` révèle une **rupture fonctionnelle majeure** introduite lors du remplacement du calcul de vitesse/accélération d'`Axis` par `encoder->getSpeed()` / `encoder->getAccel()` :

##### A. Pourquoi tous les encodeurs intégrés au `TMC4671` (ABN, SinCos, UVW, Hall) renvoient aujourd'hui `speed = 0` et `accel = 0`
1. `TMC4671` hérite de `MotorDriver` **et** de `Encoder` (`class TMC4671 : public MotorDriver, public Encoder, ...`).
2. Lorsque l'encodeur est branché sur le `TMC4671` (`conf.motconf.enctype != EncoderType_TMC::ext && !conf.combineEncoder`), `TMC4671::hasIntegratedEncoder()` (l.1872) retourne `true` et `TMC4671::getEncoder()` (l.1848) retourne `static_cast<Encoder*>(this)`.
3. Or, dans `Axis::setEncType()` (l.539-550) :
   ```cpp
   if (encoderChooser.isValidClassId(enctype) && !drv->hasIntegratedEncoder()) {
       ...
       EncoderManager::getInstance().registerEncoder(this->enc.get());
   }
   ```
   Quand `drv->hasIntegratedEncoder()` vaut `true`, **aucun encodeur n'est enregistré dans `EncoderManager`**.
4. Dans le même temps, `TMC4671` ne surcharge **ni** `getSpeed()`, **ni** `getAccel()`, et n'appelle **jamais** `Encoder::updateState()` !
5. **Conséquence actuelle** : `Axis::updateMetrics()` (l.754-755) appelle `encoder->getSpeed()` et `encoder->getAccel()` sur l'instance `TMC4671`, qui lit un filtre non alimenté et retourne **`0.0f` en permanence**. Sur toute configuration TMC4671 avec encodeur intégré (ABN quadrature, SinCos analogique, UVW, Hall), les effets Damper, Friction, Inertia (axe et DirectInput) ainsi que la détection *Hands-Off* sont totalement inopérants !

##### B. Trois Subtilités Spécifiques au `TMC4671` à Traiter Simultanément
1. **Résolution de `0x6A` (`PID_VELOCITY_ACTUAL`) à basse vitesse vs PLL sur position `0x6B` (`PID_POSITION_ACTUAL`)** :
   * Le registre matériel `0x6A` du TMC4671 en mode `VelSelection::PhiM_*` retourne un **entier signé en RPM** (`1 LSB = 1 RPM = 0.01667 rot/s = 6 deg/s`).
   * **Attention DSP critique** : Une quantification de $1\text{ RPM}$ ($6^\circ/\text{s}$) sur `0x6A` est **beaucoup trop grossière** pour les effets FFB de friction et d'amortissement autour du point milieu (où le pilote déplace le volant à $0.5^\circ/\text{s}$ — le registre `0x6A` reste bloqué à `0` puis saute brutalement par paliers de $6^\circ/\text{s}$, créant un effet d'escalier sur le Damper et rendant la friction $\tanh(\omega/\omega_0)$ aveugle sous $1\text{ RPM}$) !
   * À l'inverse, le registre `0x6B` (`PID_POSITION_ACTUAL`, lu par `TMC4671::getPos()` à $1\text{ kHz}$ depuis `Axis::getEncAngle()`) est un compteur continu **32 bits multi-tours** dont `1 tour = 65536 counts` (`getCpr() == 0xFFFF`, résolution de $0.0055^\circ$ !).
   * **Architecture optimale pour `TMC4671` en encodeur intégré** :
     * Ne **pas** enregistrer `TMC4671` dans `EncoderManager` à $10\text{ kHz}$ (`readReg(0x6B)` prend un sémaphore SPI bloquant dans une tâche et ne peut être appelé ni depuis une ISR ni à $10\text{ kHz}$ sans congestionner `motor_spi`).
     * Mettre à jour `KinematicPLL` **à $1\text{ kHz}$ directement lors de la lecture existante `TMC4671::getPos()`** (ou via un hook `Axis::updateMetrics()` $\rightarrow$ `encoder->updateAtAxisRate()`), en réutilisant la valeur `pos` déjà lue par `readReg(0x6B)` (**zéro transaction SPI supplémentaire !**). À $1\text{ kHz}$ sur $65\,536\text{ counts/tour}$ avec $f_n = 80\text{ Hz}$, la résolution de vitesse est sub-millidegré/s sans marches de $1\text{ RPM}$, et `getAccel()` est calculé gratuitement et proprement par le même `KinematicPLL` !
     * Optionnellement, `PID_VELOCITY_ACTUAL` (`0x6A`) converti en $\text{rot/s}$ peut servir de *sanity-check* / supervision ou en mode Hall très basse résolution (`EncoderType_TMC::hall`, seulement 6 états électriques par paire de pôles), où le mode `mode = 1` (*PWM sync time difference measurement* dans `setVelSel(vsel, 1)`, registre `0x50` octet haut) offre une meilleure mesure temporelle matérielle.
2. **Bascule dynamique `encsrc` / `combineEncoder` via USB CDC (`TMC4671.cpp` l.2813 & l.2859)** :
   * Lorsque l'utilisateur modifie `encsrc` (ex. passage de `ABN` à `External=5`) ou active `combineEncoder` dans le Configurator, `TMC4671::hasIntegratedEncoder()` change dynamiquement d'état (`true` $\leftrightarrow$ `false`), sans que `Axis::setEncType()` ne soit réexécuté automatiquement.
   * Il faut s'assurer que :
     * En mode `combineEncoder == true` (commutation FOC par l'encodeur interne du TMC, position FFB lue sur l'encodeur externe `drvEncoder`), `drvEncoder` soit bien instancié et enregistré dans `EncoderManager` afin que sa `KinematicPLL` tourne, **sans** que `TMC4671` n'active `ExternalEncoderAdapter` (puisque `enctype != ext`, le TMC utilise son propre $\phi_E$ interne, ce qui est déjà cohérent avec `setEncoderType` l.1523).
     * Lors d'une transition `encsrc` vers `External`, si `Axis` possédait un `conf.enctype` valide non encore créé (car bloqué précédemment par `hasIntegratedEncoder()`), l'encodeur externe soit correctement attaché ou réinitialisé proprement.
3. **Même faille sur les autres `MotorDriver` à encodeur intégré (`ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `MotorSimplemotion`)** :
   * Tous ces drivers retournent `hasIntegratedEncoder() == true` (ou `useEncoder` sur `VescCAN`) et ne sont donc **pas** dans `EncoderManager` : aujourd'hui, leurs `getSpeed()` et `getAccel()` renvoient également **`0.0f`** !
   * **Correction unifiée** :
     * **`ODriveCAN`** : Reçoit déjà `lastPos` ($\text{rot}$) et `lastSpeed` ($\text{rot/s}$) par trame CAN cyclique `0x09` (`canRxPendCallback` l.415-425). Il suffit d'appeler `updateState(lastPos - posOffset, dt)` à la réception CAN (ou de surcharger `getSpeed()` avec `lastSpeed` et calculer `accel` par le filtre RC 1er ordre).
     * **`VescCAN` / `RmdMotorCAN` / `MotorSimplemotion`** : Appeler `updateState(getPos_f(), dt)` à chaque réception d'une nouvelle trame de position (CAN / UART SimpleMotion) avec $f_n = 50 - 60\text{ Hz}$ adapté à la cadence bus ($\sim 500\text{ Hz} - 1\text{ kHz}$).

---

### 6. Synthèse des Recommandations Stratégiques et Plan d'Action Consolidé

#### Ce qu'il faut CONSERVER et VALIDER :
1. **La centralisation dans `EncoderManager`** pour tous les encodeurs matériels externes (SPI DMA, BiSS-C, MT6835, SSI, Timer Quadrature `EncoderLocal` en M-Method).
2. **Le découplage de `MotorDriver`** et l'extrapolation linéaire de phase $\phi_E$ sur la latence SPI ($\Delta t_{\text{spi}} \approx 45-50\text{ }\mu\text{s}$), **corrigée avec `getPosAbs_f()`**.
3. **Le rejet définitif des filtres de Kalman 3 états et 2 états** au profit de `KinematicPLL` (voir également la démonstration dédiée dans [Partie 4](#partie-4)).

#### Ce qu'il faut ABANDONNER IRRÉVOCABLEMENT :
1. **Le filtre de Kalman d'ordre 3 (`KinematicKalman`)** : Divergent, sensible au jitter FreeRTOS et buggé dans sa mise à jour de Joseph.
2. **La T-Method par interruptions logicielles EXTI (`EncoderLocal.cpp`)** : Risque critique de famine CPU ($>100\text{ kIRQ/s}$ à $600\text{ rpm}$).
3. **La double cascade de Biquads secondaires sur les effets conditionnels** (`Axis.cpp` et `EffectsCalculator.cpp`).
4. **Les déclenchements SPI concurrents dans `EncoderBissC::getPosAbs()`**.

#### Feuille de Route d'Implémentation (Ordre d'Exécution Sécurisé) :
1. **Créer `KinematicPLL` et remplacer `KinematicKalman` dans `Encoder`** :
   * Implémenter la PLL multi-tours en $\text{rot}$, $\text{rot/s}$ avec filtre d'accélération RC 1er ordre découplé ($\text{rot/s}^2$), clamps physiques durs ($11.14\text{ rot/s}$, $318.3\text{ rot/s}^2$), gating proportionnel à $\Delta t$, et adaptation automatique de $f_n$ selon `getCpr()`.
2. **Corriger le bug critique FOC externe (`ExternalEncoderAdapter.cpp` & `TMC4671.cpp`)** :
   * Remplacer `encoder->getPos_f()` par `encoder->getPosAbs_f()` dans `ExternalEncoderAdapter::update()`.
   * Réactiver proprement `writeRegAsync(0x1C, phiE_final)` dans `TMC4671::setExternalPhiE()`.
3. **Restaurer `getSpeed()` et `getAccel()` sur les encodeurs intégrés (`TMC4671` & drivers CAN/SimpleMotion)** :
   * Dans `TMC4671` (mode encodeur intégré ABN/SinCos/UVW/Hall) : alimenter `KinematicPLL` à $1\text{ kHz}$ à partir de la lecture 32 bits `0x6B` déjà effectuée dans le cycle `Axis` (zéro transfert SPI additionnel, résolution $1/65536\text{ tour}$ bien supérieure au pas grossier de $1\text{ RPM}$ du registre `0x6A`), avec $f_n = 80\text{ Hz}$ (et support du mode temporel / `0x6A` converti selon `VelSelection` `0x50` pour le cas Hall).
   * Gérer proprement les transitions dynamiques `encsrc` / `combineEncoder` vis-à-vis d'`Axis` et d'`EncoderManager`.
   * Brancher `updateState()` sur réception des trames de position dans `ODriveCAN` (avec exploitation directe possible de `lastSpeed` CAN `0x09`), `VescCAN`, `RmdMotorCAN` et `MotorSimplemotion`.
4. **Assainir `EncoderBissC.cpp` et `EncoderLocal.cpp`** :
   * Rendre `EncoderBissC::getPosAbs()` strictement passif (suppression de l'appel concurrent à `triggerRead()`) et vérifier le CRC-6 avant `updateState()`.
   * Désactiver les interruptions EXTI de période dans `EncoderLocal.cpp` au profit de la lecture matérielle `TIM->CNT` synchrone à $10\text{ kHz}$.
5. **Nettoyer les étages d'effets (`Axis.cpp` & `EffectsCalculator.cpp`)** :
   * Bypasser les Biquads redondants sur Damper, Friction et Inertia dans `Axis.cpp` et `EffectsCalculator.cpp` (tout en préservant les commandes HID/CDC pour le Configurator), et appliquer la loi continue $\tanh(\omega / \omega_0)$ sur la friction.


---

<a id="partie-4"></a>

# Partie 4 — Pourquoi le Kalman 2 états n'a pas été retenu

*Source d'origine : `doc/pourquoi_pas_kalman_2_etats.md`*

## Analyse Approfondie : Pourquoi le Filtre de Kalman à 2 États n'a pas été Retenu

Ce document expose de manière rigoureuse les raisons mathématiques, fréquentielles, temps-réel et ergonomiques qui justifient l'éviction du **Filtre de Kalman à 2 états ($x = [\theta, \omega]^T$)** au profit d'une **Tracking Loop numérique (PLL cinématique de Type-II du 2nd ordre)** pour l'architecture d'encodeurs d'`OpenFFBoard-refactor-encoder`.

---

### 1. Ce que le Kalman 2 États Corrigeait par Rapport au Kalman 3 États

Dans `KinematicKalman.cpp`, le modèle initial à 3 états ($[\theta, \omega, \alpha]^T$) intégrait librement une accélération non amortie ($\dot{\omega} = \alpha$), provoquant une double intégration divergente de l'erreur de position et un bug critique dans la mise à jour de Joseph de `updateVelocity()`.

Le passage à un vecteur d'état réduit à 2 dimensions :
$$x_k = \begin{bmatrix} \theta_k \\ \omega_k \end{bmatrix}, \quad F(\Delta t) = \begin{bmatrix} 1 & \Delta t \\ 0 & 1 \end{bmatrix}, \quad H = \begin{bmatrix} 1 & 0 \end{bmatrix}$$
permettait effectivement d'éliminer l'emballement exponentiel dû à l'état $\alpha$. C'est pourquoi il apparaissait en première lecture comme un candidat viable. Toutefois, son analyse approfondie dans le cadre strict d'un volant Direct Drive sous FreeRTOS révèle **six limites structurelles** qui le rendent inférieur ou équivalent (en régime stationnaire) à une PLL, tout en introduisant des vulnérabilités inutiles.

---

### 2. Les 6 Raisons Techniques de son Rejet

#### 2.1. Équivalence Asymptotique exacte avec un Filtre $\alpha\text{-}\beta$ / PLL (Redondance Mathématique)
À cadence d'échantillonnage nominale fixe ($\Delta t = 100\text{ }\mu\text{s}$ à $10\text{ kHz}$) et pour un encodeur donné (variance de quantification $R = \frac{\Delta \theta^2}{12}$ constante), l'équation algébrique de Riccati discrète (DARE) du Kalman 2 états converge en quelques millisecondes vers une matrice de covariance stationnaire $P_\infty$ fixe.

Une fois $P_\infty$ atteinte, les gains de Kalman $K = [K_0, K_1]^T$ deviennent constants :
$$\begin{aligned}
\hat{\theta}_{k|k} &= \hat{\theta}_{k-1|k-1} + \hat{\omega}_{k-1|k-1}\Delta t + K_0 \cdot \epsilon_k \\
\hat{\omega}_{k|k} &= \hat{\omega}_{k-1|k-1} + K_1 \cdot \epsilon_k
\end{aligned}$$
avec l'innovation $\epsilon_k = z_k - (\hat{\theta}_{k-1|k-1} + \hat{\omega}_{k-1|k-1}\Delta t)$.

Or, cette récurrence à gains constants est **mathématiquement isomorphe à une Tracking Loop PI (PLL du 2nd ordre)** en posant $K_0 = 2\zeta\omega_n\Delta t$ et $K_1 = \omega_n^2\Delta t$ :
* **En régime normal**, propager dynamiquement la matrice $P_{2\times 2}$ à $10\text{ kHz}$ recalcule en boucle fermée des gains qui convergent vers ceux de la PLL.
* **En régime perturbé** (jitter OS ou perte de paquets SPI), la variation dynamique de $P$ dégrade le comportement au lieu de l'améliorer (voir points 2.3 et 2.4).

#### 2.2. Erreur de Traînage Dynamique et Paradoxe Bruit / Latence (Modèle à Vitesse Constante)
Le modèle stochastique CWNA d'ordre 2 postule que la vitesse est constante entre deux échantillons et que toute accélération n'est qu'un bruit blanc centré de densité spectrale $q_{\text{vel}}$ :
$$\dot{\theta}(t) = \omega(t), \quad \dot{\omega}(t) = w(t), \quad Q(\Delta t) = q_{\text{vel}} \begin{bmatrix} \frac{\Delta t^3}{3} & \frac{\Delta t^2}{2} \\ \frac{\Delta t^2}{2} & \Delta t \end{bmatrix}$$

Sur un volant Direct Drive (couple de $20\text{ à }30\text{ Nm}$, inertie rotor + volant $J \approx 0.02\text{ kg}\cdot\text{m}^2$), les accélérations réelles atteignent $1000\text{ à }1500\text{ rad/s}^2$ lors d'un contre-braquage ou d'un vibreur :
1. Durant toute rampe d'accélération $\alpha_{\text{réelle}}$, la prédiction $\hat{\omega}_{k|k-1} = \hat{\omega}_{k-1|k-1}$ est structurellement en retard. L'erreur de traînage vaut asymptotiquement $\Delta \omega_{\text{lag}} = \frac{\alpha_{\text{réelle}} \Delta t}{K_1}$.
2. Pour réduire ce retard via le formalisme de Kalman, il faut augmenter fortement $q_{\text{vel}}$. Mais avec le couplage rigide imposé par l'indice de Kalata ($\lambda \propto \sqrt{q_{\text{vel}}/R}$), le facteur d'amortissement équivalent du filtre de Kalman tend vers $\zeta = \frac{\sqrt{2}}{2} \approx 0.707$ (sous-amorti) au lieu de $\zeta = 1.0$.
3. **Conséquence haptique** : Un amortissement $\zeta \approx 0.707$ introduit une surtension fréquentielle (+2 dB de dépassement indiciel d'environ $4.3\%$) sur l'estimation de vitesse. Injectée directement dans l'effet Damper ($T = -B \cdot \hat{\omega}$) sans Biquad secondaire, cette surtension réduit la marge de phase de la boucle mécanique lorsque le pilote lâche le volant. La PLL permet au contraire d'imposer explicitement **$\zeta = 1.0$ (amortissement critique : pôles réels doubles, zéro dépassement)** quelle que soit la bande passante $f_n$ choisie.

#### 2.3. Sensibilité au Jitter Temporel de FreeRTOS (Modulation Parasite des Gains)
Dans `Encoder.cpp` (`Encoder::triggerRead()`), le pas de temps $\Delta t$ est mesuré à la microseconde près via `micros()`, mais l'exécution dépend de l'ordonnanceur FreeRTOS, des sections critiques et des transactions SPI/DMA :
* Si une préemption retarde ponctuellement la tâche de $100\text{ }\mu\text{s}$ à $300\text{ }\mu\text{s}$ ($\times 3$), les termes de $Q(\Delta t)$ augmentent d'un facteur $27$ sur $Q_{00}$ ($\Delta t^3$) et $9$ sur $Q_{01}$ ($\Delta t^2$).
* La matrice $P$ gonfle instantanément, faisant bondir les gains $K_0$ et $K_1$ sur l'échantillon suivant, avant de redescendre sur les cycles ultérieurs.
* Cette **modulation temporelle des gains par le jitter d'ordonnancement** transforme la gigue logicielle en bruit paramétrique injecté dans la commande de courant du `TMC4671` (grésillement audible à haute fréquence dans le stator).
* Une PLL discrétisée avec $\Delta t$ borné conserve des gains proportionnel et intégral $(K_p, K_i)$ invariants face au jitter.

#### 2.4. À-coups de Couple ("Glitch Torque") après Erreurs CRC BiSS-C
En environnement CEM sévère (hacheur PWM 48V / 30A à proximité du câble d'encodeur), des trames `EncoderBissC` peuvent échouer au contrôle CRC-6 :
* **Avec un Kalman 2 états** : Lorsqu'une mesure invalide est rejetée et que seule l'étape `predict(dt)` s'exécute, $P_{k|k-1} = F P F^T + Q$ croît à chaque trame manquée (croissance quadratique de $P_{00}$ via $\Delta t^2 P_{11}$). Dès le retour d'une trame CRC valide, l'innovation accumulée est multipliée par un gain $K_1 = P_{10}/(P_{00}+R)$ anormalement élevé, provoquant un pic brutal sur $\hat{\omega}$ et donc une secousse sèche dans le volant.
* **Avec la PLL** : Aucun état de covariance ne diverge en boucle ouverte. Sur rejet CRC ou dépassement du seuil d'outlier gating, l'intégrateur de vitesse est simplement maintenu avec une décroissance exponentielle douce ($\hat{\omega} \leftarrow 0.98 \cdot \hat{\omega}$), assurant une reprise sans discontinuité de couple.

#### 2.5. Inadéquation Ergonomique et Couplage à la Résolution CPR
OpenFFBoard s'adresse à un parc matériel très hétérogène (ABZ $2\,500\text{ PPR} = 10\,000\text{ CPR}$, SinCos, MT6835 21 bits, BiSS-C 22 bits soit $4\,194\,304\text{ CPR}$) :
* Entre un encodeur $10\,000\text{ CPR}$ et un BiSS-C 22 bits, le pas angulaire varie d'un facteur $419$, donc la variance de bruit de mesure $R = \frac{\Delta \theta^2}{12}$ varie d'un facteur **$175\,000$** !
* Avec un Kalman 2 états réglé par un $q_{\text{vel}}$ unique (en $\text{rad}^2/\text{s}^3$), la fréquence de coupure effective du filtre ($f_c \propto (q_{\text{vel}}/R)^{1/4}$) change d'un facteur $\sim 20$ lorsqu'on change d'encodeur. Un $q_{\text{vel}}$ sain sur un ABZ donnera une bande passante inutilement agressive (> 1 kHz) sur un BiSS-C 22 bits, ou inversement un filtre ultra-lent sur ABZ.
* Exposer $q_{\text{vel}}$ et $R$ dans le configurateur GUI d'OpenFFBoard est incompréhensible pour l'utilisateur final. À l'inverse, la PLL se paramètre directement par une **fréquence propre physique $f_n$ en Hz** (ex: $120\text{ Hz}$ par défaut, réglable entre $50\text{ Hz}$ et $250\text{ Hz}$), indépendante des unités internes de variance, avec $\zeta = 1.0$ verrouillé.

#### 2.6. Surcoût Arithmétique Inutile à 10 kHz sur Cortex-M4F
Sur STM32F4 (FPU simple précision `float32`, sans cache de données L1, cadencé à 168 MHz) :
* **Kalman 2 états (avec forme de Joseph pour préserver la symétrie et la positivité de $P$)** :
  * Puissances $\Delta t^2, \Delta t^3$, propagation $F P F^T + Q$, calcul de $S = P_{00} + R$, **division flottante `VDIV.F32`** (14 cycles à elle seule), calcul de $K_0, K_1$, et mise à jour de Joseph $2 \times 2$.
  * Coût total : **$\approx 45\text{ à }55\text{ cycles FPU}$** par encodeur et par tick à $10\text{ kHz}$, plus un risque de perte de précision numérique en `float32` lorsque $R \approx 1.8 \times 10^{-13}\text{ rad}^2$ (cas du BiSS-C 22 bits où $\Delta\theta = 1.5\times 10^{-6}\text{ rad}$, proche du plancher de cancellation vis-à-vis de $P_{00}$).
* **Tracking Loop (PLL 2nd ordre)** :
  * Normalisation d'erreur angulaire, 2 `VMLA.F32` (PI + clamp anti-windup), 1 intégration d'angle, **zéro division flottante**, zéro matrice.
  * Coût total : **$\approx 12\text{ cycles FPU}$**, insensible au sous-dépassement (`underflow`) flottant sur encodeurs très haute résolution.

---

### 3. Tableau Récapitulatif : Kalman 2 États vs Tracking Loop (PLL)

| Critère Technique | Kalman 2 États ($[\theta, \omega]^T$) | Tracking Loop (PLL 2nd Ordre) | Avantage |
| :--- | :--- | :--- | :--- |
| **Amortissement fréquentiel** | Sous-amorti par défaut ($\zeta \approx 0.707$, overshoot ~4.3%) | Critique imposé ($\zeta = 1.0$, **0% d'overshoot**) | **PLL** |
| **Passivité haptique (Damper direct)** | Risque d'oscillation si $\zeta < 1$ sans filtre aval | Garantie par pôles réels doubles stricts | **PLL** |
| **Sensibilité au jitter de $\Delta t$ (RTOS)** | Forte ($Q$ varie en $\Delta t^3$, gains $K$ modulés) | Minimale ($K_p, K_i$ fixes) | **PLL** |
| **Reprise après erreur CRC BiSS-C** | À-coup mécanique ($P$ gonflée pendant la perte) | Douce (maintien / coast-down sans mémoire de variance) | **PLL** |
| **Précision `float32` sur BiSS-C 22 bits** | Délicate ($R \sim 10^{-13}\text{ rad}^2$, division par $S$) | Parfaite (arithmétique linéaire directe en radians) | **PLL** |
| **Paramétrage utilisateur (GUI)** | Abstrait ($q_{\text{vel}}$ couplé au CPR de l'encodeur) | Physique et universel ($f_n$ en Hz) | **PLL** |
| **Coût CPU à 10 kHz (STM32F4)** | $\sim 50\text{ cycles}$ (inclut `VDIV.F32`) | $\sim 12\text{ cycles}$ (aucune division) | **PLL** |

---

### 4. Conclusion Architecturale

Le filtre de Kalman à 2 états n'est pertinent que lorsque la variance du bruit de mesure $R(k)$ change radicalement d'un échantillon à l'autre (par exemple en poursuite radar ou fusion GPS + inertiel). Dans un servomoteur synchrone équipé d'un encodeur numérique lu à cadence fixe de $10\text{ kHz}$, le bruit de quantification est stationnaire : le filtre de Kalman dégénère vers un observateur linéaire à gains constants, mais conserve tous les effets de bord néfastes d'une covariance dynamique en présence de jitter FreeRTOS, d'erreurs CRC et de limites `float32`.

Retenir la **Tracking Loop (PLL cinématique du 2nd ordre)** à amortissement critique ($\zeta = 1.0$) offre la même optimalité asymptotique d'estimation de vitesse tout en garantissant zéro dépassement indiciel, une immunité totale aux chocs de réacquisition SPI, un coût CPU divisé par 4 et un réglage universel en Hertz.


---

<a id="partie-5"></a>

# Partie 5 — Use cases encodeurs, scalers et filtrage (état actuel)

*Source d'origine : `doc/analyse_use_cases_encodeurs.md`*

## Analyse des Use Cases d'Encodeurs, Scalers et Filtrage (OpenFFBoard)

Ce document recense de manière exhaustive les cas d'usage (Use Cases) des encodeurs dans le firmware **OpenFFBoard** (branche `refactor/encoder`), établit un audit des implémentations actuelles, et évalue la pertinence des mécanismes de cadencement (`getScaler()`, `EncoderManager`) et de filtrage (`KinematicPLL`, Biquads).

> **🔎 Révision — contre-audit du 27/11/2026**
> Le texte d'origine est conservé intégralement. Les corrections et nouvelles conclusions issues d'un contre-audit du code (`EncoderManager.cpp`, `Encoder.cpp`, `KinematicPLL.cpp`, `EncoderBissC.cpp`, `EncoderSSI.cpp`, `MtEncoderSPI.cpp`, `EncoderTW39.cpp`, `EncoderLocal.cpp`, `TMC4671.cpp`, `SPI.cpp`, `cppmain.cpp`, `Axis.cpp`) apparaissent dans des encadrés **« Révision »** à la fin de chaque section concernée, et dans les nouvelles sections **§7 (défauts temps réel)**, **§8 (architecture cible)** et **§9 (priorités révisées)**.
> Analyse statique uniquement : aucune compilation ni mesure sur cible n'a été réalisée ; les ordres de grandeur de charge IRQ sont des estimations à confirmer à l'analyseur logique / trace.

---

### 1. Contexte fréquentiel et architecture globale

Dans OpenFFBoard, la lecture de position/vitesse/accélération d'un encodeur répond à **deux domaines fréquentiels distincts** :

1. **Boucle HID / FFB (`Axis` / `EffectsCalculator`)** :
   - Cadence nominale : **1 kHz** ($T_s = 1\,\text{ms}$), pouvant être montée plus haut (ex. **2 kHz à 4 kHz**) sur des configurations à faible latence USB/HID.
   - Consomme : position centrée (`getPos_f()`), vitesse angulaire (`getSpeed()`), accélération (`getAccel()`).
   - Effets dépendants : Damper, Friction (`tanhf`), Inertia, Spring, Endstop, Hands-Off Detection.

2. **Boucle de commutation FOC externe (`ExternalEncoderAdapter` $\rightarrow$ `TMC4671`)** :
   - **Historique sur `development`** :
     - Initialement (2022, commit `19d0951`) : `TIM_TMC` configuré à `ARR = 500` (**2 kHz**).
     - Ensuite passé à `ARR = 200` (**5 kHz** = 5 cycles TMC, commenté `200 = 5khz, 250 = 4khz`), puis extrait dans la macro `#define TIM_TMC_ARR 200` (fév. 2025, commit `03448b4`).
     - Réduit à **`4 kHz`** (`#define TIM_TMC_ARR 250`) par défaut sur `F407VG` en mars 2026 (commit `0bbf362` : *"Reduced default TMC forwarding rate to 4khz"*).
   - Cadence rapide actuelle sur `refactor/encoder` : **4 kHz** ($250\,\mu\text{s}$), bornée par le rate-limit SPI sur le registre `PHI_E_EXT` (`0x1C`) de `TMC4671::setExternalPhiE()`.
   - Consomme : position mécanique absolue continue **sans offset de centrage** (`getPosAbs_f()`), et vitesse (`getSpeed()`) pour la compensation de latence.
   - Ne concerne **que** les configurations utilisant un TMC4671 avec un encodeur externe (ou combiné `combineEncoder`).

> **🔎 Révision §1 — fréquences effectives**
> - `EncoderManager::init()` écrit `ARR = 100` **en dur** (`EncoderManager.cpp:34`) avec un PSC à 1 MHz : la période est de `ARR+1 = 101 µs`, soit **≈ 9,90 kHz** et non 10 kHz. La macro `TIM_ENC_ARR` définie dans les `target_constants.h` est ignorée.
> - Le rate-limit de `setExternalPhiE()` (`(now - lastExtPhiEUpdateUs) < 250`, `TMC4671.cpp:3210`) est évalué sur une grille de 101 µs : la première écriture autorisée tombe au 3ᵉ tick (303 µs). La cadence FOC externe effective est donc **≈ 3,3 kHz**, et non 4 kHz. Le portage de l'ancien `TIM_TMC` (4 kHz exacts) vers un seuil en µs a fait perdre ~18 % de fréquence de commutation.
> - Le seuil en µs est en plus exposé au défaut de `micros()` (cf. §7.1).

---

### 2. Recensement exhaustif des Use Cases d'encodeurs

| ID | Use Case | Acteurs / Classes | Fréquence cible | Données requises | Contraintes critiques |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **UC-01** | **Boucle FFB / HID standard** | `Axis`, `EffectsCalculator`, `Encoder` | 1 kHz (extensible 2–4 kHz) | `getPos_f()`, `getSpeed()`, `getAccel()` | Faible bruit de vitesse/accélération, zéro lag excessif, pas de pic lors d'un recenter. |
| **UC-02** | **Commutation FOC externe sur TMC4671** | `EncoderManager`, `ExternalEncoderAdapter`, `TMC4671` | 4 kHz (rate-limit 250 µs sur `0x1C`) | `getPosAbs_f()`, `getSpeed()` | Position absolue continue (insensible à `setPos()`), absence de compétition SPI, déterminisme. |
| **UC-03** | **Mode `combineEncoder` (TMC4671)** | `TMC4671`, `EncoderManager`, `ExternalEncoderAdapter` | 4 kHz (FOC) + 1 kHz (FFB) | Encodeur intégré + externe | Transition dynamique propre lors du basculement de `encsrc` / `combineEncoder`, rattachement dans `EncoderManager`. |
| **UC-04** | **Encodeur intégré TMC4671 (ABN / SinCos / UVW)** | `TMC4671` (`hasIntegratedEncoder() == true`) | 1 kHz (dans `getPos()`) | `PID_POSITION_ACTUAL` (`0x6B`, 65536 counts/tour) | `Axis::setEncType()` n'enregistre rien dans `EncoderManager` ; la PLL doit être alimentée depuis `0x6B` (pas `0x6A` quantifié à 1 RPM). |
| **UC-05** | **Encodeur intégré TMC4671 mode Hall** | `TMC4671` (`encsrc == hall`) | 1 kHz | `0x50 mode=1` / `PID_VELOCITY_ACTUAL` (`0x6A`) | Résolution angulaire très faible (6 pas/paire de pôles) : utilisation du chemin vitesse / filtrage adapté. |
| **UC-06** | **Encodeurs intégrés CAN / Bus série** | `ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `MotorSimplemotion` | Asynchrone (trames CAN 100 Hz – 1 kHz) ou 1 kHz (`getPos()`) | Position et/ou vitesse remontée par le driver | Pas d'`EncoderManager` ; `ODriveCAN` utilise `updateFromVelocity()` sur CAN `0x09`, `VescCAN` sur `decodeEncoderPosition`, `RmdMotorCAN`/`MotorSimplemotion` dans `getPos()`. |
| **UC-07** | **Recentrage utilisateur (`setPos()` / Zero)** | `Axis`, `Encoder`, `KinematicPLL` | Événementiel (CDC / bouton) | `setPos(0)` / `offset` | Ne doit **jamais** provoquer de pic de vitesse/accélération (`reanchor()`) ni décaler l'angle électrique $\phi_E$ du TMC4671. |
| **UC-08** | **Calibration, Homing & Index (Z)** | `TMC4671`, `EncoderLocal`, `Axis` | Initialisation | `useIndex`, `getPosAbs()` | Alignement phase moteur / index sans perturber la PLL en régime établi. |
| **UC-09** | **Détection Hands-Off** | `Axis::updateHandsOffState()` | 1 kHz | `metric.accel`, `metric.speed` | Nécessite une estimation d'accélération non nulle et propre (`getAccel()`). |
| **UC-10** | **Configuration dynamique & Compatibilité GUI** | `Axis`, `EffectsCalculator`, OpenFFBoard Configurator | Événementiel (CDC/HID) | Commandes `damper_f`, `friction_f`, `inertia_f`, `cpr`, `encsrc` | Maintien des IDs/commandes CDC/HID pour ne pas casser le Configurator existant. |
| **UC-11** | **Anticogging & Calibration Cogging (à venir — branche `feat/anticogging`)** | `TMC4671::turn()`, `TMC4671::getFilteredPosition()`, `TMC4671CoggingCalibrator` | 1 kHz (ou cadence HID dans `turn()`), calibration cadencée | Position mécanique absolue **haute résolution** modulo 1 tour `[0, 1)` + vitesse pour *phase-lead* et *speed-ripple* | **Invariant au `zeroenc` (`setPos()`)**, résolution > 16 bits préservée (ABN `0x27` ou `drvEncoder->getPosAbs() % cpr`), zéro lecture SPI bloquante supplémentaire en `turn()`. |

> **🔎 Révision §2 — Use Cases**
> - **UC-02** : fréquence effective ≈ 3,3 kHz (cf. §1). Contrainte manquante : **l'écriture de `0x1C` ne doit jamais prendre de sémaphore en contexte ISR** (cf. §7.3).
> - **UC-03 / changement d'encodeur** : contrainte manquante de **cycle de vie** — aucun callback SPI d'un encodeur détruit ne doit pouvoir s'exécuter (cf. §7.4).
> - **UC-07** : l'exigence « jamais de pic via `reanchor()` » n'est **pas tenue** pour les encodeurs externes : `EncoderBissC::setPos()`, `EncoderSSI::setPos()`, `MtEncoderSPI::setPos()` ne rappellent pas `reanchor()` ; seul le *gating* absorbe le saut (cf. §5.1 Révision).
> - **UC-01 / UC-09** : contrainte manquante de **monotonie de la base de temps** (`dt`) : un `dt` aberrant provoque un reset complet de la PLL (vitesse et accélération à 0) (cf. §7.1).
> - **Nouveau UC-12 — Transmission démultipliée (`gearRatio`)** : l'encodeur côté moteur peut tourner à `gearRatio ×` la vitesse volant. La vitesse max encodeur n'est donc pas bornée par la physique du volant ; `MAX_RPS` doit être paramétrable (cf. §7.5).

#### 2.1 Zoom sur `UC-11` (branche `feat/anticogging`) et impact sur l'architecture

L'inspection de la branche `origin/feat/anticogging` (`TMC4671.cpp` et `TMC4671_cogging.cpp`) montre comment la position et la vitesse sont utilisées :
- **Position absolue mécanique (`TMC4671::getFilteredPosition()`)** :
  - Encodeur externe : `drvEncoder->getPosAbs() % drvEncoder->getCpr()` normalisé dans `[0, 1)` (invariant à `zeroenc`).
  - Encodeur intégré ABN : lecture du compteur brut 32 bits `0x27` (`ABN_DECODER_COUNT`) + `phiMoffset` pour conserver la résolution > 16 bits et l'invariance à `zeroenc` (contrairement à `0x6B` qui est décalé par `setPos()`).
  - Encodeur intégré SinCos/UVW : lecture matérielle `0x46` (`PHI_M_AENC`).
  - Encodeur intégré Hall : lecture matérielle `0x3A`.
- **Vitesse dans `TMC4671::turn()`** :
  - Actuellement, `feat/anticogging` recalcule une vitesse instantanée locale (`turn_vel_filtered`, EMA $\alpha = 0.25$) par dérivation de `pos_f` sur `dt_us`, utilisée pour :
    1. Le facteur d'amplitude asymptotique $S(\omega) = |\omega| / (|\omega| + \omega_c)$.
    2. L'avance de phase prédictive (`pred_pos_f = pos_f + turn_vel_filtered * cogging_phase_lead_time`).
    3. Le roll-off anti-aliasing ZOH au-dessus de 200–350 Hz (`harm_freq_hz = order * abs_vel`).
- **Remise en question des Use Cases & synergies avec `refactor/encoder`** :
  - **Ne remet pas en question** les Use Cases `UC-01` à `UC-10`, mais **confirme et renforce** deux choix structurants de `refactor/encoder` :
    1. L'obligation de maintenir une **position mécanique absolue** (`getPosAbs()` / `getPosAbs_f()`) strictement indépendante de l'offset utilisateur (`setPos()` / `zeroenc`), à la fois pour le FOC (`UC-02`) et pour l'anticogging (`UC-11`).
    2. La **passivité de `getPosAbs()`** (pas de transaction SPI synchrone lors de l'appel dans `turn()`, sinon collision SPI et latence sur la boucle FFB).
  - **Opportunité de simplification lors du merge** : dans `TMC4671::turn()`, le dérivé local `turn_vel_filtered` pourra directement être remplacé par `activeEnc->getSpeed()` issu de `KinematicPLL` (plus propre, sans bruit de quantification et sans état redondant), tout en conservant `getFilteredPosition()` (ou `getPosAbs_f()` modulo 1) pour la phase mécanique exacte.

---

### 3. Audit des implémentations actuelles

#### 3.1 Encodeurs externes (héritant de `Encoder`)

| Classe | Acquisition | `getPosAbs()` | `triggerRead()` | `getScaler()` | État & Anomalies identifiées |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`EncoderBissC`** | SPI DMA (BiSS-C) | Passif (lit le dernier angle décodé) | Déclenche lecture SPI DMA | `1` (10 kHz) | **Sain**. Plus de compétition SPI entre `EncoderManager` et `Axis`. |
| **`MtEncoderSPI`** | SPI DMA (MT6825/MT6835) | Passif | Déclenche lecture SPI DMA | `1` (10 kHz) | **Sain**. Compatible `EncoderManager`. |
| **`EncoderLocal`** | Timer matériel (`TIM->CNT`) | Lit `TIM->CNT` + tours | Vide (lecture directe) | `clamp(80000 / cpr, 1, 10)` | **Sain** (EXTI T-Method supprimée). Formule de scaler à réévaluer (cf. §4). |
| **`EncoderSSI`** | SPI DMA (SSI) | **Appelle encore `spiPort.receive_DMA()`** | Déclenche lecture SPI DMA | `1` (10 kHz) | **Anomalie (concurrence SPI)** : risque de collision SPI entre la boucle `Axis` (1 kHz) et `EncoderManager` (10 kHz), même problème que celui corrigé sur `EncoderBissC`. |
| **`EncoderTW39`** | SPI / protocole TW39 | **Sémaphores bloquants dans `getPosAbs()`** | Non surchargé | Défaut (`1`) | **Anomalie (architecture)** : possède encore son propre `cpp_freertos::Thread` et bloque `getPosAbs()` sur sémaphore, court-circuitant le modèle unifié d'`EncoderManager`. |
| **`EncoderNone`** | Aucune | Retourne 0 | Vide | Défaut | **Sain** (placeholder). |

> **🔎 Révision §3.1 — tableau corrigé**
>
> | Classe | Acquisition réelle | Contexte de la mise à jour PLL | Correction / nouvelle anomalie |
> | :--- | :--- | :--- | :--- |
> | **`EncoderBissC`** | **SPI en IT** (`spiPort.receive_IT`, `EncoderBissC.cpp:233`), pas DMA. Trame 8 octets. | **ISR SPI** (`spiRxCompleted` → `updateState()`) | Accès passif confirmé **sain**. Mais : (1) environ 16 IRQ par trame (cf. §7.6) ; (2) `dt` mesuré à la **fin** de trame via `micros()` alors que BiSS échantillonne en début de trame ; (3) `setPos()` sans `reanchor()` ; (4) `abortTransfer()` dans le destructeur n'arrête pas un transfert IT (cf. §7.4). |
> | **`MtEncoderSPI`** | **SPI en IT** (`transmitReceive_IT`, `MtEncoderSPI.cpp:211/215`). Le commentaire `:205-207` documente l'abandon du DMA (« DMA stream lockups and Watchdog resets »). | **ISR SPI** | Même remarques que BiSS-C. Les blocages DMA historiques sont plus probablement dus à la concurrence d'accès (SSI, `getPosAbs()` actifs) qu'au DMA lui-même. |
> | **`EncoderLocal`** | Timer matériel | **Tâche `ENCMGR`** (via `Encoder::triggerRead()`) | **Pas sain** : `getPos()` = `CNT + pos` (`EncoderLocal.cpp:76-78`) n'est pas atomique vis-à-vis de l'ISR de débordement (`:122-128`). Un overflow entre les deux lectures donne un saut de ±65536 counts, transformé en glitch PLL. De plus, l'adaptateur FOC est appelé ici en **contexte tâche**, contrairement aux encodeurs SPI (contexte ISR) : contexte d'exécution incohérent. |
> | **`EncoderSSI`** | SPI en DMA | **Tâche + ISR (double)** | **Anomalie aggravée** : `EncoderSSI` ne surcharge **pas** `triggerRead()`. `Encoder::triggerRead()` appelle `getPos_f()` → `getPosAbs()` → `receive_DMA` **et** `updateState()` avec la position périmée, puis `spiRxCompleted()` appelle à nouveau `updateState()` avec un `dt` d'environ 10 µs. On a donc **deux mises à jour PLL par cycle**, dont une avec une donnée retardée, et **deux appels FOC**. `beginSpiTransfer()` / `endSpiTransfer()` ne prennent pas le sémaphore du port (commenté). |
> | **`EncoderTW39`** | SPI DMA via thread dédié | **Tâche `ENCMGR`** (via `Encoder::triggerRead()`) | **Anomalie aggravée** : `triggerRead()` de base → `getPosAbs()` → `waitForUpdateSem.Take(waitThresh)` (`EncoderTW39.cpp:276`) : **`ENCMGR` (prio 33) peut bloquer**. Comme `curPos` est mis à jour plus tard par le thread TW39, la PLL reçoit **systématiquement la position du cycle précédent** (une période de retard). |

#### 3.2 Drivers moteur à encodeur intégré (`hasIntegratedEncoder() == true`)

| Classe | Source PLL | Cadence de mise à jour | État & Anomalies identifiées |
| :--- | :--- | :--- | :--- |
| **`TMC4671` (ABN/SinCos/UVW)** | `PID_POSITION_ACTUAL` (`0x6B`) via `pll.update()` | 1 kHz (dans `getPos()`) | **Sain**, mais **code mort résiduel** : hérite encore de `TimerHandler` avec un `timerElapsed()` vide. |
| **`TMC4671` (Hall)** | `0x50 mode=1` / `0x6A` | 1 kHz | **Sain** (fallback vitesse pour faible résolution). |
| **`ODriveCAN`** | `pll.updateFromVelocity()` sur trame CAN `0x09` | Cadence CAN | **Sain**. |
| **`VescCAN`** | `pll.update()` dans `decodeEncoderPosition()` | Cadence CAN | **Sain**. |
| **`RmdMotorCAN`** | `pll.update()` dans `getPos()` | 1 kHz (garde `dt_us >= 200`) | **Attention** : la garde `dt_us >= 200` (5 kHz max) est sûre à 1–4 kHz, mais à surveiller si `getPos()` est appelé plusieurs fois dans la même fenêtre de 200 µs. |
| **`MotorSimplemotion`** | `pll.update()` dans `getPos()` | 1 kHz (garde `dt_us >= 200`) | Idem `RmdMotorCAN`. |

> **🔎 Révision §3.2**
> - **`TMC4671` — code mort** : l'héritage `TimerHandler` est conditionné par `#ifdef TIM_TMC` (`TMC4671.h:376`), **macro qui n'est plus définie** sur cette branche ; l'héritage n'est donc pas compilé. Il ne reste qu'un `timerElapsed()` vide sous `#ifdef TIM_ENC` (`TMC4671.cpp:3164-3170`, `TMC4671.h:480-482`), non appelé puisque `TMC4671` n'est pas un `TimerHandler`.
> - **`TMC4671` (ABN/SinCos/UVW)** : `setPos()` → `setTmcPos()` appelle bien `pll.reanchor()` (`TMC4671.cpp:1906`). C'est le seul chemin où `reanchor()` est correctement câblé, avec `RmdMotorCAN` et `MotorSimplemotion`.
> - **`TMC4671` tuning** : `pll.setTuning(65 Hz, 1.0, 28 Hz, 55 Hz)` à 1 kHz pour ABN/AENC, soit ω_n·T ≈ 0,41 (stable, légèrement sous-amorti en discret, cf. §5.1 Révision), et `pll.setTuning(30 Hz, 1.0, 18 Hz, 32 Hz)` pour Hall (`updateFromVelocity()`).
> - **`VescCAN`** : `pll.setTuning(30 Hz, 1.0, 18 Hz, 32 Hz)` (comme `ODriveCAN`, `RmdMotorCAN` et `MotorSimplemotion`) alors que la cadence des trames dépend du polling (`askPositionEncoder()` dans `getPos_f()`). La boucle devient **instable** si les réponses arrivent à moins d'environ 230 Hz (ω_n·T > 0,83). Le statut « Sain » est à requalifier en « **conditionnel à la cadence CAN** ».
> - **`ODriveCAN`** : `updateFromVelocity()` contourne la boucle (vitesse injectée directement). Pas de risque de stabilité, mais l'accélération est dérivée d'une vitesse CAN à cadence variable.

---

### 4. Pertinence des `getScaler()` et d'`EncoderManager`

#### 4.1 Cadencement global d'`EncoderManager` (10 kHz)
Actuellement, `EncoderManager` fait tourner un timer matériel (`TIM_ENC`) à **10 kHz** qui réveille le thread FreeRTOS `ENCMGR` (priorité 33) pour appeler `triggerRead()` et `update()` (PLL).

- **Cas avec FOC externe (`TMC4671` + encodeur externe)** :
  - `TMC4671::setExternalPhiE()` applique un rate-limit SPI à **4 kHz** ($250\,\mu\text{s}$).
  - Faire tourner `EncoderBissC` / `EncoderSSI` / `MtEncoderSPI` à **10 kHz** (`scaler = 1`) sur-échantillonne le bus SPI par rapport au besoin FOC effectif (4 kHz) : 60 % des transactions SPI DMA et des calculs PLL sont consommés sans mise à jour du registre `0x1C`.
  - Un scaler de `2` (5 kHz) ou un alignement direct sur 4 kHz réduirait significativement la charge CPU et le trafic SPI/DMA tout en conservant une latence quasi identique pour le FOC.

- **Cas sans FOC externe (FFB/HID uniquement)** :
  - Si aucun `ExternalEncoderAdapter` n'est attaché (ex. encodeur externe utilisé uniquement pour l'axe HID/FFB sans FOC TMC4671), le thread `ENCMGR` continue pourtant de tourner à 10 kHz (`scaler = 1`) pour un consommateur `Axis` qui ne lit qu'à **1 kHz** (ou 2–4 kHz).
  - **Recommandation** : adapter dynamiquement le diviseur effectif d'`EncoderManager` selon la présence d'un consommateur FOC (ex. 4–5 kHz si FOC actif, sinon aligné sur la cadence HID/FFB, typiquement 1–2 kHz).

> **🔎 Révision §4.1**
> - **Chiffres corrigés** : timer à 9,90 kHz ; écritures `0x1C` à environ 3,3 kHz. Ce sont donc **2 lectures sur 3** qui sont jetées pour le FOC (≈ 67 %), et non 60 %.
> - **« Transactions SPI DMA »** : faux pour BiSS-C et MT (transferts **IT**). Le coût réel est bien supérieur à ce que suggère le DMA (cf. §7.6).
> - **Timer jamais arrêté** : `ENCMGR` est réveillé environ 10 000 fois/s **même sans aucun encodeur enregistré** (cas des drivers à encodeur intégré). Environ 20 000 changements de contexte/s inutiles.
> - **⚠️ Danger de la recommandation « 1–2 kHz sans FOC »** : avec le tuning actuel (`f_n = 130 Hz` pour CPR ≥ 65536, `Encoder.cpp:115`), passer à 1 kHz donne ω_n·T ≈ 0,82, **à la limite d'instabilité** de la PLL discrétisée (borne ≈ 0,83, cf. §5.1 Révision). Cette recommandation n'est applicable **qu'après** avoir borné `f_n` en fonction de `f_s`.
> - **Décimation** : remplacer le rate-limit en µs par un **diviseur entier sur le tick**, avec un timer choisi pour que FOC = tick/N exactement (ex. timer 8 kHz → FOC 4 kHz, FFB 1 kHz).
> - **Pile `ENCMGR`** : 128 mots (512 o) alors que la chaîne `EncoderLocal::triggerRead()` → `updateState()` → `updateAdaptersForEncoder()` → `setExternalPhiE()` → `writeRegAsync()` → `takeSemaphore()` s'exécute dans cette tâche, avec contexte FPU. À vérifier avec `uxTaskGetStackHighWaterMark()`.

#### 4.2 Formule `getScaler()` de `EncoderLocal`
Actuellement : `scaler = clamp(80000 / cpr, 1, 10)` (soit une fréquence d'échantillonnage comprise entre **1 kHz** pour `cpr <= 8000` et **10 kHz** pour `cpr >= 80000`).

- **Analyse** :
  - Pour un encodeur incrémental basse résolution (ex. `cpr = 2000` à `8000`), `scaler = 10` $\rightarrow$ **1 kHz**. C'est pertinent pour éviter de faire tourner la PLL à 10 kHz sur des pas de quantification très espacés (bruit de quantification amplifié par $f_s$).
  - Pour un encodeur haute résolution (ex. `cpr = 40000`), `scaler = 2` $\rightarrow$ **5 kHz**, et pour `cpr >= 80000`, `scaler = 1` $\rightarrow$ **10 kHz**.
  - **Limite** : cette formule ignore si le FOC externe est actif ou non. Pour un `EncoderLocal` 10 000 CPR utilisé en FOC externe, `scaler = 8` (1,25 kHz) est trop lent pour une commutation FOC fluide à haute vitesse ; inversement, pour un 80 000 CPR sans FOC, 10 kHz est inutilement coûteux.
  - **Recommandation** : découpler le besoin FOC (fréquence minimale 4 kHz si FOC actif) du besoin FFB, et laisser `KinematicPLL` gérer la quantification via son adaptation automatique de $f_n$ en fonction du CPR.

> **🔎 Révision §4.2**
> - Le scaler n'est jamais connu de la PLL : `f_n` est choisi par seuils de CPR (`Encoder.cpp:118-124` : `130/40/90` pour `≥ 65536`, `75/28/55` pour `16384..65535`, `45/22/45` pour `< 16384`) **indépendamment de `f_s`**. Exemples : `EncoderLocal` 8 000 CPR → scaler 10 → 1 kHz, `f_n = 45 Hz`, ω_n·T ≈ 0,28 (OK). `EncoderLocal` 16 384 CPR → scaler 4 → 2,5 kHz, `f_n = 75 Hz`, ω_n·T ≈ 0,19 (OK). La combinaison actuelle reste dans le domaine stable, mais **par coïncidence**, et non par construction.
> - **Recommandation complémentaire** : `f_n = min(f_n(CPR), k·f_s)` avec k ≈ 0,05 (marge d'amortissement confortable), recalculé à chaque changement de cadence ou de CPR.
> - Le scaler `cpr == 0 → 10` est cohérent, mais `EncoderLocal::getPos()` doit d'abord devenir atomique (cf. §3.1 Révision).

---

### 5. Pertinence des filtres (`KinematicPLL` et Biquads)

#### 5.1 `KinematicPLL` (Type-II Tracking Loop)
- **Pertinence** : **Très élevée** pour tous les Use Cases.
  - Multi-tours continu sans wrap modulo $[-\pi, \pi]$, éliminant les discontinuités.
  - Clamps physiques (`11.14 rot/s`, `318.3 rot/s²`) et outlier gating proportionnel à `dt`, protégeant contre les trames corrompues (BiSS-C, SSI, CAN).
  - `reanchor()` sans pic lors de `setPos()`.
  - Accélération découplée par filtre RC 1er ordre : fournit un `getAccel()` exploitable pour l'effet Inertia et la détection Hands-Off sans déstabiliser la boucle de position/vitesse.
  - Adaptation automatique de $f_n$ selon le CPR (`Encoder.cpp`) : compense proprement la différence entre un encodeur 2 000 CPR et un BiSS-C 22-bit.
- **Compatibilité HID > 1 kHz (2–4 kHz)** :
  - `KinematicPLL::update(pos, dt)` étant formulée en temps continu discrétisé par `dt` réel, elle supporte nativement une montée en fréquence de la boucle HID à 2–4 kHz sans recalcul manuel de coefficients.

> **🔎 Révision §5.1 — `KinematicPLL`**
> - **Borne de stabilité discrète (non mentionnée)** : la discrétisation Euler explicite utilisée (`vel = kp·e + ∫ki·e·dt`, `pos += vel·dt`, ζ = 1) a pour polynôme caractéristique λ² − (2 − a − b)·λ + (1 − a), avec a = 2ω_n·T et b = (ω_n·T)². Le critère de Jury impose **ω_n·T < 2(√2 − 1) ≈ 0,83**, soit **f_n < 0,13·f_s**. L'amortissement réel se dégrade bien avant : ζ_discret ≈ 0,7 dès ω_n·T ≈ 0,3. « Supporte nativement » est donc vrai **en montée** de fréquence, et **faux en descente**.
>
>   | Chemin | f_n (vel_fc / accel_fc) | f_s | ω_n·T | Verdict |
>   | :--- | :--- | :--- | :--- | :--- |
>   | Externe CPR ≥ 65536 | 130 Hz (90 / 40 Hz) | 9,9 kHz | 0,08 | OK |
>   | Externe CPR ≥ 65536 **à 1 kHz** (reco §4.1) | 130 Hz (90 / 40 Hz) | 1 kHz | **0,82** | **Limite d'instabilité** |
>   | TMC4671 intégré (ABN/AENC) | 65 Hz (55 / 28 Hz) | 1 kHz | 0,41 | Stable, légèrement sous-amorti |
>   | TMC4671 intégré (Hall) | 30 Hz (32 / 18 Hz) | 1 kHz | — (`updateFromVelocity`) | OK |
>   | VescCAN / CAN / SimpleMotion | 30 Hz (32 / 18 Hz) | ≤ 1 kHz (CAN) | ≥ 0,19 | Instable si f_s < 230 Hz |
>
> - **`reanchor()` sur `setPos()`** : vrai uniquement pour `TMC4671`, `RmdMotorCAN` et `MotorSimplemotion`. **Faux** pour tous les encodeurs externes (BiSS-C, SSI, MT, TW39, Local). Le recentrage passe par le gating : 3 échantillons en roue libre (`vel *= 0.95`, `accel *= 0.90`), puis ré-ancrage. Un recentrage **inférieur au seuil de gating** (0,004 tour minimum, `KinematicPLL.cpp:70`) est intégré comme un mouvement réel, avec un **pic de vitesse**. À corriger avec un appel `pll.reanchor()` dans chaque `setPos()`, ou un hook commun dans `Encoder`.
> - **Gating et saturation couplés** : le seuil `3·MAX_RPS·dt` et la saturation `MAX_RPS = 11,14 rot/s` utilisent la même constante. Au-delà de 668 RPM côté encodeur (cf. UC-12), la PLL décroche, entre en cycle glitch → ré-ancrage, et la vitesse reste figée (cf. §7.5).
> - **Reset sur `dt > 0.05 s`** : vitesse et accélération remises à 0. Combiné au défaut de `micros()` (§7.1), ce reset se produit en fonctionnement normal.
> - **Découplage FOC / FFB et accélération** : la boucle rapide PI (`getRawSpeed()`) alimente l'extrapolation FOC (`ExternalEncoderAdapter`) sans retard `tau_vel`. Pour le FFB (`Axis.cpp`), la vitesse (`getSpeed()`) est filtrée au 3ᵉ ordre depuis l'intégrateur (`tau_vel`, sans sauts `kp·error`), et l'accélération (`getAccel()`) est dérivée de cette vitesse lissée `(vel_ffb − vel_prev)/dt` puis filtrée par un RC (`tau_accel`, 4ᵉ ordre). Le RC reste toutefois appliqué au même `dt` bruité ; avec un `dt` déterministe (§8), le filtre deviendrait à coefficients constants.

#### 5.2 Filtres Biquads (`Axis.cpp` et `EffectsCalculator.cpp`)
- **Constat actuel** :
  - Les Biquads secondaires (`damperFilter`, `frictionFilter`, `inertiaFilter`) ont été bypassés dans `Axis.cpp` et gardés dans `EffectsCalculator.cpp`, tout en conservant les commandes CDC/HID (`damper_f`, `friction_f`, etc.) pour la compatibilité avec OpenFFBoard Configurator.
  - La friction utilise désormais un lissage continu en `tanhf` dans `Axis.cpp` et `EffectsCalculator.cpp`, éliminant les oscillations autour de vitesse nulle.
- **Pertinence** :
  - **Valide à 1 kHz** : la `KinematicPLL` filtrant déjà la vitesse et l'accélération en amont, empiler un second Biquad sur chaque effet ajoutait du déphasage (retard de groupe) et rendait le damper/inertia « mou » ou instable.
  - **Attention si HID > 1 kHz** : si des Biquads restent actifs quelque part (ex. filtre de sortie ou filtre optionnel réactivé), leurs coefficients doivent impérativement être recalculés avec la vraie fréquence `fs` de la boucle HID (et non une constante `1000.0f` codée en dur).

> **🔎 Révision §5.2 — calibrage FFB dans `Axis.cpp`**
> - `Axis.cpp` consomme `encoder->getSpeed()` (3ᵉ ordre) et `encoder->getAccel()` (4ᵉ ordre), applique un lissage anti-imaging à 1 kHz (`0.43f` sur `metric.speed`, `0.27f` sur `metric.accel`, `Axis.cpp:751-752`), et calibre la friction continue sur `tanhf(metric.speed / 58.0f)` (`Axis.cpp:702`, pente équivalente à l'ancienne rampe sinusoïdale).

---

### 6. Synthèse des actions correctives recommandées (priorisées)

1. **Haute priorité (sécurité bus SPI)** : Rendre `EncoderSSI::getPosAbs()` passif (comme `EncoderBissC`) pour supprimer toute compétition SPI DMA entre `Axis` et `EncoderManager`.
2. **Moyenne priorité (architecture temps réel)** : Refactorer `EncoderTW39` pour supprimer son thread FreeRTOS dédié et ses sémaphores bloquants dans `getPosAbs()`, afin de l'aligner sur `triggerRead()` / `EncoderManager`.
3. **Moyenne priorité (optimisation CPU/SPI)** :
   - Aligner la cadence max d'`EncoderManager` sur le besoin réel (4–5 kHz si FOC externe actif, 1–2 kHz sinon).
   - Réviser `EncoderLocal::getScaler()` pour prendre en compte l'état FOC plutôt que le seul ratio `80000 / cpr`.
4. **Basse priorité (propreté code)** : Supprimer l'héritage `TimerHandler` et le `timerElapsed()` vide dans `TMC4671`.

> **🔎 Révision §6** : cette liste reste valide sur ses points 1 et 2, mais elle est **incomplète et mal ordonnée**. Elle omet trois défauts plus graves (§7.1, §7.3, §7.4), et son point 3 (baisse de cadence) est dangereux tant que §7.2 n'est pas traité. Le point 4 concerne un héritage déjà inactif (`#ifdef TIM_TMC`). Voir la priorisation révisée en **§9**.

---

### 7. Défauts temps réel non identifiés dans la version initiale *(nouveau — révision 27/11/2026)*

#### 7.1 `micros()` non monotone → reset intempestif de la PLL
- **Code** : `cppmain.cpp:129`, `return (HAL_GetTick() * 1000) + TIM_MICROS_HALTICK.Instance->CNT;` (F407VG : `TIM_MICROS_HALTICK = htim7`, tick à priorité NVIC 4).
- **Défaut** : deux lectures non atomiques. TIM7 (priorité 4, au-dessus de `configMAX_SYSCALL_INTERRUPT_PRIORITY = 5`) préempte les tâches, les sections critiques FreeRTOS et les ISR SPI (priorités 6/8/9). Si elle survient entre la lecture de `HAL_GetTick()` et celle de `CNT`, le résultat **recule d'environ 1000 µs**. Inversement, si `CNT` reboucle alors que l'IRQ n'est pas encore servie, le résultat recule aussi.
- **Conséquence** : un `dt` négatif devient, en arithmétique `uint32_t`, une valeur d'environ 4·10⁹ µs. `KinematicPLL::update()` reçoit alors `dt > 0.05` et fait un **reset complet** (vitesse = 0, accélération = 0, intégrateur = 0). Un `dt` de +1 ms produit une erreur de dérivée d'accélération d'un facteur ~10 à 10 kHz.
- **Fréquence estimée** : fenêtre d'environ 100 ns × environ 10⁴ appels/s, soit de l'ordre d'**un événement par seconde** (à confirmer par instrumentation).
- **Symptômes attendus** : micro-à-coups du damper/friction, pics d'inertie, fausses détections Hands-Off, phiE extrapolé faux pendant quelques cycles.
- **Correctif** : lecture en boucle (`do { t = tick; c = CNT; } while (t != tick)` avec gestion du flag UIF), ou `DWT->CYCCNT`, ou timer 32 bits libre. Mieux : **`dt` déterministe dérivé du timer d'échantillonnage** (§8).

#### 7.2 Stabilité de la PLL non bornée par `f_s`
Voir le détail en §5.1 Révision. Toute baisse de cadence (§4.1, §4.2) ou tout driver CAN lent peut rendre la PLL sous-amortie ou instable. **Correctif** : `f_n = min(f_n(CPR), 0.05·f_s)`.

#### 7.3 Prise de sémaphore SPI en ISR dans le chemin FOC
- **Chaîne** : ISR SPI encodeur (`spiRxCompleted` / `spiTxRxCompleted`) → `Encoder::updateState()` → `EncoderManager::updateAdaptersForEncoder()` → `ExternalEncoderAdapter::update()` → `TMC4671::setExternalPhiE()` → `writeRegAsync()` → `spiPort.takeSemaphore()`.
- **Défaut 1** : `SPIPort::takeSemaphore()` en ISR (`SPI.cpp:197-200`) appelle `TakeFromISR` **sans tester le retour**, puis force `isTakenFlag = true`. Si le sémaphore est pris par la tâche TMC, l'ISR lance quand même la trame `0x1C`.
- **Défaut 2** : la garde `if (!spiPort.isTaken())` (`TMC4671.cpp:3207`) est **non atomique** (TOCTOU). En contexte tâche, `isTakenFlag = true` est positionné *après* `semaphore.Take()`. Une préemption entre les deux laisse voir un bus « libre » alors que le sémaphore est pris.
- **Conséquence** : **collision possible sur le SPI moteur** (trame `0x1C` lancée pendant un `readReg` / `writeReg` synchrone de la tâche TMC), avec corruption de registres TMC4671 et CS incohérent.
- **Incohérence** : le même `setExternalPhiE()` s'exécute en **ISR** (BiSS, MT, SSI) ou en **tâche** (`EncoderLocal`, `TW39`), selon l'encodeur.
- **Correctif** : boîte aux lettres phiE sans verrou (§8.3).

#### 7.4 Cycle de vie des encodeurs : risque d'use-after-free
- `Axis::setEncType()` suspend `ENCMGR` (`Axis.cpp:536`), mais **pas les transferts SPI déjà lancés**.
- Les destructeurs appellent `spiPort.abortTransfer(this)` → `HAL_SPI_DMAStop()` (`SPI.cpp:186`), **sans effet sur un transfert IT** (BiSS-C, MT). Le callback `spiRxCompleted` / `spiTxRxCompleted` peut s'exécuter **sur un objet détruit**.
- Le « RCU grace period » `vTaskDelay(2 ms)` (`TMC4671.cpp:1526`, `:1860`) est une heuristique temporelle, pas une garantie de quiescence.
- **Correctif** : `HAL_SPI_Abort_IT` / `HAL_SPI_Abort` selon le mode, attente de fin de transfert (flag `transferActive` attendu à `false`), désinscription, puis destruction. Les callbacks doivent vérifier `current_device == this`.

#### 7.5 `MAX_RPS` et gating couplés → décrochage avec transmission démultipliée
- `MAX_RPS = 11,14 rot/s` (668 RPM), `KinematicPLL.h:13`, sert à la fois de saturation vitesse, de saturation intégrateur et de base du seuil de gating `3·MAX_RPS·dt`.
- Avec `gearRatio > 1` (courroie), le moteur dépasse facilement 668 RPM lorsque le volant est lâché. La vitesse estimée sature, l'erreur de position croît, le gating rejette les mesures, puis ré-ancre au bout de 3 échantillons : **cycle répété**, vitesse figée et accélération incohérente.
- Les effets damper / endstop / protection survitesse deviennent **aveugles précisément dans la situation où ils sont nécessaires**.
- **Correctif** : `MAX_RPS` paramétrable par encodeur (CPR, ratio), relevé d'un facteur ≥ 3 par défaut, et seuil de gating découplé (par exemple `|e| > max(k·|vel_est|·dt + marge, n·LSB)`).

#### 7.6 Charge d'interruptions réelle (estimation)
| Source | Mécanisme | Ordre de grandeur |
| :--- | :--- | :--- |
| Tick `TIM_ENC` | 1 IRQ + `NotifyFromISR` + 2 changements de contexte (réveil puis retour `ENCMGR`) | ~10 k IRQ/s + ~20 k ctx/s |
| BiSS-C 8 octets en IT | `HAL_SPI_Receive_IT` en maître full-duplex → `TransmitReceive_IT` : ~2 IRQ/octet | **~160 k IRQ/s** |
| MT6835 6 octets en IT | Idem | ~120 k IRQ/s |
| Écriture `0x1C` 5 octets en IT (sans `TMC4671_ALLOW_DMA`) | ~2 IRQ/octet à ~3,3 kHz | ~33 k IRQ/s |
| PLL flottante + adaptateur + `takeSemaphore` | **Exécutés dans l'ISR SPI** | Latence ISR allongée pour tout le système |

À confirmer par mesure (GPIO toggle + analyseur logique, ou trace SWO / SEGGER SystemView).

#### 7.7 Divers
- `EncoderLocal::getPos()` non atomique (cf. §3.1 Révision).
- `dt` horodaté à la **fin** de la trame, et non à l'instant d'échantillonnage. Avec BiSS à 600 kHz, une trame de 64 bits dure environ 107 µs : erreur de phase systématique, et jitter sur `dt` qui dépend de la charge IRQ.
- Extrapolation phiE à latence fixe de 50 µs (`ExternalEncoderAdapter.cpp:20`) alors que l'âge réel de l'échantillon varie de ~100 à ~400 µs (durée de trame + attente du rate-limit).
- `EncoderManager::init()` ignore `TIM_ENC_ARR`.

---

### 8. Architecture cible proposée *(nouveau — révision 27/11/2026)*

#### 8.1 Base de temps déterministe
- L'instant d'échantillonnage est l'instant de **déclenchement** du transfert (ISR timer), et non sa réception.
- Avec un échantillonnage cadencé par timer : `dt = N × T_tick` **exactement**. Il n'y a plus d'appel à `micros()` dans la PLL, les coefficients (`kp·dt`, `ki·dt`, `alpha_rc`) sont précalculés, et le jitter de `dt` est nul.
- `micros()` reste utilisé pour le diagnostic, mais corrigé (§7.1).

#### 8.2 Chaîne ISR sans tâche pour le chemin rapide
```
TIM_ENC update ISR ──► lancement transfert SPI encodeur (DMA, HT IRQ désactivée)
                         │
DMA RX complete ISR ◄────┘
   ├─ décodage + CRC
   ├─ PLL (dt constant)
   ├─ publication atomique {pos_abs, vel, accel, t_sample, seq}
   └─ si FOC actif et tick % N_foc == 0 : publication phiE dans la mailbox (§8.3)
```
- On passe à **2–3 IRQ par cycle** et zéro changement de contexte, contre plus de 18 IRQ et 2 changements de contexte aujourd'hui.
- La tâche `ENCMGR` n'est plus nécessaire sur le chemin rapide. Elle peut être conservée pour les encodeurs lents ou de configuration.
- Le DMA doit être réhabilité **après** suppression des accès concurrents (SSI actif, TW39), probable cause des blocages documentés dans `MtEncoderSPI`.

#### 8.3 Boîte aux lettres phiE sans verrou
- L'ISR encodeur **publie seulement** `{phiE, t_sample}` (mot 32 bits atomique, ou double tampon avec numéro de séquence).
- Le propriétaire du SPI moteur l'envoie : soit la tâche TMC dans sa boucle, soit, mieux, le **callback de fin de transfert SPI moteur** qui enchaîne l'écriture `0x1C` si une valeur nouvelle est en attente et que le bus est libre (machine d'état du port, pas de sémaphore en ISR).
- Extrapolation calculée **au moment de l'envoi** avec l'âge réel : `phiE = pos + vel·(t_send − t_sample)`.

#### 8.4 Décimation déterministe et cadence pilotée par les consommateurs
| Configuration | Cadence d'échantillonnage | FOC | PLL `f_n` max (0,05·f_s) |
| :--- | :--- | :--- | :--- |
| FOC externe actif | 8 kHz (ou 10 kHz) | tick/2 → 4 kHz (ou 5 kHz) exacts | 400 Hz |
| FFB seul, HID 1 kHz | 2–4 kHz | — | 100–200 Hz |
| FFB seul, HID 4 kHz | 8 kHz | — | 400 Hz |
| Aucun encodeur enregistré | **Timer arrêté** | — | — |

#### 8.5 Contrat d'interface `Encoder` révisé
- `getPosAbs()`, `getPos()`, `getSpeed()`, `getAccel()` sont **strictement passifs** (lecture d'un instantané publié), sans aucune E/S.
- `triggerRead()` est la seule méthode qui lance une E/S. Elle est obligatoire pour tout encodeur à bus. L'implémentation par défaut de `Encoder::triggerRead()` est réservée aux sources à lecture registre instantanée.
- `setPos()` appelle systématiquement `pll.reanchor()` (hook commun dans la classe de base).
- Contexte d'exécution unique et documenté pour `updateState()` (ISR de fin de transfert, ou tâche, mais pas les deux selon l'encodeur).
- Séquence de destruction : arrêt des déclenchements → abort et attente du transfert → désinscription → destruction.

---

### 9. Priorités révisées *(remplace la priorisation du §6 — révision 27/11/2026)*

| Priorité | Action | Réf. | Statut / Commit |
| :--- | :--- | :--- | :--- |
| **P0** | `micros()` monotone, ou `dt` déterministe issu du timer d'échantillonnage | §7.1, §8.1 | **Fait** (`2a5c9e9`) — lecture atomique `HAL_GetTick()` + `CNT` + `TIM_SR_UIF` dans `cppmain.cpp` |
| **P0** | Supprimer toute prise de sémaphore SPI en ISR : mailbox phiE + envoi par le propriétaire du bus moteur | §7.3, §8.3 | **Fait** (`56e282e`) — `tryTakeSemaphoreFromISR()` atomique dans `SPIPort` + mailbox sans verrou `pendingExtPhiE`/`hasPendingExtPhiE` chaînée dans `TMC4671::endSpiTransfer()` |
| **P0** | Abort réel des transferts IT/DMA et destruction sûre des encodeurs | §7.4, §8.5 | **Fait** (`a835da7`) — `HAL_SPI_Abort()` + `HAL_SPI_DMAStop()` et `current_device = nullptr` avant les callbacks dans `SPIPort` |
| **P1** | `EncoderSSI` : surcharger `triggerRead()`, `getPosAbs()` passif, une seule mise à jour PLL par trame (ancien point 1 du §6) | §3.1 Rév. | **Fait** (`3679442`) — `getPosAbs()` passif + `triggerRead()` surchargé dans `EncoderSSI` |
| **P1** | `EncoderTW39` : supprimer le thread et le sémaphore bloquant, s'aligner sur `triggerRead()` (ancien point 2 du §6) | §3.1 Rév. | **Fait** (`9904220`) — thread/sémaphores supprimés, `getPosAbs()` passif, `triggerRead()` + `spiTxRxCompleted()` DMA non bloquant + `abortTransfer()` dans le destructeur |
| **P1** | Borner `f_n ≤ 0,05·f_s` dans `Encoder::updateState()` et les drivers CAN, **avant** toute baisse de cadence | §7.2, §5.1 Rév. | **Fait** (`dd8a41f`) — garde de stabilité Euler `f_n ≤ 0,05 / dt` directement dans `KinematicPLL::update()` (couvre tous les encodeurs et drivers CAN/TMC) |
| **P1** | `MAX_RPS` paramétrable, gating découplé de la saturation | §7.5 | **Fait** (`2580f62`) — `KinematicPLL::setVelocityLimits(max_rps_limit, gating_rps_limit)` avec `max_rps` et `gating_rps` indépendants |
| **P1** | `reanchor()` systématique dans `setPos()` des encodeurs externes | §5.1 Rév., §8.5 | **Fait** (`ce6791e`) — `pll.reanchor(getPos_f())` ajouté dans `setPos()` de `EncoderBissC`, `EncoderLocal`, `EncoderSSI`, `EncoderTW39`, `MtEncoderSPI` |
| **P2** | Passage SPI IT → DMA, chaîne ISR sans tâche, décimation entière (FOC à 4/5 kHz exacts) | §8.2, §8.4 | À faire |
| **P2** | Cadence pilotée par les consommateurs, timer arrêté sans encodeur (ancien point 3 du §6, désormais conditionné au P1 `f_n`) | §4.1 Rév., §8.4 | **Fait** (`cdba3c7`) — `TIM_ENC` arrêté sans encodeur actif (`updateTimerState()`), plafonnement à 2 kHz (`scaler >= 5`) sans adaptateur FOC externe |
| **P2** | Lecture atomique `CNT` / overflow dans `EncoderLocal` | §3.1 Rév. | **Fait** (`e3596ec`) — boucle de double lecture cohérente `(pos, CNT)` dans `EncoderLocal::getPos()` |
| **P2** | Extrapolation phiE basée sur l'âge réel de l'échantillon | §7.7, §8.3 | **Fait** (`7761ef7`) — `Encoder::getLastUpdateTime()` + extrapolation `(micros() - last_update_time + 15 µs)` bornée à 1 ms dans `ExternalEncoderAdapter::update()` |
| **P3** | Supprimer le `timerElapsed()` mort de `TMC4671` (l'héritage `TimerHandler` est déjà inactif) ; utiliser `TIM_ENC_ARR` dans `EncoderManager::init()` (ancien point 4 du §6) | §3.2 Rév., §7.7 | **Fait** (`1411542`) — `TMC4671::timerElapsed()` supprimé + `TIM_ENC_ARR` (défaut `99` pour 100 µs exacts) dans `EncoderManager::init()` |
| **P3** | Vérifier le *high-water mark* de la pile `ENCMGR` (128 mots) | §4.1 Rév. | À faire (mesure cible) |

**Validation recommandée** avant et après les correctifs :
- GPIO toggle dans l'ISR timer, dans l'ISR de fin SPI et à l'écriture `0x1C`, mesurés à l'analyseur logique (cadence et jitter réels) ;
- compteur de resets PLL (`dt > 0.05`) et de glitches de gating exposé en commande CDC ;
- trace SystemView pour la charge IRQ et les changements de contexte.


---

<!-- Définitions des images de la Partie 2 (export d'origine) -->

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADoAAAAaCAYAAADmF08eAAACBElEQVR4Xu2XvytGURjHH2EQkhIjymYwmIhJRosog51BVjK9ZTMZmCiZ2PwLTIqVDBhICkUpIvnxfDvPk+c+76Vb3td7u91PfXvP/Z5z7nu+zo/3IMrJySSfrG5vZo0ZCkHvfEXWQMgP+exydT+BthPeTDPTrAKFQWPwV5HaeHoptG3zFWkGA7ZlqNN4cWxQtF/qwSwumudJCgFOjWcZE72yHqQ8FGmRUhCqKsaLW5Zoh2DjFOpX5bnfNkojo6xlb9L3CXziKwTdny2+Iq3glPWzqeistvoKKv3+nKXSvi/CMGvFm4Y5Cl9+4CuYJ9axN/9I2YLiMKnxpkNntcl4WAHwpoxXCsoStI+15c0YChQGsGs8//uJZ5zc6t+yblib8lzHapCyhtE6rBrFBkX/dtYahbagnvVC4fDDlqsV/1cwm/rFSYUBgxF5BpjdeykDhLWXDWyPdylrWAUB4oLiHevG16AI1yzlAYr+8WPpoeIQSbSDzgLuw/DOjAcwyAvnoR1CJg2qWwN6pjBzjcZTPUr7iuCD6qCx7JIGRTD0q6ZwkcHBh/62b8VBUNyWFNyy9L8hP1j8Ri+YZ63TPa+8yec5a9D4e6b87+gevWQdUvGFY5vCcr9mzVMIpweTCu9YYh1RWJ4d6CjsUzgTcFhh1iuGX7qZBHsLSw4nur8j5+Tk5ET4AsBzoDPpSvAsAAAAAElFTkSuQmCC>
[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABkAAAAaCAYAAABCfffNAAABHklEQVR4XmNgGAVUBP+BWBtdkJoghwFiyWt0CWoCkAX/oLQKmhwuAFIbgS6IC2QAcQMDRANI4xMUWezAmAGiVhxdAhcAKUZmg7Aikhg2MJ8BVR9eAHJ9MxI/hgGi+TaSGDIIgeKfQPweynZGUYEFgAxkxCKGLShA6kCGhjJA5KdC+VbIitBBEBBPQBdkQKS06+gSUACLDxF0CWwAlJrQfQEDMN+IoUswkBAfrkA8BV0QCZQzQAw6hS4BBF+B+Cq6IDYAijgWdEE0APMNP5IYyOcgsXQkMazAEoiXowtiAQ0MEAMPIImh5w8QH2uGBPkC5kpiMSdYJwODL5QPAiBfvYOyUYA+A6YBxOD1IM1QACrfQGJ3kMRGwSgYBYMNAAD/jVRj+4BjiQAAAABJRU5ErkJggg==>
[image3]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAALAAAAAaCAYAAAAXMNbWAAAFIElEQVR4Xu2b28ttUxTAh1CuuSdR53NJueUUjpQHuRSJB+dF+QOOB0pO8ajIizw48oT6Og9ySXlAefDwhUJEiegc6tDBgxBFSS7zZ65hjz32nGvNfVtrnc781WjvNea315p7zjHHGHPM/YlUKpVKpbIKLgxyqldWirg6yNFeWcD5QY7wyqEZXYcK2BHkTa80nBXkiyC7fcOIYNxfCPJGkCNdWx/8HOQkr+yAz4wKBvGfIH817w8FTpDY5xyPB9nXvN8M8qxpGwvnSPwOxwbZCPL3VGs/dI2jB2N/2VwfJfHzpbLyeWDVc2MGc6N5fygY8f4gN3tlw06ZnpQr3PUYIHTTp0uMjuuLzHVfvBTkCa/M8FSQc831riBvmWu4S+J34dXyUZDbnG5ptkn0AMrxjW7MnCl5g1TDuNPo1IDxNmPhqyAHnY4+Puh0fXC65MfTQ5S2fC+zDu8Difc7xen3SkzrDns2JQ5SildldjLUI4zFgPG69Odao9NQPIQBA8++0Ssd5wV51FzTZ3J3D/fycwC5OVuYh73CgCfGS/jVNQb+lBi6UjBwnznd642eAR8D9M9PMKkDOh92++Jtad8QA8ZqN3zYiK8A4XX5HtzPc6tXLAMrndwnx5aUrcohoF+kBR48Gm13OP1vjX4s0BefPjAf6IfIgQHPyji1UVJ90Gjn52DldE2oViZShlIK+ejTc0pXSUd3zalcit0xbZsyud8zjY5oMgYIw/TnQ5n+3kSVrjlZJ2yu2p7PhrkkOmh0OdE3rBI81T1Ot09mw/LXEsMEXCdxha59ZXWgISqVz/4qs5OgXlk3daQR9za6UqjUsGBKpY37Je1p0VmvfIvEstpVRrcIpwX5xSsTdBnwASlLwbhH231WAp5KDRN0A+HTBb/jXPvKKoDn5wzYGwG81+gtt0s6R8uxSgNmJ05/7N7imkbHq8X/3bw8FuQG6U4NoM2AsY/PvTKBVjPmGduFwLNaGHQ/WBdIrPkp8xa8YZEU4oz/PpmnLYVA/5y51jToEaODLSkLh+tADdiCcfzhdGdLTCuWhRRwWQMmWvvFlYIIzj3WHqV/lOncltMRewqkE2/rw3gtLYO8IzHPJDwNAX1L5ebUJDEQBcP1UQT4PKnIRpAXJeajfaGbHIUx5Hqb0QGbOj3x+kRiefCYSXMxpQbctonziytHSf67PcjHQfYEuc+1FaNlpd+bVyaZV4TO8upPZrYkDv5rMkk5ujZc6yJXRrtbJqGOvtFH/2MV9eAMJEZDJYY0oy9wCjxfox07e04OPQck5u8ssJMlfqYkB/WUGjBhH7vwsNBTeo86Pbs4U2g7+5BXbMM86GkV8qXEhzOhqntg8qf/gx7D2XD6IXhe8kXxTyX2lY1L6scxRBLaH5Ll8stlIMTqWF/s2hTaMG5OHS3HNW05YTNuKTVgPmsPVhR/dOzxz7diT0MVbXvXN6wTm/9y7p3qWJ8woNqfedmSGEnUQ48Rm/9S/ivJP3OUGHDbUXIqBVsUTYEulfg8Ki29wIN0Z8lpDde+DNc330n+xzxtMHDkaFRhdNKenDSPAkptuhnFgC+X6Q31PJQYMGmU3+iCPzpeFsZbvTn5fW8pKF/wpub9lUG+leENeBEPSg5pPcoPEhfCUKlEjvdl8is10g2qRoscw/qQnvqdBdE152XJfVdpZPzu+ZsgPwW53rUdlnT9oL3SDXl2zkhLjo4rS1L/pWhxLpPZKo1CVOJfhyqVSqVSqVQqlcrA/AuvLk7qL7aG1wAAAABJRU5ErkJggg==>
[image4]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAbCAYAAABFuB6DAAAAoElEQVR4XmNgGJpgFRC/AuKHQDwdTQ4O9gDxLiT+fyDmROKDQQxUAhmA+L7IAoxQwSnIglCxcmSBBqigIJIYiI2h8B8UIwMXBohCEA0GPFABXBhuC0ynH0wACmAK4aAVKoDNfSieWwgVBPkcBiyhYiAaDmAKkcF1IP6JJsYQzYCqUBjKl0cSAwNQFCFb/R6IgxHSqCCIAeFLLTS5UTAQAADRbi2GVIRBwwAAAABJRU5ErkJggg==>
[image5]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABEAAAAYCAYAAAAcYhYyAAAA6klEQVR4XmNgGAW4gDcQPyICz4JiZog2VPAWiD8CsQ4QM0LFfgLxfyDmhPIFgHgrVAwDcADxVzQxkEEgxSDD0QG6WjCIBmJLNDFjBoghk9DEQRYeQBMDg+foAkAwnwFiiCaauAwQl6OJ4QQgJ2P1O7EAX3gQDXCFB0kgnQFiSBC6BCngKgPEEF50CVIAyACKAlWEAWLAYXQJNPAAiPMYcAR+MwPEEFACxAVAYeYLZYPUghIggyeUgwujg2QGhJwamhzRgIcBYvtsBuyWEARzgPgJlM0CxCeQ5IgG/AyQIgJUtlxnQBQbwwkAANFdPO+7Oop2AAAAAElFTkSuQmCC>
[image6]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABsAAAAaCAYAAABGiCfwAAABUUlEQVR4Xu2UMUsDQRCFJ6BFGiWNYqnYWdgFArb+ASsLf4CFvWh1oPYiqdIHwcbSNmCnjZ1FCFjYiQiCghbqTGYmmXtn9lJZyH3wuNs3szs7e8sRVfwB36w1NEuYY92xjjGQYo+02BMGEsicF3s/YPXGoTRS6MueqxD7jSZpbs3GSzYuZZeVsbZJJzzmokWkgORlwfNi8kwSdyTvouXgIaekOTPB2zAvWUy6OQrjHdJJ/eAhEn8G78z8Bvg54rlHT7QIvrBCGjsE/8H8iWyRHgniN/MeA8wJaazL6gSJ9xryCsjtw64c724B/IH5Ee82A3/EJquNZmCfdIEb8N9Y7+Cdk+bGC5PjgxJBw7ubD54Uuw5jQXIuwBvRIt1NGRnpQr3g3cLYb++kzzHsync9rerDmXr8/ouatVjsPMc6FReaRpcy2bgy75PGm6ioqPjv/ACWyWhuUdXC5gAAAABJRU5ErkJggg==>
[image7]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIwAAAAeCAYAAAD+bvZ2AAAEYElEQVR4Xu2bTahVVRTHl5gDsUjUzEgRHQiC5CAaBIYgBoUIIoKCBs0UjaBAwpoIKQSRRDhyIhEhyiMKNRoIXhDSDCoHOhCFZ1ihkUKooJG2fu61POttz333Cqbn3Lt/8Ofsr7vfeW+vt/baH1ek8KiYp7qjWp1XBN5UrVU9q9o9tqowbJxW7VPdziuM2arfQ/6fkC4MGXMkeZYnJHmZ18ZW3+WiaomlZ0hqVxhS8C7OAdW/IQ9PylgDeV31d8gXhoiZqnUhP1mScbwSyvA+GNEe01XVSKjvxSrV83lhoZ2cyAuU71Q3Q/5z1Zchj0ERJPfLi6qVeWGhfTyt2pQXSuVlXrY83sQHHEP509L9UgxmQMC7rOmi31TXrd1GqQaclRKGBi+ozlv6iGq6pKkHjwQ/29MNZrnqKWv3nKTpbZJqg6TAu9Bg3Iv00iJrf0N1STXN8kAw7MYx39KxrGNPNxiW5vTphkSaNsek3tMVBoxoHCy538vKMARwg1lheTwMbWOchDE9NCao3skLAwtUP+aFhf8djOOsaovqZCgfVX0gyZNQf0V1S5LRHFLtV00xMaWRZ4wfGh1J81w3mGtxb2woFR4d0Zs0BgKsXlE5bTAYfoG2QyDp+x396nHhATLxUGPYKVXg5bCJtDArw2AKBfkjyxON100/o1m+cD/5CqgteiDyDxA45SeqxDd1B2ZtpE1TUiPBYGJsclaqzSLw+AX8kIwgmK1s/pjkn5EUsROh/2BtgWntnGq9VJtU9HdY0jKPOpaELAHpxwM8t3x+ntd9pPpWqn7q+m4avH88RxoI/pI0IJyO8mSK8gEjluH56r3WldFwVA8M8k9V9d06lnAoei9iJQaf9hgJ0Jenvc6JhkzdqKRdy23Sve+mwU5vfM86OJxsFb47iL6yss2hbJmVOW4wDoMcB8sH+iVLR7G9PTfkj9tnoJfBxLpufTeJ6B3fyuoc4kX+YQeafg1msaVzptqTNkxjey2fG8V4BtOt7ybRsed26f6ueMbP8sJBo1+DAYJntqmdTyS198M12o1Y+l2rA59yOEiD3GCgru+mwL7Jdkv77xLPcnjv6B1jzDhQuLG4GMS6tBvNRNVlSSewfi+Edt9IuhNC3EQb4A97y8q5++q7yzvsiWJwW9d3UziY5T+V9P45dWWFIYPA/MOszL1MXDENRfxS6A3eEQPJ4Wsj0aMMRfxSGB+2Gr6X+y9LIbwLBuPfS7ogaS+JeIcyPBP1xHesqjy249sEwCoQQ8R7sQvv/WyVdCrdsTzTIeEAy3WuN3wh6XtOv1p9PPUuPGbci4wn30V/Q9KVkY8tDx6jsdvu+1Nexmc7krYiuFlH/pokYwHqgBjRFx1+TxiD4YJWR/WLtHDvp1DPeAYTL0PNlWqlOWrPjj0xEjcYX1ni+fw+E14Kb1ZoObskeQ2eeA7kZSzJZ6lOqb6WZBAcwbwv1cbqadXbqqOSLlSdkeqzsFTS0Q0xVqFQKDSQ/wBemmduxZU8WgAAAABJRU5ErkJggg==>
[image8]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACQAAAAbCAYAAAAULC3gAAABuElEQVR4Xu2WrUsFQRTFr0FQFAzPIgazwWQQ4WHSIOIHaHjJ7H9gFcQqZsFgEsUqGAyCYLGJFqNBk5osgh/3MDO78w4zwz7dhQfuDw7unDtv9jpzZ2dEav4hvaoZNgNMielbKXjBJ5sJ3lRDbJYJkmmwmWBQ9c1mWWyqztn0+FIds6mcqPbYLAP8p6NsWtxMzHNAGZYKZmlS0oMuiYkPcMCC2CybjivVIwnskrdhfXCquvfajgXVmupOzEvxvNLWw4B3XrDpwLS2xAwAHVgf2xR1cC1m4HHrg2fVodd2oB+EcW7sM5JkdlTvbDJzkic1pnoQs01DoA+KOoSrn+iSKIuSXvKMfcmTgnrawxmIYdAQrn76OOBROCGAmUHn2OyAVEKXYpY6ReGEsFT+DMUKL7VkiJ2xSRRKCJ90dFr1nl2beZFwUfP3B+2jPJyRLOplyXfGrarf+lvWg9ZtH0ds24+I6Y+/4FXCdYhtH51FZPok5lvjDwYfZxV8DOxP8TS1fbDMiH1I/HRHvMnmX8GgsaMjRSVHB9iW9OEaA4crflsJv7l+dHJ/6piuu6CBolfYCYkXeU1NafwAIzBzZpO4+AwAAAAASUVORK5CYII=>
[image9]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACIAAAAaCAYAAADSbo4CAAABy0lEQVR4Xu2WvS9EQRTFr6AQEgWJRkJ0iEohREdLwUok/gC9xh8gegWNSEQneolCp6QVDZ1OQqUQ8XFPZmb37tl5Yx6re7/khDl3Pu987BOpaB+9qik2I8youtnMoUN1qrpQdVIs0K96YTMB6qJNNsOqL1WPalT12RRtgDplVtknrk0W6BiVJ42H8rgpgyPVPnkWtFlnUzlT7bEZ40H1SB463TZlbBs8rDDGtLj4EAeUQcnICrKASvPG6/Kenciy6t2UmWNJD4bYIpuWW2ntAFsCb8N416pzUw7UvN7EHUz8v9BUw3GlumTTggF5W5AJPiPIhs0QwHZh4DVx9Q98ec5W8uyqXtkMjInr4EZ1aIRBOUsoL5EXCOcDZ6EItOU+62xJ68pBLEvwMGCMn84HSE7kRFwQKQ7Meg9/LamJIOU4aymyJmK5E3fwmKKtCdd6kwNEciK4FTY44MsjxgvgpeXDCvj9QDn2qCUPK55zuzW4fquNcBNF19euFP08m5gF1zfWvs6KuI6gCYpZsMqiB+1JXPt7DhgQt4/mrwmvbdETnyLriS8DfrxSP3pFoN0Om38h3JCynwEfbLaDf/8wKkPupyLqlMleRUWUbxUObi6lVCZ7AAAAAElFTkSuQmCC>
[image10]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACUAAAAaCAYAAAAwspV7AAABtklEQVR4Xu2WzytFQRTHj4VSlAUbsbJQipWFlCxEkcjiLf0Fdlb+ANn7F6xEslMWFq8sbdkoZWchlLJRfpxvc+fd8TVz8Loji/upb/fO98ybe+7MeXNHpOZv6VTNsBlhUlzf7OAhr2waPKp62awaJNTHpkGP6p3NKtlUnbAZ8KbaZ1M5UO2wWRV440E2C/yMLHJA6ZdMszUh9sAr4uLdHChAbI7NkA7VBpsBI6pz8g5Vl+SBJVVDdSHuwbhf/dTDcaY6ZTOkqVpjM+BZ3AMw7Z5b1W7Q9iAJCP3xIrhHosy2uHGj4O95xyaBPngI6sSDNgo9hq8na3mWxVh+ZDxGHv7mo+TxAGhj4Bi+nro4EGAmhWUIGZavSwVuqG0l1RS3HViYSXEAnXlA1NsCedbyIXbMJvFtUmGtXKmug7avJ+Ze4oXO+xPae2W4hVnoGByDPBVXLCeuEGoL1/lW75LUljAg7je4ggdxWw6DLSE5m0NSJnFUeOuBN1t4zJTEZxBg/0HsRdKnAsSn2awCDJz6zFhk+8yALbE/yCnwQcZvs9HO0eU356+2+JeHPPDT4/C4pAu/piYbH8/5ZwRdzFHBAAAAAElFTkSuQmCC>
[image11]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACUAAAAaCAYAAAAwspV7AAABxUlEQVR4Xu2WMShFURzG/4oiysBAqVeyiE0prBaLhY1ks1mYTBabAaOUDDajshhuWcRsNJAy2JQBA/+v8z8cX/ccL533MtxffXXv99173jnv/M85V6SiuUypOtgsYZ2NRjGn2mczQpvqhc3cDKhe2fyFWdUVmznBqEfZNIZVH6puDsQNZITNHKAz+NEYhxLPV1QPbOagUJ2wGYB/8ZZNo0tch9s5iLGmulftqlooC0Gj0+ShkOdNyI/tuhY+ZCCfYZNZEvcgihdM2j1GBXZUnXbdalm/3Xt6xXViQ1y+bPfwmUfVEZshWNZopIf8d9WlXb8FPjoTdphJ1ZOnUF2w6cEUoYFrDsR5yFDUm4E/aH4M1NMNm0ShumPTsyDl9QEKcdkz+ZjiWKf8ILHCUhSS6BTmFY34egkpxGXj5Kemb0xcVlZHIYUkpm9L4qPG6inLsJThc6EDrqcDKd9Ak4WOgxSNDJGPo+DUMkzJ6s84OeX+fKtJ/FyMvf9Fn7iHvM7ke496Mm/b7j1YlWWbJ8rAt7NHmcdvnthasjIh5VNbDw07ZgAO1tiBnALvcblkAzXzl0+XczZz8+8+8jz1fg4vslFR0Qw+AYwbZ7HUworDAAAAAElFTkSuQmCC>
[image12]: <data:image/png;base64,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>
[image13]: <data:image/png;base64,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>
[image14]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA8AAAAbCAYAAACjkdXHAAAAy0lEQVR4XmNgGNaAEYiF0QWJBYeA+C26IDFABYj/A/E/dAliwE8GiGYQJgnEAHEGEH9lIFEzKJD+QtkPGSCauRHS+MEqINaGsg8wQDSLw2XxAFC0XEHiL2SAaDZGEsMJngExKxK/nAGi2RdJDCtwZYD49RESfscA0QwyBC84wICIGnRchVCGCRQYIJrRgRIDRPMadAlkAEoQLOiCQCDCANF8FV0CBEBxeooBElDYgCADRPMndIk5UAkYBqUmZIDubxCOQFExCkYBLgAAkl01Nqc4nugAAAAASUVORK5CYII=>
[image15]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACcAAAAaCAYAAAA0R0VGAAAB4UlEQVR4Xu2WP0sDQRDFR1RQFEQQVBACFlZ2oiBaWmihjRai38BeQUEQrGxsFTsrG1s7iysFP4WtIGJhIYI6j9vJTV52kxBzkCI/GJJ5s3/mdnb3TqRHd7CiNsxihGMWymZb7YbFBBW1ZxbLYkbti8UmXKmds1gGn2rzLAbW1H7V+knvC3or26BtkBQmSZFJOv6gdsdiJ8nU7ll0ILFHFgMLkk68Cpb8Se00omMA41ptw/kAg6N0nhG1HbU9yeMXwZ/wjaQo7STpVQbU3sN/NNx1sUztO/zHAIhjfxnoC23aaQCnEclcSjEmfCTNIL7JooGaj6mNSv0qwMe+MHC63pyPpNAGfWNkaj8sEnjYExaN9fB7JvlEWGowHnyfLJLx+2tWGu8ZfrgYSO6WRQbl8xfjvuSDo3QGEl11Pu63VHKxSsRomhz2Agbaclom9SXBofE0KivGQmyIA0TDsgKbxG9sdHpxPsrNyWFi7mdkUruq8H0VDLRJHghgp872Hw4IfP9Kwl0VW6FU6fBg9nBLakcuZthVwldMHYuSN4R9SN7x1WlzRdMasJqxS3hKir6HFDNauoT/w7K0P0Hpry+A8qde/CmspIMc6DQVae+TKbYPS6FrPzaNVj/TD1jo0aMb+APNH2r+e9hamgAAAABJRU5ErkJggg==>
[image16]: <data:image/png;base64,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>
[image17]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAaCAYAAAC+aNwHAAAAn0lEQVR4XmNgGAXoYCkQ/ycS4wX4FIUD8Vd0QWQgwgDRfABNHAZ4gPgwuiAySGeAGOCHJMbIANEIAoJAvBBJDgNcZYAYANMAAhFAbANlMwMxB5IcBkD3Px+Uz4IkhhPA/I+O/yErwgew+V8GiLci8fECmP95kcQ8gdgFiY8XoPufJEAo/gmCcgaIAUHoEoTATgbMUAfh5ciKRsEooCkAAHrmL32+D4CKAAAAAElFTkSuQmCC>
[image18]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFIAAAAaCAYAAAAkJwuaAAACHUlEQVR4Xu2XP0hcQRDGR1RQNIooSsA2hbWtaSSFFoqKohArG5t0FhEblWBjESysxMZCBFHEQtDuQLCxUIs0wUJsrMQqTQST+dgdbm7u7/PAt3j7g497M7Pv3ey+3dl9RO+fReswLFtHJJ9u1j/rVLSTi3+3AUUTq946a43f5Aaq1QY8GdYKlR7sB1afddYSo5SdkR9NDGBwZSaizTcV05Qa5KLskruxEoXOsf9Frv064DlV1z8ov0833ifqyQ1XRqnBmmb9sc7A2GbV+WvkOqJioJnckhbQFv2dVT6w5vUqusg9NGP8ApbEuXUGBPL7y7r3Ql+WcloQ7VN2oIVNyp88VdXHeXIPRI0R8KdSsDtYOyoWGtfGzrAOlN3A2lC2ILNyRvnswCbiF7kH6J0ODx/w1zgK4EiQFujwArnZtmpin1mfjA8vHX0S9lhTrMkCumK9+HbYoB799aH/TYStj23exptMm5/kcun09iBlS5DkrXN/Uj4MkMy6chrCzcwz645cTU2E1EcreUuVgg5vJdBXd1tJ0A65NCofdtJLZQdDofrYyzpRdhpgRiCvjLdRXrA87cAGg9THD8o3zPqi7DTA8QV5HbHWWWOUbp0uiyzlakm6tOfcbUXBFwjywokheMqdH9MEqwK5FfpmHreOtJG3PmEDASC7LY43mgsKKN8zyt2hRThvhUQL5eZ3S4FuNJFIJBKJRCKRSOSN+Q9/xJW7VedzSQAAAABJRU5ErkJggg==>
[image19]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAD0AAAAaCAYAAAAEy1RnAAABw0lEQVR4Xu2WzSsGURTGj7JQlERJKbKwlzUrFjaUlbLzB1iwUFY2trKwkpJkI3+BpFmzJRsWNjaKEsorH+fp3GvunJl5X7d0m1f3V0/Nfc7MmXvP/ZghingzzHpmPbAOVezf8sXqN9evrLU01PxMsm61STLLfeYag153Yk0PZvRFmw4DJPe06EBA2kkKj35A49mwH5uUJipihrXP2tKBgHSS9A/Ft3ywlp32r+lg3bPeqXzQlnNWos1AXLAulTdNjftcyBWrm2Q/6wSoas1p71H+nlDgvXi/Cw5Y+KPKr8sIa9dcJyQJMPOWCeO1mjYqjYqHppekHzvKxwELf1X5dfmk9GA6IElgT2rLCeuUpCjX2VAwMJPo24ry7aCPlF8KEsw6bXyKvJdKCRusbQ/Ny2OlNBp0ovxCsFwflbdEksAtRFX4k0EnJDcXyWt/BMIOTv8YWV8fcDkGqbgyQ+S5P+rgu7wX5LFScO4UDc6e3lPKz/FG6Wns0kOSQH8LqwK2o+4bBos+tyn/B1TrjHWnA4YukgRPOlARxkj65/4G37COnXYGfN/cfav/sfW+huYyd1SDRUqXeY1kEiORSCQSiUSqzzckiX2gUNUGHAAAAABJRU5ErkJggg==>
[image20]: <data:image/png;base64,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>
[image21]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKYAAAAaCAYAAAA9gCd5AAADyklEQVR4Xu2aTahNURTHl1A+Q4lEoUzMlFBKvWJAIvmImChmSCgfvcEbKSYGFGUiA4WZTA12mShGMhIDEkUYEeVr/9/e65511jvn3nPO3e+907V+9e/tvda+79299rp7r7PvIzIMwzAMY0CZrg1GchZqg9GdT15/vWZrh9EB8amiO/wCxRkK/sPKbpTw1Wu710wKgZuXdxuC1RRidEs7Iq+9zmmj54LX09jGTySp0QUc36tEf4rXDtE38lylkJhrtSOCpFynjRQSUzKi+obRF58pJCY+wMwc0b/otUT4jBpc8XqrBLYo291oNzKQlN+U7Z1ozxdtHWPEvchudX1kg9deyor159G+1Os9hZoT/qFoNwJF9eVBrw+iL0EM+eiHNkX70dg/FsdMi3YjsoiyoO3xuhnb8piqgtOGSeQ6hXlU1ebwskrIJJOCvRsvKIz7TaGmR/tsbsTE4LShzSAhZZDLnsh5h52rHZSfMD79JyiM7Zfz1K4HsaL6EldD2El7gaTkGD9SPgYn1gOvx9pRk6ler7SRxm+drlH5w2Bf8E6pgy5BgV82Caf6O6m/4K7x2uf1htqVmJj/d2XD9VAVeKfstVs6r93aWINLlJUcGqf6/a7TtijU3MkTkwP2J/5EfVkEJsH3cBpX0D+kbE1okph1j/Kqv7+ovqyD/PBD6/PuDvAt0MYGVElM9FOsU/LExO6IIwZBQ5uDhr7GUZgEjgnUTPKogE/CwV3hdc/rWc5bnSaJOV70ur/sxnEKr8UmgN2S46y/Apan0kkKu9+RzF2LKomZap2SJeYQhZoRu+MPCrUN2EVZ0IbjGAa2xV6nvDbGPuNEm4OL43i5132vJ8JfhzYlJoKPeZWVOhrUcIgfnrzxutPCjpsP2LApYAxuSQCfSpcp/J2fXgeiry69EjPlOiVLTOf1hcIdGt4cLz7s6MPOwQM8CbzxGdEmcaKN4GLsCI1dRC62y7Q1GzpKGxJTv0fWSjmoAI4ZYolYoy3tH6MP7dvR52JfxwG8pLHvQWpWNnQU/nsSJ9op1ylZYtZF1peoRfWTu1NtHPkYUxScOrQhMScSxAtH635qvnsxRbF3qp1qnSYtMXG9wU+KmAS+qXAdb74NP66UMIYn3Ouer4z/KTFlvPCki5ijdKpyFVVEUbI50U65TpOWmKh18J9H4AaFY2VZ5u5MGPUT6iYG/0qHkkAfFb3AJPXxMeigbn8Y23go+kXNdk1H+bjhwy19INU6YdNo9To5bTBaidOGQcdpg9FKnDYMOk4bjFbitMEwDMMwDCMR/wDEDED6E0zO/wAAAABJRU5ErkJggg==>
[image22]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAaCAYAAAC+aNwHAAABBklEQVR4XmNgGPYgBohvQLEHmhxecAqI/wOxFpTPiCQmAVOEDXAzQBQtRpeAgisMEHmQgRiAlQEiCbIJF9BkgKgpR5cAga8MeEyHAh4GiJqH6BIRUIkp6BJoAGYAyDIU8BMqwY8ugQb0GbC4gAMqCMKEQCsDRN0kZEElqOBdZEEcAOZSEWRBGajgAWRBLECWAaIOFJUoABb3V9El0MBrBjyx9J4BEQaggPwGxHOBeDNUbBZUHmcqBGkCKQApRHbJViDeCJUDuRQv4ATivwwQxaeB+B2UHYmkhqAh6GAPA2ouPIPEJgocYIC4AuYaUIolCYAyDUgjCN9CkyMamACxFLrgKKAiAADH+kIonnzszwAAAABJRU5ErkJggg==>
[image23]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAbCAYAAAB1NA+iAAAA4ElEQVR4XmNgGAWjgMqAGYg/AvF5IL4DxDVA/AiIXZAV4QKsQPwfiIORxJ5AxTiQxHCC10D8E03sAAPEAIJAkQGisAFNHCS2B00MK4hmgChG9ytIrBVNDCtYyIDpVyWoGIgmCOYwYPoVZDO6WDgDJHbmAvF0ZAlYGLBA+QZQ/nO4CgaGMiCeD2WDXHwdSQ4MQKaDNIFwKZSG+Z8RyueB8gkCTQZU/8P4RINJDKgaQDZ/Q+KDwDE0PhiIAHEIEP+DYhAblEJBIBOInwLxQyCeAhXDACoMEE06QOwLZY8CegAAbLUxxr+YcbEAAAAASUVORK5CYII=>
[image24]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAAaCAYAAAAHfFpPAAACpklEQVR4Xu2Xz8tOURDHv0IRESm/N6xIUURKsrBgwRaxlY29kq0/gKyUIqUsJZIsHrEQlkSiKDshC0ry43yaMz1jnnvf532fhXK7n5qec2fm3nPOnJlzziP19PRMwbEiL6vsS7ZO87jI7yIb6/OsoFvhTl1kgWySV7Oh8kxmJyCdY65scqx0GxtkPqeyoQt81fjVXSjzeZcN/zuHZRO7kA0JDwDB6hTfZRNbnA2JzepgBsyTTQoZx1mZ3/lsqGwqsjw8Lw3tf8FuWZbOiHWySb3JhgY8U5Yl/b0ir2XBhGtFrhc5UGR/kQ9FfspKDY7KvsOpsqPqjlfdxSKrNSxLfBx8yb4vRfYEvfNRtkgzYo2so0HSZ9ZqdEBwV80nB34EAHZpNMPuFHmRdA/T8xnZeweDbmWRreHZIfgE4Ec2jMPP/ufZkGAV8YunBHsGupj2DoP0AAB+ceDfqs6Zr9HbJsftIZkfxzS0BeCkzIbvkmQby2cNB8OkGNylIjerjrTEnm+BJ6q+CQLlJQEEeFDb9OFZsbfq+FY+gv2+8UrDEm0LANd14IRq26Na8ZVkojETbhW5UW1kSoYBtgUgw+q6r0/sUZEntf2g/kbcb47sXf6bNAWA7CEDgHd+Bdu04SNsVHTEoD7V9pHgk4PA6k0VgC2hzeriy23yftV5FjBB3yAj8cbJPoDveo0GgMlfli0gv/jljXoi2N1jXT4NbYfOdmal7HRBIu+L3Nbf3+T9c7IgZPKVm42V1c0B8PR3KINxl7ppMZAN0LOhaZW2y2yzk566zfjxF2udkmP3biIHwLMoBoDMPR2eYeIyyHiNI00TchbJyudtkSuyO0ETrDJZECEb2AAjnB7eb752E3BupEBf7ucnTnw3B2sithVZlZU9PT09PR3gD+LwpXVTzbeZAAAAAElFTkSuQmCC>
[image25]: <data:image/png;base64,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>
[image26]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAG0AAAAaCAYAAAC939IvAAADOklEQVR4Xu2ZS8hNURTHl4EiIu9H9MVAoRhQMpfHACUDIn1lICNFIRmYGJgYyMgrMjAwkpSBwYmJUAyIvpJHMhFKUZTH+ltrdZb1nXPPvefe7z60f7W6Z//X3uex19qPcy5RIpFIJGqwm+2F2sbgS/QZD9h+sy3T8jinzbVKif5gEklgrkaH8pTEjyAm+oDxJAHBiCpjKUmdI9GR6A1fqXoUTSap8yY6Et1nB0kwzkZHwIKGAA8aM9jusD1hm6fa9dzdFBtI2sKGSGYesFW19WxrVMPMdY+kv36yDatuTGc7zHaR7TRJu7f62xTfSU4+NToCK2kwRxqChfteoeW9bEdVa4VNbJdI2t2iPGj7VcOab51uwTJG2D67MoJ2jKTea7YDbC+1jL1FQyaQVGzmAU6S1DsTHR0C2XmuRatKNJtFtgU9U70OaHc/aHH2QcA+uPICknZIfA+0X3qMpDrvfKUsJmmIKFdhI3Km006pBkMmGsgk0x87vdt8oeLgbKZivRmukbS19X8dSXI0wmYpXNcD7WHQKrEMyIIeWUhSD1OAgRtdose2+1yu5bv6a8e+XTexxIm0E7RpJG33afkdjd7ATWH7QVIPa1ejoGVBq8TezZ5FRwBD3WcXuKCa8ZHtCuXnXKs6MrGZDqozPc7627IcTD1F124naADrE2aeiTS602eTnPuy07BR6VjQAG7AHgBrxDeSXc1N1dA58Fd9DUGdomkC73V4wF5gU9mcoO9SvS62VmINWhR8SFz4sNs2fNAyp9cOGgKFxgiOH3HYId1QX9WOZgv9uzvyoP1QFLsEZgZc/73TMDqgtRM0gPa2ifBYQP323Xaw2BD5NQwaXg1qgQfBjgcnwUk/6fFOV6cscAg63jGKGCFZN3sJAvec8kBhdGD0txu021T+degQ5ddDv6Lvjmt5D9sqPUa/wXBcdq6WQHb4r/uP3LGBdchvOrDOGWhvgR6r14S6+KDZdFZlA0FGcrM26uJ6hQx+xbZdbZjyhfYEySg1X9lI7BWdGGl9iT0YDNNcpChDseDa5y5vmTTpC5BE9o/FQWrh09GgsJptfhQHHMwG9g0R9t8FLZFIJBKJRGJM+AOFqPWI5GSRSQAAAABJRU5ErkJggg==>
[image27]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHAAAAAaCAYAAABvj9h3AAADWElEQVR4Xu2YT+hNQRTHj1BE2YiU8jcbShJF2MhC/iz8KcWeJBYWis0r2Us/G6lfFlIoNjayuEtZWJHyJxEbihSK8mc+b2bcc8+b+/786t3n1XzqdN98z8zceXPuzJx7RTKZTCYzJP44W23FzHhwQnwAP1rHf8wK8WPGfjlbXHV35ZyUbR85m1Z1t5nj7LuU9bZV3RVmix/DyGCAv8N1pfHVQd1DVmyIjeLvPzOUuVJe969GPQ+cPVfl/eLbauYFbYnSCNBpVSbAP6UMsO2jMY45a4kPBoN4V/GmWS++7kLraAgm85LRJp39MJplgfhxzzc6WkuVnzh7qsqwV+qDVEi9b+joG8cnaZnSUjBZoxowk8+99xh9X9DnGl1D0FPj5qH9psrUuabKwBaNzsNrKSTd79Bh1Z1X5SPiB/JCaZoDwXjSP4ff2ys1hs9O8WPkqiGg6BuMrnkl6Yl+I6XOrsLvq6W7zaKgnzU6FJLut45dzr6Kb3PK2euqu3/owB7gaJjdHqlHwA6K918O5c26UgOckfRKiAE8bHQNqyw10TGAM6Q8HriPJgbwttGhkHS/KXgw9KLhPNerv2/Yci5aUcqM9Jl1BOIftOdICoJ+ZUDrlU32CqCdeE2vALL99gpgYXQoJN1vCjJbm/xNKYBknXb1RRgMxqFvmZT+BzsMxj2AD8XXJYM9LmUmPRA7nE1YUREniXckC5NgM7QmiYHaUqPb5EajzzqN1mOgLpTuim6TGygk3W8dL8XXj3ar6u4NSQj7fTdi57wTRVixaEeV1o2pbKHL2y3ridmgDVTMQu3Zrbku6YkmC416/I82UPG+NnmCQtL9ptBHD6uPRULbfo6kNpuc3bBigpb4jgul2fc/ynY/bwLGYN8DSQ7sJJLhsXIiayQ9WWj3VJkM2+4yMfudZXQopPPedbCD2aOLtvZIqIXVR4NBjE9FELcpYBCfwu+mIYuzn6440/WXkviw2Ynl/+vkban4Ovr9cWvQ9ETzCnJflTWPpbN+HdRjp4nEFd9PW1krncHpx+7QOMD3UjT28VFyU8rJ4GpXJHxxdtJo08UnEO+d3RXfdlWlhod2+NhKqZ/KB/DjexvsQ9C6rSb8u8M1Wq/MO5PJZDKZTCaTyWQymcy48RcA3xzvNRwtcwAAAABJRU5ErkJggg==>
[image28]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAaCAYAAACO5M0mAAAApElEQVR4XmNgGAW0ANxAvAaIdwAxIxDzAXEkigogmAHE7xkgCliB+D8UKyErKoYKghTAwFKoGByATAAJnEAWBIK3QPwcWSCaAaLQBVkQKtaKLLAQKsiBJAZyF4b75kAFkQHIJHQxBkWoIAuUbwDlo7gPBsIZEMFRCqVR3IcNaDJgcR82MIkBi/uQgQgQhwDxPygGsZEjAA5UGCCSOkDsC2UPTwAAAIwjzQqHl14AAAAASUVORK5CYII=>
[image29]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHoAAAAaCAYAAAB4rUi+AAADoElEQVR4Xu2YS6hOURTHl1BEKeSmlGcmlCSKMJFiwMAjxJyBCAO6UrdkLjER3QxEEiZKMrhDMSXlMSBGigwo5HF+d+91zzrr7vO9cr9P7F+tvrP+e+9z9tlr733W/kQymUwm84/xq7AlXsz8WxyUEOj3vuAvZqGEPmM/CptTLW7ISSnbPipsXLV4mCmFfZGy3vpq8Qi3pKxz2ZV5ThR23IvdhE7+jL+LXFkd1N3txS6xSsLzJ0afX/zlIzXqeVDYc+Nvl9DWMi1qc43GZDpmfPhY2EXjc41muS7lRMB6FugDhQ1ICBodeVspTbNCQt0+X9AlGPRzThss7KvTPLMk9Hum09EGjP+ksKfGh61SnRAbnQ/sDGirnQ5TpceBtp3VWTffaCkYVP+S3YIg8ewtTt8WdQa0DiZHqt9M7s/Gp84V4wOfBnQmOTARUvdCG/Ki9DjQrOLTxt8noTMvjGbZEY2VwxbF9YZKjbFns4Q+8msh8OgrnW55JengvJZSZ5fi+lJZPMzsqPdHn+vUvdD4tnvaDfTkwh5KaMMOc0RCHzqCm/hERF/Ab8vUI7A7JZRfiP4aW6kLMFA8X1eWooHe63QLqzYVHA30BCk/Sz4gGuib0efa7gJK3QRoJ9Ba18LnqqNAs9Wd9aKUGfgzXxDRgfDfuRRMDhKUdqxZ9tws0I0GslmgGeBmgR6K/lgGmtj4fIN2HQWaLNuvZkU7S/LiGZT0i3SL/yHQ5Eh6n6sSjpIdQcZ43osGHUzOmB5ezmek3UQDurZG90maxX6LLVbXgJ4piyu6Jmlcfy+LR/gTgYZdUt5LTY+TLcO2wPeoEXpzzpSKHh/2G60RnWzdC4Zb1qPZrw+oZt19TrewOlJBIOtWXd+xLuvWJNC2saCR9HnaCTQBHW/8TRLa3jVaUzjjXfNiggEJNx8ymj8/4/fiTxP64M/RZMl+4A9L9bu2VEIdn1/4QeRE4XctzfYnRV9zGQ9aaiG0E2gmMfe39EvYeVqG1cwD2zFSfdDtEZj5H+J1t+FISBZqIeew/1zppNT+Kry/TULnSahDIJR1UbM5DKv0vvF15fMZVHS7TTFDQtkpX5DgqIx+/j0ZHfxalsnoILZit2kc4f9wtJdG6wU3JPSD7Z5fv8LhU2GHnMaW+K2wd4XdkdB2caVGgHaUsYVTP5WvTJdQ53E0rvmP3EJ79DfG8FOJnMKq3yOh/xqD1PtlMplMJpPJZDKZTCaTyWRa4Tek/kZ2QmeOkwAAAABJRU5ErkJggg==>
[image30]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAaCAYAAAC+aNwHAAAAoklEQVR4XmNgGAXooAmIHwHxfyh+BuWD8C+o2FO4ajwAZgA64GSAiN9Gl0AGPAwQRQfQxGEAl+Fw4MkAUeCHLgEEggxEGLCHAaIA5BJ00MwAkctAl0AGuGzQZICIL0aXQAbcDAgD0PE3IBZGKMUOQP4GKU5HlyAWHGCAGCCCJk40wOV/ogAvA0TzaXQJYkECA5n+n8OAGeIgzI+saBSMApoCAMOUMYrsnD1KAAAAAElFTkSuQmCC>
[image31]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAANMAAAAaCAYAAAAt4GmlAAAFn0lEQVR4Xu2bW+htQxjAP6Hc7yV5cEkk4kEKUZIHEg8ohDrxgBOp0ynxoBMJJUme5EWSXEqSNw9bSi5v4sWlNrkUSQk5x3V+Zr7/nv3tWbNm7cva//X/z6++2mu+2WvPmjXfZb61tkilUqlUKpXNw622oVLEwU7OsY2V7cuzTq6zjZVimL+rbGNlwkNOvnbyb5DvwjGyL7R9u9F7uFzj5EPbGLhJpucA4fjauFMPXCLT4/g7HJ8U9Xkj0iOvRroSvnTyg0y+/42Td4LuTpn+fe4/x0QlhTEdGx1bTpfpc/wajpH4dw/VL2xF9CItTCTtn1vFwOAa4kWR4lPx/Y62ip75Sfw4DrAKx/lOfnSyn1V04Ezx53/NKgJNawEweH6/jXfFn+Mwq3D8I83nHzxcMBc3Mu1KbnKHwN1OxrYxwWa5Tsbwp210POnkQds4B4+I/43zrMJxkHgdEawJ9KfYRkNuLr8SrzvBKrYCV4q/OFIhC146NzFD4DcnN9tGw3GSdyh9car4cdioQTQgoiyD78X/Riq6XS5et8sqIkZO3rKNEaRwubnU9XSkVWwF3hZ/camQ/LB4Hfn0ECFVYvwYS447pNmh9IlGjYvD8RHio9T+Gz0Wh/PjYFLoWjjeKiLYS+aca845nyZe94lVbBXUU1g0t37BKgaEevo2dL+Ucih9EkcNFiOf9071WAydj/ecXJ+Qkv1M25w2OecDQ/vPpn3LoCE5Jb9LvnKjXG0b5oQ9AeXXUmlL3YCx5W68ote8bnTeWZA3iq+20XZ23GkBNPLtlllDuiXocvsl0D324VYRsOsolsuifjnusw1DQL0faU4OotRHTl60Cpk1JsI8Ho7K07rB4NqMZJ79EpvnUilN0dTjI8eEtgvDMXPfxIniS+ZU0NpYdL8EakypAoLqiPQ5qKxSTMFxpIiNKbf2usB9+MI2LpOR+Itv21PAWNIGYo0Jmm5Y37Tl9zDPfskaTE5KjampyqYGliqVKyMpey7GeZoWMEUF9Ln9EuSMqdQ5AwbTZCA2Mo0lvfZKeUwm25aVoTeqhKYbao0JT5kq7bbRNc27zX8tS0map/ulprSlL5qixp7Q/oxpj0Hf9nxMI1/TAi7ZL0EuzRuJ15U6Zy20WKwxNa29rpRc31wwGZw8l0IosYE84eRRJ5eGY2tMTISWdj928qb45xfroMQbdXEoq4QxpKpsWpFsGqMubrhXvBe+faLe4Gnx/VIeXvfObfslyBUgcuO0qIEwHpzj65EuNqbc2utK6dg6s0P8yUtDMgbyfDjme+TzYI1pLN7jvOzkKJlM2jpoK42T0qAvcSirhAID42iKGvoqzg7TDqRWjP9x8VFtr/jihYUqGuewkQ+0+PCAVSQgnSSKWShWcY62/RKogfCuH8+bPnDyUqSPjSm39rqydGN6TiYeJJbcQ7Sx+D4XmHawxkQ/blxb7t0XLC5b+btHZq8fSXn0VXKXzI4BOSPo2W9xrO/p7QvHMaPQdoVpV+y5EaqEMM9aGMn0Q+VzZfb7SMqgFQyEPjutIhAb01h839Ta+0xmfzeWQyZd/4e2tcMg8PL3y7QHgdiY4pBMyjCvB1kme6TsdaKhwr1hv3SDk/eNbhXwe22vE7UxFp+9nCzp52ixMeXWXlfWbkxEGK0AUTbFm5GWaOoUGxN6TVUwJrxWbuPcB6Q1TGLbi65DRPc6wCMJnk9xv5b16pGFF135Z8GiMGbuC2P9JXyOtxtqTG1rrytrNybC9VPhM+Gfi3tlop4yJnLfs8JncmteaiQvXjf8j6npLxhD5iLxBR7gDYO/ZLXRiXQzlwKWwD46fvP8D5mknYoaU9vaK2Uk0+kf63JTYvdMmxUqRvXPgfPD/PXlGOM0b1sxFGOC+rf1+SBF7vNv69WYKpUlsW2NqVKpVCqVSqWydv4DagO5PXmF19YAAAAASUVORK5CYII=>
[image32]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB0AAAAaCAYAAABLlle3AAAA4ElEQVR4Xu2SsQ4BQRRFbyuICD+h9gkqBd+g4w/0dFqtzm8otvcJOgm9gkLDe3mzVq6ZZGNWs/Ykt9h7J+/O5C1QUXYWoqPo4XR236q7806v0wWTljI1mH/gIJYGbHBCfkroQlEMYUPHHAht/Kh0BxuqL2aWsGzGQSyhl/Rg/paDWOrISlk3USc7Why6Ry2YcuBBL3Bh07EW9dkMkcBKu+QzK9FAdCVff0KV+rlLQ/v0oUO5NCV3aRNWuOcgQCGlE+TfpxJVusHnn6pqvR/yEFX6Lf9RyquYO3/kySoqSsQTlMpNbOd3/qcAAAAASUVORK5CYII=>
[image33]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB0AAAAaCAYAAABLlle3AAABPUlEQVR4Xu2TsS4FURCGf6EgiAiJCI3Se2gkaEhEp6NQ6xFvoRGFKBU6xe0UHoBEc0ncm3gABQlmMnPs5L+7uZKzmpv9kj8755+zM7s7Z4GGQedY9CL6dnV8rfpw7/V3d82kpswYzH/iRC4TsMIt8hNVD5TFKqzoBieEafxT01tYUX1j5gSW2+dELlVvsgzzLziRyziKpqx30UyxtT50jtpgjxPEGoqHOQr+kOjNff3F9LT3pQW7YZb8yJJoJ6y/UMy4LRr2eBvlY+qhap6RdLpT8RvRs8fqX3k86ut5X5cyCdt0z4k+tEWXbAoLsHojnIjs4m/zjCzC7tFZMg+iQzYTZyg+a9RU3FSCNvr0K6PNDtisg26I70K8KVrxWP/tuZDL4lG0FXTuvjY5Df61+9mso3cU6insqxoaBogfs71Ygzb7kOYAAAAASUVORK5CYII=>
[image34]: <data:image/png;base64,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>
[image35]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABUAAAAYCAYAAAAVibZIAAAAXUlEQVR4XmNgGAWjYFCA0+gC1ABP0AWoAbSBeDq6IDXALCAOQhdEBpJk4mlAfB2ImRmoBLiB+C8Qs6JLkAsYgfg3uiClYAoQy6ILUgq+ogtQCkBeN0cXHAWjgIYAALXGChPdherVAAAAAElFTkSuQmCC>
[image36]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFcAAAAaCAYAAADCDsDeAAACpUlEQVR4Xu2YT6hNURTGP6HIvxiQqDdTSigxkAFDiQEmYm5igiLm5jKglIER6ZUBAwODk5EiUpREMZIJJQbIn/W113pnnXW23r3nPM+h/auvu9fa++57zjprr73PBQqFQuG/46TouWh17Ch0Z47ou2i7tn+KFjdGFDrzQXTe2W9EF5xd6MgZpEz1MLhUoQdWAiaD/xsGHtw9SJvDIrXniy7W3YPgKFJw1wc/fQ+CbzBcQrpA036kusZMGRJv0S4Ja9Q32Jr7KNjPRBPBNy4bRZfH1HQwiF/Q/M4T9W9y47qwSrRS2/He6Wc/6XXsu4f25AeRbmBJ8M8mvLlchvps3i26oTavmeJ42it0DKG9U9tcnazZc7X9WvRZ+wj7Fmr7MdorZ2ReoXkRBs+QnSedIbYgXcNW55unvmvOt1d9nnPBdxP13sI+lkGPBfew6KzvQHvukXiPtInl2IfxN4yZLgsWXL96jqnPX3cuuCcyPuOq6FbwPdTP06KnvkO4G+xpYWC5LDzc0IwK6SlyDH/speubLZYjBci/idE+5WwSg7tUbTthMJi0OY6sVZu6LlqmfmIrlrqDuu6OzAuk4HKCT0hPLV40bU58HOmV83dZ8KfhK+8GbTPTc1lkweXGQ3Gj+yFa58ZUqINLGGDeuwXSryI+nHeuL/ebWY6IDmibS8smODQ1on5690ULnP9vwKyya4wZa8TMJbYZWlZWaAbXsw3t7xsTSH1Wr3vj6y0zwC+bIZILrr3Z7VC7Qh1clokIxzKpWHMjH9HzOObhMrDd1J5aNdU7PHLBZZbTZy9FFZrBtdVL7EEQBjeukDh3L3hot3Me3+RYo1mjhgj/X7Cy4XUbzcCanycQBneX6CtSrfWrk8HdjFTrr+jnUO+9UCgUCoXCP8UvzOa1/1BdtRsAAAAASUVORK5CYII=>
[image37]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIIAAAAaCAYAAAB7NoTTAAAChUlEQVR4Xu2av+8MQRjGXwkVCiIRjVCKhEKlp1BQiEShEQ2FFtGJTisqIeIfEHqRN1EoFBpCoyASiUQkgoII85iZ3O6zs7c3zP74rveTvMm9z3s37+7ss7dzuydiGIax1jni4mYIo06clwtcmCOXXOxg0aihLORw1cVbF79CfAn5ehcbw+vvlTryM38+OSxdRngmy+sluSeL+bhNtb54Ioue16gWURb+htgkxS3xtQNcGJCUET7IYrsRXO+DT1K/POE1tD756eJiJX/o4mUljygLuWwRP5FKeuSjtJtkKFJGiByVYYxwWJrzsC5oB0kvxTlp9twUtN2kK+XZnBI/8DEuBFD7zOLATMEIL6R5UMCyk+hfwUn4lUXxPe+QppRn81T8wHAas198DZeHMZmCEdCjzQjfWCxE20kI/R1pSnk2cQdPJOJBqMEQYzIVI7SdnSmDlADjvmFRvP6DNKU8i7g+eC5NEyDQLGcn98nid+2qsQpmhDqpnkp5FmthfQDMCHVSPZXyLOL6YDMXHHvE18ZeH4CpGIG/jkHqoJQC475nUdKmVMqzWLYT18XXctYHc740YHGWmitor1ksBBahfMABeuJ+QhWlfGW2iR/wMRcCuCSkdnwMpmCE85KeD2hnWSzEDWn2xB1faIdIV8pXJu7YcS44Noiv9fWzKJdlRjgtflt3kl6aePMIN5YiJ4PWF/E47K1ol8XfbWSUhS7uix+cA88Xtid0xBV8cERSRsBXJrYNzz9iIL9bfVNhtorvgbVVXF/hDO2TXeL7PHLxSvyzHxiEURbmSMoIRh1lYY6YEbpRFuaIGaEbZWGOmBG6URbmiP1VrZ04L//FX9UMwzAMw+iX36A44VKwZ4Z2AAAAAElFTkSuQmCC>
[image38]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEMAAAAaCAYAAADsS+FMAAAB/UlEQVR4Xu2XzStEURjGH2EhXwvJgpWFImWhLJSsJRZ2ysLOxspGWSr/gJRCTbLwFygLZdZsrZR8JEpJhEI+zjPvOTPX647hzHTv4t5fPc3M+5x77nvPPec9Z4CUlJQUfxaNLow+ra7s78tAbDLfOh5OjG5QyIf5UY+BWFO+dQVwnWqmIHF+xkkHJI+MNiAvkF6tNnxogHSWVXHSDfHOtREx85A8+rVh2IF4Y9rwYQTSGT81sxBvVxsRw6USNnPJHcTr1YYPe5DO6rVheEcFp2AZMIcHHURh+Rxrwxd2pke9zejV6N6oSnlRw1yY34aKT9j4sop7w9nADvngukp3Bdr9BmfN2j/VnLvyb8xB8nE7ndv9XlDhFzWO8HqxYuN1Kh4HZ/g5c8kHpF5UjCzC68WMjQ+qeBwUqxe3CB8kb8LqBTmExPu0EYLPMmnNXVmadkgeGW2geO5eNEI6y6o4cTdi8YqTBUge+nzRaeN6MAaM1o1WjU4hJ2x+L8k0pDNWZY27EQeMPAe8KHHHcF0o3WHQ5dVitG20b70nGydsp8tAni0UHjaomkCbURs7giTEG0SJzo3i/6UgbilvGr2hMGDcapdcI0gbnrITyTVkGRE3gxILH97NkgOjoYCXKDgTuKy57fIM0vPdTha6XiQW7ijcRYaNqpWXkpKSUjZfiD+cjiWKmAsAAAAASUVORK5CYII=>
[image39]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA4AAAAZCAYAAAABmx/yAAAAl0lEQVR4XmNgGAXDCTACcSG6IBJQA+LT6IIgcACIY9AFkcBXIP4PxCLIgvxA/BpZAAsAqQFp5EEWbAViHWQBIPgLxJpoYiCNKOA5Gl+JAYuzgOABGh/DpCAgPoEmdguIxdDEwBonQNnRUD4IKzJAQvs8EJ+CyqOA5QwIxSBswADxI4wPYoMMwApAAeGGJgbiG6OJjYJhBgA1AB3Pvtj8HwAAAABJRU5ErkJggg==>
[image40]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA0AAAAaCAYAAABsONZfAAAAoElEQVR4XmNgGAVDFRQD8SMgngjEjGhyGCAOiP8DsQyUbwXl80D5E4CYG8oGg2AGiAJhZEEg+A3EJ6DsX8gSICeANJxGFoQCkBhITgeIG5AloqESLsiCUHCAASL3CU2cYSFUAsW9UHCAASJnhibO0AqVwAaWMuCQ42SASKigiZ8C4i1QOZC/81ClGRgkGCCSMLydARFHr6FiPVD+KBjOAAC2ZiMWGRk1MgAAAABJRU5ErkJggg==>
[image41]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADwAAAAaCAYAAADrCT9ZAAACNklEQVR4Xu2Yv0sdQRDHR4iFJBEUEQIBidhYWYhFCklhY5MU0U6sbawstLXxH7AU4VVi7d+QTiFdSJPCpEiKEAIBBQt/7Jfd4e37uns7d74HJrkPDNzNd2/29m7mdvdEWv5P1tnxiHnibImddThwtsLOR862s012Wnjn7JSdgVFnt0ZbCNfU4aOz79KNgeNPQVt29i3SrsP5ZNDBb2cz0bkJBBthJ7Evvt08C44h8doLFoxMiL/+jIXAH/E6+mEwWDwIM0iJc3Ym+CX5TsFXyWsl1sTHRqalgIZB57hx9pqdOS7Ed1gCnaJtTJxaP6LjuuDNIv4zFhyz4rVDFiI60i2DSvClQzCkVBXaKQLHxIOMB18XxIalwEChzbEQgW9H7voepsXWMFW/J872ovOmjImP/cXZasKQVaV71BipDOnhrZSDAa1fNrz5h6L1i4fHg4VBq6pfBe1espPRzkqgDdfvJZ035aH1q6Bdagbp4b2UB6ydHpEfb70faLak0FKqql/FNGBLSqfqt19o7X1gIaClZMGU0vr2qijNvznwoJ6yk9gQHxuZlgKapX7NH63StKQrqLr1uiP+Oq575rP4ds9ZcCyK17iUUpinJXAl9xcemFMRIGUWhsUv93LtVYsN9wG2EhrsVdBTdCS/LL3HrtiWlk34yY4BUWtpqWlb2jzUBfGO2TkAsHnQ7DCDfXBue9gUbCaQ2oMG28Mpdlro9w+AcXYMgMY/AJS/7RfPG3a2tLT8m9wBsjulBW8OWKYAAAAASUVORK5CYII=>
[image42]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABEAAAAZCAYAAADXPsWXAAAA70lEQVR4XmNgGDEgC4htoWxZIN4DxHeB2AquAg/gBOL/QFwGxKxQ9jsoDcO74KqxAHkGiCIZBoQBZljkQZgfSRwOeBhQNX0F4gNwWQQ4wQBRF4QuAQLvgfgUlA1SAFIIMhgdRDNA5MrRJUABCJIQh/LXALEkQhoF+DJA1KajSzxggEgQA0CaQWo10SVAgg/RBXGArQxYLIQFKLGGgNRuRheEGfINXQILCGeAqAVFPwaAxT0ooeEC2NINCrjOAFFwC10CCpgZIPIgl+AEoNQHc81HIBaGijMCcR9UXAEqhhfwMSAMQsZxyIpGwSggFwAAjoU7jHqe3JcAAAAASUVORK5CYII=>
[image43]: <data:image/png;base64,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>
[image44]: <data:image/png;base64,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>
[image45]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAK4AAAAZCAYAAACowxUjAAAE8UlEQVR4Xu2bT6hVRRzHf1KCUSFKIWH4eApGtIrIRZiIIRhoizRQgnBXCNZCUHTVQ1y4kSz1QQQPg0hCFIIHIS6kFmIt2hQEFVSLJEUiwcBEc77M/Dq/+30z5989z3Pzzgd+3DffOeeec+Z8Z85v5twnkslkMj3zgYvnSNvs4t8QU1SXaclqF0tZbMl+FsaIl0PckEHjTrrYYcp3XLxlyvclj7h40sVyruiINS7Os0gscPGFKb/m4mfx+z1sdDDh4hvSxg02LsyMkfaBUJ518WtR3Qk/stAnR6R4vGyhui5Ap8B3l6GPOHSeDeHv38KnxrP/be2ZdnGQtL751sUTLAbQ+f6W4nrWDVY3ho3L/OLiUxaHACP6MRYrWCXF9d4Wf387p61xL7BAoJduYtEAA+ImgK3izwOjr/Jh0G4aDWAb6A+Rfq+5IoMdLGbcxeLrJoyGG7nHlJtSZlwYhNtxWH6XZt+HpyzOYWEo4zM2AA1NW+NeY8GwTPz3psCN0wbWmxu7GWoKbjg8DrscVYYBbZcy7ncuviftFRlsm92hnAru/Cnjoo1uhc+ueMbFeyxWgI75PmkzMncAGpo2xq1KA3CiqVxU930xlDEy68jLXBa/Lfax4MaVHf9eUmZc6CdJ01ExZr46pIyLtlIumr+H4U9p1hEek7ifXg0638faIIE/Kj6PfCdosQOB4+Lr/nDxlNF1NEVsC7He1AP0/DdJU74WP/NVTkm6cTDJwHEWka7pAs6lb1LG1Xb6iHRsB/0A6XWJGfcHKe4FgjtLG3CMpnMJnSji06Jt9DzptdDcA5MgABOjEdi4mpPsDGVMLlBGHgrQMJ8ELWVc1HHjAjXcaa5IgHQE28fg8+6LlHH1qbCPdDVu3TZQ9Dg2UnoX7dLm0Y5rjd17PcfXSa/kQfE7HiZdH9v2QvHoxohpQSNrQwE9wRj6nXwjAXocH68MbJtKOdDpqkYtdBRM8ppE0xlwW+NeIL0PTsjc60d87uJSRH/J75akyrjcFpXgsY0dnyadjasjInIkNLDGG0FXyoy7RHxdLJ/R/eoYd6X4bTFBiAHjdvFIHJb/s3FTtBltQefGxQ2ONS4bVxuVjauhlBn3Uak27gGuiICXEIgUo25cbctDCX0Uzj3GRmn/5k3bYm1CrzNgDYDliVjjsnExCaozGrBxvzJ/10kVMFstA/k0tktN2sB8pQoY6ZuQMq4+vdiguqrAE5g+iKUKmDizplFlPL023k5XFRpPpifF78jJMRsXXJXBWb9yVnyuDNS4Oqpi9m9BHT8ugN5MhE72mBfE11e9YODz7ouUcQE6KK/j6sybV0pGARhsO4sNwbXxOi5WVqC3Au/+sThs+VL8F9pJm74QeNdo+JGM/c2B5m/oEIBH0LLlsL1SmPdjKUZVHBcd4C+jpdAOgHXDvtkp/lxWkA6wVo06ez1If86Z8ijxDwstwBIa+wwD4R7SGjElhWnw/vxxU7Y9AktlPxn9jKlTZqSoh+kseKuVWg0AO2TwuHo+sVErhnacPtGlRKyJa6DMqcHbRocxsI49imCk5bd0bflM/DUjvcAnj8Aji6Ym88WsjM4r3/sFXgIdW/DjjK56sEXTBP0RR2Z4sIqgr+DHHs2Vu2ZafJ6c6Y7rLIw7dX5I3oQJKc+dM83BqkjqJc9Y0+W/7uxiIZPJZDKZTCYzltwFBgt+rx5t5oUAAAAASUVORK5CYII=>
[image46]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB0AAAAaCAYAAABLlle3AAABHUlEQVR4XmNgGAXDHTQB8SMg/g/Fz6B8EP4FFXsKV01lALMUHXAyQMRvo0tQCngYIAYfQBOHAVwOogh4MkAM9UOXAAJBBhpZuocBYijIx+igmQEil4EuQSnA5RNNBoj4YnQJSgE3A8JSdPwNiIURSqkHQPEIsiAdXYKW4AADxFIRNHGaAlzxSTPAywCx8DS6BBagwIBw4EcgZoaKszJASi5QDgClARAfL0hgID4+ZyGxQakZpA+UCN8jiYPANjQ+HMxhwEypIMyPrAgJGDNALEAGtxggetB9BlKLLb+TDPQZMA0yY4BYuhlN3IYB04Fkg6lIbFCIgIIVRIMsbkCSu4bEphiAXP8XiucjiYMS1CsGiOUgmhFJbhSMgmEAAMXVSXWgxNNhAAAAAElFTkSuQmCC>
[image47]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHcAAAAaCAYAAACNU8MOAAADeUlEQVR4Xu2aO8gUMRDHR7RQ8K0oYnPa2WghFoL2gmjhoxLBThBtbGxEvsbCwkasRLASG0Es7D+w8QF2YqNwiigoIgiKD3zkbzLc3Fw2mcs99jjyg4HdmWx2dpPMbJIlqlQq88NJrai0wgonO7VyFG44OaqVldZAexzUyhIOO3milYHVTv4aZU+4Zlosd/LGyU/q+YDzC6IMnk36+ErYLDxz8o561+P4ebAdIH8/tv0O55uCnTlOg++qSZaEawDq2yDOi0ClCAUprpEvt1sbyDsE2xZtmBLs2z5tIP9cf8h30lI2kq//qTYEvtBgw8T4RM3lOuRtkv1OPirdUJx10tXKCCnHwGtqtk2a9+R9W6b0iCQvlK6EE+TrRxSIARsaOAfKfdVKwTetIH/NNq20gpvB+Rwxx2T4wQtuC/j2S+luObmodKVgxOIeK7XBsYO87aY2KLgc/JLIsIsBoll08kArLaCn44YIOymaHJMNqvPMtNhO3re74RzRA6GswwXGAOqHxECjwrZLGxSxtIYReVmcrxXHzBFqvncSfjE5Yo7dp37H2gI+wDeE4PXhGKJDdCnryNf30smxiCCaWd6hTmtLyV+LNkhhbaMBDpHtQnZMC0Z023C+PUW+wy2E8+u9IiPB+RadSDcsBDZrvo1JDqQClFulDTnY8Rwoo/NtLPmnwJe0VdCrrfBL4o8dTjWp58IX9CWyPcOk8i1GMAZNDm7coWcilnjOjt1WeotjEt2AKbE2Locs+dLAo6Dfq/QSzIX1M8VIdRROV9Z8K9cBNgd9juLGtYTlWL6dFTjfat/woQL9Z6WXdCk+L5Zwvn2oDQFOVzl0vh2G4rDMozLFKI5NmpRvP8jbMEJiwIYQjtGEpb57/eb/nCZfDhEuBmzWfKvTmpXiD6rcVAgvDXZLbpo2Od+ukLfHVpW2kp8XY+12jZPHTu70lfBgAaRp1GD1CLZcaEdUsZRrAh0LK2xFoIfrRQzMWeFQTNqGlwK1XBVleNRinfdDOEYjMsi30J0ROgnWdHX9qBOcj9ggehXpnLBJeSsLGVik3hx+aBbItvw4T3TJ59sO9RptVol1HDMc3nIbB/MEnhfPjXyMnIlj5NdZA6Efu1AjgX3cpi2/eQNLfHKn5TsNHyqnBdKDTCfF1M362WJsm/VM/c1mNhj7bzaVSqVSqVTGzz97eB4mK4mC3QAAAABJRU5ErkJggg==>
[image48]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJYAAAAaCAYAAABVc6VBAAADU0lEQVR4Xu2aT6gOURjGX6EUFiRS0rVRoiysFBvZWLDAQtnIQha2SDY3srBDbORPFrKxYGEnnVKEoqRIyZ9IFAuhkD/n8Z7TnHm+M/f7vjEz38x1fvXWfZ937pyZOU9n3pn5RBKJRCLRHTbYOO0ikcdfl71cSPRnv42FLCZyGBaG5ZCNVzZ+u/js8mk2Zrq/vwd15Dv//md36WesBzJxvUoOSnZt79qYki/Xwm3JxjxKNY9hoSx+oBhnRGuruNBRYsZ6L9k1QHC9Dq7beBrkW6R4Dqrip419QY5jeBzkHsNCGeaInpAh3fNB6j/hJokZy7NRmjHWfNFx5pEObZy0qtgtvfM4y2lLSDeUl2K76M43ccGB2icWO0wbjHVCeicZvLbxhcWKwAIR2zeO4zxphvJS3BPdOdzLrBSt4XY4WWiDsZ5J3FgvJa5XQdECAR2GDjGUlwI7RmyNxFVXg8EmC20wFlaOmIG8sfDwVDXYL/bPQP9BmqF8aHx/9Uh6TYXAgLEL0ASLJHuvMmgM8lTVBWPF7h7/ykTG4mMxlA/N/9ZfgWSsPLUYy/dXs7lgWSZam0z9FWiDsYp6qSK9CrDftyyK6tzUG8qHJuZWj39yGVV/VeZWOEhv0gZjXZT4dUcTHdOr4Kv0GghgPLzPCjGUDwXeoWCnN7ngwC2QTxIXHBoO8p2Nsy6fG2xzy8YuG0dsnHIaep/nombBAwEmd1S0wVgrRMeJvce6RlpVnJTe+cTXFWjrSTeUD8Ue0Z1u5oJlumQGYnDRQ325ZAd8SbRv81y2sU10wkIztdVYO0TPZTHpdfDNxrEgHxMdu47+Cvg5xXx5Doi+jWcMC4NwRXQADtxGFkR0xDj+0YFJ4SUV2/jVLPz0g0lE3+BPCoHPJ4M8vdVFzFi+mca3UB/IL4QbVcxU0e+wbySbk6W5LapnTHScGzaeiI6PuWEMC01QZCxvSjYWXgbOcDlO4pyNF36DERAzViKPYaEJMCm/gnytZMsplvbDQe2+aB23wtWBHns6aYpkrP4YFprAr1gPRZdUvrXhR2IfRZfZdU6DsY7buCPan4XNftMkY/XHsNAEsVthl0jG6o9hoW7QcK6RrFnvIumnycX465J+mpxIJBKJRCLRAv4AMN4N5Qy5XqoAAAAASUVORK5CYII=>
[image49]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAF8AAAAcCAYAAAAHgGM3AAADFklEQVR4Xu2aT6hPQRTHj7AQIhZYWIiUKAtZkOwUiYWUvwtWSjZSlCwkG7GQrNiQpMTKRljcKImFFNlQSJSSCIX8OZ93Ztx5x+/3ct+7vz/vNp/6NnfOzO/de8/MPffM3CeSqYNjqjGqk6qtri3TYV6rZqgmqV66tkwFZqlWemMLFqrGO9sG1T5ny1SAWfzLGxMIL19C+TSUMFF1InbKVGe2apPqt2q1a4vg8Hnh+LxqhdgA7A62M6HMVORJKC9J69k/R/UjqdN/neq92IAhBiRTEV6Ym8MxsRxHriqbByhUp5I6fRYn9RFxRPVKylF8E+rEwWjb8rd3s3jk6tdU35yN+78iFlrOhfq4tEMdREd7tovZKZvEdPn3niaI3euyxEY9vmC3iU3KWiFX5SSFs8MCsbam5bJ3vSFwXcrZzwxPJySOJ92slTViJ6H07BFr46KawhTVZdXGFtopdr9LQt/ofLKdW+G4VvijnIS81fNTrM0vLkYzTKQYZtvpQ+jLIuqm2GB1hHjCFDKB76qPUsa8TM0w23E8jibLQZ+DbX7Sr9vsFcsw/leHB341ylgv5mgf708HOxlAk/DhpW5VohD7kY/3u4J9ubNnaqTdiD0Qs9e2mqtI1bBz1H42epgs5uDC2SEOCi/eCHsY2Cgfiv3uWdI+TSxLIGXjRT0z2MkYSFkXSeuB7ifYXvB7NUul9MdZ1zZsdoj9QZzjiSdjgOBrKLmwg+EYWJIj8I6NdbZj45KcTal+ZKrYLuVFGex81gSHkjr3wterYXNBSuemSvcs1gbbY9U7sZUucGEHYiexxQj9YpkS7fvDMer3Dw/cX+p8wi7XzSAAm2wMQE/wzo+PZDvnc/GEI5grtmiLq8d+xDvfU6jueWO34MJIQyP3pXwMcXZcCcctWvgUSiDs8DGiXxnK+XEPrGerfS7suOqF2Ms1jX9jxUIUCzVK6vBc7ONDobodbP3KUM7no0rPHA8+7DSNds5/K+U2y520oVuQEdxQXQ3HTaSV83la053PYlBrZsTEeJ6KfymJiUSqJj/5mUwH+APGbO9TULqOgAAAAABJRU5ErkJggg==>
[image50]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJMAAAAaCAYAAACzWm4FAAADO0lEQVR4Xu2aO4gUQRCGf1FBUVFREEHwAQZGBqJg4CM10EBEBBMxMRETOYSLDsHQxMBATIxEMPAwETFoEEQ01UwDDQwEEUUFT3z0f9XF9NT27e3t9szuYH9Q7HXVzVR3zz/92gUKhUKhMBls8HYr2I566L9nClXfFAZgq7cr1lmo8d06lspVb++9/Q32LZRXeFsT/p6L4iyfn7+yWywkJrbxB6r2Ha6HG+Wmt+PW2RDnULXxjbfltagwspgUTZTiNiS2zwY6REpM6yHt2h75fnu7HJVz8wxVX9PaEBOnr89R+QAkNweMmCxi2gi5uTN+5RMWFlpXSInplbfXxncC7bSV9WlDTKshefYYP4VzN+EbmbOQhOzIFIx9tc6OkRIT23XH+LYFf9OjcFtiugDJs9b4nwZ/TBYxvUQ6IdkLiXGq6zJWTFuQbpc+5Gnjz01bYnoCycO1YYwL/mWRL4uYeFPaqYTNhhhF1WWsmDjysF12tNKHfN/4c9OWmN6hdwQiDuLnEkcZWUy6XuL6wQqJ9ivE24Jvip55DGqcmhZjqWJyxp+bSRET66GMLKY21ktHIbukpjuuH0VMdRwaEJOul9bZAGQHwJhdVwyDQ/Md1w8rJn2Y1yJf7LcL89y0JSYHyWOPAdQfr5NHFhNvmFIuuQGJ5VgvOQzWccNMc7vmr+yPFRPzpESju7ljxp+btsQ0jV7RkOy7uc2QG/LGKTi92YTaCfogWMlYkBzNeMLK6TOunEPzHdcPKybCgzx7zkQRsS2rjD83bYlJd632nInP1rZ9JDFdhCQ6aQOelZAYv2pIwZgOnY/Dp77tCqcQfYAOzXdcP1JiOgSpb7w9fouqPU2yG5L7tA00wAdvD6OynvzbjctQYnqAajSJjeJQJVub4YURjyBiJDPhcz96r+M5B3GYPDGRS5B6cpSd8/aiHs6Og+Tjd5xqLHOh3BR8Wb5ARuJ7kHxHav8hDCWmHOz09tPbGciRPdEDzhQOkymmQsXYxET+oHfepW9TVL4ePh2KmCadsYqJ09xB4+NPGz5C5unnwedQTXvjElQR0+KMVUxdoohpcYqYBqT8bHdhplD1TaFQKBQKhUIhM/8AIC8Gitd9GN0AAAAASUVORK5CYII=>
[image51]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGIAAAAaCAYAAABM1ImiAAAD1ElEQVR4Xu2ZTahOQRjHH6EIISKlbkpKSJIFpSRCQsmCsrhRbKxslJWUlY1kIambJCWUpCSLE4pY2PgoH4VkYSNCIR/zu8+MM553zr3nvB/3vlfnV0/nvP85Z87MPDPPfLwiNTU1w8cEZ4us+J+xywrdxmRnH6yYYKaz385G2YQRAuX+4a9Nc9jZG9GGwN75328jbcffp6vBu2OtmOCp6LPHbMIIYolo27VMaHTLTlGdaxVOOzthxQSMBkZN+H6ZXjVf9FlGXDeBI9ZasQoTRSuWGR1CpV/bhAGgMXmHfAfjkbM5zu6JvnPon9Q0fZLuNMPNGmffrFiFDaIV42rZJ5p23SYMwGbRmDkY00RDINC7i0al5Yuzl1bsAkIHnG4TynJTNANWOJafomllYn3ggbNrVkzAKJgb/b4v+q0DkRbg+9u88cw5f98TP9QFfHW234plSfVEYvd3Zx+lXNyOYTSkGjOGEWAntxmSLgvQy2j4g6Lpvf53072vQ9AJMyuWgVFAxWh0VkvYZ6/Ni56rAu9usqIhc7bAipKvoAiJKfok7ahu4Yzk4bYSxHMqZucHVjzo441eBt5basUI8izaX4Q9RVFjMz88tmKTUI7zonm2CxzRVH6ZaKXt/LDX6yuMXobBHHHV2UIrRjwXzcMumcNkSNnaxSxpsuEKaNoRRb2PWIe+2CaUgPeKQhOT7mAFZTmbKhfORWvnvNAJR1QOTZNEK5YZHUJDECpinoiGhtnOxok+Y/cLv6R4siYULLdigrCz3xppfV4LsGlk0qfy6FwfitbnhX+GDhHXkUbnNw6A2BHkxWqPuvF+GNWrRVd0e0Tn0IFoarLulcbKBoIjcBawLFsvGldfSV5IChieCRQtX8dInm9Zw6mBTPJG63F2Kk/qdwIrqgDhDwOckeVJ/XmkHEEeoV50Lu6pL+UIXBSdV4ugnUqHzrPSWGGMhgps9Bo73/eiO2wIjRk4Ht0Htkt6Q3dSGr9ZxlaJElZ4mP0ujRiPwhDGoKwjeiTP/67X6KS2PIzEFGEOm2oTOgG94Xb0e6Ceb0NWJ7GOWCbVHTHFXyk3Ia5PdDVZdg5p+YijCuwaj/h7eh3HFCkuSLlDv3aBI+LvETLDaS6NmeVJ/Q5iHgAbmsJBIs4gDIVeHk4W6GS7/b2FDepKK3YKCsbm74407j1ibAU6DY14VHT+Yo9ij9Q5Icic3RI9wKRs6/wVwxnkcUX0XO2Ts9G8KDpPEPtZQFz2moVj8GdW7BbK/jHUDmxoGkrodC3/MdRphuKvUmL7DWeX/P1Qs8UKNTU1NTU1rfAHlbgX8dHeVBIAAAAASUVORK5CYII=>
[image52]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAF8AAAAgCAYAAAB0OKThAAADZklEQVR4Xu2aTahNURTHlzAQEomERCgiAyHFUPlICoUyMKPMFAYGXsnA2ItIvQxEyMTE8MbEQIx89FAUSooSSvKxftZe3X1297qf3Xvct3/17+y9z+l2W3udtdde+4gMFgtVv1U70xsRB1R7VLNVw8VbmU54rLqq+pXeCMxTvYv6P6J2pgPmi3n8BDHv31y8/Zc3qg2hPVPsuUwXwOud66qfUR+mSNHYW1Sfo36mTWap9kb9SWKG3hiN8VYwIReDPqluRvcbsUm1Lh3MiNxPB5Q7qu9R/7LqStRncligm4U3h9/IRExTHUoHper960MfL98e2hj9Q2g3SzZ+DfD63XX0VvU1PHdQqsYn42HSYK5YCALC0aIw5oZ+FK5ufBbqNapxYr+3S7U46F8p7sDh3t1IK8Lz31TvVTNC36mE6/TQjr3c7/mYZ1P8zkSxRZtn0LXwbKYFKuGK149I0fj3wtXH5ohNAJ5fEXvz6MPycG2Lbapnqsmhz8yeq94eWF6rjqieR2OvVCfEws6o6omYx68Um5AzqmVik8D+gUnzUNYy56X4qhK/iIX8+KBTSQd6zcOkz6ZlQTLWDkye59bNijJAr9ghtjin60DfuCvdMXymRV5KibygAWlWUza1xEexRbabtBN2yDrGFBh+fDLmmw9i8FA07jC7rPhl42Q6UGZGxYyPMb+oHoT2sXD/RbimjKhWpYN9Zkjsv5OX14NCXCnYL7ZFBkKOx6t9YYwNRFwxjGOZ18/LREWsoEZhrRa+Q/0vIP6uDu2pUqwgpulpvzku5vEU0zAw5YcUDlPi2n+pId6TerJwPpXqxouttk9KWYgdA++/HfWho2ykX2Bw/izVvBuhfbTwRP+hrBxv7zn84H+m2RvnthTQWoGKJZuxTB1qHahwkM6ButNJvPfSdCaBhKBWBsMhOsb2pKCTeJ+NXwfS5PQgxYXxL4XnWKc4VAE2c+xruE8Vd61Ymsp65p+UbJXqAYwb348iuRLmTkm1vDzmcO9uJAy0RKx8TALhxB7tIcnHSChS43P8yEH8UrFJpeRcEStBZ1okNr63/UqCkRrfz4Q5iiTc+XdAvazEDgwY9YLYgYgfIp0WC0/DYl5+VuytIKPC0w+LHbTALbEQVusjrUwD8kLaJ/gIikU5fwyVyfSMP4D1+jrFZoT6AAAAAElFTkSuQmCC>
[image53]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABUAAAAaCAYAAABYQRdDAAAApElEQVR4XmNgGAWjYGgCKSAOwYJFkBURCzSB+D8e3ItQShwwY4BodEUS2wPE/5D4JAOQgRPQxHyh4mQBPwaIZg408VaoOFkAlzefQzFZYCsQP0QTE2aAuFIFTZwbKo6OQepRgDZUAgYYgfgvEOcgicHEQb4CAR0GSGrBC8IZELZ+Y8BiMxoA+Y4FXZBSQHYE4gLSQHwXXZBSUA7EDeiCo2AUQAAAZKkogn9oDzQAAAAASUVORK5CYII=>
[image54]: <data:image/png;base64,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>
[image55]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAaCAYAAACtv5zzAAABKElEQVR4Xu2TsUoDURBFbyBCQmJlkcY6YGVhly+ICBbpAhZ2Nqn8BMEfkPyB3yBYpFhIKm3V2oRgkS4QLATRuczbZJi85sUtLPbAZd/eO8wusztAyX/jRjQT/QR9hPu58frr6j+QN/NcQH1ed6YJbZI5nxxBs6kPUjiFNuHVM4Bmjz5IYQRt0vCB8A3N9nyQQmz+LdGXaCmquCwJvjWbsxn/HmoVvLap25lzxOc/DH7d+clkiM//Kvgd5ycTmz95hvrHPkhhH9okcz7JH8yPbXkTvYoORTVoDfcoyiW0oOd8kj+AL0E+RV3oN3kXnQT/ydSsucemgVXV1JwF70W0gG40YY0d6Z05FwL/urG5fzDnQrgW3YYzx3RgskLgVnMpJ9jenZKSgvgFLOlI+jiUmXEAAAAASUVORK5CYII=>
[image56]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACkAAAAaCAYAAAAqjnX1AAABhklEQVR4Xu2VvytHYRTGH8UgTAaLwaRYDDabCYnBImWwWQxKYlY2g2QmGf0DBsMtZbEYDFYSxaRQSJync871drrpO3y7V7qferrvfc6p+3zfH98XqKn522yIbkRfpjt7v028uby7YjxQZB7q81kpndAgWfDJALR2HQtlMwENwmdkCVo7joWyOYEG6YgF4RNaa4uFsinajz2id9GTqCXUSoezx4AMxFNNPZvXn/RVyjSK9+Ou+e3Br4QMxftx0fyR4FdC0X4k51B/KBbKpgsaJAs+8fA8QM6aeeuiC9GR6BG6VQ6gh2ws7wb2RZuiPdFW4p+JdkTLNv6VBehHZ4JPPCR/CHm1J4PyynTYM2rj4aQ2Dl0N5wN6aWxDwxOu0lXeETjET4hUrUnPpHmXogfozUMYkrPmsIcfJwz5YuNTq6ViKIaNh7TpNBqSF0Ta5zDkVDSbTaMh+0RvNiasdYtWoKvj8K+uqfjBoRg0XUrOjo896KC9c5/OmkdWodftvag38Wtqav4934G2dLfaKKXKAAAAAElFTkSuQmCC>
[image57]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFgAAAAbCAYAAAD4WUj2AAACiElEQVR4Xu2YPWgVQRDH/4KCYsAPxA+slDRGRUFUAopFEBTJB8bCUkSwTxPQxsbCVqxsgkUggp2NRZAHAVEbQVAhiZBAQCQEQbAyUefPzHL7hoTse74k7y77gz+3N3M5LvNmZ2cXKC9/RVPeGLFF1OGNmTSuiy5Bg3zA+QIPRY+9MZPGJ7vOib7EjohvomPemFmdK6IjNu6EZvH+wo3bZgsainyZBKbd/bzovbMdhWZwo1wWnffGzcRFaNbGnIBm6q7I1mz95aL4zBs3E1+9wfghehvdN1t/1zzAC6j/0HaiW3RPdGMZPYBmcWjLOCac8rQdhv4I5Cm0hNAWgvnBriHA+0Rnoa1er2gQOnModjBNwZe186LAAMWL13J6Zc/WRG+gQQrQRvbYOM7W4Au2rdD3fRdtE/20Z6gxe7ZhzkBfulJfWXZqdmX2jqA+wBN2DbZD0CAz6WrQWc170mXXZHqg0+w1NMBh2lWNWejsjHd/M6L70BIxKfoMjcFJaNAfQWs5A82emz9MvJgmMQAN6CL0IzimrWrUvGE9CfX3rndUhH5o4uz1jvUi1F+unp6roj+oXzBWg3WcdSxF2+1vKg1rCwO8EvQxy1NpdYB9t9BOSuIXigMUD/vF396YScf3v2xH4s3GsOiFjT+KXiIt6zIGez8GmHWYsF1hcx2YEV0QPRfthj4besIysUO0BP1+dkuNlLz/ZgRFTdnpfLRxF1X2DUh8Csf/l6dwG05cf3nQ0h35ygYT5aaNuXlIXqDWEtblURszwKdETwp3aelDmwT4nei4jXmKxNp1rXCXFtbic96YaQ3jotPemGkNPAs+aOOqHgtsGLdEd1CcFrLcZVqI3+LmAFeBf99HsAl7R2i9AAAAAElFTkSuQmCC>
[image58]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAbCAYAAAB1NA+iAAAA0UlEQVR4XmNgGAVUB+FAnIkuSAr4D8Rr0AWJBSIMEANs0CWIBdEMEANY0CUIgU8MEI3omGRAM/+vBuK/DBAv4gSE/P+bAWIJTjCfAb+/QXKM6ILI4CsQX0UXhAJpBogLcAKQySAbiqB8kDdOIKQZyhkQgXsJiDcDMQdCmoGBhwFigDGUfwuIWRHSDA8YIIG7EogFGHCEFSwMQJgbTQ4k9h6IxdHEiQLI/r8LxJZIckQBULgshbJBBugD8RSENGFwEoi1oewgIH4IxN4I6VEwOAAAH2ot03iDW9UAAAAASUVORK5CYII=>
[image59]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC4AAAAbCAYAAAADDr0pAAABrElEQVR4Xu2WvyuFURjHH4NByKD8CANZlKREKbJQSiQZDAZJUTaLYrH4B4wWGe0Gg+GWuslikvKjKGVQzJT4fjvP6R5Pl7iu94fupz695zzndHve555z3iPyNW/w0gYDymCVDcbNFBwSl3y9GfNswi0bjJszfd7B83Ag4B522GCcjMJWbbeLq3pdbljmNeZdCcZi5cr0H+CJibWJq/hPmYRNNlgMBsVVOaRTXGVrglih67sHjttgMbi2AeUJHgf9Qtf3nyTeD9fgdB43xFXdH39skxGNdUnupQ9hrbglsauxU336xIdhtc5rhNuwHM7CFp37bVjVcNPl80DnZmAW9mqfyfskuf7ZDmMZffrEm8X9nn8htjnnCC5pLBLCJAfgqokxIeITH9M+K865z9onfKnIYJIXcFk+nkA3cF1cZTn+CF/EJb8P92ClyqXGPr/IkRFWN1X4TVxhB0p8gj09kmSJfwc35qu4v/dWIj7WfkN4q9wRd6tMBaz0jLZ5AUvlxpqQlCbOtd5ng0mH19puG0w6vFc3aHsxHEgyc3BBcvcWHompwH6qU5N4ZLwDEX2ChXaekqEAAAAASUVORK5CYII=>
[image60]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABYAAAAZCAYAAAA14t7uAAABMUlEQVR4XmNgGAVEAAF0AWqA6UD8H4hd0CUoAXOAeDWU/QiIg5DkKAIZaPwmNP7gA6FAfBeI1wAxJ5ocWUCTARJhTlC+EJRvCOW7ArE+lE00kGeAGGKLJn4aiH9C2deBmBFJjijwF4h/owsCQSsDxEJQkJxAEgfx64D4G5IYBlBigGiehC4BBOUMELlLQMyDRW4pmhgK8GWAaMYWfjCDy9AlgOABENugCyIDkCRIsyS6BAMkc4Dk0F0LAiBxFiA2BeJZQLweVRoCQIpi0MQmA/EBBoicDBDbMSDKD2kGSJx4AzE/EJ8E4uVQORTACsS/GCCGgPAdBkQa3g4VA6UQGIAFURaSGFXAAwZIECowIJIjVQDItaA0LQ7En6DsdBQVZABQOL9G4v8A4idI/FEw1AEAOVw5p5DYBTsAAAAASUVORK5CYII=>
[image61]: <data:image/png;base64,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>
[image62]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAmwAAAA8CAYAAADbhOb7AAADpUlEQVR4Xu3dz6unUxwH8CMUUYhCLCQpshMlKdQoCxaoEX/AbGyRsrgb2UlSykZ2YolsZiEroWzUlFJDykKxQYyf532fc+Z77nG7btPc73XvvF716Tk/nul+v8/q3Tnn+0wpAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABwg38wDAAD8f9xZ6/la78wTAADsvwtr3dTa59V6aJgDAAAA4CC5rNaftf4exq6f+ofNNWX5fv07fj/MrcMb80CTz/PbPAgA8HG7jgHt01q/D/24t9aV01h3flnms7U3u7os82fLS7Ve30Xt5L2h/cPQXofHa708DzZHy+EOygDAGbqqXcegkPb7rX1HrZ9b++JaJ1q7y/wTrf1MrVuGuYS+hLX7a/3Rxh6r9VdZVvZG35blbz44je+lfNb5c+zW5bWu/Y/azndla7Adn/ultb4a+gAAp71Q65WhnxCRbdEEi5PDeNxW69nWzkrRydXUph/Lav7JYTzBrst8Atro7rLeg/e/luXg/7p9OLQvKqswHE/Vun3o7+SCeQAAONwSGvqK0BVlWQGLhLhHWnvUV4VynecTxsb51BzEck9W3xL+4uGyBJX5vr2QEDqGpDjWrtmu7BKI9mJ7sj/beK5V90W75vN9VOvzWhttLOcMXyzLK0D6M07tNuABAAdczrH1rcwvy2o79M2yfYgaA9k8Pwa2rNL91Ppj+Mk9WU3KWbl4t6wvsGUFMD8yyOf5rF3jrtN3rCQ0nW39WYz1S7tmy3m8L24uS7C9sfWzpRp9FRMAOIfkJa45z5XVnevaWEJUD29dVuD6dmbunecTwubtztgoyxmt6GEjoSRBKatZ6whs49mxtHOervukXXOmr4fXBMm98EBZbcfmjN923/vroX28rJ5dJ7ABwDnktbJazckvOseVsJj7r5bVC1/vKf+eT7/Pj+es8mOCHph62MiW6wetvY7AtpOsvMV97ZrwutHa+2EMbNmq7SF6foZZBQUADrm89+tIWVZ6ErYu2Tq9GRBOlOVVGTljdevW6c35/LvMz+Et4edUWVbd3mpjfRuwn73KNmBWj/r4GFTW6dGy/P0b2vXoltn1ysplfx7d22X5f0efbv0892w378cPJwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgDP0D3AonhFt+9m0AAAAAElFTkSuQmCC>
[image63]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAsAAAAaCAYAAABhJqYYAAAAwElEQVR4XmNgGAX0BMxAfAGIG9ElgMAdmcMCxO+h7P9AHI0k5wcVY4QJLAdifiDmgUqAFMDAaSD+jcRn8IDSDQxopkD5W5H4cPAPiK8i8QUZIIpdkMTAAJsTQG4HiXEgiYGBMVRCEkkM5F6QbRgA5EGQYk0oXx7Kx+peEMhigCgA4bVQGsO9ICDMAIkYGGhgwAwZMLCBSiyE8kEKQPwyuAokYAnErxggikCm/wXi+Sgq0IATED8C4gVAzIoqNYIAADoEJlQV7DzfAAAAAElFTkSuQmCC>
[image64]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABUAAAAaCAYAAABYQRdDAAABGUlEQVR4XmNgGERgORD/h2JDNDmyQAMQi0LZ/AwQg8XgsmSCPUD8E4n/D4jLkfhUASCXWqILUgKKgfgWuiAlQB6IT6MLUgJAEbQYiV+ExIYDLSB+BMQT0SWQgCSUZgXiC0AcAsW5QGwMUwQDIFtXMUAUgwIdOWZhAORVWNgdYECkURjmgcphBQ0MEEU6SGIgS98j8UkGLAwQQ09A+SDX/0ZIkw8eMEAMZmSAJGwQTTGIZkCEFcilVAHcDBADW9ElKAGgBA0ylKLIQQargdgKiNcwQAzmQJUmHfQAcQKUrcQAMTQHLksGAGmuRBMDGQqKfbKAGRDPQhcEggwGiMFlQKwAxHNQZAkAPXQBJAAKX1A+b0SXGAXDAAAAPdA0pVsCM10AAAAASUVORK5CYII=>
[image65]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAmwAAABNCAYAAAAb+jifAAADOklEQVR4Xu3dP6iXVRgH8BPVEBkSguTkZoQGgaAEjg4uOugi1BbhIjQ4uATZmKOLIMHlDi3SEEQQ4taQuIigFIWDLkJQkeCgovk8vOflHg/Xm5d7+/1e7/184Mt73uccro6H9/z5lQIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGwW+yKv9EUAAObveGRXbS9Edjd9AABMwNeRf2v7vchi0wcAwMRcjGzviwAATMPOyN99EQCAaXg98k5t5xIpAAAzlqc/r0UeNrXXIvcix8qwh23M3mYMAAAzcqM+c0K2pbYPRi7XNgAAc5Rf196PfBj5panfLcOpUAAAJuJmGb6qjcarPAAAmIh2gnakewcAYALaCVq2/2zeAQCYgA/Ks6dBzz3bDQDAPB2NXKntPIRgORQAYGJuR87X9m9l+FUDAAAm5tfIJ33xf/BP5Oe+CADAfL1bhmXWHX3Hc+SPzI976XKJdnS8aQMAsA5eLcNPXn3Wd6xgaxkmahcif0S+r/W82BcAYFNqT4auJit5owxjcsK2Wg+696uRtyO/d3UAANZgLRO23oEynGLd03dUL3L9yIuMAQDYlHL/WR4wWM2SaC9/jL79orc/8mXk47J0BUnujRsnh4cjZ5v35cYAALCMnDD911LqcnLC9mNtL0b21vZ39dn/zev1mVeVjPoxAAAbxq7Incip+r4eG//zWo9LfXEFhyIf1fbOsrSHLv9vqZ+MbSvDYYW23o8BAHjpjcuI7ZUa+X6/eZ+V/Dffqu38wja6VZ/jZOx0Gb6+5Z63lP0Ha7sdAwCwIeTJzHEZcvRTZKGrzUL7dSwnbGdq+159Zn9+VctDCdsjn0fejPwV+WqZMQAAG0JOcE50tW/KcL3GrOUJUQAAOuM+sXZJFACACTlZliZtYwAAmKhPyzBhGzfwAwAwZ1/0hXC3DBfSAgAwZ3klxg99MTzqCwAAzMe3kSdd7Vixhw0AYDIel6VLc8fkhA0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGDDego6nYOQBHweggAAAABJRU5ErkJggg==>
[image66]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFkAAAAaCAYAAADcx/BtAAACmElEQVR4Xu2YP2gVQRDGJySCQVFEMQQESSMY7ARBMVYGTKGFBExvoYXYpBAEK7EQrKxEtLBIZ2ERCQSLV4pWhghiYmGjoKggKKhEnY+Z8c3Nu9M7gz59mR8M2f12j9v73v6ZDVGSJEmSdJ+bHN80roa2n7GB4yO1nz1UbE6MzxzXXf0exwNXr2IzibE7nbbCMe3qCXODxCjMSGObaiNOK2OR43HQjpE8mzi+Urkp0GaiGECfW0HbofreoJcyyvGE41psYA5E4T/G9tIItDdRdAyR9MFK8Ayrfj7oHWAZ2MsRH1zbgLb3Cl+o0+T1qkXdg5mK9nNBN5NvB70AHlrm6NN6P8lmvl/r+OW2a/lvg6WIA6pJ2HdUcZHElI1OO6jaakxuBb3ApygwWzkeannJN/QI76i47F+SGIXUrIpVmVwFltUUx57YEGiSY4I4yG5xmcScu1pHOWYOHjPzUoUeD8RaYCa/j2IJOK3rcpyamfw72wXOkKZYCodJVQW2oTIzLbuYCHotWlQ0ZI4ky8BA7GC0fQzxXLVdHAsckyRJPwYBMDjfHzOgGxwmef9Jp51RzbOJ40LQsM3E2Q5z8SwOz8a0OAZd3Wccd1w5Dg4f8UjLMNLvczC6yUz+E5gp41pfp/V9P3oI+F7oR502ppo/XJ9xzLt6I+I28JTasxCz1YgmAxycuKpaf+NfMBm8JsmgXpGMD3eECH4MrERkW56zJM/gW9Be5zpeyhaSE9djy+EIFX9NM/G0/sW+CIMB0qQqk63/muUUdd5s7rtyi9oHjJloWQZWgF0xkZn4JYc+1g/vWNMgs9gdNOzDyDZwccFBYVzheEvt/0RhK4GxsxwnSJamzWzb/7BMf3Vp6HleRCFJkiRJkqQW3wETtrUvrU/ggwAAAABJRU5ErkJggg==>
[image67]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABcAAAAaCAYAAABctMd+AAABDElEQVR4XmNgGAUDCfyB+D8U/wVidVRp8sEEIM5H4oczQCzJQBIjG8BcLALl8yCJUQyuM0AMYoHyeaH8b3AVVAQRDPiDhRGIfYE4BA0HICvCBowYIAb3oUtAAcyXuDBOAHLxXAaIojA0ORB4D8SvGSAuBwF+BohaF7gKIgAsQvcgiZVDxWAGwwBIbCGaGEEAikyQRkUoH8TeipCGA5D4JHRBGIB5DdmVIPAVKg6KOG4oG937SlBxEI0VgDRjiwyYGMjlHFC2JIoKBoZTQPwMTQwFcDJANAogiSlAxW4hiYEiMgeJD8rFoGICPQ4wgAwDxDCQYpBLQOx1KCoghnyEyoFwJ6r0KBgFo4DqAAA5H0bKEU33OgAAAABJRU5ErkJggg==>
[image68]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAhCAYAAAAmnK7xAAACv0lEQVR4Xu2ZT6jMURTHj1CEhAVLRUoKIUXsWEhZyIKIt1AsrCwoWUysbCS9FeqRZCMbiWQxC6GUlVKyIVlQilCUP+fbOfe9M2d+854Z08yZ5nzq2+/ec37T3Lnn/jn3DlHiWcVayFrHeuR8SQ+osY5r+Y+xJx0wl7XXGytYxprlbPjsS2dL2uQpTT2KP7Gms+6zZhv7c1NOOgAj+CRJAI45X+EBa6uW8e5+Ld/Q5w59Jh1Q12eNqmfBfGq03yUJwkW1Q9+MP2kDLCU1LU8j6cyj415hjKTTC18owIg/w3pLEyPgvdbfGdu+8bfjcsfVL1DzLPjOuse6xLpM4l/Q8EYfKZ3tOUBixzMqM1lnna3MApsRoT5Py1tYP4yvr2DzQuPqzg5WkvjeeEcgbpJ0uGeUGgeVLSNb2m7qfQXrIBpXtR4im4APKVtEZrAes/ZUCKMfbd+t7/7W5yLWKy2H4CFJQ+d4B/OLxIdpHpEyyidT6fgNrCckAQtFaahlMesn6zNVT++kS2DUo/PR2ch+oK9qW2He+x9w2EHm8a86Lx8bDnaRdLZf/8vUtkf1iPjlppvqCXWSL/Pr/xG1b3b2pMu0ivYzEvsa7+iAdpcgXA0MBTiUoJPrzg5KYLAZW5A/I6s4yDpHsndE3aRXU/Nvw5JaMjucbfra9hGShpQ82VICUE6OOMZvI7lDRwCWqh2XWVe0HAW08QTrNDUH4LUpj7E+mnrPuE7NGw6EQ01hp9pesD6QnIgBfpxdsnC5dc3UI4HBUXc2tL1cT5RT/kCBOxTsDwXMhk2mHomqAFhKBjhQ3GKd0vJyktvTqEwVAOwFG70xOhjxV0nuhm43usIxWQBw/bLWG6Pj1//otAoAUt0lWsZ5ZyDAn9iHSLII3CYOAlUBGGEdpokb08jX7AONz/DWt7BnAJIkSZIkSZJh4C8q+NERa7QyYAAAAABJRU5ErkJggg==>
[image69]: <data:image/png;base64,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>
[image70]: <data:image/png;base64,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>
[image71]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAaCAYAAAC+aNwHAAAA2ElEQVR4Xu2SMQrCQBRER9BC1NobCFb23kAECzvBws7GU3gGb+AlLCzS23oBg1jYCWIhiP7P3+h3srGyzINhyczsLn8JUMIsRanoGXQK30fnTd7tH2RlZgrzdS2kCSsl5CtdWHbgwDOAlXRlFrBsw4FnCys1OBAesKzGgSc2f1t0F11EFcq+0Ft1s5b19VXX4HVcr5AR4vOvgl8nP0eC+Pzz4PfJzxGbX9nB/B4HnhaslJCvZAfrYxYyg5XG5CvZAXqJcnMZ1vgUvKquMwzeXnSG/ZElJf/jBUgmPUURM6ZLAAAAAElFTkSuQmCC>
[image72]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAdCAYAAADb/iBbAAAC80lEQVR4Xu2XwatOQRjGH6EIKaREXQsrkoWwsmOHBaWkbJUlRdb+ARvZqJuFQlKyvYvp2oiyIiUbFrcQNigk5jHzfN/7vXfm3G8zdzW/euu8zztnzpnnm5lvDtDpdDqd1uyJ8deLhrUx/njRsBvpfsYPpPZLwf72ezFyA+O+3qLc1+oYHzFud3qyXOR6jODFlujlLOti/DI1XxeXkGorcj6Tcw68BgfINt7UTzF2mnweqd1Wo7FfageN9jXGTZN7eD/vCU5vxuMYC6ibRgLK9fVI+i2nf47x1GmCA+QM9KaqL/uc4zmfMxrfl/1b2A/brXK6+I1lNHVXjFnUTRMB5fp5JP2K00PWS/yMsR2LTSXUOOvEyazdMVrJnA1ZP+F0ci3GIZTva4L2yYC6CSSgXKeZQ6ZucboGuA1lUz2c7Wyn5c8tibmduUSz3JpPNiNtIWRZTJ1FmqkkoGyaCCjXz6Js6qusW9M2xniWr6cx9SpSmwNG0323jUZk6nOnf8d4r29u6kyM+yYPKJsmAsp1/Wn4QVLzpnFZa4BDpm6KcSHGI6SVtMPUljL1ndEuxzhs8uamcuO2BJRNEwH1+imk2sqcczALWZMhPCEczddkyFSL9lQ+g0xrKvOX4/J/mprKowfPpZaAumkkYLhO874hHcF4HZDar0F5gNOaypnNdnq2zHswajGph5zbVSGamvokxnsXenFe3x03HREwbKqHRx7+y5O9WPy8L0j98QDPnPhZKfRuNE55GFUT+ve/mHP/PI2R2wmvj+R2TeGGPmRaQL2ulxWaXTSphs6fdqaGrPFHF/q3t89+jcXn1H1Ibfxpw1L6MZrCT8uaaeQFUt0vKUKdX0KCx503Ji+hWWlnDPfcD5h8hk4A3KcFTyzU7OfrPJLZNfRD81TSnID0MLtEqAnm3CdV1/e2nWHnsqb7H5qaR3uplr22AXEm59S5R/OamucYUu0e0v7JL7QabKdlr3fkSul0Op1Op9PpdDqdTqfTmYp/mg0INnBduEMAAAAASUVORK5CYII=>
[image73]: <data:image/png;base64,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>
[image74]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAmwAAABNCAYAAAAb+jifAAAJDUlEQVR4Xu3de6h16RwH8EcocktIrm80Rm653+ktueaSSAiZ/0yT5BYNk39Q5B+NPyZMaUhp+E9ySdooRJGikUsuuRShFIVc1tdaT+c5z157v2u/Z+9z5jWfTz2dtX5r7bX32WefvX7rua1SAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALux2Q3lMH9zifB8AANjFVX2ArW6cfj5yKG9v4m9tlqu7DOWaafmvQ/lgsw0AYJEkFE/og2x0+6H8pw9OblvGpKyVpK7uf21Z3w4AsNXDynwC8dwyJhn36jds8dmhvLCL/WsoTxnKa4fyxW7b68tY85Tn2JQA7cPLynj8O/Ybyhh/yVDeVY5qwS4kNWr/nJafWsaEt5Wk7d1drPr7UK7sgwAA2yRZe3IX+1k5SnJ2Sdiyf5uwPX0o323Ws/3V0/LdhnJTs+1WQ/lCs74vvxrKs8t8wpbEKYlplX0ua9Y3ScLWJpjfaZaruQQ0v+P1fRAA4EJqTdGcXRK23w5lVY4nbHl8279rNcUi8dQ2tX7Xrbfe2AcaS17jXMLWJ1W/LvO1jb3Hl+P7zT1mVcZBCa0rpp9fb4MAANs8sIx9qjZZmrAlWUsz4KosT9huMy2nvKGMNWEX8uNu/b5D+XMX26RP2PLYPmH75Uxsk/pa0vevbxKNJHX1d8/2+rum3FB3AgC4kPTd6vuctZYmbLWZc1WOHy81T23C9sdyPCG6w7Se8p4mvk2boC1N1qJP2B47xVq7JGyRZPeJfXCS9+3LfRAAYFfvLWPissmShK1tUl2V9QSwJkDpv5WauLqeGrm6/Ixpua9B2ySJ4C7JWhwiYdsmz5XjAQCcyDvKyRK268o4yrRalfWErTY9frQcbxL9STle+3Z+2nb3JrbJPhK2uZGpEjZOS5rM81nb1+cNgP9j+2oSrZJItcfLfGUvbtZzvDoSNMt3bbbV2EO6WG9fTaI1lmbZKq9/28CHXWgSvXn5fVlPjp4z/Ux8rh/iobTz8mVgyi+ONgHAuiRMn+uDjZxU7j8T60981d+G8rpm/c1D+cO0nBNi2+R5dRnnaKvSZPqDZr13rhwdqzU3pUYvx55LEDPtSHvXgbmk7mIlGX5mH+RM9aN5Pz79zN/9Ve2GU5TP27f6IAD05qb1yAksNRIZuZnSJmjvH8pPm/XISSf7ZN+2JqMmSn9qYq0PlKPHzW1vXey0Hjlu+s7lOTJtR/88/xjKb6afl3fbTmJVxpGw3Hz0CVuVv/1Z+VIZ+3MCsAc5yWcKjLj1UL5d5mt7LkWZC62fOJeT6xPDs5IJivNaVl18TvbLiN/Ukva1ndnWl0P20ftemU/E31nG587/YC4IdrEpYcvx0tfyUB5Rjt6zVprfd/0dANggfar6L9pI7BV98BL06LL5RMbFSY1JO6DirKRW8cNlWcKW5uE2eUhClFuHRfpZ9clayiH6fbXH7xO29AmszeovLfP/l9ts+pxnIuNMOXNI/ajkc83yN5tlAC5SThJ95/F7lvHLt72t0aUsJ143f9+PuZu/n8S9y3hrrU2W1NAsSdhWQ7lHs54RxJ+Zlp/WxONFQ/lIF2u9qQ80ljYTzyVsibV3j8j73PdL3Kb9u6QW8S3TcpK13Af3kNqErS7XcnNI7gEueflCrc2hUecPy7B8OKR8ztIEH6mF+VCzLfJZXJIALUnY6vQr9Tm3zYn3tj4wox1QEs8v6xc+2/QJW71IaqVJNonlxUrilMSzvsdz7lPGfX5Uxt8h99mt7leO3rOHNvEqzdG1lrOvYQNgj5Ko5Us2txJKSef1nIge3O50yr5fjl+dbypc2lKr9YAuloQtf9sHlbFJbenfOfut+uCMm8rR52dTzd2NfWCDPL5N0L7SLC+R19AmbHMJTxK2Wgt4KEnYUjOX537BUP4ylOvLOF9gYrXGL8vt4IF2YujwfwlwQKuy/iWb9V2aQvMln8e0I9HmYldMsdNQTx7K2ZfTkOdZ9cHOuTImIpHPZR7TN3s+fIovlaQtFzi7JmuR51mSsK262CGkybJ/7nhWs5x9aoJaR0e3zbeZn3DuGADswdxJNetLaxnildPPlw/lyi2x1Kj8cFqGyMTD9TP4vm5bteTOEEsStn9360m0+s9+5rybmwZmk30mbHODf5KwfbKLHcKmhC3SD64muKsp1k6MW0nYAA4oX7C532YfazsJ33kq6QfTXnFXdWLXzKhf+7/MxfLzSUN53rS+iSbRW4b02epruPJ3bfuPpZl+iTaZmFPn0ev1sawnSVpi302itdaqlXn1LvT/sg9zCVsutNpY9llNy3PJpYQN4EDq/FV3amK130pmsU8flcxQntnsE8sJJSeY9HGZk/5Bfb+gNtYeI1fo3LKd7wONdIBPstJ/njbJZ+sbXSyJV5tAfK0cn2riE+X4XSBiacKWKTc+3wfLbqNn+4StxtI02q6fhrmELf/nbSy3XFuVo1uz5V65mSqlynvZHwOAE8otlvLlWks7sWzW0wSSJqTaybjWmCV5mzsp5cRa57TaFKvNoWkadUsj9qF+fufuWPGobj2ynibMlGu6bZHtS0Z6nmRaj/ZuGXkdWa9Su1hfc2rsTmPy6npBVt/H+r+ZaXASy4VdymvK+Hp/Pm2vNYKpeb9sKF+d1jNwCYAzkFFktUYsNyNvr6ojX+yPm5ZrrcNcrF6Zp+aE43LSm7t5d2RG/qvK0UkVAGBNmkty5Z8r6uumWEbb1RF3NZFIqX3f5mIfK+Mx0hTFkU+Vsek5TXZ9QpZmpjS/VandzAz9AADH9KPrqn6wAiezKusJW977NlanfkgnegCA/6mj69r5liI1Ze0kmpzcqqwnbJcP5dpmPf3/sk9G3wIAcMpWZT1h66XT99V9EACA07Eq2xO2JGuf7oMAAJyeVdmcsJ0ry25IDgDAAa3KfMKW2zdlIuOqzokHAMApW5X1hK1OTppbONXS7wMAwIHdUMYkrJ2tv96eKMt9UcMGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAw+C+9jJSCScQLLwAAAABJRU5ErkJggg==>
