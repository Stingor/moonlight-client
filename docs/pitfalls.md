# Pièges connus

Des défauts que ce code a déjà payés, et la règle qui les évite. Chacun a coûté
au moins une bâtisse perdue, un essai qui ne prouvait rien, ou un défaut vu en
jeu avant de l'être en relecture.

## Compilateurs et éditeur de liens

### MSVC refuse un alias local dans le défaut d'un paramètre de lambda

`using Silhouette = …::BodySilhouette;` puis une lambda
`[](Silhouette s = Silhouette::kIgnored)` : MSVC répond C2653 et C2065 sur la
ligne de la lambda. C'est du C++ valide, et g++ le prend.

**Règle :** dans le défaut d'un paramètre de lambda, écrire le nom qualifié
complet. L'alias reste utilisable aux appels.

### MSVC ne voit pas ce que g++ refuse

Un chantier bâti sans un avertissement sous MSVC peut échouer sous Linux, où les
avertissements sont en erreur : conversion `int` → `float` (`-Wconversion`),
`int` → `size_t` (`-Wsign-conversion`), format de `snprintf` venu d'un paramètre
(`-Wformat-nonliteral`), `case` oublié dans un `switch` sur une énumération
(`-Wswitch`).

**Règle :** passer aussi le code sous g++ 14 (WSL suffit) avant de le croire
propre. En ajoutant une valeur à une énumération, chercher soi-même tous les
`switch` sur son type. Un format traduit se passe EN LITTÉRAL à `snprintf` :
une aide intermédiaire perd l'attribut `format_arg`.

### Le lien incrémental peut relier du périmé

Sous MSVC, le lien incrémental (fichier `.ilk`) peut rafistoler un exécutable
sans reprendre un objet tiré d'une bibliothèque statique modifiée. L'exécutable
porte une date neuve, et pourtant un essai saboté reste vert.

