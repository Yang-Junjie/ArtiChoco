#include "artichoco/asset/asset_database.h"
#include "artichoco/asset/asset_manager.h"
#include "artichoco/project/project_info.h"
#include "artichoco/project/project_manager.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace asset_test {

class TestAsset final : public arti::asset::Asset {
public:
    TestAsset(arti::core::UUID handle, std::string contents)
            : Asset(handle),
              m_contents(std::move(contents)) {}

    [[nodiscard]] arti::asset::AssetType getType() const override;
    [[nodiscard]] const std::string& getContents() const noexcept { return m_contents; }

private:
    std::string m_contents;
};

} // namespace asset_test

namespace arti::asset {

template<>
struct AssetTraits<asset_test::TestAsset> {
    static constexpr std::string_view name = "artichoco.example.test_asset";
};

} // namespace arti::asset

namespace asset_test {
namespace {

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input{ path, std::ios::binary };
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open '" + path.string() + "' for reading.");
    }

    std::string contents{ std::istreambuf_iterator<char>{ input },
        std::istreambuf_iterator<char>{} };
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed while reading '" + path.string() + "'.");
    }
    return contents;
}

void writeTextFile(const std::filesystem::path& path, std::string_view contents) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        throw std::runtime_error(
                "Failed to create '" + path.parent_path().string() + "': " + error.message());
    }

    std::ofstream output{ path, std::ios::binary | std::ios::trunc };
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open '" + path.string() + "' for writing.");
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output.good()) {
        throw std::runtime_error("Failed while writing '" + path.string() + "'.");
    }
}

class TemporaryProjectDirectory final {
public:
    TemporaryProjectDirectory() {
        std::error_code error;
        const std::filesystem::path temporary_root = std::filesystem::temp_directory_path(error);
        if (error) {
            throw std::runtime_error(
                    "Failed to resolve the temporary directory: " + error.message());
        }

        m_path = temporary_root /
                 ("artichoco_asset_test_" + arti::core::UUID::generate().toString());
        std::filesystem::create_directories(m_path, error);
        if (error) {
            throw std::runtime_error("Failed to create the temporary project: " + error.message());
        }
    }

    ~TemporaryProjectDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }

    TemporaryProjectDirectory(const TemporaryProjectDirectory&) = delete;
    TemporaryProjectDirectory& operator=(const TemporaryProjectDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace

arti::asset::AssetType TestAsset::getType() const { return arti::asset::assetType<TestAsset>(); }

class TestImporter final : public arti::asset::AssetImporter {
public:
    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override {
        return { ".testasset" };
    }

    [[nodiscard]] arti::asset::AssetImportResult import(
            const arti::asset::AssetImportContext& context) override {
        ++m_import_count;
        try {
            const std::string source_contents = readTextFile(context.source_file);
            arti::asset::AssetMetadata metadata = context.metadata;
            metadata.type = arti::asset::assetType<TestAsset>();
            metadata.source_path = context.source_path;
            metadata.artifact_path = std::filesystem::path{ "Imported" } /
                                     (metadata.handle.toString() + ".testartifact");
            metadata.properties["importer"] = "asset_test.TestImporter";
            metadata.properties["source_size"] = static_cast<uint64_t>(source_contents.size());
            metadata.properties["reimporting"] = context.reimporting;
            metadata.properties["preserve_text"] = true;

            writeTextFile(context.artifacts_root / metadata.artifact_path, source_contents);
            return { .metadata = std::move(metadata) };
        } catch (const std::exception& exception) {
            return { .error = exception.what() };
        }
    }

    [[nodiscard]] size_t getImportCount() const noexcept { return m_import_count; }

private:
    size_t m_import_count{ 0 };
};

class TestLoader final : public arti::asset::AssetLoader {
public:
    [[nodiscard]] arti::asset::AssetType getType() const override {
        return arti::asset::assetType<TestAsset>();
    }

    [[nodiscard]] std::shared_ptr<arti::asset::Asset> load(
            const arti::asset::AssetLoadContext& context) override {
        ++m_load_count;
        try {
            return std::make_shared<TestAsset>(context.metadata.handle,
                    readTextFile(context.artifact_file));
        } catch (const std::exception&) {
            return {};
        }
    }

    [[nodiscard]] size_t getLoadCount() const noexcept { return m_load_count; }

private:
    size_t m_load_count{ 0 };
};

} // namespace asset_test

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const arti::asset::AssetMetadata& requireImport(const arti::asset::AssetImportResult& result,
        std::string_view operation) {
    if (!result) {
        throw std::runtime_error(std::string{ operation } + " failed: " + result.error);
    }
    return *result.metadata;
}

