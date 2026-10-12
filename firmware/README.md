# Firmware du module ESP32-C3 de la carte N2

Change OpenSpec `n2-esp32-spy-alarm-2026-10-008`, ticket essensys-hub/essensys-feature-lifecycle#28.

Le module observe le bus I2C BP ↔ BA **sans jamais pouvoir y agir**, surveille sa santé et le +15 V, gère une
alarme intrusion sur batterie et conserve la preuve des surtensions. Il publie en MQTT sur TLS. Le protocole
HTTP legacy IoT entre le BP et le serveur n'est pas modifié. L'alarme est une aide à la détection, elle n'est pas
certifiée EN 50131.

## Organisation

| Chemin | Rôle |
|---|---|
| `components/n2_core/` | Logique pure, sans dépendance matérielle, compilée par ESP-IDF et sur hôte |
| `components/n2_core/include/n2_pins.h` | **Seul** fichier de brochage (TODO de recoupement avec le schéma) |
| `components/n2_core/include/n2_board.h` | Voies des ADC MCP3428, adresses du bus local, échelles des chaînes de mesure |
| `main/` | Application ESP-IDF : HAL, écoute, réseau, OTA, console, stockage |
| `test/` | Tests hôtes (`ctest`), traces SDA/SCL (`test/traces/`), contrôles de revue |
| `provisioning/n2_prov.csv.example` | Modèle de provisionnement, valeurs fictives |
| `tools/n2_cmd.py` | Signature d'une commande MQTT (banc, exploitation) |

| Module (`n2_core`) | Tâche | Rôle |
|---|---|---|
| `i2c_decoder` | 3.2 | Machine d'états START, adresse, R/W, octets, ACK/NACK, REPEATED START, STOP |
| `essensys_proto` | 3.2 | Trames BP ↔ BA : codes 1 à 5, longueurs, CRC-16/MODBUS, écho du code et du CRC |
| `frame_log`, `edge_ring` | 3.3 | Journal horodaté en anneau, compteurs, trames et fronts perdus |
| `debug_mode` | 3.4 | Debug désactivé au démarrage, activable localement ou par MQTT authentifié, durée bornée |
| `bus_health` | 4.1, 4.2 | BA muet (10 s par défaut), ligne SDA/SCL collée plus de 100 ms |
| `power_monitor` | 4.3 | Chute du +15 V, PTC déclenchée par départ, cavalier ouvert, batterie |
| `alarm`, `siren` | 5.1 à 5.3 | Boucles équilibrées, autoprotection, temporisations, sirène 3 min max |
| `surge` | 6.1 | Latch surtension (lecture, valeur crête, réarmement), écart de 0 V |
| `event_log`, `timebase` | 6.2 | Journal non volatil avec CRC, horodatage RTC puis NTP |
| `mqtt_cmd`, `cmd_handler`, `n2_topics` | 7.1, 7.2 | Commandes HMAC-SHA256 avec anti-rejeu, liste blanche, topics `essensys/n2/<id>/…` |
| `ota_policy` | 7.3 | URL https sous préfixe, forme du bloc de signature Secure Boot V2 |
| `pin_guard` | 3.2 | Garde : broches d'écoute jamais en sortie |

## Garantie « aucune émission sur le bus »

1. **Matériel** (design D2) : isolateur numérique unidirectionnel bus → module, cavalier de coupure.
2. **Firmware** :
   - aucune API d'écriture ;
   - les broches d'écoute sont configurées en entrée, sans tirage ;
   - aucun périphérique I2C esclave ni émission RMT ;
   - garde au démarrage, puis toutes les 10 s (`pin_guard`) ;
   - `test/check_no_bus_write.sh`, exécuté par `ctest`, échoue si une de ces règles est enfreinte ;
   - le chemin des commandes MQTT n'atteint aucun GPIO.
3. **Banc** : NR-i2c-etoile-0v-1 (tâche 8.1), avec un firmware qui force les broches.

Le bus I2C **local** est un bus distinct, sur ses propres broches (GPIO4, GPIO5). Il dessert la RTC RV-3028, l'extension
d'E/S TCA9534 (latches, clé, chargeur, LED alarme) et les quatre ADC MCP3428.

## Matériel visé

Révision B de la carte, d'après `scripts/n2_module.py` (2026-10-12). Le brochage est dans `n2_pins.h`.

