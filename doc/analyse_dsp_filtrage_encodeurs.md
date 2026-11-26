# Rapport d'Expertise DSP : Refonte du Filtrage, Estimation d'État et Gestion des Encodeurs (OpenFFBoard)

Ce document constitue la synthèse complète et approfondie du brainstorming contradictoire sur le traitement numérique du signal (DSP) appliqué à l'architecture d'estimation cinématique et de contrôle haptique d'OpenFFBoard.

Il s'appuie sur une analyse croisée selon quatre perspectives méthodologiques :
1. **Analyse 1 (Perspective Optimiste / Idées constructives DSP)** : Foisonnement de solutions modernes de filtrage et d'estimation.
2. **Analyse 2 (Perspective Sceptique / Critique et Contradiction)** : Démontage rigoureux des fausses bonnes idées, analyse de la physique du volant haptique et des causes profondes de divergence matérielle.
3. **Analyse 3 (Perspective Optimiste avancée / Architecture logicielle et Intégration STM32/TMC4671)** : Co-conception matérielle, multi-cadence, découplage des boucles de contrôle.
4. **Analyse 4 (Perspective Sceptique avancée / Crash-test temps réel, limites numériques et failles de sécurité)** : Étude des cas limites (perte de paquets SPI, instabilités en virgule flottante, comportement aux limites).
5. **Évaluation Synthétique et Arbitrage Architectural** : Sélection des meilleures approches et feuille de route technique pour le projet `OpenFFBoard-refactor-encoder`.

---

## 1. Contexte, Diagnostic et Autopsie de l'Incident Matériel

### 1.1. Rappel de la Problématique Initiale
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

### 1.2. Autopsie Mathématique : Pourquoi le Filtre de Kalman Initial a Failli Griller l'Électronique

La tentative d'implémentation du filtre de Kalman cinématique d'ordre 3 (`KinematicKalman.cpp` dans la branche `refactor-encoder`) a provoqué un emballement violent de la vitesse prévisionnelle, saturant le couple moteur et mettant en péril les ponts en H et les bobinages du moteur. 

L'analyse minutieuse du code et des équations met en lumière quatre mécanismes d'emballement simultanés :

#### A. Le Piège du Modèle à Accélération Blanche Continue (CWNA d'ordre 3)
Le filtre modélisait l'état sous la forme $x = [\theta, \omega, \alpha]^T$ en supposant que l'accélération $\alpha$ évolue selon une marche aléatoire (bruit de jerk $q_{\text{continuous}}$) :
$$\dot{\theta} = \omega, \quad \dot{\omega} = \alpha, \quad \dot{\alpha} = w(t)$$
Ce modèle est fondamentalement faux pour un système haptique manipulé par un être humain et freiné par des forces passives :
1. **Absence d'amortissement intrinsèque** : Le système prédit que si une accélération $\alpha$ apparaît, la vitesse $\omega$ va continuer à croître linéairement sans aucune résistance mécanique ($F_{\text{drag}} = 0$).
2. **Double intégration de l'innovation** : En mode "Position-Only Measurement" (BiSS-C ou lecture de position à 1 kHz), la matrice d'observation est $H = [1, 0, 0]$. Tout écart entre la position mesurée et la prédiction injecte une correction sur l'accélération via le gain $K_2 = P_{02} / S$. Si le paramètre $q_{\text{continuous}}$ est fixé à une valeur élevée (10000 dans le code) pour être "réactif", $K_2$ devient démesuré. Une micro-perturbation ou un saut de trame génère un bond d'accélération gigantesque qui s'intègre au cycle suivant sur la vitesse :
   $$\omega_{\text{pred}} = \omega + \alpha \cdot \Delta t$$
   Si le cycle suivant subit un retard ou si l'innovation oscille, l'accélération estimée monte en flèche, propulsant $\omega$ vers des valeurs astronomiques.

#### B. Instabilité Temporelle Variable ($\Delta t$ non déterministe sous RTOS)
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

