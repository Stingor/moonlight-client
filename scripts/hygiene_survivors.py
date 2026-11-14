"""Le mode survivors se rejoue à l'identique sous MSVC et sous g++.

Un journal de partie ne porte que le départ et les entrées du joueur : le
serveur le rejoue pour vérifier un score, et l'essai en or le rejoue sous les
deux compilateurs. Tout ce qui peut rendre un état différent d'une machine à
l'autre est donc refusé dans la simulation du mode, dans son générateur, et
dans ce qu'elle lit de la marche (le champ de poursuite, la case libre la plus
proche, la franchissabilité, les positions de case) :

- les flottants, dont l'arrondi dépend des options et du jeu d'instructions :
  le mot `float` ou `double`, un littéral flottant, une constante de
  `std::numbers`, et les fonctions mathématiques de `<cmath>` et de
  `<math.h>`, qualifiées par `std::` ou `::`, ou appelées sans qualification
  comme les rend une inclusion transitive (`sqrt(x)`, `sqrtf(x)`) ;
- les distributions et générateurs de la bibliothèque standard, dont la suite
  diffère d'une bibliothèque à l'autre ou n'est pas promise (`ro::core::Pcg32`
  écrit les siens), et `rand` ;
- les ordres non spécifiés : `std::sort`, `std::partition` et le tas
  (`priority_queue`, `make_heap`…), dont l'ordre entre éléments égaux dépend
  de l'implémentation, les conteneurs `unordered_*` (`pmr` compris) et
  `std::hash` ;
- l'horloge (`<chrono>`, `<ctime>`, `time`, `clock`) : la partie avance par
  pas, jamais par temps mesuré ;
- `using namespace std`, qui ferait d'un appel nu un appel à la bibliothèque.

Une exception se NOMME, par fichier et par jeton, avec sa raison : le
`std::priority_queue` de `walk_field.cpp` ne sort jamais deux éléments égaux,
sa clé (coût, rang de case) étant un ordre total. `<cstdlib>` reste permis :
c'est là que vit `std::abs` des entiers ; ce qui y est refusé (`rand`,
`qsort`) l'est par son nom. `walk_path.h`, d'où le champ tire ses coûts de
pas, n'est pas surveillé : il sert d'autres usages du client et inclut
`<unordered_map>`.

Les essais restent libres : ils comparent la table des sinus à `std::sin`.
"""

from __future__ import annotations

import re
from pathlib import Path

import hygiene_french

WATCHED_PREFIXES = ("libs/sim/include/ro/sim/survivors_", "libs/sim/src/survivors_")
WATCHED_FILES = frozenset({
    "libs/core/include/ro/core/pcg32.h",
    "libs/core/src/pcg32.cpp",
    "libs/sim/include/ro/sim/walk_field.h",
    "libs/sim/src/walk_field.cpp",
    "libs/sim/include/ro/sim/walk_map.h",
    "libs/sim/src/walk_map.cpp",
    "libs/sim/include/ro/sim/packed_position.h",
    "libs/sim/src/packed_position.cpp",
})
# Les jetons qu'un fichier surveillé a le droit d'employer, avec leur raison
# dans la docstring.
ALLOWED = {
    "libs/sim/src/walk_field.cpp": frozenset({"priority_queue"}),
}

# Les fonctions mathématiques : avec un suffixe `f` ou `l`, elles restent des
# flottants.
MATH = (r"sqrt|cbrt|sin|cos|tan|asin|acos|atan2?|sinh|cosh|tanh|pow|exp2?|expm1"
        r"|log(?:2|10|1p)?|hypot|floor|ceil|l?l?round|trunc|fmod|fabs|fmin|fmax|fma|lerp"
        r"|remainder|nearbyint|l?l?rint")
# Ce qui s'appelle : l'aléa, l'horloge, les ordres non spécifiés, les maths.
CALLABLE = (r"rand|srand|time|clock|qsort|sort|nth_element|partial_sort\w*|partition"
            r"|shuffle|random_shuffle|sample|make_heap|push_heap|pop_heap|sort_heap|" + MATH)
# Ce qui se nomme sans s'appeler : types, moteurs, espaces de noms.
NAMED = (r"unordered_\w+|random_device|hash|uniform_\w+|\w+_distribution|mt19937\w*"
         r"|minstd_rand0?|ranlux\w*|knuth_b|\w+_engine|chrono|priority_queue|numbers")
# `std::`, avec ou sans espaces, éventuellement suivi de `ranges::` ou `pmr::`.
STD = r"\bstd\s*::\s*(?:(?:ranges|pmr)\s*::\s*)?"