void runAssetTest() {
    asset_test::TemporaryProjectDirectory temporary_project;

    arti::project::ProjectInfo project_info;
    project_info.name = "AssetTest";
    project_info.author = "ArtiChoco";

    arti::project::ProjectManager project_manager;
    require(project_manager.createProject(temporary_project.path(), project_info),
            "Failed to create the temporary ArtiChoco project.");

    const auto assets_root = project_manager.getAssetsRootPath();
    const auto artifacts_root = project_manager.getArtifactsRootPath();
    require(assets_root && artifacts_root,
            "ProjectManager did not expose the Asset workspace roots.");

    const std::filesystem::path source_path = "Content/example.testasset";
    const auto source_file = project_manager.resolveAssetPath(source_path);
    require(source_file.has_value(), "ProjectManager did not resolve the source Asset path.");
    asset_test::writeTextFile(*source_file, "first revision");

    arti::asset::AssetDatabase database;
    auto importer = std::make_unique<asset_test::TestImporter>();
    asset_test::TestImporter* importer_view = importer.get();
    require(database.registerImporter(std::move(importer)),
            "Failed to register the external TestImporter.");
    require(database.open(*assets_root, *artifacts_root), "Failed to open the AssetDatabase.");

    arti::asset::AssetImportResult first_result = database.import(source_path);
    const arti::asset::AssetMetadata first_metadata = requireImport(first_result, "Initial import");
    require(first_metadata.handle.isValid() &&
                    first_metadata.type == arti::asset::assetType<asset_test::TestAsset>() &&
                    first_metadata.source_path == source_path &&
                    importer_view->getImportCount() == 1,
            "Initial import returned unexpected AssetMetadata.");
    require(first_metadata.properties.at("source_size").get<uint64_t>() == 14 &&
                    !first_metadata.properties.at("reimporting").get<bool>(),
            "Importer custom metadata was not preserved.");

    std::filesystem::path metadata_file = *source_file;
    metadata_file += ".meta";
    const auto artifact_file = database.resolveArtifactPath(first_metadata.artifact_path);
    require(std::filesystem::is_regular_file(metadata_file),
            "The source-side .meta file was not written.");
    require(artifact_file && std::filesystem::is_regular_file(*artifact_file),
            "The declared Artifact was not written.");
    require(database.find(first_metadata.handle) == first_metadata &&
                    database.findBySourcePath(source_path) == first_metadata,
            "AssetDatabase lookup did not return the imported Asset.");

    arti::asset::AssetManager assets{ database };
    auto loader = std::make_unique<asset_test::TestLoader>();
    asset_test::TestLoader* loader_view = loader.get();
    require(assets.registerLoader(std::move(loader)),
            "Failed to register the external TestLoader.");

    auto loaded = assets.load<asset_test::TestAsset>(first_metadata.handle);
    auto cached = assets.load<asset_test::TestAsset>(first_metadata.handle);
    require(loaded && cached == loaded && loaded->getContents() == "first revision" &&
                    loader_view->getLoadCount() == 1,
            "AssetManager did not load and cache the TestAsset.");

    asset_test::writeTextFile(*source_file, "second revision");
    arti::asset::AssetImportResult second_result = database.import(source_path);
    const arti::asset::AssetMetadata second_metadata = requireImport(second_result, "Reimport");
    require(second_metadata.handle == first_metadata.handle &&
                    second_metadata.properties.at("reimporting").get<bool>() &&
                    importer_view->getImportCount() == 2 && database.size() == 1,
            "Reimport did not preserve the Asset identity.");

    assets.unload(second_metadata.handle);
    loaded.reset();
    cached.reset();
    auto reloaded = assets.load<asset_test::TestAsset>(second_metadata.handle);
    require(reloaded && reloaded->getContents() == "second revision" &&
                    loader_view->getLoadCount() == 2,
            "AssetManager did not reload the updated Artifact.");

    database.close();
    arti::asset::AssetDatabase reopened_database;
    require(reopened_database.open(*assets_root, *artifacts_root) &&
                    reopened_database.find(first_metadata.handle) == second_metadata,
            "AssetDatabase did not rebuild itself from the persisted .meta file.");

    arti::asset::AssetManager reopened_assets{ reopened_database };
    require(reopened_assets.registerLoader(std::make_unique<asset_test::TestLoader>()),
            "Failed to register a Loader after reopening the project.");
    const auto restored = reopened_assets.load<asset_test::TestAsset>(first_metadata.handle);
    require(restored && restored->getContents() == "second revision",
            "A restored UUID could not be loaded after reopening the project.");
}

} // namespace

int main() {
    try {
        runAssetTest();
        std::cout << "Asset database and manager test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Asset test failed: " << exception.what() << '\n';
        return 1;
    }
}
