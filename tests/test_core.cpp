#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "core/caps_state_machine.hpp"
#include "core/detector.hpp"
#include "core/layout_engine.hpp"

using namespace Core;
using namespace Config;

namespace {

int g_testsPassed = 0;

#define CHECK(expr)                                                                 \
    do {                                                                            \
        if (!(expr)) {                                                              \
            std::cerr << "FAIL at " << __FILE__ << ":" << __LINE__ << ": " #expr   \
                      << std::endl;                                                 \
            std::exit(1);                                                           \
        }                                                                           \
    } while (0)

void TypeWord(Detector& det, const std::wstring& word) {
    det.Reset();
    for (wchar_t ch : word) {
        det.OnChar(ch);
    }
}

void TestCapsHoldAndType() {
    CapsLockStateMachine sm;
    CapsConfig cfg{ true, true, true };

    // Hold Caps (EN -> RU), type 'a', release Caps -> reverts RU -> EN
    auto a1 = sm.OnCapsDown(1000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(a1.suppressKey);
    CHECK(a1.switchLayout && a1.targetLang == LANG_ID_RU);
    CHECK(!a1.triggerDoubleCaps);
    CHECK(!a1.resetBuffer);

    // First non-Shift key while held resets Detector buffer and skips Detector
    auto k1 = sm.OnOtherKeyDown('A', false, false, false, false, false, cfg);
    CHECK(k1.skipDetectorForKey);
    CHECK(k1.resetBuffer);
    CHECK(!k1.emitShiftedDigit);
    CHECK(!k1.suppressKey);

    // Second key while held skips Detector, does not redundantly reset
    auto k2 = sm.OnOtherKeyDown('B', false, false, false, false, false, cfg);
    CHECK(k2.skipDetectorForKey);
    CHECK(!k2.resetBuffer);
    CHECK(!k2.emitShiftedDigit);

    // Release Caps quickly (<200ms) -> still reverts because a key was typed while held
    auto u1 = sm.OnCapsUp(1080, false, cfg);
    CHECK(u1.suppressKey);
    CHECK(u1.switchLayout && u1.targetLang == LANG_ID_EN);
    CHECK(u1.resetBuffer);
    CHECK(!sm.IsCapsPressed());

    ++g_testsPassed;
}

void TestShiftAndCapsSimultaneous() {
    CapsLockStateMachine sm;
    CapsConfig cfg{ true, true, true };

    // Order 1: Caps down -> Shift down -> Key down -> Shift up -> Caps up
    auto d1 = sm.OnCapsDown(2000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(d1.suppressKey && d1.switchLayout && d1.targetLang == LANG_ID_RU);

    auto sh = sm.OnOtherKeyDown(VK_LSHIFT, true, false, false, false, false, cfg);
    CHECK(sh.skipDetectorForKey);
    CHECK(!sh.resetBuffer); // Shift alone does not need to reset buffer yet

    auto k = sm.OnOtherKeyDown('G', true, false, false, false, false, cfg);
    CHECK(k.skipDetectorForKey);
    CHECK(k.resetBuffer);

    auto u1 = sm.OnCapsUp(2100, false, cfg);
    CHECK(u1.suppressKey && u1.switchLayout && u1.targetLang == LANG_ID_EN && u1.resetBuffer);

    // Order 2: Shift down FIRST -> Caps down -> Key down -> Caps up
    // Must NOT pass Caps to OS, must switch layout, and must revert on Caps up even if quick!
    auto d2 = sm.OnCapsDown(3000, true, false, false, false, false, LANG_ID_RU, cfg);
    CHECK(d2.suppressKey);
    CHECK(d2.switchLayout && d2.targetLang == LANG_ID_EN);
    CHECK(!d2.triggerDoubleCaps);

    auto k2 = sm.OnOtherKeyDown('H', true, false, false, false, false, cfg);
    CHECK(k2.skipDetectorForKey && k2.resetBuffer);

    auto u2 = sm.OnCapsUp(3050, false, cfg);
    CHECK(u2.suppressKey);
    CHECK(u2.switchLayout && u2.targetLang == LANG_ID_RU);
    CHECK(u2.resetBuffer);

    ++g_testsPassed;
}

void TestHoldCapsDigitShiftedSymbol() {
    CapsLockStateMachine sm;
    CapsConfig cfg{ true, true, true };

    // 1. In RU layout -> hold Caps (switches to EN) -> press '2' without Shift -> emits EN shifted digit ('@')
    auto d1 = sm.OnCapsDown(1000, false, false, false, false, false, LANG_ID_RU, cfg);
    CHECK(d1.suppressKey && d1.switchLayout && d1.targetLang == LANG_ID_EN);

    auto dig2 = sm.OnOtherKeyDown('2', false, false, false, false, false, cfg);
    CHECK(dig2.suppressKey);
    CHECK(dig2.emitShiftedDigit);
    CHECK(dig2.targetLang == LANG_ID_EN);
    CHECK(dig2.resetBuffer);
    CHECK(dig2.skipDetectorForKey);

    // Release Caps Lock first, then release '2' -> orphan KEYUP of '2' must still be suppressed
    auto u1 = sm.OnCapsUp(1080, false, cfg);
    CHECK(u1.suppressKey && u1.switchLayout && u1.targetLang == LANG_ID_RU);
    CHECK(sm.ShouldSuppressKeyUp('2'));
    CHECK(!sm.ShouldSuppressKeyUp('2')); // only once

    // 2. Ctrl+2 while Caps held must NOT emit shifted digit
    auto d2 = sm.OnCapsDown(2000, false, false, false, false, false, LANG_ID_RU, cfg);
    CHECK(d2.suppressKey && d2.switchLayout);
    auto ctrlDig = sm.OnOtherKeyDown('2', false, true, false, false, false, cfg);
    CHECK(!ctrlDig.emitShiftedDigit);
    CHECK(!ctrlDig.suppressKey);
    CHECK(!sm.ShouldSuppressKeyUp('2'));
    sm.OnCapsUp(2100, false, cfg);

    // 3. When capsHoldSwitch is disabled, digit does NOT emit shifted symbol
    CapsConfig noHold{ true, false, true };
    sm.OnCapsDown(3000, false, false, false, false, false, LANG_ID_RU, noHold);
    auto digNoHold = sm.OnOtherKeyDown('3', false, false, false, false, false, noHold);
    CHECK(!digNoHold.emitShiftedDigit && !digNoHold.suppressKey);
    sm.OnCapsUp(3100, false, noHold);

    // 4. Verify LayoutEngine shifted digit mapping for EN and RU
    auto& le = LayoutEngine::Instance();
    CHECK(le.GetShiftedDigitChar('1', 0x02, LANG_ID_EN) == L'!');
    CHECK(le.GetShiftedDigitChar('2', 0x03, LANG_ID_EN) == L'@');
    CHECK(le.GetShiftedDigitChar('3', 0x04, LANG_ID_EN) == L'#');
    CHECK(le.GetShiftedDigitChar('4', 0x05, LANG_ID_EN) == L'$');
    CHECK(le.GetShiftedDigitChar('6', 0x07, LANG_ID_EN) == L'^');
    CHECK(le.GetShiftedDigitChar('7', 0x08, LANG_ID_EN) == L'&');

    CHECK(le.GetShiftedDigitChar('1', 0x02, LANG_ID_RU) == L'!');
    CHECK(le.GetShiftedDigitChar('2', 0x03, LANG_ID_RU) == L'\"');
    CHECK(le.GetShiftedDigitChar('3', 0x04, LANG_ID_RU) == L'№');
    CHECK(le.GetShiftedDigitChar('4', 0x05, LANG_ID_RU) == L';');
    CHECK(le.GetShiftedDigitChar('6', 0x07, LANG_ID_RU) == L':');
    CHECK(le.GetShiftedDigitChar('7', 0x08, LANG_ID_RU) == L'?');

    ++g_testsPassed;
}

void TestHoldTimeoutAndQuickTap() {
    CapsLockStateMachine sm;
    CapsConfig cfg{ true, true, true };

    // Hold >= 200ms without typing any key -> reverts on up
    auto d1 = sm.OnCapsDown(1000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(d1.suppressKey && d1.switchLayout && d1.targetLang == LANG_ID_RU);
    auto u1 = sm.OnCapsUp(1200, false, cfg);
    CHECK(u1.suppressKey && u1.switchLayout && u1.targetLang == LANG_ID_EN && u1.resetBuffer);

    // Quick tap (< 200ms, no Shift, no key) with capsLockSwitch=true -> stays switched
    auto d2 = sm.OnCapsDown(2000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(d2.suppressKey && d2.switchLayout && d2.targetLang == LANG_ID_RU);
    auto u2 = sm.OnCapsUp(2090, false, cfg);
    CHECK(u2.suppressKey);
    CHECK(!u2.switchLayout);

    // Quick tap with capsLockSwitch=false, capsHoldSwitch=true -> reverts on up
    CapsConfig holdOnly{ false, true, false };
    auto d3 = sm.OnCapsDown(3000, false, false, false, false, false, LANG_ID_EN, holdOnly);
    CHECK(d3.suppressKey && d3.switchLayout && d3.targetLang == LANG_ID_RU);
    auto u3 = sm.OnCapsUp(3050, false, holdOnly);
    CHECK(u3.suppressKey && u3.switchLayout && u3.targetLang == LANG_ID_EN && u3.resetBuffer);

    ++g_testsPassed;
}

void TestDoubleCapsAutoRepeatAndWinCaps() {
    CapsLockStateMachine sm;
    CapsConfig cfg{ true, true, true };

    // Double quick tap (<450ms) -> triggers DoubleCapsConvert, does NOT reset buffer on 1st down
    auto d1 = sm.OnCapsDown(1000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(d1.suppressKey && d1.switchLayout && !d1.resetBuffer);
    // Auto-repeat while held -> suppressed, no duplicate switch
    auto rep = sm.OnCapsDown(1040, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(rep.suppressKey && !rep.switchLayout && !rep.triggerDoubleCaps);
    auto u1 = sm.OnCapsUp(1070, false, cfg);
    CHECK(u1.suppressKey && !u1.switchLayout);

    auto d2 = sm.OnCapsDown(1200, false, false, false, false, false, LANG_ID_RU, cfg);
    CHECK(d2.suppressKey && d2.triggerDoubleCaps && !d2.switchLayout);
    auto u2 = sm.OnCapsUp(1250, false, cfg);
    CHECK(u2.suppressKey && !u2.switchLayout);

    // Win + Caps Lock -> passes down through to OS, suppresses auto-repeat, passes up through even if Win released first
    auto wd = sm.OnCapsDown(2000, false, false, false, true, false, LANG_ID_EN, cfg);
    CHECK(!wd.suppressKey && wd.resetBuffer && !wd.switchLayout);
    auto wrep = sm.OnCapsDown(2050, false, false, false, true, false, LANG_ID_EN, cfg);
    CHECK(wrep.suppressKey);
    auto wu = sm.OnCapsUp(2100, false, cfg);
    CHECK(!wu.suppressKey && !wu.switchLayout);

    // Pause mid-hold -> resets cleanly, suppresses matching keyup
    auto pd = sm.OnCapsDown(3000, false, false, false, false, false, LANG_ID_EN, cfg);
    CHECK(pd.suppressKey && pd.switchLayout);
    auto pu = sm.OnCapsUp(3300, true, cfg);
    CHECK(pu.suppressKey && !pu.switchLayout);
    CHECK(!sm.IsCapsPressed());

    ++g_testsPassed;
}

void TestConfigJsonRoundTripAndBackwardCompat() {
    AppConfig cfg;
    cfg.capsHoldSwitch = false;
    cfg.wordDictionary.clear();
    cfg.SetWordRule(L"спс", AppLayout::Russian);
    cfg.SetWordRule(L"docker", AppLayout::English);
    cfg.SetWordRule(L"v,", AppLayout::Russian);
    cfg.SetWordRule(L"[p", AppLayout::Russian);
    cfg.SetWordRule(L"g;", AppLayout::Russian);

    std::string json = SerializeConfigJson(cfg);

    AppConfig parsed;
    parsed.capsHoldSwitch = true;
    parsed.wordDictionary.clear();
    ParseConfigJson(json, parsed);

    CHECK(parsed.capsHoldSwitch == false);
    CHECK(parsed.GetWordRule(L"спс") == AppLayout::Russian);
    CHECK(parsed.GetWordRule(L"docker") == AppLayout::English);
    CHECK(parsed.GetWordRule(L"v,") == AppLayout::Russian);
    CHECK(parsed.GetWordRule(L"[p") == AppLayout::Russian);
    CHECK(parsed.GetWordRule(L"g;") == AppLayout::Russian);
    CHECK(parsed.wordDictionary.size() == 5);

    // Backward compatibility: older config.json without caps_hold_switch or switch_dictionary keeps defaults
    AppConfig legacy;
    std::string legacyJson = R"({
      "auto_switch": true,
      "caps_lock_switch": true,
      "double_caps_convert": true,
      "password_guard": true,
      "autostart": false,
      "exclusions": ["cmd.exe"],
      "app_layouts": {"code.exe": "en"}
    })";
    ParseConfigJson(legacyJson, legacy);
    CHECK(legacy.capsHoldSwitch == true);
    CHECK(!legacy.wordDictionary.empty());
    CHECK(legacy.GetWordRule(L"спс") == AppLayout::Russian);
    CHECK(legacy.GetWordRule(L"docker") == AppLayout::English);

    // Explicit empty switch_dictionary loads empty
    AppConfig emptyDict;
    std::string emptyDictJson = R"({
      "auto_switch": true,
      "switch_dictionary": {}
    })";
    ParseConfigJson(emptyDictJson, emptyDict);
    CHECK(emptyDict.wordDictionary.empty());

    ++g_testsPassed;
}

void TestDetectorCustomDictionaryAndHeuristics() {
    auto& cfgMgr = ConfigManager::Instance();
    auto& det = Detector::Instance();

    // Ensure starter + custom rules in memory (without touching config.json on disk)
    cfgMgr.Update([](AppConfig& cfg) {
        cfg.wordDictionary.clear();
        cfg.SetWordRule(L"спс", AppLayout::Russian);
        cfg.SetWordRule(L"го", AppLayout::Russian);
        cfg.SetWordRule(L"хз", AppLayout::Russian);
        cfg.SetWordRule(L"пж", AppLayout::Russian);
        cfg.SetWordRule(L"мб", AppLayout::Russian);
        cfg.SetWordRule(L"docker", AppLayout::English);
        cfg.SetWordRule(L"key", AppLayout::English);     // collision override ('лун' looks like Russian)
        cfg.SetWordRule(L"ghbdtn", AppLayout::Russian);  // trigger-form directly
        cfg.SetWordRule(L"руддщ", AppLayout::English);   // trigger-form directly
    });

    // Target-form RU word ("спс" -> "cgc")
    TypeWord(det, L"cgc");
    auto sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"спс");

    TypeWord(det, L"Cgc");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"Спс");

    TypeWord(det, L"cgc!");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"спс!");