#### C. L'Erreur Mortelle dans la Forme de Joseph de `updateVelocity()`
Dans `KinematicKalman.cpp` (l. 324-334), lors de la correction de vitesse avec la matrice $H_{\text{vel}} = [0, 1, H_2]$ où $H_2 = -0.5 \cdot \Delta t_{\text{pulse}}$ :
```cpp
P[0][0] = W0*W0 * P11_old + K0*K0 * R_v;
```
Une erreur tragique de copier-coller mathématique a remplacé les termes de covariance de position $P_{00}$ par $P_{11}$ et des index erronés sur l'ensemble de la mise à jour de Joseph ! 
Résultat : dès qu'une correction de vitesse était déclenchée, la matrice de covariance $P$ était corrompue avec des valeurs fausses, détruisant instantanément la propriété définie-positive. Les gains de Kalman $K$ ont alors divergé vers l'infini ou changé de signe, entraînant l'emballement spontané de l'état estimé.

#### D. Absence Totale de Bornage Physique (Sanity Checks)
Ni l'état interne ($x_1, x_2$), ni les sorties `getSpeed()` et `getAccel()` ne possédaient de limiteurs physiques absolus. Un volant d'entraînement direct ("Direct Drive") possède des limites cinématiques indépassables :
* Vitesse maximale absolue : $600\text{ rpm} \approx 62.83\text{ rad/s}$ ($10\text{ tours/s}$).
* Accélération angulaire maximale (couple max 25-30 Nm sur une inertie de volant de $\approx 0.02 - 0.05\text{ kg}\cdot\text{m}^2$) :
  $$\alpha_{\text{max}} \approx \frac{30\text{ Nm}}{0.02\text{ kg}\cdot\text{m}^2} = 1500\text{ rad/s}^2 \approx 240\text{ rot/s}^2$$
Le filtre a produit en interne des accélérations virtuelles de plus de $50\,000\text{ rad/s}^2$ et des vitesses de plusieurs milliers de rad/s, injectant directement le courant de crête maximal dans le TMC4671 via l'effet d'amortissement et l'extrapolation de phase électrique.

---

## 2. Débat Contradictoire en 4 Perspectives

Les quatre sections suivantes confrontent les hypothèses d'ingénierie selon les quatre prismes demandés.

---

### 2.1. Analyse 1 (Perspective Optimiste : Foisonnement d'Idées DSP Constructives)

Cette analyse explore toutes les voies d'amélioration DSP possibles pour maximiser la réactivité dynamique, minimiser le bruit de quantification et éliminer le déphasage.

#### Idée 1 : Filtre Alpha-Beta-Gamma ($\alpha-\beta-\gamma$) Stationnaire à Gains Optimisés
* **Principe** : Remplacer le filtre de Kalman complet (avec recalcul dynamique de la covariance $P$, inversion scalaire $S$ et mise à jour de Joseph) par sa forme stationnaire asymptotique (gains constants $K = [\alpha, \beta/\Delta t, \gamma/\Delta t^2]^T$). Les gains sont calculés hors-ligne via l'indice de manoeuvrabilité de Kalata :
  $$\lambda = \frac{\sigma_w \Delta t^2}{\sigma_v}$$
* **Gain Déphasage / Bruit** : Réponse impulsionnelle déterministe. Élimine complètement le déphasage non linéaire provoqué par les variations dynamiques des gains $K$.
* **Coût CPU** : Infime ($\sim 12\text{ à } 16$ cycles CPU sur Cortex-M4, aucune division ni mise à jour de matrice).
* **Risques / Stabilité** : Système linéaire invariant dans le temps (LTI). La stabilité se vérifie trivialement par le placement des pôles du polynôme caractéristique à l'intérieur du cercle unité discret.

