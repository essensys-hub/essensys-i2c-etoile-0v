#!/bin/sh
# Régénère schéma, PCB, contrôles et exports. Usage : sh scripts/build.sh (depuis n'importe où)
set -e
cd "$(dirname "$0")/.."
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
N=n2-etoile-0v
python3 scripts/gen_sch.py .
$KP scripts/gen_pcb.py .
$KP scripts/fill_zones.py $N.kicad_pcb
mkdir -p rapports export/gerber
$KC sch erc --severity-all -o rapports/erc.rpt $N.kicad_sch
$KC pcb drc --severity-all --schematic-parity -o rapports/drc.rpt $N.kicad_pcb
$KC sch export pdf -o export/$N-schema.pdf $N.kicad_sch
$KC sch export bom --fields 'Reference,Value,Footprint,Manufacturer,MPN,${QUANTITY}' --labels 'Repère,Valeur,Empreinte,Fabricant,Référence,Qté' --group-by 'Value,Footprint,MPN' --exclude-dnp -o export/$N-bom.csv $N.kicad_sch
rm -rf export/gerber/*
$KC pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,F.Paste,B.Paste,Edge.Cuts -o export/gerber/ $N.kicad_pcb
$KC pcb export drill --generate-map --map-format pdf -o export/gerber/ $N.kicad_pcb
(cd export/gerber && rm -f ../$N-gerber.zip && zip -q ../$N-gerber.zip *)
$KC pcb export svg --layers F.Cu,F.Silkscreen,Edge.Cuts --mode-single --page-size-mode 2 --exclude-drawing-sheet -o export/$N-pcb-top.svg $N.kicad_pcb
$KC pcb export svg --layers B.Cu,B.Silkscreen,Edge.Cuts --mode-single --page-size-mode 2 --exclude-drawing-sheet --mirror -o export/$N-pcb-bottom.svg $N.kicad_pcb
$KC pcb render --side top --width 1600 --height 1000 --quality high -o export/$N-pcb-top.png $N.kicad_pcb
$KC pcb render --side bottom --width 1600 --height 1000 --quality high -o export/$N-pcb-bottom.png $N.kicad_pcb
grep -q "Errors 0  Warnings 0" rapports/erc.rpt || { echo "ERC en erreur" >&2; exit 1; }
grep -q "Found 0 DRC violations" rapports/drc.rpt && grep -q "Found 0 unconnected pads" rapports/drc.rpt || { echo "DRC en erreur" >&2; exit 1; }
echo "ERC/DRC : 0 erreur"
