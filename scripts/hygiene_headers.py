"""Un en-tête ne définit pas de fonction à liaison externe.

Une fonction définie dans un en-tête sans `inline` est définie une fois par
fichier qui l'inclut. Tant qu'un seul l'inclut, rien ne se voit ; la seconde
inclusion casse le lien, loin de l'en-tête et longtemps après. Le compilateur
ne dit rien, et la relecture ne regarde pas un en-tête que personne ne touche.

Ce qui rend une définition sûre : `inline`, `constexpr` ou `consteval` (qui
l'impliquent), un gabarit, `static` ou un espace de noms anonyme (liaison
interne). Une méthode définie DANS sa classe est implicitement `inline` ; une
méthode définie hors de sa classe, dans l'en-tête, ne l'est pas, et la règle la
voit aussi.
"""

from __future__ import annotations

import re
from pathlib import Path

HEADER_SUFFIXES = (".h", ".hpp")

# Ce qui, dans la tête d'une définition, lui ôte la liaison externe ou la rend
# définissable plusieurs fois.
SAFE_SPECIFIER = re.compile(r"\b(inline|constexpr|consteval|static|template)\b")

# Une tête qui ouvre une portée où les définitions sont à liaison externe.
NAMESPACE_HEAD = re.compile(r"^\s*(inline\s+)?namespace\b[\w:\s]*$|^\s*extern\s*(\"\")?\s*$")
ANONYMOUS_NAMESPACE_HEAD = re.compile(r"^\s*(inline\s+)?namespace\s*$")
TYPE_HEAD = re.compile(r"\b(class|struct|union|enum)\b")

# La fin d'une tête de fonction : la parenthèse fermante des paramètres, puis
# ce qu'on peut écrire entre elle et le corps.
FUNCTION_TAIL = re.compile(
    r"\)\s*(const|noexcept(\s*\([^)]*\))?|override|final|&&?|->\s*[\w:<>,\s*&]+|\s)*"
    r"(:\s*[\w:]+\s*[({].*)?$",
    re.DOTALL,
)


def strip_comments_and_literals(text: str) -> str:
    """Le texte sans commentaires, chaînes ni directives, lignes conservées."""
    out = []
    at = 0
    size = len(text)
    while at < size:
        c = text[at]
        if text.startswith("//", at):
            end = text.find("\n", at)
            at = size if end < 0 else end
        elif text.startswith("/*", at):
            end = text.find("*/", at + 2)
            end = size if end < 0 else end + 2
            out.append("\n" * text.count("\n", at, end))
            at = end
        elif c in "\"'":
            at += 1
            while at < size and text[at] != c:
                at += 2 if text[at] == "\\" else 1
            out.append(c + c)
            at += 1
        elif c == "#" and not text[text.rfind("\n", 0, at) + 1:at].strip():
            while at < size and text[at] != "\n":
                at += 2 if text.startswith("\\\n", at) else 1
        else:
            out.append(c)
            at += 1
    return "".join(out)


def is_function_head(head: str) -> bool:
    opening = head.find("(")
    if opening < 0 or "=" in head[:opening]:
        return False
    return FUNCTION_TAIL.search(head[opening:]) is not None


def rule_header_defines_inline(path: Path, text: str) -> list[str]:
    """Aucune fonction à liaison externe définie dans un en-tête."""
    if path.suffix not in HEADER_SUFFIXES:
        return []
    code = strip_comments_and_literals(text)
    problems = []
    # Ce que chaque accolade ouverte a ouvert : « ns » une portée de noms à
    # liaison externe, « interne » une portée à liaison interne, « autre » le
    # reste (type, corps, initialiseur).
    scopes: list[str] = []
    head_start = 0
    for at, c in enumerate(code):
        if c not in "{};":
            continue
        head = code[head_start:at]
        head_start = at + 1
        if c != "{":
            if c == "}" and scopes:
                scopes.pop()
            continue
        at_namespace_scope = all(scope == "ns" for scope in scopes)
        if ANONYMOUS_NAMESPACE_HEAD.match(head):
            scopes.append("interne")
        elif NAMESPACE_HEAD.match(head):
            scopes.append("ns")
        elif at_namespace_scope and not TYPE_HEAD.search(head.split("(")[0]) and \
                is_function_head(head) and not SAFE_SPECIFIER.search(head):
            opening = head_start - 1 - len(head) + head.find("(")
            words = code[:opening].split()
            problems.append(
                f"{path}:{code.count(chr(10), 0, opening) + 1}: `{words[-1] if words else '?'}` "
                f"est définie dans un en-tête sans `inline` — la seconde inclusion cassera le lien"
            )
            scopes.append("autre")
        else:
            scopes.append("autre")
    return problems


def self_test_header_definitions() -> list[str]:
    """La règle mord sur une définition à liaison externe, et se tait sur le reste."""
    failures = []
    header = Path("libs/formats/src/fictif.h")
    must_reject = [
        ("namespace ro {\nbool IsDigit(char c) { return c >= '0'; }\n}\n",
         "une fonction libre"),
        ("[[nodiscard]] int Twice(int value)\n{\n  return value * 2;\n}\n",
         "une fonction à l'accolade seule"),
        ("struct Foo { int Get() const; };\nint Foo::Get() const { return 0; }\n",
         "une méthode hors de sa classe"),
    ]
    for text, label in must_reject:
        if not rule_header_defines_inline(header, text):
            failures.append(f"self-test : {label} définie dans un en-tête passe encore")
    must_accept = [
        ("inline bool IsDigit(char c) { return c >= '0'; }\n", "une fonction `inline`"),
        ("constexpr bool IsDigit(char c) { return c >= '0'; }\n", "une fonction `constexpr`"),
        ("template <typename T>\nbool Has(const T& words) {\n  return true;\n}\n",
         "un gabarit"),
        ("namespace {\nint Hidden() { return 0; }\n}\n", "un espace de noms anonyme"),
        ("class Lexer {\n public:\n  int Next() { if (a) { return 1; } return 0; }\n};\n",
         "une méthode définie dans sa classe"),
        ("constexpr std::array<int, 2> kPair = {1, 2};\nint Declared(int value);\n",
         "un initialiseur et une déclaration"),
        ("// void Commented() { }\nconst char* kText = \"f() {\";\n",
         "un commentaire et une chaîne"),
        ("#define BODY() { }\n", "une directive"),
        ("inline const auto kTwice = [](int value) { return value * 2; };\n", "une lambda"),
    ]
    for text, label in must_accept:
        found = rule_header_defines_inline(header, text)
        if found:
            failures.append(f"self-test : faux positif sur {label} — {found[0]}")
    if rule_header_defines_inline(Path("libs/formats/src/fictif.cpp"), must_reject[0][0]):
        failures.append("self-test : la règle des en-têtes mord sur un .cpp")
    return failures
