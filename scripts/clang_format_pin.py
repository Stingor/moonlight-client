"""La version de clang-format que le dépôt épingle, et sa vérification.

Deux versions de clang-format coupent les lignes différemment : employer celle
qui traîne sur la machine fait échouer le contrôle sur un arbre que personne
n'a touché. `scripts/format_version.txt` nomme la version, et
`verifier_clang_format()` refuse net toute autre, en donnant la commande qui
installe la bonne.
"""
import io
import os
import re
import subprocess

ICI = os.path.dirname(os.path.abspath(__file__))
DEPOT = os.path.dirname(ICI)
FICHIER_VERSION = os.path.join(ICI, "format_version.txt")


# Le message que voit quelqu'un dont la version diverge. Il porte la commande
# exacte : une erreur qui ne dit pas comment en sortir fait perdre le temps
# qu'elle prétend économiser.
REMEDE = (
    'python -m pip install "clang-format==%s"\n'
    "(la version est épinglée par scripts/format_version.txt, et l'intégration\n"
    " emploie exactement celle-là)"
)


class FormatIndisponible(RuntimeError):
    """clang-format manque, ou n'est pas la version que le dépôt épingle."""


def version_epinglee():
    """La version que le dépôt exige, telle que l'intégration la lit."""
    return io.open(FICHIER_VERSION, encoding="utf-8").read().strip()


def verifier_clang_format():
    """Rend le nom de l'exécutable, après avoir vérifié qu'il est le bon.

    Lève FormatIndisponible si clang-format manque ou diverge de l'épingle.
    """
    attendue = version_epinglee()
    try:
        dit = subprocess.run(
            ["clang-format", "--version"], capture_output=True, check=False
        )
    except OSError:
        raise FormatIndisponible(
            "clang-format est introuvable.\n" + REMEDE % attendue
        )
    if dit.returncode != 0:
        raise FormatIndisponible(
            "clang-format ne répond pas à --version.\n" + REMEDE % attendue
        )

    # « clang-format version 23.1.0 », parfois précédé du nom d'une distribution
    # (« Ubuntu clang-format version 18.1.3 (1ubuntu1) ») ou suivi d'une
    # parenthèse de build (« (https://github.com/llvm/…) »). ⚠ LE NUMÉRO SE
    # CHERCHE APRÈS « version », et non en dernier mot : le dernier mot est
    # justement cette parenthèse, et un binaire LLVM officiel à la bonne version
    # était refusé.
    annonce = dit.stdout.decode(errors="replace").strip()
    lu = re.search(r"version\s+(\d+(?:\.\d+)*)", annonce)
    trouvee = lu.group(1) if lu else ""
    if trouvee != attendue and not trouvee.startswith(attendue + "."):
        raise FormatIndisponible(
            "clang-format %s est installé, le dépôt épingle %s.\n%s"
            % (trouvee or annonce, attendue, REMEDE % attendue)
        )
    return "clang-format"
