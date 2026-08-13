ArtiChoco Asset

两阶段管线:Import(外部源 → 引擎资产 + metadata)→ Load(artifact → Asset 实例)

```
外部源文件 (.obj/.gltf/.png 等)
   │ AssetImporter.import()
   ▼
.meta sidecar(与源文件同目录)+ artifact(写入 artifacts_root)
   │ AssetManager.load() → AssetLoader.decode()
   ▼
Asset 实例(缓存于 AssetManager 的 weak 缓存,最后一个引用释放后自动回收)
```

-------------------------- ArtiChoco 定义的部分 --------------------------

AssetHandle<T> 类型化引用
 - 框架内部使用 core::UUID(UUID 64 位)作为裸 handle,metadata 与磁盘序列化都用裸 handle
 - 具体资产类型之间互相引用时使用 AssetHandle<T>(如 AssetHandle<TextureAsset>),
   编译期防止"材质槽里塞进 Mesh handle"这类错误
 - AssetHandle<T> 提供 id() / isValid() / operator<=>,可作 unordered_map 的 key
 - AssetManager::load<T>() 接受裸 UUID 或 AssetHandle<T>

Asset 抽象接口
 - 拥有 getType() (AssetType 表示 Asset 的类型)
 - 拥有 getHandle() 返回 core::UUID

