## Purpose

Fournir une alarme intrusion locale autonome sur la carte N2, fonctionnant même pendant une coupure secteur.

## ADDED Requirements

### Requirement: Entrées capteurs TBT
Le module SHALL accepter au moins 4 entrées capteurs TBT (contact sec ouverture, détecteur de mouvement) protégées et supervisées (boucle ouverte/fermée/coupée).

#### Scenario: Ouverture en mode armé
- **WHEN** une entrée passe en état ouvert alors que l'alarme est armée
- **THEN** une alarme horodatée identifiant l'entrée est publiée

#### Scenario: Câble coupé
- **WHEN** la boucle d'une entrée est coupée
- **THEN** un défaut d'autoprotection est signalé, armé ou non

### Requirement: Sortie sirène
Le module SHALL commander une sortie sirène TBT protégée contre les courts-circuits, avec durée maximale configurable (défaut 3 min).

#### Scenario: Déclenchement
- **WHEN** une alarme survient en mode armé
- **THEN** la sirène est activée puis coupée au plus tard à la durée maximale

### Requirement: Armement et désarmement
L'armement et le désarmement SHALL être possibles localement et à distance par MQTT authentifié, avec temporisations d'entrée/sortie.

#### Scenario: Désarmement non authentifié
- **WHEN** une commande de désarmement arrive sans authentification valide
- **THEN** elle est rejetée et journalisée

### Requirement: Fonctionnement sur batterie
L'alarme MUST rester opérationnelle sur batterie de secours après la coupure du +15 V par le BP.

#### Scenario: Coupure secteur
- **WHEN** le +15 V disparaît
- **THEN** la détection et la sirène restent fonctionnelles pendant l'autonomie spécifiée
