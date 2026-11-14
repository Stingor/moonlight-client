"""Un identifiant C++ est en anglais.

CLAUDE.md le veut : commentaires en français, identifiants en anglais. La
relecture ne le voit pas — un `joueur` se lit aussi bien qu'un `player` pour
qui parle français, et c'est précisément pourquoi il en est entré des
centaines.

La règle découpe chaque identifiant en mots (`snake_case`, `CamelCase`,
`kConstante`) et refuse ceux que nomme `FRENCH_WORDS`, un dictionnaire de mots
français écrits sans accents. Les commentaires, les chaînes et les chaînes
brutes ne sont pas lus : la prose y reste française.

Le dictionnaire vise ce qui est français DANS CE DÉPÔT, pas toute collision
possible avec l'anglais : `entree` y est, parce qu'ici il s'écrit à la place
d'`entry`. En sont exclus les mots que le code emploie en anglais (`vit`, la
statistique ; `axe`, l'arme ; `retire`, le verbe).

Les fichiers qui en portent encore sont nommés dans
`french_identifier_exemptions.txt`. Cette liste ne fait que RÉTRÉCIR : un
fichier qui n'y est pas ne peut pas en recevoir, et un fichier qui y est sans
plus en porter est une erreur — la dispense se retire avec le dernier mot.
"""

from __future__ import annotations

import re
from pathlib import Path

CXX_SUFFIXES = (".h", ".hpp", ".cpp", ".cc")

EXEMPTIONS_FILE = Path(__file__).resolve().parent / "french_identifier_exemptions.txt"

FRENCH_WORDS = frozenset("""
absentes acteur adresse ailes ailleurs aire alea alleges ame ancre anneau
anneaux annules appel apport apres arme attendu attente autre avance avancee
avancees avancer avant avec azimut balayage banc bande bandes bas blanc bleu
boite bord bords bornes boucle boule boules bourdon bout bras bruit butin cadre
camoufle carre cas cercle chemin chemins cible cle clef coin coins colonne
combien commencer competence composante composee compte compteur connue contre
corps couche couches couleur coup coups coupure courant couronne courte cran
croix dans debut decalage decale degres deja demande demandee demandes demi
depart deployee depuis dernier derniere derriere descente deux diviseur doit
dossier droit droite ecart echelle eclat ecran efface effet effets encore
entendu entier entree essai etat eteinte etincelle etincelles etroit eventail
exterieur famille fantome fenetre feuillage fiche fichier fixe flaque fleche
forcee frappeur fumee gauche generateur gerbe geste graine graines grille gris
haut hauteur histoire horloge ici identifiant inclinaison inconnu interieur
jetons joue jouer joueur jupe lancer lanceur largeur lente les libre ligne
lignes liste loin longueur luciole mains marche marque melange meneuse metier
milieu monde montants monte montee montre montres mot muette muettes mur
naissance neige neuf noeud noeuds nom nuee nulle numero onde ordinaire ordre
oreille origine ouverte paire parcouru partenaire particule parure pas patron
patrons peindre pente periode petit piste pistes plafond plante plein pleine
pointe portail porte portee porteur posee poser pousse precedent premier
premiere presentes proche profondeur propre proprietaire proximite quand quatre
qui quoi racine rangee rapide rapport rayon rayons rebond recette relu rendu
reste retard roue rouge roulis ruban seconde seme semees semer semis serree seul
seule sien sienne siens signe soin sommet sons sorte soucis souffle souleve
sous suivant suivi sur taille tard teinte temoin tenu tenue tete texte tic tics
tirage tire tirer tireur tourne tous tout toutes trainee tremble trois troupe
trouve une unite usine valeur valeurs variante veille venin vide vie vif vise
vite vitesse vivant vive vives voiles voisin voulu vue vues vus
""".split())

# Un mot d'identifiant : une capitale suivie de minuscules, des minuscules, ou
# un sigle en capitales. `kFichierMs` donne k, fichier, ms.
WORD = re.compile(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])")
IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")
RAW_STRING_OPENING = re.compile(r'R"([^()\\\s]{0,16})\(')


