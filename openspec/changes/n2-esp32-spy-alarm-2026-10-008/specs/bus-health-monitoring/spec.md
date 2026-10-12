## Purpose

Détecter les défauts du bus I2C et de son alimentation +15 V sur la carte N2 et les signaler sans agir sur le bus.

## ADDED Requirements

### Requirement: BA muet
Le module SHALL signaler un BA dont l'adresse ne reçoit plus d'ACK pendant un délai configurable (défaut 10 s).

#### Scenario: Retrait d'un BA
- **WHEN** un BA est débranché sur le banc
- **THEN** une alerte « BA muet » portant son adresse est publiée dans le délai configuré

### Requirement: Ligne collée
Le module SHALL signaler une ligne SDA ou SCL maintenue basse plus de 100 ms.

#### Scenario: SDA forcée basse
- **WHEN** SDA est forcée à 0 V sur le banc (cas NR-board-sc944d-3)
- **THEN** une alerte « ligne collée SDA » est publiée et la fin du défaut est journalisée

### Requirement: Surveillance du +15 V et des PTC
Le module SHALL mesurer le +15 V en amont et en aval de chaque PTC et signaler une chute sous un seuil configurable ou une PTC déclenchée.

#### Scenario: PTC déclenchée
- **WHEN** un départ est mis en surcharge sur le banc
- **THEN** une alerte « PTC déclenchée » identifiant le départ est publiée

### Requirement: Indépendance du bus
Une panne, un redémarrage ou une perte de Wi-Fi du module MUST NOT affecter le fonctionnement du bus.

#### Scenario: Redémarrage du module
- **WHEN** le module redémarre pendant un trafic I2C
- **THEN** le BP ne relève aucune erreur de communication imputable
