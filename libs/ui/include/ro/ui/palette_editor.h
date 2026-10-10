#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/palette_ramps.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// L'ÉDITEUR DE STYLE : le joueur compose ses couleurs, puis les valide.
//
// 🔴 RIEN NE TOUCHE AU PERSONNAGE AVANT LA VALIDATION. L'aperçu vit dans le
// PANTIN de cette fenêtre, et là seulement : chaque geste ne fait que
// recalculer une palette pour lui. L'acteur en scène, la fiche et les autres
// joueurs ne voient que ce qui a été validé.
//
// Pousser chaque réglage dans le rendu ferait qu'ouvrir la fenêtre change déjà
// l'apparence, qu'un « annuler » laisse des restes à moitié posés, et qu'il
// faut défaire à la fermeture ce qu'on a posé à l'ouverture. Le pantin rend
// l'essai GRATUIT.
//
// ⚠ ON NE RÈGLE PAS 256 COULEURS, mais les PIÈCES du costume — les dégradés
// que `DetectRamps` découpe. Et pas au curseur : une PASTILLE par pièce, un
// nuancier, et `AdjustToReach` derrière. Pas de curseurs de teinte : ils
// doubleraient une commande déjà présente et demanderaient de sélectionner une
// pièce avant d'agir.

// Le pantin d'aperçu : ce que `app` y pose, et ce qu'il en repart.
//
// ⚠ NI COIFFE NI CAPE NI ARME, et c'est un choix : on règle la palette du
// CORPS, et un chapeau volumineux rétrécirait le personnage tout en masquant
// les pièces qu'on cherche justement à voir.
struct PaletteDollState {
  // La silhouette composée, posée par `app`.
  SkinImage image;
  // La boîte, mesurée ICI et lue par `app` à l'image suivante : elle dépend de
  // la hauteur de la liste des pièces, qu'on ne connaît qu'au dessin.
  float box_width = 0.0F;
  float box_height = 0.0F;
  // La direction du .ACT, que la molette tourne. Huit crans, sans exception :
  // la pose est celle du repos, qui les tient toutes.
  std::uint8_t direction = 0;
  // Le portail qui empêche de faire tourner le pantin en parcourant la
  // fenêtre. Voir `ro/ui/wheel_gate.h`.
  WheelGateState wheel;

  // ── LA PIPETTE : OÙ LE CURSEUR EST SUR LE PANTIN ────────────────────────
  //
  // 🔴 C'EST LA RÉCIPROQUE DE LA PULSATION. Celle-ci répond à « où agit cette
  // ligne ? » ; la pipette à « quelle ligne commande cette manche ? » — et
  // c'est la SECONDE question que le joueur se pose vraiment.
  //
  // ⚠ `ui` NE RÉPOND PAS : elle relève le point, en pixels de la BOÎTE du
  // pantin, coin haut-gauche à l'origine. La réponse demande le sprite et ses
  // index par pixel, qui n'appartiennent pas à cette couche ; `app` la donne à
  // l'image suivante dans `PaletteEditorView::pipette_ramp`.
  bool hovered = false;
  float hover_x = 0.0F;
  float hover_y = 0.0F;
};

// ── LES TROIS GRILLES DE VIGNETTES ──────────────────────────────────────────
//
// 🔴 DES VIGNETTES, ET PAS DES CURSEURS, et c'est l'écran de création de
// personnage qui tranche : « coiffure 47 » ne veut rien dire, on choisit une
// coupe en la VOYANT. Un curseur obligeait à parcourir quatre-vingts valeurs
// une par une pour découvrir ce qu'elles sont.
//
// ⚠ ET EN SURGISSANT, pas dans la fenêtre : trois grilles de 80, 251 et 553
// cases y tiendraient mal, et l'éditeur doit rester lisible quand on ne
// choisit rien. Le libellé du bouton porte le choix courant — c'est ce qui
// remplace la valeur qu'affichait le curseur.
enum class PaletteEditorPicker : std::uint8_t { kNone, kHairStyle, kHairColor, kBodyPalette };

// CE QUE LE SERVEUR ACCEPTE — voir `ro/sim/style.h`, où ces bornes sont lues
// dans sa configuration.
//
// ⚠ ELLES VIENNENT DE `app`, ET NON D'ICI : c'est le serveur qui les fixe, et
// une fenêtre qui les redéclarerait en ferait une seconde vérité — celle qui
// proposerait au joueur une coiffure que le serveur rabote sans le dire.
struct PaletteChoiceBounds {
  // Le premier numéro proposé. ⚠ UN, JAMAIS ZÉRO : zéro n'est pas une palette
  // mais son ABSENCE, auquel cas le jeu garde celle du sprite. Le proposer
  // offrirait une teinte que personne ne voit.
  int first = 0;
  int hair_style = 0;
  int hair_palette = 0;
  int body_palette = 0;

