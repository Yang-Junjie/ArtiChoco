#include "artichoco/asset/asset_importer.h"
#include "artichoco/asset/asset_manager.h"
#include "artichoco/asset/asset_storage.h"
#include "artichoco/project/project_info.h"
#include "artichoco/project/project_manager.h"

#include <cstddef>
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
#include <unordered_map>
#include <utility>
#include <vector>

namespace asset_test {

constexpr std::string_view test_asset_type = "artichoco.example.test_asset";
constexpr std::string_view test_mesh_type = "artichoco.example.test_mesh";
constexpr std::string_view test_material_type = "artichoco.example.test_material";
constexpr std::string_view test_linked_a_type = "artichoco.example.test_linked_a";
constexpr std::string_view test_linked_b_type = "artichoco.example.test_linked_b";
constexpr std::string_view test_cyclic_type = "artichoco.example.test_cyclic";

class TestTextAsset final : public arti::asset::Asset {
public:
    TestTextAsset(arti::core::UUID handle, std::string type, std::string contents)
            : Asset(handle),
              m_type(std::move(type)),
              m_contents(std::move(contents)) {}

    [[nodiscard]] arti::asset::AssetType getType() const override { return m_type; }
    [[nodiscard]] const std::string& getContents() const noexcept { return m_contents; }

private:
    std::string m_type;
    std::string m_contents;
};

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

std::vector<std::byte> toBytes(std::string_view text) {
    return { reinterpret_cast<const std::byte*>(text.data()),
        reinterpret_cast<const std::byte*>(text.data()) + text.size() };
}

std::string readSourceText(const arti::asset::AssetStorage& storage,
        const std::filesystem::path& source_path) {
    const auto file = storage.resolveSourcePath(source_path);
    if (!file) {
        throw std::runtime_error("Failed to resolve the source '" + source_path.string() + "'.");
    }
    return readTextFile(*file);
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

class TestImporter final : public arti::asset::AssetImporter {
public:
    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override {
        return { ".testasset" };
    }

    [[nodiscard]] arti::asset::AssetImportResult import(
            const std::filesystem::path& source_path) override {
        ++m_import_count;
        try {
            const std::string contents = readSourceText(*m_storage, source_path);

            arti::asset::AssetImportOutput output;
            output.metadata.handle = arti::core::UUID::generate();
            output.metadata.type = std::string{ test_asset_type };
            output.metadata.artifact_path = std::filesystem::path{ "Imported" } /
                                            (output.metadata.handle.toString() + ".testartifact");
            output.metadata.properties["importer"] = "asset_test.TestImporter";
            output.metadata.properties["source_size"] = static_cast<uint64_t>(contents.size());
            output.metadata.properties["preserve_text"] = true;
            output.encoded = encode(output.metadata, source_path);

            return { .outputs = { std::move(output) } };
        } catch (const std::exception& exception) {
            return { .error = exception.what() };
        }
    }

    [[nodiscard]] size_t getImportCount() const noexcept { return m_import_count; }

private:
    [[nodiscard]] std::vector<std::byte> encode(const arti::asset::AssetMetadata&,
            const std::filesystem::path& source_path) const override {
        return toBytes(readSourceText(*m_storage, source_path));
    }

    size_t m_import_count{ 0 };
};

class TestModelImporter final : public arti::asset::AssetImporter {
public:
    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override {
        return { ".testmodel" };
    }

    [[nodiscard]] arti::asset::AssetImportResult import(
            const std::filesystem::path& source_path) override {
        ++m_import_count;
        try {
            arti::asset::AssetImportResult result;
            for (uint64_t index = 0; index < 2; ++index) {
                result.outputs.push_back(buildSubAsset(source_path, test_mesh_type,
                        "#mesh_" + std::to_string(index), index));
            }
            for (uint64_t index = 0; index < 2; ++index) {
                result.outputs.push_back(buildSubAsset(source_path, test_material_type,
                        "#material_" + std::to_string(index), index));
            }
            return result;
        } catch (const std::exception& exception) {
            return { .error = exception.what() };
        }
    }

    [[nodiscard]] size_t getImportCount() const noexcept { return m_import_count; }

private:
    [[nodiscard]] arti::asset::AssetImportOutput buildSubAsset(
            const std::filesystem::path& source_path, std::string_view type, std::string suffix,
            uint64_t index) const {
        std::filesystem::path identity = source_path;
        identity += suffix;

        arti::asset::AssetImportOutput output;
        const auto existing =
                m_catalog->findBySourcePathAndType(identity, std::string{ type });
        output.metadata.handle = existing ? existing->handle : arti::core::UUID::generate();
        output.metadata.type = std::string{ type };
        output.metadata.artifact_path = std::filesystem::path{ "Imported" } /
                                        (output.metadata.handle.toString() + ".testsubartifact");
        output.metadata.properties["index"] = index;
        if (type == test_mesh_type) {
            output.metadata.properties["material_slots"] =
                    std::string{ "slot" } + std::to_string(index);
        }
        output.source_suffix = std::move(suffix);
        if (existing && hasCurrentFiles(*existing)) {
            output.already_imported = true;
        } else {
            output.encoded = encode(output.metadata, source_path);
        }
        return output;
    }

    [[nodiscard]] std::vector<std::byte> encode(const arti::asset::AssetMetadata& metadata,
            const std::filesystem::path& source_path) const override {
        const std::string source = readSourceText(*m_storage, source_path);
        const uint64_t index = std::get<uint64_t>(metadata.properties.at("index"));
        return toBytes(source +
                       (metadata.type == test_mesh_type ? " mesh#" : " material#") +
                       std::to_string(index));
    }

    size_t m_import_count{ 0 };
};

class TestLinkedImporter final : public arti::asset::AssetImporter {
public:
    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override {
        return { ".testlinked" };
    }

    [[nodiscard]] arti::asset::AssetImportResult import(
            const std::filesystem::path& source_path) override {
        ++m_import_count;
        try {
            arti::asset::AssetImportResult result;
            const auto a = buildSubAsset(source_path, test_linked_a_type, {});
            result.outputs.push_back(a);
            result.outputs.push_back(buildSubAsset(source_path, test_linked_b_type, "#b",
                    { a.metadata.handle }));
            return result;
        } catch (const std::exception& exception) {
            return { .error = exception.what() };
        }
    }

    [[nodiscard]] size_t getImportCount() const noexcept { return m_import_count; }

private:
    [[nodiscard]] arti::asset::AssetImportOutput buildSubAsset(
            const std::filesystem::path& source_path, std::string_view type, std::string suffix,
            std::vector<arti::core::UUID> dependencies = {}) const {
        std::filesystem::path identity = source_path;
        identity += suffix;

        arti::asset::AssetImportOutput output;
        const auto existing =
                m_catalog->findBySourcePathAndType(identity, std::string{ type });
        output.metadata.handle = existing ? existing->handle : arti::core::UUID::generate();
        output.metadata.type = std::string{ type };
        output.metadata.artifact_path = std::filesystem::path{ "Imported" } /
                                        (output.metadata.handle.toString() + ".testlinkedartifact");
        output.metadata.dependencies = std::move(dependencies);
        output.source_suffix = std::move(suffix);
        if (existing && hasCurrentFiles(*existing)) {
            output.already_imported = true;
        } else {
            output.encoded = encode(output.metadata, source_path);
        }
        return output;
    }

    [[nodiscard]] std::vector<std::byte> encode(const arti::asset::AssetMetadata& metadata,
            const std::filesystem::path& source_path) const override {
        return toBytes(readSourceText(*m_storage, source_path) +
                       (metadata.type == test_linked_a_type ? " linked-a" : " linked-b"));
    }

    size_t m_import_count{ 0 };
};

class TestCyclicImporter final : public arti::asset::AssetImporter {
public:
    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override {
        return { ".testcyclic" };
    }

    [[nodiscard]] arti::asset::AssetImportResult import(
            const std::filesystem::path& source_path) override {
        ++m_import_count;
        try {
            arti::asset::AssetImportOutput output;
            const auto existing = m_catalog->findBySourcePathAndType(
                    source_path, std::string{ test_cyclic_type });
            output.metadata.handle = existing ? existing->handle : arti::core::UUID::generate();
            output.metadata.type = std::string{ test_cyclic_type };
            output.metadata.artifact_path = std::filesystem::path{ "Imported" } /
                                            (output.metadata.handle.toString() +
                                                    ".testcyclicartifact");
            output.metadata.dependencies = { output.metadata.handle };
            if (existing && hasCurrentFiles(*existing)) {
                output.already_imported = true;
            } else {
                output.encoded = encode(output.metadata, source_path);
            }

            return { .outputs = { std::move(output) } };
        } catch (const std::exception& exception) {
            return { .error = exception.what() };
        }
    }

    [[nodiscard]] size_t getImportCount() const noexcept { return m_import_count; }

private:
    [[nodiscard]] std::vector<std::byte> encode(const arti::asset::AssetMetadata&,
            const std::filesystem::path& source_path) const override {
        return toBytes(readSourceText(*m_storage, source_path) + " cyclic");
    }

    size_t m_import_count{ 0 };
};

class TextLoader final : public arti::asset::AssetLoader {
public:
    explicit TextLoader(std::string type)
            : m_type(std::move(type)) {}

    [[nodiscard]] arti::asset::AssetType getType() const override { return m_type; }

    [[nodiscard]] size_t getLoadCount() const noexcept { return m_load_count; }

private:
    [[nodiscard]] std::shared_ptr<arti::asset::Asset> decode(
            const arti::asset::AssetMetadata& metadata,
            const std::filesystem::path& artifact_file,
            std::span<const std::shared_ptr<arti::asset::Asset>>) override {
        ++m_load_count;
        try {
            return std::make_shared<TestTextAsset>(metadata.handle, metadata.type,
                    readTextFile(artifact_file));
        } catch (const std::exception&) {
            return {};
        }
    }

    std::string m_type;
    size_t m_load_count{ 0 };
};

class LinkedLoader final : public arti::asset::AssetLoader {
public:
    explicit LinkedLoader(std::string type)
            : m_type(std::move(type)) {}

    [[nodiscard]] arti::asset::AssetType getType() const override { return m_type; }

private:
    [[nodiscard]] std::shared_ptr<arti::asset::Asset> decode(
            const arti::asset::AssetMetadata& metadata,
            const std::filesystem::path& artifact_file,
            std::span<const std::shared_ptr<arti::asset::Asset>> dependencies) override {
        for (const auto& dependency : dependencies) {
            auto typed = std::dynamic_pointer_cast<asset_test::TestTextAsset>(dependency);
            if (!typed) {
                return {};
            }
            m_dependency = std::move(typed);
        }
        try {
            return std::make_shared<TestTextAsset>(metadata.handle, metadata.type,
                    readTextFile(artifact_file));
        } catch (const std::exception&) {
            return {};
        }
    }

    std::string m_type;
    std::shared_ptr<TestTextAsset> m_dependency;
};

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<const arti::asset::AssetImportOutput*> collectOutputs(
        const std::vector<arti::asset::AssetImportResult>& results, std::string_view type) {
    std::vector<const arti::asset::AssetImportOutput*> outputs;
    for (const arti::asset::AssetImportResult& result : results) {
        require(bool(result), "Import failed: " + result.error);
        for (const arti::asset::AssetImportOutput& output : result.outputs) {
            if (output.metadata.type == type) {
                outputs.push_back(&output);
            }
        }
    }
    return outputs;
}

const arti::asset::AssetMetadata* requireResult(
        const std::vector<arti::asset::AssetImportResult>& results, std::string_view type,
        std::string_view operation) {
    const auto outputs = collectOutputs(results, type);
    if (outputs.empty()) {
        throw std::runtime_error(
                std::string{ operation } + " did not produce the expected asset type.");
    }
    return &outputs.front()->metadata;
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

    const std::filesystem::path asset_source = "Content/example.testasset";
    const std::filesystem::path mesh_source = "Content/example.testmodel";
    const std::filesystem::path linked_source = "Content/example.testlinked";
    const std::filesystem::path cyclic_source = "Content/example.testcyclic";
    const auto asset_file = project_manager.resolveAssetPath(asset_source);
    const auto mesh_file = project_manager.resolveAssetPath(mesh_source);
    const auto linked_file = project_manager.resolveAssetPath(linked_source);
    const auto cyclic_file = project_manager.resolveAssetPath(cyclic_source);
    require(asset_file.has_value() && mesh_file.has_value() && linked_file.has_value() &&
                    cyclic_file.has_value(),
            "ProjectManager did not resolve the source Asset paths.");
    asset_test::writeTextFile(*asset_file, "first revision");
    asset_test::writeTextFile(*mesh_file, "composite source");
    asset_test::writeTextFile(*linked_file, "linked source");
    asset_test::writeTextFile(*cyclic_file, "cyclic source");

    arti::asset::AssetManager assets;

    auto importer = std::make_unique<asset_test::TestImporter>();
    asset_test::TestImporter* importer_view = importer.get();
    require(assets.registerImporter(std::move(importer)),
            "Failed to register the TestImporter.");
    require(!assets.registerImporter(std::make_unique<asset_test::TestImporter>()),
            "A second importer for the same source extension must be rejected.");

    auto model_importer = std::make_unique<asset_test::TestModelImporter>();
    asset_test::TestModelImporter* model_importer_view = model_importer.get();
    require(assets.registerImporter(std::move(model_importer)),
            "Failed to register the model importer.");

    auto linked_importer = std::make_unique<asset_test::TestLinkedImporter>();
    asset_test::TestLinkedImporter* linked_importer_view = linked_importer.get();
    require(assets.registerImporter(std::move(linked_importer)),
            "Failed to register the linked importer.");

    auto cyclic_importer = std::make_unique<asset_test::TestCyclicImporter>();
    asset_test::TestCyclicImporter* cyclic_importer_view = cyclic_importer.get();
    require(assets.registerImporter(std::move(cyclic_importer)),
            "Failed to register the cyclic importer.");

    require(assets.open(*assets_root, *artifacts_root), "Failed to open the AssetManager.");

    auto asset_loader = std::make_unique<asset_test::TextLoader>(std::string{ test_asset_type });
    asset_test::TextLoader* asset_loader_view = asset_loader.get();
    require(assets.registerLoader(std::move(asset_loader)), "Failed to register a loader.");
    require(assets.registerLoader(
                    std::make_unique<asset_test::TextLoader>(std::string{ test_mesh_type })),
            "Failed to register the mesh loader.");
    require(assets.registerLoader(
                    std::make_unique<asset_test::TextLoader>(std::string{ test_material_type })),
            "Failed to register the material loader.");
    require(assets.registerLoader(std::make_unique<asset_test::LinkedLoader>(
                    std::string{ test_linked_a_type })),
            "Failed to register the linked-a loader.");
    require(assets.registerLoader(std::make_unique<asset_test::LinkedLoader>(
                    std::string{ test_linked_b_type })),
            "Failed to register the linked-b loader.");
    require(assets.registerLoader(
                    std::make_unique<asset_test::LinkedLoader>(std::string{ test_cyclic_type })),
            "Failed to register the cyclic loader.");

    asset_test::TestImporter unregistered;
    require(assets.import(asset_source, unregistered).empty(),
            "An unregistered importer should be rejected.");

    std::vector<arti::asset::AssetImportResult> results =
            assets.import(asset_source, *importer_view);
    require(results.size() == 1, "Explicit import should run a single importer.");
    const arti::asset::AssetMetadata* first =
            requireResult(results, test_asset_type, "Initial asset import");
    require(first->handle.isValid() && first->source_path == asset_source &&
                    importer_view->getImportCount() == 1,
            "Initial import returned unexpected metadata.");
    require(std::get<uint64_t>(first->properties.at("source_size")) == 14 &&
                    std::get<bool>(first->properties.at("preserve_text")),
            "Importer custom properties were not preserved.");

    std::filesystem::path metadata_file = *asset_file;
    metadata_file += ".meta";
    const auto artifact_file = assets.storage().resolveArtifactPath(first->artifact_path);
    require(std::filesystem::is_regular_file(metadata_file),
            "The source-side metadata file was not written.");
    require(artifact_file && std::filesystem::is_regular_file(*artifact_file),
            "The declared artifact was not written.");

    const auto found = assets.catalog().find(first->handle);
    require(found && *found == *first, "AssetCatalog lookup did not return the imported asset.");
    require(assets.catalog().findBySourcePath(asset_source).size() == 1,
            "The imported asset was not found by its source path.");

    auto loaded = assets.load<asset_test::TestTextAsset>(first->handle);
    auto cached = assets.load<asset_test::TestTextAsset>(first->handle);
    require(loaded && cached == loaded && loaded->getContents() == "first revision" &&
                    asset_loader_view->getLoadCount() == 1,
            "AssetManager did not load and cache the asset.");
    require(assets.getAsset(first->handle) == loaded,
            "The loaded asset was not stored in the AssetManager.");

    const arti::asset::AssetHandle<asset_test::TestTextAsset> typed_handle{ first->handle };
    require(typed_handle.isValid() && typed_handle.id() == first->handle,
            "The typed handle must wrap the raw handle.");
    require(!arti::asset::AssetHandle<asset_test::TestTextAsset>{}.isValid(),
            "A default typed handle must be invalid.");
    auto typed_loaded = assets.load(typed_handle);
    require(typed_loaded && typed_loaded == loaded,
            "The typed load overload must return the cached Asset.");
    std::unordered_map<arti::asset::AssetHandle<asset_test::TestTextAsset>, int> typed_lookup;
    typed_lookup.emplace(typed_handle, 7);
    require(typed_lookup.contains(typed_handle) && typed_lookup.at(typed_handle) == 7,
            "Typed handles must be usable as map keys.");

    asset_test::writeTextFile(*asset_file, "second revision");
    std::vector<arti::asset::AssetImportResult> reimported =
            assets.import(asset_source, *importer_view);
    const arti::asset::AssetMetadata* reimported_asset =
            requireResult(reimported, test_asset_type, "Asset reimport");
    require(reimported_asset->handle == first->handle &&
                    std::get<uint64_t>(reimported_asset->properties.at("source_size")) == 15 &&
                    importer_view->getImportCount() == 2 && assets.catalog().importedCount() == 1,
            "Reimport did not preserve the asset identity.");
    require(!assets.getAsset(first->handle),
            "Reimport should drop the previously loaded instance.");

    auto reloaded = assets.load<asset_test::TestTextAsset>(first->handle);
    require(reloaded && reloaded->getContents() == "second revision" &&
                    asset_loader_view->getLoadCount() == 2,
            "AssetManager did not reload the updated artifact.");

    loaded.reset();
    cached.reset();
    reloaded.reset();
    typed_loaded.reset();
    require(!assets.getAsset(first->handle),
            "Assets should be released when the last reference is dropped.");
    auto redecoded = assets.load<asset_test::TestTextAsset>(first->handle);
    require(redecoded && redecoded->getContents() == "second revision" &&
                    asset_loader_view->getLoadCount() == 3,
            "Expired cached assets should be re-decoded on the next load.");

    std::vector<arti::asset::AssetImportResult> model_results =
            assets.import(mesh_source, *model_importer_view);
    require(model_results.size() == 1, "Model import should run a single importer.");
    const auto mesh_outputs = collectOutputs(model_results, test_mesh_type);
    const auto material_outputs = collectOutputs(model_results, test_material_type);
    require(mesh_outputs.size() == 2 && material_outputs.size() == 2 &&
                    !mesh_outputs[0]->already_imported && !mesh_outputs[1]->already_imported &&
                    !material_outputs[0]->already_imported &&
                    !material_outputs[1]->already_imported &&
                    model_importer_view->getImportCount() == 1,
            "Model import did not produce the expected sub-assets.");
    require(mesh_outputs[0]->metadata.source_path ==
                    std::filesystem::path{ "Content/example.testmodel#mesh_0" } &&
                    mesh_outputs[1]->metadata.source_path ==
                            std::filesystem::path{ "Content/example.testmodel#mesh_1" },
            "Sub-assets must carry their own source identity.");
    require(std::get<std::string>(mesh_outputs[0]->metadata.properties.at("material_slots")) ==
                    "slot0" &&
                    std::get<std::string>(
                            mesh_outputs[1]->metadata.properties.at("material_slots")) == "slot1",
            "Meshes must declare their material slots.");
    require(assets.catalog().findBySourcePath(mesh_source).empty() &&
                    assets.catalog().findBySourcePath(
                            std::filesystem::path{ "Content/example.testmodel#mesh_0" })
                                    .size() == 1,
            "Sub-asset source paths must be distinct from the plain source path.");

    const auto mesh_artifact = assets.storage().resolveArtifactPath(mesh_outputs[0]->metadata.artifact_path);
    require(mesh_artifact && std::filesystem::is_regular_file(*mesh_artifact) &&
                    asset_test::readTextFile(*mesh_artifact) == "composite source mesh#0",
            "The mesh sub-asset artifact was not written correctly.");

    std::vector<arti::asset::AssetImportResult> model_reimported =
            assets.import(mesh_source, *model_importer_view);
    const auto mesh_outputs_reused = collectOutputs(model_reimported, test_mesh_type);
    const auto material_outputs_reused = collectOutputs(model_reimported, test_material_type);
    require(mesh_outputs_reused.size() == 2 && material_outputs_reused.size() == 2 &&
                    mesh_outputs_reused[0]->already_imported &&
                    mesh_outputs_reused[1]->already_imported &&
                    material_outputs_reused[0]->already_imported &&
                    material_outputs_reused[1]->already_imported &&
                    mesh_outputs_reused[0]->metadata.handle == mesh_outputs[0]->metadata.handle &&
                    mesh_outputs_reused[1]->metadata.handle == mesh_outputs[1]->metadata.handle &&
                    material_outputs_reused[0]->metadata.handle ==
                            material_outputs[0]->metadata.handle &&
                    material_outputs_reused[1]->metadata.handle ==
                            material_outputs[1]->metadata.handle,
            "The importer must reuse existing sub-asset metadata on reimport.");

    std::error_code ignored;
    std::filesystem::remove(mesh_file->string() + "#mesh_0.meta", ignored);
    std::filesystem::remove(mesh_file->string() + "#mesh_1.meta", ignored);
    std::vector<arti::asset::AssetImportResult> model_regenerated =
            assets.import(mesh_source, *model_importer_view);
    const auto mesh_outputs_regenerated = collectOutputs(model_regenerated, test_mesh_type);
    const auto material_outputs_regenerated =
            collectOutputs(model_regenerated, test_material_type);
    require(mesh_outputs_regenerated.size() == 2 &&
                    !mesh_outputs_regenerated[0]->already_imported &&
                    !mesh_outputs_regenerated[1]->already_imported &&
                    material_outputs_regenerated.size() == 2 &&
                    material_outputs_regenerated[0]->already_imported &&
                    material_outputs_regenerated[1]->already_imported,
            "Missing .meta files must trigger a reimport of only the affected sub-assets.");
    require(mesh_outputs_regenerated[0]->metadata.handle == mesh_outputs[0]->metadata.handle &&
                    mesh_outputs_regenerated[1]->metadata.handle == mesh_outputs[1]->metadata.handle,
            "The AssetManager must preserve sub-asset identities via the suffix-aware lookup.");

    require(assets.catalog().find(mesh_outputs[0]->metadata.handle).has_value() &&
                    assets.catalog().find(mesh_outputs[1]->metadata.handle).has_value() &&
                    assets.catalog().find(material_outputs[0]->metadata.handle).has_value() &&
                    assets.catalog().find(material_outputs[1]->metadata.handle).has_value(),
            "Sub-assets were not stored in the catalog.");

    std::vector<arti::asset::AssetImportResult> linked_results =
            assets.import(linked_source, *linked_importer_view);
    require(linked_results.size() == 1, "Linked import should run a single importer.");
    const arti::asset::AssetMetadata* linked_a =
            requireResult(linked_results, test_linked_a_type, "Linked import");
    const arti::asset::AssetMetadata* linked_b =
            requireResult(linked_results, test_linked_b_type, "Linked import");
    require(linked_importer_view->getImportCount() == 1 &&
                    linked_b->dependencies == std::vector<arti::core::UUID>{ linked_a->handle },
            "Linked-b must depend on linked-a.");
    require(assets.catalog().dependentsOf(linked_a->handle) ==
                            std::vector<arti::core::UUID>{ linked_b->handle } &&
                    assets.catalog().dependentsOf(linked_b->handle).empty(),
            "The dependents index must list linked-b for linked-a.");

    auto loaded_a = assets.load<asset_test::TestTextAsset>(linked_a->handle);
    auto loaded_b = assets.load<asset_test::TestTextAsset>(linked_b->handle);
    require(loaded_a && loaded_a->getContents() == "linked source linked-a" && loaded_b &&
                    loaded_b->getContents() == "linked source linked-b",
            "Linked assets did not load recursively.");

    assets.unloadWithDependents(linked_a->handle);
    require(!assets.getAsset(linked_a->handle) && !assets.getAsset(linked_b->handle),
            "unloadWithDependents must drop the dependent asset as well.");
    auto reloaded_b = assets.load<asset_test::TestTextAsset>(linked_b->handle);
    require(reloaded_b && assets.getAsset(linked_a->handle) != nullptr,
            "Loading linked-b must recursively reload linked-a.");

    std::vector<arti::asset::AssetImportResult> linked_again =
            assets.import(linked_source, *linked_importer_view);
    const arti::asset::AssetMetadata* linked_a_again =
            requireResult(linked_again, test_linked_a_type, "Linked reimport");
    const arti::asset::AssetMetadata* linked_b_again =
            requireResult(linked_again, test_linked_b_type, "Linked reimport");
    require(linked_a_again->handle == linked_a->handle &&
                    linked_b_again->handle == linked_b->handle &&
                    linked_b_again->dependencies ==
                            std::vector<arti::core::UUID>{ linked_a_again->handle } &&
                    assets.catalog().dependentsOf(linked_a->handle) ==
                            std::vector<arti::core::UUID>{ linked_b->handle },
            "Linked reimport must preserve handles and the dependency index.");

    std::vector<arti::asset::AssetImportResult> cyclic_results =
            assets.import(cyclic_source, *cyclic_importer_view);
    const arti::asset::AssetMetadata* cyclic =
            requireResult(cyclic_results, test_cyclic_type, "Cyclic import");
    require(cyclic_importer_view->getImportCount() == 1 &&
                    cyclic->dependencies == std::vector<arti::core::UUID>{ cyclic->handle },
            "The cyclic asset must depend on itself.");
    const auto cyclic_dependents = assets.catalog().dependentsOf(cyclic->handle);
    require(cyclic_dependents.size() == 1 && cyclic_dependents[0] == cyclic->handle,
            "The dependents index must tolerate self-dependencies.");
    require(assets.load<asset_test::TestTextAsset>(cyclic->handle) == nullptr,
            "Loading a cyclic asset must fail instead of recursing forever.");

    assets.close();

    arti::asset::AssetManager reopened_assets;
    require(reopened_assets.registerImporter(std::make_unique<asset_test::TestImporter>()) &&
                    reopened_assets.registerImporter(
                            std::make_unique<asset_test::TestModelImporter>()) &&
                    reopened_assets.registerImporter(
                            std::make_unique<asset_test::TestLinkedImporter>()) &&
                    reopened_assets.registerImporter(
                            std::make_unique<asset_test::TestCyclicImporter>()),
            "Failed to register importers after reopening.");
    require(reopened_assets.open(*assets_root, *artifacts_root),
            "Failed to reopen the AssetManager.");
    require(reopened_assets.catalog().importedCount() == 8,
            "The AssetCatalog did not rebuild itself from the persisted metadata files.");
    require(reopened_assets.catalog().find(first->handle).has_value() &&
                    reopened_assets.catalog().find(mesh_outputs[0]->metadata.handle).has_value() &&
                    reopened_assets.catalog().find(mesh_outputs[1]->metadata.handle).has_value() &&
                    reopened_assets.catalog().find(material_outputs[0]->metadata.handle).has_value() &&
                    reopened_assets.catalog().find(material_outputs[1]->metadata.handle).has_value() &&
                    reopened_assets.catalog().find(linked_a->handle).has_value() &&
                    reopened_assets.catalog().find(linked_b->handle).has_value() &&
                    reopened_assets.catalog().find(cyclic->handle).has_value(),
            "Restored handles do not match the imported ones.");
    require(reopened_assets.catalog().dependentsOf(linked_a->handle) ==
                    std::vector<arti::core::UUID>{ linked_b->handle },
            "Dependencies must be restored from the persisted metadata files.");

    require(reopened_assets.registerLoader(
                    std::make_unique<asset_test::TextLoader>(std::string{ test_asset_type })) &&
                    reopened_assets.registerLoader(std::make_unique<asset_test::TextLoader>(
                            std::string{ test_mesh_type })) &&
                    reopened_assets.registerLoader(std::make_unique<asset_test::TextLoader>(
                            std::string{ test_material_type })) &&
                    reopened_assets.registerLoader(std::make_unique<asset_test::LinkedLoader>(
                            std::string{ test_linked_a_type })) &&
                    reopened_assets.registerLoader(std::make_unique<asset_test::LinkedLoader>(
                            std::string{ test_linked_b_type })) &&
                    reopened_assets.registerLoader(std::make_unique<asset_test::LinkedLoader>(
                            std::string{ test_cyclic_type })),
            "Failed to register loaders after reopening.");

    const auto restored_asset = reopened_assets.load<asset_test::TestTextAsset>(first->handle);
    const auto restored_mesh =
            reopened_assets.load<asset_test::TestTextAsset>(mesh_outputs[0]->metadata.handle);
    const auto restored_b = reopened_assets.load<asset_test::TestTextAsset>(linked_b->handle);
    require(restored_asset && restored_asset->getContents() == "second revision" &&
                    restored_mesh && restored_mesh->getContents() == "composite source mesh#0" &&
                    restored_b && reopened_assets.getAsset(linked_a->handle) != nullptr,
            "Restored assets could not be loaded after reopening the project.");
}

} // namespace

} // namespace asset_test

int main() {
    try {
        asset_test::runAssetTest();
        std::cout << "Asset manager test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Asset test failed: " << exception.what() << '\n';
        return 1;
    }
}
