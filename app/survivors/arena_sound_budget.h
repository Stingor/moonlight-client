#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// LE LIMITEUR DE VOIX DU MODE SURVIVORS, décidé sans son : il dit si un bruit
// de l'arène part, et lequel lui cède sa voix. Le mélangeur ne fait que jouer
// ce qui a été admis.
namespace moonclient {

// LES VOIX DU MODE : vingt au plus. Le mélangeur en a trente-deux pour tout le
// client, et les douze qui restent appartiennent à l'interface — un clic, une
// fenêtre qui s'ouvre ne doivent jamais se taire faute de voix sous la horde.
inline constexpr std::size_t kArenaSoundVoices = 20;
inline constexpr std::size_t kArenaInterfaceVoices = 12;

// UN DÉPART PAR NOM au plus dans cette fenêtre : trois cents monstres touchés
// à la même image ne lancent pas trois cents fois le même coup.
inline constexpr std::uint64_t kArenaSoundNameCadenceMs = 60;

// LE RANG D'UN BRUIT, du plus humble au plus pressant. Un rang cède sa voix à
// un rang supérieur, jamais à un égal ni à un inférieur.
enum class ArenaSoundRank : std::uint8_t {
  // Cris et morts des monstres.
  kCry,
  // Frappes des skills du héros.
  kSkill,
  // Ce que fait un boss.
  kBoss,
  // Une carte qui tombe ou qu'on ramasse : rare, elle passe devant le boss.
  kTreasure,
  // Montée de niveau et évolution.
  kRise,
  // Un coup encaissé par le héros : le joueur doit l'entendre.
  kHeroHurt,
};

struct ArenaSoundVerdict {
  // Vrai : le bruit part, sa voix est comptée jusqu'à sa fin estimée.
  bool play = false;
  // Le bruit de rang inférieur qui cède sa place, à faire taire avant de jouer
  // celui-ci ; vide quand une voix était libre. Toutes ses voix se libèrent,
  // comme le mélangeur fait taire toutes celles qui jouent un même nom.
  std::string silenced;
};

// L'OCCUPATION DES VOIX DU MODE, estimée par la durée décodée de chaque bruit.
//
// L'horloge est celle du mur (`now_ms`) : un bruit parti sonne jusqu'au bout
// même quand la partie se fige.
class ArenaSoundBudget {
 public:
  // Demande une voix pour `name` à `now_ms`. Une durée inconnue est refusée :
  // le mélangeur ne joue pas un bruit qu'il ne connaît pas.
  [[nodiscard]] ArenaSoundVerdict Ask(std::string_view name, ArenaSoundRank rank,
                                      std::optional<std::uint64_t> length_ms, std::uint64_t now_ms);

  // Oublie toutes les voix et toutes les cadences : la partie est finie ou
  // relancée, et le mélangeur a fait taire ses bruits.
  void Clear();

  // Les voix encore occupées à `now_ms`.
  [[nodiscard]] std::size_t busy(std::uint64_t now_ms) const noexcept;

  [[nodiscard]] std::size_t played_total() const noexcept { return played_total_; }
  [[nodiscard]] std::size_t refused_total() const noexcept { return refused_total_; }
  [[nodiscard]] std::size_t silenced_total() const noexcept { return silenced_total_; }

 private:
  struct Voice {
    std::string name;
    ArenaSoundRank rank = ArenaSoundRank::kCry;
    std::uint64_t ends_ms = 0;
  };

  // Rend les voix dont la fin estimée est passée.
  void Release(std::uint64_t now_ms);
  // La voix qu'un bruit de rang `rank` peut prendre : le rang le plus bas
  // strictement inférieur, et parmi lui celle qui finit la première. Un nom
  // dont une voix au moins tient un rang égal ou supérieur ne cède pas. Nulle
  // quand aucune ne cède.
  [[nodiscard]] const Voice* Yielding(ArenaSoundRank rank) const noexcept;

  std::vector<Voice> voices_;
  // Le dernier départ admis de chaque nom.
  std::map<std::string, std::uint64_t, std::less<>> last_start_ms_;
  std::size_t played_total_ = 0;
  std::size_t refused_total_ = 0;
  std::size_t silenced_total_ = 0;
};

}  // namespace moonclient
