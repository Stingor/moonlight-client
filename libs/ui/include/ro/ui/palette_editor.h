#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/palette_rest.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// L'ÉDITEUR DE STYLE : le joueur compose ses couleurs, puis les valide.
//
// 🔴 LA FENÊTRE N'ÉCRIT PAS LA RECETTE. Elle la reçoit dans sa vue et rend des
// INTENTIONS — telle pièce vers telle couleur, telle coupe, page blanche —
// qu'`app` applique : la recette n'a qu'un écrivain, et la fenêtre ne peut pas
// la mettre dans un état que l'éditeur ne connaît pas.
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
// ⚠ IL NE PORTE QUE LE CALQUE QU'ON RÈGLE : le corps seul sur l'onglet du
// corps — un chapeau volumineux rétrécirait le personnage tout en masquant les
// pièces qu'on cherche justement à voir —, la coiffe cadrée sur la tête, la
// cape vue de dos, la peau sur le visage et les bras ensemble.
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
  // Un clic sur le pantin à cette image : la pipette choisit la pièce et
  // ouvre son nuancier.
  bool clicked = false;

  // ── AVANT / APRÈS ───────────────────────────────────────────────────────
  //
  // Tant que le bouton est tenu, le pantin montre ce que le personnage PORTE,
  // et non ce qu'on compose.
  bool before = false;

  // ── LA POSE ─────────────────────────────────────────────────────────────
  //
  // Le rang de la pose dans `kPaletteDollPoses`, et si elle défile.
  int pose = 0;
  bool animated = false;
  // Le fond du pantin, clair ou sombre : une couleur se juge sur les deux.
  bool light_background = false;
};

// LES POSES QUE LE PANTIN PREND : repos, marche, assis, attaque — rangs du
// .act d'un joueur.
inline constexpr std::array<std::size_t, 4> kPaletteDollPoses = {0, 1, 2, 10};

// ── LES ONGLETS DE CALQUE ────────────────────────────────────────────────────
//
// 🔴 UN ONGLET PAR CALQUE PORTÉ, ET SEULEMENT PORTÉ : un chapeau qu'on n'a pas
// sur la tête n'a rien à montrer, et son onglet serait une case vide. L'onglet
// d'un objet porte le nom de l'objet, pas celui de l'emplacement : c'est ce
// chapeau-là qu'on teint, et la teinture lui appartient.
//
// ⚠ LES NUMÉROS VOYAGENT : le styliste ouvre l'éditeur sur un onglet par son
// numéro, voir `ro/sim/style.h`. On en ajoute en queue, jamais au milieu.
enum class PaletteTabKind : std::uint8_t {
  kBody = 0,
  kSkin = 1,
  kHair = 2,
  kHeadgearTop = 3,
  kHeadgearMiddle = 4,
  kHeadgearLow = 5,
  kGarment = 6,
};
inline constexpr std::size_t kPaletteTabKinds = 7;

// POURQUOI UN CALQUE NE SE RÈGLE PAS, dit dans son onglet grisé.
enum class PaletteTabBlock : std::uint8_t {
  kNone,
  // Le profil n'y trouve aucune pièce assez grande.
  kNoPiece,
  // L'habit du GM garde les couleurs de ses sprites.
  kGmOutfit,
  // La tête d'orc est un visage d'effet, sans palette de cheveux.
  kOrcHead,
  // Sur cette monture, la peau peint aussi l'animal.
  kSkinOnMount,
  // L'apparence affichée ne correspond à aucun objet de l'équipement.
  kNotFound,
};

struct PaletteTab {
  PaletteTabKind kind = PaletteTabKind::kBody;
  // Le nom de l'objet porté, prêt à l'affichage ; vide pour le corps, la peau
  // et les cheveux, que la fenêtre nomme elle-même.
  std::string name;
  PaletteTabBlock blocked = PaletteTabBlock::kNone;
  // La part du calque qu'un réglage atteint, en pour-cent, ou moins un.
  int coverage_percent = -1;
  // Une retouche attend d'être validée.
  bool edited = false;
  // Le calque porte une teinture, validée.
  bool dyed = false;
  // UN OBJET : la teinture appartient à l'exemplaire équipé.
  bool item = false;
};

