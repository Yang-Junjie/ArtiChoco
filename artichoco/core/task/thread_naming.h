#pragma once

namespace arti::core::detail {

// 给**当前**线程起名。名字纯粹是诊断设施（调试器的线程窗口、profiler 的泳道），
// 所以起不上就静默放过 —— 不该让一个观测手段影响启动。
//
// 只有 Windows 一支实现（`SetThreadDescription`）。这里用 `#ifdef` 而不是照
// `Tools/platform` 的惯例在 CMake 层按平台选源文件：那套分派是为一整组平台接口准备的，
// 为这两行建一套不成比例。真到了要支持第二个平台并且这里长胖的时候再拆。
void setCurrentThreadName(const char* name);

} // namespace arti::core::detail