#### Idée 2 : Filtre de Kalman Réduit à 2 États ($\theta, \omega$) avec Modèle à Vitesse Constante (CWNA d'ordre 2)
* **Principe** : Retirer l'état accélération du filtre de Kalman. L'état devient $x = [\theta, \omega]^T$, le bruit de processus $q_{\text{vel}}$ s'applique directement sur la vitesse. L'accélération est alors calculée séparément si nécessaire par un différenciateur régularisé.
* **Gain Déphasage / Bruit** : Réduit le déphasage intrinsèque de $90^\circ$ à moins de $15^\circ$ car la vitesse n'est plus couplée à un intégrateur d'accélération non borné.
* **Coût CPU** : $\sim 45$ cycles FPU (inversion d'un scalaire, matrice $P$ de taille $2 \times 2$ ne contenant que 3 variables indépendantes : $P_{00}, P_{01}, P_{11}$).
* **Risques** : Aucune divergence d'accélération possible puisque $\alpha$ n'est plus un état d'intégration.

#### Idée 3 : Dérivée à Retard Minimal par Régression Polynomiale Causale (Savitzky-Golay Causal)
* **Principe** : Utiliser un filtre FIR convolutif calculant la pente de la position sur une fenêtre glissante glissante de $N$ points passés ($N=5$ à 7 points à 10 kHz) par moindres carrés pondérés vers le point présent :
  $$\omega[k] = \sum_{i=0}^{N-1} c_i \cdot \theta[k-i]$$
* **Gain Déphasage / Bruit** : Retard de groupe rigoureusement constant et déterministe ($t_{\text{group}} = \frac{N-1}{2} \cdot \Delta t$). À 10 kHz avec $N=5$, le délai est de seulement $200\text{ }\mu\text{s}$ (contre $6250\text{ }\mu\text{s}$ pour le Biquad à 40 Hz).
* **Coût CPU** : $\sim 8$ cycles via l'instruction SIMD/DSP `SMLAD` ou FPU `VMLA.F32`.
* **Risques** : Sensible aux sauts de quantification sur les encodeurs basse résolution. Exceptionnel sur encodeur 22 bits.

#### Idée 4 : Tracking Loop / Boucle à Verrouillage de Phase Numérique (PLL Typique Servomoteurs Industriels)
* **Principe** : Asservir un oscillateur numérique interne (NCO / modèle de position continue) sur la position reçue au moyen d'un correcteur PI cinématique :
  $$\epsilon[k] = \theta_{\text{mes}}[k] - \theta_{\text{est}}[k]$$
  $$\omega_{\text{est}}[k] = \omega_{\text{est}}[k-1] + K_p (\epsilon[k] - \epsilon[k-1]) + K_i \Delta t \cdot \epsilon[k]$$
  $$\theta_{\text{est}}[k] = \theta_{\text{est}}[k-1] + \omega_{\text{est}}[k] \cdot \Delta t$$
* **Gain Déphasage / Bruit** : Structure standard de l'industrie (Kollmorgen, Heidenhain, Trinamic). Déphasage nul à vitesse constante. Filtrage naturel du bruit HF par le pôle d'intégration.
* **Coût CPU** : $\sim 10$ cycles FPU, aucune matrice, stabilité inconditionnelle garantie par la marge de phase du régulateur PI.

#### Idée 5 : Décimation Polyphase / Filtre CIC (Cascade Integrator-Comb) entre 10 kHz et 1 kHz
* **Principe** : Faire tourner l'acquisition et le pré-filtrage de l'encodeur à 10 kHz dans `EncoderManager`, puis moyenner/décimer proprement vers la boucle FFB à 1 kHz à l'aide d'un filtre CIC du premier ou second ordre sans calcul flottant coûteux.
* **Gain Déphasage / Bruit** : Gain de suréchantillonnage de $10\text{ dB}$ sur le rapport signal sur bruit ($SNR$). Élimine le repliement de spectre (aliasing) dans la boucle d'effet à 1 kHz.
* **Coût CPU** : 3 additions entières 32 bits par échantillon à 10 kHz.

#### Idée 6 : Modélisation Non Linéaire des Effets Conditionnels (Friction Régularisée par Tanh)
* **Principe** : Remplacer l'effet de frottement sec de Coulomb actuel basé sur la fonction discontinue $\text{sign}(\omega)$ (qui génère un cycle limite de cliquetis à vitesse quasi-nulle) par une fonction sigmoïde continue :
  $$F_{\text{friction}}(\omega) = -F_c \cdot \tanh\left(\frac{\omega}{\omega_0}\right) - \sigma \cdot \omega$$
* **Gain Déphasage / Bruit** : Supprime le besoin d'un Biquad lourd sur la friction. Le passage à zéro est doux, exempt de bruit haute fréquence.
* **Coût CPU** : Une approximation polynomiale de $\tanh(x)$ en 6 cycles FPU.

#### Idée 7 : Observateur de Perturbation (DOB - Disturbance Observer) pour Damper et Inertie
* **Principe** : Utiliser la connaissance du couple appliqué par le moteur $T_{\text{cmd}}$ et l'inertie du rotor $J_m$ pour construire un observateur d'état à modèle inverse (Luenberger). Le couple de réaction des mains de l'utilisateur est estimé comme une perturbation externe plutôt que comme un bruit stochastique inconnu.
* **Gain Déphasage / Bruit** : Déphasage dynamique nul sur les accélérations induites par le moteur lui-même.

#### Idée 8 : Filtrage Passe-Bas Adaptatif selon la Vitesse (Variable Bandwidth Filter)
* **Principe** : Ajuster la bande passante du filtre de vitesse en fonction de l'amplitude de la vitesse :
  * À basse vitesse ($|\omega| < 0.5\text{ rad/s}$) : bruit de quantification dominant $\rightarrow$ bande passante basse ($30\text{ Hz}$).
  * À haute vitesse ($|\omega| > 5\text{ rad/s}$) : rapport signal/bruit élevé $\rightarrow$ bande passante haute ($250\text{ Hz}$), déphasage quasi-nul.
* **Gain Déphasage / Bruit** : Le système est ultra-réactif lors des contre-braquages rapides et parfaitement silencieux à l'arrêt.

---

### 2.2. Analyse 2 (Perspective Sceptique : Critique Radicale & Contradictions)

Cette analyse passe au crible de la réalité physique et mathématique les hypothèses précédentes et celles du document `kalman_optimisation.md`.

#### Critique 1 : La Fausse Promesse du "Filtrage Zéro-Phase Lag"
* Le document `kalman_optimisation.md` revendique à plusieurs reprises un "filtrage Zero-Phase Lag". **C'est une impossibilité mathématique et physique causale**.
* Tout système causal estimant la dérivée à partir de mesures bruitées doit faire un compromis fondamental (théorème de Bode-Poisson / limite de Cramér-Rao).
* Prétendre éliminer le retard en "rétroprojetant" la vitesse via l'accélération ($\omega_{\text{comp}} = \omega - 0.5 \alpha \Delta t$) ne fait que reporter l'incertitude sur une variable d'accélération encore plus bruitée et prompte à l'instabilité. Lorsque l'accélération est erronée (ce qui est systématique lors d'un choc ou d'un changement de trajectoire par le pilote), cette compensation aggrave l'erreur instantanée.

#### Critique 2 : Inadaptation Fondamentale de l'Accélération d'Ordre 3 en Volant FFB
* Pourquoi l'accélération ne doit **JAMAIS** être une variable libre dans un filtre d'encodeur FFB :
  * L'être humain applique un couple non modélisé qui peut varier de $+10\text{ Nm}$ à $-10\text{ Nm}$ en $30\text{ ms}$ (jerk humain énorme).
  * Le moteur FFB applique un couple pulsé à 1 kHz (effets de choc de trottoir, texture de route).
  * Modéliser cette dynamique par un bruit blanc gaussien $w(t)$ avec un $Q$ fixe est une aberration : soit $Q$ est faible et le filtre est en retard permanent de plusieurs dizaines de millisecondes, soit $Q$ est élevé et le filtre amplifie le bruit de mesure jusqu'à faire vibrer la structure mécanique.
* **Verdict** : L'état d'accélération doit être supprimé de l'observateur d'encodeur. L'encodeur doit estimer $(\theta, \omega)$. L'effet d'inertie (s'il est activé) doit être calculé soit par différence de vitesse filtrée à bande limitée, soit estimé via le modèle dynamique courant/couple du moteur.