  [[nodiscard]] bool usable() const noexcept { return first > 0; }
};

// CE QUE LA GRILLE DEMANDE À `app`, écrit ICI pendant le dessin et lu à
// l'image SUIVANTE.
//
// ⚠ UNE IMAGE DE RETARD, ASSUMÉE, et c'est la même mécanique que la boîte du
// pantin : on ne sait quelles cases sont visibles qu'en les parcourant, donc
// après que `app` a eu l'occasion de composer. Une case ouverte reste vide le
// temps d'une image, ce qui ne se voit pas — la composition, elle, en demande
// plusieurs de toute façon.
struct PalettePickerState {
  PaletteEditorPicker open = PaletteEditorPicker::kNone;
  // La page de la grille des teintes de base. ⚠ ELLE SEULE EST PAGINÉE : voir
  // `DrawBodyPalettePicker` — chaque vignette y est une TEXTURE distincte, et
  // cinq cent cinquante-trois d'un coup videraient le cache en pleine image.
  int page = 0;
  // La tranche de numéros que la grille montre vraiment, hors de ce que
  // l'ascenseur a fait sortir de vue.
  int first = 0;
  int count = 0;
  // Le côté d'une case, en pixels d'écran : c'est la taille à laquelle `app`
  // doit composer.
  float cell = 0.0F;
};

// CE QUE LA FENÊTRE MONTRE, posé par `app` à chaque image.
struct PaletteEditorView {
  // Les pièces du corps porté. Vide quand il n'y a rien à régler — et la
  // fenêtre le DIT, plutôt que d'aligner des commandes qui ne commandent rien.
  std::span<const render::PaletteRamp> ramps;
  // La pastille de chaque pièce, dans le même ordre : sa couleur une fois la
  // recette appliquée. ⚠ CE QUE LA FENÊTRE MONTRE EST DONC CE QUE LE JOUEUR
  // VERRA SUR LUI.
  std::span<const ImU32> colors;
  // La clarté maximale de chaque pièce, de 0 à 255 — voir `RampValueCeiling`.
  // C'est elle qui fait écrire « (ombre) » : une pièce peinte sombre ne peut
  // pas être éclaircie, et sans le dire la commande a l'air cassée.
  std::span<const int> ceilings;
  // La part du corps qu'un réglage atteint, en pour-cent ; moins un tant
  // qu'on ne l'a pas mesurée.
  //
  // 🔴 ON LE DIT, PARCE QUE LE JOUEUR SE LE DEMANDE. Au-delà des huit
  // plus grands dégradés, le reste du corps n'a pas de commande et n'en aura
  // pas ; le taire laisse le joueur chercher indéfiniment la pièce qui
  // commande une zone que rien ne commande.
  int coverage_percent = -1;
  // ⛔ PAS DE « SPRITE LU SUR LE PERSONNAGE » CONTRE « DÉDUIT DE LA CLASSE ». Ce
  // client n'a qu'une résolution — la nôtre, celle qui dessine —, et une ligne
  // qui dirait toujours la même chose ne serait que du bruit.
  // Ce corps a-t-il son propre style, et combien le joueur en a-t-il en tout ?
  bool body_has_own_style = false;
  int body_style_count = 0;
  // Sans liaison, valider ne peut rien : le bouton se grise plutôt que de
  // laisser croire que le serveur a reçu quelque chose.
  bool connected = false;
  // La recette diffère-t-elle de ce que le personnage PORTE ? C'est ce qui
  // donne son sens à « Revenir à mon style ».
  bool has_edits = false;
  // Le paquet vient de partir. ⚠ CELA NE DIT PAS QUE LE SERVEUR A ACCEPTÉ —
  // il n'accuse rien — seulement que la demande est partie.
  bool just_sent = false;

  // ── CE QUE LES GRILLES MONTRENT ─────────────────────────────────────────

