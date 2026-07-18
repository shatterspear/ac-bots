#include "bot_ai.h"
#include "botcommon.h"
#include "botconfig.h"
#include "botdatamgr.h"
#include "botgossip.h"
#include "botspell.h"
#include "bottext.h"
#include "botmgr.h"
#include "Chat.h"
#include "Creature.h"
#include "Log.h"
#include "Player.h"
#include "RaceMgr.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include <map>
/*
NPCbot giver NPC by Trickerer (<https://github.com/trickerer/> <onlysuffering@gmail.com>)
Complete - 100%
*/

#define HIRE GOSSIP_SENDER_BOTGIVER_HIRE
#define HIRE_CLASS GOSSIP_SENDER_BOTGIVER_HIRE_CLASS
#define HIRE_RACE GOSSIP_SENDER_BOTGIVER_HIRE_RACE
#define HIRE_GENDER GOSSIP_SENDER_BOTGIVER_HIRE_GENDER
#define HIRE_ENTRY GOSSIP_SENDER_BOTGIVER_HIRE_ENTRY

namespace
{
uint32 PackBotSelection(uint8 botclass, uint8 race, uint8 gender = 0)
{
    return GOSSIP_ACTION_INFO_DEF + uint32(botclass) + (uint32(race) << 8) + (uint32(gender) << 16);
}

uint8 UnpackBotClass(uint32 action) { return uint8((action - GOSSIP_ACTION_INFO_DEF) & 0xFF); }
uint8 UnpackBotRace(uint32 action) { return uint8(((action - GOSSIP_ACTION_INFO_DEF) >> 8) & 0xFF); }
uint8 UnpackBotGender(uint32 action) { return uint8(((action - GOSSIP_ACTION_INFO_DEF) >> 16) & 0xFF); }

uint32 GetRaceTextId(uint8 race)
{
    switch (race)
    {
        case RACE_HUMAN:         return BOT_TEXT_RACE_HUMAN;
        case RACE_ORC:           return BOT_TEXT_RACE_ORC;
        case RACE_DWARF:         return BOT_TEXT_RACE_DWARF;
        case RACE_NIGHTELF:      return BOT_TEXT_RACE_NELF;
        case RACE_UNDEAD_PLAYER: return BOT_TEXT_RACE_UNDEAD;
        case RACE_TAUREN:        return BOT_TEXT_RACE_TAUREN;
        case RACE_GNOME:         return BOT_TEXT_RACE_GNOME;
        case RACE_TROLL:         return BOT_TEXT_RACE_TROLL;
        case RACE_BLOODELF:      return BOT_TEXT_RACE_BELF;
        case RACE_DRAENEI:       return BOT_TEXT_RACE_DRAENEI;
        default:                 return BOT_TEXT_RACE_UNKNOWN;
    }
}
}

class script_bot_giver : public CreatureScript
{
public:
    script_bot_giver() : CreatureScript("script_bot_giver") { }

    //struct bot_giver_AI : public CreatureAI
    //{
    //    bot_giver_AI(Creature* creature) : CreatureAI(creature) {}

    //    void UpdateAI(uint32 /*diff*/) override {}

        bool OnGossipHello(Player* player, Creature* me) override
        {
            if (!BotCfg::IsNpcBotModEnabled())
            {
                player->PlayerTalkClass->SendCloseGossip();
                return true;
            }

            if (me->isMoving())
                me->BotStopMovement();

            AddGossipItemFor(player, GOSSIP_ICON_TALK, bot_ai::LocalizedNpcText(player, BOT_TEXT_BOTGIVER_SERVICE), HIRE, GOSSIP_ACTION_INFO_DEF + 1);

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, bot_ai::LocalizedNpcText(player, BOT_TEXT_NEVERMIND), 0, GOSSIP_ACTION_INFO_DEF + 2);

