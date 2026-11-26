# Analyse Approfondie : Pourquoi le Filtre de Kalman à 2 États n'a pas été Retenu

Ce document expose de manière rigoureuse les raisons mathématiques, fréquentielles, temps-réel et ergonomiques qui justifient l'éviction du **Filtre de Kalman à 2 états ($x = [\theta, \omega]^T$)** au profit d'une **Tracking Loop numérique (PLL cinématique de Type-II du 2nd ordre)** pour l'architecture d'encodeurs d'`OpenFFBoard-refactor-encoder`.

---

## 1. Ce que le Kalman 2 États Corrigeait par Rapport au Kalman 3 États

Dans `KinematicKalman.cpp`, le modèle initial à 3 états ($[\theta, \omega, \alpha]^T$) intégrait librement une accélération non amortie ($\dot{\omega} = \alpha$), provoquant une double intégration divergente de l'erreur de position et un bug critique dans la mise à jour de Joseph de `updateVelocity()`.

Le passage à un vecteur d'état réduit à 2 dimensions :
$$x_k = \begin{bmatrix} \theta_k \\ \omega_k \end{bmatrix}, \quad F(\Delta t) = \begin{bmatrix} 1 & \Delta t \\ 0 & 1 \end{bmatrix}, \quad H = \begin{bmatrix} 1 & 0 \end{bmatrix}$$
permettait effectivement d'éliminer l'emballement exponentiel dû à l'état $\alpha$. C'est pourquoi il apparaissait en première lecture comme un candidat viable. Toutefois, son analyse approfondie dans le cadre strict d'un volant Direct Drive sous FreeRTOS révèle **six limites structurelles** qui le rendent inférieur ou équivalent (en régime stationnaire) à une PLL, tout en introduisant des vulnérabilités inutiles.

---

## 2. Les 6 Raisons Techniques de son Rejet

### 2.1. Équivalence Asymptotique exacte avec un Filtre $\alpha\text{-}\beta$ / PLL (Redondance Mathématique)
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

### 2.2. Erreur de Traînage Dynamique et Paradoxe Bruit / Latence (Modèle à Vitesse Constante)
Le modèle stochastique CWNA d'ordre 2 postule que la vitesse est constante entre deux échantillons et que toute accélération n'est qu'un bruit blanc centré de densité spectrale $q_{\text{vel}}$ :
$$\dot{\theta}(t) = \omega(t), \quad \dot{\omega}(t) = w(t), \quad Q(\Delta t) = q_{\text{vel}} \begin{bmatrix} \frac{\Delta t^3}{3} & \frac{\Delta t^2}{2} \\ \frac{\Delta t^2}{2} & \Delta t \end{bmatrix}$$

