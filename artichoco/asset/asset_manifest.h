#pragma once
#include "asset_metadata.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arti::asset {

// 打包产物里的 catalog 快照，放在项目根下。
//
// 开发期 catalog 是靠扫 Assets/ 下所有 .meta 建起来的，那要求源文件树在场。发布出去的游戏里
// 没有源文件也没有 .meta，所以打包时把 catalog 拍成这一个文件，运行时直接读它。
//
// 放项目根而不是 artifacts_root 下面：ArtifactsPath 是项目里可配的，拿它的父目录去猜
// Library/ 在哪儿是脆的；而项目根一定存在，.artiproj 就在那儿。
inline constexpr std::string_view kAssetManifestFileName{ "catalog.artimanifest" };
inline constexpr std::uint32_t kAssetManifestVersion{ 1 };

// manifest 只装 AssetOrigin::User 的条目。Engine（builtin）资产的身份是编译期常量，
// 运行时由 ensureBuiltinAssets() 重新登记 —— 写进 manifest 反而会和它撞 handle。
struct AssetManifest final {
    std::uint32_t version{ kAssetManifestVersion };
    std::vector<AssetMetadata> assets;

    bool operator==(const AssetManifest&) const = default;
};

std::optional<std::string> serializeAssetManifest(const AssetManifest& manifest);
// 版本不匹配、有条目校验不过、或者出现重复 handle 都返回 nullopt ——
// 一个坏 manifest 该让游戏起不来，而不是少几个资产悄悄接着跑。
std::optional<AssetManifest> deserializeAssetManifest(std::string_view text);

} // namespace arti::asset