  // Les vignettes de la grille ouverte, celle du numéro `thumbnails_first` en
  // tête. Une image inutilisable est une case VIDE, et c'est une réponse
  // juste : tous les numéros n'ont pas de sprite.
  std::span<const SkinImage> thumbnails;
  int thumbnails_first = 0;
  PaletteChoiceBounds bounds;
  // Ce que le personnage porte VRAIMENT, pour la case allumée de la grille des
  // coiffures quand la recette n'en impose aucune.
  int worn_hair_style = 0;
  // LA PIÈCE QUE LA PIPETTE DÉSIGNE, ou moins un — la réponse de `app` au
  // point relevé à l'image précédente. Voir `PaletteDollState`.
  int pipette_ramp = -1;

  // ── CE QUE LE JOUEUR A RANGÉ ────────────────────────────────────────────

  // Les préréglages, dans l'ordre du fichier.
  std::span<const std::string> presets;
  // Le plafond est atteint : « Enregistrer » ne peut plus ajouter, seulement
  // remplacer. On le grise plutôt que de laisser un clic sans effet.
  bool presets_full = false;
  // Y a-t-il un style composé et non validé à récupérer ?
  //
  // 🔴 QUAND IL Y EN A UN, LA SECTION S'OUVRE D'ELLE-MÊME, et une seule fois.
  // C'est le seul moment où quelqu'un a besoin de la trouver sans savoir
  // qu'elle existe : il vient de perdre une session, et un bouton de sauvetage
  // replié dans un menu ne sauve personne.
  bool has_draft = false;
  // Le code du style courant, prêt à copier. ⚠ COMPOSÉ PAR `app`, qui seul
  // sait l'écrire — et qui comble ce que la recette laisse ouvert, parce qu'un
  // code voyage SANS son porteur.
  const char* share_code = "";
  // Le dernier collage a été refusé, et on le dit quelques secondes : un
  // collage raté qui ne dit rien laisse croire à un défaut de l'éditeur.
  bool code_refused = false;
};

// CE QUE LE JOUEUR A DEMANDÉ.
//
// 🔴 TROIS RETOURS EN ARRIÈRE QUI NE SE RECOUVRENT PAS, et qu'il ne faut ni
// confondre ni réduire à deux. Ils répondent à trois questions distinctes :
//   « annule mes retouches »       → revenir à ce qu'on PORTE
//   « donne-moi une page blanche » → repartir de l'état CANONIQUE
//   « enlève tout ça »             → supprimer, serveur COMPRIS
enum class PaletteEditorVerb : std::uint8_t {
  kNothing,
  // Pousser la recette sur le personnage, et la partager.
  kApply,
  // Annuler les retouches en cours : revenir à ce que le personnage porte.
  kBackToWorn,
  // Page blanche : les pièces reviennent à leur état d'origine.
  kBlankSlate,
  // Effacer le style de CE corps. Les autres survivent.
  kForgetBody,
  // Tout effacer, chez nous ET sur le serveur.
  kDeleteAll,

  // ── LES PRÉRÉGLAGES ET LES CODES ────────────────────────────────────────
  //
  // ⚠ PUREMENT LOCAUX : le serveur n'en sait rien. Un préréglage n'est pas une
  // apparence, c'est un brouillon qu'on reprend.

  // Reprendre le dernier style composé sans être validé.
  kLoadDraft,
  // Ranger le style courant sous le nom saisi.
  kSavePreset,
  // Charger le préréglage que `preset` désigne, ou l'oublier.
  kLoadPreset,
  kDeletePreset,
  // Poser le code d'un préréglage dans la saisie du chat.
  //
  // ⚠ MAJ + CLIC, la convention de gestes du projet : la même que sur un objet
  // ou une compétence, et c'est justement ce qui la rend utile — le joueur n'a
  // rien de neuf à apprendre. Le geste s'arrête au DÉPÔT dans la barre : c'est
  // lui qui envoie.
  kSharePreset,
  // Mettre le code du style courant dans le presse-papiers.
  kCopyCode,
  // Le poser dans la saisie du chat.
  kShareCode,
  // Lire le code que `pasted` porte. ⚠ IL NE S'APPLIQUE À RIEN : un code vient
  // d'un autre joueur, et il ne doit pas pouvoir changer une apparence d'un
  // simple clic. Il faudra valider, comme pour tout le reste.
  kPasteCode,
};