    TypeWord(det, L"cgc/");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"спс.");

    TypeWord(det, L"спс");
    sw = det.EvaluateWord(L' ');
    CHECK(!sw.shouldSwitch);

    // Target-form EN word ("docker" -> "вщслук")
    TypeWord(det, L"вщслук");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::English && sw.convertedWord == L"docker");

    TypeWord(det, L"Вщслук");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::English && sw.convertedWord == L"Docker");

    TypeWord(det, L"вщслукб");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::English && sw.convertedWord == L"docker,");

    TypeWord(det, L"docker");
    sw = det.EvaluateWord(L' ');
    CHECK(!sw.shouldSwitch);

    // Trigger-form directly
    TypeWord(det, L"ghbdtn");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"привет");

    TypeWord(det, L"руддщ");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::English && sw.convertedWord == L"hello");

    // Short / no-vowel / punctuation-letter custom words
    TypeWord(det, L"[p");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"хз");

    TypeWord(det, L"v,");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"мб");

    TypeWord(det, L"uj");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"го");

    TypeWord(det, L"g;");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::Russian && sw.convertedWord == L"пж");

    // Collision override ("key" -> "лун")
    TypeWord(det, L"лун");
    sw = det.EvaluateWord(L' ');
    CHECK(sw.shouldSwitch && sw.targetLang == WordLanguage::English && sw.convertedWord == L"key");

    // Removing a rule disables the custom switch
    cfgMgr.Update([](AppConfig& cfg) {
        cfg.RemoveWordRule(L"key");
    });
    TypeWord(det, L"лун");
    sw = det.EvaluateWord(L' ');
    CHECK(!sw.shouldSwitch);

    // Built-in heuristics & protection remain intact
    TypeWord(det, L"hello");
    sw = det.EvaluateWord(L' ');
    CHECK(!sw.shouldSwitch);

    TypeWord(det, L"привет");
    sw = det.EvaluateWord(L' ');
    CHECK(!sw.shouldSwitch);

    ++g_testsPassed;
}

} // namespace

int main() {
    TestCapsHoldAndType();
    TestShiftAndCapsSimultaneous();
    TestHoldCapsDigitShiftedSymbol();
    TestHoldTimeoutAndQuickTap();
    TestDoubleCapsAutoRepeatAndWinCaps();
    TestConfigJsonRoundTripAndBackwardCompat();
    TestDetectorCustomDictionaryAndHeuristics();

    std::cout << "All " << g_testsPassed << " test suites passed!" << std::endl;
    return 0;
}
