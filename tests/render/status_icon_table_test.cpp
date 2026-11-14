// LA SECONDE SOURCE D'IMAGES D'ÉTAT, celle que l'exécutable porte.
//
// 🔴 CE QUI SE PROUVE ICI EST QU'ELLE RÉPOND. Une table engendrée qui tomberait
// à vide — des données effacées, un générateur cassé — ne ferait rien échouer :
// la barre montrerait des lettres là où elle montrait des icônes, et l'on
// chercherait la panne dans le dessin.

#include "ro/render/status_icon_table.h"

#include <catch2/catch_test_macros.hpp>

using ro::render::BuiltInStatusIconCount;
using ro::render::BuiltInStatusIconPath;

namespace {

// ⚠ LE COMPTE EST UNE MESURE, pas un vœu : s'il change, c'est la table du
// client qu'il faut relire, pas cet essai qu'il faut ajuster.
constexpr std::size_t kMeasuredEntries = 354;

// Deux entrées au chemin ASCII, aux deux bouts de la table.
constexpr std::uint32_t kSteelBody = 87;
constexpr std::uint32_t kSwordClan = 762;

// Le métier de tout le monde, et un de la lignée Taekwon — Star Gladiator.
constexpr std::uint32_t kOrdinaryJob = 0;
constexpr std::uint32_t kStarGladiator = 4047;

}  // namespace

TEST_CASE("Table d'icônes de l'exécutable : elle porte ce qui a été relevé", "[render][etats]") {
  CHECK(BuiltInStatusIconCount() == kMeasuredEntries);
}

TEST_CASE("Table d'icônes de l'exécutable : un chemin se retrouve par son numéro",
          "[render][etats]") {
  // ⚠ LE CHEMIN EST RELATIF À LA RACINE DES TEXTURES — « effect\… » —, et non au
  // dossier des effets : c'est ainsi que l'exécutable l'écrit, et confondre les
  // deux ferait chercher « effect\effect\… ».
  CHECK(BuiltInStatusIconPath(kSteelBody, kOrdinaryJob) == "effect\\STEELBODY.tga");
  CHECK(BuiltInStatusIconPath(kSwordClan, kOrdinaryJob) == "effect\\SWORDCLAN.TGA");
}

TEST_CASE("Table d'icônes de l'exécutable : les noms coréens sont là", "[render][etats]") {
  // 🔴 LES CHEMINS CORÉENS SONT LES PLUS NOMBREUX. Une table qui ne garderait
  // que ses chemins ASCII tiendrait 113 entrées et en manquerait 226 — dont
  // Blessing, Gloria, Kyrie Eleison, Endure, c'est-à-dire les plus ordinaires.
  // Ces quatre-là sont la preuve que les octets coréens sont là.
  constexpr std::uint32_t kBlessing = 10;
  constexpr std::uint32_t kEndure = 1;
  CHECK(BuiltInStatusIconPath(kBlessing, kOrdinaryJob) == "effect\\블레싱.tga");
  CHECK(BuiltInStatusIconPath(kEndure, kOrdinaryJob) == "effect\\인듀어.tga");
}

TEST_CASE("Table d'icônes de l'exécutable : ce qu'elle ne porte pas rend vide", "[render][etats]") {
  // ⚠ LA DICHOTOMIE NE DÉBORDE NI D'UN CÔTÉ NI DE L'AUTRE. Un état hors table
  // rend vide, et la case se replie alors sur son abréviation — c'est un cas
  // ORDINAIRE, pas une panne.
  CHECK(BuiltInStatusIconPath(kSwordClan + 100, kOrdinaryJob).empty());
  CHECK(BuiltInStatusIconPath(9'999, kOrdinaryJob).empty());
}

TEST_CASE("Table d'icônes de l'exécutable : six états changent d'image selon le métier",
          "[render][etats]") {
  // 🔴 LA LIGNÉE TAEKWON A SON PROPRE JEU D'ICÔNES pour six états — Aspersio,
  // Enchant weapon et les quatre armes élémentaires. Sans le métier, une des deux
  // réponses serait forcément fausse pour quelqu'un, et rien ne le dirait : les
  // deux images existent, les deux se chargent, et seule la classe du joueur
  // sépare la juste de l'autre.
  constexpr std::uint32_t kFlameLauncher = 90;
  CHECK(BuiltInStatusIconPath(kFlameLauncher, kOrdinaryJob) == "effect\\프레임런쳐.tga");
  CHECK(BuiltInStatusIconPath(kFlameLauncher, kStarGladiator) == "effect\\i_p_FIRE.tga");

  // ⚠ ET LE MÉTIER NE DÉTOURNE QUE CES SIX-LÀ : un état ordinaire rend la même
  // image pour tout le monde.
  CHECK(BuiltInStatusIconPath(kSteelBody, kStarGladiator) ==
        BuiltInStatusIconPath(kSteelBody, kOrdinaryJob));
}
