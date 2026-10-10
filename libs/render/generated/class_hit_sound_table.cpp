// LE BRUIT D'UN JOUEUR QU'ON FRAPPE, par CLASSE.
//
// 🔴 IL NE VIENT D'AUCUN LUA : le client le porte en dur. Il dimensionne au
// démarrage une liste de noms à 28053 entrées, toutes égales au défaut
// ci-dessous, puis en
// réassigne 138 — dont 54 qui répètent ce défaut. Seules les 84 qui portent
// vraiment autre chose sont ici.
//
// ⚠ L'INDEX N'EST PAS LA CLASSE. Le client indexe par la classe jusqu'à
// 3950, et par `classe − 3950` au-delà ; les classes hors de ses bornes ne
// passent pas par ici du tout. La conversion vit dans
// `ClassHitSound`, pas dans cette table, qui garde l'index tel que le
// binaire l'écrit.
//
// Trois bruits seulement : player_clothes.wav, player_metal.wav, player_wooden_male.wav.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_class_hit_sounds.py depuis le
// relevé scripts/client_class_hit_sounds.json, pas écrit à la main.

#include "ro/render/class_hit_sound_table.h"

#include <array>

namespace ro::render {
namespace {

// Triée par index : la recherche est dichotomique.
constexpr std::array<ClassHitSoundEntry, 84> kClassHitSounds = {{
    {1, "player_metal.wav"},         {3, "player_wooden_male.wav"},
    {6, "player_wooden_male.wav"},   {7, "player_metal.wav"},
    {11, "player_wooden_male.wav"},  {12, "player_wooden_male.wav"},
    {14, "player_metal.wav"},        {15, "player_metal.wav"},
    {17, "player_wooden_male.wav"},  {19, "player_wooden_male.wav"},
    {20, "player_wooden_male.wav"},  {24, "player_wooden_male.wav"},
    {25, "player_wooden_male.wav"},  {52, "player_metal.wav"},
    {54, "player_wooden_male.wav"},  {57, "player_wooden_male.wav"},
    {58, "player_metal.wav"},        {62, "player_wooden_male.wav"},
    {63, "player_wooden_male.wav"},  {65, "player_metal.wav"},
    {66, "player_metal.wav"},        {68, "player_wooden_male.wav"},
    {70, "player_wooden_male.wav"},  {71, "player_wooden_male.wav"},
    {74, "player_metal.wav"},        {76, "player_wooden_male.wav"},
    {79, "player_wooden_male.wav"},  {80, "player_metal.wav"},
    {84, "player_wooden_male.wav"},  {85, "player_wooden_male.wav"},
    {87, "player_metal.wav"},        {88, "player_metal.wav"},
    {90, "player_wooden_male.wav"},  {92, "player_wooden_male.wav"},
    {93, "player_wooden_male.wav"},  {96, "player_wooden_male.wav"},
    {97, "player_metal.wav"},        {98, "player_metal.wav"},
    {100, "player_wooden_male.wav"}, {101, "player_metal.wav"},
    {104, "player_metal.wav"},       {106, "player_wooden_male.wav"},
    {109, "player_wooden_male.wav"}, {110, "player_metal.wav"},
    {112, "player_wooden_male.wav"}, {115, "player_wooden_male.wav"},
    {116, "player_metal.wav"},       {118, "player_wooden_male.wav"},
    {119, "player_wooden_male.wav"}, {120, "player_metal.wav"},
    {122, "player_wooden_male.wav"}, {123, "player_metal.wav"},
    {125, "player_wooden_male.wav"}, {126, "player_wooden_male.wav"},
    {127, "player_metal.wav"},       {129, "player_wooden_male.wav"},
    {261, "player_wooden_male.wav"}, {262, "player_wooden_male.wav"},
    {272, "player_wooden_male.wav"}, {273, "player_wooden_male.wav"},
    {274, "player_wooden_male.wav"}, {275, "player_wooden_male.wav"},
    {276, "player_metal.wav"},       {278, "player_wooden_male.wav"},
    {279, "player_wooden_male.wav"}, {288, "player_metal.wav"},
    {289, "player_metal.wav"},       {291, "player_metal.wav"},
    {302, "player_metal.wav"},       {304, "player_wooden_male.wav"},
    {305, "player_metal.wav"},       {307, "player_wooden_male.wav"},
    {308, "player_metal.wav"},       {310, "player_wooden_male.wav"},
    {312, "player_metal.wav"},       {313, "player_wooden_male.wav"},
    {314, "player_wooden_male.wav"}, {330, "player_metal.wav"},
    {331, "player_metal.wav"},       {352, "player_metal.wav"},
    {354, "player_wooden_male.wav"}, {355, "player_wooden_male.wav"},
    {356, "player_wooden_male.wav"}, {366, "player_metal.wav"},
}};

}  // namespace

std::span<const ClassHitSoundEntry> ClassHitSoundTable() { return kClassHitSounds; }

}  // namespace ro::render
