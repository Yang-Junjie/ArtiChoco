#pragma once

#include "asset_metadata.h"
#include "asset_settings.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace arti::asset {

class AssetCatalog;
class AssetStorage;

struct AssetImportOutput {
    AssetRecord record;
    std::vector<std::byte> encoded{};
    bool already_imported{ false };
};

struct AssetImportResult {
    std::vector<AssetImportOutput> outputs;
    std::string error{};

    explicit operator bool() const noexcept { return error.empty(); }
};

// import() 的入参。settings 已经按 default → inferred → authored 解析完，
// importer 直接取值，不需要 fallback 分支。
struct AssetImportRequest final {
    std::filesystem::path source_path;
    const ResolvedSettings* settings{ nullptr };
};

// 一条设置推断：容器对它引用的源文件的设置有意见。
// 例如 glTF 知道某张图绑在 normalTexture 槽上，因此应该是 linear ——
// TextureImporter 单看一张 jpg 是无从判断的。
struct InferenceSuggestion final {
    std::filesystem::path target_source;  // 建议给谁（Assets-relative）
    std::string key;
    Value value;
    std::string usage;  // "normal_texture"，给 UI 解释理由
};

// prescan() 的产物。只读文件头，不解几何、不解码图片 —— 它在 plan 阶段跑，
// 必须便宜。
struct SourcePrescan final {
    // 我引用了这些源文件的产出。planner 用它排拓扑序，保证被引用者先导入。
    std::vector<std::filesystem::path> referenced_sources;
    // 我对这些源文件的设置有意见。
    std::vector<InferenceSuggestion> suggestions;
};

class AssetImporter {
public:
    virtual ~AssetImporter() = default;

    virtual std::vector<std::string> getSupportedExtensions() const = 0;
    // 写进 sidecar 的 Importer.Name。
    virtual std::string getName() const = 0;
    // 写进 sidecar 的 Importer.Version。改了导入算法就 bump。
    // 目前只写不读 —— 变更检测排在多线程之后。
    virtual uint32_t getVersion() const { return 1; }

    // 这个 importer 支持哪些导入设置。schema 是权威：解析后每个键都保证存在
    // 且类型正确，所以 import() 里不需要写默认值 fallback。
    virtual std::vector<SettingDescriptor> getSettingSchema() const { return {}; }

    // 在 plan 阶段被调用，声明跨源引用和设置推断。默认什么都不声明。
    // 必须便宜：只读文件头，不解几何、不解码图片。抛异常等价于"什么都没声明"。
    virtual SourcePrescan prescan(const std::filesystem::path& source_path) const {
        (void) source_path;
        return {};
    }

    virtual AssetImportResult import(const AssetImportRequest& request) = 0;

    void setWorkspace(AssetStorage& storage, AssetCatalog& catalog) noexcept {
        m_storage = &storage;
        m_catalog = &catalog;
    }

protected:
    bool hasCurrentFiles(const AssetMetadata& metadata) const;

    AssetStorage* m_storage{ nullptr };
    AssetCatalog* m_catalog{ nullptr };
};

}
