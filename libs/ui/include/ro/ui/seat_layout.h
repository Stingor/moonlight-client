#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/character_seats.h"

namespace ro::ui {

// LA MISE EN PAGE DE LA TABLÉE, telle que le joueur l'a composée : les sièges,
// leur pose, les blocs d'interface, le décor.
//
// 🔴 LE MODE « PERSONNALISER ». La table d'usine reste celle de
// `kBanquetSeats` ; celle-ci en est la version VIVANTE, que le joueur déplace
// et qu'`app` enregistre.
//
// ⚠ PORTÉE : LA MACHINE, PAS LE COMPTE. Une seule mise en page vaut pour toutes
// les connexions depuis ce client.
//
// ⚠ DES FRACTIONS D'ÉCRAN, comme la table d'usine : le décor est étiré plein
// cadre, donc une mise en page calée en 1920 × 1080 vaut telle quelle en
// 1280 × 720.

// LE POINT D'ANCRAGE NOMMÉ D'UN BLOC D'INTERFACE.
//
// ⚠ LE NOM EST UNE CLÉ DU FICHIER : c'est lui qui retrouve la position du
// titre à la lecture suivante.
struct SeatAnchor {
  std::string name;
  float x = 0.0F;
  float y = 0.0F;
};

// LES QUATRE BLOCS DÉPLAÇABLES. ⚠ GRAVÉS : le fichier de mise en page les
// porte sous ces noms-là.
inline constexpr std::string_view kTitleAnchor = "titre";
inline constexpr std::string_view kActionBarAnchor = "boutons";
inline constexpr std::string_view kExitBarAnchor = "sortie";
inline constexpr std::string_view kPageBarAnchor = "pages";

// Le registre des ancres est borné, et un nom aussi : un fichier bricolé ne
// doit pas faire grossir la table sans fin.
inline constexpr std::size_t kSeatAnchorsMax = 16;
inline constexpr std::size_t kSeatAnchorNameMax = 23;

struct SeatLayout {
  std::array<CharacterSeat, kSeatCount> seats = kBanquetSeats;
  // ⚠ VIDE À L'USINE : chaque bloc déclare sa position d'origine là où il se
  // dessine, et s'enregistre à sa première image. Un seul endroit connaît donc
  // la place d'usine du titre.
  std::vector<SeatAnchor> anchors;
  // Relatif à `kBackgroundRoot`. Vide vaut le décor d'usine.
  std::string background = std::string(kBanquetBackground);
  // LES PLACES LIBRES — le « + » qui invite à créer. Les masquer donne une
  // scène qui ne montre que les personnages ; l'opacité les garde discrètes
  // sans les perdre de vue.
  //
  // 🔴 MASQUÉES, ELLES RESTENT CLIQUABLES ET REPARAISSENT AU SURVOL : sans cela,
  // les masquer supprimerait le seul chemin de création de personnage. Et
  // pendant la personnalisation elles restent toujours visibles, sans quoi on
  // ne pourrait plus les placer.
  bool hide_free_seats = false;
  float free_seat_alpha = 1.0F;
};

// LES MISES EN PAGE ENREGISTRÉES : combien on en garde, et la place d'un nom,
// zéro final compris — quarante octets.
inline constexpr std::size_t kPresetsMax = 20;
inline constexpr std::size_t kPresetNameBytes = 41;

// ── LES BORNES DE LECTURE ────────────────────────────────────────────────────
//
// ⚠ GÉNÉREUSES À DESSEIN : un siège légèrement hors cadre est un choix de mise
// en page recevable — un personnage qui entre par le bord —, une coordonnée à
// quarante est un fichier corrompu.
inline constexpr float kSeatFractionLowest = -0.5F;
inline constexpr float kSeatFractionHighest = 1.5F;
inline constexpr float kSeatHeightLowest = 0.02F;
inline constexpr float kSeatHeightHighest = 0.60F;
// Au-delà d'une vingtaine d'actions, plus aucun sprite n'en porte autant.
inline constexpr int kSeatAnimationHighest = 20;
inline constexpr int kSeatFrameHighest = 63;
inline constexpr std::size_t kBackgroundPathMax = 120;

[[nodiscard]] bool ValidSeatFraction(float fraction) noexcept;
[[nodiscard]] bool ValidSeatHeight(float height) noexcept;

// UN CHEMIN DE DÉCOR ACCEPTABLE : ASCII imprimable, longueur raisonnable, ni
// remontée ni chemin absolu.
//
// ⚠ PAS UNE FRONTIÈRE DE SÉCURITÉ : le fichier est écrit par le joueur
// lui-même. Un garde-fou contre une valeur bricolée à la main qui ferait
// chercher n'importe quoi au VFS.
[[nodiscard]] bool ValidBackgroundPath(std::string_view path) noexcept;

// L'ANCRE `name`, enregistrée à la position donnée si elle n'existe pas encore.
// Rend nullptr quand le registre est plein : le bloc se pose alors à sa place
// d'usine, sans poignée.
[[nodiscard]] SeatAnchor* AnchorOf(SeatLayout& layout, std::string_view name, float default_x,
                                   float default_y);

// ── LES REMISES À L'USINE ────────────────────────────────────────────────────
//
// ⚠ `ResetSeatPlacement` NE TOUCHE PAS AU DÉCOR : on veut pouvoir repartir d'un
// placement propre SUR son propre fond. `ResetSeatLayout` restaure aussi le
// décor d'origine. Ni l'une ni l'autre ne touche aux places libres, qui sont
// un goût et non un placement.
void ResetSeatPlacement(SeatLayout& layout);
void ResetSeatLayout(SeatLayout& layout);

// La pose de `source` — action, animation, tête, orientation, image — à TOUTES
// les places. Replacer vingt-cinq sièges un par un après un changement de décor
// est le geste le plus pénible du mode.
void ApplyPoseEverywhere(SeatLayout& layout, const CharacterSeat& source);

// L'IMAGE QUE TIENT LE CONVIVE DE CE SIÈGE, ou -1 quand son action défile.
//
// ⚠ LE CORPS NE DÉFILE QU'HORS DU REPOS ET DE L'ASSISE, et seulement si le
// siège le demande : les images de ces deux actions sont des orientations du
// regard, pas un mouvement. L'image imposée l'emporte sur tout ; la tête ne
// se tourne que sur un corps immobile, puisque c'est la même image.
//
// 🔴 BORNÉE À `frame_count` : une classe dont le corps porte moins d'images
// que la tête ne rendrait sinon AUCUN calque, et le pantin disparaîtrait au
// lieu de simplement ne pas pencher la tête. Zéro image connue ne borne rien.
[[nodiscard]] int SeatHeldFrame(const CharacterSeat& seat, int frame_count) noexcept;

// LE CHEMIN DU DÉCOR POUR LE VFS : `kBackgroundRoot` + le décor choisi, ou
// celui d'usine si le réglage est vide.
[[nodiscard]] std::string BackgroundPath(const SeatLayout& layout);

// ── LE MENU DE POSE ──────────────────────────────────────────────────────────

struct SeatPoseChoice {
  int animation = kSeatAnimation;
  bool animated = false;
  const char* label = nullptr;
};

// ⚠ L'ORDRE EST CELUI DU MENU AFFICHÉ, et « animé » ne vaut que pour les
// actions que le composeur fait défiler : ailleurs, les images sont des poses.
inline constexpr std::array<SeatPoseChoice, 15> kSeatPoseChoices = {{
    {kSeatAnimSit, false, core::TrText("Assis")},
    {kSeatAnimStand, false, core::TrText("Debout")},
    {kSeatAnimFight, false, core::TrText("Combat")},
    {kSeatAnimFight, true, core::TrText("Combat (animé)")},
    {kSeatAnimWalk, true, core::TrText("Marche (animée)")},
    {kSeatAnimPick, false, core::TrText("Ramassage")},
    {kSeatAnimAttack, false, core::TrText("Attaque")},
    {kSeatAnimAttack, true, core::TrText("Attaque (animée)")},
    {kSeatAnimAttack2, true, core::TrText("Attaque 2 (animée)")},
    {kSeatAnimAttack3, true, core::TrText("Attaque 3 (animée)")},
    {kSeatAnimHurt, false, core::TrText("Touché")},
    {kSeatAnimFrozen, false, core::TrText("Gelé")},
    {kSeatAnimDie, false, core::TrText("Mort")},
    {kSeatAnimExtra1, false, core::TrText("Action 12")},
    {kSeatAnimExtra2, false, core::TrText("Action 13")},
}};

struct HeadTurnChoice {
  int head_turn = kHeadAlongBody;
  const char* label = nullptr;
};

// Les trois inclinaisons que porte un sprite de tête.
inline constexpr std::array<HeadTurnChoice, 3> kHeadTurnChoices = {{
    {0, core::TrText("Droit devant")},
    {1, core::TrText("Penchée à droite")},
    {kHeadTurnMost, core::TrText("Penchée à gauche")},
}};

// ── LA GALERIE DE DÉCORS ─────────────────────────────────────────────────────

// D'OÙ VIENT UN DÉCOR. ⚠ ÇA NE CHANGE RIEN AU CHOIX, mais ça explique pourquoi
// un décor apparaît alors que le joueur n'a rien déposé : il est livré dans une
// archive du serveur.
enum class BackgroundOrigin : std::uint8_t {
  kFactory,  // livré avec le client
  kPacked,   // trouvé dans une archive montée
  kDisk,     // déposé par le joueur
};

// Le libellé du décor d'usine, traduit à l'affichage.
inline constexpr const char* kFactoryBackgroundLabel = core::TrText("Banquet (d'origine)");

struct LayoutBackground {
  // Relatif à `kBackgroundRoot`, comme `SeatLayout::background`.
  std::string path;
  std::string label;
  BackgroundOrigin origin = BackgroundOrigin::kFactory;
};

// ── LES LIBELLÉS, traduits à la lecture ──────────────────────────────────────
//
// ⚠ LES TABLES CI-DESSUS SONT CONSTANTES : les traduire à leur définition les
// figerait en français. Elles se traduisent donc ici, au moment de les montrer.
[[nodiscard]] const char* PoseLabel(const SeatPoseChoice& choice);
[[nodiscard]] const char* HeadTurnLabel(const HeadTurnChoice& choice);
// Le nom de fichier d'un décor déposé, le libellé traduit de celui d'usine.
[[nodiscard]] const char* BackgroundLabel(const LayoutBackground& background);

}  // namespace ro::ui
