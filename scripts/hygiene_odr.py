"""Un nom de type par namespace : deux définitions du même type se taisent.

Deux `struct SkillRule` dans `ro::sim`, posées dans deux fichiers que rien ne
relie, compilent et se lient sans un mot : c'est une violation de la règle de
la définition unique, que ni MSVC ni l'éditeur de liens ne signalent. Le
lieur garde UNE des deux dispositions pour les fonctions en ligne qui les
emploient, et le code de l'autre lit ses champs au mauvais pas.

La règle relève chaque type défini à portée de namespace — `struct`, `class`,
`union`, `enum` — sous son nom complet, et refuse qu'un même nom complet soit
défini dans deux fichiers. Un namespace anonyme est propre à son fichier : ce
qu'il définit n'entre pas dans le relevé. Une déclaration anticipée
(`struct X;`) et une spécialisation (`struct hash<X>`) ne définissent pas de
nouveau nom.
"""

from __future__ import annotations

import re
import tempfile
from pathlib import Path

# Les cibles liées ensemble : une collision entre deux de leurs fichiers est
# une collision dans un même programme.
SCANNED_DIRECTORIES = ("libs", "app", "tests")
CXX_SUFFIXES = {".h", ".hpp", ".cpp", ".cc"}

# Le texte du code, sans commentaires ni littéraux : une accolade ou un
# « struct » écrits dans une chaîne ne comptent pas.
_NOISE = re.compile(
    r'//[^\n]*|/\*.*?\*/|R"(?P<tag>[^(\s]*)\(.*?\)(?P=tag)"|"(?:\\.|[^"\\\n])*"'
    r"|'(?:\\.|[^'\\\n])*'",
    re.DOTALL,
)
_NAMESPACE = re.compile(r"(?:\binline\s+)?\bnamespace\s*(?P<name>[\w:]*)\s*(?:\[\[[^\]]*\]\]\s*)?$")
_EXTERN_BLOCK = re.compile(r'\bextern\s*$')
_TYPE = re.compile(
    r"(?:^|[\s;])(?:struct|class|union|enum(?:\s+(?:class|struct))?)\s+"
    r"(?:\[\[[^\]]*\]\]\s*|alignas\s*\([^)]*\)\s*)*(?P<name>\w+)\s*(?P<after>[^{]*)$"
)


def _strip_noise(text: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "\n" * match.group(0).count("\n") + " "

    text = re.sub(r"^[ \t]*#[^\n]*(?:\\\n[^\n]*)*", "", text, flags=re.MULTILINE)
    return _NOISE.sub(blank, text)


def defined_types(text: str) -> list[tuple[str, int]]:
    """Les types définis à portée de namespace nommé : (nom complet, ligne)."""
    code = _strip_noise(text)
    found = []
    # Chaque accolade ouverte : le namespace qu'elle ouvre, « » pour un bloc
    # qui ne porte pas de types de namespace (anonyme, classe, fonction), et
    # None pour un bloc transparent (`extern "C"`).
    scopes: list[str | None] = []
    statement_start = 0
    for index, character in enumerate(code):
        if character in ";}":
            if character == "}" and scopes:
                scopes.pop()
            statement_start = index + 1
            continue
        if character != "{":
            continue
        head = code[statement_start:index].strip()
        statement_start = index + 1
        namespace = _NAMESPACE.search(head)
        if namespace:
            scopes.append(namespace.group("name") or "")
            continue
        if _EXTERN_BLOCK.search(head):
            scopes.append(None)
            continue
        opened = [scope for scope in scopes if scope is not None]
        in_namespace = all(scope for scope in opened)
        kind = _TYPE.search(head) if in_namespace and opened else None
        # `<` ouvre une spécialisation, `::` la définition hors ligne d'un
        # type imbriqué (`struct Owner::Impl`) : ni l'une ni l'autre ne
        # nomme un type de namespace.
        if kind and not kind.group("after").lstrip().startswith(("<", "::")):
            qualified = "::".join([*opened, kind.group("name")])
            found.append((qualified, code.count("\n", 0, index) + 1))
        scopes.append("")
    return found


def rule_one_type_per_name(root: Path) -> list[str]:
    first_seen: dict[str, str] = {}
    problems = []
    for directory in SCANNED_DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in CXX_SUFFIXES:
                continue
            relative = path.relative_to(root).as_posix()
            for qualified, line in defined_types(path.read_text(encoding="utf-8")):
                where = f"{relative}:{line}"
                if qualified not in first_seen:
                    first_seen[qualified] = where
                elif not first_seen[qualified].startswith(f"{relative}:"):
                    problems.append(
                        f"{where}: `{qualified}` est déjà défini en {first_seen[qualified]} — "
                        f"deux types du même nom dans un namespace se lient sans un mot ; "
                        f"renommer l'un, ou le mettre dans un namespace anonyme"
                    )
    return problems


def self_test_one_type_per_name() -> list[str]:
    failures = []
    cases = (
        ("namespace ro::sim {\nstruct SkillRule {\n  int a;\n};\n}\n", ["ro::sim::SkillRule"]),
        ("namespace ro { namespace sim {\nenum class Kind : int { kA };\n} }\n",
         ["ro::sim::Kind"]),
        ("namespace ro::sim {\nnamespace {\nstruct Local {};\n}\n}\n", []),
        ("namespace ro::sim {\nstruct Forward;\nstruct Outer {\n  struct Inner {};\n};\n}\n",
         ["ro::sim::Outer"]),
        ("namespace std {\ntemplate <>\nstruct hash<ro::Key> {};\n}\n", []),
        ('namespace ro {\n// struct Fake {\nconst char* s = "struct Nope {";\n'
         "void F() {\n  struct InFunction {};\n}\n}\n", []),
        ('extern "C" {\nnamespace ro {\nstruct Visible {};\n}\n}\n', ["ro::Visible"]),
        ("namespace ro {\nstruct Owner::Impl {};\n}\n", []),
    )
    for text, expected in cases:
        got = [name for name, _ in defined_types(text)]
        if got != expected:
            failures.append(f"self-test : un type par nom — attendu {expected}, relevé {got}")

    with tempfile.TemporaryDirectory() as scratch:
        root = Path(scratch)
        (root / "libs").mkdir()
        rule = "namespace ro::sim {\nstruct SkillRule {\n  int a;\n};\n}\n"
        (root / "libs" / "skill_tree.h").write_text(rule, encoding="utf-8")
        (root / "libs" / "survivors_rules.h").write_text(rule, encoding="utf-8")
        if len(rule_one_type_per_name(root)) != 1:
            failures.append("self-test : un type par nom — deux fichiers ne sont pas refusés")
    return failures
