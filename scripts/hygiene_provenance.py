"""Un commentaire dit ce que fait le code, pas d'où il vient.

Une adresse, une étiquette générée ou une place dans un objet ne disent rien à
qui lit le code : un commentaire nomme ce que la valeur PORTE et décrit le
comportement. Cette règle attrape ce qui se reconnaît à coup sûr, dans la partie
commentaire d'une ligne ; le reste est affaire de relecture.
"""

from __future__ import annotations

import re
from pathlib import Path

# Là où vit le code du client et de ses tests.
CHECKED_ROOTS = ("libs/", "app/", "tests/")

# Une adresse sur huit chiffres à zéros de tête, ou une étiquette générée
# (`sub_` suivi de chiffres hexadécimaux).
ADDRESS_OR_LABEL = re.compile(
    r"0x00[0-9A-Fa-f]{6}\b"
    r"|\b(?:sub|loc|off|unk|byte|word|dword|qword|stru|flt|dbl|nullsub)_[0-9A-Fa-f]{5,}\b")

# UNE ADRESSE SANS SES ZÉROS DE TÊTE. ⚠ UNE COULEUR S'ÉCRIT AUSSI EN SIX
# CHIFFRES : une VALEUR citée se met entre accents graves, et passe. Huit
# chiffres sans zéros de tête sont des valeurs — un mot de drapeaux, une couleur
# avec son alpha — et passent aussi.
BARE_ADDRESS = re.compile(r"(?<![`\w])0x[0-9A-Fa-f]{6}\b(?!`)")

# UNE PLACE DANS UN OBJET : `+0xF0`, `instance+372`, `this + 1107`. Un
# commentaire nomme le champ par ce qu'il porte. `base + 90`, un calcul, passe.
OBJECT_OFFSET = re.compile(
    r"\+\s*0x[0-9A-Fa-f]+"
    r"|\b(?:this|instance|nœud|noeud|node)\s*\+\s*\d+")


def foreign_citations(comment: str) -> list[str]:
    """Ce qu'un commentaire cite d'autre que le comportement du code."""
    found = [match.group(0) for match in ADDRESS_OR_LABEL.finditer(comment)]
    found += [match.group(0) for match in BARE_ADDRESS.finditer(comment)]
    found += [match.group(0) for match in OBJECT_OFFSET.finditer(comment)]
    return found


def rule_no_native_provenance(path: Path, text: str) -> list[str]:
    """Aucune adresse, aucune étiquette générée ni place dans un objet en commentaire."""
    posix = path.as_posix()
    if not posix.startswith(CHECKED_ROOTS):
        return []
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        comment = line.find("//")
        if comment < 0:
            continue
        found = foreign_citations(line[comment:])
        if found:
            hint = (" (une VALEUR de six chiffres, une couleur, s'écrit entre accents graves)"
                    if BARE_ADDRESS.fullmatch(found[0]) else "")
            problems.append(
                f"{path}:{number}: le commentaire cite une adresse (« {found[0]} »){hint} "
                f"— il décrit le comportement"
            )
    return problems


def self_test_native_provenance() -> list[str]:
    """La règle mord sur chaque forme qu'elle vise, et se tait sur ce qu'elle permet."""
    failures = []
    sample = Path("libs/render/src/fictif.cpp")
    # Les échantillons se composent en deux morceaux : écrits d'un seul tenant,
    # ce fichier contiendrait ce qu'il interdit.
    must_reject = [
        ("int x = 0;  // voir " + "0x00" + "C5DFC0\n", "une adresse"),
        ("// voir dword" + "_C5DFC0\n", "une étiquette générée"),
        ("// un OCTET de l'acteur, `+0xF0`\n", "un décalage hexadécimal"),
        ("// le rang va dans `instance+372`\n", "un décalage décimal"),
        ("// elle seule lit `this + 1107`\n", "un décalage espacé"),
        ("// comme le dessin d'origine (0x" + "A5A590)\n", "une adresse sans ses zéros de tête"),
    ]
    for text, label in must_reject:
        if not rule_no_native_provenance(sample, text):
            failures.append(f"self-test : {label} passe encore dans un commentaire")
    must_accept = [
        ("PutU32(archive, 34, 0x" + "00A50000U);\n", "une valeur dans le CODE"),
        ("// la couleur du client, `0x32FF32`\n", "une couleur entre accents graves"),
        ("// un bit du mot, 0x80000000\n", "une valeur de huit chiffres"),
        ("// le tableau `{base, base + 90}` que la boucle indexe\n", "un calcul"),
    ]
    for text, label in must_accept:
        found = rule_no_native_provenance(sample, text)
        if found:
            failures.append(f"self-test : faux positif sur {label} — {found[0]}")
    # Hors du code du client, la règle se tait.
    if rule_no_native_provenance(Path("scripts/gen_x.cpp"), "// voir dword" + "_C5DFC0\n"):
        failures.append("self-test : la règle de provenance mord hors du code du client")
    return failures
