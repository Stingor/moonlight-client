"""Le contrôle de format, le MÊME des deux côtés.

POURQUOI CE FICHIER EXISTE. L'étape d'intégration était écrite ainsi :

    find app libs tools tests -type f \\( -name '*.h' ... \\) -print0
      | xargs -0 clang-format --dry-run --Werror

`find` et `xargs` n'existent pas sur un poste Windows. Le contrôle qui juge le
dépôt n'était donc pas à la portée de qui y travaille : on n'apprenait son
verdict qu'après le push, par un courriel. Il est resté rouge deux cent
soixante-et-onze commits d'affilée pour cette seule raison.

Un contrôle qu'on ne peut pas rejouer chez soi n'est pas un contrôle, c'est une
sanction. Celui-ci est du Python — l'intégration l'appelle, le crochet de push
l'appelle, et vous l'appelez :

    python scripts/check_format.py            tout l'arbre
    python scripts/check_format.py --corriger  formate au lieu de refuser
    python scripts/check_format.py a.cpp b.h   seulement ces fichiers

Il rend 0 si tout est conforme, 1 sinon.
"""
import io
import os
import re
import subprocess
import sys
import tempfile

from clang_format_pin import DEPOT, FormatIndisponible, verifier_clang_format

# Les racines que l'intégration parcourt, et les extensions qu'elle retient.
RACINES = ("app", "libs", "tools", "tests")
EXTENSIONS = (".h", ".hpp", ".cpp", ".cc")

# clang-format reçoit les fichiers par paquets : une ligne de commande Windows
# plafonne à 32 768 caractères, et l'arbre en compte neuf cent trente-neuf.
PAR_PAQUET = 200

# « chemin:ligne:colonne: error: … » — le chemin va jusqu'au dernier deux-points
# qui précède le couple de nombres, et non jusqu'au premier rencontré.
ANNONCE_D_ECART = re.compile(r"^(.*?):\d+:\d+: error:")


def sources():
    """Tous les fichiers source de l'arbre, chemins relatifs au dépôt."""
    trouves = []
    for racine in RACINES:
        for dossier, _, fichiers in os.walk(os.path.join(DEPOT, racine)):
            for fichier in fichiers:
                if fichier.endswith(EXTENSIONS):
                    plein = os.path.join(dossier, fichier)
                    trouves.append(os.path.relpath(plein, DEPOT).replace(os.sep, "/"))
    return sorted(trouves)


def uniformise(chemin):
    """Un seul séparateur, pour que deux écritures du même fichier se joignent."""
    return str(chemin).replace(os.sep, "/")


def non_conformes(exe, chemins):
    """Les fichiers que clang-format réécrirait, s'il en est."""
    fautifs = []
    for debut in range(0, len(chemins), PAR_PAQUET):
        paquet = [uniformise(c) for c in chemins[debut : debut + PAR_PAQUET]]
        dit = subprocess.run(
            [exe, "--dry-run", "--Werror"] + paquet,
            cwd=DEPOT,
            capture_output=True,
            check=False,
        )
        if dit.returncode == 0:
            continue

        # clang-format annonce « chemin:ligne:colonne: error: ... » par écart ;
        # un fichier en produit plusieurs, on ne le nomme qu'une fois.
        #
        # ⚠ On ne coupe PAS au premier deux-points : un chemin Windows absolu
        # commence par « C: », et le fichier s'appellerait alors « C ».
        plainte = dit.stderr.decode(errors="replace")
        nommes = []
        for ligne in plainte.splitlines():
            trouve = ANNONCE_D_ECART.match(ligne)
            if not trouve:
                continue
            chemin = uniformise(trouve.group(1))
            if chemin in paquet and chemin not in nommes:
                nommes.append(chemin)

        # ⚠ CLANG-FORMAT A DIT NON, ON NE REND PAS « RIEN À SIGNALER ». S'il
        # refuse sans qu'aucune de ses lignes ne se laisse rattacher à un
        # fichier du paquet, le silence ferait passer le contrôle pour vert —
        # la panne exacte qu'il est là pour empêcher. On rend le paquet, et on
        # recopie sa plainte.
        if not nommes:
            sys.stderr.write(plainte)
            nommes = paquet

        fautifs.extend(c for c in nommes if c not in fautifs)
    return fautifs


def auto_test(exe):
    """Le contrôle REJETTE-t-il ce qu'il doit rejeter ?

    Un contrôle qui passe toujours ne prouve rien — et celui-ci a remplacé une
    étape d'intégration que personne ne pouvait rejouer. On lui fait donc lire
    un fichier tordu et un fichier droit, et on exige les deux verdicts.
    """
    essais = os.path.join(tempfile.mkdtemp(prefix="check_format_"), "essai.cpp")
    ennuis = []

    # Deux instructions sur une ligne : aucune configuration de clang-format
    # ne laisse passer cela, quelle que soit la largeur de colonne.
    io.open(essais, "w", encoding="utf-8", newline="\n").write(
        "int principal() { int a=1;int b=2; return a+b; }\n"
    )
    if not non_conformes(exe, [essais]):
        ennuis.append("un fichier tordu a été déclaré conforme")

    subprocess.run([exe, "-i", essais], capture_output=True, check=False)
    if non_conformes(exe, [essais]):
        ennuis.append("un fichier que clang-format vient d'écrire est dit fautif")

    os.remove(essais)
    os.rmdir(os.path.dirname(essais))

    if ennuis:
        for ennui in ennuis:
            sys.stderr.write("auto-test : " + ennui + "\n")
        return 1
    print("auto-test : le contrôle mord, et ne crie pas sans raison.")
    return 0


def main(arguments):
    corriger = "--corriger" in arguments
    demandes = [a for a in arguments if not a.startswith("--")]

    try:
        exe = verifier_clang_format()
    except FormatIndisponible as souci:
        sys.stderr.write(str(souci) + "\n")
        return 1

    if "--self-test" in arguments:
        return auto_test(exe)

    chemins = demandes or sources()
    chemins = [c.replace(os.sep, "/") for c in chemins]
    if not chemins:
        print("aucun fichier source à contrôler.")
        return 0

    fautifs = non_conformes(exe, chemins)
    if not fautifs:
        print("%d fichier(s) source au format." % len(chemins))
        return 0

    if corriger:
        for debut in range(0, len(fautifs), PAR_PAQUET):
            subprocess.run(
                [exe, "-i"] + fautifs[debut : debut + PAR_PAQUET],
                cwd=DEPOT,
                check=False,
            )
        print("%d fichier(s) formaté(s) :" % len(fautifs))
        for chemin in fautifs:
            print("  " + chemin)
        return 0

    sys.stderr.write("%d fichier(s) hors format :\n" % len(fautifs))
    for chemin in fautifs:
        sys.stderr.write("  " + chemin + "\n")
    sys.stderr.write("\nPour les remettre au format :\n")
    sys.stderr.write("    python scripts/check_format.py --corriger\n")
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