FORBIDDEN_CODE = (
    re.compile(r"\bfloat\b"),
    re.compile(r"\bdouble\b"),
    # Les littéraux flottants : `1.5`, `1.`, `.5`, `1e3`, avec ou sans suffixe.
    re.compile(r"\b\d+\.\d*([eE][-+]?\d+)?[fFlL]?\b|\b\d+[eE][-+]?\d+\b"
               r"|(?<![\w.])\.\d+([eE][-+]?\d+)?[fFlL]?\b|\b\d+\.(?![\w.])"),
    re.compile(STD + r"(?:" + NAMED + r")\b"),
    # Qualifié par `std::` ou par `::` seul (l'espace global de <math.h>).
    re.compile(r"(?:" + STD + r"|(?<![\w:])::\s*)(?:" + CALLABLE + r")[fl]?\b"),
    # Appelé sans qualification : ni membre (`.`, `->`), ni d'un autre espace.
    re.compile(r"(?<![\w.>:])(?:" + CALLABLE + r")[fl]?\s*\("),
    re.compile(r"\busing\s+namespace\s+std\b"),
)
# Sur la ligne brute, ancrée en tête : une directive commentée ne mord pas.
FORBIDDEN_INCLUDE = re.compile(
    r"^\s*#\s*include\s*[<\"](random|chrono|cmath|ctime|unordered_map|unordered_set|math\.h"
    r"|stdlib\.h|time\.h|numbers|cfloat|float\.h)[>\"]")
DEFINE = re.compile(r"#\s*define\b")

RULE = ("le mode survivors se rejoue sous MSVC et sous g++ : ni flottant, ni aléa ou horloge "
        "std::, ni ordre non spécifié")


def watched(path: Path) -> bool:
    posix = path.as_posix()
    return posix.startswith(WATCHED_PREFIXES) or posix in WATCHED_FILES


def _bites(path: Path, line: str) -> list[str]:
    """Les jetons interdits d'une ligne de code, un par emplacement."""
    allowed = ALLOWED.get(path.as_posix(), frozenset())
    taken: list[tuple[int, int]] = []
    tokens = []
    for pattern in FORBIDDEN_CODE:
        for found in pattern.finditer(line):
            span = found.span()
            # Un même emplacement ne mord qu'une fois, quelle que soit la forme
            # qui le voit.
            if any(span[0] < end and start < span[1] for start, end in taken):
                continue
            taken.append(span)
            token = found.group(0)
            if any(re.search(rf"\b{word}\b", token) for word in allowed):
                continue
            tokens.append(token)
    return tokens


def rule_survivors_stay_integral(path: Path, text: str) -> list[str]:
    """Aucun flottant, aléa standard, ordre non spécifié ni horloge dans la simulation du mode."""
    if path.suffix not in hygiene_french.CXX_SUFFIXES or not watched(path):
        return []
    problems = []
    for number, line in enumerate(hygiene_french.code_only(text).splitlines(), 1):
        for token in _bites(path, line):
            problems.append(f"{path}:{number}: `{token}` — {RULE}")
    # Le corps d'un #define est du code que code_only retire avec la directive :
    # il passe au même crible, sans ses commentaires ni ses chaînes.
    for start, end, kind in hygiene_french.segments(text):
        directive = text[start:end]
        opening = DEFINE.match(directive.lstrip())
        if kind != "directive" or not opening:
            continue
        body = hygiene_french.code_only(directive.lstrip()[opening.end():])
        first_line = text.count("\n", 0, start) + 1
        for offset, line in enumerate(body.splitlines()):
            for token in _bites(path, line):
                problems.append(f"{path}:{first_line + offset}: `{token}` — {RULE}")
    for number, line in enumerate(text.splitlines(), 1):
        found = FORBIDDEN_INCLUDE.search(line)
        if found:
            problems.append(f"{path}:{number}: `<{found.group(1)}>` — {RULE}")
    return problems


