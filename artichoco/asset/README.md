# ArtiChoco Asset

> 草稿。内容来自设计与实现过程中的结论，尚未逐条对照源码复核。
> 文末「待复核」列出了需要核对的条目。

两阶段管线：**Import**（外部源 → 引擎资产 + metadata）→ **Load**（artifact → Asset 实例）。

## 核心不变式

```
Assets/ 源文件 + .meta   =  唯一真相
Library/ artifact        =  完全可推导，可随时整个删掉
内存 catalog 的 User 集合 =  磁盘 .meta 的纯函数
```

三条的实际含义：

- 删掉 `Library/` 再 reconcile，一切恢复，**UUID 不变**（身份存在 `.meta` 里，不在 artifact 里）
- 删掉某个 `.meta`，那个资产会被重新导入并拿到**新 UUID** —— 场景引用会断。所以 `.meta` 必须跟源文件一起进版本控制、一起改名
- catalog 里不可能存在「磁盘上没有 `.meta` 支撑」的条目：`applyReconcile` 每次都按磁盘重建 User 集合，这是结构保证，不靠调用方自觉

## 分层

```
AssetStorage    文件 I/O。拥有工作区布局，路径一律相对 Assets root
AssetCatalog    内存索引。绝不接触文件系统
AssetImporter   外部格式 → 引擎资产（每个源扩展名唯一归属一个 importer）
AssetLoader     artifact → Asset 实例（纯解码器，无状态）
AssetManager    门面。持有以上四者，唯一的写入点
```

### AssetStorage

- `scanMetadata()` → `MetadataScan { entries, issues }`。**坏 `.meta` 隔离进 `issues`，不让整个项目打不开**；但遍历失败是硬失败（扫不全会让 reconcile 误删没看到的东西）
- `scanSources()` → 磁盘上所有非 `.meta` 的常规文件
- `readMetadata` / `writeMetadata` / `removeMetadata`，`writeArtifact` / `removeArtifact`
- `resolveSourcePath` / `relativeSourcePath` 互为逆运算；后者让 prescan 能把 glTF 里的外部图片引用换算成资产身份空间里的路径

### AssetCatalog

三个索引：

| 索引 | 用途 |
| --- | --- |
| `handle → AssetEntry` | 主键 |
| `source_path → handles` | 分组查询 O(1) |
| `dependency → dependents` | 依赖反向图，级联 unload 用 |

- `AssetEntry { metadata, origin }`，`origin` 分 **User**（源自 `Assets/`，身份持久化在 `.meta`）和 **Engine**（引擎自带，编译期常量，磁盘上既无源文件也无 `.meta`）
- `insert` 返回 `{ Added, Updated, Conflicted }`。同一 UUID 声称两个不同 `(source_path, local_id)` 时**拒绝后来者并报错**，不静默覆盖
- `revision()` 每次变更自增，UI 靠它判断缓存失效
- `erase` / `eraseOrigin` 让 catalog 可被整体重建

### AssetManager

```cpp
ReconcilePlan   planReconcile() const;        // 纯读
ReconcileReport applyReconcile(const ReconcilePlan&);  // 唯一写入点
ReconcileReport reconcile();                  // 两者组合
AssetIntegrityReport checkIntegrity() const;  // 只读校验
```

`registerEngineAssetProvider` 让 Engine 来源资产（builtin）在每轮 reconcile 自愈缺失的 artifact —— 它们没有源文件，三方对账管不到。

## reconcile

对账三个数据源，算出差异并修复。

```
Assets/ 里的实际文件  ┐
Assets/ 里的 .meta    ├─ planReconcile() ─→ ReconcilePlan ─→ applyReconcile()
内存 catalog          ┘      纯读              （数据）        唯一写入点
```

`ReconcilePlan`（定义在 `asset_reconcile.h`，纯数据）：

- `items` —— 每个源文件一条，`action` 是 `Import` / `Reimport` / `Current` / `Unsupported`，另带 `inferred`（本轮裁决出的设置推断）和 `references`（prescan 声明的跨源引用）
- 四类问题各自单列，不混进 action：`orphans`（有 `.meta` 无源文件）、`conflicts`（UUID 撞了）、`inference_conflicts`、`metadata_issues`（坏 `.meta`）、`dependency_cycles`

plan 是纯读的，所以同时是三样东西：