**Règle :** quand le verdict compte (prouver qu'un essai mord, passage final) et
que la modification ne touche qu'une bibliothèque statique, supprimer
l'exécutable d'essai et son `.ilk` avant de bâtir.

### Une bibliothèque qui se lie dans un test n'est pas autonome

L'éditeur de liens ne tire un objet d'une bibliothèque statique que si quelqu'un
en a besoin. Une bibliothèque qui appelle une fonction restée dans l'exécutable
se lie sans erreur dans un test… tant que le test ne tire pas cet objet-là.

**Règle :** en sortant des fichiers de `app/` vers une bibliothèque, chercher
pour chaque inclusion d'`app/` les fonctions NON inline réellement appelées.

### Un essai de tri stable veut plus de 32 éléments

Le `std::sort` de MSVC trie par insertion jusqu'à 32 éléments, ce qui est stable
en pratique, et un tableau tout égal ressort dans son ordre. Un essai de
stabilité sur 24 éléments passe donc avec `std::sort` à la place de
`std::stable_sort`.

**Règle :** prouver un tri stable sur plus de 32 éléments, avec au moins deux
clés MÊLÉES, puis vérifier que l'essai échoue en remplaçant `stable_sort` par
`sort`.

## ImGui

### Un libellé qui change veut `###`

Avec `##`, ImGui hache le libellé ENTIER : le texte affiché entre dans
l'identifiant. Un onglet `"Coiffe *##4"` devient un autre onglet dès que
l'étoile de retouche apparaît ; l'ancien disparaît, et la sélection saute
ailleurs.

**Règle :** tout libellé bâti avec un état variable (marque de retouche, nom,
compteur, texte traduit) s'écrit `texte + "###" + identifiant stable`. Un essai
de barre d'onglets se pilote sur plusieurs images : une sélection prend effet à
l'image suivante, un onglet non soumis n'est retiré qu'à celle d'après. Un
surgissant ouvert dans une liste qui change de propriétaire pousse ce
propriétaire par `PushID`.

### Une modale ouverte dans une fenêtre enfant ne paraît jamais

`OpenPopup(id)` nomme le surgissant dans la pile d'identifiants DU MOMENT.
Ouvert depuis un `BeginChild` ou une ligne de table, alors que la boîte se
dessine au niveau de la fenêtre, les deux identités diffèrent : la boîte ne
s'ouvre pas, et rien ne le dit.

**Règle :** dans un enfant, poser un drapeau dans l'état, et ouvrir la modale au
niveau de la fenêtre, à la même image, juste avant de la dessiner. Le prouver
par un essai qui CLIQUE jusqu'à l'action.

### Un surgissant hérite des couleurs de son parent

Un surgissant hérite des couleurs poussées là où il s'ouvre : ouvert depuis une
zone qui pose le texte en blanc, ses entrées deviennent illisibles.

**Règle :** un menu passe par `BeginSkinPopup` (`ro/ui/skin_popup.h`), qui pose
le texte de l'habillage dans le surgissant. `scripts/hygiene_popups.py` refuse
un `ImGui::BeginPopup` brut. Un défaut qui revient se corrige par une porte
unique et une règle mécanique, pas par un correctif de plus.

### Un banc de dessin peut être creux

`ImGui::Begin("essai")` sans taille ouvre une fenêtre minuscule : le texte est
coupé au bord de la FENÊTRE bien avant de sortir du widget, et un essai de
débordement passe quoi qu'il arrive. Un libellé centré qui déborde sort des
deux côtés, et à droite l'art du bouton noie la mesure.

**Règle :** ouvrir une fenêtre large (`SetNextWindowSize`), mesurer le bord
GAUCHE, et saboter la ligne surveillée pour voir l'essai échouer avant de le
croire.

### Un fond peint se découpe sur le contenu

Une image de fond entière, posée d'un bloc en haut de la fenêtre, met ses
bandes à une place fixe en pixels, pendant que les widgets suivent la hauteur
d'une ligne de texte (police, échelle, langue). Le cadre ne recouvre plus ce
qu'il encadre.

**Règle :** relever les bandes de l'image ligne par ligne, et dessiner le fond
en TRANCHES : celles qui portent un dessin gardent leur hauteur, les tranches
unies s'étirent. Le fond se dessine après le contenu, sur un second canal de la
liste d'affichage (`ChannelsSplit(2)`).

## Écrire et corriger

### Un porteur de pointeurs a une durée de vie exacte

Une rangée qui garde un `std::span` de libellés traduits ne les copie pas. Un
tableau `static` de `Tr("…")` pend dès que la langue change ; un tableau local
pend à chaque image. Le second semble corriger le premier, et il est pire.

**Règle :** énoncer l'INVARIANT avant de corriger (« le porteur vit de la
composition jusqu'au dessin, ni moins ni plus ») et regarder comment le dépôt le
fait déjà ailleurs. Corriger le symptôme lu ne suffit pas.

### Deux défauts qui se compensent se corrigent ensemble

Un défaut qui se voit peu peut être le reste d'une compensation. En corriger une
moitié peut supprimer l'effet au lieu de le réparer, ou le doubler.

**Règle :** confronter le RÉSULTAT entier à sa référence après chaque
correction, au lieu de relire seulement le code corrigé.

### Un `case` qui chute hérite du suivant

Un `case` qui tombe dans le suivant reçoit tout ce qu'on y ajoute plus tard : un
appel ajouté au cas d'en dessous s'exécute deux fois pour celui d'au-dessus. Les
essais de la fonction appelée passent ; seul l'ensemble montre le double appel.

**Règle :** avant d'ajouter une ligne à un `case`, remonter voir si un cas y
chute. Éprouver le câblage entier (une itération complète du système), pas
seulement la fonction.

### Un commentaire qui recopie une borne se périme

« La conversation garde soixante-quatre lignes » : la constante, définie
ailleurs, valait mille par genre. Le commentaire justifiait un coût, et il l'a
caché.

**Règle :** un commentaire qui s'appuie sur une borne définie ailleurs écrit le
NOM de la constante, qu'une recherche retrouve et qui ne ment pas.

### Zéro n'est pas une réponse « courte »

Une fonction qui parcourt une liste de familles et rend zéro pour celle qu'elle
oublie rend un nombre plausible. Trois appels plus loin, ce zéro devient une
cadence d'une image : un effet se relance soixante fois par seconde.

**Règle :** une absence se dit (`std::optional`, une valeur nommée), elle ne se
déguise pas en zéro. Un essai qui garde une liste parcourt TOUTE la table, pas
le seul cas qui a cassé.

### Une clé de traduction réemployée a déjà un sens

Le texte français est la clé du catalogue : un `Tr("…")` neuf dont le texte
existe déjà hérite de la traduction du premier usage, même si le sens diffère
(« Cartes » du monde traduit comme les cartes à jouer ; « Staff » traduit comme
un bâton). Le contrôle des traductions ne le voit pas, puisque la clé est
traduite.

**Règle :** avant d'engager des textes d'interface, relire les traductions des
clés qui existaient déjà, surtout celles d'un seul mot. En cas de doute,
distinguer le français.

## Protocole

### Un emplacement n'est pas un rang

Le serveur envoie la liste des personnages DENSE, chacun portant son
emplacement. L'indexer par l'emplacement marche tant que le compte n'a pas de
trou, c'est-à-dire sur tous les comptes d'essai. Après une suppression, le
client lirait la mauvaise ligne et entrerait sous un autre personnage.

**Règle :** une liste dense se cherche par le champ qui porte la place, jamais
par l'index. Et le nombre de places existantes vient d'un autre paquet que la
liste.

### Le serveur répond par un verdict, pas par un nouvel état

Créer, réserver une suppression, l'annuler, la confirmer : aucun de ces gestes
ne renvoie la liste. Un accusé peut même ne rien porter du tout.

**Règle :** retenir la cible de sa PROPRE demande pour appliquer le verdict. Et
un refus de ces gestes (nom pris, personnage en guilde) n'est pas un refus de
phase : la session continue, alors que les autres refus la terminent.

## Mesurer

### Les images par seconde d'une Debug ne jugent rien

Une Debug est des dizaines de fois plus lente qu'une Release sur le code du
rendu. Corriger d'après les images par seconde d'une Debug fait courir après des
fantômes.

**Règle :** une lenteur se mesure en Release, puis se confirme en Release.

### Un essai neuf se prouve par un sabotage

Un essai qui passe ne prouve rien tant qu'on ne l'a pas vu échouer.

**Règle :** saboter la ligne qu'il surveille, recompiler, vérifier qu'il échoue,
puis remettre. C'est la seule preuve qu'il mord.

### Des fins de ligne mélangées font échouer un remplacement

Avec `core.autocrlf`, deux fichiers voisins peuvent être l'un en CRLF, l'autre
en LF. Un script qui cherche un bloc écrit avec des `\n` réussit sur l'un et
échoue sur l'autre ; la panne ressemble à « le texte cherché n'existe pas ».

**Règle :** dans un script de remplacement, lire en binaire, normaliser les fins
de ligne pour chercher, et rendre les CRLF à l'écriture. Rendre le remplacement
idempotent.