def self_test_survivors_integral() -> list[str]:
    """La règle mord quatre fois sur un fichier fautif, et se tait ailleurs."""
    failures = []
    fake = Path("libs/sim/src/survivors_x.cpp")
    guilty = ("#include <random>\n"
              "double ratio = 0;\n"
              "int half = static_cast<int>(0.5f);\n"
              "void Order(std::vector<int>& v) { std::sort(v.begin(), v.end()); }\n")
    found = rule_survivors_stay_integral(fake, guilty)
    if len(found) != 4:
        failures.append(f"self-test : la règle survivors devait mordre 4 fois, elle a mordu "
                        f"{len(found)} fois — {found}")
    # Chacune de ces formes mord une fois, seule dans son fichier.
    for text in [
        "void Order(std::vector<int>& v) { std::ranges::sort(v); }\n",
        "std::partial_sort_copy(a.begin(), a.end(), b.begin(), b.end());\n",
        "int half = Scale(.5f);\n",
        "int whole = Scale(5.);\n",
        "int whole = Scale(5. );\n",
        "#define kHalf 0.5\n",
        "#define kHalf \\\n  0.5\n",
        "#include <math.h>\n",
        "#include \"math.h\"\n",
        "#include <numbers>\n",
        "#include <cfloat>\n",
        "#include <float.h>\n",
        "std::int64_t root = std::sqrt(value);\n",
        "auto root = sqrt(value);\n",
        "auto root = ::sqrt(value);\n",
        "auto root = sqrtf(value);\n",
        "auto size = std::fabs(value);\n",
        "auto mid = std::lerp(a, b, t);\n",
        "auto low = std::fmin(a, b);\n",
        "auto pi = std::numbers::pi;\n",
        "std::mt19937 engine;\n",
        "std::default_random_engine engine;\n",
        "std::minstd_rand engine;\n",
        "std::ranlux24 engine;\n",
        "int roll = rand();\n",
        "auto now = std::time(nullptr);\n",
        "auto now = time(nullptr);\n",
        "auto ticks = std::clock();\n",
        "std::qsort(base, count, size, compare);\n",
        "std::random_shuffle(a, b);\n",
        "std::priority_queue<int> open;\n",
        "std::make_heap(a, b);\n",
        "std::partition(a, b, keep);\n",
        "std::ranges::partition(v, keep);\n",
        "std :: sort(a, b);\n",
        "std::pmr::unordered_map<int, int> table;\n",
        "using namespace std;\n",
    ]:
        found = rule_survivors_stay_integral(fake, text)
        if len(found) != 1:
            failures.append(f"self-test : la règle survivors devait mordre une fois sur "
                            f"{text.strip()!r}, elle a mordu {len(found)} fois — {found}")
    for text, label in [
        ("// #include <random>, double, 0.5f, std::sort\nint x = 0;\n", "un commentaire"),
        ("const char* kAide = \"double 0.5f std::sort\";\n", "une chaîne"),
        ("std::stable_sort(v.begin(), v.end());\n", "std::stable_sort"),
        ("std::ranges::stable_sort(v);\n", "std::ranges::stable_sort"),
        ("std::stable_partition(a, b, keep);\n", "std::stable_partition"),
        ("std::uint32_t total = values[0].x + 12;\n", "un entier"),
        ("throw std::logic_error(\"x\"); auto n = std::size(v);\n", "std::logic_error"),
        ("auto e = std::expected<int, int>{};\n", "std::expected"),
        ("#define kCount 5  // 0.5 en commentaire\n", "un #define entier"),
        ("#include <cstdint>\n", "une inclusion permise"),
        ("#include <cstdlib>\n", "<cstdlib>, où vit std::abs"),
        ("auto gap = std::abs(dx);\n", "std::abs"),
        ("std::uint32_t time_step_ms = wave.floor + rule->exp;\n", "des noms de champ"),
        ("auto at = clock.time(3); auto key = sort_key(x);\n", "un membre et un préfixe"),
        ("auto order = ro::sim::sort(v);\n", "une fonction d'un autre espace"),
    ]:
        found = rule_survivors_stay_integral(fake, text)
        if found:
            failures.append(f"self-test : faux positif de la règle survivors sur {label} — {found[0]}")
    # L'exception nommée : le tas du champ de marche, et lui seul.
    queue = "std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;\n"
    if rule_survivors_stay_integral(Path("libs/sim/src/walk_field.cpp"), queue):
        failures.append("self-test : la règle survivors refuse le tas permis de walk_field.cpp")
    if len(rule_survivors_stay_integral(Path("libs/sim/src/walk_field.cpp"), guilty)) != 4:
        failures.append("self-test : l'exception de walk_field.cpp couvre plus que son tas")
    for path in ("libs/sim/src/world.cpp", "tests/sim/survivors_geometry_test.cpp"):
        if rule_survivors_stay_integral(Path(path), guilty):
            failures.append(f"self-test : la règle survivors mord hors de son champ ({path})")
    for path in sorted(WATCHED_FILES):
        if not rule_survivors_stay_integral(Path(path), guilty):
            failures.append(f"self-test : la règle survivors ne voit pas {path}")
    return failures
