#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "character_panes.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Ce qui sépare la colonne des libellés de celle des valeurs.
constexpr float kStatValueGap = 12.0F;
// Ce dont le fond d'un libellé déborde, à gauche et à droite.
constexpr float kStatRowBleedLeft = 3.0F;
constexpr float kStatRowBleedRight = 5.0F;
constexpr float kStatRowRounding = 4.0F;
// Ce qui sépare le « + » du coût qu'il fait payer.
constexpr float kCostGap = 6.0F;

// Le montant qu'un bouton « Max » demande. Le serveur le borne lui-même au
// nombre de crans que les points permettent (`pc_maxparameterincrease`), et
// c'est pour cela qu'on peut demander l'impossible sans risque.
constexpr std::uint8_t kRaiseAsMuchAsPossible = 255;

// Ce qui sépare les deux unités à virgule de leur pour cent : le serveur compte
// les taux d'auto-sort par mille, et les chances de statut par dix mille.
constexpr std::int32_t kPerMillePerPercent = 10;
constexpr std::int32_t kCentiPerPercent = 100;

// Les paliers ronds du jeu. Maj+clic monte jusqu'au prochain, parce que les
// seuils de vitesse d'attaque et de temps d'incantation tombent dessus.
constexpr std::int64_t kStatStep = 10;

// ── LES GABARITS DE MESURE ──────────────────────────────────────────────────
//
// ⚠ CE NE SONT PAS DES VALEURS PLAUSIBLES MAIS DES PIRES CAS. Une
// caractéristique plafonne à trois chiffres et son bonus en prend cinq ; une
// dérivée, sur un personnage de haut niveau, en prend six de chaque côté.
// Mesurer sur ce qu'on voit d'ordinaire donne une colonne qui craque le jour
// où le personnage grossit.
constexpr const char* kWidestAttributeValue = "- 999 (+99999)";
constexpr const char* kWidestDerivedValue = "999999 ~ 999999";
// Le jour entre le bouton « Max » et le « + ».
constexpr float kButtonGap = 4.0F;

// La place du coût du cran suivant, à droite du « + ». Trois chiffres
// suffisent : le coût d'une caractéristique plafonne bien avant mille.
[[nodiscard]] float StatCostWidth() { return ImGui::CalcTextSize("999").x + kCostGap * Scale(); }

// Combien il y en a de chaque sorte. Les deux boucles de mesure les
// parcourent, et un libellé ajouté sans son compte sortirait de la mesure
// sans sortir de l'écran.
constexpr std::size_t kAttributeCount = 6;
constexpr std::size_t kDerivedCount = 9;

// Les six, dans l'ordre du client.
[[nodiscard]] const char* AttributeLabel(std::size_t rank) {
  // ⚠ DES LITTÉRAUX, un par branche : `core::Tr` veut voir sa chaîne à l'appel.
  // Et ces six-là ne se traduisent pas — ce sont les noms que les joueurs
  // lisent depuis toujours.
  switch (rank) {
    case 0:
      return core::Tr("STR");
    case 1:
      return core::Tr("AGI");
    case 2:
      return core::Tr("VIT");
    case 3:
      return core::Tr("INT");
    case 4:
      return core::Tr("DEX");
    default:
      return core::Tr("LUK");
  }
}

// Le fond d'un libellé : il s'arrête au MOT et ne court pas jusqu'au bout de la
// rangée. C'est ce qui laisse les valeurs lire comme une colonne.
void PaintLabelBackground(std::string_view text) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const float width = TextWidth(text);
  const float scale = Scale();
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(at.x - kStatRowBleedLeft * scale, at.y),
      ImVec2(at.x + width + kStatRowBleedRight * scale, at.y + ImGui::GetTextLineHeight()),
      kCharacterStatRowBackground, kStatRowRounding * scale);
}

// LES NEUF DÉRIVÉES, DANS L'ORDRE DU CLIENT.
//
// ⚠ UN AIGUILLAGE DE LITTÉRAUX, pour la même raison que les six : `core::Tr`
// veut voir sa chaîne à l'appel. Et il sert DEUX FOIS — pour mesurer la colonne
// et pour l'écrire — ce qui est précisément ce qui les empêche de diverger.
[[nodiscard]] const char* DerivedLabel(std::size_t rank) {
  switch (rank) {
    case 0:
      return core::Tr("ATK");
    case 1:
      return core::Tr("MATK");
    case 2:
      return core::Tr("DEF");
    case 3:
      return core::Tr("MDEF");
    case 4:
      return core::Tr("HIT");
    case 5:
      return core::Tr("FLEE");
    case 6:
      return core::Tr("CRI");
    case 7:
      return core::Tr("ASPD");
    default:
      return core::Tr("Esq.P");
  }
}