def segments(text: str):
    """Découpe le texte en tranches `(début, fin, sorte)` jointives.

    La sorte vaut `code`, ou nomme ce qui n'en est pas : `raw` (chaîne brute),
    `line_comment`, `block_comment`, `digit_separator` (l'apostrophe de
    `1'000`), `string` (chaîne ou caractère) et `directive` (ligne de
    préprocesseur). C'est le découpage unique que lisent `code_only` et
    `code_mask`.
    """
    at = 0
    size = len(text)
    code_start = 0

    def cut(start: int, end: int, kind: str):
        return (start, min(end, size), kind)

    while at < size:
        c = text[at]
        start = at
        raw = RAW_STRING_OPENING.match(text, at) if c == "R" else None
        if raw and (at == 0 or not (text[at - 1].isalnum() or text[at - 1] == "_")):
            closing = ")" + raw.group(1) + '"'
            end = text.find(closing, raw.end())
            at = size if end < 0 else end + len(closing)
            kind = "raw"
        elif text.startswith("//", at):
            end = text.find("\n", at)
            at = size if end < 0 else end
            kind = "line_comment"
        elif text.startswith("/*", at):
            end = text.find("*/", at + 2)
            at = size if end < 0 else end + 2
            kind = "block_comment"
        elif c == "'" and at > 0 and text[at - 1].isalnum() and _in_number(text, at):
            at += 1
            kind = "digit_separator"
        elif c in "\"'":
            at += 1
            while at < size and text[at] != c and text[at] != "\n":
                at += 2 if text[at] == "\\" else 1
            at += 1
            kind = "string"
        elif c == "#" and not text[text.rfind("\n", 0, at) + 1:at].strip():
            while at < size and text[at] != "\n":
                at += 2 if text.startswith("\\\n", at) else 1
            kind = "directive"
        else:
            at += 1
            continue
        if code_start < start:
            yield cut(code_start, start, "code")
        yield cut(start, at, kind)
        code_start = min(at, size)
    if code_start < size:
        yield cut(code_start, size, "code")


def code_only(text: str) -> str:
    """Le texte sans commentaires, chaînes, caractères ni directives."""
    out = []
    for start, end, kind in segments(text):
        if kind == "code":
            out.append(text[start:end])
        elif kind in ("raw", "block_comment"):
            out.append(" " + "\n" * text.count("\n", start, end))
        elif kind == "string":
            out.append(" ")
    return "".join(out)


def code_mask(text: str) -> str:
    """Le texte à la même longueur, tout ce qui n'est pas du code blanchi.

    Chaque caractère hors du code devient une espace, sauf les sauts de ligne
    qui restent : une position du masque est la position du texte.
    """
    out = []
    for start, end, kind in segments(text):
        piece = text[start:end]
        out.append(piece if kind == "code" else re.sub(r"[^\n]", " ", piece))
    return "".join(out)


def _in_number(text: str, at: int) -> bool:
    """L'apostrophe en `at` sépare-t-elle les chiffres d'un littéral numérique ?"""
    start = at
    while start > 0 and (text[start - 1].isalnum() or text[start - 1] in "'."):
        start -= 1
    return text[start].isdigit()


# Les mots anglais du jeu qu'un mot du dictionnaire compose : la race
# « Demi-Human » de Ragnarok.
ENGLISH_COMPOUNDS = re.compile(r"demi_?human", re.IGNORECASE)


def french_words_of(identifier: str) -> list[str]:
    identifier = ENGLISH_COMPOUNDS.sub("_", identifier)
    return [w for w in (m.lower() for m in WORD.findall(identifier)) if w in FRENCH_WORDS]


def french_identifiers(text: str) -> dict[str, int]:
    """Chaque identifiant français du code, avec la ligne de sa première apparition."""
    found: dict[str, int] = {}
    for number, line in enumerate(code_only(text).splitlines(), start=1):
        for identifier in IDENTIFIER.findall(line):
            if identifier not in found and french_words_of(identifier):
                found[identifier] = number
    return found


