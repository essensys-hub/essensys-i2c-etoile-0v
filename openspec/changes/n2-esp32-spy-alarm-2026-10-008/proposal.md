## Why

Après l'incident foudre (essensys-hub/essensys-board-SC944D#1, change parent `i2c-surge-protection-2026-10-007`), la carte N2 « étoile 0 V » protège le bus I2C BP↔BA mais n'offre aucune visibilité : on ne sait ni ce qui circule sur le bus, ni quand un BA se tait, ni quand une surtension survient. Le ticket essensys-hub/essensys-feature-lifecycle#28 demande d'ajouter à la carte N2 un module ESP32 qui observe le bus sans jamais pouvoir y agir, surveille sa santé, gère une alarme intrusion et conserve la preuve des surtensions, avec une batterie locale.

## What Changes

- Ajout sur la carte N2 d'un module MCU Wi-Fi (ESP32-C3 ou équivalent) avec cavalier de coupure.
- Espion I2C passif à haute impédance, matériellement incapable d'émettre sur SDA/SCL.
- Surveillance du bus : BA muet, ligne collée, chute du +15 V, déclenchement d'une PTC.
- Alarme intrusion : entrées capteurs TBT, sortie sirène, armement/désarmement.
- Détection et horodatage des surtensions (écart de 0 V, PTC), journal non volatil.
- Alimentation 15 V → 3,3 V protégée et batterie de secours locale avec chargeur.
- Publication Wi-Fi/MQTT authentifiée et chiffrée, mode debug ; protocole HTTP legacy IoT inchangé.
- Révision du schéma et du PCB de la carte N2, essais sur banc et tests NR associés.

## Capabilities

### New Capabilities
- `i2c-passive-sniffer`: écoute passive et journal horodaté des trames I2C BP↔BA sans aucune émission possible.
- `bus-health-monitoring`: détection BA muet, ligne collée, chute du +15 V et déclenchement de PTC.
- `intrusion-alarm`: entrées capteurs, sirène, armement/désarmement, fonctionnement sur batterie.
- `surge-event-logging`: détection, horodatage et conservation des événements de surtension.
- `n2-power-backup`: alimentation du module depuis le +15 V et batterie de secours locale.

### Modified Capabilities

## Impact

- Dépôt `essensys-i2c-etoile-0v` : schéma, PCB, BOM, exports, nouveau firmware du module, README.
- Dépôts liés : `essensys-board-SC944D` (change parent, NR-board-sc944d-1), `essensys-doc`, éventuel abonné MQTT côté serveur.
- Aucun changement des firmwares BP/BA ni du protocole HTTP legacy IoT.
- Ticket : essensys-hub/essensys-feature-lifecycle#28.