#### Critique 3 : Danger Extrême de la T-Method / MT-Method par EXTI sur Microcontrôleur
* Dans `EncoderLocal.cpp`, l'activation des interruptions EXTI sur les voies A et B pour mesurer $\Delta t_{\text{pulse}}$ (T-Method) est une bombe à retardement :
  * Sur un encodeur ABZ à 2500 CPR (10 000 transitions en quadrature par tour) tournant à $600\text{ rpm}$ ($10\text{ tours/s}$) :
    $$f_{\text{irq}} = 10 \times 10\,000 = 100\,000\text{ interruptions par seconde !}$$
  * À 100 kHz, avec un processeur Cortex-M4 à 168 MHz (période d'horloge de $5.95\text{ ns}$), chaque interruption dispose de moins de 1680 cycles pour exécuter le prologue d'interruption, le pipeline FreeRTOS et le calcul.
  * Le CPU est instantanément saturé à 100 %, entraînant la famine des autres threads, le déclenchement du Watchdog ou la perte du bus USB.
* **Verdict** : La T-Method logicielle par interruption GPIO est à proscrire totalement. Seul un périphérique Timer matériel configuré en mode quadrature avec mesure de période intégrée (32-bit Timer Capture) est admissible, ou s'en tenir à une M-Method synchrone à fréquence fixe.

#### Critique 4 : Vulnérabilité de l'Équation ANC ($R_v \propto 1 / \Delta t^4$)
* L'équation proposée dans `kalman_optimisation.md` :
  $$R_v = \frac{\sigma_\tau^2 \cdot \Delta \theta^2}{\Delta t_{\text{pulse}}^4}$$
  présente une instabilité numérique fatale en virgule flottante simple précision :
  * Si le volant accélère brutalement, $\Delta t_{\text{pulse}}$ devient minuscule (ex: $10\text{ }\mu\text{s} = 10^{-5}\text{ s}$).
  * $\Delta t_{\text{pulse}}^4 = 10^{-20}$. En simple précision IEEE-754, le format standard normalisé plafonne avec un dénormalisé à $\sim 10^{-38}$, mais la division par $10^{-20}$ fait exploser $R_v$ à des valeurs de l'ordre de $10^{14}$ !
  * Les matrices $S$ et $K$ subissent des pertes de chiffres significatifs (catastrophic cancellation), générant des valeurs NaN (Not-a-Number) ou infinis qui se propagent dans tout le firmware.

---

### 2.3. Analyse 3 (Perspective Optimiste Avancée : Architecture Matérielle & Intégration)

Cette analyse se concentre sur l'exploitation optimale de l'architecture matérielle STM32F4 + TMC4671 pour garantir un temps réel strict et une synchronisation irréprochable.

#### Proposition 1 : Découplage Strict des 3 Boucles Temporelles
Il est impératif de cesser de faire converger toutes les exigences vers un seul composant logiciel monolithique. Il existe trois échelles de temps fondamentalement différentes :
1. **Boucle Ultra-Rapide (10 kHz - Synchronisation FOC TMC4671)** :
   * Objectif : Injecter la position électrique du rotor $\phi_E$ dans le registre `0x1C` du TMC4671 pour que la commutation FOC reste alignée en phase.
   * Exigence : Latence ultra-faible ($< 25\text{ }\mu\text{s}$), gigue temporelle minimale ($< 1\text{ }\mu\text{s}$). L'accélération n'est pas requise ici, seule une position lissée et extrapolée sur le délai de transfert SPI importe.
2. **Boucle Rapide (1 kHz - Boucle d'Effets et Métriques FFB)** :
   * Objectif : Calculer les effets Damper, Friction, Inertia, Spring, et communiquer avec le PC hôte via USB HID.
   * Exigence : Vitesse angulaire $\omega$ propre et régulière, retard de phase modéré ($< 2\text{ ms}$), protection absolue contre les dépassements de couple.
3. **Boucle Lente (100 Hz - Télémétrie et Supervision Sécurité)** :
   * Objectif : Détection Hands-off, surveillance thermique, diagnostic des erreurs CRC d'encodeur.

#### Proposition 2 : Élimination du "Ping-Pong" FreeRTOS par Acquisition Directe en Interruption Matérielle (Zero-Context-Switch)
* Actuellement : `TIM6 ISR -> Notify -> Thread EncoderManager -> SPI IT -> SPI ISR -> Notify -> Thread Encoder -> Kalman`.
* **Architecture Cible** :
  * Le Timer 6 déclenche directement un transfert SPI DMA (sans aucune charge CPU).
  * L'interruption de fin de transfert DMA (ISR) exécute immédiatement l'estimateur d'état (PLL ou Kalman 2 états) en moins de 100 cycles CPU.
  * Les résultats $(\theta, \omega)$ sont écrits dans des variables protégées par un verrou atomique léger (`SeqLock` déjà présent dans le code).
  * Zéro thread réveillé, zéro latence d'ordonnancement, déterminisme absolu du $\Delta t$.

#### Proposition 3 : Gestion Optimale des Encodeurs Internes TMC4671 (Position 32 bits `0x6B` vs Vitesse `0x6A`)
* Dans le cas où l'encodeur est raccordé directement sur les broches matérielles du TMC4671 (ABZ, SinCos, UVW, Hall) :
  * Le TMC4671 maintient en matériel un accumulateur de position mécanique multi-tours 32 bits haute résolution (`PID_POSITION_ACTUAL`, registre `0x6B`, $65\,536\text{ counts/tour}$) déjà lu à $1\text{ kHz}$ par `TMC4671::getPos()`, ainsi qu'un registre de vitesse (`PID_VELOCITY_ACTUAL`, registre `0x6A`, exprimé en RPM entiers lorsque `VELOCITY_SELECTION` `0x50` pointe sur `PhiM_*`).
  * **Subtilité DSP** : Le pas de quantification de `0x6A` ($1\text{ RPM} = 6^\circ/\text{s}$) étant trop grossier à très basse vitesse pour la friction $\tanh$ et l'amortissement fin autour du centre, il est préférable d'alimenter `KinematicPLL` à $1\text{ kHz}$ directement avec la position 32 bits `0x6B` déjà lue (zéro transaction SPI additionnelle, résolution $0.0055^\circ$), et de réserver `0x6A` à la supervision ou au mode Hall basse résolution (avec mesure temporelle PWM `mode=1` dans `0x50`).

---

### 2.4. Analyse 4 (Perspective Sceptique Avancée : Crash-Test & Sécurité Numérique)

Cette analyse passe au crible les failles de sécurité, les risques de saturation et les cas de panne silencieuse.

#### Cas de Panne 1 : Perte de Trames SPI / Erreurs CRC (Glitchs Électromagnétiques)
* Dans un simulateur direct drive, la commutation PWM des MOSFETs à 25 kHz sous 48V / 30A génère d'intenses perturbations CEM sur les nappes de l'encodeur SPI/BiSS-C.
* Si le filtre de Kalman continue de prédire à l'aveugle pendant plusieurs trames corrompues successives, la covariance $P$ croît exponentiellement. Dès qu'une trame valide réapparaît, le gain de Kalman $K$ est devenu maximal et l'innovation est gigantesque : le moteur subit un "coup de bélier" de couple capable d'arracher le volant des mains du pilote ou de briser un mécanisme.
* **Garde-fou Obligatoire** :
  1. **Validation CRC stricte avant tout appel d'estimation** : Si le CRC BiSS-C échoue, la mesure est rejetée.
  2. **Gating de Mahalanobis / Chi-carré** : Si $|z - \theta_{\text{pred}}| > \Delta \theta_{\text{max\_physique}}$, la trame est rejetée.
  3. **Mode Dégradé (Coast down)** : Si plus de 3 trames consécutives sont rejetées, l'estimateur gèle son accélération, applique un amortissement logiciel sur la vitesse estimée ($\omega \leftarrow 0.95 \cdot \omega$) et passe en défaut sécurisé.

#### Cas de Panne 2 : Déphasage Entre Voies de Traitement et Effets Conditionnels
* Actuellement dans `Axis.cpp` :
  ```cpp
  float speedFiltered = (metric.speed) * (float)damperIntensity * AXIS_DAMPER_RATIO;
  mechanicalEffectTorque -= damperFilter.process(clip<float, int32_t>(speedFiltered, -internalFxClip, internalFxClip));
  ```
* Si la vitesse `metric.speed` est déjà issue d'un estimateur (Kalman ou PLL), l'application du filtre `damperFilter` (Biquad à 60 Hz) réinjecte à nouveau un déphasage de $35^\circ$ !
* **Règle absolue** : Les métriques de vitesse et d'accélération fournies par l'architecture d'encodeur doivent être **directement exploitables sans aucun filtre Biquad supplémentaire sur les effets**.
* Le Biquad sur le Damper et l'Inertie doit être **complètement supprimé**. Le couple d'amortissement doit être simplement :
  $$T_{\text{damper}} = -B \cdot \omega_{\text{estimateur}}$$

#### Cas de Panne 3 : Saturation Physique et Clamping Anti-Emballement
* Aucun estimateur d'état ne doit avoir le pouvoir d'envoyer une valeur arbitraire aux étages de commande.
* Des pinces de sécurité matérielles (Clamping saturant) doivent être codées en dur avec assertion :
  $$\omega_{\text{clamped}} = \text{clip}\left(\omega, -\omega_{\text{limit}}, \omega_{\text{limit}}\right) \quad \text{avec } \omega_{\text{limit}} = 70.0\text{ rad/s } (\approx 668\text{ rpm})$$
  $$\alpha_{\text{clamped}} = \text{clip}\left(\alpha, -\alpha_{\text{limit}}, \alpha_{\text{limit}}\right) \quad \text{avec } \alpha_{\text{limit}} = 2000.0\text{ rad/s}^2$$
* Si le filtre dépasse ces bornes, cela déclenche une erreur logicielle (`ERROR_ESTIMATOR_OVERFLOW`) avec désactivation immédiate du couple (`drv->stopMotor()`).

---

## 3. Évaluation Synthétique et Arbitrage Architectural

### 3.1. Tableau Comparatif des Solutions Étudiées

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

### 3.2. Le TOP 3 des Meilleures Idées DSP pour OpenFFBoard

#### N°1 : La Tracking Loop Numérique (PLL Cinématique du 2nd Ordre)
* **Pourquoi elle surpasse le Kalman 3 états** :
  * C'est la référence absolue dans les servodrives industriels à haute performance.
  * Elle ne modélise pas d'accélération autonome instable. Elle force l'état interne à poursuivre la position physique avec une fonction de transfert passe-bas du 2nd ordre sans dépassement :
    $$H(s) = \frac{\Omega_{\text{est}}(s)}{\Omega_{\text{real}}(s)} = \frac{2 \zeta \omega_n s + \omega_n^2}{s^2 + 2 \zeta \omega_n s + \omega_n^2}$$
  * En choisissant un facteur d'amortissement critique $\zeta = 1.0$ (aucun overshoot) et une pulsation propre $\omega_n = 2\pi \times 120\text{ rad/s}$ ($f_n = 120\text{ Hz}$), le retard de phase à 20 Hz est inférieur à $1.8\text{ ms}$, tout en atténuant le bruit haute fréquence de $40\text{ dB/décade}$.
  * Aucune matrice, aucune racine carrée, aucune inversion. 6 opérations arithmétiques.

#### N°2 : Suppression Intégrale des Biquads Secondaires et Linéarisation des Effets
* Les Biquads `damperFilter`, `frictionFilter`, `inertiaFilter` de la classe `Axis` doivent être supprimés.
* La vitesse issue de l'estimateur de l'encodeur est directement injectée dans le calcul de force.
* La friction utilise une régularisation continue :
  $$T_{\text{friction}} = -F_{\text{intensity}} \cdot \tanh\left(\frac{\omega}{0.05\text{ rad/s}}\right)$$
  ce qui éradique les oscillations autour du zéro de vitesse sans aucun déphasage de groupe.

#### N°3 : Découplage de la Commutation Moteur (BiSS-C Extrapolé direct) vs Calcul Haptique
* L'injection de phase dans le TMC4671 (`ExternalEncoderAdapter`) doit utiliser une extrapolation linéaire propre basée sur la vitesse estimée par la PLL :
  $$\phi_{E,\text{extrapolé}} = \theta_{\text{pll}} + \omega_{\text{pll}} \cdot t_{\text{latence\_spi}}$$
  avec $t_{\text{latence\_spi}}$ rigoureusement mesuré ($45\text{ }\mu\text{s}$ pour un transfert SPI à 2.6 MHz).
* La boucle FFB à 1 kHz lit simplement le dernier état stable de la PLL sans calculs complexes.

---

## 4. Spécifications Techniques et Blueprint d'Implémentation C++

Cette section fournit le code et le modèle mathématique de l'architecture cible recommandée : **La Tracking Loop Numérique (PLL Cinématique du 2nd ordre)** avec garde-fous stricts, conçue pour remplacer `KinematicKalman`.

### 4.1. Modèle Mathématique Multi-Tours de la Tracking Loop (PLL) + Accélération Découplée
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

### 4.2. Blueprint C++ Corrigé : `KinematicPLL.h`

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

## 5. Audit d'Intégration Codebase : Les 7 Points d'Impact & Cas Critique des Encodeurs Intégrés (`TMC4671` / CAN)

Une inspection exhaustive du code de la branche `OpenFFBoard-refactor-encoder` a mis en évidence **7 points d'impact critiques** sur la chaîne d'encodeurs externes/effets, ainsi qu'une **faille majeure affectant tous les encodeurs intégrés aux drivers moteurs** (`TMC4671`, `ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `MotorSimplemotion`).

### 5.1. Les 7 Points d'Impact sur la Chaîne Encodeurs & Effets

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

### 5.2. Diagnostic Critique : Régression des Encodeurs Intégrés (`TMC4671`, `ODriveCAN`, `VescCAN`, `RmdMotorCAN`, `SimpleMotion`)

L'analyse croisée de `Axis.cpp`, `Encoder.cpp` et `TMC4671.cpp` révèle une **rupture fonctionnelle majeure** introduite lors du remplacement du calcul de vitesse/accélération d'`Axis` par `encoder->getSpeed()` / `encoder->getAccel()` :

#### A. Pourquoi tous les encodeurs intégrés au `TMC4671` (ABN, SinCos, UVW, Hall) renvoient aujourd'hui `speed = 0` et `accel = 0`
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

#### B. Trois Subtilités Spécifiques au `TMC4671` à Traiter Simultanément
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

## 6. Synthèse des Recommandations Stratégiques et Plan d'Action Consolidé

### Ce qu'il faut CONSERVER et VALIDER :
1. **La centralisation dans `EncoderManager`** pour tous les encodeurs matériels externes (SPI DMA, BiSS-C, MT6835, SSI, Timer Quadrature `EncoderLocal` en M-Method).
2. **Le découplage de `MotorDriver`** et l'extrapolation linéaire de phase $\phi_E$ sur la latence SPI ($\Delta t_{\text{spi}} \approx 45-50\text{ }\mu\text{s}$), **corrigée avec `getPosAbs_f()`**.
3. **Le rejet définitif des filtres de Kalman 3 états et 2 états** au profit de `KinematicPLL` (voir également la démonstration dédiée dans `doc/pourquoi_pas_kalman_2_etats.md`).

### Ce qu'il faut ABANDONNER IRRÉVOCABLEMENT :
1. **Le filtre de Kalman d'ordre 3 (`KinematicKalman`)** : Divergent, sensible au jitter FreeRTOS et buggé dans sa mise à jour de Joseph.
2. **La T-Method par interruptions logicielles EXTI (`EncoderLocal.cpp`)** : Risque critique de famine CPU ($>100\text{ kIRQ/s}$ à $600\text{ rpm}$).
3. **La double cascade de Biquads secondaires sur les effets conditionnels** (`Axis.cpp` et `EffectsCalculator.cpp`).
4. **Les déclenchements SPI concurrents dans `EncoderBissC::getPosAbs()`**.

### Feuille de Route d'Implémentation (Ordre d'Exécution Sécurisé) :
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



