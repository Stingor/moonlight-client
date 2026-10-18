#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include "ro/render/shown_job.h"

// LE FAUCON D'UN CHASSEUR : un sprite-enfant qui naît sur tout acteur portant
// l'option faucon, le suit en planant à vingt-cinq unités au-dessus du sol, et
// que le lancer d'un piège envoie en piqué vers celui-ci avant qu'il ne revienne.
//
// 🔴 TOUT SE COMPTE DANS LE REPÈRE DU CLIENT : l'ordonnée DESCEND et la
// profondeur est l'opposée de la nôtre. Les formules du jeu s'y écrivent telles
// quelles ; c'est l'appelant qui convertit à l'entrée et à la sortie, et nulle
// part ailleurs — un signe retourné à mi-chemin retournerait le vol.
namespace ro::render {

// Un point du monde dans le repère du CLIENT, en unités du monde.
struct FalconPoint {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

// ── LA RECETTE : quel oiseau pour quel métier ──────────────────────────────

// Les deux fichiers d'un oiseau, sous `data\sprite\`. ⚠ DEUX CHEMINS ET NON
// UN : le faucon du bébé chasseur prend l'image du faucon ordinaire et ses
// propres mouvements.
struct FalconArt {
  std::string_view spr_path;
  std::string_view act_path;
};

// Le bit d'option du faucon, dans `e_option`. Ceux qui changent le corps sont
// dans `shown_job.h`.
inline constexpr std::uint32_t kOptionFalcon = 0x10;

// L'OISEAU D'UN JOUEUR, d'après son STYLE DE CORPS tel que l'annonce le
// serveur, et non d'après son métier : le tireur d'élite a son faucon à lui, le
// bébé chasseur le sien, le ranger — transcendé, bébé ou en tenue alternative —
// sa chouette, le Wind Hawk son épervier ; Gangsi, Death Knight et Dark
// Collector portent un monstre en guise d'oiseau. Toute autre valeur prend le
// faucon ordinaire.
//
// ⚠ LE STYLE DE CORPS EST UN MÉTIER : le serveur y met celui du personnage
// quand il n'en porte aucun, et le styliste y pose le métier dont le joueur
// emprunte l'apparence. Un tireur d'élite habillé en ranger porte donc la
// chouette, habillé en Wind Hawk l'épervier. Aucune option n'y entre : ni
// costume, ni monture, ni vol. Un style nul donne le faucon ordinaire.
[[nodiscard]] FalconArt PlayerFalconArtFor(std::uint32_t body_style) noexcept;

// L'OISEAU D'UN ACTEUR QUI N'EST PAS UN JOUEUR, d'après le métier qu'il
// MONTRE. Mêmes oiseaux que pour un joueur, tenue alternative exceptée.
//
// ⚠ LE MÉTIER MONTRÉ N'EST PAS LE MÉTIER : un costume d'option — mariage, Noël,
// été, hanbok, Oktoberfest — remplace le corps, et l'oiseau redevient alors le
// faucon ordinaire. Le vol annule tous ces costumes sauf le mariage, qui passe
// avant lui. Un ranger monté sur son loup montre un autre corps, et reprend lui
// aussi le faucon ordinaire — pas le Wind Hawk, dont le corps monté garde son
// épervier.
[[nodiscard]] FalconArt FalconArtFor(std::uint32_t job_id, std::uint32_t option) noexcept;

// LE MÉTIER QU'UN ACTEUR MONTRE (`ShownJobFor`), réduit à ce que les
// compagnons distinguent : `kShownCostumeJob` pour un corps de costume, le
// corps monté sinon. Le faucon et la charrette choisissent leur sprite sur lui.
inline constexpr std::uint32_t kShownCostumeJob = 0;
[[nodiscard]] std::uint32_t CompanionShownJob(std::uint32_t job_id, std::uint32_t option) noexcept;

// VRAI QUAND L'ACTEUR PORTE SON FAUCON : l'option est levée et l'acteur se
// voit. Caché, camouflé ou invisible, il le perd, et le retrouve en reparaissant
// — un faucon NEUF, qui renaît sur son maître.
[[nodiscard]] bool CarriesFalcon(std::uint32_t option) noexcept;

// ── LA MÉCANIQUE ──────────────────────────────────────────────────────────

// De combien le faucon naît au-dessus des pieds de son maître.
inline constexpr float kFalconBirthRiseUnits = 20.0F;
// De combien il naît DERRIÈRE lui, le long de son cap.
inline constexpr float kFalconBirthBehindUnits = 5.0F;
// De combien il plane au-dessus du sol, au repos. Recalé à chaque tic.
inline constexpr float kFalconHoverUnits = 25.0F;
// Au-delà de cette distance à son maître, un faucon qui revient plane vers lui
// d'un vingt-cinquième de l'écart par tic, ailes figées.
inline constexpr float kFalconGlideRangeUnits = 25.0F;
inline constexpr float kFalconGlideDivisor = 25.0F;
// Au-delà de celle-ci, un faucon arrivé le suit d'une unité par tic.
inline constexpr float kFalconCruiseRangeUnits = 12.0F;
// Au-delà de celle-ci, d'un vingtième de l'écart ; en deçà, il ne bouge plus.
inline constexpr float kFalconSettleRangeUnits = 3.0F;
inline constexpr float kFalconSettleDivisor = 20.0F;
// Le battement d'ailes : une image tous les six tics.
inline constexpr std::int32_t kFalconFrameDelayTicks = 6;
// L'image, ailes figées, du faucon qui plane vers son maître.
inline constexpr std::size_t kFalconGlideFrame = 8;
// Le second groupe de huit actions : le vol vers le piège.
inline constexpr std::size_t kFalconFlightActionBase = 8;
// Le vol part d'un dixième de l'écart au piège par tic.
inline constexpr float kFalconDiveDivisor = 10.0F;
// Le compteur du vol : il descend jusqu'au dixième tic, file à plat jusqu'au
// vingt-quatrième, puis remonte, et le vol s'achève passé le trente-cinquième.
inline constexpr std::int32_t kFalconDescentLastTick = 10;
inline constexpr std::int32_t kFalconClimbFirstTick = 25;
inline constexpr std::int32_t kFalconFlightLastTick = 35;
// Ce que devient la vitesse verticale à chaque tic de la remontée.
inline constexpr float kFalconClimbFactor = -1.9F;
// L'opacité du faucon quand le joueur rend les invocations translucides.
inline constexpr std::uint8_t kFalconTranslucentAlpha = 50;
inline constexpr std::uint8_t kFalconOpaqueAlpha = 255;

// Un faucon, tel que le jeu le fait vivre tic après tic.
struct Falcon {
  FalconPoint position;
  // LE CAP, en degrés du client : zéro vers la profondeur croissante de son
  // repère, quatre-vingt-dix vers l'abscisse croissante.
  float heading_degrees = 0.0F;
  // Le cap que l'action MONTRE. Au repos, c'est celui du tic PRÉCÉDENT : le jeu
  // choisit l'action avant de retourner l'oiseau vers son maître.
  float shown_heading_degrees = 0.0F;
  // Vrai tant qu'il vole vers le piège, puis au-delà jusqu'à la fin du vol.
  bool flying = false;
  // Vrai une fois ARRIVÉ près de son maître. Un faucon naît arrivé ; le vol le
  // fait repartir de loin, et il revient alors en planant.
  bool arrived = true;
  // Vrai dès que la remontée a commencé : c'est elle qui arme la fin du vol.
  bool climbing = false;
  // Le compteur de tics. Au repos il rythme le battement d'ailes, et le vol le
  // reprend TEL QUEL — il ne repart pas de zéro.
  std::int32_t counter_ticks = 0;
  FalconPoint velocity;
  std::size_t frame = 0;
};

// Ce que le faucon sait de son maître à ce tic.
struct FalconMaster {
  FalconPoint feet;
  float heading_degrees = 0.0F;
};

// La hauteur du sol, dans le repère du client, sous un point (x, z) de ce repère.
using FalconGround = std::function<float(float x, float z)>;
// Le nombre d'images d'une action du `.act` de l'oiseau ; zéro pour une action
// absente.
using FalconFrameCount = std::function<std::size_t(std::size_t action)>;

// La naissance : sur son maître, un peu au-dessus et un peu derrière.
[[nodiscard]] Falcon BornFalcon(const FalconMaster& master) noexcept;

// Le lancer : un piqué vers `target`, qui reprend le compteur là où il en est.
void LaunchFalcon(Falcon& falcon, const FalconPoint& target) noexcept;

// VRAI POUR LES COMPÉTENCES DE DÉGÂTS QUI ENVOIENT LE FAUCON de leur auteur vers
// leur cible : Blitz Beat, Falcon Assault, et trois compétences du Wind Hawk.
// Le coup annoncé par le serveur suffit, qu'il soit lancé ou déclenché de
// lui-même — le Blitz Beat automatique arrive par le même paquet.
//
// ⚠ LA CIBLE DOIT ÊTRE UN ACTEUR EN VUE : un coup sur une unité au sol ou sur
// un acteur inconnu laisse le faucon où il est.
[[nodiscard]] bool SkillHitSendsFalcon(std::uint32_t skill_id) noexcept;

// Un tic de seize millisecondes, vu d'une caméra tournée de
// `camera_yaw_degrees` : le battement d'ailes boucle sur l'action que le
// faucon MONTRE, et celle-ci dépend de la vue.
void StepFalcon(Falcon& falcon, const FalconMaster& master, const FalconGround& ground,
                const FalconFrameCount& frame_count, float camera_yaw_degrees);

// Le cap, en degrés du client, d'un point vers un autre sur le plan (x, z).
// Rend `unchanged` quand les deux points coïncident.
[[nodiscard]] float FalconHeadingToward(const FalconPoint& from, float to_x, float to_z,
                                        float unchanged) noexcept;

// La direction d'écran — de zéro à sept — d'un cap vu d'une caméra tournée de
// `camera_yaw_degrees` (dans la convention de `SpriteDirection`).
[[nodiscard]] std::size_t FalconScreenDirection(float heading_degrees,
                                                float camera_yaw_degrees) noexcept;

// L'action que le faucon montre : la direction d'écran, dans le groupe du repos
// ou dans celui du vol.
[[nodiscard]] std::size_t FalconAction(const Falcon& falcon, float camera_yaw_degrees) noexcept;

}  // namespace ro::render
