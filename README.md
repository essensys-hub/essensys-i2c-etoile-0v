# essensys-i2c-etoile-0v – carte N2 « étoile 0 V » sur rail DIN

Carte passive qui relie le boîtier principal (BP, carte SC944D) aux boîtiers auxiliaires (BA1 SC940D, BA2 SC941C,
BA3 SC942C) d'une installation Essensys **en étoile** autour d'un plan 0 V commun, à la place du chaînage d'origine.

![Vue de dessus](export/n2-etoile-0v-pcb-top.png)

## Contexte : foudre 2026

Un coup de foudre indirect (août 2026) a détruit des étages I2C entre le BP et les BA : la surtension induite fait
circuler un courant d'équilibrage dans le 0 V du chaînage BP → BA1 → BA2 → BA3, et les écrêteurs des lignes SDA/SCL
conduisent dès que la chute de tension sur ce 0 V devient trop forte. Analyse et retour d'expérience :
[synthèse RETEX foudre 2026](https://www.essensys.fr/docs/retex/foudre-2026/synthese/).

N2 est la mesure « court terme » côté câblage : raccourcir et doubler les chemins 0 V, isoler chaque BA (cavalier, PTC, TVS).

- Change OpenSpec : [`i2c-surge-protection-2026-10-007`](https://github.com/essensys-hub/essensys-board-SC944D/tree/feat/i2c-surge-protection-2026-10-007/openspec/changes/i2c-surge-protection-2026-10-007)
  dans essensys-board-SC944D, tâche 4.1 (sections D3/D4 de `design.md`, spec `aux-box-interconnect`).
- Ticket : [essensys-hub/essensys-board-SC944D#1](https://github.com/essensys-hub/essensys-board-SC944D/issues/1).

## Fonction

N2 remplace le chaînage BP → BA1 → BA2 → BA3 par une **étoile** : le bus du BP (J31) arrive sur la carte
et repart vers chaque BA par un port dédié. Le 0 V de chaque BA rejoint celui du BP par un **plan 0 V double face**,
avec deux faisceaux AWG20 par BA (J1 et J2 du BA). Le 0 V du BP est doublé par une queue de cochon AWG20
soudée sur la broche traversante J31-3. But : relever le seuil de courant d'équilibrage à partir duquel
les écrêteurs I2C conduisent (au moins 4 fois la référence, à mesurer en 4.3), et garder l'écart statique de 0 V
sous 0,05 V.

Par port BA :

- **cavalier JPn** (embase 2×3 au pas de 2,54 mm, 3 shunts) qui coupe SDA, SCL et +15 V ; le 0 V n'est jamais coupé.
  Un pont 2×3 (boîtier femelle 2×3 avec 3 boucles) permet d'ouvrir les trois lignes en une seule manœuvre ;
- **PTC Fn** (CMS 1812, 30 V au moins) sur le +15 V, **puis TVS Dn SMAJ16A** entre le +15 V du port et le 0 V.
  Une TVS en court-circuit ne fait déclencher que la PTC de son port : les autres BA restent alimentés.

Aucun composant actif, aucun firmware, aucune retouche des BA.

## Brochage (WR-MPC3 2×2, compatible Würth 66200421122)

| Broche | Signal |
|---|---|
| 1 | SDA |
| 2 | SCL |
| 3 | 0 V |
| 4 | +15 V |

| Repère | Rôle |
|---|---|
| J1 | Entrée BP, vers J31 du BP |
| J8 | Pastille (Ø 1,25 mm) pour la queue de cochon AWG20 vers J31-3 du BP |
| J2 / J3 | BA1 SC940D, vers J1 / J2 du BA |
| J4 / J5 | BA2 SC941C, vers J1 / J2 du BA |
| J6 / J7 | BA3 SC942C, vers J1 / J2 du BA |
| JP1 / JP2 / JP3 | Cavalier du port BA1 / BA2 / BA3 (impairs côté BP, pairs côté port) |
| F1–F3, D1–D3 | PTC puis TVS du +15 V de chaque port |
| H1–H4 | Trous M3 non métallisés, **non reliés** (fixation sur support DIN isolant) |

Nets : `SDA_BP`, `SCL_BP`, `+15V_BP`, `0V` ; par port `SDA_BAn`, `SCL_BAn`, `+15V_SWn` (après cavalier), `+15V_BAn` (après PTC).

## Carte

- 110 × 68 mm, FR-4 1,6 mm, 2 couches, **cuivre 2 oz (70 µm) demandé**, HASL, vernis vert, sérigraphie blanche.
- Plan 0 V sur les deux faces (raccord plein, sans thermique), vias de couture, liaison J31-3 → pastille J8 en piste de 3 mm.
- Pistes : +15 V 1,5 à 2 mm, SDA/SCL 0,5 mm ; tronc en peigne depuis J1, une descente par port.
- Support DIN : à monter sur un porte-carte DIN **isolant** (profilé plastique pour carte de 68 mm, ou entretoises nylon M3 sur adaptateur DIN). Les trous de fixation ne sont reliés à rien.

## Fichiers

| Fichier | Contenu |
|---|---|
| `n2-etoile-0v.kicad_pro/.kicad_sch/.kicad_pcb` | Projet KiCad 9 |
| `export/n2-etoile-0v-schema.pdf` | Schéma |
| `export/n2-etoile-0v-bom.csv` | BOM |
| `export/n2-etoile-0v-gerber.zip` | Gerber, perçages Excellon, plan de perçage, job Gerber |
| `export/n2-etoile-0v-pcb-{top,bottom}.{png,svg}` | Rendus 3D et tracés cuivre |
| `rapports/erc.rpt`, `rapports/drc.rpt` | ERC et DRC (avec parité schéma) : 0 erreur, 0 avertissement |
| `scripts/` | Générateurs Python (schéma, PCB, remplissage des zones) et `build.sh` |

## Régénérer

Prérequis : KiCad 9 installé dans `/Applications/KiCad` (macOS ; adapter `KC` et `KP` dans `scripts/build.sh` sinon).

```sh
sh scripts/build.sh
```

Le script régénère schéma et PCB, relance ERC et DRC (avec parité schéma), refait tous les exports
et échoue si ERC ou DRC signalent une erreur.

## BOM

| Repères | Qté | Composant | Note |
|---|---|---|---|
| J1–J7 | 7 | Embase WR-MPC3 2×2 verticale THT, Würth 66200421122 (empreinte Molex Micro-Fit 3.0 43045-0412) | |
| F1–F3 | 3 | PTC CMS 1812, 30 V au moins, ex. Littelfuse 1812L050/30MR | Courant de maintien à fixer (voir limites) |
| D1–D3 | 3 | TVS SMAJ16A (SMA) | |
| JP1–JP3 | 3 | Embase mâle 2×3, pas de 2,54 mm | + 9 shunts, ou 3 ponts 2×3 |
| J8 | – | Pastille, pas de composant | Queue de cochon AWG20 |
| – | 1 | Porte-carte DIN isolant + 4 vis/entretoises M3 nylon | Hors BOM KiCad |

Faisceaux (tâche 4.2) : 7 cordons AWG20 2×2 WR-MPC3 femelle des deux côtés, 15 cm au plus, sertis à l'outil.

## Consignes de pose

- **Le 0 V n'est jamais relié au PE** : pas de borne PE sur la carte ni dans le coffret domotique, support DIN isolant,
  aucune vis de fixation reliée au 0 V.
- Faisceaux AWG20 de **15 cm au plus**, isolés, ni vert-jaune ni bleu clair, repérés aux deux extrémités des liaisons 0 V :
  **« 0 V TBTS – NE PAS RELIER À LA TERRE »**.
- La queue de cochon vers J31-3 est soudée sur la broche traversante pendant la dépose du BP, jamais sur une pastille CMS,
  avec un arrêt de traction ; elle est repérée comme les autres liaisons 0 V.
- Câbler **J1 et J2 de chaque BA** (les deux faisceaux comptent dans le seuil). Ne plus chaîner les BA entre eux.
- Déconnecter un BA : ouvrir JPn (SDA, SCL et +15 V coupés, 0 V conservé), sans toucher au câblage 230 V.
- Ne jamais piloter une armoire réelle pendant les essais.

## Consignes TBTS

Le bus BP/BA est en TBTS (+15 V) : voir les consignes de pose ci-dessus (0 V isolé du PE, repérage, support isolant).
Aucune intervention dans le coffret sans coupure de l'alimentation 230 V par une personne habilitée.

## Vérifications restantes (banc, tâches 4.2 à 4.4)

- **4.2** faisceaux : moins de 60 mΩ par conducteur, contacts compris ; repérage contrôlé.
- **4.3** seuil d'équilibrage en onde 8/20 et sur un front de 1 Ipk/µs : au moins 4 fois la référence de la chaîne d'origine ;
  écart statique de 0 V entre J31-3 du BP et J1-3 de chaque BA d'au plus 0,05 V à consommation maximale.
- **4.4** sélectivité des PTC : sortie en court-circuit 60 s, autres sorties à 14 V ou plus en 5 s sans déclencher le BP ;
  autres BA de nouveau acquittés moins de 10 s après ouverture du cavalier ; remontée en 120 s au plus ;
  aucun déclenchement en 24 h à 50 °C à consommation maximale.

## Limites

- **Empreintes à vérifier** sur les fiches techniques avant fabrication : la WR-MPC3 66200421122 est placée avec l'empreinte
  Molex Micro-Fit 3.0 43045-0412 (pas de 3 mm, deux plots de centrage Ø 1,02 mm) ; numérotation des broches à contrôler
  face au brochage du BA (1 SDA, 2 SCL, 3 0 V, 4 +15 V) et à l'orientation du détrompeur.
- **PTC** : la référence 1812L050/30MR est indicative. Le courant de maintien à 50 °C doit valoir au moins 1,5 fois le
  courant de BA_ALIM d'un BA relais collés (design D3) ; à mesurer, puis à valider en 4.4. Si aucune PTC 1812 ne convient,
  changer l'empreinte (radiale).
- Le cavalier à 3 shunts demande trois gestes ; pour la « manœuvre unique » de la spec, utiliser un pont 2×3.
- Cuivre 2 oz : à préciser à la commande (non porté par l'empilage KiCad, seulement par la sérigraphie de fabrication).
- Pas de TVS de données sur SDA/SCL (non demandée pour N2) ; modèles 3D non vérifiés.

## Licence

MIT, comme essensys-board-SC944D (voir `LICENSE`).