AssetMetadata
 - Asset 的元数据,以 .meta YAML sidecar 形式持久化在源文件旁(所有 Asset 统一使用 .meta,
   不分 importer)
  - Handle: core::UUID
  - Type: AssetType
  - SourcePath: 该 Asset Source 相对于 .artiproj 的路径(子资产带 #后缀,如 "Content/a.gltf#mesh_0")
  - ArtifactPath: 该 Asset Artifact 相对于 artifacts_root 的路径
  - Dependencies: 该 Asset 引用的其他 Asset 的 handle 列表,用于依赖索引与重导入失效传播
  - Properties: 该 Asset 的自定义属性

AssetStorage
 - 文件层:拥有工作区布局与全部文件 I/O,路径一律相对 .artiproj
 - open(assets_root, artifacts_root) / close(),resolveSourcePath() / resolveArtifactPath()
 - writeArtifact() 写运行时 artifact;writeMetadata() 写 .meta sidecar
 - scanMetadata() open 时递归扫描 assets_root 下所有 .meta,校验摆放位置并反序列化

AssetCatalog
 - 数据层:内存中已导入资产的目录,绝不接触文件系统
 - metadata 表 unordered_map<core::UUID, AssetMetadata>,提供 find / findBySourcePath /
   findBySourcePathAndType / allMetadata / importedCount
 - 维护依赖索引(asset → 引用它的资产),提供 dependentsOf();insert() 时增量维护

AssetImporter 抽象接口
 - 职责:将外部资源导入为引擎内部能识别的 Asset,生成 AssetMetadata 与运行时能读取的 artifact
 - import() 接收相对于 Assets root 的源路径(与 metadata.SourcePath 同一身份空间,子资产加 "#name"),
   注册时注入 AssetStorage 与 AssetCatalog(不依赖 AssetManager):
   用 m_storage 读取文件,用 m_catalog 查询既有 identity
 - 每个源扩展名只允许注册一个 importer:一份源文件的 .meta 归属唯一 importer,
   一个源文件要产出多种 Asset 时由该 importer 一次产出全部子资产(如 glTFImporter 产出 mesh 与 material)
 - 一个源文件可产出多个 Asset,每个产出:
  - source_suffix: 子资产身份(如 "#mesh_0"),与源路径共同构成唯一 identity = (SourcePath, Type),
    子资产的 .meta 文件名也由它区分("a.gltf#mesh_0.meta")
 - 重导入时 handle 必须稳定:importer 用 m_catalog->findBySourcePathAndType() 查询既有 identity,
   存在则复用其 handle;再用 hasCurrentFiles()(受保护 helper)确认 .meta 与 artifact 仍在磁盘,
   都在则置 already_imported 跳过 encode 与写文件——manager 只更新内存中的 metadata
 - public:
  - import(source_path)
  - getSupportedExtensions() 返回该 Importer 支持的资源文件的扩展名
 - private:
  - encode() 将 Asset 编码为 artifact 二进制

AssetLoader 抽象接口
 - 职责:将 artifact 解码为 Asset 实例。纯解码器,无状态、不注入任何服务:
  - decode(metadata, artifact_file, dependencies) —— dependencies 与 metadata.dependencies
    同序,由 AssetManager 在调用前递归加载完成;loader 可选择持有其中部分 shared_ptr 保持依赖存活
 - public:
  - getType()
 - private(AssetManager 为 friend):
  - decode(metadata, artifact_file, dependencies)

AssetManager
 - 职责:编排整个资产管线,拥有 AssetStorage 与 AssetCatalog,通过注册的方式添加 Importer 和 Loader
 - 运行时状态:weak_ptr 缓存(loaded 实例)+ 加载集合(环检测)
 - open():storage.open → scanMetadata → 逐条 catalog.insert;close() 清理全部
 - import(source_path, importer):调用显式指定的 importer,写 artifact 与 .meta、更新 catalog,
   重导入时按 identity 复用既有 handle 并级联 unload 依赖它的已加载资产
 - load(handle) / load<T>(handle):缓存命中直接返回;否则按 type 分发,先递归加载
   metadata.dependencies(同一加载路径,环检测天然覆盖),再解析 artifact 调用 decode;
   解码或依赖加载抛异常时清理加载集合并报错
 - unload(handle) / unloadWithDependents(handle)(按 catalog 依赖索引做 BFS)

-------------------------- 下面的不归 ArtiChoco 所定义,是使用 ArtiChoco::Asset 的规范 --------------------------

资产分类
 - 数据资产(Data Asset):由外部源导入,引擎只读不编辑
  - TextureAsset / MeshAsset
 - 引擎资产(Engine Asset):由引擎内部定义与编辑,artifact 直接以 YAML 保存(runtime 用 yaml-cpp 解析)
  - MaterialAsset /(未来的)PrefabAsset、SceneAsset

TextureAsset 继承 Asset 是一种数据资产
 - 只表示纹理信息(宽、高、格式、像素数据/压缩块)
 - 例如 .png .jpg .hdr 通过 TextureImporter 生成 .meta 和 .texture 二进制文件

MeshAsset 继承 Asset 是一种数据资产
 - 只表示纯几何网格:顶点数据、索引、primitive(子网格)列表、包围盒
 - 材质绑定不烘焙进 MeshAsset:每个 primitive 带一个 material slot 名(如 "slot0"),
   实际绑定到哪个 MaterialAsset 由场景层决定(见下文),导入时可把 glTF 默认绑定写入 metadata properties
 - 例如 .obj .gltf .fbx 通过 MeshImporter 生成 N 个 .meta(N 个子网格)+ .mesh 二进制文件

MaterialAsset 继承 Asset 是一种引擎资产
 - 只表示材质信息,不包含几何信息,与 Mesh 完全解耦
 - 由引擎内部定义 例如:
            struct MaterialParams {
                glm::vec4 base_color{ 1.0f, 1.0f, 1.0f, 1.0f };
                float metallic{ 0.0f };
                float roughness{ 1.0f };
                glm::vec3 emission_color{ 0.0f, 0.0f, 0.0f };
                float emission_strength{ 1.0f };
            };
            struct MaterialTextures {
                AssetHandle<TextureAsset> base_color_texture;
                AssetHandle<TextureAsset> metallic_roughness_texture;
                AssetHandle<TextureAsset> normal_texture;
                AssetHandle<TextureAsset> emissive_texture;
                AssetHandle<TextureAsset> occlusion_texture;
            }
            struct MaterialAsset : public Asset {
                MaterialParams params;
                MaterialTextures textures;
            }
 - 引用使用类型化 handle,引用对象写入 metadata.Dependencies
 - 例如 .obj 的 mtl、gltf 的材质通过 MaterialImporter 生成 .meta 和 .artimaterial(YAML,编辑器可修改)

不定义 ModelAsset —— "模型"由组合表达(Unity 方案)
 - 不存在 ModelAsset 资产类型。"一个模型" = MeshAsset(几何)+ 场景中的渲染组件(MeshRendererComponent,
   持有 Handle<MeshAsset> 与每个 material slot 对应的 Handle<MaterialAsset> 列表)
 - 节点层次与实例化是 Scene 的职责,不属于任何资产:
   从 glTF 导入模型时,Importer 产出 N 个 MeshAsset + M 个 MaterialAsset(子资产),
   实例化由引擎层的 helper(如 instantiateMeshAsset(Scene&, Handle<MeshAsset>))创建 Entity 并挂渲染组件
 - 带骨骼的模型未来增加 SkeletonAsset / AnimationAsset,同样不引入"Model"聚合资产

glTF 子资产导出规范示例
 - Content/character.gltf 由一个 glTFImporter 一次导入,产出:
  - Content/character.gltf#mesh_0 → character.gltf#mesh_0.meta + .mesh(primitive 带 slot 名)
  - Content/character.gltf#material_0 → character.gltf#material_0.meta + .artimaterial
  - 每个产出的 metadata.Dependencies 记录该资产引用的其他子资产(如材质引用纹理)

后缀与目录约定
 - metadata sidecar:源路径 + ".meta"(子资产加 #子资产名,即 "a.gltf#mesh_0.meta"),位于 assets_root
 - artifact:artifacts_root/Imported/<handle>.<ext>(artifacts_root 不入版本控制,可随时删除重建)
 - 引擎资产(如 .artimaterial)为 YAML 文本,直接存放于 assets_root,metadata 的 ArtifactPath 指向它

-------------------------- 决策记录 --------------------------

 - 砍掉 ModelAsset:纯 handle + 节点树的"模型资产"本质是 Prefab,与 Scene 职责重叠;Unity 用
   "Mesh + 材质槽 + 场景组件"表达模型,更简单且不重复
 - AssetHandle<T> 类型化引用:裸 UUID 无法防止类型错配,编译期检查优于运行时 dynamic_cast
 - metadata.Dependencies + 依赖索引:支持递归加载、级联卸载与重导入失效传播
 - 统一 .meta:metadata sidecar 不按 importer 区分后缀,importer 身份由 meta 内的 Type 表达,
   子资产由文件名中的 #后缀区分;代价是每个源扩展名只允许一个 importer(这也是 glTFImporter
   这类复合 importer 的既有形态)
 - importer 的 identity 查询统一走 AssetCatalog.findBySourcePathAndType(),不再由 importer
   直接检查磁盘 .meta 文件;文件状态判断收敛为 AssetImporter::hasCurrentFiles()
 - 保留 Importer 多输出模型:容器格式(glTF/FBX)本质上一对多,多输出 + source_suffix 是唯一
   正确的子资产寻址方案;拆分多次 import 会失去事务性,importer 直接写库会破坏分层
 - 拆分 AssetDatabase:文件 I/O 归 AssetStorage,metadata 索引与依赖图归 AssetCatalog,
   loaded 缓存归 AssetManager;importer 注入 Storage+Catalog、loader 为纯解码器,
   均不再反向依赖 AssetManager,依赖加载策略(递归 + 环检测)收敛到 Manager 一处
 - Cook 阶段暂缓:引擎资产 YAML 直接运行时解析(yaml-cpp 已是依赖),未来需要缩短加载时间时再加

-------------------------- 未来扩展 --------------------------

 - Cook 阶段:Import 产出可编辑 YAML,Build 时 Cook 成运行时二进制;metadata 预留 cooked 路径字段
 - PrefabAsset:复用 Scene 序列化,实例化即把 Entity 树拷贝进场景
 - SkeletonAsset / AnimationAsset