// OÙ COMMENCE LA COLONNE DES VALEURS.
//
// 🔴 ELLE SE MESURE SUR LES LIBELLÉS TRADUITS, ET C'EST TOUT L'INTÉRÊT. Un seul
// littéral français pris pour référence donnerait une colonne juste en français
// et trop étroite ailleurs : « Esq.P » fait cinq caractères, sa traduction
// anglaise « P.Dodge » en fait sept, et la valeur s'écrirait par-dessus le
// libellé.
// Une mesure qui ne passe pas par la traduction est fausse dans toutes les
// langues sauf une.
[[nodiscard]] float ValueColumnOffset() {
  float widest = 0.0F;
  for (std::size_t rank = 0; rank < kAttributeCount; ++rank) {
    widest = std::max(widest, ImGui::CalcTextSize(AttributeLabel(rank)).x);
  }
  for (std::size_t rank = 0; rank < kDerivedCount; ++rank) {
    widest = std::max(widest, ImGui::CalcTextSize(DerivedLabel(rank)).x);
  }
  return widest + kStatValueGap * Scale();
}

// Une rangée d'APPORT : même colonne que les dérivées, mais le libellé peut la
// dépasser.
//
// ⚠ UN LIBELLÉ TROP LONG POUSSE SA VALEUR au lieu de se la faire écrire dessus.
// `SameLine` à un décalage ABSOLU ramènerait le curseur en arrière — et « Réduc.
// distance » traduit dépasse la colonne mesurée sur « MDEF ». Une valeur
// décalée vaut mieux qu'une rangée illisible.
void BonusRow(const CharacterBonusRow& row) {
  const float start = ImGui::GetCursorPosX();
  const char* label = row.label.data();
  const char* label_end = row.label.data() + row.label.size();
  PaintLabelBackground(row.label);
  ImGui::TextUnformatted(label, label_end);
  if (!row.tooltip.empty() && ImGui::IsItemHovered()) {
    SkinTooltipFormat("%.*s", static_cast<int>(row.tooltip.size()), row.tooltip.data());
  }

  const float after =
      start + ImGui::CalcTextSize(label, label_end).x + ImGui::GetStyle().ItemSpacing.x;
  const float column = start + ValueColumnOffset();
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(after, column));

  char value[32] = {};
  WriteBonusValue(value, sizeof(value), row);
  ImGui::TextUnformatted(value);
  if (!row.tooltip.empty() && ImGui::IsItemHovered()) {
    SkinTooltipFormat("%.*s", static_cast<int>(row.tooltip.size()), row.tooltip.data());
  }
}

// L'en-tête d'une section repliable, aux couleurs de la peau.
//
// ⚠ IMGUI LE DESSINE DANS SES PROPRES COULEURS, qui sont celles d'un outil et
// non celles du jeu. Les trois états se posent donc ici, et le texte avec : un
// bandeau bleu vif au milieu d'une fenêtre beige se voit plus que la section
// qu'il annonce.
[[nodiscard]] bool BonusSection(const char* label, const SkinLook& look) {
  ImGui::PushStyleColor(ImGuiCol_Header, kCharacterStatRowBackground);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, look.input);
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, look.input);
  ImGui::PushStyleColor(ImGuiCol_Text, look.title_text);
  const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
  ImGui::PopStyleColor(4);
  return open;
}

// Une rangée de dérivée : le libellé, puis la valeur sur sa colonne.
void DerivedRow(const char* label, const char* value) {
  const float start = ImGui::GetCursorPosX();
  PaintLabelBackground(label);
  ImGui::TextUnformatted(label);
  ImGui::SameLine(start + ValueColumnOffset());
  ImGui::TextUnformatted(value);
}