// LA RÉPONSE DE LA PIPETTE, toutes pièces confondues : l'onglet du calque sous
// le curseur, et la pièce de cet onglet — moins un hors de toute pièce.
struct PalettePipette {
  bool found = false;
  PaletteTabKind tab = PaletteTabKind::kBody;
  int piece = -1;
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

// CE QUE LE SERVEUR A FAIT DE LA DERNIÈRE DEMANDE, d'après son ÉCHO.
//
// 🔴 « ENVOYÉ » NE DISAIT RIEN : le serveur jette sans un mot ce qu'il refuse,
// et seul le retour de notre propre style dit qu'il est partagé. `app` attend
// cet écho, et la fenêtre dit ce qu'il en est.
enum class PaletteSendStatus : std::uint8_t {
  kNone,
  // Parti, l'écho n'est pas encore revenu.
  kWaiting,
  // L'écho porte ce qu'on a demandé.
  kShared,
  // Pas d'écho dans le délai, ou rien n'a pu partir.
  kRefused,
};

// CE QUE LA FENÊTRE MONTRE, posé par `app` à chaque image.
struct PaletteEditorView {
  // LA RECETTE EN COURS, dont `app` est le seul écrivain. Les réglages de
  // chaque pièce, les trois numéros choisis : la fenêtre les LIT ici.
  render::PaletteRecipe recipe;
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
  // Sans liaison, ni valider ni effacer ne peut rien : les boutons se grisent
  // plutôt que de laisser croire que le serveur a reçu quelque chose.
  bool connected = false;
  // La recette diffère-t-elle de ce que le personnage PORTE ? C'est ce qui
  // donne son sens à « Revenir à mon style ».
  bool has_edits = false;
  // Ce que le serveur a fait de la dernière demande.
  PaletteSendStatus send = PaletteSendStatus::kNone;
  // « Valider » changerait-il quelque chose ? Grisé sinon.
  bool has_something_to_send = false;
  // Une demande peut-elle partir ? Non tant que la précédente attend, ni dans
  // la seconde qui la suit : le serveur la jetterait.
  bool can_send = false;
  // Valider ce corps évincerait-il le plus ancien ? On prévient avant.
  bool evicts_oldest = false;
  // Ce corps est-il déjà le style principal ?
  bool is_main = false;
  // L'historique a-t-il un pas à défaire, à refaire ?
  bool can_undo = false;
  bool can_redo = false;

  // ── LES PIÈCES ──────────────────────────────────────────────────────────

  // La couleur D'ORIGINE de chaque pièce, sur la base : c'est d'elle que la
  // pièce tire son nom, qui ne change pas quand on la repeint.
  std::span<const ImU32> base_colors;
  // Le lien de chaque pièce : zéro pour une pièce libre, le même numéro pour
  // des pièces liées — régler l'une règle les autres.
  std::span<const std::uint8_t> links;
  // Les pièces que l'inspiration ne touche pas, une par bit.
  std::uint32_t locked = 0;
  // LES CORPS QU'ON PEUT RÉGLER D'ICI, sans en changer : le corps porté en
  // tête, puis ses variantes — à pied, monté, en tenue. Des clés du
  // catalogue, en français. Vide ou seul : rien à choisir.
  std::span<const char* const> bodies;
  // Le rang de celui qu'on règle.
  int edited_body = 0;

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
  // LA PIÈCE QUE LA PIPETTE DÉSIGNE dans l'onglet ouvert, ou moins un — la
  // réponse de `app` au point relevé à l'image précédente. Voir
  // `PaletteDollState`.
  int pipette_ramp = -1;
  // CE QU'ELLE DÉSIGNE TOUS CALQUES CONFONDUS : un clic sur un chapeau ouvre
  // l'onglet du chapeau, et la pièce sous le curseur.
  PalettePipette pipette;

  // ── LES CALQUES ─────────────────────────────────────────────────────────

