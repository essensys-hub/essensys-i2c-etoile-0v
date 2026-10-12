## Purpose

Observer le trafic I2C BP↔BA de la carte N2 à des fins de diagnostic, sans jamais pouvoir perturber ni piloter le bus.

## ADDED Requirements

### Requirement: Absence matérielle d'émission
Le module MUST être raccordé à SDA et SCL par un chemin matériellement unidirectionnel (bus vers module) de sorte qu'aucune configuration logicielle, aucun accès Wi-Fi et aucune panne du module ne puisse tirer une ligne du bus.

#### Scenario: Firmware malveillant
- **WHEN** le firmware configure les broches d'écoute en sortie niveau bas
- **THEN** les niveaux SDA/SCL mesurés sur le bus restent inchangés

#### Scenario: Module non alimenté
- **WHEN** le module est hors tension ou retiré
- **THEN** les niveaux et temps de montée de NR-board-sc944d-1 restent dans la tolérance de référence

### Requirement: Charge haute impédance
L'écoute MUST ajouter au plus 10 pF et un courant de fuite inférieur à 1 µA par ligne, alimenté ou non.

#### Scenario: Temps de montée
- **WHEN** le temps de montée SDA/SCL est mesuré avec et sans module à 50 kHz
- **THEN** l'écart est inférieur à 5 %

### Requirement: Journal des trames
Le module SHALL journaliser chaque transaction avec horodatage, adresse, sens, octets, ACK/NACK et erreurs de checksum, et SHALL le rendre consultable en Wi-Fi/MQTT.

#### Scenario: NACK d'un BA
- **WHEN** un BA ne répond pas à son adresse
- **THEN** une entrée NACK horodatée portant l'adresse est journalisée et publiée

### Requirement: Mode debug et coupure
Le module SHALL proposer un mode debug détaillé désactivé par défaut, et un cavalier MUST permettre de déconnecter physiquement l'écoute.

#### Scenario: Cavalier retiré
- **WHEN** le cavalier d'écoute est retiré
- **THEN** aucune liaison électrique ne subsiste entre le module et SDA/SCL
