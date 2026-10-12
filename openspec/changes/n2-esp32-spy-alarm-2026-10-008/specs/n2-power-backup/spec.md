## Purpose

Alimenter le module de la carte N2 depuis le +15 V du bus sans compromettre la TBTS ni les PTC, et le maintenir sur batterie locale.

## ADDED Requirements

### Requirement: Alimentation protégée
Le module SHALL être alimenté depuis le +15 V par un abaisseur protégé (fusible/PTC propre, protection inversion et surtension) dont le défaut MUST NOT faire chuter le +15 V des BA.

#### Scenario: Court-circuit du module
- **WHEN** l'alimentation du module est court-circuitée
- **THEN** sa protection propre s'ouvre et les BA restent alimentés

### Requirement: Budget de consommation
La consommation du module sur le +15 V, charge batterie comprise, MUST rester compatible avec un dimensionnement des PTC ≥ 1,5 × BA_ALIM sans déclenchement intempestif.

#### Scenario: Charge maximale
- **WHEN** la batterie est vide et le Wi-Fi émet
- **THEN** le courant total mesuré reste sous le budget documenté et aucune PTC ne déclenche

### Requirement: Batterie de secours
Le module SHALL disposer d'une batterie locale avec chargeur et basculement sans coupure, offrant au moins 24 h d'autonomie en alarme armée, et signaler la batterie faible.

#### Scenario: Passage sur batterie
- **WHEN** le BP coupe le +15 V
- **THEN** le module continue sans redémarrage et publie l'événement « sur batterie »

### Requirement: TBTS préservée
Aucun élément ajouté MUST NOT relier le 0 V au PE.

#### Scenario: Contrôle d'isolement
- **WHEN** l'isolement 0 V–PE est mesuré sur la carte assemblée
- **THEN** aucune liaison n'est détectée
