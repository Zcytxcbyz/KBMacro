#pragma once

#include <string>
#include <string_view>

#include "common/Types.hpp"

namespace utils {

    // Hotkey → 显示字符串，如 "Ctrl+Alt+F1"
    // 修饰键顺序固定：Ctrl、Shift、Alt、Win
    // 空热键返回空串；只有修饰键（vk == 0）返回 "Ctrl" 之类
    [[nodiscard]] std::wstring hotkeyToString(const Hotkey &hk);

    // 虚拟键码 → 键名，如 VK_F1 → "F1"
    // 未知键返回 "?"，vk == 0 返回空串
    [[nodiscard]] std::wstring vkToString(unsigned vk);

    // 校验热键是否有效
    //   requireMainKey == true  必须有主键（vk != 0），用于全局热键
    //   requireMainKey == false 允许纯修饰键或纯主键
    // 修饰键和主键都为空 → false
    [[nodiscard]] bool isValidHotkey(const Hotkey &hk, bool requireMainKey);

    // 判断某个虚拟键码是否是修饰键本身
    [[nodiscard]] bool isModifierKey(unsigned vk);

    // 修饰键虚拟码 → hk::kMod* 位掩码；非修饰键返回 0
    [[nodiscard]] unsigned modifierBitForVk(unsigned vk);

} // namespace utils
