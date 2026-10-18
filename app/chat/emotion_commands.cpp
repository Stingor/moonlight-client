#include "chat/emotion_commands.h"

#include <array>

namespace moonclient {
namespace {

// Les commandes d'émoticône du jeu, et le numéro qu'elles envoient.
//
// D'OÙ ELLES VIENNENT. Les numéros sont ceux de emotion_type, dans
// src/map/clif.hpp ; le serveur y écrit lui-même la commande de vingt-deux
// d'entre elles, en commentaire. Viennent ensuite celles de la liste classique
// du jeu, qui suit exactement le même ordre — de la deuxième à la
// trente-quatrième, sans trou —, puis celles que le client du joueur nomme
// d'un numéro ou d'un mot, et enfin les mêmes tapées sur un clavier coréen
// resté en mode latin (« /rlQma » est « /기쁨 »).
//
// ⚠ `/dice` ENVOIE LE PREMIER DÉ, et c'est le serveur qui tire le résultat
// (`client_reshuffle_dice`, allumé sur Moonlight) : le client ne choisit pas
// la face.
constexpr auto kEmotionCommands = std::to_array<EmotionCommandRow>({
    {"/!", 0},        {"/?", 1},       {"/ho", 2},      {"/delight", 2},   {"/lv", 3},
    {"/heart", 3},    {"/swt", 4},     {"/sweat", 4},   {"/ic", 5},        {"/aha", 5},
    {"/an", 6},       {"/fret", 6},    {"/ag", 7},      {"/anger", 7},     {"/$", 8},
    {"/money", 8},    {"/...", 9},     {"/gawi", 10},   {"/scissors", 10}, {"/bawi", 11},
    {"/rock", 11},    {"/bo", 12},     {"/paper", 12},  {"/korea", 13},    {"/lv2", 14},
    {"/thx", 15},     {"/thk", 15},    {"/wah", 16},    {"/sry", 17},      {"/heh", 18},
    {"/swt2", 19},    {"/hmm", 20},    {"/no1", 21},    {"/??", 22},       {"/omg", 23},
    {"/oh", 24},      {"/X", 25},      {"/hlp", 26},    {"/go", 27},       {"/sob", 28},
    {"/gg", 29},      {"/kis", 30},    {"/kis2", 31},   {"/pif", 32},      {"/ok", 33},
    {"/bzz", 36},     {"/stare", 36},  {"/awsm", 38},   {"/cool", 38},     {"/pat", 41},
    {"/goodboy", 41}, {"/mp", 42},     {"/sptime", 42}, {"/com", 44},      {"/comeon", 44},
    {"/yawn", 45},    {"/sleepy", 45}, {"/grat", 46},   {"/congrats", 46}, {"/hp", 47},
    {"/hptime", 47},  {"/fsh", 52},    {"/spin", 53},   {"/dum", 55},      {"/crwd", 56},
    {"/otl", 57},     {"/sorry", 17},  {"/smile", 18},  {"/rice", 37},     {"/meh", 39},
    {"/shy", 40},     {"/slur", 43},   {"/sigh", 54},   {"/desp", 57},     {"/dice", 58},
    {"/hum", 74},     {"/abs", 75},    {"/oops", 76},   {"/spit", 77},     {"/ene", 78},
    {"/panic", 79},   {"/whisp", 80},  {"/e1", 36},     {"/e2", 37},       {"/e3", 38},
    {"/e4", 39},      {"/e5", 40},     {"/e6", 41},     {"/e7", 42},       {"/e8", 43},
    {"/e9", 44},      {"/e10", 45},    {"/e11", 46},    {"/e12", 47},      {"/e13", 52},
    {"/e14", 53},     {"/e15", 54},    {"/e16", 55},    {"/e17", 56},      {"/e18", 57},
    {"/e19", 58},     {"/e20", 65},    {"/e27", 74},    {"/e28", 75},      {"/e29", 76},
    {"/e30", 77},     {"/e31", 78},    {"/e32", 79},    {"/e33", 80},      {"/rlQma", 2},
    {"/gkxm", 3},     {"/Eka", 4},     {"/dkgk", 5},    {"/Wkwmd", 6},     {"/ghk", 7},
    {"/ehs", 8},      {"/rkdnl", 10},  {"/wnajr", 11},  {"/qkdnl", 11},    {"/qh", 12},
});

// CE QUE `/emotion` ÉNUMÈRE, dans l'ordre du client du joueur : la liste
// classique de la deuxième émoticône à la trente-quatrième, puis la suivante
// de la trente-sixième à la quarante-septième.
constexpr auto kEmotionListLines = std::to_array<std::string_view>({
    "/lv2", "/swt",  "/ic",  "/an",   "/ag",  "/$",    "/...",  "/thx",  "/wah",  "/sry",
    "/heh", "/swt2", "/hmm", "/no1",  "/??",  "/omg",  "/oh",   "/X",    "/hlp",  "/go",
    "/sob", "/gg",   "/kis", "/kis2", "/pif", "/ok",   "/bzz",  "/rice", "/awsm", "/meh",
    "/shy", "/pat",  "/mp",  "/slur", "/com", "/yawn", "/grat", "/hp",
});

// La commande de NUMÉRO, qui n'existe pas dans le jeu : elle est à nous, pour
// atteindre les quatre-vingt-dix-huit émoticônes du sprite, y compris celles
// qu'aucune commande ne nomme.
constexpr std::string_view kNumberedEmotion = "/e ";

// Le plus grand numéro qu'un octet porte : la commande numérotée n'en atteint
// pas d'autre.
constexpr unsigned kLargestNumberedEmotion = 255;
constexpr unsigned kDecimalBase = 10;

// Le numéro que porte une commande numérotée, ou rien.
std::optional<std::uint16_t> NumberedEmotion(std::string_view line) {
  if (!line.starts_with(kNumberedEmotion)) {
    return std::nullopt;
  }
  const std::string_view digits = line.substr(kNumberedEmotion.size());
  if (digits.empty()) {
    return std::nullopt;
  }
  unsigned value = 0;
  for (const char digit : digits) {
    if (digit < '0' || digit > '9') {
      return std::nullopt;
    }
    value = value * kDecimalBase + static_cast<unsigned>(digit - '0');
    if (value > kLargestNumberedEmotion) {
      return std::nullopt;
    }
  }
  return static_cast<std::uint16_t>(value);
}

}  // namespace

std::optional<std::uint16_t> EmotionCommand(std::string_view line) {
  for (const EmotionCommandRow& command : kEmotionCommands) {
    if (line == command.name) {
      return command.emotion;
    }
  }
  return NumberedEmotion(line);
}

std::string_view EmotionCommandOf(std::uint16_t emotion) {
  for (const EmotionCommandRow& command : kEmotionCommands) {
    if (command.emotion == emotion) {
      return command.name;
    }
  }
  return {};
}

std::span<const std::string_view> EmotionListLines() { return kEmotionListLines; }

std::span<const EmotionCommandRow> EmotionCommandRows() { return kEmotionCommands; }

}  // namespace moonclient
