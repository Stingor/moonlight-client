#pragma once

// LES TEXTES DU CATALOGUE DE LA MÉTA QUE `sim` NE PORTE PAS — l'effet chiffré
// d'une Bénédiction, la description d'un service Kafra, le but d'un exploit —,
// désignés par la CLÉ stable du catalogue : la vue ne passe que la clé, et
// `ui` ne connaît aucun type de `sim`. Avec eux, les dates et les montants des
// fenêtres de la méta. Le fichier source tient aussi les phrases du
// classement par code, déclarées dans `ro/ui/survivors_ranking_window.h`
// parce qu'`app` les dit en conversation.
//
// ⚠ À PART DE `survivors_text.cpp`, qui TRANSMET des textes marqués ailleurs
// et que le relevé des traductions écarte pour cela : ceux-ci se déclarent
// ici, et le relevé doit les voir.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ro::ui {

inline constexpr std::size_t kSurvivorsEffectTextSize = 64;
using SurvivorsEffectText = std::array<char, kSurvivorsEffectTextSize>;

// L'EFFET D'UNE BÉNÉDICTION À UN TOTAL (« Puissance +9 % »), traduit. Une clé
// inconnue rend le total signé seul.
[[nodiscard]] SurvivorsEffectText SurvivorsBlessingEffect(std::string_view blessing_key, int total);

// L'effet d'un rang de service Kafra : les charges par partie.
[[nodiscard]] SurvivorsEffectText SurvivorsKafraEffect(int charges);

// Ce que fait un service Kafra, traduit ; "" pour une clé inconnue.
[[nodiscard]] const char* SurvivorsKafraDescription(std::string_view service_key);

// Le but d'un exploit, traduit ; "" pour une clé inconnue.
[[nodiscard]] const char* SurvivorsFeatGoal(std::string_view feat_key);

// UNE DATE UTC « AAAA-MM-JJ », depuis des jours ou des secondes comptés du
// 1970-01-01.
inline constexpr std::size_t kSurvivorsDateTextSize = 16;
using SurvivorsDateText = std::array<char, kSurvivorsDateTextSize>;
[[nodiscard]] SurvivorsDateText SurvivorsDateOfDay(std::int64_t days_since_epoch);
[[nodiscard]] SurvivorsDateText SurvivorsDateOfUtc(std::int64_t utc_s);

// Un montant groupé par milliers, comme dans les autres fenêtres.
inline constexpr std::size_t kSurvivorsZenyTextSize = 32;
using SurvivorsZenyText = std::array<char, kSurvivorsZenyTextSize>;
[[nodiscard]] SurvivorsZenyText SurvivorsZeny(std::uint64_t zeny);

}  // namespace ro::ui