struct PaletteEditorAction {
  PaletteEditorVerb verb = PaletteEditorVerb::kNothing;
  // « Signaler un bug » : `app` compose le contexte du style essayé.
  bool report_bug = false;
  // LA PIÈCE DONT LA COULEUR VIENT D'ÊTRE DÉSIGNÉE, et la couleur désignée ;
  // moins un quand personne n'a rien choisi.
  //
  // 🔴 `ui` NE TRADUIT PAS UNE COULEUR EN RÉGLAGE. Le calcul part de la
  // palette de BASE — jamais de celle qu'on voit, sans quoi les réglages
  // s'additionneraient à chaque clic et la couleur dériverait sous la main du
  // joueur — et cette base vit dans la garde-robe. C'est donc `app` qui
  // appelle `AdjustToReach` et écrit dans la recette.
  int picked_ramp = -1;
  std::array<std::uint8_t, 3> picked_color{};
  // Le préréglage que le verbe désigne, quand il en désigne un.
  std::string preset;
  // Ce que le presse-papiers portait, pour `kPasteCode`. ⚠ `ui` LE LIT MAIS NE
  // LE COMPREND PAS : décoder un style demande `sim`, qu'elle ne voit pas.
  std::string pasted;
};

// Ce que la fenêtre garde d'une image à l'autre.
// La place d'un nom de préréglage, en octets. ⚠ TRENTE-DEUX : de quoi
// nommer une allure — « soirée », « ma monture » — sans
// qu'une liste devienne une colonne de phrases.
inline constexpr std::size_t kPresetNameSize = 32;

struct PaletteEditorState {
  bool open = false;
  bool pinned = false;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
  // Le BROUILLON : ce que le joueur compose, et qui ne part qu'à la
  // validation.
  render::PaletteRecipe recipe;
  PaletteDollState doll;
  PalettePickerState picker;
  // La pièce que la souris survole, ou moins un.
  //
  // ⚠ UN RELEVÉ QUE `app` LIT pour faire PULSER cette pièce sur le pantin. Le
  // pantin est dessiné AVANT la liste — il est à gauche —, donc il porte celle
  // de l'image PRÉCÉDENTE. Sur une pulsation qui bat deux fois par seconde, un
  // retard d'une image ne se voit pas.
  int hovered_ramp = -1;

  // Le nom en cours de saisie pour un nouveau préréglage.
  std::array<char, kPresetNameSize> preset_name{};
  // L'identifiant ImGui du champ ci-dessus, relevé à l'image PRÉCÉDENTE.
  //
  // 🔴 ÉCRIRE DANS LE TAMPON NE SUFFIT PAS quand le champ a le focus : ImGui
  // travaille alors sur sa copie interne et la réécrit par-dessus la nôtre à
  // l'image suivante. Notre valeur n'aurait jamais été affichée — et surtout,
  // « Enregistrer » repartirait de l'ancien texte, créant un doublon au lieu
  // de mettre à jour. Il faut resynchroniser le widget par son état, d'où cet
  // identifiant, qui n'est connu qu'APRÈS sa soumission.
  unsigned int preset_field = 0;
};

[[nodiscard]] PaletteEditorAction DrawPaletteEditor(PaletteEditorState& state,
                                                    const PaletteEditorView& view,
                                                    const SkinArt& art);

// ── LA GÉOMÉTRIE D'UNE GRILLE ───────────────────────────────────────────────
//
// ⚠ EXPOSÉE POUR ÊTRE ÉPROUVÉE, et pas parce qu'on en aurait besoin ailleurs :
// la règle ci-dessous est de celles qui ne se signalent pas.

// Cette grille aura-t-elle un ascenseur ?
[[nodiscard]] bool ThumbnailGridScrolls(int total, int columns, int visible_rows);

// La largeur qu'il faut lui donner.
//
// 🔴 ELLE DOIT COMPTER L'ASCENSEUR quand la grille défile, sinon celui-ci mord
// sur la dernière colonne et rogne ses vignettes. RIEN NE LE SIGNALE : le
// contenu est simplement coupé à droite, et l'on croit à un défaut de sprite.
// La grille de couleurs de la création de personnage y est exposée de même.
//
// ⚠ DEMANDE UN CONTEXTE ImGui : elle lit l'espacement et la marge du style.
[[nodiscard]] float ThumbnailGridWidth(int columns, float cell, bool with_scrollbar);

// La hauteur qu'il faut lui donner, bornée à `visible_rows` lignes.
[[nodiscard]] float ThumbnailGridHeight(int total, int columns, float cell, int visible_rows);

inline constexpr const char* kPaletteEditorTitle = core::TrText("Style du personnage");
inline constexpr const char* kPaletteEditorId = "style";

}  // namespace ro::ui
