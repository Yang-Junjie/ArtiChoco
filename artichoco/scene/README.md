# Scene Architecture

The Scene module is an EnTT-based entity/component runtime. `Scene` is its public
facade and aggregate root: application code uses it to create entities, manage the
hierarchy, query components, and run systems without accessing the underlying
`entt::registry`.

```text
Application code
      |
      v
    Scene
      |-- SceneEntityStorage  entity lifetime, registry, UUID index
      |-- SceneHierarchy      parent relationships and world transforms
      `-- SceneSystemManager  system ownership and staged execution

    SceneCloner               whole-scene component copying
    SceneSerializer           YAML and file persistence
      `-- SceneSerializationRegistry
                               component serialization policies
```

`SceneEntityStorage`, `SceneHierarchy`, and `SceneSystemManager` have public
methods because they are concrete collaborators, but they should be treated as
implementation classes. Normal engine and application code should depend on
`Scene`, `Entity`, `SceneSystem`, `SceneSerializer`, and
`SceneSerializationRegistry`.

## Responsibilities

| Type | Responsibility |
| --- | --- |
| `Scene` | Stable facade, composition root, and coordinator for entity, hierarchy, clone, and system operations. |
| `Entity` | Non-owning handle combining an `entt::entity` with its registry. Provides checked component access. |
| `SceneEntityStorage` | Owns the registry, enforces unique UUIDs, and maintains the UUID-to-entity index. |
| `SceneHierarchy` | Owns parent invariants, rejects cycles, calculates world transforms, and collects subtrees. |
| `SceneSystemManager` | Owns systems and coordinates attach, detach, enablement, and stage execution. |
| `SceneCloner` | Copies registered component types between scenes while rebuilding destination entity handles. |
| `SceneSerializer` | Converts scenes to and from YAML and files using a serialization registry. |
| `SceneSerializationRegistry` | Maps stable serialized type names and C++ component types to serialization policies. |
| `SceneSystem` | Base class for scene-aware lifecycle and update behavior. |
| `ComponentSerialization<T>` | Strategy interface for serializing one component type. |

## Public Scene API

### Entity lifetime and lookup

```cpp
Entity createEntity(std::string tag = "Entity");
Entity createEntityWithUUID(core::UUID id, std::string tag = "Entity");
void destroyEntity(Entity entity);
void clearEntities();

Entity findEntity(core::UUID id) noexcept;
Entity findEntityByTag(std::string_view tag) noexcept;
bool containsEntity(core::UUID id) const noexcept;
bool isValid(Entity entity) const noexcept;
```

UUIDs are unique within a scene. `destroyEntity()` destroys the selected entity
and all of its descendants. An `Entity` is only valid while its registry and
underlying EnTT entity remain alive.

### Hierarchy and transforms

```cpp
void setParent(Entity child, Entity parent);
void detachFromParent(Entity entity);
Entity getParent(Entity entity) noexcept;
std::vector<Entity> getChildren(Entity entity);

const glm::mat4& getWorldTransform(Entity entity) const;
void updateWorldTransforms();
```

Use these functions instead of editing `ParentComponent` directly. Parent
assignments validate scene ownership, reject self-parenting and cycles, and mark
derived transforms for recalculation.

World transforms are updated before every `runSystems()` call. Code that changes
a local `TransformComponent` and reads a world transform before system execution
must call `updateWorldTransforms()` explicitly. An update visits the scene once,
but only recalculates branches whose local transform, applied parent, dirty state,
or ancestor result changed.

### Components and views

```cpp
auto entity = scene.createEntity("Camera");
auto& transform = entity.getComponent<TransformComponent>();
transform.translation = { 0.0f, 2.0f, 5.0f };

for (auto [handle, local, camera] :
        scene.view<TransformComponent, CameraComponent>().each()) {
    // Application-owned components remain mutable in a mutable Scene view.
}
```

Every entity is created with these required components:

| Component | Meaning | Mutability |
| --- | --- | --- |
| `IDComponent` | Persistent UUID used by lookup and serialization. | Scene-owned, read-only. |
| `TagComponent` | Human-readable entity name. | Mutable. |
| `TransformComponent` | Local translation, rotation, and scale. | Mutable. |
| `ParentComponent` | Parent UUID. | Scene-owned, read-only. |
| `WorldTransformComponent` | Cached derived transform state. | Scene-owned, read-only. |

