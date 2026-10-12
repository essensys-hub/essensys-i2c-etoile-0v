#!/bin/sh
# Dépôt public : aucun secret, certificat, adresse privée ni fichier de
# provisionnement réel sous firmware/. Usage : check_confidentiality.sh <firmware/>
set -u
FW=${1:-$(dirname "$0")/..}
SELF=check_confidentiality.sh
fail=0

g() {
    grep -rnIE --exclude="$SELF" --exclude-dir=build --exclude-dir='build-*' "$1" "$FW" 2>/dev/null
}

check() {
    out=$(g "$2")
    if [ -n "$out" ]; then
        echo "CONFIDENTIALITE ($1) :"
        echo "$out"
        fail=1
    fi
}

check "cle privee ou certificat" '-----BEGIN [A-Z ]*(PRIVATE KEY|CERTIFICATE)-----'
check "adresse IPv4 privee" '(^|[^0-9.])(192\.168|10\.[0-9]{1,3}|172\.(1[6-9]|2[0-9]|3[01]))\.[0-9]{1,3}(\.[0-9]{1,3})?([^0-9.]|$)'
check "adresse e-mail" '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+\.[A-Za-z.]{2,}'
check "mot de passe en clair" '(pass(word)?|psk|secret|token)[[:space:]]*[=:][[:space:]]*"[^"]{4,}"'

# Fichiers qui ne doivent jamais être versionnés.
files=$(find "$FW" \( -path '*/build' -o -path '*/build-*' \) -prune -o -type f \
    \( -name '*.pem' -o -name '*.key' -o -name '*.der' -o -name '*.p12' -o -name 'sdkconfig' \
       -o \( -path '*/provisioning/*' ! -name '*.example' ! -name '.gitkeep' \) \
       -o -path '*/secrets/*' \) -print)
if [ -n "$files" ]; then
    echo "CONFIDENTIALITE (fichier interdit) :"
    echo "$files"
    fail=1
fi

# L'exemple de provisionnement ne contient que des valeurs fictives.
ex="$FW/provisioning/n2_prov.csv.example"
if [ -f "$ex" ]; then
    out=$(grep -nE '^(wifi_ssid|wifi_pass|mqtt_user|mqtt_pass),' "$ex" | grep -v 'A_RENSEIGNER')
    [ -n "$out" ] && { echo "CONFIDENTIALITE (exemple non fictif) :"; echo "$out"; fail=1; }
    out=$(grep -nE '^cmd_key,' "$ex" | grep -vE ',0{64}$')
    [ -n "$out" ] && { echo "CONFIDENTIALITE (cle d'exemple non nulle) :"; echo "$out"; fail=1; }
fi

[ "$fail" -ne 0 ] && exit 1
echo "check_confidentiality : rien a signaler"