1. **dry-run 报告** —— CLI 的 `asset_tools plan`
2. **UI 视图** —— Content Browser 每行的状态直接读它，不自己再推一遍
3. **确定性执行顺序** —— 先按路径字典序，再按依赖拓扑排序

apply 的顺序：Engine provider 补齐 → 按 plan 重建 catalog User 集合 → 回收孤儿（删 `.meta` + artifact + catalog 条目）→ 按拓扑序导入。

## sidecar 格式（v2）

**一个源文件一份 `.meta`**，子资产内嵌：

```yaml
Version: 2
Source:
  Path: Model/DamagedHelmet/DamagedHelmet.gltf
  ContentHash: 0          # 槽位已留，目前只写不读
  Size: 4537
Importer:
  Name: artiengine.GltfImporter
  Version: 1
Settings:
  Authored: {}            # 用户显式设定
  Inferred: {}            # 容器推断的缓存，带出处
  ResolvedHash: 14695981039346656037
Assets:
  - LocalId: material.Material_MR
    Handle: 7e5adaad8c3e593b
    Type: artiengine.asset.material
    ArtifactPath: Imported/7e5adaad8c3e593b.artimaterial
    Properties: {}
    Dependencies: [2602d161d24cf698, ...]
```

### 身份

**identity = `(source_path, local_id)`**。`local_id` 为空表示源文件本身就是唯一产出。

`local_id` **优先用源文件里的稳定名字**（glTF 的 mesh/material name、OBJ 的 shape/mtl 名），只在无名时回退下标：下标是位置相关的，在 glTF 里插入一个 mesh 会让同一个 UUID 指向另一块几何 —— 不报错，只是渲染不对。重名时追加 `#N`（那几个之间仍然位置相关，importer 单方面解决不了）。

`ContentHash` / `Size` / `Importer.Version` **目前只写不读**，是给源内容变更检测留的槽位（排在多线程之后）。所以现在**只有 artifact 缺失才触发重导**，改了源文件内容不会 —— 必须手动重导。

`ResolvedHash` 是真正被读的：推断变了它就变，从而触发重导。

## 设置解析

三层，**逐键**（不是整块覆盖 —— 用户改了 `Colorspace` 不该把容器对其他键的推断一起废掉）：

```
Authored（用户显式设定，最高）
  ↓ 缺失时
Inferred（引用它的容器推断的）
  ↓ 缺失时
Default（importer 在 getSettingSchema() 里声明的）
```

两条关键性质：

- **`Authored` 里「键的存在」本身是信息。** 缺失 ≠ false，缺失 = 未指定、往下层取。所以解析后的有效值**绝不能写回 `Authored`** —— 否则推断永远失效。这是整个设计的地基
- **哈希的是 resolved 而不是 authored。** 这样在代码里改一个默认值，所有依赖它的资产都会失效重导

`ResolvedSettings` 的取值器**不可失败**：schema 是权威，解析后每个声明过的键都保证存在且类型正确，所以 importer 里一条 fallback 分支都不用写。取一个没声明的键是编程错误。

**前缀族**（`SettingDescriptor::is_prefix`）用于键集编译期未知的设置 —— 比如 Extract 的 `ExtractedMaterial.<local_id>`，`local_id` 只有导入时才知道。用 `withPrefix()` 枚举。

## prescan / 拓扑序 / 推断

`prescan()` 在 plan 阶段被调用，**必须便宜**：只读文件头，不解几何、不解码图片。

```cpp
struct SourcePrescan {
    std::vector<std::filesystem::path> referenced_sources;  // 拓扑排序用
    std::vector<InferenceSuggestion> suggestions;           // 设置推断用
};
```

两者分开是必要的：引用一个文件不代表对它的设置有意见（glTF 引用 `.bin` 但无意见）。但它们来自同一次轻量解析，所以合并成一次调用。

**推断解决的问题**：`TextureImporter` 单看一张 jpg 无从判断它是法线还是漫反射，但 glTF 知道它绑在哪个材质槽上。所以由容器把用途告诉贴图。

**冲突**：一个源文件只有一份设置（Godot 模型），所以同一张图被两个容器当不同用途引用时**只能有一个胜出**。按发布者 `source_path` 字典序裁决，落败者记进 `plan.inference_conflicts` 并由 CLI 打印：

```
setting-conflict  M/t.tga  Colorspace
  kept M/a.gltf (base_color), ignored M/h.gltf (normal)
```

这是「一文件一设置」的固有代价，**显式报告而不静默**。