// Une des six : sa valeur, son bonus, ce que le cran suivant coûte, et de quoi
// le payer.
void AttributeRow(std::size_t rank, const CharacterAttributeView& attribute,
                  std::int64_t status_point, const SkinArt& art, CharacterWindowAction& action) {
  ImGui::PushID(static_cast<int>(rank));
  const char* label = AttributeLabel(rank);
  const float start = ImGui::GetCursorPosX();
  PaintLabelBackground(label);
  ImGui::TextUnformatted(label);
  ImGui::SameLine(start + ValueColumnOffset());

  char value[64] = {};
  if (attribute.bonus != 0) {
    // « 40 + 12 » DIT D'OÙ VIENT LE TOTAL, « 52 » ne dit rien.
    //
    // ⚠ TIRET ASCII, et c'est celui du client : un cadratin n'est pas garanti
    // dans l'atlas, et un glyphe manquant se montre en carré vide.
    std::snprintf(value, sizeof(value), "- %lld (+%lld)", static_cast<long long>(attribute.base),
                  static_cast<long long>(attribute.bonus));
  } else {
    std::snprintf(value, sizeof(value), "- %lld", static_cast<long long>(attribute.base));
  }
  ImGui::TextUnformatted(value);

  // ⚠ UN COÛT NUL NE SE PAIE PAS : c'est « pas encore dit », ou le plafond,
  // que le serveur annonce par un zéro. Les deux boutons DISPARAISSENT alors,
  // au lieu de griser : rien ne les rallumera sur une caractéristique au bout.
  if (attribute.cost <= 0) {
    ImGui::PopID();
    return;
  }
  const bool affordable = attribute.cost <= status_point;

  // 🔴 SUR UNE COLONNE CALÉE À DROITE : posés au fil de la valeur, les boutons
  // danseraient d'une rangée à l'autre selon sa longueur.
  const std::array<const char*, 1> raise = {core::Tr("Max")};
  const float max_width = SkinWidestButton(raise, art);
  const float square = ImGui::GetFrameHeight();
  const float column = ImGui::GetContentRegionMax().x -
                       (max_width + kButtonGap * Scale() + square + StatCostWidth());
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), column));
  ImGui::BeginDisabled(!affordable);
  if (SkinButton(core::Tr("Max"), art, ImVec2(max_width, 0.0F))) {
    action.verb = CharacterVerb::kRaiseAttribute;
    action.attribute = static_cast<std::uint8_t>(rank);
    action.amount = kRaiseAsMuchAsPossible;
  }
  ImGui::SameLine(0.0F, kButtonGap * Scale());
  // ⚠ CARRÉ, ET C'EST CE QUI LE REND MESURABLE. Un bouton dont la largeur
  // suit son texte se mesurerait autrement que la colonne ne le prévoit, et
  // la rangée déborderait d'un cheveu — assez pour pousser le coût dehors.
  if (SkinButton("+", art, ImVec2(square, 0.0F))) {
    action.verb = CharacterVerb::kRaiseAttribute;
    action.attribute = static_cast<std::uint8_t>(rank);
    action.amount = AttributeRaiseAmount(attribute, ImGui::GetIO().KeyShift);
  }
  ImGui::EndDisabled();

  ImGui::SameLine(0.0F, kCostGap * Scale());
  ImGui::TextDisabled("%lld", static_cast<long long>(attribute.cost));
  ImGui::PopID();
}

// La largeur du bouton des préréglages, mesurée sur son libellé traduit.
[[nodiscard]] float StatPresetButtonWidth(const SkinArt& art) {
  const std::array<const char*, 1> label = {core::Tr("Préréglages")};
  return SkinWidestButton(label, art);
}

// LA PORTE DES PRÉRÉGLAGES DE STATS, au bout de la rangée des points : c'est
// eux qu'un préréglage dépense.
//
// ⚠ CALÉE À DROITE, sur le bord où finissent les « + » : à la suite du texte,
// elle bougerait avec le nombre de points.
void StatPresetButton(const SkinArt& art, CharacterWindowAction& action) {
  const float width = StatPresetButtonWidth(art);
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - width));
  if (SkinButton(core::Tr("Préréglages"), art, ImVec2(width, 0.0F))) {
    action.toggle_stat_presets = true;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Enregistrer et appliquer des préréglages de stats"));
  }
}

}  // namespace

int AttributeRaiseAmount(const CharacterAttributeView& attribute, bool to_next_step) {
  if (!to_next_step) {
    return 1;
  }
  const std::int64_t total = attribute.base + attribute.bonus;
  return static_cast<int>(kStatStep - (total % kStatStep));
}

