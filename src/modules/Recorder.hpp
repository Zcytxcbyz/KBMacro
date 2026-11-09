#pragma once

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/ITimer.hpp"

// 键鼠录制 —— 低级钩子采集端。
//
// 线程契约：
//   run() 必须跑在它自己的线程里（ThreadManager 的工作线程）。低级钩子只能
//   装在"自己泵消息"的线程上：如果装在 UI 线程，StopRecord 的阻塞 join 会让
//   钩子回调无人分发，系统等满 LowLevelHooksTimeout（默认 300ms）后把钩子
//   静默卸掉，之后录不到任何东西。
//
// 输入契约：
//   opts 由调用方（UI 线程）从 ConfigManager + 热键配置读好后传进来；这里
//   不读单例、不读全局状态，线程内部只认这一个参数结构。
//
// 产物契约：
//   - 被 stop 打断、达到动作上限、一个事件都没录到，都是正常返回（不抛）
//   - 钩子安装失败抛（wil::ResultException），由 ThreadManager 记 Failed
//   - 返回空表 = 没录到动作，由面板决定提示什么
//
// 约定：本模块不认识 ThreadManager、不碰 UI；日志只有头尾两条 + 每秒一条进度。
namespace recorder {

    // 录制参数。全部由 UI 线程填好，录制线程里只读。
    struct RecordOptions {
        int startDelayMs = 200;      // 装钩子前先等这么久，避开开始热键自身的按键
        int moveIntervalMs = 16;     // 鼠标移动采样间隔；0 = 不采样（全记）
        bool ignoreInjected = false; // 连别的程序注入的事件也不录（自家的永远不录）
        int maxActions = 100000;     // 动作上限；0 = 不限

        // 开始/停止热键的主键与修饰键，用于把热键自身从产物里剔除（0 = 未配置）
        unsigned startKeyVk = 0;
        unsigned startMods = 0;
        unsigned stopKeyVk = 0;
        unsigned stopMods = 0;
    };

    // 一直录到被 stop 打断或达到上限，返回归一化后的动作序列（可能为空）。
    [[nodiscard]] std::vector<Action> run(const RecordOptions &opts, const ITimer &timer,
                                          const std::stop_token &st);

} // namespace recorder