Required components cannot be removed. `IDComponent`, `ParentComponent`, and
`WorldTransformComponent` cannot be obtained through mutable `Entity` component
access. A mutable `Scene::view<T...>()` automatically exposes those three types as
`const`, so existing read-only queries do not need to spell `const` explicitly.

New entities already contain a `ParentComponent`. Establish a relationship with
`Scene::setParent()`; do not call `addComponent<ParentComponent>()`.

### Systems

Systems derive from `SceneSystem` and may implement `onAttach`, `onDetach`, and
`onUpdate`. They are assigned one execution stage:

```cpp
enum class SystemStage {
    FixedUpdate,
    Update,
    LateUpdate,
    RenderExtract
};

scene.addSystem<MovementSystem>(SystemStage::Update, constructor_args...);
scene.setSystemEnabled<MovementSystem>(true);
scene.runSystems(SystemStage::Update, update_context);
```

The facade also provides `hasSystem<T>()`, `getSystem<T>()`, `removeSystem<T>()`,
and `isSystemEnabled<T>()`. Only one system of each concrete type may be attached
to a scene. Adding or removing systems during execution or lifecycle callbacks is
rejected, as is nested system execution.

`UpdateContext` carries `deltaTime`, `fixedDeltaTime`, and `frameIndex`. Systems
are detached in reverse registration order when their manager is destroyed.

## Cloning

`copyEntitiesFrom()` replaces the destination's entities; it is not an append
operation. Systems already owned by the destination scene are unaffected.

```cpp
Scene::registerComponentCopy<MyComponent>();

Scene destination;
destination.copyEntitiesFrom(source);
```

The five built-in components are registered automatically. Each custom component
that must survive cloning must be copy-constructible and registered before the
copy. A populated but unregistered component storage is skipped with a warning.
Component-copy registration is process-wide.

## Serialization

`SceneSerializationRegistry` automatically registers serializers for
`IDComponent`, `TagComponent`, `ParentComponent`, and `TransformComponent`.
`WorldTransformComponent` is derived state and is rebuilt after loading rather
than serialized.

```cpp
SceneSerializationRegistry registry;
registry.registerComponent<MyComponent>(
        "engine.my_component",
        std::make_unique<MyComponentSerialization>());

SceneSerializer serializer{ registry };
serializer.save(scene, path);
serializer.load(path, scene);
```

A custom policy derives from `ComponentSerialization<MyComponent>` and implements
`serialize(const MyComponent&)` and `deserialize(const YAML::Node&)`. Serialized
type names are persistent file-format identifiers and must remain unique and
stable across C++ renames.

Deserialization builds and validates a staging scene first. It validates required
components, UUID uniqueness, parent references, and hierarchy cycles, then rebuilds
world transforms and replaces the destination storage. A failed load therefore
does not partially overwrite the destination scene.

## Extension checklist

For a new component type:

1. Add and query it normally through `Entity` or `Scene::view()`.
2. Register it with `Scene::registerComponentCopy<T>()` if scene cloning must keep it.
3. Implement `ComponentSerialization<T>` and register a stable type name if it must be persisted.
4. Keep resource ownership and runtime-only data out of serialization unless the file format has an explicit representation for them.

Clone registration and serialization registration are intentionally separate:
some runtime components are copyable but not persistent, while other projects may
choose different persistence policies for the same ECS component set.

## Diagnostics

Scene diagnostics use the `ArtiScene` log channel. The intended levels are:

- `debug` for entity, hierarchy, clone, registry, and System lifecycle changes.
- `info` for successful user-visible file save and load operations.
- `warn` for rejected API requests and recoverable omissions such as an
  unregistered clone component.
- `error` for broken internal invariants, failed file operations, and exceptions
  escaping System callbacks.

Normal per-frame System execution and successful world-transform updates are not
logged. Those paths are intentionally quiet so that lifecycle and failure messages
remain useful during debugging.
