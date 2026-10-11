#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/ui/bug_report_window.h"

namespace moonclient {

// CE QU'UN RAPPORT DE BUG EMPORTE, composé ici et nulle part ailleurs — voir
// l'ADR 0052.
//
// 🔴 LE CONTEXTE NE SE CONSTRUIT PAS DANS LA FENÊTRE QUI LE DÉCLENCHE : aucune
// ne connaît le format, les clés ni les bornes. Elle dit d'où elle parle ; ce
// fichier sait le reste.
//
// Chaque contexte porte la RÉVISION du client — version, commit, arbre — : sans
// elle, on ne sait même pas si le défaut signalé est déjà corrigé.
//
// ⚠ `npc_gid` EST UN NOM CONTRACTUEL : le serveur le cherche pour ajouter la
// carte, la place et le script du PNJ. Toutes les autres clés sont libres.
struct BugReportContext {
  std::uint8_t category = 0;
  std::string label;
  std::string json;
};

[[nodiscard]] BugReportContext ItemBugReport(std::uint32_t item_id, std::string_view name,
                                             int refine, const std::array<std::uint32_t, 4>& cards);
[[nodiscard]] BugReportContext SkillBugReport(std::uint16_t skill_id, std::string_view name,
                                              int level);
[[nodiscard]] BugReportContext NpcBugReport(std::uint32_t npc_gid, std::string_view name);
[[nodiscard]] BugReportContext StyleBugReport(std::string_view style_code);
[[nodiscard]] BugReportContext GenericBugReport();
// Un rapport sans catégorie propre au serveur, mais avec SES champs :
// `fields_json` est une suite de membres JSON (`"clé":valeur,…`), que la
// révision du client vient fermer.
[[nodiscard]] BugReportContext FieldsBugReport(std::string label, std::string_view fields_json);

// Demande la boîte avec ce contexte. ⚠ UNE BOÎTE OUVERTE NE SE REMPLACE PAS :
// le message en cours de frappe serait jeté.
void AskBugReport(ro::ui::BugReportWindowState& window, BugReportContext context);
// De même, s'il y a un rapport : celui qu'un étage rend une fois, ou rien.
void AskBugReport(ro::ui::BugReportWindowState& window, std::optional<BugReportContext> context);

// Une chaîne prête à entrer entre guillemets dans le JSON.
[[nodiscard]] std::string JsonEscaped(std::string_view text);

}  // namespace moonclient
