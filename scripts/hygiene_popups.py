"""Un menu surgissant prend le texte de l'habillage.

Un surgissant d'ImGui hérite des couleurs de l'endroit où il s'ouvre. La
conversation pose un texte BLANC pour son fond sombre : un menu ouvert sur un
lien du fil prenait ce blanc sur son propre fond clair, et ses entrées
disparaissaient. Le joueur l'a signalé menu après menu, parce que chaque
appelant devait penser à rétablir le texte lui-même — et le suivant oubliait.

La règle refuse donc, dans `libs/ui` et `app`, les portes d'ImGui vers un
surgissant brut, hors de `skin_popup.cpp` qui les habille : `BeginSkinPopup` et
`BeginSkinPopupContextItem` posent le texte de l'habillage DANS le surgissant,
et, quand l'endroit impose un fond sombre, le fond clair qui va avec : le texte
sombre seul ne se lit pas sur le fond sombre d'un HUD.
Les modales (`BeginPopupModal`) ne sont pas visées : elles s'habillent par
`BeginSkinnedModal`.
"""

from __future__ import annotations

import re
from pathlib import Path

import hygiene_french

RAW_POPUP = re.compile(
    r"\bImGui::(BeginPopup|BeginPopupContextItem|BeginPopupContextWindow|BeginPopupContextVoid)"
    r"\s*\(")

WATCHED_PREFIXES = ("libs/ui/", "app/")

# Le surgissant habillé lui-même : il ouvre celui d'ImGui, puis l'habille.
IMPLEMENTATIONS = frozenset({"libs/ui/src/skin_popup.cpp"})


def raw_popups(text: str) -> list[int]:
    """Les lignes où le code ouvre un surgissant brut, commentaires et chaînes ôtés."""
    return [number for number, line in enumerate(hygiene_french.code_only(text).splitlines(), 1)
            if RAW_POPUP.search(line)]


def rule_popups_are_skinned(path: Path, text: str) -> list[str]:
    """Aucun surgissant brut hors de l'implémentation."""
    posix = path.as_posix()
    if (path.suffix not in hygiene_french.CXX_SUFFIXES
            or not posix.startswith(WATCHED_PREFIXES) or posix in IMPLEMENTATIONS):
        return []
    return [f"{path}:{number}: surgissant d'ImGui brut — `BeginSkinPopup` / `EndSkinPopup`, "
            "sans quoi il hérite du texte de ce qui l'entoure (blanc sur la conversation)"
            for number in raw_popups(text)]


def self_test_skinned_popups() -> list[str]:
    """La règle mord sur un surgissant brut, et se tait sur l'habillé et le reste."""
    failures = []
    fake = Path("libs/ui/src/fictif.cpp")
    for text, label in [
        ("if (ImGui::BeginPopup(\"##menu\")) { ImGui::EndPopup(); }\n", "BeginPopup"),
        ("if (ImGui::BeginPopupContextItem(\"menu\")) {}\n", "BeginPopupContextItem"),
        ("if (ImGui::BeginPopupContextWindow()) {}\n", "BeginPopupContextWindow"),
    ]:
        if not rule_popups_are_skinned(fake, text):
            failures.append(f"self-test : un surgissant brut ({label}) passe encore")
    for text, label in [
        ("if (BeginSkinPopup(\"##menu\")) { EndSkinPopup(); }\n", "le surgissant habillé"),
        ("if (BeginSkinPopupContextItem(\"menu\")) { EndSkinPopup(); }\n", "le clic droit habillé"),
        ("if (ImGui::BeginPopupModal(\"x\")) { ImGui::EndPopup(); }\n", "une modale"),
        ("// ImGui::BeginPopup(\"ancien\");\nint x = 0;\n", "un commentaire"),
        ("const char* kAide = \"ImGui::BeginPopup(\";\n", "une chaîne"),
    ]:
        found = rule_popups_are_skinned(fake, text)
        if found:
            failures.append(f"self-test : faux positif sur {label} — {found[0]}")
    for implementation in sorted(IMPLEMENTATIONS):
        if rule_popups_are_skinned(Path(implementation), "ImGui::BeginPopup(\"x\");\n"):
            failures.append(f"self-test : l'implémentation {implementation} est refusée")
    if rule_popups_are_skinned(Path("libs/render/src/fictif.cpp"), "ImGui::BeginPopup(\"x\");\n"):
        failures.append("self-test : la règle des surgissants mord hors de ui et app")
    return failures