  // Les onglets des calques portés, le corps en tête.
  std::span<const PaletteTab> tabs;
  // L'ONGLET DONT CETTE VUE EST FAITE. ⚠ PAS FORCÉMENT `state.tab` : un clic
  // sur la barre change celui-ci pendant le dessin, et la vue ne suivra qu'à
  // l'image suivante. Ce que la fenêtre montre, et ce que `app` fait des
  // intentions, se rapportent à celui-là.
  PaletteTabKind tab = PaletteTabKind::kBody;
  // LES RÉGLAGES DES PIÈCES DE L'ONGLET OUVERT, quand ce n'est pas le corps :
  // un par pièce, dans l'ordre de `ramps`. Vide sur le corps, dont les
  // réglages sont ceux de `recipe`.
  //
  // ⚠ UN CHAPEAU PEUT EN AVOIR TREIZE : sans pièce assez grande, ou en couleurs
  // vraies, il se règle par CLASSES DE TEINTE — les rouges, les oranges… — et
  // non par pièces.
  std::span<const render::RampAdjust> adjusts;
  // Les pièces de l'onglet ouvert sont des classes de teinte.
  bool hue_classes = false;
  // LE RESTE DU PROFIL MIXTE : la première rangée de `ramps` qui désigne une
  // classe de teinte du reste plutôt qu'une pièce, ou -1. Les rangées qui
  // suivent se nomment par leur classe, sous l'intitulé « Le reste ».
  int rest_first = -1;
  // LES RETOUCHES EXACTES de l'onglet ouvert, permises ou non (la peau, une
  // image en couleurs vraies, un serveur qui ne les connaît pas), au plus
  // `retouches_most`.
  bool retouches_allowed = false;
  int retouches_most = 0;
  std::span<const render::PaletteRetouch> retouches;
  // La palette PEINTE du calque ouvert, index par index : la couleur que la
  // retouche part de. Vide sans retouche permise.
  std::span<const ImU32> palette_colors;
  // L'index de palette sous le curseur sur le pantin, dans le calque ouvert, ou
  // -1 : celui qu'une retouche viserait.
  int pick_index = -1;
  // Ce que le dernier code collé n'a pas apporté, ou rien.
  const char* code_note = nullptr;
  // L'ATELIER DU STYLISTE EST-IL OUVERT ? Une teinture d'objet ne se valide
  // que là ; hors de l'atelier, l'onglet d'un objet n'est qu'un aperçu.
  bool atelier_open = false;
  // Rien d'autre à valider que des teintures d'objets, hors de l'atelier :
  // c'est ce que « Valider » grisé doit dire.
  bool items_wait_atelier = false;
  // Une tête teinte d'une autre coupe existe : sa teinture peut s'adapter à
  // la coupe portée.
  bool can_adapt_cut = false;
  // LES TEINTS PROPOSÉS sur l'onglet Peau, du plus clair au plus foncé.
  std::span<const ImU32> skin_tones;
  // Ce que le serveur a fait des teintures de calques envoyées, et la raison
  // d'un refus, déjà traduite, ou nulle.
  PaletteSendStatus layers_send = PaletteSendStatus::kNone;
  const char* layers_refusal = nullptr;
  // Le code de l'onglet ouvert peut-il partir dans le chat ? Seul le style du
  // corps a une balise que les autres clients lisent.
  bool code_shares_in_chat = true;

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
  // Renommer le préréglage `preset` en `renamed`.
  kRenamePreset,

  // ── LES AUTRES GESTES ───────────────────────────────────────────────────

  // Valider, et faire de ce corps le style principal.
  kMakeMain,
  // Défaire, refaire le dernier pas.
  kUndo,
  kRedo,
  // Tirer des couleurs en harmonie sur les pièces non verrouillées.
  kInspire,
};

// CE QU'UN PRÉRÉGLAGE APPORTE QUAND ON LE CHARGE.
enum class PresetPart : std::uint8_t {
  // Tout : pièces, teinte de base, coupe et couleur de cheveux.
  kAll,
  // Les pièces et la teinte de base, sans toucher aux cheveux.
  kColors,
  // La coupe et la couleur de cheveux seules.
  kHair,
};

// L'HARMONIE QUE L'INSPIRATION TIRE.
enum class PaletteHarmony : std::uint8_t { kAnalogous, kComplementary, kTriadic };

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
  // LE TRAIT auquel ce choix appartient : tous les choix d'un même glissement
  // ne font qu'un pas d'historique. Zéro : un pas à lui seul.
  std::uint32_t stroke = 0;
  // LE RÉGLAGE DIRECT d'une pièce, par le mode avancé : une rotation de
  // teinte relative, ou absolue. Moins un quand rien n'est réglé.
  int adjusted_ramp = -1;
  render::RampAdjust adjust;
  // LIER deux pièces, ou délier une pièce ; moins un sinon.
  int link_from = -1;
  int link_to = -1;
  int unlink = -1;
  // La pièce dont le verrou d'inspiration bascule, ou moins un.
  int toggle_lock = -1;
  PaletteHarmony harmony = PaletteHarmony::kAnalogous;
  // Ce que le chargement d'un préréglage apporte, et le nouveau nom d'un
  // préréglage renommé.
  PresetPart preset_part = PresetPart::kAll;
  std::string renamed;
  // Le corps à régler, en rang : zéro pour le corps porté. Moins un : rien.
  int edit_body = -1;
  // LA PIÈCE DONT LA CROIX VIENT D'ÊTRE CLIQUÉE — elle revient à neutre —, ou
  // moins un.
  int reset_ramp = -1;
  // LE NUMÉRO QU'UNE GRILLE VIENT DE FAIRE CHOISIR, et laquelle ; aucune quand
  // rien n'est choisi.
  PaletteEditorPicker chosen_in = PaletteEditorPicker::kNone;
  int chosen = 0;
  // Le préréglage que le verbe désigne, quand il en désigne un.
  std::string preset;
  // Ce que le presse-papiers portait, pour `kPasteCode`. ⚠ `ui` LE LIT MAIS NE
  // LE COMPREND PAS : décoder un style demande `sim`, qu'elle ne voit pas.
  std::string pasted;