| Fonction | Matériel | Lecture par le firmware |
|---|---|---|
| Écoute | JP4 → 1 kΩ → ISO7720 → GPIO0 (SCL), GPIO1 (SDA) | Interruption sur fronts, entrées seules |
| +15 V, PTC, batterie | Ponts ÷16 (÷2 pour VBAT) → MCP3428 U10/U11 | 12 bits, 1 mV/pas, toutes les 50 ms |
| Écart de 0 V | Amplis différentiels G = 0,249 → MCP3428 U13 | Écart signé, ±8 V |
| Latch surtension | LM339, fenêtre ±1 V sur l'écart de 0 V, et +15 V > 20 V → 74HC74 | Bits P0/P1 du TCA9534, réarmement par impulsion basse sur GPIO7 |
| Boucles Z1 à Z4 | Tirage 10 kΩ, 1 kΩ série, fin de ligne 2 x 4,7 kΩ → MCP3428 U12 | Pour-mille de 3,3 V |
| Sirène 12 V | MT3608 + BTS4140N sur GPIO6, retour ÷11 → U13 | Défaut si la sortie reste sous 6 V après 300 ms |
| Clé d'armement | J11 → P4 du TCA9534 (actif bas) | Anti-rebond de 150 ms |

## Tests hôtes

```sh
cmake -S firmware/test -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Les tests sont compilés avec `-Wall -Wextra -Werror` et AddressSanitizer/UBSan. Les traces de `test/traces/`
sont produites par `test/tools/gen_traces.py` (format CSV `t_us,scl,sda`, compatible avec l'export d'un
analyseur logique). Le test `traces_up_to_date` vérifie qu'elles correspondent au générateur. Pour rejouer
une capture réelle (tâche 3.3), exporter l'analyseur logique au même format.

## Build ESP-IDF

ESP-IDF **v5.3.2**, cible `esp32c3`. Le workflow `.github/workflows/firmware.yml` construit les deux profils.

```sh
# dev : Secure Boot et chiffrement flash désactivés, OTA signée vérifiée
idf.py set-target esp32c3 && idf.py build

# release
idf.py -B build-release -D SDKCONFIG=build-release/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.release" build
```

Les paramètres (délai BA muet, ligne collée, seuils, temporisations, sirène) sont dans `menuconfig`, menu
« Essensys carte N2 ». Certains peuvent être surchargés par appareil dans le provisionnement.

## Profils de sécurité

| | dev (`sdkconfig.defaults`) | release (`+ sdkconfig.defaults.release`) |
|---|---|---|
| Secure Boot V2 (RSA-3072) | non | oui |
| Chiffrement flash | non | oui, mode Release |
| NVS chiffrée (identifiants) | non | oui (`nvs_keys`) |
| Signature des images OTA vérifiée | oui | oui |
| Anti-retour arrière | retour arrière automatique | et version de sécurité |
| Téléchargement ROM | ouvert | sécurisé |

**Signature.** Les binaires ne sont jamais signés en CI : aucune clé n'est présente dans le dépôt ni dans les
secrets GitHub. Le build produit des images non signées, qu'on signe hors ligne :

```sh
espsecure.py sign_data --version 2 --keyfile <clé RSA-3072 hors dépôt> \
    --output n2-signed.bin build-release/n2_esp32_spy_alarm.bin