            player->PlayerTalkClass->SendGossipMenu(GOSSIP_BOTGIVER_GREET, me->GetGUID());
            return true;
        }

        //bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        bool OnGossipSelect(Player* player, Creature* me, uint32 sender, uint32 action) override
        {
            if (!BotCfg::IsNpcBotModEnabled())
            {
                player->PlayerTalkClass->SendCloseGossip();
                return true;
            }

            //uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            //uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);

            player->PlayerTalkClass->ClearMenus();
            bool subMenu = false;

            uint32 gossipTextId = GOSSIP_BOTGIVER_GREET;

            switch (sender)
            {
                case 0: //exit
                    break;
                case 1: //BACK: return to main menu
                    return OnGossipHello(player, me);
                case HIRE:
                {
                    gossipTextId = GOSSIP_BOTGIVER_HIRE;

                    if (player->GetNpcBotsCount() >= BotCfg::GetMaxNpcBots(player->GetLevel()))
                    {
                        WhisperTo(player, me, bot_ai::LocalizedNpcText(player, BOT_TEXT_BOTGIVER_TOO_MANY_BOTS).c_str());
                        break;
                    }

                    if (uint32 maxBotsPerAccount = BotCfg::GetMaxAccountBots())
                    {
                        uint32 accountBotsCount = BotDataMgr::GetAccountBotsCount(player->GetSession()->GetAccountId());
                        if (accountBotsCount >= maxBotsPerAccount)
                        {
                            ChatHandler ch(player->GetSession());
                            ch.PSendSysMessage(bot_ai::LocalizedNpcText(player, BOT_TEXT_HIREFAIL_MAXBOTS_ACCOUNT).c_str(), accountBotsCount, maxBotsPerAccount);
                            break;
                        }
                    }

                    subMenu = true;

                    uint8 availCount = 0;
                    std::array<uint32, BOT_CLASS_END> npcbot_count_per_class{ 0 };

                    {
                        std::shared_lock lock(*BotDataMgr::GetLock());
                        for (Creature const* bot : BotDataMgr::GetExistingNPCBots())
                        {
                            if (!bot->IsAlive() || bot->IsTempBot() || bot->IsSummon() || bot->IsWandererBot() || bot->GetBotAI()->GetBotOwnerGuid() || bot->HasAura(BERSERK))
                                continue;
                            if (BotCfg::FilterRaces() && bot->GetBotClass() < BOT_CLASS_EX_START && (bot->GetRaceMask() & sRaceMgr->GetPlayableRaceMask()) &&
                                !(bot->GetRaceMask() & ((player->GetRaceMask() & sRaceMgr->GetAllianceRaceMask()) ? sRaceMgr->GetAllianceRaceMask() : sRaceMgr->GetHordeRaceMask())))
                                continue;

                            ++npcbot_count_per_class[bot->GetBotClass()];
                        }
                    }

                    for (uint8 botclass = BOT_CLASS_WARRIOR; botclass < BOT_CLASS_END; ++botclass)
                    {
                        if (!BotCfg::IsClassEnabled(botclass))
                            continue;

                        if (player->HaveBot() && BotCfg::GetMaxClassBots())
                        {
                            uint8 count = static_cast<uint8>(std::ranges::count_if(*player->GetBotMgr()->GetBotMap(), [=](BotMap::value_type const& kv) {
                                return kv.second->GetBotClass() == botclass;
                            }));
                            if (count >= BotCfg::GetMaxClassBots())
                                continue;
                        }

                        uint32 textId;
                        switch (botclass)
                        {
                            case BOT_CLASS_WARRIOR:     textId = BOT_TEXT_CLASS_WARRIOR_PLU;        break;
                            case BOT_CLASS_PALADIN:     textId = BOT_TEXT_CLASS_PALADIN_PLU;        break;
                            case BOT_CLASS_MAGE:        textId = BOT_TEXT_CLASS_MAGE_PLU;           break;
                            case BOT_CLASS_PRIEST:      textId = BOT_TEXT_CLASS_PRIEST_PLU;         break;
                            case BOT_CLASS_WARLOCK:     textId = BOT_TEXT_CLASS_WARLOCK_PLU;        break;
                            case BOT_CLASS_DRUID:       textId = BOT_TEXT_CLASS_DRUID_PLU;          break;
                            case BOT_CLASS_DEATH_KNIGHT:textId = BOT_TEXT_CLASS_DEATH_KNIGHT_PLU;   break;
                            case BOT_CLASS_ROGUE:       textId = BOT_TEXT_CLASS_ROGUE_PLU;          break;
                            case BOT_CLASS_SHAMAN:      textId = BOT_TEXT_CLASS_SHAMAN_PLU;         break;
                            case BOT_CLASS_HUNTER:      textId = BOT_TEXT_CLASS_HUNTER_PLU;         break;
                            case BOT_CLASS_BM:          textId = BOT_TEXT_CLASS_BM_PLU;             break;
                            case BOT_CLASS_SPHYNX:      textId = BOT_TEXT_CLASS_SPHYNX_PLU;         break;
                            case BOT_CLASS_ARCHMAGE:    textId = BOT_TEXT_CLASS_ARCHMAGE_PLU;       break;
                            case BOT_CLASS_DREADLORD:   textId = BOT_TEXT_CLASS_DREADLORD_PLU;      break;
                            case BOT_CLASS_SPELLBREAKER:textId = BOT_TEXT_CLASS_SPELLBREAKER_PLU;   break;
                            case BOT_CLASS_DARK_RANGER: textId = BOT_TEXT_CLASS_DARK_RANGER_PLU;    break;
                            case BOT_CLASS_NECROMANCER: textId = BOT_TEXT_CLASS_NECROMANCER_PLU;    break;
                            case BOT_CLASS_SEA_WITCH:   textId = BOT_TEXT_CLASS_SEAWITCH_PLU;       break;
                            case BOT_CLASS_CRYPT_LORD:  textId = BOT_TEXT_CLASS_CRYPT_LORD_PLU;     break;
                            default:                    textId = 0;                                 break;
                        }

                        if (!textId)
                            continue;

                        std::ostringstream bclass;
                        bclass << npcbot_count_per_class[botclass] << " " << bot_ai::LocalizedNpcText(player, textId) << " (" << BotCfg::GetNpcBotCostStr(player->GetLevel(), botclass) << ")";

                        AddGossipItemFor(player, GOSSIP_ICON_TALK, bclass.str(), HIRE_CLASS, GOSSIP_ACTION_INFO_DEF + botclass);

                        if (++availCount >= BOT_GOSSIP_MAX_ITEMS - 1) //back
                            break;
                    }

                    if (availCount == 0)
                        gossipTextId = GOSSIP_BOTGIVER_HIRE_EMPTY;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, bot_ai::LocalizedNpcText(player, BOT_TEXT_NEVERMIND), 0, GOSSIP_ACTION_INFO_DEF + 1);

                    break;
                }
                case HIRE_CLASS:
                {
                    gossipTextId = GOSSIP_BOTGIVER_HIRE_CLASS;

                    uint8 botclass = action - GOSSIP_ACTION_INFO_DEF;

                    uint32 cost = BotCfg::GetNpcBotCostHire(player->GetLevel(), botclass);
                    if (!player->HasEnoughMoney(cost))
                    {
                        WhisperTo(player, me, bot_ai::LocalizedNpcText(player, BOT_TEXT_HIREFAIL_COST).c_str());
                        break;
                    }

                    subMenu = true;

                    std::map<uint8, uint32> raceCounts;

                    {
                        std::shared_lock lock(*BotDataMgr::GetLock());
                        for (Creature const* bot : BotDataMgr::GetExistingNPCBots())
                        {
                            bot_ai const* ai = bot->GetBotAI();
                            if (bot->GetBotClass() != botclass || !bot->IsAlive() || ai->IsTempBot() || bot->IsWandererBot() || bot->IsSummon() || ai->GetBotOwnerGuid() || bot->HasAura(BERSERK))
                                continue;
                            if (BotCfg::FilterRaces() && botclass < BOT_CLASS_EX_START && (bot->GetRaceMask() & sRaceMgr->GetPlayableRaceMask()) &&
                                !(bot->GetRaceMask() & ((player->GetRaceMask() & sRaceMgr->GetAllianceRaceMask()) ? sRaceMgr->GetAllianceRaceMask() : sRaceMgr->GetHordeRaceMask())))
                                continue;

                            ++raceCounts[bot->GetRace()];
                        }
                    }

                    for (auto const& [race, count] : raceCounts)
                    {
                        std::ostringstream label;
                        label << bot_ai::LocalizedNpcText(player, GetRaceTextId(race)) << " (" << count << ')';
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, label.str(), HIRE_RACE, PackBotSelection(botclass, race));
                    }

                    if (raceCounts.empty())
                        gossipTextId = GOSSIP_BOTGIVER_HIRE_EMPTY;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, bot_ai::LocalizedNpcText(player, BOT_TEXT_BACK), HIRE, GOSSIP_ACTION_INFO_DEF + 1);

                    break;
                }
                case HIRE_RACE:
                {
                    gossipTextId = GOSSIP_BOTGIVER_HIRE_CLASS;
                    uint8 botclass = UnpackBotClass(action);
                    uint8 race = UnpackBotRace(action);
                    std::array<uint32, GENDER_NONE> genderCounts{};

                    subMenu = true;
                    {
                        std::shared_lock lock(*BotDataMgr::GetLock());
                        for (Creature const* bot : BotDataMgr::GetExistingNPCBots())
                        {
                            bot_ai const* ai = bot->GetBotAI();
                            if (bot->GetBotClass() != botclass || bot->GetRace() != race || !bot->IsAlive() || ai->IsTempBot() ||
                                bot->IsWandererBot() || bot->IsSummon() || ai->GetBotOwnerGuid() || bot->HasAura(BERSERK))
                                continue;
                            if (bot->GetGender() < GENDER_NONE)
                                ++genderCounts[bot->GetGender()];
                        }
                    }

                    if (genderCounts[GENDER_MALE])
                    {
                        std::ostringstream label;
                        label << bot_ai::LocalizedNpcText(player, BOT_TEXT_GENDER_MALE) << " (" << genderCounts[GENDER_MALE] << ')';
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, label.str(), HIRE_GENDER,
                            PackBotSelection(botclass, race, GENDER_MALE));
                    }
                    if (genderCounts[GENDER_FEMALE])
                    {
                        std::ostringstream label;
                        label << bot_ai::LocalizedNpcText(player, BOT_TEXT_GENDER_FEMALE) << " (" << genderCounts[GENDER_FEMALE] << ')';
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, label.str(), HIRE_GENDER,
                            PackBotSelection(botclass, race, GENDER_FEMALE));
                    }
                    if (!genderCounts[GENDER_MALE] && !genderCounts[GENDER_FEMALE])
                        gossipTextId = GOSSIP_BOTGIVER_HIRE_EMPTY;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, bot_ai::LocalizedNpcText(player, BOT_TEXT_BACK), HIRE_CLASS,
                        GOSSIP_ACTION_INFO_DEF + botclass);
                    break;
                }
                case HIRE_GENDER:
                {
                    gossipTextId = GOSSIP_BOTGIVER_HIRE_CLASS;
                    uint8 botclass = UnpackBotClass(action);
                    uint8 race = UnpackBotRace(action);
                    uint8 gender = UnpackBotGender(action);
                    uint32 cost = BotCfg::GetNpcBotCostHire(player->GetLevel(), botclass);
                    uint8 availCount = 0;

                    subMenu = true;
                    {
                        std::shared_lock lock(*BotDataMgr::GetLock());
                        for (Creature const* bot : BotDataMgr::GetExistingNPCBots())
                        {
                            bot_ai const* ai = bot->GetBotAI();
                            if (bot->GetBotClass() != botclass || bot->GetRace() != race || bot->GetGender() != gender ||
                                !bot->IsAlive() || ai->IsTempBot() || bot->IsWandererBot() || bot->IsSummon() ||
                                ai->GetBotOwnerGuid() || bot->HasAura(BERSERK))
                                continue;

                            std::ostringstream confirmation;
                            confirmation << bot_ai::LocalizedNpcText(player, BOT_TEXT_BOTGIVER_WISH_TO_HIRE_) << bot->GetName() << '?';
                            player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, GOSSIP_ICON_TALK, bot->GetName(),
                                HIRE_ENTRY, GOSSIP_ACTION_INFO_DEF + bot->GetEntry(), confirmation.str(), cost, false);

                            if (++availCount >= BOT_GOSSIP_MAX_ITEMS - 1)
                                break;
                        }
                    }

                    if (availCount == 0)
                        gossipTextId = GOSSIP_BOTGIVER_HIRE_EMPTY;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, bot_ai::LocalizedNpcText(player, BOT_TEXT_BACK), HIRE_RACE,
                        PackBotSelection(botclass, race));
                    break;
                }
                case HIRE_ENTRY:
                {
                    uint32 entry = action - GOSSIP_ACTION_INFO_DEF;
                    Creature const* bot = BotDataMgr::FindBot(entry);
                    if (!bot)
                    {
                        //possible but still
                        BOT_LOG_ERROR("entities.unit", "HIRE_NBOT_ENTRY: bot {} not found!", entry);
                        break;
                    }

                    bot_ai const* ai = bot->GetBotAI();
                    if (bot->IsInCombat() || !bot->IsAlive() || bot_ai::CCed(bot) ||
                        bot->HasUnitState(UNIT_STATE_CASTING) || ai->GetBotOwnerGuid() || bot->HasAura(BERSERK))
                    {
                        //BOT_LOG_ERROR("entities.unit", "HIRE_NBOT_ENTRY: bot %u (%s) is unavailable all of the sudden!", entry);
                        std::ostringstream failMsg;
                        failMsg << bot->GetName() << bot_ai::LocalizedNpcText(player, BOT_TEXT_BOTGIVER__BOT_BUSY);
                        WhisperTo(player, me, failMsg.view());
                        break;
                    }

                    //laways returns true
                    bot->GetBotAI()->OnGossipSelect(player, me, GOSSIP_SENDER_HIRE, GOSSIP_ACTION_INFO_DEF);

                    if (player->HaveBot() && player->GetBotMgr()->GetBot(bot->GetGUID()))
                        WhisperTo(player, me, bot_ai::LocalizedNpcText(player, BOT_TEXT_BOTGIVER_HIRESUCCESS).c_str());

                    break;
                }
            }

            if (subMenu)
                player->PlayerTalkClass->SendGossipMenu(gossipTextId, me->GetGUID());
            else
                player->PlayerTalkClass->SendCloseGossip();

            return true;
        }

        void WhisperTo(Player* player, Creature* me, std::string_view message)
        {
            me->Whisper(message, LANG_UNIVERSAL, player);
        }
    //};

    //CreatureAI* GetAI(Creature* creature) const override
    //{
    //    return new bot_giver_AI(creature);
    //}
};

void AddSC_script_bot_giver()
{
    new script_bot_giver();
}