  // ── LES CALQUES ─────────────────────────────────────────────────────────

  // Une seule couleur pour toutes les pièces des cheveux, en un geste : elles
  // se lient, et prennent la couleur de la première.
  bool one_hair_color = false;
  // Reprendre la teinture d'une autre coupe sur la coupe portée, pièce par
  // pièce, appariées par teinte.
  bool adapt_cut = false;
  // Retirer la teinture de l'objet de l'onglet ouvert.
  bool clear_item_dye = false;
  // Le teint proposé que le joueur vient de choisir, en rang, ou moins un.
  int skin_tone = -1;
  // UNE RETOUCHE : l'index visé et sa couleur exacte ; -1 sans retouche. Et
  // l'index dont on retire la retouche.
  int retouch_index = -1;
  std::array<std::uint8_t, 3> retouch_color{};
  int remove_retouch = -1;
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
  // ⛔ PAS DE RECETTE ICI : elle vient de la vue, et la fenêtre rend des
  // intentions. Cet état ne garde que ce que la fenêtre relève ou retient
  // d'une image à l'autre.
  PaletteDollState doll;
  PalettePickerState picker;
  // La pièce que la souris survole, ou moins un.
  //
  // ⚠ UN RELEVÉ QUE `app` LIT pour faire PULSER cette pièce sur le pantin. Le
  // pantin est dessiné AVANT la liste — il est à gauche —, donc il porte celle
  // de l'image PRÉCÉDENTE. Sur une pulsation qui bat deux fois par seconde, un
  // retard d'une image ne se voit pas.
  int hovered_ramp = -1;
  // Le préréglage que la souris survole, ou moins un : `app` le montre sur le
  // pantin, le temps du survol.
  int hovered_preset = -1;
  // LE TRAIT EN COURS : il avance à chaque appui du bouton gauche. Tous les
  // choix faits bouton tenu appartiennent au même trait.
  std::uint32_t stroke = 0;
  // La pièce dont la pipette demande d'ouvrir le nuancier, ou moins un.
  int open_piece = -1;
  // Le mode avancé du nuancier est-il déplié ?
  bool advanced = false;

  // L'ONGLET OUVERT. ⚠ `app` LE LIT pour composer la vue de cet onglet, et
  // retombe sur le corps quand le calque n'est plus porté.
  PaletteTabKind tab = PaletteTabKind::kBody;
  // Un onglet à ouvrir de force à la prochaine image — la pipette, le
  // styliste —, puis remis à faux : un clic du joueur sur la barre l'emporte
  // ensuite.
  bool tab_asked = false;
  // LE MODE « RETOUCHE EXACTE » : un clic sur le pantin vise une couleur de la
  // palette plutôt qu'une pièce. Et l'index dont le nuancier est ouvert, ou -1.
  bool retouch_mode = false;
  int retouch_index = -1;
  bool retouch_opening = false;

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

// LE NOM D'UNE COULEUR, tel qu'une pièce le porte : « rouge », « bleu »,
// « gris »…, DÉJÀ TRADUIT dans la langue du joueur.
[[nodiscard]] const char* PieceColorName(ImU32 color);

inline constexpr const char* kPaletteEditorTitle = core::TrText("Couleurs du personnage");
inline constexpr const char* kPaletteEditorId = "style";

}  // namespace ro::ui
