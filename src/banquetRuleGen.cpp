#include "banquetRuleGen.hpp"
#include "utils/json.hpp"
#include "functions.hpp"
#include <cassert>
double randomRecipeTime = 0;
double randomChefTime = 0;
double banquetRuleTime = 0;
double generateBanquetRuleTime = 0;
double generateBanquetRuleTimeOut = 0;
const Json::Value &getIntentById(const Json::Value &intentsGD, int intentId) {
    // std::cout << intentsGD;
    for (auto &intent : intentsGD) {
        if (intent["intentId"].asInt() == intentId) {
            return intent;
        }
    }
    throw std::runtime_error("Intent not found " + std::to_string(intentId));
}
const Json::Value &getBuffById(const Json::Value &intentsGD, int intentId) {
    for (auto &intent : intentsGD) {
        if (intent["buffId"].asInt() == intentId) {
            return intent;
        }
    }
    throw std::runtime_error("Buff not found " + std::to_string(intentId));
}
Rule *getRuleFromJson(const Json::Value &intent, int d,
                      const Json::Value &allIntents,
                      const Json::Value &allBuffs,
                      int remainingDishes = DISH_PER_CHEF);

int loadBanquetRuleFromJson(RuleInfo &ruleInfo, const Json::Value &rulesTarget,
                            const Json::Value &allBuffs,
                            const Json::Value &allIntents) {
    int d = 0;
    int bestFull[NUM_GUESTS];
    int num_guest = 0;
    for (auto &guest : rulesTarget) {
        num_guest++;
        ruleInfo.bestFull[d / DISH_PER_CHEF / CHEFS_PER_GUEST] =
            guest["Satiety"].asInt();
        auto &intents = guest["IntentList"];
        for (auto &phaseIntents : intents) {
            for (auto &intent : phaseIntents) {
                int intentId = intent.asInt();
                auto &intentContent = getIntentById(allIntents, intentId);
                ruleInfo.rl.push_back(
                    getRuleFromJson(intentContent, d, allIntents, allBuffs));
            }
            d += DISH_PER_CHEF;
        }
    }
    return num_guest;
}

/**
 * @todo Error handling.
 */
int loadBanquetRule(RuleInfo &ruleInfo, const Json::Value &gameData, int ruleID,
                    bool print) {
    auto &buffsGD = gameData["buffs"];
    auto &intentsGD = gameData["intents"];
    auto &rulesGD = gameData["rules"];
    // find the rule in rulesGD with Id ruleID
    Json::Value ruleGD;
    for (auto &r : rulesGD) {
        if (r["Id"].asInt() == ruleID) {
            ruleGD = r;
            break;
        }
    }
    if (print)
        std::cout << "规则: " << ruleGD["Title"].asString() << std::endl;
    auto &rulesTarget =
        ruleGD.isMember("Group") ? ruleGD["Group"] : ruleGD["group"];
    if (rulesTarget.size() == 0) {
        std::cout << "规则为空。" << std::endl;
        return -1;
    }
    return loadBanquetRuleFromJson(ruleInfo, rulesTarget, buffsGD, intentsGD);
}
int loadBanquetRuleFromInput(RuleInfo &ruleInfo, const Json::Value &ruleData,
                             bool print) {
    if (print)
        std::cout << "规则: " << ruleData["title"].asString() << std::endl;
    auto &rulesTarget =
        ruleData.isMember("Group") ? ruleData["Group"] : ruleData["group"];
    if (rulesTarget.size() == 0) {
        std::cout << "规则为空。" << std::endl;
        return false;
    }
    auto &buffs = ruleData["buffs"];
    auto &intents = ruleData["intents"];
    return loadBanquetRuleFromJson(ruleInfo, rulesTarget, buffs, intents);
}
Rule *getRuleFromJson(const Json::Value &intent, int dish,
                      const Json::Value &allIntents,
                      const Json::Value &allBuffs, int remainingDishes) {
    Condition *c;
    auto effectType = intent["effectType"].asString();
    auto effectValue = intent["effectValue"].asInt();
    std::string conditionType;
    std::string conditionValue;

    if (intent.isMember("conditionType")) {
        conditionType = intent["conditionType"].asString();
        conditionValue = intent["conditionValue"].asString();
        if (conditionType == "CookSkill") {
            c = new SkillCondition(dish, conditionValue, remainingDishes);
        } else if (conditionType == "CondimentSkill") {
            c = new FlavorCondition(dish, conditionValue, remainingDishes);
        } else if (conditionType == "Order") {
            c = new OrderCondition(dish, getInt(intent["conditionValue"]));
        } else if (conditionType == "Rarity") {
            c = new RarityCondition(dish, getInt(intent["conditionValue"]),
                                    remainingDishes);
        } else if (conditionType == "Group") {
            assert(effectType == "CreateBuff");
            c = new GroupCondition(dish, conditionValue, remainingDishes);
        } else if (conditionType == "Rank") {
            c = new RankCondition(dish, getInt(intent["conditionValue"]),
                                  remainingDishes);
        } else {
            std::cout << "Unknown condition type: " << conditionType
                      << std::endl;
        }
    } else {
        // assert(getInt(intent["intentId"]) == 1);
        c = new AlwaysTrueCondition(dish);
    }

    Effect *e;
    if (effectType == "BasicPriceChange") {
        e = new BasePriceAddEffect(effectValue);
    } else if (effectType == "BasicPriceChangePercent") {
        e = new BasePricePercentEffect(effectValue);
    } else if (effectType == "PriceChangePercent") {
        e = new PricePercentEffect(effectValue);
    } else if (effectType == "SatietyChange") {
        e = new FullAddEffect(effectValue);
    } else if (effectType == "SetSatietyValue") {
        e = new FullSetEffect(effectValue);
    } else if (effectType == "IntentAdd") {
        e = new IntentAddEffect();
    } else if (effectType == "CreateIntent") {
        auto newIntent = getIntentById(allIntents, effectValue);
        auto newRule =
            getRuleFromJson(newIntent, dish + 1, allIntents, allBuffs, 1);
        e = new NextRuleEffect(newRule);

    } else if (effectType == "CreateBuff") {
        assert(conditionType == "Group");
        auto newIntent = getBuffById(allBuffs, effectValue);
        int lastRounds = getInt(newIntent["lastRounds"]);
        auto newRule = getRuleFromJson(newIntent, dish + DISH_PER_CHEF,
                                       allIntents, allBuffs, 1);

        e = new CreateRulesEffect(newRule, DISH_PER_CHEF * lastRounds, true);
    }
    return new SingleConditionRule(c, e);
}
void banquetRuleGenerated(BanquetRuleTogether *brt, States &s,
                          const RuleInfo &allRules) {
#ifdef MEASURE_TIME
    struct timespec start, end;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
#endif
    for (auto &r : allRules.rl) {
        (*r)(brt, s);
    }
#ifdef MEASURE_TIME
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
    generateBanquetRuleTime +=
        (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
#endif
}