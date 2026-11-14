"""Le graphe des couches, gardé dans les DEUX sens.

L'éditeur de liens refuse qu'une couche inclue ce qu'elle ne lie pas ; mais un
lien PUBLIC est transitif, et il laisse donc passer ce qu'une couche ne voit
que par une autre. Cette règle-ci ferme le second sens. Elle vit à part de
`check_hygiene.py`, qui l'emploie, pour que celui-ci tienne sous son plafond.
"""

from __future__ import annotations

import functools
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Ce qu'une couche inclut d'une autre : « #include "ro/<couche>/… ».
LAYER_INCLUDE = re.compile(r'^\s*#\s*include\s+"ro/([a-z_]+)/')
LAYER_PUBLIC_LINKS = re.compile(r"target_link_libraries\(\s*ro_([a-z_]+)\s+PUBLIC([^)]*)\)")


@functools.lru_cache(maxsize=None)
def layer_dependencies() -> dict[str, frozenset[str]]:
    """Le graphe des couches, LU dans leurs `CMakeLists.txt` et nulle part ailleurs.

    ⚠ UNE SEULE SOURCE : une table recopiée ici dériverait du graphe qu'elle
    prétend garder, et c'est l'éditeur de liens qui fait foi.
    """
    graph: dict[str, frozenset[str]] = {}
    for listing in sorted((REPO_ROOT / "libs").glob("*/CMakeLists.txt")):
        layer = listing.parent.name
        for target, links in LAYER_PUBLIC_LINKS.findall(listing.read_text(encoding="utf-8")):
            if target != layer:
                continue
            graph[layer] = frozenset(re.findall(r"\bro_([a-z_]+)", links))
    return graph


def rule_layer_includes(path: Path, text: str) -> list[str]:
    """Une couche n'inclut que ses dépendances DIRECTES.

    L'éditeur de liens garde le graphe dans un sens seulement : un lien PUBLIC
    est transitif, si bien que `ui`, qui lie `render`, voit aussi les en-têtes
    de `vfs` que `render` lie — et les inclurait sans une erreur. La couche
    dépendrait alors de ce que CLAUDE.md ne lui donne pas, sans que rien le dise.
    """
    parts = path.as_posix().split("/")
    if len(parts) < 3 or parts[0] != "libs":
        return []
    layer = parts[1]
    allowed = layer_dependencies().get(layer)
    if allowed is None:
        return []
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        included = LAYER_INCLUDE.match(line)
        if included is None:
            continue
        other = included.group(1)
        if other != layer and other not in allowed:
            problems.append(
                f"{path}:{number}: `{layer}` inclut `ro/{other}` — "
                f"hors de ses dépendances directes ({', '.join(sorted(allowed)) or 'aucune'})"
            )
    return problems
