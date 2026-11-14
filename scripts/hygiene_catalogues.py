"""Les fichiers dispensés du plafond de lignes, et le plafond de leurs entrées.

`check_hygiene.py` les applique : voir `rule_file_length`.
"""

# LES CATALOGUES, et pourquoi chacun est dispensé du plafond de FICHIER.
#
# ⚠ CETTE DISPENSE SE MÉRITE, elle ne s'accorde pas. Le plafond existe contre
# le `OnRenderUI` de 1 702 lignes à onze niveaux d'accolades : un fichier qui
# RACONTE quelque chose et qu'on ne relit plus. Un catalogue ne raconte rien —
# c'est une liste d'entrées indépendantes, sans état partagé ni ordre de
# lecture, où l'on CHERCHE une entrée et où l'on n'en lit jamais deux.
#
# Le découper n'achète aucune lisibilité, et coûte un nom qui MENT : « la
# suite » ne désigne aucune nature, et rien ne dit plus de quel côté chercher.
# C'est le piège de l'ADR 0017 déplacé du sous-programme au fichier.
#
# LA CONTREPARTIE EST LA TAILLE DES ENTRÉES. La dispense porte sur leur NOMBRE,
# jamais sur leur longueur : un catalogue dont une entrée enfle redevient
# exactement ce que le plafond combat, et `MAX_CATALOGUE_ENTRY_LINES` le
# rattrape. C'est ce second plafond qui fait de l'exemption une mesure et non un
# passe-droit.
#
# LA LISTE EST NOMINATIVE, JAMAIS UN MOTIF : un chemin s'y ajoute par un geste
# délibéré, qui se relit en revue. Un motif dispenserait demain des fichiers que
# personne n'a examinés.
CATALOGUE_FILES = {
    "app/effects/effect_node_sow.cpp":
        "une méthode par famille d'effets, quarante et quelques, toutes de même "
        "forme et sans lien entre elles : chercher `PlayXxx` répond, lire le "
        "fichier de bout en bout n'apprend rien",
    "libs/proto/src/manual_packets.cpp":
        "un `Encode`/`Decode` par paquet, chacun bâti sur le même patron — "
        "recouper la disposition du serveur, vérifier la longueur, écrire les "
        "champs — et sans aucun lien avec son voisin : on y cherche LE paquet "
        "qu'on porte, jamais deux. Le découper par direction séparerait les "
        "deux moitiés d'une même conversation, et par ordre alphabétique "
        "donnerait deux noms qui mentent",
    "libs/render/include/ro/render/effect_track_motion.h":
        "une déclaration par SOUS-TYPE de mouvement, chacune avec ce qui la "
        "distingue de sa voisine : on y cherche le mouvement "
        "qu'on porte, jamais deux. Il a DÉJÀ été coupé une fois — le modèle "
        "d'une piste est parti dans `effect_track.h` et cinq familles dans "
        "`effect_track_burst.h` —, et ce qui reste ne se coupe plus sur une "
        "frontière : le partager par ordre alphabétique ou par taille donnerait "
        "deux noms qui mentent",
}

# Les catalogues des scripts que le dépôt public ne porte pas vivent avec eux,
# dans un module à part : sans ces scripts, il n'y a rien à dispenser. Si le
# module manquait à côté d'eux, leur longueur les ferait refuser.
try:
    from hygiene_script_catalogues import SCRIPT_CATALOGUE_FILES
except ModuleNotFoundError:
    SCRIPT_CATALOGUE_FILES = {}
CATALOGUE_FILES.update(SCRIPT_CATALOGUE_FILES)

# Ce qu'une entrée de catalogue ne doit pas dépasser. La plus longue des
# méthodes de `effect_node_sow.cpp` en fait trente-quatre : la marge est large,
# et c'est voulu — ce plafond-ci n'est pas là pour serrer le style mais pour
# attraper l'entrée qui a cessé d'en être une.
MAX_CATALOGUE_ENTRY_LINES = 80
