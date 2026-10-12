## Context

Voir proposal.md (Why). La carte N2 (KiCad, ce dépôt) relie en étoile le 0 V et distribue SDA, SCL et +15 V protégé par PTC vers les BA. Le BP SC944D est l'unique maître I2C (50 kHz, 5 V). Le BP coupe +15VSP environ 12 s après passage sur batterie. Le dépôt est public : aucune donnée de configuration privée n'y est versionnée.

## Goals / Non-Goals

**Goals:**
- Garantie matérielle (et non logicielle) qu'aucune émission sur le bus n'est possible.
- Module autonome sur batterie pour l'alarme.
- Mesures analogiques +15 V, PTC et écart de 0 V.

**Non-Goals:**
- Modifier les firmwares BP/BA ou le protocole HTTP legacy IoT.
- Remplacer une centrale d'alarme certifiée (pas de certification EN 50131 visée).
- Piloter le bus ou les BA.

## Decisions

### D1 — MCU : ESP32-C3 (module certifié radio)
ESP32-C3-MINI-1 : Wi-Fi intégré, module pré-certifié CE, RISC-V basse consommation (deep sleep ≈ 5 µA), ADC, RMT/GPIO suffisants pour décoder 50 kHz, secure boot et chiffrement flash, écosystème ESP-IDF. Alternatives : ESP32-S3 (plus de broches, consommation supérieure, inutile) ; RP2040 + module Wi-Fi (PIO idéal pour sniffer mais radio externe, deux firmwares) ; STM32 + Ethernet (pas de Wi-Fi, coûteux). Le décodage I2C à 50 kHz se fait par interruption GPIO ou RMT sans périphérique I2C en mode esclave (qui pourrait ACK).

### D2 — Écoute haute impédance : buffer unidirectionnel
Option A : GPIO avec résistances série (≥ 10 kΩ) et absence de pilote de sortie configuré. Rejetée : la garantie est logicielle ; une erreur de firmware ou une prise de contrôle peut configurer la broche en sortie et, via la résistance, perturber les niveaux ; le chemin de protection ESD du MCU charge le bus quand il est hors tension.
Option B retenue : comparateur ou buffer CMOS à entrée haute impédance, alimenté côté bus, sortie unidirectionnelle vers le MCU (ex. 74LVC1G17 alimenté 3,3 V avec diviseur résistif haute valeur et entrées tolérantes, ou isolateur numérique unidirectionnel type ISO7720/Si8620 « deux canaux même sens »). Le choix final privilégie l'isolateur numérique unidirectionnel : il n'existe physiquement aucun canal module → bus, l'entrée côté bus présente quelques pF et < 1 µA, et l'isolement découple les pannes du module. Résistances série 1 kΩ en entrée + cavalier de coupure. Côté bus l'isolateur est alimenté par un petit LDO depuis le +15 V ; hors tension, ses entrées restent haute impédance (vérifié sur datasheet et banc).

### D3 — Alimentation et batterie
+15 V → PTC propre (≈ 100 mA) → diode anti-retour/TVS → abaisseur synchrone (ex. TPS62933 ou équivalent) 5 V → chargeur → régulateur 3,3 V. Batterie LiFePO4 18650 1 S (≈ 1,5 Ah) : chimie sûre en armoire, tolérante, tension 3,2 V proche du 3,3 V ; chargeur CC/CV 3,6 V limité à 200 mA avec sonde de température (CN3058 ou équivalent) ; power-path pour basculement sans coupure. Autonomie cible ≥ 24 h armée avec Wi-Fi en économie (moyenne ≈ 40 mA). Alternative Li-ion 2 S rejetée (risque thermique, chargeur plus complexe) ; supercondensateur rejeté (autonomie insuffisante).

### D4 — Mesures analogiques
ADC externe isolé côté bus ou ADC ESP32 via ponts diviseurs haute valeur et buffer : +15 V amont/aval de chaque PTC, écarts entre 0 V des départs (amplificateur différentiel, protection TVS et résistances série). Un comparateur rapide verrouillé (latch) capture les impulsions trop brèves pour l'ADC ; le MCU lit et réarme le latch. Horloge RTC sauvegardée (RV-3028, < 100 nA).

### D5 — Alarme TBT
Entrées boucle équilibrée (résistances de fin de ligne) mesurées par ADC, protégées TVS. Sortie sirène 12 V par MOSFET avec limitation de courant, alimentée par le rail batterie via booster dédié. Tout reste en TBTS, 0 V jamais relié au PE.

### D6 — Réseau et sécurité
Wi-Fi WPA2/WPA3, MQTT sur TLS avec identifiants par appareil provisionnés hors dépôt (NVS chiffrée), secure boot et chiffrement flash activés, OTA signée, aucune commande MQTT ne touche au bus (seulement alarme et consultation). Topics `essensys/n2/<id>/…`. Le serveur s'y abonne sans changer le protocole HTTP legacy.

### D7 — Budget de consommation
Pire cas sur +15 V : charge 200 mA × 3,6 V + module 250 mA crête × 3,3 V ≈ 1,6 W → ≈ 120 mA à 15 V (rendement 90 %). La PTC propre du module la sépare des départs BA ; le budget du +15 V global est vérifié avec PTC ≥ 1,5 × BA_ALIM, chaque départ BA restant inchangé.

## Risks / Trade-offs

- [Isolateur charge le bus hors tension] → vérification banc NR dédiée, module alimenté et non alimenté.
- [Décodage I2C manqué à 50 kHz sous charge Wi-Fi] → décodage sur cœur dédié/RMT, compteur de trames perdues publié.
- [Batterie en armoire chaude] → LiFePO4, sonde CTN, charge inhibée hors 0–45 °C.
- [Fuite de données sur dépôt public] → secrets uniquement en NVS chiffrée, grep de confidentialité en CI.
- [Alarme non certifiée] → documenté comme aide à la détection.

## Migration Plan

Nouvelle révision de la carte N2 ; l'ancienne révision reste utilisable. Retour arrière : retrait du cavalier d'écoute et de la PTC du module, la carte se comporte comme la révision précédente.

## Open Questions

- Référence exacte de l'isolateur et de l'abaisseur selon disponibilité au moment de la commande.
