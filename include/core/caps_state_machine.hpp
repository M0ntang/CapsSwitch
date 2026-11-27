#pragma once

#include <windows.h>
#include <cstdint>
#include "core/layout_engine.hpp"

namespace Core {

struct CapsConfig {
    bool capsLockSwitch = true;
    bool capsHoldSwitch = true;
    bool doubleCapsConvert = true;
};

struct CapsAction {
    bool suppressKey = false;
    bool switchLayout = false;
    WORD targetLang = 0;        // 0 = none, LANG_ID_EN or LANG_ID_RU
    bool triggerDoubleCaps = false;
    bool resetBuffer = false;
    bool skipDetectorForKey = false;
};

class CapsLockStateMachine {
public:
    static constexpr uint64_t kHoldThresholdMs = 200;
    static constexpr uint64_t kDoubleTapMinMs = 30;
    static constexpr uint64_t kDoubleTapMaxMs = 450;

    CapsAction OnCapsDown(
        uint64_t nowMs,
        bool isShift,
        bool isCtrl,
        bool isAlt,
        bool isWin,
        bool isPaused,
        WORD currentLang,
        const CapsConfig& cfg
    ) {
        if (isPaused) {
            return {};
        }

        // Win + Caps Lock toggles real Windows Caps Lock natively
        if (isWin && !isCtrl && !isAlt) {
            if (!winCapsPassthrough_) {
                winCapsPassthrough_ = true;
                return { false, false, 0, false, true, false };
            }
            return { true, false, 0, false, false, false };
        }

        if (isCtrl || isAlt || isWin) {
            return {};
        }

        if (!(cfg.capsLockSwitch || cfg.capsHoldSwitch || cfg.doubleCapsConvert)) {
            return {};
        }

        // Filter auto-repeat while Caps Lock is held
        if (capsPressed_) {
            return { true, false, 0, false, false, false };
        }

        capsPressed_ = true;
        pressTimeMs_ = nowMs;
        holdSwitched_ = false;
        usedWhileHeld_ = isShift;
        bufferResetForHold_ = false;

        // Check quick double-tap only for bare Caps Lock
        if (!isShift && cfg.doubleCapsConvert) {
            uint64_t diff = (lastTapMs_ != 0 && nowMs >= lastTapMs_)
                                ? (nowMs - lastTapMs_)
                                : UINT64_MAX;
            if (diff >= kDoubleTapMinMs && diff <= kDoubleTapMaxMs) {
                lastTapMs_ = 0;
                return { true, false, 0, true, false, false };
            }
        }

        lastTapMs_ = isShift ? 0 : nowMs;

        if (cfg.capsLockSwitch || cfg.capsHoldSwitch) {
            bool isRu = ((currentLang & 0xFF) == 0x19);
            origLang_ = isRu ? LANG_ID_RU : LANG_ID_EN;
            targetLang_ = isRu ? LANG_ID_EN : LANG_ID_RU;
            holdSwitched_ = true;
            return { true, true, targetLang_, false, false, false };
        }

        return { true, false, 0, false, false, false };
    }

    CapsAction OnCapsUp(uint64_t nowMs, bool isPaused, const CapsConfig& cfg) {
        if (winCapsPassthrough_) {
            winCapsPassthrough_ = false;
            return {};
        }

        if (capsPressed_) {
            capsPressed_ = false;
            if (holdSwitched_) {
                holdSwitched_ = false;
                uint64_t heldMs = (nowMs >= pressTimeMs_) ? (nowMs - pressTimeMs_) : 0;
                bool shouldRevert = !isPaused && cfg.capsHoldSwitch &&
                    (usedWhileHeld_ || heldMs >= kHoldThresholdMs || !cfg.capsLockSwitch);
                if (shouldRevert) {
                    lastTapMs_ = 0;
                    return { true, true, origLang_, false, true, false };
                }
            }
            return { true, false, 0, false, false, false };
        }

        return {};
    }

    CapsAction OnOtherKeyDown(DWORD vkCode, bool isPaused, const CapsConfig& cfg) {
        if (!capsPressed_) {
            return {};
        }

        usedWhileHeld_ = true;
        lastTapMs_ = 0;

        if (isPaused || !cfg.capsHoldSwitch) {
            return {};
        }

        bool isShiftKey = (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT);
        CapsAction act;
        act.skipDetectorForKey = true;
        if (!isShiftKey && !bufferResetForHold_) {
            bufferResetForHold_ = true;
            act.resetBuffer = true;
        }
        return act;
    }

    void Reset() {
        capsPressed_ = false;
        winCapsPassthrough_ = false;
        holdSwitched_ = false;
        usedWhileHeld_ = false;
        bufferResetForHold_ = false;
        origLang_ = LANG_ID_EN;
        targetLang_ = LANG_ID_RU;
        pressTimeMs_ = 0;
        lastTapMs_ = 0;
    }

    bool IsCapsPressed() const { return capsPressed_; }

private:
    bool capsPressed_ = false;
    bool winCapsPassthrough_ = false;
    bool holdSwitched_ = false;
    bool usedWhileHeld_ = false;
    bool bufferResetForHold_ = false;
    WORD origLang_ = LANG_ID_EN;
    WORD targetLang_ = LANG_ID_RU;
    uint64_t pressTimeMs_ = 0;
    uint64_t lastTapMs_ = 0;
};

} // namespace Core
