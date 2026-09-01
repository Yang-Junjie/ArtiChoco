#pragma once

#include <filesystem>

namespace arti::core {

// 本进程可执行文件所在的目录。第一次调用时解析，之后返回缓存。
//
// 存在的理由是「exe 旁边的资源优先于构建期注入的源码树路径」：着色器和编辑器字体都走这条
// 查找顺序，否则构建产物一离开这台机器（或者源码树挪了窝）就跑不起来。
//
// 刻意不用 argv[0]：通过 PATH 启动时它可能只是一个文件名，推不出目录。
// 也不用 std::filesystem::current_path()：双击 exe 时 cwd 不是 exe 所在目录，把文件拖到
// exe 图标上时更不是。
//
// 解析失败抛 std::runtime_error —— 拿不到 exe 目录之后所有相对它的路径都是错的，静默兜底
// 只会把错误推迟到「为什么找不到 shader」那一步，那时已经看不出根因。
const std::filesystem::path& executableDir();

} // namespace arti::core