Sur un volant Direct Drive (couple de $20\text{ à }30\text{ Nm}$, inertie rotor + volant $J \approx 0.02\text{ kg}\cdot\text{m}^2$), les accélérations réelles atteignent $1000\text{ à }1500\text{ rad/s}^2$ lors d'un contre-braquage ou d'un vibreur :
1. Durant toute rampe d'accélération $\alpha_{\text{réelle}}$, la prédiction $\hat{\omega}_{k|k-1} = \hat{\omega}_{k-1|k-1}$ est structurellement en retard. L'erreur de traînage vaut asymptotiquement $\Delta \omega_{\text{lag}} = \frac{\alpha_{\text{réelle}} \Delta t}{K_1}$.
2. Pour réduire ce retard via le formalisme de Kalman, il faut augmenter fortement $q_{\text{vel}}$. Mais avec le couplage rigide imposé par l'indice de Kalata ($\lambda \propto \sqrt{q_{\text{vel}}/R}$), le facteur d'amortissement équivalent du filtre de Kalman tend vers $\zeta = \frac{\sqrt{2}}{2} \approx 0.707$ (sous-amorti) au lieu de $\zeta = 1.0$.
3. **Conséquence haptique** : Un amortissement $\zeta \approx 0.707$ introduit une surtension fréquentielle (+2 dB de dépassement indiciel d'environ $4.3\%$) sur l'estimation de vitesse. Injectée directement dans l'effet Damper ($T = -B \cdot \hat{\omega}$) sans Biquad secondaire, cette surtension réduit la marge de phase de la boucle mécanique lorsque le pilote lâche le volant. La PLL permet au contraire d'imposer explicitement **$\zeta = 1.0$ (amortissement critique : pôles réels doubles, zéro dépassement)** quelle que soit la bande passante $f_n$ choisie.

### 2.3. Sensibilité au Jitter Temporel de FreeRTOS (Modulation Parasite des Gains)
Dans `Encoder.cpp` (`Encoder::triggerRead()`), le pas de temps $\Delta t$ est mesuré à la microseconde près via `micros()`, mais l'exécution dépend de l'ordonnanceur FreeRTOS, des sections critiques et des transactions SPI/DMA :
* Si une préemption retarde ponctuellement la tâche de $100\text{ }\mu\text{s}$ à $300\text{ }\mu\text{s}$ ($\times 3$), les termes de $Q(\Delta t)$ augmentent d'un facteur $27$ sur $Q_{00}$ ($\Delta t^3$) et $9$ sur $Q_{01}$ ($\Delta t^2$).
* La matrice $P$ gonfle instantanément, faisant bondir les gains $K_0$ et $K_1$ sur l'échantillon suivant, avant de redescendre sur les cycles ultérieurs.
* Cette **modulation temporelle des gains par le jitter d'ordonnancement** transforme la gigue logicielle en bruit paramétrique injecté dans la commande de courant du `TMC4671` (grésillement audible à haute fréquence dans le stator).
* Une PLL discrétisée avec $\Delta t$ borné conserve des gains proportionnel et intégral $(K_p, K_i)$ invariants face au jitter.

### 2.4. À-coups de Couple ("Glitch Torque") après Erreurs CRC BiSS-C
En environnement CEM sévère (hacheur PWM 48V / 30A à proximité du câble d'encodeur), des trames `EncoderBissC` peuvent échouer au contrôle CRC-6 :
* **Avec un Kalman 2 états** : Lorsqu'une mesure invalide est rejetée et que seule l'étape `predict(dt)` s'exécute, $P_{k|k-1} = F P F^T + Q$ croît à chaque trame manquée (croissance quadratique de $P_{00}$ via $\Delta t^2 P_{11}$). Dès le retour d'une trame CRC valide, l'innovation accumulée est multipliée par un gain $K_1 = P_{10}/(P_{00}+R)$ anormalement élevé, provoquant un pic brutal sur $\hat{\omega}$ et donc une secousse sèche dans le volant.
* **Avec la PLL** : Aucun état de covariance ne diverge en boucle ouverte. Sur rejet CRC ou dépassement du seuil d'outlier gating, l'intégrateur de vitesse est simplement maintenu avec une décroissance exponentielle douce ($\hat{\omega} \leftarrow 0.98 \cdot \hat{\omega}$), assurant une reprise sans discontinuité de couple.

### 2.5. Inadéquation Ergonomique et Couplage à la Résolution CPR
OpenFFBoard s'adresse à un parc matériel très hétérogène (ABZ $2\,500\text{ PPR} = 10\,000\text{ CPR}$, SinCos, MT6835 21 bits, BiSS-C 22 bits soit $4\,194\,304\text{ CPR}$) :
* Entre un encodeur $10\,000\text{ CPR}$ et un BiSS-C 22 bits, le pas angulaire varie d'un facteur $419$, donc la variance de bruit de mesure $R = \frac{\Delta \theta^2}{12}$ varie d'un facteur **$175\,000$** !
* Avec un Kalman 2 états réglé par un $q_{\text{vel}}$ unique (en $\text{rad}^2/\text{s}^3$), la fréquence de coupure effective du filtre ($f_c \propto (q_{\text{vel}}/R)^{1/4}$) change d'un facteur $\sim 20$ lorsqu'on change d'encodeur. Un $q_{\text{vel}}$ sain sur un ABZ donnera une bande passante inutilement agressive (> 1 kHz) sur un BiSS-C 22 bits, ou inversement un filtre ultra-lent sur ABZ.
* Exposer $q_{\text{vel}}$ et $R$ dans le configurateur GUI d'OpenFFBoard est incompréhensible pour l'utilisateur final. À l'inverse, la PLL se paramètre directement par une **fréquence propre physique $f_n$ en Hz** (ex: $120\text{ Hz}$ par défaut, réglable entre $50\text{ Hz}$ et $250\text{ Hz}$), indépendante des unités internes de variance, avec $\zeta = 1.0$ verrouillé.

### 2.6. Surcoût Arithmétique Inutile à 10 kHz sur Cortex-M4F
Sur STM32F4 (FPU simple précision `float32`, sans cache de données L1, cadencé à 168 MHz) :
* **Kalman 2 états (avec forme de Joseph pour préserver la symétrie et la positivité de $P$)** :
  * Puissances $\Delta t^2, \Delta t^3$, propagation $F P F^T + Q$, calcul de $S = P_{00} + R$, **division flottante `VDIV.F32`** (14 cycles à elle seule), calcul de $K_0, K_1$, et mise à jour de Joseph $2 \times 2$.
  * Coût total : **$\approx 45\text{ à }55\text{ cycles FPU}$** par encodeur et par tick à $10\text{ kHz}$, plus un risque de perte de précision numérique en `float32` lorsque $R \approx 1.8 \times 10^{-13}\text{ rad}^2$ (cas du BiSS-C 22 bits où $\Delta\theta = 1.5\times 10^{-6}\text{ rad}$, proche du plancher de cancellation vis-à-vis de $P_{00}$).
* **Tracking Loop (PLL 2nd ordre)** :
  * Normalisation d'erreur angulaire, 2 `VMLA.F32` (PI + clamp anti-windup), 1 intégration d'angle, **zéro division flottante**, zéro matrice.
  * Coût total : **$\approx 12\text{ cycles FPU}$**, insensible au sous-dépassement (`underflow`) flottant sur encodeurs très haute résolution.

---

## 3. Tableau Récapitulatif : Kalman 2 États vs Tracking Loop (PLL)

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

## 4. Conclusion Architecturale

Le filtre de Kalman à 2 états n'est pertinent que lorsque la variance du bruit de mesure $R(k)$ change radicalement d'un échantillon à l'autre (par exemple en poursuite radar ou fusion GPS + inertiel). Dans un servomoteur synchrone équipé d'un encodeur numérique lu à cadence fixe de $10\text{ kHz}$, le bruit de quantification est stationnaire : le filtre de Kalman dégénère vers un observateur linéaire à gains constants, mais conserve tous les effets de bord néfastes d'une covariance dynamique en présence de jitter FreeRTOS, d'erreurs CRC et de limites `float32`.

Retenir la **Tracking Loop (PLL cinématique du 2nd ordre)** à amortissement critique ($\zeta = 1.0$) offre la même optimalité asymptotique d'estimation de vitesse tout en garantissant zéro dépassement indiciel, une immunité totale aux chocs de réacquisition SPI, un coût CPU divisé par 4 et un réglage universel en Hertz.