## 数据流：一个 PNG

```
Assets/Textures/rock_n.png                      ← 用户丢进来
```

**plan**

1. `scanSources()` 看到它，`scanMetadata()` 没有对应 `.meta` → `action = Import`
2. 它自己的 `prescan()` 什么都不发布（独立图片没有跨源引用）
3. 但**别人**会对它发布推断：某个 glTF 或 `.artimaterial` 把它绑在 `normalTexture` 槽上 → 建议 `Colorspace = linear`，`usage = "normal"`
4. 裁决后写进 `item.inferred`

**apply**

5. `persistInferences()` 先把推断写进 sidecar 的 `Settings.Inferred`
6. `import()` 解析设置：`Authored` 里没有 `Colorspace` → 取 `Inferred` 的 `linear`
7. `TextureImporter` 据此选 `RGBA8Unorm`（sRGB 用于颜色数据，Unorm 用于数据贴图），解码图片、编码 artifact
8. `commitOutputs` 写 `Library/Artifacts/Imported/<uuid>.artitexture` + sidecar

**为什么颜色空间必须在这里定对**

`forward_pbr.slang` 里全是裸 `Sample`，**没有任何手动 sRGB 转换**（唯二两处 `pow` 是 Fresnel 项）。解码完全由硬件 sRGB 视图完成：

```
.meta 的 format → TextureAsset::m_format → rendering::TextureDesc
  → detail::toRHIFormat() → nvrhi::Format::SRGBA8_UNORM 或 RGBA8_UNORM
```

法线贴图走 sRGB 视图的后果：`0.5`（切线空间零偏移）被解码成约 `0.214`，`* 2.0 - 1.0` 之后是 `-0.57` 而不是 `0` —— 法线整体被非线性地推歪。

**load**

```
AssetManager::load(uuid)
  → TextureLoader::decode(metadata, artifact_file, dependencies)
  → TextureAsset
  → GPUAssetCache::textureHandle() 创建 GPU 纹理并按 UUID 缓存
```

## 数据流：一个 glTF

```
Assets/Model/DamagedHelmet/
  DamagedHelmet.gltf
  DamagedHelmet.bin          ← 无 importer 认领 → Unsupported
  Default_albedo.jpg         ← 五张外部贴图，各自是独立纹理资产
  Default_normal.jpg
  Default_metalRoughness.jpg
  Default_AO.jpg
  Default_emissive.jpg
```

**plan**

1. `GltfImporter::prescan()` 只解析 JSON（不 `load_buffers`、不 `validate`、不解码图片），逐个材质记录每张外部图片绑在哪个槽位：
   - `baseColorTexture` / `emissiveTexture` → `srgb`
   - `normalTexture` / `metallicRoughnessTexture` / `occlusionTexture` → `linear`
2. 这五张图作为 `referenced_sources` 声明出来，同时发布 `Colorspace` 推断
3. 也从 sidecar 读 `ExtractedMaterial.*` 覆盖，把提取出的 `.artimaterial` 也声明成引用
4. **拓扑排序**把五张贴图排在 glTF 之前 —— 字母序恰好相反（`DamagedHelmet.gltf` < `Default_*.jpg`），所以这一步是必需的

**apply**（按拓扑序）

5. 五张贴图先导入，各自带正确的 `Colorspace`
6. 然后导入 glTF：
   - **外部图片** → 查 `findBySourceAndLocalId(相对路径, "")` 拿到已导入的纹理 handle，**不再自己解码一份**（一源一资产）
   - **内嵌图片**（data URI / buffer view）没有源文件 → 仍然产出成子资产
   - **材质** → 子资产，`dependencies` 指向那些共享纹理
   - **网格** → 子资产，只带 `material_slots` 名字，**不绑具体材质**
   - **prefab** → 子资产，持有节点树，每个节点记 `mesh` + `materials` 的 UUID

结果：glTF 的 sidecar 只有三条记录（material / mesh / prefab），**五张纹理不在其中** —— 它们是五个独立的 Root 资产。

**引用方向**（这是关键，容易搞反）

```
MeshAsset      只有 material_slots 名字，不绑材质
PrefabNode     绑 mesh UUID + materials UUID 列表   ← 绑定发生在这里
MaterialAsset  引用 texture handle
```

所以 mesh 从不绑材质，是 **prefab** 在绑。

**编辑器里拖进 Viewport**