def read_exemptions(path: Path = EXEMPTIONS_FILE) -> frozenset[str]:
    if not path.is_file():
        return frozenset()
    entries = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            entries.add(line)
    return frozenset(entries)


EXEMPTIONS = read_exemptions()


def rule_identifiers_are_english(path: Path, text: str,
                                 exemptions: frozenset[str] = EXEMPTIONS) -> list[str]:
    """Aucun identifiant français hors des fichiers encore dispensés."""
    if path.suffix not in CXX_SUFFIXES or path.as_posix() in exemptions:
        return []
    return [
        f"{path}:{number}: identifiant français `{identifier}` "
        f"({', '.join(french_words_of(identifier))}) — les identifiants sont en anglais"
        for identifier, number in french_identifiers(text).items()
    ]


def rule_exemptions_still_needed(root: Path,
                                 exemptions: frozenset[str] = EXEMPTIONS) -> list[str]:
    """Une dispense nomme un fichier qui existe et porte encore un identifiant français."""
    problems = []
    for entry in sorted(exemptions):
        path = root / entry
        if not path.is_file():
            problems.append(f"{EXEMPTIONS_FILE.name}: `{entry}` n'existe plus — "
                            "retirer sa dispense")
        elif not french_identifiers(path.read_text(encoding="utf-8")):
            problems.append(f"{EXEMPTIONS_FILE.name}: `{entry}` n'a plus d'identifiant "
                            "français — retirer sa dispense")
    return problems


def self_test_french_identifiers() -> list[str]:
    """La règle mord sur un identifiant français, et se tait sur la prose et l'anglais."""
    failures = []
    fake = Path("libs/core/src/fictif.cpp")
    none: frozenset[str] = frozenset()
    must_reject = [
        ("int joueur = 0;\n", "une locale"),
        ("struct Entry { int hauteur_px; };\n", "un champ en snake_case"),
        ("constexpr int kPremierTic = 3;\n", "une constante en kCamelCase"),
        ("void TirerGraine();\n", "une fonction en CamelCase"),
        ("for (const auto& entree : table) {}\n", "« entree » pour « entry »"),
        ("int value = 1'000; int noeud = 0;\n", "un identifiant après un séparateur de chiffres"),
    ]
    for text, label in must_reject:
        if not rule_identifiers_are_english(fake, text, none):
            failures.append(f"self-test : {label} française passe encore")
    must_accept = [
        ("// le joueur tire une graine\nint player = 0;\n", "un commentaire"),
        ('const char* kText = "la hauteur du joueur";\n', "une chaîne"),
        ('const char* kLua = R"(joueur = { hauteur = 2 })";\n', "une chaîne brute"),
        ('const char* kLua = R"lua(f("x") -- noeud)lua";\n', "une chaîne brute délimitée"),
        ("/* entree\n   noeud */ int entry = 0;\n", "un commentaire de bloc"),
        ("int vit = player.vit;\nint axe_class = 0;\nvoid Retire(int key);\n",
         "le jargon et l'anglais exclus"),
        ("constexpr int kDemiHuman = 7;\nint demi_human_bonus = 0;\n", "la race Demi-Human"),
        ("#include \"ro/ui/joueur.h\"\n", "une directive"),
    ]
    for text, label in must_accept:
        found = rule_identifiers_are_english(fake, text, none)
        if found:
            failures.append(f"self-test : faux positif sur {label} — {found[0]}")
    if rule_identifiers_are_english(fake, "int joueur = 0;\n", frozenset({fake.as_posix()})):
        failures.append("self-test : un fichier dispensé est encore contrôlé")
    if rule_identifiers_are_english(Path("scripts/outil.py"), "joueur = 0\n", none):
        failures.append("self-test : la règle des identifiants mord sur un script")
    return failures
