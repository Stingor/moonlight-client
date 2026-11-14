"""Un générateur échappe par `gen_support`, et ne pose pas son propre dossier.

Un chemin du client s'écrit avec des antislashs, qui doublent dans un littéral
C++. Chaque générateur qui recopie l'échappement, ou le nom du dossier des
textures, choisit seul s'il le tient brut ou déjà doublé : un seul des deux
sens est juste à chaque endroit, et rien ne dit lequel. `gen_support` en porte
un nom par sens — `DOSSIER_TEXTURES` et `DOSSIER_TEXTURES_CPP`,
`echapper_cpp`, `chaine_cpp`, `chemin_texture_cpp` — et cette règle les y garde.

⚠ ELLE NE VISE QUE LES GÉNÉRATEURS. Ailleurs, le même `.replace` peut échapper
autre chose qu'un littéral C++ : le catalogue des traductions, par exemple,
n'échappe pas les guillemets.
"""

from __future__ import annotations

import re
from pathlib import Path

SUPPORT = "scripts/gen_support.py"

OWN_ESCAPE = re.compile(
    r"^\s*def (?:chemin_cpp|cxx)\b"
    r"|^DOSSIER_(?:TEXTURES|SPRITES)(?:_CPP)? ="
    r'|\.replace\("\\\\", "\\\\\\\\"\)')


def rule_generator_escapes_via_support(path: Path, text: str) -> list[str]:
    """Aucun générateur ne recopie l'échappement C++ ni le dossier des textures."""
    posix = path.as_posix()
    if not posix.startswith("scripts/gen_") or posix == SUPPORT:
        return []
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        if OWN_ESCAPE.search(line):
            problems.append(
                f"{path}:{number}: échappement ou dossier recopié — employer ceux de "
                f"gen_support (echapper_cpp, chemin_texture_cpp, DOSSIER_TEXTURES_CPP…)"
            )
    return problems


def self_test_generator_escapes() -> list[str]:
    """La règle mord sur chaque copie qu'elle vise, et se tait hors des générateurs."""
    failures = []
    generator = Path("scripts/gen_client_fictif.py")
    must_reject = [
        ("def chemin_cpp(ressource):\n    return ressource\n", "une fonction de chemin"),
        ("def cxx(text):\n    return text\n", "une fonction de littéral"),
        ('DOSSIER_TEXTURES = "data\\\\\\\\texture\\\\\\\\"\n', "un dossier recopié"),
        ('chemin = image.replace("\\\\", "\\\\\\\\")\n', "un échappement en ligne"),
    ]
    for text, label in must_reject:
        if not rule_generator_escapes_via_support(generator, text):
            failures.append(f"self-test : {label} passe encore dans un générateur")
    accepted = 'from gen_support import chemin_texture_cpp\nchemin = chemin_texture_cpp(image)\n'
    found = rule_generator_escapes_via_support(generator, accepted)
    if found:
        failures.append(f"self-test : faux positif sur l'emploi de gen_support — {found[0]}")
    escape = 'return text.replace("\\\\", "\\\\\\\\").replace("\\n", "\\\\n")\n'
    if rule_generator_escapes_via_support(Path("scripts/check_translations.py"), escape):
        failures.append("self-test : la règle des générateurs mord hors des générateurs")
    if rule_generator_escapes_via_support(Path(SUPPORT), must_reject[3][0]):
        failures.append("self-test : la règle des générateurs mord sur gen_support lui-même")
    return failures