- 拖 **prefab** → 按节点树生成实体，每个节点带 `MeshRendererComponent`
- 拖 **mesh** → 生成单个实体，材质用 builtin default

**去重效果**

改造前 11 个 `.artitexture` 对 6 张图（glTF 把外部贴图又解码了一份），且 standalone 那份的 `normal` / `metalRoughness` / `AO` 颜色空间**全错**（`TextureImporter` 无上下文，一律按 sRGB）。

现在 6 个，颜色空间**自动正确**。

## Extract

派生资产（容器产出的子资产）是**只读**的。用户要改 glTF 带来的材质，就把它提取成自己拥有的 Root 资产：

```
提取前                          提取后
foo.gltf                        foo.gltf
  ├─ material.Steel  ←只读        （不再产出）
  ├─ mesh.Body                  ├─ mesh.Body
  └─ prefab → 指向上面那份        └─ prefab → 指向 ↓
                                Materials/foo_material.Steel.artimaterial
                                  └─ 独立 Root 资产，可编辑
```

实现：容器 sidecar 里记 `Settings.Authored` 的 `ExtractedMaterial.<local_id> = <路径>`。`GltfImporter::import` 见到覆盖就引用提取物的 handle，不再产出自己那份。因为覆盖在 `Authored` 里，所以**活过 reconcile 和 Library 重建**。

`.artimaterial` 是**真实源文件**（YAML 文本），由 `MaterialImporter` 编译成 artifact。所以编辑器创作的材质就是普通 Root 资产，管线不需要「无源文件的用户资产」这种特例。源文件里贴图用**路径**引用（人可读、可 diff），导入时解析成 UUID：

```yaml
BaseColorTexture: Model/DamagedHelmet/Default_albedo.jpg
NormalTexture: Model/DamagedHelmet/Default_normal.jpg      # 独立纹理
BaseColorTexture: Model/foo.gltf#texture.albedo            # 容器子资产
```

## 文件清单

| 文件 | 内容 |
| --- | --- |
| `asset.h` | `Asset` 基类、`AssetHandle<T>` 类型化句柄、`AssetType` |
| `asset_metadata.{h,cpp}` | v2 sidecar 结构（`SourceMetadata` / `AssetRecord`）+ YAML 序列化 |
| `asset_settings.{h,cpp}` | 三层设置解析、`SettingDescriptor`、`ResolvedSettings`、FNV-1a 哈希 |
| `asset_storage.{h,cpp}` | 文件 I/O、扫描、路径解析 |
| `asset_catalog.{h,cpp}` | 内存索引、origin、UUID 冲突检测、revision |
| `asset_importer.{h,cpp}` | importer 接口、`AssetImportRequest`、`SourcePrescan` |
| `asset_loader.h` | loader 接口（纯解码器） |
| `asset_reconcile.h` | plan / report 纯数据类型 |
| `asset_manager.{h,cpp}` | 门面、reconcile 实现、拓扑排序、推断裁决 |
| `asset_log.{h,cpp}` | 日志通道 |

## 明确未做

- **源内容变更检测** —— `ContentHash` 只写不读。改了源文件内容不会自动重导，必须手动。对 `.artimaterial` 尤其突出（Extract 的意义就是让人改它）
- **多线程** —— reconcile 全程同步单线程。接缝已留：`scan()` 纯读、无共享写，将来换 `parallelFor` 语义不变
- **rename / delete** —— 在文件管理器里改名会让旧 UUID 变孤儿被回收、新文件拿到新 UUID，场景引用静默失效。变通办法是**连 `.meta` 一起改名**
- **`uid://`** —— Godot 那套路径无关引用
- **Extract 的编辑器 UI** —— 只有 CLI（`asset_tools extract`）
- **OBJ 的 roughness / metallic** —— MTL 分两张图，渲染端只有一个合并槽，两张都有时会丢一张

## 待复核

草稿写完未逐条对照源码，以下条目请核对后再删掉本节：

1. 文件清单里 `asset.h` / `asset_loader.h` / `asset_log.*` 的职责描述（这几个文件本轮没细看）
2. PNG 的 load 链路里 `TextureLoader::decode` 的确切签名
3. 拖 mesh 进 Viewport 用 builtin default material —— 方向应该对，但来源不可靠
4. `toRHIFormat` 的确切位置与命名（`detail/format_mapping.h`）
5. sidecar 示例里的 UUID 取自某一轮导入，仅作示意
