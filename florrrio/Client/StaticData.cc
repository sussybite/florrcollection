#include <Client/StaticData.hh>

std::array<uint32_t, RarityID::kNumRarities> const RARITY_COLORS = { 
    0xff03fc0b, 0xff200fa3, 0xffff1760, 
    0xff9b2bf0, 0xffe32d00, 0xff1fdbde,
    0xffde1f65
 }; // 0xffff2b75, 0xfff70fb6};

std::array<uint32_t, ColorID::kNumColors> const FLOWER_COLORS = {
    0xffffe763, 0xff999999, 0xff689ce2, 0xffec6869
};

std::array<char const *, RarityID::kNumRarities> const RARITY_NAMES = {
    "çºµµº˜",
    "¨˜¨ß¨å¬",
    "®å®´",
    "´πˆç",
    "¬´©´˜∂å®¥",
    "µ¥†˙ˆç",
    "¨˜ˆœ¨´"
};

std::array<char const, MAX_SLOT_COUNT> const SLOT_KEYBINDS = 
    {'1','2','3','4','5','6','7','8'};
