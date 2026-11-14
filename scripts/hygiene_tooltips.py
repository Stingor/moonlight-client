"""Une infobulle est habillée.

L'infobulle d'ImGui a un fond SOMBRE ; celle du jeu — `BeginSkinnedTooltip` —
a le fond clair et le cadre des panneaux de description. Les confondre a deux
visages, que le joueur signale l'un après l'autre : une bulle sans l'habit du
jeu au milieu de l'interface, et un texte sombre, écrit pour le fond clair,
qui disparaît sur le fond sombre. Aucun compilateur ne le voit, et la relecture
non plus : il faut survoler la chose en jeu.

La règle refuse donc, dans `libs/ui` et `app`, les quatre portes d'ImGui vers
une infobulle brute — sauf dans les deux implémentations de l'infobulle
habillée, qui les emploient elles-mêmes : `BeginSkinnedTooltip`, qui a
l'habillage en main, et `BeginSkinTooltip` / `SkinTooltipFormat`, qui prennent
celui de la fenêtre en cours de dessin.

Les fichiers qui en ouvrent encore sont nommés dans `DISPENSED` : la liste ne
fait que RÉTRÉCIR. Un fichier absent ne peut pas en ouvrir, et un fichier
présent qui n'en ouvre plus est une erreur — sa dispense se retire avec lui.
"""

from __future__ import annotations

import re
from pathlib import Path

import hygiene_french

RAW_TOOLTIP = re.compile(r"\bImGui::(BeginTooltip|SetTooltip|BeginItemTooltip|SetItemTooltip)\s*\(")

WATCHED_PREFIXES = ("libs/ui/", "app/")

# L'infobulle habillée elle-même : elle ouvre celle d'ImGui, puis l'habille.
IMPLEMENTATIONS = frozenset({
    "libs/ui/src/skinned_window.cpp",
    "libs/ui/src/skin_tooltip.cpp",
})

# Les fichiers qui ouvrent encore une infobulle brute, et c'est voulu : l'aperçu
# de description, qui s'habille lui-même à sa largeur ; l'image de carte et le
# mannequin, qui flottent SANS fond au survol d'une description ; les états,
# qui ont leur habillage clair. La liste ne fait que rétrécir.
DISPENSED = frozenset({
    "libs/ui/src/description_panels.cpp",
    "libs/ui/src/description_window.cpp",
    "libs/ui/src/status_cell.cpp",
})


def raw_tooltips(text: str) -> list[int]:
    """Les lignes où le code ouvre une infobulle brute, commentaires et chaînes ôtés."""
    return [number for number, line in enumerate(hygiene_french.code_only(text).splitlines(), 1)
            if RAW_TOOLTIP.search(line)]


def rule_tooltips_are_skinned(path: Path, text: str,
                              dispensed: frozenset[str] = DISPENSED) -> list[str]:
    """Aucune infobulle brute hors de l'implémentation et des fichiers dispensés."""
    posix = path.as_posix()
    if (path.suffix not in hygiene_french.CXX_SUFFIXES
            or not posix.startswith(WATCHED_PREFIXES)
            or posix in IMPLEMENTATIONS or posix in dispensed):
        return []
    return [f"{path}:{number}: infobulle d'ImGui brute — `BeginSkinTooltip` ou "
            "`SkinTooltipFormat`, sans quoi le fond est sombre et le texte du jeu s'y perd"
            for number in raw_tooltips(text)]


def rule_tooltip_dispenses_still_needed(root: Path,
                                        dispensed: frozenset[str] = DISPENSED) -> list[str]:
    """Une dispense nomme un fichier qui existe et ouvre encore une infobulle brute."""
    problems = []
    for entry in sorted(dispensed):
        path = root / entry
        if not path.is_file():
            problems.append(f"hygiene_tooltips.py : `{entry}` n'existe plus — retirer sa dispense")
        elif not raw_tooltips(path.read_text(encoding="utf-8")):
            problems.append(f"hygiene_tooltips.py : `{entry}` n'ouvre plus d'infobulle brute — "
                            "retirer sa dispense")
    return problems


def self_test_skinned_tooltips() -> list[str]:
    """La règle mord sur une infobulle brute, et se tait sur l'habillée et le reste."""
    failures = []
    fake = Path("libs/ui/src/fictif.cpp")
    none: frozenset[str] = frozenset()
    for text, label in [
        ("if (ImGui::BeginTooltip()) { ImGui::EndTooltip(); }\n", "BeginTooltip"),
        ("ImGui::SetTooltip(\"%s\", texte);\n", "SetTooltip"),
        ("ImGui::SetItemTooltip(\"aide\");\n", "SetItemTooltip"),
        ("if (ImGui::BeginItemTooltip()) {}\n", "BeginItemTooltip"),
    ]:
        if not rule_tooltips_are_skinned(fake, text, none):
            failures.append(f"self-test : une infobulle brute ({label}) passe encore")
    for text, label in [
        ("if (BeginSkinnedTooltip(art)) { EndSkinnedTooltip(art); }\n", "l'infobulle habillée"),
        ("if (BeginSkinTooltip()) { EndSkinTooltip(); }\n", "la bulle sans habillage en main"),
        ("SkinTooltipFormat(\"%s\", texte);\n", "la bulle de texte"),
        ("// ImGui::SetTooltip(\"ancien\");\nint x = 0;\n", "un commentaire"),
        ("const char* kAide = \"ImGui::SetTooltip(\";\n", "une chaîne"),
    ]:
        found = rule_tooltips_are_skinned(fake, text, none)
        if found:
            failures.append(f"self-test : faux positif sur {label} — {found[0]}")
    for implementation in sorted(IMPLEMENTATIONS):
        if rule_tooltips_are_skinned(Path(implementation), "ImGui::BeginTooltip();\n", none):
            failures.append(f"self-test : l'implémentation {implementation} est refusée")
    if rule_tooltips_are_skinned(fake, "ImGui::SetTooltip(\"x\");\n",
                                 frozenset({fake.as_posix()})):
        failures.append("self-test : un fichier dispensé est encore contrôlé")
    if rule_tooltips_are_skinned(Path("libs/render/src/fictif.cpp"), "ImGui::SetTooltip(\"x\");\n",
                                 none):
        failures.append("self-test : la règle des infobulles mord hors de ui et app")
    return failures
