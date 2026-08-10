#include "scene_serialization_registry.h"
#include "component/id_serialization.h"
#include "component/parent_serialization.h"
#include "component/tag_serialization.h"
#include "component/transform_serialization.h"
#include "scene_log.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace arti::scene {

SceneSerializationRegistry::SceneSerializationRegistry()
{
    registerComponent<IDComponent>(std::string{IDSerialization::typeName()}, std::make_unique<IDSerialization>());
    registerComponent<TagComponent>(std::string{TagSerialization::typeName()}, std::make_unique<TagSerialization>());
    registerComponent<ParentComponent>(
            std::string{ParentSerialization::typeName()}, std::make_unique<ParentSerialization>());
    registerComponent<TransformComponent>(
            std::string{TransformSerialization::typeName()}, std::make_unique<TransformSerialization>());
}

void SceneSerializationRegistry::registerEntry(std::unique_ptr<Entry> entry)
{
    if (!entry) {
        throw std::invalid_argument("A component serialization entry cannot be null.");
    }
    const std::string type_name{entry->typeName()};
    if (m_by_name.contains(type_name)) {
        getLogChannel().warn("Rejected duplicate component serialization type name: {}", type_name);
        throw std::logic_error("A component serialization type name is already registered.");
    }
    if (m_by_component_type.contains(entry->componentType())) {
        getLogChannel().warn("Rejected duplicate component serialization for type name: {}", type_name);
        throw std::logic_error("A component serialization type is already registered.");
    }

    Entry* raw_entry = entry.get();
    m_entries.push_back(std::move(entry));
    m_by_name.emplace(type_name, raw_entry);
    m_by_component_type.emplace(raw_entry->componentType(), raw_entry);
    getLogChannel().debug("Registered component serialization '{}'", type_name);
}

const SceneSerializationRegistry::Entry*
    SceneSerializationRegistry::findEntry(std::string_view type_name) const noexcept
{
    const auto found = m_by_name.find(std::string(type_name));
    return found == m_by_name.end() ? nullptr : found->second;
}

} // namespace arti::scene