float CharacterStatsPaneWidth(const SkinArt& art) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float scale = Scale();
  const float value_x = ValueColumnOffset();

  // 🔴 LA RANGÉE LA PLUS CONTRAINTE EST CELLE D'UNE CARACTÉRISTIQUE, et non
  // celle d'une dérivée : sa valeur s'écrit au fil du texte alors que « Max »,
  // « + » et le coût sont poussés à droite. Trop étroite, la valeur passe SOUS
  // les boutons — elle ne déborde pas, elle disparaît.
  const std::array<const char*, 1> raise = {core::Tr("Max")};
  const float attribute = value_x + ImGui::CalcTextSize(kWidestAttributeValue).x +
                          style.ItemSpacing.x + SkinWidestButton(raise, art) + kButtonGap * scale +
                          ImGui::GetFrameHeight() + StatCostWidth();

  // ⚠ LE GABARIT DES DÉRIVÉES EST GÉNÉREUX À SIX CHIFFRES, et il le faut : sur
  // ce serveur un MATK se compte en centaines de milliers. Mesurer sur trois
  // chiffres donnerait une colonne juste sur un personnage neuf et fausse sur
  // tous les autres.
  const float derived = value_x + ImGui::CalcTextSize(kWidestDerivedValue).x;

  // ⚠ LE BOUTON DES PRÉRÉGLAGES PARTAGE CETTE RANGÉE, et il compte : traduit,
  // il peut l'emporter sur celle d'une caractéristique.
  const float points = ImGui::CalcTextSize(core::Tr("Points de statut :")).x +
                       ImGui::CalcTextSize(" 9999").x + style.ItemSpacing.x +
                       StatPresetButtonWidth(art);

  const float content = std::max(std::max(attribute, derived), points);
  // ⚠ ET L'ASCENSEUR EST COMPTÉ, même quand rien ne défile aujourd'hui : il
  // prend sa largeur SUR le contenu, et une fenêtre courte en fait apparaître un
  // — qui rognerait alors la colonne qu'on vient de mesurer.
  return std::max(kCharacterStatsPaneMinimum * scale,
                  std::ceil(content + style.WindowPadding.x * 2.0F + style.ScrollbarSize));
}

