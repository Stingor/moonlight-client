#!/usr/bin/env python3
"""Contrôles d'hygiène mécaniques, en local et en CI.

Chaque règle ici a été payée au moins une fois. Ce ne sont pas des
préférences de style — clang-format s'en charge — mais des défauts que ni le
compilateur ni la relecture n'attrapent.

Usage :
    python scripts/check_hygiene.py            # contrôle le dépôt
    python scripts/check_hygiene.py --self-test # vérifie que les règles MORDENT

Le mode --self-test existe pour une raison précise : un contrôle de fins de
ligne écrit en bash affichait « ok » sur tous les fichiers et ne testait
RIEN — le motif partait vide et matchait chaque ligne. La leçon tient en une
phrase : un contrôle qui passe toujours ne prouve rien, et il faut le soumettre à
un cas qu'il DOIT rejeter avant de lui faire confiance. Le crochet de push le
lance donc avant le contrôle réel, et l'intégration aussi.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from hygiene_catalogues import CATALOGUE_FILES, MAX_CATALOGUE_ENTRY_LINES
from hygiene_frames import rule_every_family_case_is_routed, self_test_family_routing
import hygiene_modules
from hygiene_generators import rule_generator_escapes_via_support, self_test_generator_escapes
from hygiene_headers import rule_header_defines_inline, self_test_header_definitions
from hygiene_layers import layer_dependencies, rule_layer_includes
from hygiene_provenance import rule_no_native_provenance, self_test_native_provenance

REPO_ROOT = Path(__file__).resolve().parent.parent

SCANNED_DIRECTORIES = ("app", "libs", "scripts", "tools", "tests")

# Les scripts comptent AUSSI. Le générateur de protocole a dépassé 1 100 lignes
# sans que rien ne le dise, parce que le contrôle ne regardait que le C++ : la
# règle existait, l'outil qui la fait respecter s'en exemptait lui-même.
SOURCE_SUFFIXES = (".h", ".hpp", ".cpp", ".cc", ".py")
CXX_SUFFIXES = (".h", ".hpp", ".cpp", ".cc")

# Un fichier de 9 086 lignes s'est déjà vu, et l'audit qui l'a mesuré impute trois
# bugs critiques à un OnRenderUI de 1 702 lignes à onze niveaux d'accolades.
# 800 lignes laisse largement de quoi écrire une fenêtre complète.
MAX_LINES = 800

# Le dossier où atterrit le code produit par un générateur.
GENERATED_DIRECTORY = "generated"

MEMSET_ON_AGGREGATE = re.compile(r"\bmemset\s*\(\s*&")
TRAILING_WHITESPACE = re.compile(r"[ \t]+$")


def longest_catalogue_entry(text: str) -> tuple[int, str]:
    """La plus longue entrée d'un catalogue, et la ligne qui l'ouvre.

    ⚠ UNE ENTRÉE SE DÉLIMITE PAR SA FERMETURE, jamais par son ouverture. Une
    signature qui déborde sur deux lignes commence par de l'indentation ; la
    chercher par son début laisserait passer justement les entrées les plus
    grosses, qui sont celles dont la signature déborde.

    Une entrée s'ouvre à la première ligne de premier niveau qui ne soit ni une
    fermeture, ni une directive, ni un commentaire, ni une portée nommée — un
    `namespace` engloberait le fichier entier et rendrait toujours zéro entrée
    trop longue.

    ⚠ ET UN CATALOGUE DE DÉCLARATIONS N'A AUCUNE ACCOLADE FERMANTE. Un en-tête
    qui ne porte que des prototypes n'en referme jamais une au premier niveau, si
    bien qu'un délimiteur qui n'attend que `}` ne voit qu'UNE entrée : tout le
    fichier. Il rendait donc « entrée de 557 lignes » pour un catalogue dont la
    plus grosse entrée en fait trente — et la dispense devenait inutilisable
    justement là où elle sert. Une déclaration se ferme sur le point-virgule qui
    clôt sa signature, et c'est une fermeture aussi bonne que l'accolade.

    ⚠ MAIS UN POINT-VIRGULE NE FERME QUE HORS DES ACCOLADES. Sans cette
    réserve, la PREMIÈRE instruction d'un corps de fonction refermait l'entrée :
    `effect_node_sow.cpp` se mesurait alors à douze lignes quand sa plus grosse
    méthode en fait trente-quatre, et le plafond de quatre-vingts devenait
    inatteignable. Le contrôle passait toujours — c'est-à-dire qu'il ne prouvait
    plus rien, exactement le défaut que le mode --self-test existe pour
    attraper, et qu'il a attrapé.
    """
    longest, name = 0, ""
    ouverture: tuple[int, str] | None = None
    # La profondeur d'accolades DEPUIS l'ouverture de l'entrée en cours. C'est
    # elle qui distingue le point-virgule d'une déclaration de celui d'une
    # instruction.
    depth = 0
    for index, line in enumerate(text.splitlines()):
        if ouverture is None:
            if not line or line[0].isspace() or line.startswith(
                    ("}", "#", "//", "namespace", "using", "extern")):
                continue
            ouverture = (index, line)
            depth = 0
        depth += line.count("{") - line.count("}")
        closing = line.rstrip()
        if depth <= 0 and (closing.endswith((";", "}")) or line.startswith("}")):
            longueur = index - ouverture[0] + 1
            if longueur > longest:
                longest, name = longueur, ouverture[1].strip()
            ouverture = None
    return longest, name


def longest_python_catalogue_entry(text: str) -> tuple[int, str]:
    """La plus longue entrée d'un catalogue Python, et la ligne qui l'ouvre.

    ⚠ DANS UNE TABLE PYTHON, L'ENTRÉE EST L'ARTICLE, PAS LA TABLE. Le
    délimiteur C++ ne verrait qu'une entrée par table — `CHAMPS_LUS_A_LA_MAIN`
    en fait cent quatre-vingt-onze lignes — et la dispense serait inutilisable
    justement là où elle sert. Ce qu'on cherche dans un tel fichier est UN
    article, jamais la table qui le range.

    Une entrée commence donc à l'une de ces deux frontières, et court jusqu'à la
    suivante :

    · une ligne SANS INDENTATION, hors de toute parenthèse — un énoncé de
      premier niveau. C'est elle qui attrape ce qui n'est pas une table : une
      fonction qui enflerait dans un fichier dispensé serait sinon invisible,
      et le passe-droit reviendrait par cette porte ;
    · dans une table, la ligne qui suit celle ramenant la profondeur au niveau
      de la table ET se terminant par une virgule — la fin d'un article.

    Les commentaires qui précèdent un article lui appartiennent : ils disent
    d'où il vient, et c'est justement ce qui fait la taille d'une entrée.
    """
    lines = text.splitlines()
    starts: list[int] = []
    depth = 0
    for index, line in enumerate(lines):
        before = depth
        depth += sum(line.count(c) for c in "([{") - sum(line.count(c) for c in ")]}")
        stripped = line.strip()
        if before == 0 and stripped and not line[0].isspace():
            starts.append(index)
        elif before >= 1 and depth == 1 and stripped.endswith(","):
            starts.append(index + 1)
    starts = [start for start in starts if start < len(lines)]
    longest, name = 0, ""
    for position, start in enumerate(starts):
        end = starts[position + 1] if position + 1 < len(starts) else len(lines)
        if end - start > longest:
            longest, name = end - start, lines[start].strip()
    return longest, name


def rule_file_length(path: Path, text: str) -> list[str]:
    """Un fichier trop long ne se relit plus, il se parcourt.

    ⚠ Le plafond ne s'applique PAS aux fichiers produits par un générateur. Il
    existe pour forcer à découper ce qu'on écrit à la main ; une table de mille
    entrées écrite par machine ne se relit pas, ne se maintient pas, et se
    régénère d'un coup. Le découper ne rendrait service à personne.

    ⚠ Il ne s'applique pas non plus aux CATALOGUES, et pour une autre raison :
    ceux-là sont écrits à la main, mais ce sont des listes d'entrées
    indépendantes où l'on cherche sans jamais lire. Ils paient leur dispense
    d'un second plafond, sur la taille de chaque entrée. Voir `CATALOGUE_FILES`.

    Les deux exemptions sont ÉTROITES : elles ne portent que sur la longueur du
    fichier. Toutes les autres règles s'appliquent au code généré comme au
    catalogue, parce qu'un générateur peut très bien émettre un commentaire
    terminé par un antislash.
    """
    if GENERATED_DIRECTORY in path.parts:
        return []
    if path.as_posix() in CATALOGUE_FILES:
        # ⚠ LE DÉLIMITEUR SUIT LE LANGAGE. Une entrée de C++ se ferme sur une
        # accolade ou un point-virgule ; une entrée de Python est un article de
        # table, et le délimiteur C++ rendrait la table entière.
        longueur, entree = (longest_python_catalogue_entry(text) if path.suffix == ".py"
                            else longest_catalogue_entry(text))
        if longueur > MAX_CATALOGUE_ENTRY_LINES:
            return [f"{path}: entrée de {longueur} lignes "
                    f"(plafond {MAX_CATALOGUE_ENTRY_LINES}) — « {entree} ». "
                    f"Un catalogue est dispensé du plafond de FICHIER, pas de "
                    f"celui de ses entrées : découper celle-ci, ou sortir le "
                    f"fichier de CATALOGUE_FILES"]
        return []
    line_count = len(text.splitlines())
    if line_count > MAX_LINES:
        return [f"{path}: {line_count} lignes (plafond {MAX_LINES}) — à découper"]
    return []


def rule_backslash_comment(path: Path, text: str) -> list[str]:
    r"""Un commentaire // terminé par un antislash AVALE la ligne suivante.

    La continuation de ligne est appliquée en phase 2 de traduction, AVANT la
    reconnaissance des commentaires en phase 3 : le compilateur ne voit qu'une
    ligne de commentaire, et l'instruction suivante disparaît du programme sans
    laisser la moindre trace — pas même sa chaîne littérale dans le binaire.

    Vécu : une variable restait non initialisée et le jeu plantait
    dans mkdir. MSVC a bien le warning C4010, mais la cible était compilée /w.
    """
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.rstrip()
        if not stripped.endswith("\\"):
            continue
        comment_at = stripped.find("//")
        if comment_at != -1 and comment_at < len(stripped) - 1:
            problems.append(
                f"{path}:{number}: commentaire // terminé par un antislash — "
                f"la ligne suivante ne sera pas compilée"
            )
    return problems


def rule_memset_on_aggregate(path: Path, text: str) -> list[str]:
    """memset(&x, 0, ...) demande LA VALEUR ZÉRO, pas « les défauts ».

    Une struct dont un champ vaut -1 par défaut (« rien d'imposé ») ne revient
    pas à son défaut après un memset : elle passe à 0, valeur en général
    légitime ET différente. Vu ailleurs : un bouton « réinitialiser » ne
    réinitialisait rien de visible, sans la moindre erreur.

    La remise à zéro d'un agrégat C++ s'écrit `x = T();`.
    """
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        if MEMSET_ON_AGGREGATE.search(line):
            problems.append(
                f"{path}:{number}: memset sur un agrégat — écrire `x = T();`"
            )
    return problems


def rule_trailing_whitespace(path: Path, text: str) -> list[str]:
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        if TRAILING_WHITESPACE.search(line):
            problems.append(f"{path}:{number}: espaces en fin de ligne")
    return problems


def rule_mixed_line_endings(path: Path, raw: bytes) -> list[str]:
    """Seul le MÉLANGE est un défaut, pas le CRLF en lui-même.

    Le dépôt est normalisé en LF par .gitattributes, mais git le rend en CRLF
    dans l'arbre de travail sous Windows (core.autocrlf) : c'est NORMAL. Ce qui
    ne l'est pas, c'est un fichier qui porte les deux — signe d'un script qui a
    réécrit une partie du fichier en changeant ses fins de ligne.

    On compte des OCTETS, en Python. Le même contrôle en bash rendait « ok »
    partout sans rien mesurer.
    """
    crlf = raw.count(b"\r\n")
    lone_lf = raw.count(b"\n") - crlf
    if crlf and lone_lf:
        return [f"{path}: fins de ligne MIXTES ({crlf} CRLF, {lone_lf} LF isolés)"]
    return []


# Le dossier où vit le DOS d'ImGui, et lui seul.
UI_BACKEND_DIRECTORY = "libs/ui/gl/"

# ── CE QUI S'ÉCRIT HORS DU JOURNAL N'EXISTE POUR PERSONNE ────────────────────
#
# La cible est bâtie en sous-système CONSOLE : un `std::printf` s'affiche, donc
# il a l'air de marcher. Il n'entre pourtant ni dans le `moonclient.log` qu'un
# joueur joint à son rapport, ni dans la fenêtre de journal, ni derrière son
# filtre par gravité. Dix écritures avaient dérivé ainsi, dont la connexion,
# l'entrée sur la première carte et TOUTE la famille des refus d'émission — la
# marche, le ramassage, le battement d'horloge — pendant que leurs sœurs du sac
# et du coffre passaient par `LogWarning`.
#
# ⚠ `std::snprintf` N'EST PAS VISÉ : il formate dans un tampon, il n'écrit nulle
# part. Ce qui est interdit, c'est d'ÉMETTRE.
CONSOLE_WRITES = re.compile(
    r"\bstd::(?:printf|fprintf|vprintf|vfprintf|puts|fputs|putchar|perror|cout|cerr|clog)\b"
)

# ⚠ NOMINATIVE, jamais un motif. Le texte d'usage de `--aide` n'est pas une
# trace : il répond à une question posée sur la ligne de commande, et il doit
# sortir avant que le journal n'existe.
CONSOLE_FILES = ("app/game/command_line.cpp",)

# Ce qu'un fichier de `libs/ui/` hors de ce dossier n'a pas le droit d'inclure.
PLATFORM_INCLUDES = re.compile(
    r'#\s*include\s*[<"](?:imgui_impl_|SDL3/|glad/|GL/|KHR/)'
)


# ── CE QU'UNE LISTE DE CHOIX DOIT ÊTRE ──────────────────────────────────────
#
# 🔴 UNE RANGÉE DE RÉGLAGE NE COPIE PAS SES CHOIX, elle en garde un `span`. Les
# libellés doivent donc vivre de la COMPOSITION des rangées jusqu'à leur DESSIN —
# ni moins, ni plus.
#
# Les deux façons de se tromper mènent au même plantage, et les deux ont été
# écrites :
#
#   · un `static` capture les pointeurs UNE fois, et le catalogue de traduction
#     qui les porte est remplacé au premier changement de langue ;
#   · un tableau LOCAL meurt à la fin de la composition, donc avant le dessin.
#
# Un tampon de l'OBJET, vidé et rempli à chaque image, n'a ni l'un ni l'autre
# défaut. C'est ce que font les onze listes du dépôt — toutes, sans exception, et
# c'est cette unanimité qui autorise la règle.
#
# ⚠ LA MARQUE EST LE SOULIGNÉ FINAL, la convention de membre du dépôt. Elle ne
# prouve pas la durée de vie ; elle refuse ce qui n'a manifestement pas la bonne.
CHOICE_LIST = re.compile(r"\.choices\s*=\s*([A-Za-z_][A-Za-z0-9_]*)")
CHOICE_CALL = re.compile(r"\bChoice\(")

# Le rang de la liste parmi les arguments de `Choice` : (option, libellé, aide,
# LISTE, choisi).
CHOICE_LIST_ARGUMENT = 3


def choice_call_lists(text: str):
    """Rend (ligne, identifiant) pour chaque liste passée à `Choice(...)`.

    🔴 IL FAUT LIRE L'APPEL, ET PAS SEULEMENT `.choices =`. La faute qui a fait
    planter le client passait sa liste par CET argument-là : une règle qui ne
    regarderait que l'affectation aurait laissé passer exactement le cas qui l'a
    motivée.

    Le découpage suit la PROFONDEUR de parenthèses : `Tr("…")` et
    `static_cast<int>(…)` en contiennent, et un découpage sur les virgules nues
    prendrait leur intérieur pour des arguments.
    """
    for found in CHOICE_CALL.finditer(text):
        start = found.end()
        depth = 1
        arguments = [""]
        rank = start
        while rank < len(text) and depth > 0:
            letter = text[rank]
            if letter in "([":
                depth += 1
            elif letter in ")]":
                depth -= 1
                if depth == 0:
                    break
            if letter == "," and depth == 1:
                arguments.append("")
            else:
                arguments[-1] += letter
            rank += 1
        if len(arguments) <= CHOICE_LIST_ARGUMENT:
            continue
        holder = arguments[CHOICE_LIST_ARGUMENT].strip()
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", holder):
            yield text.count("\n", 0, found.start()) + 1, holder


def rule_choice_list_is_a_member(path: Path, text: str) -> list[str]:
    """Les libellés d'une liste déroulante vivent dans un MEMBRE, jamais ailleurs.

    Un local meurt avant le dessin, un `static` survit au catalogue qui le
    porte. Les deux rendent une violation d'accès en lisant le libellé choisi,
    et celle-ci a coûté une session.
    """
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        found = CHOICE_LIST.search(line)
        if found is None:
            continue
        holder = found.group(1)
        # ⚠ UN SPAN REÇU EN PARAMÈTRE SE RETRANSMET LIBREMENT : il ne possède
        # rien, et c'est chez son APPELANT que la règle mord. L'assembleur de
        # rangées ne fait que passer le sien.
        forwarded = f"std::span<const char* const> {holder}" in text
        if not holder.endswith("_") and not forwarded:
            problems.append(
                f"{path}:{number}: la liste de choix `{holder}` n'est pas un membre — "
                "un local meurt avant le dessin"
            )
    for number, holder in choice_call_lists(text):
        forwarded = f"std::span<const char* const> {holder}" in text
        if not holder.endswith("_") and not forwarded:
            problems.append(
                f"{path}:{number}: la liste de choix `{holder}` n'est pas un membre — "
                "un local meurt avant le dessin"
            )
    return problems


# Une déclaration de VARIABLE `static` ou `constexpr` : elle porte un `=`, là où
# une FONCTION `static` n'en a pas. C'est ce qui sépare les deux sans analyser
# le C++.
TRANSLATED_HOLDER = re.compile(r"^\s*(?:const\s+|inline\s+)*(?:static|constexpr)\b[^=;]*=")


def rule_no_translated_static(path: Path, text: str) -> list[str]:
    """`Tr` ne se range pas dans un `static` : son pointeur meurt avec la langue.

    `ro/core/i18n.h` le dit à la ligne où il rend ce pointeur — « le consommer
    dans l'image est sûr ; le ranger dans un `static const char*` ne l'est pas ».
    Une liste gardée une fois pour toutes montre des libellés de l'ancienne
    langue, ou de la mémoire libérée.
    """
    problems = []
    lines = text.splitlines()
    for number, line in enumerate(lines, start=1):
        if TRANSLATED_HOLDER.match(line) is None:
            continue
        # La déclaration peut courir sur plusieurs lignes : on la suit jusqu'au
        # point-virgule, sans quoi un `Tr` renvoyé à la ligne passerait.
        statement = line
        rank = number
        while ";" not in statement and rank < len(lines):
            statement += lines[rank]
            rank += 1
        if "Tr(" in statement:
            problems.append(
                f"{path}:{number}: `Tr` rangé dans un `static` — son pointeur "
                "meurt au changement de langue"
            )
    return problems


def rule_writes_to_the_log(path: Path, text: str) -> list[str]:
    """Une trace passe par `ro::core::Log*`, jamais par la console.

    Le défaut ne se voit pas : la cible est bâtie en sous-système console, donc
    un `std::printf` s'affiche pendant qu'on développe et paraît fonctionner.
    Il manque au fichier que le joueur envoie, et c'est là qu'on le lit.
    """
    posix = path.as_posix()
    if not (posix.startswith("app/") or posix.startswith("libs/")):
        return []
    if posix in CONSOLE_FILES:
        return []
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        if CONSOLE_WRITES.search(line):
            problems.append(
                f"{path}:{number}: écrit sur la console — une trace passe par "
                f"`ro::core::Log*`, sans quoi elle manque au journal du joueur"
            )
    return problems


def rule_ui_stays_platform_free(path: Path, text: str) -> list[str]:
    """`ro_ui` ne connaît NI SDL NI OpenGL, et l'éditeur de liens ne peut pas le dire.

    Partout ailleurs la couche est une contrainte de build : `formats` ne peut
    pas inclure `sim`, parce que le chemin d'inclusion ne le contient pas.
    ImGui casse ce mécanisme — vcpkg n'en installe QU'UNE bibliothèque, dos
    compris, si bien qu'un fichier de `ro_ui` qui inclurait `imgui_impl_sdl3.h`
    compilerait et se lierait sans un mot. La frontière redeviendrait ce que
    CLAUDE.md refuse : une convention.

    Elle est donc vérifiée ici, mécaniquement, et c'est l'exception qui confirme
    la règle plutôt que de l'abroger. Ce qui est en jeu est concret : `ro_ui`
    est bâti et testé sur le runner Linux sous ASan, où SDL n'est même pas
    téléchargé. Un seul include de plateforme y met fin.
    """
    posix = path.as_posix()
    if not posix.startswith("libs/ui/") or posix.startswith(UI_BACKEND_DIRECTORY):
        return []
    problems = []
    for number, line in enumerate(text.splitlines(), start=1):
        if PLATFORM_INCLUDES.search(line):
            problems.append(
                f"{path}:{number}: `ro_ui` inclut la plateforme — "
                f"ce code appartient à {UI_BACKEND_DIRECTORY}"
            )
    return problems


TEXT_RULES = (
    rule_file_length,
    rule_backslash_comment,
    rule_memset_on_aggregate,
    rule_trailing_whitespace,
    rule_ui_stays_platform_free,
    rule_writes_to_the_log,
    rule_choice_list_is_a_member,
    rule_no_translated_static,
    rule_layer_includes,
    rule_no_native_provenance,
    rule_header_defines_inline,
    rule_generator_escapes_via_support,
    *hygiene_modules.TEXT_RULES,
)

# Ces trois-là guettent des pièges qui n'existent qu'en C++, et se tromperaient
# ailleurs : « // » est une division entière en Python, memset n'y existe pas, et
# un script ne fait pas d'include.
CXX_ONLY_RULES = (rule_backslash_comment, rule_memset_on_aggregate,
                  rule_ui_stays_platform_free, rule_writes_to_the_log,
                  rule_choice_list_is_a_member, rule_no_translated_static,
                  rule_layer_includes, rule_no_native_provenance,
                  rule_header_defines_inline)


def iter_sources() -> list[Path]:
    sources = []
    for directory in SCANNED_DIRECTORIES:
        base = REPO_ROOT / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                sources.append(path)
    return sources


def check_repository() -> list[str]:
    problems = []
    sources = iter_sources()
    if not sources:
        return ["aucune source trouvée — le contrôle ne mesurerait rien"]

    for path in sources:
        raw = path.read_bytes()
        text = raw.decode("utf-8")
        relative = path.relative_to(REPO_ROOT).as_posix()
        problems += rule_mixed_line_endings(Path(relative), raw)
        for rule in TEXT_RULES:
            if rule in CXX_ONLY_RULES and path.suffix not in CXX_SUFFIXES:
                continue
            problems += rule(Path(relative), text)
    problems += rule_every_family_case_is_routed(REPO_ROOT)
    problems += hygiene_modules.repository_problems(REPO_ROOT)
    print(f"{len(sources)} fichier(s) source contrôlé(s).")
    return problems


def self_test() -> list[str]:
    """Chaque règle reçoit un cas qu'elle DOIT rejeter, et un cas propre."""
    failures = []
    fake = Path("fictif.cpp")

    must_reject = [
        (rule_file_length, "x\n" * (MAX_LINES + 1), "fichier trop long"),
        (
            rule_backslash_comment,
            "// chemin doom\\.savegame\\\nint topdir = 0;\n",
            "commentaire terminé par un antislash",
        ),
        (
            rule_memset_on_aggregate,
            "std::memset(&recipe, 0, sizeof(recipe));\n",
            "memset sur un agrégat",
        ),
        (rule_trailing_whitespace, "int value = 0;   \n", "espaces en fin de ligne"),
    ]
    for rule, sample, label in must_reject:
        if not rule(fake, sample):
            failures.append(f"self-test : la règle « {label} » n'a RIEN attrapé")

    must_accept = [
        (rule_file_length, "x\n" * (MAX_LINES - 1)),
        (rule_backslash_comment, "#define JOIN(a, b) \\\n    a##b\n"),
        (rule_memset_on_aggregate, "std::memset(buffer.data(), 0, buffer.size());\n"),
        (rule_trailing_whitespace, "int value = 0;\n"),
    ]
    for rule, sample in must_accept:
        found = rule(fake, sample)
        if found:
            failures.append(f"self-test : faux positif — {found[0]}")

    # L'exemption du code généré doit être ÉTROITE : elle lève le plafond de
    # lignes, et rien d'autre. Une exemption qui déborderait laisserait passer
    # dans le code généré ce qu'on refuse partout ailleurs.
    generated = Path("libs") / GENERATED_DIRECTORY / "table.cpp"
    if rule_file_length(generated, "x\n" * (MAX_LINES + 1)):
        failures.append("self-test : le plafond de lignes mord encore sur du code généré")
    if not rule_trailing_whitespace(generated, "int value = 0;   \n"):
        failures.append("self-test : les autres règles ne mordent plus sur du code généré")

    # Les fins de ligne se testent sur des octets, pas sur du texte.
    if not rule_mixed_line_endings(fake, b"a\r\nb\nc\r\n"):
        failures.append("self-test : la règle « fins de ligne mixtes » n'a rien attrapé")
    if rule_mixed_line_endings(fake, b"a\nb\nc\n"):
        failures.append("self-test : faux positif sur du LF pur")
    if rule_mixed_line_endings(fake, b"a\r\nb\r\n"):
        failures.append("self-test : faux positif sur du CRLF pur")

    # LA DISPENSE DE CATALOGUE : elle lève le plafond de FICHIER...
    catalogue = Path(next(iter(CATALOGUE_FILES)))
    liste = "bool f() {\n  return true;\n}\n" * MAX_LINES
    if rule_file_length(catalogue, liste):
        failures.append("self-test : le plafond de fichier mord encore sur un catalogue")
    # ... et elle RETOMBE dès qu'une entrée enfle, sans quoi elle serait un
    # passe-droit et non une mesure.
    enflee = "bool f() {\n" + "  int x = 0;\n" * MAX_CATALOGUE_ENTRY_LINES + "}\n"
    if not rule_file_length(catalogue, enflee):
        failures.append("self-test : le plafond d'entrée de catalogue ne mord pas")
    if not rule_trailing_whitespace(catalogue, "int value = 0;   \n"):
        failures.append("self-test : les autres règles ne mordent plus sur un catalogue")

    # LE MÊME MARCHÉ POUR UN CATALOGUE PYTHON, où l'entrée est l'ARTICLE d'une
    # table. Un délimiteur C++ y rendrait la table entière, et les deux plafonds
    # tomberaient ensemble : celui du fichier levé, celui de l'entrée
    # inatteignable.
    # Le catalogue éprouvé est FICTIF, inscrit le temps de l'essai : un dépôt
    # sans catalogue Python doit éprouver la mesure aussi.
    script = Path("scripts/catalogue_fictif.py")
    CATALOGUE_FILES[script.as_posix()] = "un catalogue fictif, le temps de l'auto-test"
    try:
        table = "TABLE = {\n" + '    "clef": 1,\n' * MAX_LINES + "}\n"
        if rule_file_length(script, table):
            failures.append("self-test : le plafond de fichier mord encore sur un catalogue Python")
        gonflee = ("TABLE = {\n" + "    # une adresse\n" * MAX_CATALOGUE_ENTRY_LINES
                   + '    "clef": 1,\n}\n')
        if not rule_file_length(script, gonflee):
            failures.append("self-test : l'article enflé d'un catalogue Python passe encore")
        # ⚠ ET CE QUI N'EST PAS UNE TABLE NE DOIT PAS ÉCHAPPER À LA MESURE : sans
        # cela, une fonction de mille lignes entrerait dans un fichier dispensé.
        fonction = "def f():\n" + "    x = 0\n" * MAX_CATALOGUE_ENTRY_LINES
        if not rule_file_length(script, fonction):
            failures.append("self-test : une fonction enflée passe dans un catalogue Python")
    finally:
        del CATALOGUE_FILES[script.as_posix()]
    # ⚠ ET UN CHEMIN DISPENSÉ QUI N'EXISTE PLUS est une exemption qui ne protège
    # rien et que personne ne relit — le fichier a été renommé, la ligne est
    # restée. La liste se vérifie par le code, comme toute liste tenue à la main.
    for chemin in CATALOGUE_FILES:
        if not (REPO_ROOT / chemin).is_file():
            failures.append(f"self-test : CATALOGUE_FILES dispense {chemin}, qui n'existe pas")

    # LA FRONTIÈRE DE `ro_ui`, qu'aucun éditeur de liens ne porte : elle doit
    # mordre dans la couche, et PAS sur son dos, sans quoi elle interdirait à
    # celui-ci d'exister. Les deux cas se testent nommément parce que c'est le
    # CHEMIN qui décide, et non le contenu.
    dans_ui = Path("libs/ui/src/fenetre.cpp")
    dos_ui = Path(UI_BACKEND_DIRECTORY + "ui_backend_gl.cpp")
    inclusion = '#include "imgui_impl_sdl3.h"\n#include <SDL3/SDL.h>\n'
    if not rule_ui_stays_platform_free(dans_ui, inclusion):
        failures.append("self-test : `ro_ui` peut inclure la plateforme sans que rien ne le dise")
    if rule_ui_stays_platform_free(dos_ui, inclusion):
        failures.append("self-test : la règle interdit au DOS d'ImGui d'inclure la plateforme")
    if rule_ui_stays_platform_free(dans_ui, '#include "ro/render/camera.h"\n'):
        failures.append("self-test : faux positif — `ro_ui` a le droit d'inclure `render`")
    # Et elle ne doit pas déborder hors de `libs/ui` : `app` et le renderer
    # OpenGL incluent SDL légitimement.
    if rule_ui_stays_platform_free(Path("app/game/main.cpp"), inclusion):
        failures.append("self-test : la règle de `ro_ui` mord hors de `libs/ui`")

    # CE QUI S'ÉCRIT HORS DU JOURNAL. Elle doit mordre dans `app` et `libs`, se
    # taire sur `tools` et sur le texte d'usage, et ne JAMAIS confondre
    # `snprintf` — qui formate dans un tampon — avec une écriture.
    dans_app = Path("app/world/click_action.cpp")
    ecriture = 'std::fprintf(stderr, "demande refusée : %s\\n", texte);\n'
    if not rule_writes_to_the_log(dans_app, ecriture):
        failures.append("self-test : une écriture hors du journal passe encore")
    if not rule_writes_to_the_log(Path("libs/net/src/session.cpp"), "std::cerr << x;\n"):
        failures.append("self-test : un flux de la console passe encore dans `libs`")
    if rule_writes_to_the_log(Path("tools/roc_cli/main.cpp"), ecriture):
        failures.append("self-test : la règle mord sur `tools`, qui EST une ligne de commande")
    if rule_writes_to_the_log(Path(CONSOLE_FILES[0]), ecriture):
        failures.append("self-test : la règle mord sur le texte d'usage, qui en est dispensé")
    if rule_writes_to_the_log(dans_app, 'std::snprintf(ligne.data(), ligne.size(), "@x");\n'):
        failures.append("self-test : faux positif — `snprintf` formate, il n'écrit pas")
    if rule_writes_to_the_log(dans_app, "ro::core::LogWarning(\"x\");\n"):
        failures.append("self-test : faux positif — le journal est justement la bonne voie")
    # ⚠ ET UN CHEMIN DISPENSÉ QUI N'EXISTE PLUS ne protège rien : le fichier a
    # été renommé, la ligne est restée.
    for chemin in CONSOLE_FILES:
        if not (REPO_ROOT / chemin).is_file():
            failures.append(f"self-test : CONSOLE_FILES dispense {chemin}, qui n'existe pas")

    # LES DEUX RÈGLES PAYÉES PAR UN PLANTAGE. Chacune doit mordre sur la faute
    # EXACTE qui a été écrite, et se taire sur ce que le dépôt fait partout.
    if not rule_choice_list_is_a_member(fake, "  .choices = kShapeChoices,\n"):
        failures.append("self-test : une liste de choix locale passe encore")
    if rule_choice_list_is_a_member(fake, "  .choices = font_names_});\n"):
        failures.append("self-test : faux positif — un membre est le bon porteur")
    # ⚠ ET PAR L'ARGUMENT DE `Choice`, qui est la forme par laquelle la faute est
    # arrivée : une règle aveugle à celle-ci aurait raté ce qui l'a motivée.
    appel = ('  rows_.push_back(Choice(GameOption::kX, Tr("Texte"), nullptr,\n'
             '                         kBarTextChoices, static_cast<int>(mode_)));\n')
    if not rule_choice_list_is_a_member(fake, appel):
        failures.append("self-test : une liste locale passée à `Choice` passe encore")
    propre = ('  rows_.push_back(Choice(GameOption::kX, Tr("Texte"), nullptr,\n'
              '                         text_names_, static_cast<int>(mode_)));\n')
    if rule_choice_list_is_a_member(fake, propre):
        failures.append("self-test : faux positif sur un membre passé à `Choice`")

    relais = ("OptionRowView Choice(std::span<const char* const> choices) {\n"
              "  return OptionRowView{.choices = choices};\n}\n")
    if rule_choice_list_is_a_member(fake, relais):
        failures.append("self-test : faux positif — un span de paramètre se retransmet")

    statique = (
        '  static const std::array<const char*, 2> kNames = {Tr("Rien"),\n'
        '                                                    Tr("Les deux")};\n'
    )
    if not rule_no_translated_static(fake, statique):
        failures.append("self-test : `Tr` dans un `static` passe encore")
    # ⚠ ET LA RÈGLE NE DOIT PAS MORDRE SUR UNE FONCTION `static` qui rend une
    # traduction : celle-là est parfaitement licite, et c'est la forme que prend
    # tout aiguillage de libellés.
    fonction = '  static const char* Name(int i) {\n    return Tr("Vie");\n  }\n'
    if rule_no_translated_static(fake, fonction):
        failures.append("self-test : faux positif — une fonction `static` peut traduire")
    if rule_no_translated_static(fake, '  names_.push_back(Tr("Vie"));\n'):
        failures.append("self-test : faux positif sur un remplissage de membre")

    # LE GRAPHE DES COUCHES : `ui` ne voit `vfs` que par transitivité, et c'est
    # exactement ce que la règle refuse ; `render`, lui, le lie directement.
    if not rule_layer_includes(Path("libs/ui/src/fictif.cpp"), '#include "ro/vfs/vfs.h"\n'):
        failures.append("self-test : une inclusion transitive entre couches passe encore")
    if rule_layer_includes(Path("libs/render/src/fictif.cpp"), '#include "ro/vfs/vfs.h"\n'):
        failures.append("self-test : faux positif — `render` lie `vfs` directement")
    if "render" not in layer_dependencies().get("ui", frozenset()):
        failures.append("self-test : le graphe des couches ne se lit plus dans les CMakeLists")

    # LA PROVENANCE : ses cas vivent avec la règle, formes et liste blanche
    # ensemble.
    failures += self_test_native_provenance()
    failures += self_test_header_definitions()
    failures += self_test_generator_escapes()
    failures += self_test_family_routing()
    failures += hygiene_modules.self_test()

    # Un script Python ne doit pas échapper au plafond de lignes, ni recevoir
    # les règles qui n'ont de sens qu'en C++.
    if not rule_file_length(Path("outil.py"), "x\n" * (MAX_LINES + 1)):
        failures.append("self-test : le plafond de lignes ne mord pas sur un script")
    if ".py" not in SOURCE_SUFFIXES:
        failures.append("self-test : les scripts ne sont pas contrôlés")
    if rule_backslash_comment not in CXX_ONLY_RULES:
        failures.append("self-test : une règle C++ s'appliquerait aux scripts")

    if not failures:
        print(f"self-test : les {len(TEXT_RULES) + 1} règles mordent "
              "et ne produisent pas de faux positif.")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="vérifie que les règles rejettent bien ce qu'elles doivent")
    arguments = parser.parse_args()

    problems = self_test() if arguments.self_test else check_repository()
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"\n{len(problems)} problème(s).", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
