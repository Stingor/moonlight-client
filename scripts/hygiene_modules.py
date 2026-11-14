"""Le branchement des règles qui vivent dans leur propre module.

`check_hygiene.py` est à son plafond de lignes : chaque règle y coûtait quatre
lignes de câblage — l'import, la règle de texte, le contrôle du dépôt entier,
l'auto-test. Elles se déclarent ici une fois, et `check_hygiene.py` ne les
connaît que par ces trois noms.

Pour une règle de plus : son import, et une ligne dans chacun des trois.
"""

from __future__ import annotations

from pathlib import Path

import hygiene_french
import hygiene_odr
import hygiene_popups
import hygiene_survivors
import hygiene_tooltips

# Les règles de texte, appliquées fichier par fichier ; chacune choisit
# elle-même les fichiers qu'elle regarde.
TEXT_RULES = (
    hygiene_french.rule_identifiers_are_english,
    hygiene_tooltips.rule_tooltips_are_skinned,
    hygiene_popups.rule_popups_are_skinned,
    hygiene_survivors.rule_survivors_stay_integral,
)


def repository_problems(root: Path) -> list[str]:
    """Ce qui ne se voit que sur le dépôt entier : les dispenses devenues inutiles,
    et deux types du même nom dans deux fichiers."""
    return (hygiene_french.rule_exemptions_still_needed(root)
            + hygiene_tooltips.rule_tooltip_dispenses_still_needed(root)
            + hygiene_odr.rule_one_type_per_name(root))


def self_test() -> list[str]:
    return (hygiene_french.self_test_french_identifiers()
            + hygiene_tooltips.self_test_skinned_tooltips()
            + hygiene_popups.self_test_skinned_popups()
            + hygiene_survivors.self_test_survivors_integral()
            + hygiene_odr.self_test_one_type_per_name())