```

En dev, l'OTA n'accepte qu'une image signée par la même clé que l'image en cours. Pour tester l'OTA sur banc,
flasher d'abord une image signée par la clé de banc.

**Passage en release (irréversible).** Au premier démarrage, une image release grave des eFuses : empreinte de
la clé Secure Boot, clé de chiffrement flash, désactivation de JTAG et du téléchargement ROM non sécurisé.
Procédure :

1. provisionner la NVS ;
2. flasher le chargeur et l'application signés ;
3. laisser le premier démarrage chiffrer la flash ;
4. vérifier `espefuse.py summary`.

Ne jamais le faire sur une carte de banc à reprogrammer.

## Provisionnement (hors dépôt)

Les identifiants sont propres à chaque appareil et ne sont **jamais** versionnés. `provisioning/*` est ignoré par
git, sauf les fichiers `*.example`, et `test/check_confidentiality.sh` échoue si un secret apparaît sous
`firmware/`.

1. Copier `provisioning/n2_prov.csv.example` vers `provisioning/n2_prov.csv` et le renseigner : `dev_id` en
   `[a-z0-9_-]`, URI `mqtts://`, certificats CA dans `secrets/`, `cmd_key` de 32 octets aléatoires.
2. Générer l'image, chiffrée en release :
   `python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate provisioning/n2_prov.csv nvs.bin 0x6000`.
   En release, utiliser `encrypt` avec les clés de `nvs_keys`.
3. Flasher l'image à l'adresse de la partition `nvs`.

Un module sans provisionnement fonctionne en autonome : écoute, journal, alarme et sirène, sans réseau.

## MQTT

Racine des topics : `essensys/n2/<dev_id>/`.

| Topic | Contenu |
|---|---|
| `status` (retenu) | `online` (ou `offline` par LWT), version, état de l'alarme, mode debug, source de l'heure, BA muets |
| `events` (QoS 1) | journal d'événements ; les événements survenus hors réseau sont publiés au retour |
| `frames` | trames en erreur (NACK, CRC…) ; toutes, avec les octets, en mode debug |
| `counters` | compteurs de trames, erreurs, trames et fronts perdus, toutes les 60 s |
| `alarm/state` (retenu) | état de l'alarme |
| `events/dump`, `frames/dump` | relecture des journaux à la demande |
| `cmd` | **seule souscription** : commandes signées |
| `cmd/result` | résultat de chaque commande, rejets compris |

Commande : `v=1;seq=<n>;cmd=<arm|disarm|debug|status|events|frames|ota>[;arg=…][;ttl=…];mac=<hex>`.

- `mac` vaut HMAC-SHA256(`cmd_key`, `"<dev_id>|"` + le texte qui précède `;mac=`).
- `seq` doit être strictement croissant ; il est conservé en NVS.
- Toute commande non authentifiée, rejouée ou inconnue est rejetée, journalisée (`cmd_rejected`) et publiée sur
  `cmd/result`.
- **Aucune commande n'agit sur le bus.**

Pour signer une commande : `N2_CMD_KEY=<hex> tools/n2_cmd.py --device <id> --seq <n> disarm`.

## Actions locales

La console USB-Serial-JTAG propose les commandes `status`, `debug on|off`, `arm`, `disarm`, `events`, `frames` et
`events-clear CONFIRMER`. Ce dernier est le **seul** moyen d'effacer les preuves, et il n'est pas accessible par
MQTT.

| Commande locale | Moyen |
|---|---|
| Armement et désarmement | Contact à clé (J11, bit P4 du TCA9534) |
| Mode debug local | Appui de 3 s sur le bouton |

## Reste à valider sur banc

Ces points n'ont pas été vérifiés sur le matériel :

| Tâches | Vérification |
|---|---|
| 3.3 | Concordance du journal avec un analyseur logique, et trames perdues sous charge Wi-Fi (interruption GPIO de niveau 3, file de 2048 fronts) |
| 4.1, 4.2, 4.3 | Retrait d'un BA, ligne forcée basse, surcharge d'un départ |
| 5.1, 5.2 | Seuils des boucles réelles et tenue de la sirène en court-circuit |
| 5.4 | Autonomie |
| 6.1 | Seuil du latch par impulsion injectée |
| 6.2 | Relecture après coupure totale |
| 7.1 | Broker de banc |
| 7.3 | Rejet d'une image non signée |
| 8.x | Tests NR |

À faire aussi :

- recouper le brochage (`n2_pins.h`), les adresses des MCP3428 et les échelles de mesure (`n2_board.c`) avec le
  schéma final (tâches 1.1 et 1.2) ;
- valider la configuration de basculement batterie de la RTC RV-3028.

## Limites connues

- **Pas de détecteur de crête en révision B.** La valeur d'un événement `surge` est l'écart de 0 V, ou le +15 V,
  mesuré lors du traitement du latch, au plus 50 ms après l'impulsion. Le latch, lui, prouve le dépassement.
  L'horodatage matériel EVI du RV-3028, relié à `LATCH_0V`, n'est pas encore exploité.
- **Pas d'entrée d'autoprotection du coffret** en révision B : câbler le contact sur une boucle configurée en
  zone 24 h (`zone_types` dans le provisionnement).
- **Écriture en NVS pendant le décodage.** L'écriture d'un événement en NVS se fait sous le verrou partagé : elle
  peut retarder le décodage. Les fronts sont mis en file (2048) ; s'ils débordent, la perte est comptée
  (`lost`, `edges_dropped`).
