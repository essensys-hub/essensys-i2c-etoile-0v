#!/usr/bin/env python3
"""Remplit les zones d'un .kicad_pcb (KiCad 9). Usage : <python KiCad> fill_zones.py <fichier.kicad_pcb>"""
import sys, wx
_app = wx.App(False)
import pcbnew
b = pcbnew.LoadBoard(sys.argv[1])
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(sys.argv[1], b)
print('zones remplies')
