## 1. Conception matérielle (#1)
- [x] 1.1 Ajouter au schéma KiCad le module ESP32-C3-MINI-1, l'isolateur unidirectionnel SDA/SCL, le cavalier de coupure et la RTC ; vérifier ERC sans erreur
- [x] 1.2 Ajouter les chaînes de mesure +15 V amont/aval PTC, écart 0 V et latch surtension ; vérifier ERC et calcul de tolérances dans le README
- [x] 1.3 Router le PCB révisé (séparation zone bus / zone module, aucune liaison 0 V–PE) ; vérifier DRC sans erreur et exports Gerber/BOM/PDF régénérés dans export/

## 2. Alimentation et batterie (#2)
- [ ] 2.1 Concevoir PTC propre, TVS, abaisseur 15 V→5 V et régulateur 3,3 V ; vérifier sur banc la tension et le court-circuit sans chute du +15 V des BA
- [ ] 2.2 Intégrer chargeur LiFePO4 avec CTN et power-path ; vérifier sur banc la charge CC/CV 3,6 V et le basculement sans redémarrage
- [ ] 2.3 Mesurer le budget de consommation pire cas ; vérifier qu'il reste sous le budget documenté avec PTC ≥ 1,5 × BA_ALIM sans déclenchement

## 3. Firmware espion et debug (#3)
- [x] 3.1 Créer le projet firmware ESP-IDF (firmware/) avec secure boot et chiffrement flash ; vérifier que le build CI passe
- [x] 3.2 Implémenter le décodeur I2C passif par GPIO/RMT (aucune broche en sortie vers l'isolateur) ; vérifier par test unitaire sur traces enregistrées
- [ ] 3.3 Implémenter le journal horodaté (adresse, sens, octets, ACK/NACK, checksum, trames perdues) ; vérifier sur banc la concordance avec un analyseur logique
- [x] 3.4 Implémenter le mode debug désactivé par défaut ; vérifier qu'il n'est activable que localement ou par MQTT authentifié

## 4. Surveillance du bus (#4)
- [ ] 4.1 Détecter BA muet (délai configurable) ; vérifier sur banc par retrait d'un BA (NR-board-sc944d-2)
- [ ] 4.2 Détecter ligne collée > 100 ms ; vérifier sur banc avec SDA puis SCL forcées basses (NR-board-sc944d-3)
- [ ] 4.3 Détecter chute du +15 V et PTC déclenchée ; vérifier sur banc par surcharge d'un départ

## 5. Alarme intrusion (#5)
- [ ] 5.1 Implémenter les entrées boucle équilibrée et l'autoprotection ; vérifier sur banc ouvert/fermé/coupé
- [ ] 5.2 Implémenter sortie sirène limitée et durée maximale ; vérifier sur banc l'arrêt à la durée configurée et la tenue au court-circuit
- [x] 5.3 Implémenter armement/désarmement local et MQTT authentifié avec temporisations ; vérifier le rejet d'une commande non authentifiée
- [ ] 5.4 Mesurer l'autonomie sur batterie alarme armée ; vérifier ≥ 24 h

## 6. Surtension (#6)
- [ ] 6.1 Implémenter la lecture/réarmement du latch et la mesure d'écart 0 V ; vérifier sur banc par impulsion injectée au-delà du seuil
- [ ] 6.2 Stocker les événements en mémoire non volatile avec horodatage RTC/NTP ; vérifier la relecture après coupure totale

## 7. Intégration MQTT et serveur (#7)
- [ ] 7.1 Implémenter MQTT TLS, provisionnement des identifiants hors dépôt et topics essensys/n2/<id>/… ; vérifier la publication sur un broker de banc
- [ ] 7.2 Vérifier qu'aucune commande MQTT n'agit sur le bus et que le protocole HTTP legacy IoT est inchangé (revue de code et test de banc)
- [ ] 7.3 Implémenter l'OTA signée ; vérifier le rejet d'une image non signée

## 8. Essais sur banc et NR (#8)
- [ ] 8.1 Créer NR-i2c-etoile-0v-1 : niveaux et temps de montée NR-board-sc944d-1 inchangés, module alimenté, non alimenté et firmware forçant les broches ; vérifier écart < 5 %
- [ ] 8.2 Créer NR-i2c-etoile-0v-2 : redémarrage du module sans erreur de bus ; vérifier sur banc
- [ ] 8.3 Contrôler l'isolement 0 V–PE de la carte assemblée ; vérifier l'absence de liaison et consigner le rapport dans rapports/
- [ ] 8.4 Lancer /checkup ; vérifier un rapport vert (jamais sur une armoire réelle)

## 9. Documentation (#9)
- [x] 9.1 Mettre à jour le README (architecture, câblage, budget, sécurité, sans donnée privée) ; vérifier le grep de confidentialité vide
- [ ] 9.2 Documenter le module dans essensys-doc (installation, alarme, MQTT) ; vérifier la PR liée au ticket #28
