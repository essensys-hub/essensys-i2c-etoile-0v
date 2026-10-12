## Purpose

Détecter, horodater et conserver la preuve des événements de surtension ou de foudre observés sur la carte N2.

## ADDED Requirements

### Requirement: Détection de surtension
Le module SHALL détecter un écart de potentiel entre les 0 V des départs de l'étoile supérieur à un seuil configurable, ainsi qu'un déclenchement de PTC.

#### Scenario: Écart de 0 V
- **WHEN** une impulsion injectée sur le banc crée un écart de 0 V au-delà du seuil
- **THEN** un événement « surtension » horodaté avec la valeur crête est enregistré

### Requirement: Conservation des preuves
Les événements MUST être stockés en mémoire non volatile, survivre à une coupure d'alimentation, et n'être effacés que par une action locale explicite.

#### Scenario: Coupure après événement
- **WHEN** l'alimentation et la batterie sont coupées après un événement
- **THEN** l'événement est relu intact au redémarrage

### Requirement: Horodatage fiable
Les événements SHALL être horodatés par une horloge maintenue sur batterie et synchronisée par NTP quand le réseau est disponible.

#### Scenario: Réseau absent
- **WHEN** un événement survient sans Wi-Fi
- **THEN** l'horodatage provient de l'horloge locale et est publié dès le retour du réseau
