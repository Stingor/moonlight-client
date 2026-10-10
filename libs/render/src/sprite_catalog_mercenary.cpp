// LES MERCENAIRES HUMAINS : un corps de créature, habillé comme un joueur.
//
// Le serveur n'envoie d'eux que leur classe — ni coupe, ni sexe, ni
// équipement. C'est le client qui les habille, et chaque pièce se déduit de la
// classe seule : une tête, l'arme de la lignée, sa lueur, une coiffe propre à
// chaque classe, et trois ornements de plus pour les trois meilleurs.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

// Les trois lignées, dix classes chacune : les archères, les lanciers, les
// épéistes.
constexpr std::uint32_t kFirstArcher = 6017;
constexpr std::uint32_t kFirstSpearman = 6027;
constexpr std::uint32_t kFirstSwordsman = 6037;
constexpr std::uint32_t kLastSwordsman = 6046;
constexpr std::uint32_t kClassesPerLine = 10;

// Les meilleurs de chaque lignée, qui portent trois ornements de plus.
constexpr std::uint32_t kBestArcher = 6026;
constexpr std::uint32_t kBestSpearman = 6036;
constexpr std::uint32_t kBestSwordsman = 6046;

// LA TÊTE : un numéro de fichier tiré de la classe, et non une coupe — sans la
// permutation des coiffures d'un joueur. Vingt-trois têtes, de une à vingt-trois.
constexpr std::uint32_t kMercenaryHeads = 23;

constexpr std::string_view kMercenaryFolder = "용병";

// L'arme de chaque lignée, et le suffixe de sa lueur.
constexpr std::string_view kArcherWeapon = "활용병_활";
constexpr std::string_view kSpearmanWeapon = "창용병_창";
constexpr std::string_view kSwordsmanWeapon = "검용병_검";
constexpr std::string_view kWeaponGlowSuffix = "_검광";

// LA COIFFE DE CHAQUE CLASSE, sous le dossier des accessoires, dans l'ordre des
// classes : les dix archères, puis les dix lanciers, puis les dix épéistes.
constexpr std::array<std::string_view, 30> kClassHeadgears = {
    "여\\여_털모자",        "여\\여_골든헤드기어", "여\\여_대형금방울",   "여\\여_엘리스모자",
    "여\\여_가부키가면",    "여\\여_여왕의투구",   "여\\여_지르타스가면", "여\\여_실버티아라",
    "여\\여_고양이모자",    "여\\여_예쁜리본",     "남\\남_간호모",       "남\\남_검은눈가리개",
    "남\\남_고블린가면4호", "남\\남_양모자",       "남\\남_귀찮은너구리", "남\\남_돈잃은자의마음",
    "남\\남_마리오넷모자",  "남\\남_라이온마스크", "남\\남_클로스헬멧",   "남\\남_로드카호의뿔",
    "남\\남_반창고",        "남\\남_분노입",       "남\\남_코끼리모자",   "남\\남_야구모자",
    "남\\남_장례건",        "남\\남_부용",         "남\\남_타이거마스크", "남\\남_초승달투구",
    "남\\남_엔젤링모자",    "남\\남_태양신의모자"};

// LES ORNEMENTS DES MEILLEURS, après la coiffe, dans l'ordre du client. Vide :
// pas d'ornement à ce rang. ⚠ LE MEILLEUR ÉPÉISTE N'A PAS DE PREMIER ORNEMENT à
// lui : le client y repeint sa coiffe, au même endroit, ce qui ne se voit pas.
struct BestOrnaments {
  std::uint32_t job = 0;
  std::array<std::string_view, 3> ornaments;
};
constexpr std::array<BestOrnaments, 3> kBestOrnaments = {{
    {.job = kBestArcher, .ornaments = {"여\\여_꽃잎", "여\\여_요정의귀", "여\\여_미스틱로즈"}},
    {.job = kBestSpearman,
     .ornaments = {"남\\남_토스트", "남\\남_요정의귀", "남\\남_타키우스눈가리개"}},
    {.job = kBestSwordsman, .ornaments = {"", "남\\남_요정의귀", "남\\남_사이클롭스아이"}},
}};

[[nodiscard]] std::string_view WeaponOf(std::uint32_t job) {
  if (job < kFirstSpearman) {
    return kArcherWeapon;
  }
  return job < kFirstSwordsman ? kSpearmanWeapon : kSwordsmanWeapon;
}

[[nodiscard]] std::string HeadgearPath(std::string_view name) {
  return Join({kSpriteRoot, kHeadgearFolder, kSeparator, name});
}

}  // namespace

bool IsHumanMercenary(std::uint32_t job) { return job >= kFirstArcher && job <= kLastSwordsman; }

void AppendMercenaryPieces(std::uint32_t job, std::vector<ActorPieceFiles>& files) {
  if (!IsHumanMercenary(job)) {
    return;
  }
  // LA TÊTE : femme pour les archères, homme pour les autres.
  const bool archer = job < kFirstArcher + kClassesPerLine;
  const std::string_view sex = archer ? kFemaleToken : kMaleToken;
  const std::string head_number = std::to_string(job % kMercenaryHeads + 1);
  files.push_back(
      ActorPieceFiles{.part = ActorPart::kHead,
                      .candidates = {Join({kSpriteRoot, kHumanRace, kSeparator, kHeadFolder,
                                           kSeparator, sex, kSeparator, head_number, "_", sex})}});

  // L'ARME DE LA LIGNÉE, puis sa lueur — que seuls les lanciers et les
  // épéistes portent. ⚠ SONDÉES : l'installation peut ne pas les avoir.
  const std::string weapon =
      Join({kSpriteRoot, kHumanRace, kSeparator, kMercenaryFolder, kSeparator, WeaponOf(job)});
  files.push_back(
      ActorPieceFiles{.part = ActorPart::kWeapon, .candidates = {weapon}, .optional = true});
  if (!archer) {
    files.push_back(ActorPieceFiles{.part = ActorPart::kWeaponTrail,
                                    .candidates = {weapon + std::string(kWeaponGlowSuffix)},
                                    .optional = true});
  }

  // LA COIFFE DE LA CLASSE, portée par la tête ; puis les ornements des
  // meilleurs, portés de même.
  files.push_back(ActorPieceFiles{.part = ActorPart::kHeadgearLow,
                                  .candidates = {HeadgearPath(kClassHeadgears[job - kFirstArcher])},
                                  .optional = true});
  for (const BestOrnaments& best : kBestOrnaments) {
    if (best.job != job) {
      continue;
    }
    for (const std::string_view ornament : best.ornaments) {
      if (!ornament.empty()) {
        files.push_back(ActorPieceFiles{.part = ActorPart::kHeadgearTop,
                                        .candidates = {HeadgearPath(ornament)},
                                        .optional = true});
      }
    }
  }
}

}  // namespace ro::render::catalog_internal

namespace ro::render {

bool IsHumanMercenaryClass(std::uint32_t job) { return catalog_internal::IsHumanMercenary(job); }

}  // namespace ro::render