void DrawCharacterStats(const CharacterStatsView& stats, const SkinArt& art,
                        CharacterWindowAction& action, CharacterStatRows& rows) {
  // 🔴 LES ORDONNÉES SE RELÈVENT PENDANT LE DESSIN, et elles sont RELATIVES au
  // haut du contenu. Le volet du staff pose ses commandes en face de la valeur
  // qu'elles changent, et il ne peut pas les deviner : la police et l'échelle
  // décident de la hauteur de chaque rangée. Les prendre en coordonnées d'écran
  // fausserait l'alignement dès que ce volet-ci défile.
  const float top = ImGui::GetCursorPosY();
  for (std::size_t rank = 0; rank < stats.attributes.size(); ++rank) {
    rows.attribute[rank] = ImGui::GetCursorPosY() - top;
    AttributeRow(rank, stats.attributes[rank], stats.status_point, art, action);
  }

  rows.status_point = ImGui::GetCursorPosY() - top;
  char line[64] = {};
  std::snprintf(line, sizeof(line), "%s %lld", core::Tr("Points de statut :"),
                static_cast<long long>(stats.status_point));
  // ⚠ CALÉ SUR LE BOUTON qui partage la rangée : sans cela, le texte flotterait
  // au-dessus de son centre.
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(line);
  StatPresetButton(art, action);
  ImGui::Separator();

  // LES NEUF DÉRIVÉES, AUX FORMATS DU CLIENT.
  //
  // ⚠ CHAQUE FORME DIT QUELQUE CHOSE : « + » sépare une base d'un bonus, « ~ »
  // un minimum d'un maximum, et le pour cent ne s'écrit que du côté qui en est
  // un. Les uniformiser effacerait ce que le joueur lit d'un coup d'œil.
  //
  // ⚠ ET LES LIBELLÉS VIENNENT DE `DerivedLabel`, jamais d'un littéral posé
  // ici : c'est lui que la colonne a mesuré, et deux sources finiraient par
  // s'écarter — l'écran remettrait alors la valeur par-dessus le mot.
  rows.first_derived = ImGui::GetCursorPosY() - top;
  rows.measured = true;
  std::size_t row = 0;
  const auto say = [&](const char* value) { DerivedRow(DerivedLabel(row++), value); };

  std::snprintf(line, sizeof(line), "%lld + %lld", static_cast<long long>(stats.atk1),
                static_cast<long long>(stats.atk2));
  say(line);
  // 🔴 LE MINIMUM D'ABORD, ET C'EST `matk2`. Le serveur inverse cette paire-là
  // et elle seule : `SP_MATK1` porte le côté DROIT — le maximum — alors que
  // `SP_ATK1`, `SP_DEF1` et `SP_MDEF1` portent tous le côté gauche
  // (`clif_updatestatus`, src/map/clif.cpp). Écrire « matk1 ~ matk2 » donnerait
  // « 163615 ~ 84380 », un intervalle à l'envers que rien ne signalerait.
  std::snprintf(line, sizeof(line), "%lld ~ %lld", static_cast<long long>(stats.matk2),
                static_cast<long long>(stats.matk1));
  say(line);
  std::snprintf(line, sizeof(line), "%lld%% + %lld", static_cast<long long>(stats.def1),
                static_cast<long long>(stats.def2));
  say(line);
  std::snprintf(line, sizeof(line), "%lld%% + %lld", static_cast<long long>(stats.mdef1),
                static_cast<long long>(stats.mdef2));
  say(line);

  std::snprintf(line, sizeof(line), "%lld", static_cast<long long>(stats.hit));
  say(line);
  std::snprintf(line, sizeof(line), "%lld", static_cast<long long>(stats.flee1));
  say(line);
  // ⚠ DÉJÀ EN POUR CENT ENTIERS : le serveur les divise par dix avant de les
  // envoyer, et les rediviser montrerait un dixième de la vraie valeur.
  std::snprintf(line, sizeof(line), "%lld%%", static_cast<long long>(stats.critical));
  say(line);
  std::snprintf(line, sizeof(line), "%lld", static_cast<long long>(stats.aspd));
  say(line);
  std::snprintf(line, sizeof(line), "%lld%%", static_cast<long long>(stats.flee2));
  say(line);

  // CE QUE L'ÉQUIPEMENT APPORTE, et que le client d'origine ne sait pas dire.
  //
  // ⚠ RIEN DU TOUT PLUTÔT QU'UNE SECTION VIDE : la liste arrive déjà triée, et
  // vide elle veut dire « le serveur n'a rien dit » ou « ce personnage n'a aucun
  // apport ». Un en-tête qui s'ouvre sur rien se lit comme une panne.
  if (stats.equipment_bonuses.empty() && stats.conditional_bonuses.empty() &&
      stats.skill_bonuses.empty() && stats.item_bonuses.empty()) {
    return;
  }
  ImGui::Separator();
  if (!stats.equipment_bonuses.empty() &&
      BonusSection(core::Tr("Bonus d'équipement"), art.look())) {
    for (const CharacterBonusRow& gear_bonus : stats.equipment_bonuses) {
      BonusRow(gear_bonus);
    }
  }
  // ⚠ SA PROPRE SECTION, ET REPLIABLE À PART : un personnage équipé en porte des
  // dizaines, et les mêler aux apports plats noierait ces derniers.
  if (!stats.conditional_bonuses.empty() && BonusSection(core::Tr("Conditionnels"), art.look())) {
    for (const CharacterBonusRow& conditional : stats.conditional_bonuses) {
      BonusRow(conditional);
    }
  }
  if (!stats.skill_bonuses.empty() && BonusSection(core::Tr("Skills & statuts"), art.look())) {
    for (const CharacterBonusRow& held_bonus : stats.skill_bonuses) {
      BonusRow(held_bonus);
    }
  }
  if (!stats.item_bonuses.empty() && BonusSection(core::Tr("Items"), art.look())) {
    for (const CharacterBonusRow& item_bonus : stats.item_bonuses) {
      BonusRow(item_bonus);
    }
  }
}

// La valeur d'une rangée, dans l'unité que le serveur lui donne.
//
// ⚠ LE SIGNE S'ÉCRIT TOUJOURS pour les trois unités entières : un apport négatif
// existe — une réduction de temps d'incantation en est une — et « 12 % » ne dit
// pas dans quel sens. Les deux unités à virgule, elles, sont des TAUX, que le
// serveur n'envoie jamais négatifs ; on garde quand même la partie décimale en
// valeur absolue, sans quoi un jour « -1,-5 % » s'écrirait.
void WriteBonusValue(char* into, std::size_t room, const CharacterBonusRow& row) {
  switch (row.unit) {
    case BonusUnit::kPercent:
      std::snprintf(into, room, "%+d%%", row.value);
      return;
    case BonusUnit::kMilliseconds:
      std::snprintf(into, room, core::Tr("%+d ms"), row.value);
      return;
    case BonusUnit::kPerMille:
      std::snprintf(into, room, "%d,%d%%", row.value / kPerMillePerPercent,
                    std::abs(row.value) % kPerMillePerPercent);
      return;
    case BonusUnit::kCentiPercent:
      std::snprintf(into, room, "%d,%02d%%", row.value / kCentiPerPercent,
                    std::abs(row.value) % kCentiPerPercent);
      return;
    case BonusUnit::kFlat:
      break;
  }
  std::snprintf(into, room, "%+d", row.value);
}

}  // namespace ro::ui
