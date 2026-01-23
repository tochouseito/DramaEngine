#pragma once

// ECSManager.h
// C++ 標準ライブラリヘッダ一覧:
//
//   <cstdint>       // C++11: uint32_t, etc.
//   <cstddef>       // C++98: size_t
//   <bitset>        // C++98: std::bitset
//   <vector>        // C++98: std::vector
//   <unordered_map> // C++11: std::unordered_map
//   <memory>        // C++11: std::shared_ptr, std::weak_ptr
//   <functional>    // C++98: std::function
//   <algorithm>     // C++98: std::sort
//   <concepts>      // C++20: std::derived_from, etc.
//   <type_traits>   // C++11: std::false_type, std::true_type
#include <cstdint>
#include <cstddef>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <concepts>
#include <type_traits>

// Timer
#include <chrono>
#include <typeindex>

// ComponentIDHelper.h
#include "ComponentIDHelper.h"

namespace Drama::ECS
{

using Entity = uint32_t;
using CompID = size_t;
using Archetype = std::bitset<256>;

// コンポーネントだと判別するためのタグ
struct IComponentTag
{
    virtual ~IComponentTag() = default; // 仮想デストラクタを定義

    // 所有エンティティ設定
    //virtual void set_owner(Entity e) noexcept { m_owner = e; }
    //Entity get_owner() const noexcept { return m_owner; }

    // 所属 ECSManager 設定
    //virtual void set_ecs_manager(ECSManager* ecs) noexcept { m_pECS = ecs; }
    //ECSManager* get_ecs_manager() const noexcept { return m_pECS; }

    // 初期化関数
    virtual void initialize() {}

    // アクティブ状態
    bool is_active() const noexcept { return m_active; }
    void set_active(bool active) noexcept { m_active = active; }

private:
    bool m_active = true;        // アクティブ状態
    //Entity m_owner = 0;          // 所属エンティティID
    //ECSManager* m_pECS = nullptr; // 自分を管理するECSへのポインタ
};

// コンポーネントが複数持てるか(デフォルトは持てない)
template<typename T>
struct IsMultiComponent : std::false_type {};

// マルチコンポーネントを許可
//template<>
//struct IsMultiComponent</*マルチにしたいコンポーネント*/> : std::true_type {};

// コンポーネント型のみ許可する
template<typename T>
concept ComponentType = std::derived_from<T, IComponentTag>;

// ECS用ヘルパー
template<typename C>
concept HasInitialize = requires(C & c) { c.initialize(); };

struct IIComponentEventListener
{
    // ECS側から呼ばれる
    virtual void on_component_added(Entity e, CompID compType) = 0;
    virtual void on_component_copied(Entity src, Entity dst, CompID compType) = 0;
    virtual void on_component_removed(Entity e, CompID compType) = 0;
    virtual void on_component_removed_instance(Entity e, CompID compType, void* rawVec, size_t idx) = 0;
    // Prefabからの復元時に呼ばれる
    virtual void on_component_restored_from_prefab(Entity, CompID) {}
};

struct IEntityEventListener
{
    virtual ~IEntityEventListener() = default;

    /// e が新しく生成された直後に呼ばれる
    virtual void on_entity_created(Entity e) = 0;

    /// e のすべてのコンポーネントがクリアされ、RecycleQueue に入った直後に呼ばれる
    virtual void on_entity_destroyed(Entity e) = 0;
};

class ECSManager
{
    friend class IPrefab;
public:
    /*---------------------------------------------------------------------
        エンティティ管理
    ---------------------------------------------------------------------*/
    ECSManager() : m_nextEntityId(static_cast<Entity>(-1)) {}
    ~ECSManager() = default;

    // 公開API
    [[nodiscard]]
    bool is_entity_active(Entity e) const
    {
        return e < m_entityToActive.size() && m_entityToActive[e];
    }

    void set_entity_active(Entity e, bool f)
    {
        if (e < m_entityToActive.size())
        {
            m_entityToActive[e] = f;
        }
    }

    /*-------------------- Entity create / recycle ----------------------*/
    [[ nodiscard ]]
    inline const Entity generate_entity()
    {
        Entity entity = m_recycleEntities.empty()
            ? ++m_nextEntityId
            : m_recycleEntities.back();
        if (!m_recycleEntities.empty())
        {
            m_recycleEntities.pop_back();
        }

        // アクティブ化
        if (m_entityToActive.size() <= entity)
        {
            m_entityToActive.resize(entity + 1, false);
        }
        m_entityToActive[entity] = true;

        // Archetype 配列も拡張
        if (m_entityToArchetype.size() <= entity)
        {
            m_entityToArchetype.resize(entity + 1);
        }

        // 1) 空のアーキタイプ（全ビットfalse）にも必ず登録しておく
        {
            Archetype emptyArch;  // 全ビットfalse
            m_archToEntities[emptyArch].add(entity);
        }

        // 2) エンティティ生成イベントを通知（更新中なら遅延）
        auto notify = [this, entity]() {
            for (auto& wp : m_entityListeners)
            {
                if (auto sp = wp.lock())
                {
                    sp->on_entity_created(entity);
                }
            }
            };
        notify();

        if (m_isUpdating)
        {
            // Staging に積むだけ（本体に反映しない）
            m_stagingEntities.push_back(entity);
            m_stagingEntityActive.push_back(true);
            m_stagingEntityArchetypes.push_back(Archetype{});
        }
        else
        {
            // 通常通り即時反映
            if (m_entityToActive.size() <= entity)
            {
                m_entityToActive.resize(entity + 1, false);
            }
            m_entityToActive[entity] = true;

            if (m_entityToArchetype.size() <= entity)
            {
                m_entityToArchetype.resize(entity + 1);
            }
            m_entityToArchetype[entity] = Archetype{};

            // m_archToEntities[Archetype{}].add(entity);
        }

        return entity;
    }

    /*-------------------- Clear all components ------------------------*/
    inline void clear_entity(const Entity& e)
    {
        if (e >= m_entityToArchetype.size())
        {
            return;
        }
        Archetype old = m_entityToArchetype[e];

        // 1) 削除対象の CompID を collect
        std::vector<CompID> toRemove;
        for (CompID id = 0; id < old.size(); ++id)
        {
            if (old.test(id))
            {
                toRemove.push_back(id);
            }
        }

        // 2) 優先度→CompID でソート
        std::sort(toRemove.begin(), toRemove.end(),
            [&](CompID a, CompID b) {
                int pa = m_deletePriority.count(a) ? m_deletePriority[a] : 0;
                int pb = m_deletePriority.count(b) ? m_deletePriority[b] : 0;
                if (pa != pb)
                {
                    return pa < pb;
                }
                return a < b;
            });

        // 3) ソート後の順で通知＆削除
        for (CompID id : toRemove)
        {
            auto* pool = m_typeToComponents[id].get();
            size_t cnt = pool->get_component_count(e);

            for (auto& up : m_systems)
            {
                up->finalize_entity(e);
            }

            if (pool->is_multi_component_trait(id))
            {
                void* raw = pool->get_raw_component(e);
                for (size_t i = 0; i < cnt; ++i)
                {
                    notify_component_removed_instance(e, id, raw, i);
                }
            }
            else
            {
                notify_component_removed(e, id);
            }
            pool->cleanup(e); // クリーンアップ呼び出し
            pool->remove_component(e);
        }

        // バケット・アーキタイプクリア
        m_archToEntities[old].remove(e);
        m_entityToArchetype[e].reset();
    }

    //――――――――――――――――――
    // 他のシステムやメインループから呼んでよい、
    // 更新中も安全に追加できる汎用 defer
    //――――――――――――――――――
    void defer(std::function<void()> cmd)
    {
        m_deferredCommands.push_back(std::move(cmd));
    }

    /*-------------------- Disable entity ------------------------------*/
    inline void remove_entity(const Entity& e)
    {
        if (m_isUpdating)
        {
            // システム更新中なら遅延キューに積む
            defer([this, e] { remove_entity_impl(e); });
        }
        else
        {
            // 通常フレームなら即時実行
            remove_entity_impl(e);
        }
    }

    /*-------------------- Copy whole entity ---------------------------*/
    [[ nodiscard ]]
    Entity copy_entity(const Entity& src)
    {
        // 1) 元のアーキタイプを取得
        const Archetype arch = get_archetype(src);

        // 2) 生成（ステージングなら generate_entity もステージングに入るよう修正済み）
        Entity dst = generate_entity();

        // ── コピーする CompID を集めて優先度順にソート ──
        std::vector<CompID> toCopy;
        for (CompID id = 0; id < arch.size(); ++id)
        {
            if (arch.test(id))
            {
                toCopy.push_back(id);
            }
        }

        std::sort(toCopy.begin(), toCopy.end(),
            [&](CompID a, CompID b) {
                int pa = m_copyPriority.count(a) ? m_copyPriority[a] : 0;
                int pb = m_copyPriority.count(b) ? m_copyPriority[b] : 0;
                if (pa != pb)
                {
                    return pa < pb;
                }
                return a < b;
            });

        // 3) 優先度順にしてからコピー＆通知
        for (CompID id : toCopy)
        {
            auto* pool = m_typeToComponents[id].get();
            if (m_isUpdating)
            {
                pool->copy_component_staging(src, dst);
                size_t idx = staging_index_for_entity(dst);
                m_stagingEntityArchetypes[idx].set(id);
            }
            else
            {
                // 通常フレーム：即時コピー
                pool->copy_component(src, dst);

                // → ここで本番バッファにもビットを立てる
                if (m_entityToArchetype.size() <= dst)
                {
                    m_entityToArchetype.resize(dst + 1);
                }
                /* Archetype& archDst = m_entityToArchetype[dst];
                 if (!archDst.test(id))
                 {
                     m_archToEntities[archDst].remove(dst);
                     archDst.set(id);
                     m_archToEntities[archDst].add(dst);
                 }*/
                m_entityToArchetype[dst].set(id);
            }
            notify_component_copied(src, dst, id);
        }

        // 4) アーキタイプの反映
        if (m_isUpdating)
        {
            for (auto& sys : m_systems)
            {
                sys->awake_entity(dst);
            }
        }
        else
        {
            // 即時反映
            /*if (m_entityToArchetype.size() <= dst)
                m_entityToArchetype.resize(dst + 1);
            m_entityToArchetype[dst] = arch;
            m_archToEntities[arch].add(dst);*/
            Archetype oldArch = Archetype{};
            Archetype& newArch = m_entityToArchetype[dst];
            if (oldArch != newArch)
            {
                m_archToEntities[oldArch].remove(dst);
                m_archToEntities[newArch].add(dst);
            }
        }

        return dst;
    }

    //-------------------- Copy into existing entity --------------------
    void copy_entity(const Entity& src, const Entity& dst)
    {
        // 1) ソースのアーキタイプを取得
        const Archetype arch = get_archetype(src);

        Archetype oldArch = m_isUpdating ? Archetype{} : get_archetype(dst);

        // ── コピーする CompID を集めて優先度順にソート ──
        std::vector<CompID> toCopy;
        for (CompID id = 0; id < arch.size(); ++id)
        {
            if (arch.test(id))
            {
                toCopy.push_back(id);
            }
        }

        std::sort(toCopy.begin(), toCopy.end(),
            [&](CompID a, CompID b) {
                int pa = m_copyPriority.count(a) ? m_copyPriority[a] : 0;
                int pb = m_copyPriority.count(b) ? m_copyPriority[b] : 0;
                if (pa != pb)
                {
                    return pa < pb;
                }
                return a < b;
            });

        // 3) 優先度順にしてからコピー＆通知
        for (CompID id : toCopy)
        {
            auto* pool = m_typeToComponents[id].get();
            if (m_isUpdating)
            {
                pool->copy_component_staging(src, dst);
                size_t idx = staging_index_for_entity(dst);
                m_stagingEntityArchetypes[idx].set(id);
            }
            else
            {
                // 通常フレーム：即時コピー
                pool->copy_component(src, dst);

                // → ここで本番バッファにもビットを立てる
                if (m_entityToArchetype.size() <= dst)
                {
                    m_entityToArchetype.resize(dst + 1);
                }
                m_entityToArchetype[dst].set(id);
            }
            notify_component_copied(src, dst, id);
        }

        // 4) アーキタイプの反映
        if (m_isUpdating)
        {
            for (auto& sys : m_systems)
            {
                sys->awake_entity(dst);
            }
        }
        else
        {
            // 即時反映
            /*if (m_entityToArchetype.size() <= dst)
                m_entityToArchetype.resize(dst + 1);
            m_entityToArchetype[dst] = arch;
            m_archToEntities[arch].add(dst);*/
            Archetype& newArch = m_entityToArchetype[dst];
            if (oldArch != newArch)
            {
                m_archToEntities[oldArch].remove(dst);
                m_archToEntities[newArch].add(dst);
            }
        }
    }

    /// Entity e のステージングバッファ内インデックスを返す。
    /// なければ追加して新しいインデックスを返す。
    size_t staging_index_for_entity(Entity e)
    {
        // 1) 既存エントリがあればその位置を返す
        auto it = std::find(m_stagingEntities.begin(), m_stagingEntities.end(), e);
        if (it != m_stagingEntities.end())
        {
            return static_cast<size_t>(std::distance(m_stagingEntities.begin(), it));
        }

        // 2) なければバッファ末尾に追加
        size_t newIndex = m_stagingEntities.size();
        m_stagingEntities.push_back(e);

        // デフォルト active=true, archetype は空ビットセットで追加
        m_stagingEntityActive.push_back(true);
        m_stagingEntityArchetypes.push_back(Archetype{});

        return newIndex;
    }

    /*-------------------- Copy selected components --------------------*/
    void copy_components(Entity src, Entity dst, bool overwrite = true)
    {
        const Archetype& archSrc = get_archetype(src);

        // コピー先アーキタイプへの参照取得（ステージング中かどうかで使い分け）
        Archetype* pArchDst;
        Archetype  oldArch;
        if (m_isUpdating)
        {
            // dst がステージングリストにあればそのインデックスを取得、
            // なければ追加してからインデックスを取得
            size_t idx = staging_index_for_entity(dst);
            pArchDst = &m_stagingEntityArchetypes[idx];
        }
        else
        {
            if (m_entityToArchetype.size() <= dst)
            {
                m_entityToArchetype.resize(dst + 1);
            }
            pArchDst = &m_entityToArchetype[dst];
        }
        oldArch = *pArchDst;

        // ソースのビットセットを走査
        for (CompID id = 0; id < archSrc.size(); ++id)
        {
            if (!archSrc.test(id))
            {
                continue;
            }
            if (!overwrite && pArchDst->test(id))
            {
                continue;
            }

            auto* pool = m_typeToComponents[id].get();
            if (m_isUpdating)
            {
                // フレーム中はステージングコピー
                pool->copy_component_staging(src, dst);
            }
            else
            {
                // フレーム外は即時コピー
                pool->copy_component(src, dst);
                notify_component_copied(src, dst, id);
            }

            // アーキタイプのビットをセット
            pArchDst->set(id);
        }

        // 即時フレーム外ならバケット（EntityContainer）も更新
        if (!m_isUpdating && *pArchDst != oldArch)
        {
            m_archToEntities[oldArch].remove(dst);
            m_archToEntities[*pArchDst].add(dst);
        }
    }

    /*-------------------- add component -------------------------------*/
    template<ComponentType T>
    T* add_component(const Entity& entity)
    {
        CompID type = ComponentPool<T>::get_id();

        // プール取得 or 作成
        auto [it, _] = m_typeToComponents.try_emplace(
            type,
            std::make_shared<ComponentPool<T>>(4096)
        );
        auto pool = std::static_pointer_cast<ComponentPool<T>>(it->second);

        // 更新中ならステージングに追加して即時参照可能にする
        if (m_isUpdating)
        {
            T* comp = pool->add_component_staging(entity); // ← 後述の新関数で staging に追加

            // Archetype は staging にも保持しておく（後で flush_staging_entities で反映）
            if (std::find(m_stagingEntities.begin(), m_stagingEntities.end(), entity) == m_stagingEntities.end())
            {
                // Entity がまだ Staging に登録されていなければ追加
                m_stagingEntities.push_back(entity);
                m_stagingEntityActive.push_back(true);
                m_stagingEntityArchetypes.push_back(Archetype{});
            }

            // Archetype を更新（set type ビット）
            for (size_t i = 0; i < m_stagingEntities.size(); ++i)
            {
                if (m_stagingEntities[i] == entity)
                {
                    m_stagingEntityArchetypes[i].set(type);
                    break;
                }
            }

            if constexpr (HasInitialize<T>) comp->initialize();
            //comp->set_owner(entity);
            //comp->set_ecs_manager(this);
            notify_component_added(entity, type);

            return comp; // 即時取得可能
        }

        // 通常フロー（即時反映）
        T* comp = pool->add_component(entity);
        //comp->set_owner(entity);
        //comp->set_ecs_manager(this);

        if (m_entityToArchetype.size() <= entity)
        {
            m_entityToArchetype.resize(entity + 1, Archetype{});
        }
        Archetype& arch = m_entityToArchetype[entity];
        if (!arch.test(type))
        {
            m_archToEntities[arch].remove(entity);
            arch.set(type);
            m_archToEntities[arch].add(entity);
        }

        if constexpr (HasInitialize<T>) comp->initialize();
        notify_component_added(entity, type);

        return comp;
    }

    /// Prefab復元時だけ使う、イベントを起こさずコンポーネントを追加
    template<ComponentType T>
    void prefab_add_component(Entity e, T const& comp)
    {
        CompID type = ComponentPool<T>::get_id();

        // プール取得または生成
        auto [it, _] = m_typeToComponents.try_emplace(
            type,
            std::make_shared<ComponentPool<T>>(4096)
        );
        auto pool = std::static_pointer_cast<ComponentPool<T>>(it->second);

        if (m_isUpdating)
        {
            // ▼ ステージングに即時追加
            pool->add_prefab_component_staging(e, comp);
            //T* pComp = pool->get_component(e);
            //pComp->set_owner(e);
            //pComp->set_ecs_manager(this);

            // ▼ Archetype も staging に反映
            auto itE = std::find(m_stagingEntities.begin(), m_stagingEntities.end(), e);
            if (itE != m_stagingEntities.end())
            {
                size_t i = std::distance(m_stagingEntities.begin(), itE);
                m_stagingEntityArchetypes[i].set(type);
            }
            else
            {
                m_stagingEntities.push_back(e);
                m_stagingEntityActive.push_back(true);
                Archetype arch{};
                arch.set(type);
                m_stagingEntityArchetypes.push_back(arch);
            }

            return; // 遅延不要
        }

        // ▼ 通常の即時追加（更新中でない場合）
        pool->add_prefab_component(e, comp);
        //T* pComp = pool->get_component(e);
        //pComp->set_owner(e);
        //pComp->set_ecs_manager(this);

        if (m_entityToArchetype.size() <= e)
        {
            m_entityToArchetype.resize(e + 1, Archetype{});
        }
        Archetype& arch = m_entityToArchetype[e];

        if (!arch.test(type))
        {
            m_archToEntities[arch].remove(e);
            arch.set(type);
            m_archToEntities[arch].add(e);
        }

        notify_component_restored_from_prefab(e, ComponentPool<T>::get_id());
    }

    /*-------------------- Get component -------------------------------*/
    template<ComponentType T>
    T* get_component(const Entity& entity)
    {
        CompID type = ComponentPool<T>::get_id();
        bool has = false;

        // ステージング中はステージングとメイン両方をチェック
        if (m_isUpdating)
        {
            // メイン側チェック
            if (entity < m_entityToArchetype.size() && m_entityToArchetype[entity].test(type))
            {
                has = true;
            }
            else
            {
                // ステージングバッファ内のアーキタイプをチェック
                auto it = std::find(m_stagingEntities.begin(),
                    m_stagingEntities.end(), entity);
                if (it != m_stagingEntities.end())
                {
                    size_t idx = it - m_stagingEntities.begin();
                    has = m_stagingEntityArchetypes[idx].test(type);
                }
            }
        }
        else
        {
            // 通常フレームならメインのみチェック
            has = (entity < m_entityToArchetype.size() &&
                m_entityToArchetype[entity].test(type));
        }

        if (!has)
        {
            return nullptr;
        }

        // プールから取得し、ステージング込みの get_component を呼ぶ
        auto pool = static_cast<ComponentPool<T>*>(get_raw_component_pool(type));
        return pool->get_component(entity);
    }

    /*-------------------- Get all (multi) -----------------------------*/
    template<ComponentType T>
    std::vector<T>* get_all_components(const Entity& entity) requires IsMultiComponent<T>::value
    {
        auto it = m_typeToComponents.find(ComponentPool<T>::get_id());
        if (it == m_typeToComponents.end())
        {
            return nullptr;
        }
        auto pool = std::static_pointer_cast<ComponentPool<T>>(it->second);
        return pool->get_all_components(entity);
    }

    /*-------------------- remove component ----------------------------*/
    template<ComponentType T>
    void remove_component(const Entity& entity)
    {
        if (m_isUpdating)
        {
            defer([this, entity]() { remove_component<T>(entity); });
            return;
        }

        static_assert(!IsMultiComponent<T>::value, "Use remove_all_components for multi-instance.");
        CompID type = ComponentPool<T>::get_id();
        if (entity >= m_entityToArchetype.size() || !m_entityToArchetype[entity].test(type))
        {
            return;
        }
        auto pool = get_component_pool<T>();
        if (!pool)
        {
            return;
        }

        // 2) 対応するシステムに「このコンポーネントを削除する前のFinalize」を呼ぶ
        for (auto& up : m_systems)
        {
            if (auto sys = dynamic_cast<System<T>*>(up.get()))
            {
                sys->finalize_entity(entity);
            }
        }

        notify_component_removed(entity, type);
        pool->remove_component(entity);

        Archetype& arch = m_entityToArchetype[entity];
        m_archToEntities[arch].remove(entity);
        arch.reset(type);
        m_archToEntities[arch].add(entity);
    }

    /*-------------------- remove ALL multi ----------------------------*/
    template<ComponentType T>
    void remove_all_components(const Entity& entity) requires IsMultiComponent<T>::value
    {
        if (m_isUpdating)
        {
            defer([this, entity]() { remove_all_components<T>(entity); });
            return;
        }

        auto* pool = get_component_pool<T>();
        if (!pool)
        {
            return;
        }

        // マルチ用 finalize 呼び
        for (auto& up : m_systems)
        {
            if (auto ms = dynamic_cast<MultiComponentSystem<T>*>(up.get()))
            {
                ms->finalize_entity(entity);
            }
        }

        notify_component_removed(entity, ComponentPool<T>::get_id());
        pool->remove_all(entity);

        Archetype& arch = m_entityToArchetype[entity];
        arch.reset(ComponentPool<T>::get_id());
        m_archToEntities[arch].remove(entity);
        m_archToEntities[arch].add(entity);
    }

    // マルチコンポーネントの単一インスタンスを消す
    template<ComponentType T>
    void remove_component_instance(const Entity& e, size_t index)
        requires IsMultiComponent<T>::value
    {
        if (m_isUpdating)
        {
            defer([this, e, index]() { remove_component_instance<T>(e, index); });
            return;
        }

        CompID id = ComponentPool<T>::get_id();
        auto* pool = get_component_pool<T>();
        void* rawVec = pool ? pool->get_raw_component(e) : nullptr;
        if (pool)
        {
            auto* vec = static_cast<std::vector<T>*>(rawVec);
            if (vec && index < vec->size())
            {
                notify_component_removed_instance(e, id, rawVec, index);
            }
        }

        if (pool)
        {
            pool->remove_instance(e, index);
        }

        auto& arch = m_entityToArchetype[e];
        auto* remaining = pool ? pool->get_all_components(e) : nullptr;
        if (!remaining || remaining->empty())
        {
            m_archToEntities[arch].remove(e);
            arch.reset(id);
            m_archToEntities[arch].add(e);
        }
    }

    // コンポーネント別に「削除時の優先度」を設定できるように
    template<ComponentType T>
    void set_deletion_priority(int priority)
    {
        m_deletePriority[ComponentPool<T>::get_id()] = priority;
    }

    // コピー時の優先度を設定するテンプレート関数
    template<ComponentType T>
    void set_copy_priority(int priority)
    {
        m_copyPriority[ComponentPool<T>::get_id()] = priority;
    }

    /*-------------------- Accessors ----------------------------------*/
    inline const Archetype& get_archetype(Entity e) const
    {
        static Archetype empty{};
        return (e < m_entityToArchetype.size()) ? m_entityToArchetype[e] : empty;
    }


    /// コンポーネントイベントリスナーを登録（shared_ptr で受け取る）
    void add_component_listener(std::shared_ptr<IIComponentEventListener> listener)
    {
        m_componentListeners.emplace_back(listener);
    }

    /// エンティティイベントリスナーを登録（shared_ptr で受け取る）
    void add_entity_listener(std::shared_ptr<IEntityEventListener> listener)
    {
        m_entityListeners.emplace_back(listener);
    }
    /// (オプション) 全リスナーをクリア
    void clear_component_listeners()
    {
        m_componentListeners.clear();
    }
    void clear_entity_listeners()
    {
        m_entityListeners.clear();
    }

    //――――――――――――――――――
    // システム登録
    //――――――――――――――――――
    template<typename S, typename... Args>
    S& add_system(Args&&... args)
    {
        static_assert(std::is_base_of_v<ISystem, S>, "S must derive from ISystem");
        auto ptr = std::make_unique<S>(std::forward<Args>(args)...);
        S* raw = ptr.get();
        // 所属する ECSManager のポインタを渡す
        raw->on_register(this);
        m_systems.push_back(std::move(ptr));
        return *raw;
    }
    template<typename S>
    S* get_system()
    {
        for (auto& up : m_systems)
        {
            if (auto p = dynamic_cast<S*>(up.get()))
            {
                return p;
            }
        }
        return nullptr;
    }

    // 更新ループを中止するAPI
    void cancel_update_loop()
    {
        m_cancelUpdate = true;
    }

    // 1) ゲーム開始前に一度だけ
    void initialize_all_systems()
    {
        using Clock = std::chrono::steady_clock;
        m_isUpdating = true;
        m_cancelUpdate = false; // ← 毎フレーム最初にリセット
        // 全体計測開始
        auto t0Total = Clock::now();

        std::sort(m_systems.begin(), m_systems.end(),
            [](auto& a, auto& b) { return a->get_priority() < b->get_priority(); });
        for (auto& sys : m_systems)
        {
            if (sys->is_enabled())
            {
                // 各システム計測開始
                auto t0 = Clock::now();
                sys->initialize();
                auto t1 = Clock::now();
                // 型情報をキーに時間を保存
                std::type_index ti(typeid(*sys));
                m_lastSystemInitializeTimeMs[ti] =
                    std::chrono::duration<double, std::milli>(t1 - t0).count();
            }
        }

        m_isUpdating = false;
        m_cancelUpdate = false;

        // 全体計測終了
        auto t1Total = Clock::now();
        m_lastTotalInitializeTimeMs =
            std::chrono::duration<double, std::milli>(t1Total - t0Total).count();
    }

    //――――――――――――――――――
    // フレーム毎に呼ぶ：全システムを優先度順に更新
    //――――――――――――――――――
    void update_all_systems()
    {
        using Clock = std::chrono::steady_clock;
        m_isUpdating = true;
        m_cancelUpdate = false; // ← 毎フレーム最初にリセット

        // 総合計測開始
        auto t0Total = Clock::now();

        // 追加直後の Entity を初期化システムに通す
        for (Entity e : m_newEntitiesLastFrame)
        {
            for (auto& sys : m_systems)
            {
                sys->initialize_entity(e);
            }
        }
        m_newEntitiesLastFrame.clear();

        // システム更新
        std::sort(m_systems.begin(), m_systems.end(),
            [](auto& a, auto& b) { return a->get_priority() < b->get_priority(); });
        for (auto& sys : m_systems)
        {
            if (sys->is_enabled())
            {
                if (m_cancelUpdate) break; // 中止判定
                // 各システム計測開始
                auto t0 = Clock::now();
                sys->update();
                auto t1 = Clock::now();
                // 型情報をキーに時間を保存
                std::type_index ti(typeid(*sys));
                m_lastSystemUpdateTimeMs[ti] =
                    std::chrono::duration<double, std::milli>(t1 - t0).count();
            }
        }

        m_isUpdating = false;
        m_cancelUpdate = false;
        flush_staging_entities();
        flush_staging_components();
        flush_deferred();

        // 総合計測終了
        auto t1Total = Clock::now();
        m_lastTotalUpdateTimeMs =
            std::chrono::duration<double, std::milli>(t1Total - t0Total).count();
    }

    // 3) ゲーム終了後に一度だけ
    void finalize_all_systems()
    {
        using Clock = std::chrono::steady_clock;
        m_isUpdating = true;
        m_cancelUpdate = false; // ← 毎フレーム最初にリセット
        // 全体計測開始
        auto t0Total = Clock::now();

        std::sort(m_systems.begin(), m_systems.end(),
            [](auto& a, auto& b) { return a->get_priority() < b->get_priority(); });
        for (auto& sys : m_systems)
        {
            if (sys->is_enabled())
            {
                // 各システム計測開始
                auto t0 = Clock::now();
                sys->finalize();
                auto t1 = Clock::now();

                std::type_index ti(typeid(*sys));
                m_lastSystemFinalizeTimeMs[ti] =
                    std::chrono::duration<double, std::milli>(t1 - t0).count();
            }
        }

        m_isUpdating = false;
        m_cancelUpdate = false;

        // 全体計測終了
        auto t1Total = Clock::now();
        m_lastTotalFinalizeTimeMs =
            std::chrono::duration<double, std::milli>(t1Total - t0Total).count();
    }

    // 1) ゲーム開始前に一度だけ
    void awake_all_systems()
    {
        using Clock = std::chrono::steady_clock;
        m_isUpdating = true;
        m_cancelUpdate = false; // ← 毎フレーム最初にリセット
        // 全体計測開始
        auto t0Total = Clock::now();

        std::sort(m_systems.begin(), m_systems.end(),
            [](auto& a, auto& b) { return a->get_priority() < b->get_priority(); });
        for (auto& sys : m_systems)
        {
            if (sys->is_enabled())
            {
                // 各システム計測開始
                auto t0 = Clock::now();
                sys->awake();
                auto t1 = Clock::now();
                // 型情報をキーに時間を保存
                std::type_index ti(typeid(*sys));
                m_lastSystemAwakeTimeMs[ti] =
                    std::chrono::duration<double, std::milli>(t1 - t0).count();
            }
        }

        m_isUpdating = false;
        m_cancelUpdate = false;

        // 全体計測終了
        auto t1Total = Clock::now();
        m_lastTotalAwakeTimeMs =
            std::chrono::duration<double, std::milli>(t1Total - t0Total).count();
    }

    /// 最後のフレームで更新に要した「全システム分」の時間を取得(ms)
    double get_last_total_update_time_ms() const
    {
        return m_lastTotalUpdateTimeMs;
    }

    /// 最後のフレームで更新に要した、システム型 S の時間を取得(ms)
    template<typename S>
    double get_last_system_update_time_ms() const
    {
        std::type_index ti(typeid(S));
        auto it = m_lastSystemUpdateTimeMs.find(ti);
        if (it == m_lastSystemUpdateTimeMs.end())
        {
            return 0.0;
        }
        return it->second;
    }

    // initialize
    double get_last_total_initialize_time_ms() const
    {
        return m_lastTotalInitializeTimeMs;
    }

    template<typename S>
    double get_last_system_initialize_time_ms() const
    {
        auto it = m_lastSystemInitializeTimeMs.find(typeid(S));
        return it == m_lastSystemInitializeTimeMs.end() ? 0.0 : it->second;
    }

    // finalize
    double get_last_total_finalize_time_ms() const
    {
        return m_lastTotalFinalizeTimeMs;
    }

    template<typename S>
    double get_last_system_finalize_time_ms() const
    {
        auto it = m_lastSystemFinalizeTimeMs.find(typeid(S));
        return it == m_lastSystemFinalizeTimeMs.end() ? 0.0 : it->second;
    }

    void flush_staging_entities()
    {
        for (size_t i = 0; i < m_stagingEntities.size(); ++i)
        {
            Entity e = m_stagingEntities[i];

            // m_entityToActive を拡張して反映
            if (m_entityToActive.size() <= e)
            {
                m_entityToActive.resize(e + 1, false);
            }
            m_entityToActive[e] = m_stagingEntityActive[i];

            // Archetype を反映
            if (m_entityToArchetype.size() <= e)
            {
                m_entityToArchetype.resize(e + 1);
            }
            m_entityToArchetype[e] = m_stagingEntityArchetypes[i];

            // Archetype バケットに登録
            m_archToEntities[m_stagingEntityArchetypes[i]].add(e);

            // 新規Entityリストに追加
            m_newEntitiesLastFrame.push_back(e);
        }

        // 一時バッファをクリア
        m_stagingEntities.clear();
        m_stagingEntityActive.clear();
        m_stagingEntityArchetypes.clear();
    }
    void flush_staging_components()
    {
        for (auto& [id, pool] : m_typeToComponents)
        {
            pool->flush_staging();
        }
    }

    /*---------------------------------------------------------------------
        EntityContainer (archetype bucket)
    ---------------------------------------------------------------------*/
    class EntityContainer
    {
    public:
        void add(Entity e)
        {
            m_entities.emplace_back(e);
            if (m_entityToIndex.size() <= e)
            {
                m_entityToIndex.resize(e + 1);
            }
            m_entityToIndex[e] = static_cast<uint32_t>(m_entities.size() - 1);
        }
        void remove(Entity e)
        {
            if (e >= m_entityToIndex.size())
            {
                return;
            }
            uint32_t idx = m_entityToIndex[e];
            if (idx >= m_entities.size())
            {
                return;
            }
            uint32_t last = static_cast<uint32_t>(m_entities.size() - 1);
            Entity   back = m_entities[last];
            if (e != back)
            {
                m_entities[idx] = back;
                m_entityToIndex[back] = idx;
            }
            m_entities.pop_back();
        }
        const std::vector<Entity>& get_entities() const noexcept { return m_entities; }
    private:
        std::vector<Entity>  m_entities;
        std::vector<uint32_t> m_entityToIndex;
    };

    /*---------------------------------------------------------------------
        IComponentPool (interface)
    ---------------------------------------------------------------------*/
    class IComponentPool
    {
    public:
        virtual ~IComponentPool() = default;
        virtual void copy_component(Entity src, Entity dst) = 0;
        virtual void remove_component(Entity e) = 0;
        virtual void* get_raw_component(Entity e) const = 0;
        virtual std::shared_ptr<void> clone_component(CompID id, void* ptr) = 0;
        virtual bool is_multi_component_trait(CompID id) const = 0;
        virtual size_t get_component_count(Entity e) const = 0;
        /// 生ポインタから直接エンティティ dst にクローンして追加
        virtual void clone_raw_component_to(Entity dst, void* raw) = 0;
        /// 削除前のクリーンアップ呼び出し用
        virtual void cleanup(Entity) {}
        virtual void flush_staging() {} // ステージングバッファをフラッシュ
        virtual void copy_component_staging(Entity src, Entity dst)
        {
            // デフォルトでは通常コピーにフォールバック
            copy_component(src, dst);
        }
    };

    IComponentPool* get_raw_component_pool(CompID id)
    {
        auto it = m_typeToComponents.find(id);
        return (it != m_typeToComponents.end()) ? it->second.get() : nullptr;
    }

    /*---------------------------------------------------------------------
        ComponentPool  (vector‑backed)
    ---------------------------------------------------------------------*/
    template<ComponentType T>
    class ComponentPool : public IComponentPool
    {
        using Storage = std::vector<T>;                    // ★ vector 版
        static constexpr uint32_t kInvalid = ~0u;
    public:
        explicit ComponentPool(size_t reserveEntities = 0) { m_storage.reserve(reserveEntities); }

        // 登録された型がマルチかどうかを返す
        bool is_multi_component_trait(CompID) const override
        {
            return IsMultiComponent<T>::value;
        }

        /*-------------------- add ---------------------*/
        T* add_component(Entity e)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                return &m_multi[e].emplace_back();          // vector of components per entity
            }
            else
            {
                if (m_entityToIndex.size() <= e)
                {
                    m_entityToIndex.resize(e + 1, kInvalid);
                }
                uint32_t idx = m_entityToIndex[e];
                if (idx == kInvalid)
                {
                    idx = static_cast<uint32_t>(m_storage.size());
                    m_storage.emplace_back();              // default construct
                    if (m_indexToEntity.size() <= idx)
                    {
                        m_indexToEntity.resize(idx + 1, kInvalid);
                    }
                    m_entityToIndex[e] = idx;
                    m_indexToEntity[idx] = e;
                }
                return &m_storage[idx];
            }
        }

        T* add_component_staging(Entity e)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto& vec = m_stagingMulti[e];
                vec.emplace_back();
                T& inst = vec.back();
                //inst.set_owner(e);
                //inst.set_ecs_manager(m_pEcs);
                return &inst;
            }
            else
            {
                // 上書きも可能
                T& inst = m_stagingSingle[e];
                //inst.set_owner(e);
                //inst.set_ecs_manager(m_pEcs);
                return &inst;
            }
        }

        /*-------------------- get ---------------------*/
        T* get_component(Entity e)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                // 1) 一時バッファにあるかチェック
                auto itStaging = m_stagingMulti.find(e);
                if (itStaging != m_stagingMulti.end() && !itStaging->second.empty())
                {
                    return &itStaging->second.front();
                }

                // 2) 本番マップから取得
                auto it = m_multi.find(e);
                return (it != m_multi.end() && !it->second.empty()) ? &it->second.front() : nullptr;
            }
            else
            {
                // 1) 一時バッファにあるかチェック
                auto itStaging = m_stagingSingle.find(e);
                if (itStaging != m_stagingSingle.end())
                {
                    return &itStaging->second;
                }

                // 2) 本番ストレージから取得
                if (e >= m_entityToIndex.size())
                {
                    return nullptr;
                }
                uint32_t idx = m_entityToIndex[e];
                return (idx != kInvalid) ? &m_storage[idx] : nullptr;
            }
        }

        /*-------------------- remove ------------------*/
        void remove_component(Entity e) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                m_multi.erase(e);
            }
            else
            {
                if (e >= m_entityToIndex.size())
                {
                    return;
                }
                uint32_t idx = m_entityToIndex[e];
                if (idx == kInvalid)
                {
                    return;
                }
                uint32_t last = static_cast<uint32_t>(m_storage.size() - 1);
                if (idx != last)
                {
                    m_storage[idx] = std::move(m_storage[last]);
                    Entity movedEnt = m_indexToEntity[last];
                    m_indexToEntity[idx] = movedEnt;
                    m_entityToIndex[movedEnt] = idx;
                }
                m_storage.pop_back();
                m_entityToIndex[e] = kInvalid;
                m_indexToEntity[last] = kInvalid;
            }
        }

        /*-------------------- multi helpers -----------*/
        std::vector<T>* get_all_components(Entity e) requires IsMultiComponent<T>::value
        {
            // 1) ステージングバッファ中のマルチコンポーネントを優先
            auto itStaging = m_stagingMulti.find(e);
            if (itStaging != m_stagingMulti.end() && !itStaging->second.empty())
            {
                return &itStaging->second;
            }

            // 2) なければ本番マップを返す
            auto itMain = m_multi.find(e);
            return (itMain != m_multi.end() && !itMain->second.empty())
                ? &itMain->second
                : nullptr;
        }
        // マルチコンポーネント用：特定インデックスの要素を消す
        void remove_instance(Entity e, size_t index) requires IsMultiComponent<T>::value
        {
            auto it = m_multi.find(e);
            if (it == m_multi.end())
            {
                return;
            }
            auto& vec = it->second;
            if (index >= vec.size())
            {
                return;
            }
            vec.erase(vec.begin() + index);
            if (vec.empty())
            {
                // すべて消えたらバケットからも削除
                m_multi.erase(e);
            }
        }
        void remove_all(Entity e) requires IsMultiComponent<T>::value { m_multi.erase(e); }

        /*-------------------- copy --------------------*/
        void copy_component(Entity src, Entity dst) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto it = m_multi.find(src);
                if (it == m_multi.end() || it->second.empty()) { return; }
                // shallow copy
                auto& vecSrc = it->second;
                auto& vecDst = m_multi[dst] = vecSrc; // copy vector
                vecDst;
                //           for (auto& inst : vecDst)
                //           {
                               ////inst.set_owner(dst);
                //           }
            }
            else
            {
                uint32_t idxSrc = m_entityToIndex[src];
                if (idxSrc == kInvalid) { return; }
                // すでに dst にコンポーネントがある場合
                if (dst < m_entityToIndex.size() && m_entityToIndex[dst] != kInvalid)
                {
                    auto& c = m_storage[m_entityToIndex[dst]];
                    c = m_storage[idxSrc];
                }
                else
                {
                    // 新規にコピーする場合
                    m_storage.emplace_back(m_storage[idxSrc]);
                    auto& newComp = m_storage.back();
                    newComp;

                    uint32_t idxDst = static_cast<uint32_t>(m_storage.size() - 1);
                    if (m_entityToIndex.size() <= dst)
                    {
                        m_entityToIndex.resize(dst + 1, kInvalid);
                    }
                    if (m_indexToEntity.size() <= idxDst)
                    {
                        m_indexToEntity.resize(idxDst + 1, kInvalid);
                    }
                    m_entityToIndex[dst] = idxDst;
                    m_indexToEntity[idxDst] = dst;
                }
                /*T* comp = get_component(dst);
                if (comp)
                {
                    comp->set_owner(dst);
                }*/
            }
        }

        // 更新中のステージング用コピーをオーバーライド
        void copy_component_staging(Entity src, Entity dst) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                // マルチコンポーネントなら全インスタンスをステージングに追加
                auto it = m_multi.find(src);
                if (it == m_multi.end())
                {
                    return;
                }
                for (auto const& inst : it->second)
                {
                    m_stagingMulti[dst].push_back(inst);
                    //m_stagingMulti[dst].back().set_owner(dst);
                    //m_stagingMulti[dst].back().set_ecs_manager(m_pEcs);
                }
            }
            else
            {
                // シングルコンポーネントなら最新の値をステージングバッファに上書き
                T* srcComp = get_component(src);
                if (!srcComp)
                {
                    return;
                }
                m_stagingSingle[dst] = *srcComp;
                //m_stagingSingle[dst].set_owner(dst);
                //m_stagingSingle[dst].set_ecs_manager(m_pEcs);
            }
        }

        void clone_raw_component_to(Entity dst, void* raw) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                // マルチコンポーネントなら vector<T> 全体をコピー
                auto* srcVec = static_cast<std::vector<T>*>(raw);
                m_multi[dst] = *srcVec;
            }
            else
            {
                // 単一コンポーネントなら add_component＋コピー代入
                T* dstComp = add_component(dst);
                *dstComp = *static_cast<T*>(raw);
            }
        }

        /// Prefab復元時だけ使う、「生のコピー」を行う
        void prefab_clone_raw(Entity e, void* rawPtr)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                // マルチコンポーネントなら vector<T> 全体を直接コピー
                auto* srcVec = static_cast<std::vector<T>*>(rawPtr);
                m_multi[e] = *srcVec;  // コピーコンストラクタを使う
            }
            else
            {
                // 単一コンポーネントなら storage に直接 emplace_back
                // （operator= ではなく、T のコピーコンストラクタで構築される）
                if (m_entityToIndex.size() <= e)
                {
                    m_entityToIndex.resize(e + 1, kInvalid);
                }
                uint32_t idx = static_cast<uint32_t>(m_storage.size());
                m_storage.emplace_back(*static_cast<T*>(rawPtr));
                if (m_indexToEntity.size() <= idx)
                {
                    m_indexToEntity.resize(idx + 1, kInvalid);
                }
                m_entityToIndex[e] = idx;
                m_indexToEntity[idx] = e;
            }
        }

        void add_prefab_component_staging(Entity e, const T& comp)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                m_stagingMulti[e].push_back(comp);
                //m_stagingMulti[e].back().set_owner(e);
                //m_stagingMulti[e].back().set_ecs_manager(m_pEcs);
            }
            else
            {
                m_stagingSingle[e] = comp;
                //m_stagingSingle[e].set_owner(e);
                //m_stagingSingle[e].set_ecs_manager(m_pEcs);
            }
        }

        void add_prefab_component(Entity e, const T& comp)
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                m_multi[e].push_back(comp);
            }
            else
            {
                if (m_entityToIndex.size() <= e)
                {
                    m_entityToIndex.resize(e + 1, kInvalid);
                }
                uint32_t& idx = m_entityToIndex[e];
                if (idx == kInvalid)
                {
                    idx = static_cast<uint32_t>(m_storage.size());
                    m_storage.push_back(comp);
                    if (m_indexToEntity.size() <= idx)
                    {
                        m_indexToEntity.resize(idx + 1, kInvalid);
                    }
                    m_indexToEntity[idx] = e;
                }
                else
                {
                    m_storage[idx] = comp;
                }
            }
        }

        // ─── 削除前のクリーンアップ
        void cleanup(Entity e) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto it = m_multi.find(e);
                if (it != m_multi.end())
                {
                    for (auto& inst : it->second)
                    {
                        inst.initialize();
                    }
                }
            }
            else
            {
                T* comp = get_component(e);
                if (comp)
                {
                    comp->initialize();
                }
            }
        }

        /*-------------------- static ID --------------*/
        static CompID get_id()
        {
            //static CompID id = ++ECSManager::m_nextCompTypeId; return id;
            return component_id<T>();
        }

        /*-------------------- expose map -------------*/
        auto& map() { return m_multi; }
        const auto& map() const { return m_multi; }

        // 単一コンポーネントを void* で取得
        void* get_raw_component(Entity e) const override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto itS = m_stagingMulti.find(e);
                if (itS != m_stagingMulti.end() && !itS->second.empty())
                {
                    return (void*)&itS->second;
                }
                auto itM = m_multi.find(e);
                return (itM != m_multi.end() && !itM->second.empty())
                    ? (void*)&itM->second
                    : nullptr;
            }
            else
            {
                auto itS = m_stagingSingle.find(e);
                if (itS != m_stagingSingle.end())
                {
                    return (void*)&itS->second;
                }
                if (e >= m_entityToIndex.size())
                {
                    return nullptr;
                }
                uint32_t idx = m_entityToIndex[e];
                return idx != kInvalid ? (void*)&m_storage[idx] : nullptr;
            }
        }

        // 任意の型を shared_ptr<void> に包んでPrefabにコピー
        std::shared_ptr<void> clone_component(CompID, void* ptr) override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto src = static_cast<std::vector<T>*>(ptr);
                return std::make_shared<std::vector<T>>(*src); // Deep copy
            }
            else
            {
                T* src = static_cast<T*>(ptr);
                return std::make_shared<T>(*src); // Deep copy
            }
        }
        size_t get_component_count(Entity e) const override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                auto it = m_multi.find(e);
                return (it == m_multi.end()) ? 0u : it->second.size();
            }
            else
            {
                if (e >= m_entityToIndex.size())
                {
                    return 0u;
                }
                return (m_entityToIndex[e] != kInvalid) ? 1u : 0u;
            }
        }
        void flush_staging() override
        {
            if constexpr (IsMultiComponent<T>::value)
            {
                for (auto& [e, stagingVec] : m_stagingMulti)
                {
                    auto& vec = m_multi[e];
                    vec.insert(vec.end(), stagingVec.begin(), stagingVec.end());
                }
                m_stagingMulti.clear();
            }
            else
            {
                for (auto& [e, comp] : m_stagingSingle)
                {
                    if (m_entityToIndex.size() <= e)
                    {
                        m_entityToIndex.resize(e + 1, kInvalid);
                    }
                    uint32_t idx = m_entityToIndex[e];

                    if (idx == kInvalid)
                    {
                        idx = static_cast<uint32_t>(m_storage.size());
                        m_storage.push_back(comp);
                        if (m_indexToEntity.size() <= idx)
                        {
                            m_indexToEntity.resize(idx + 1, kInvalid);
                        }
                        m_entityToIndex[e] = idx;
                        m_indexToEntity[idx] = e;
                    }
                    else
                    {
                        m_storage[idx] = comp; // 上書き
                    }
                }
                m_stagingSingle.clear();
            }
        }
        const std::unordered_map<Entity, std::vector<T>>& get_staging_multi() const
        {
            return m_stagingMulti;
        }
    private:
        Storage                         m_storage;          // dense
        std::vector<uint32_t>           m_entityToIndex;    // entity -> index
        std::vector<Entity>             m_indexToEntity;    // index  -> entity
        std::unordered_map<Entity, std::vector<T>> m_multi; // multi‑instance
        std::unordered_map<Entity, T> m_stagingSingle; // フレーム中に追加された T
        std::unordered_map<Entity, std::vector<T>> m_stagingMulti;
    };

    template<ComponentType T>
    ComponentPool<T>* get_component_pool()
    {
        auto it = m_typeToComponents.find(ComponentPool<T>::get_id());
        if (it == m_typeToComponents.end())
        {
            return nullptr;
        }
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template<ComponentType T>
    ComponentPool<T>& ensure_pool()
    {
        CompID id = ComponentPool<T>::get_id();
        auto [it, inserted] = m_typeToComponents.try_emplace(
            id,
            std::make_shared<ComponentPool<T>>(4096)
        );
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    class ISystem
    {
    public:
        ISystem() : m_pEcs(nullptr) {}
        /// 開始時に一度だけ呼ばれる
        virtual void initialize() {}
        /// 毎フレーム呼ばれる
        virtual void update() = 0;
        /// 終了時に一度だけ呼ばれる
        virtual void finalize() {}
        /// 一度だけ呼ばれる
        virtual void awake() {}
        virtual ~ISystem() = default;
        /// ECSManager に登録されたタイミングで呼び出される
        virtual void on_register(ECSManager* ecs)
        {
            m_pEcs = ecs;
        }
        /// フレーム中に遅延で追加されたエンティティ／コンポーネントを受け取って、
        /// そのエンティティだけフェーズの処理を走らせたいときに使う
        virtual void initialize_entity(Entity) {}
        virtual void finalize_entity(Entity) {}
        virtual void awake_entity(Entity) {}
        virtual int get_priority() const { return m_priority; }
        virtual void set_priority(int p) { m_priority = p; }
        virtual bool is_enabled() const { return m_enabled; }
        virtual void set_enabled(bool e) { m_enabled = e; }
    protected:
        ECSManager* m_pEcs = nullptr; // ECSManager へのポインタ（初期化時に設定される）
        uint32_t m_priority = 0; // 優先度
        bool m_enabled = true; // 有効フラグ
    };

    /*---------------------------------------------------------------------
        System & multi_system  (unchanged logic)
    ---------------------------------------------------------------------*/
    template<ComponentType... T>
    class System : public ISystem
    {
        static constexpr bool kNoMulti = (!IsMultiComponent<T>::value && ...);
        static_assert(kNoMulti, "System<T...> cannot include multi-instance components");
        using UpdateFunc = std::function<void(Entity, T&...)>;
        using InitFunc = std::function<void(Entity, T&...)>;
        using FinFunc = std::function<void(Entity, T&...)>;
        using AwakeFunc = std::function<void(Entity, T&...)>;
    public:
        explicit System(
            UpdateFunc u,
            InitFunc   i = {},
            FinFunc    f = {},
            AwakeFunc  w = {})
            : m_update(u), m_init(i), m_fin(f), m_awake(w)
        {
            (m_required.set(ComponentPool<T>::get_id()), ...);
        }
        // 初期化フェーズでエンティティごとの処理
        void initialize() override
        {
            if (!m_init)
            {
                return;
            }
            for (auto& [arch, bucket] : m_pEcs->get_arch_to_entities())
            {
                if ((arch & m_required) != m_required)
                {
                    continue;
                }
                for (Entity e : bucket.get_entities())
                {
                    if (!m_pEcs->is_entity_active(e))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e) != nullptr) && ...))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e)->is_active()) && ...))
                    {
                        continue;
                    }
                    m_init(e, *m_pEcs->get_component<T>(e)...);
                }
            }
        }
        void update() override
        {
            for (auto& [arch, bucket] : m_pEcs->get_arch_to_entities())
            {
                // アーキタイプフィルタ
                if ((arch & m_required) != m_required)
                {
                    continue;
                }

                for (Entity e : bucket.get_entities())
                {
                    // 1) エンティティが非アクティブならスキップ
                    if (!m_pEcs->is_entity_active(e))
                    {
                        continue;
                    }

                    // 2) 全コンポーネントが存在するかチェック
                    if (!((m_pEcs->get_component<T>(e) != nullptr) && ...))
                    {
                        continue;
                    }

                    // 3) 全コンポーネントがアクティブかチェック
                    if (!((m_pEcs->get_component<T>(e)->is_active()) && ...))
                    {
                        continue;
                    }

                    // 4) 問題なければコールバック
                    m_update(e, *m_pEcs->get_component<T>(e)...);
                }
            }
        }
        // 終了フェーズでエンティティごとの処理
        void finalize() override
        {
            if (!m_fin)
            {
                return;
            }
            for (auto& [arch, bucket] : m_pEcs->get_arch_to_entities())
            {
                if ((arch & m_required) != m_required)
                {
                    continue;
                }
                for (Entity e : bucket.get_entities())
                {
                    if (!m_pEcs->is_entity_active(e))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e) != nullptr) && ...))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e)->is_active()) && ...))
                    {
                        continue;
                    }
                    m_fin(e, *m_pEcs->get_component<T>(e)...);
                }
            }
        }
        // awake フェーズでエンティティごとの処理
        void awake() override
        {
            if (!m_awake)
            {
                return;
            }
            for (auto& [arch, bucket] : m_pEcs->get_arch_to_entities())
            {
                if ((arch & m_required) != m_required)
                {
                    continue;
                }
                for (Entity e : bucket.get_entities())
                {
                    if (!m_pEcs->is_entity_active(e))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e) != nullptr) && ...))
                    {
                        continue;
                    }
                    if (!((m_pEcs->get_component<T>(e)->is_active()) && ...))
                    {
                        continue;
                    }
                    m_awake(e, *m_pEcs->get_component<T>(e)...);
                }
            }
        }
        void initialize_entity(Entity e) override
        {
            if (!m_init)
            {
                return;
            }
            // ステージングも含めて、必要なコンポーネントが存在＆アクティブかチェック
            if (!((m_pEcs->get_component<T>(e) && m_pEcs->get_component<T>(e)->is_active()) && ...))
            {
                return;
            }
            // 初期化コールバックを実行
            m_init(e, *m_pEcs->get_component<T>(e)...);
        }
        void finalize_entity(Entity e) override
        {
            if (!m_fin)
            {
                return;
            }
            // ステージングも含めて、必要なコンポーネントが存在＆アクティブかチェック
            if (!((m_pEcs->get_component<T>(e) && m_pEcs->get_component<T>(e)->is_active()) && ...))
            {
                return;
            }
            // 終了コールバック
            m_fin(e, *m_pEcs->get_component<T>(e)...);
        }
        void awake_entity(Entity e) override
        {
            if (!m_awake)
            {
                return;
            }
            // ステージングも含めて、必要なコンポーネントが存在＆アクティブかチェック
            if (!((m_pEcs->get_component<T>(e) && m_pEcs->get_component<T>(e)->is_active()) && ...))
            {
                return;
            }
            // Awakeコールバック
            m_awake(e, *m_pEcs->get_component<T>(e)...);
        }
        const Archetype& get_required() const { return m_required; }
    private:
        Archetype          m_required;
        UpdateFunc         m_update;
        InitFunc           m_init;
        FinFunc            m_fin;
        AwakeFunc          m_awake; // 追加：Awake用のコールバック
    };

    template<ComponentType T>
        requires IsMultiComponent<T>::value
    class MultiComponentSystem : public ISystem
    {
        using InitFunc = std::function<void(Entity, std::vector<T>&)>;
        using UpdateFunc = std::function<void(Entity, std::vector<T>&)>;
        using FinFunc = std::function<void(Entity, std::vector<T>&)>;
        using AwakeFunc = std::function<void(Entity, std::vector<T>&)>;
    public:
        explicit MultiComponentSystem(
            UpdateFunc u,
            InitFunc   i = {},
            FinFunc    f = {},
            AwakeFunc  w = {})
            : m_update(u), m_init(i), m_fin(f), m_awake(w)
        {
        }
        // 起動時に一度だけ呼ばれる
        void initialize() override
        {
            if (!m_init)
            {
                return;
            }
            processAll(m_init);
        }

        // 毎フレーム呼ばれる
        void update() override
        {
            processAll(m_update);
        }

        // 終了時に一度だけ呼ばれる
        void finalize() override
        {
            if (!m_fin)
            {
                return;
            }
            processAll(m_fin);
        }

        // awake フェーズでエンティティごとの処理
        void awake() override
        {
            if (!m_awake)
            {
                return;
            }
            processAll(m_awake);
        }
        void initialize_entity(Entity e) override
        {
            if (!m_init)
            {
                return;
            }
            auto* pool = m_pEcs->get_component_pool<T>();
            if (!pool)
            {
                return;
            }

            std::vector<T> filtered;

            // 1) ステージングバッファのチェック
            const auto& stagingMap = pool->get_staging_multi(); // m_stagingMulti への参照を返す
            if (auto itS = stagingMap.find(e); itS != stagingMap.end())
            {
                for (auto& inst : itS->second)
                {
                    if (inst.is_active())
                    {
                        filtered.push_back(inst);
                    }
                }
            }

            // 2) 本番データのチェック
            const auto& mainMap = pool->map(); // 既存の m_multi
            if (auto itM = mainMap.find(e); itM != mainMap.end())
            {
                for (auto& inst : itM->second)
                {
                    if (inst.is_active())
                    {
                        filtered.push_back(inst);
                    }
                }
            }

            if (!filtered.empty())
            {
                m_init(e, filtered);
            }
        }
        void finalize_entity(Entity e) override
        {
            if (!m_fin)
            {
                return;
            }
            auto* pool = m_pEcs->get_component_pool<T>();
            if (!pool)
            {
                return;
            }
            // 本番バッファのみ（ステージング反映後にここが呼ばれる想定）
            auto it = pool->map().find(e);
            if (it == pool->map().end() || it->second.empty())
            {
                return;
            }
            // 有効なものだけを抜き出して呼ぶ
            std::vector<T> tmp;
            for (auto& c : it->second)
            {
                if (c.is_active())
                {
                    tmp.push_back(c);
                }
            }
            if (!tmp.empty())
            {
                m_fin(e, tmp);
            }
        }
        void awake_entity(Entity e) override
        {
            if (!m_awake)
            {
                return;
            }
            auto* pool = m_pEcs->get_component_pool<T>();
            if (!pool)
            {
                return;
            }

            std::vector<T> filtered;

            // 1) ステージングバッファのチェック
            const auto& stagingMap = pool->get_staging_multi(); // m_stagingMulti への参照を返す
            if (auto itS = stagingMap.find(e); itS != stagingMap.end())
            {
                for (auto& inst : itS->second)
                {
                    if (inst.is_active())
                    {
                        filtered.push_back(inst);
                    }
                }
            }

            // 2) 本番データのチェック
            const auto& mainMap = pool->map(); // 既存の m_multi
            if (auto itM = mainMap.find(e); itM != mainMap.end())
            {
                for (auto& inst : itM->second)
                {
                    if (inst.is_active())
                    {
                        filtered.push_back(inst);
                    }
                }
            }

            if (!filtered.empty())
            {
                m_awake(e, filtered);
            }
        }
    private:
        // 実際にプールを走査してコールバックを呼び出す共通処理
        template<typename Func>
        void processAll(const Func& func)
        {
            auto* pool = m_pEcs->get_component_pool<T>();
            if (!pool)
            {
                return;
            }

            for (auto& [e, vec] : pool->map())
            {
                // 1) エンティティがアクティブでない、またはインスタンスが空ならスキップ
                if (!m_pEcs->is_entity_active(e) || vec.empty())
                {
                    continue;
                }

                // 2) インスタンスごとに is_active フラグを確認して、filteredVec を作る
                std::vector<T> filtered;
                filtered.reserve(vec.size());
                for (auto& inst : vec)
                {
                    if (inst.is_active())
                    {
                        filtered.push_back(inst);
                    }
                }

                // 3) 有効インスタンスがひとつでもあればコール
                if (!filtered.empty())
                {
                    func(e, filtered);
                }
            }
        }

        UpdateFunc         m_update;
        InitFunc           m_init;
        FinFunc            m_fin;
        AwakeFunc          m_awake; // 追加：Awake用のコールバック
    };

    std::unordered_map<Archetype, EntityContainer>& get_arch_to_entities() { return m_archToEntities; }

private:
    void notify_component_added(Entity e, CompID c)
    {
        for (auto& wp : m_componentListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_component_added(e, c); // Notify listeners
            }
        }
    }
    void notify_component_copied(Entity src, Entity dst, CompID c)
    {
        for (auto& wp : m_componentListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_component_copied(src, dst, c); // Notify listeners
            }
        }
    }
    void notify_component_removed(Entity e, CompID c)
    {
        for (auto& wp : m_componentListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_component_removed(e, c);
            }
        }
    }
    void notify_component_removed_instance(Entity e, CompID c, void* v, size_t i)
    {
        for (auto& wp : m_componentListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_component_removed_instance(e, c, v, i);
            }
        }
    }
    void notify_component_restored_from_prefab(Entity e, CompID c)
    {
        for (auto& wp : m_componentListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_component_restored_from_prefab(e, c);
            }
        }
    }
    bool is_multi_component_by_id(CompID id) const
    {
        auto it = m_typeToComponents.find(id);
        if (it != m_typeToComponents.end())
        {
            return it->second->is_multi_component_trait(id);
        }
        return false;
    }
    //――――――――――――――――――
    // remove_entity の本体（clear_entity → リスナ通知 → 再利用キュー）
    //――――――――――――――――――
    void remove_entity_impl(Entity e)
    {
        // 1) すべてのコンポーネントをクリア（イベント込み）
        clear_entity(e);
        // 2) 非アクティブ化
        m_entityToActive[e] = false;
        // 3) リサイクル
        m_recycleEntities.push_back(e);
        // 4) エンティティ破棄イベント
        for (auto& wp : m_entityListeners)
        {
            if (auto sp = wp.lock())
            {
                sp->on_entity_destroyed(e);
            }
        }
    }

    // フレーム末にまとめてコマンドを実行
    void flush_deferred()
    {
        for (auto& cmd : m_deferredCommands)
        {
            cmd();
        }
        m_deferredCommands.clear();
    }

    /*-------------------- data members --------------------------------*/

    bool                                                        m_isUpdating = false;
    bool                                                        m_cancelUpdate = false;
    Entity                                                      m_nextEntityId = 0;
    std::vector<bool>                                           m_entityToActive;
    std::vector<Entity>                                         m_recycleEntities;
    std::vector<Entity>                                         m_stagingEntities;            // フレーム中に生成された Entity の一覧
    std::vector<bool>                                           m_stagingEntityActive;          // 各 Entity の一時アクティブ状態
    std::vector<Archetype>                                      m_stagingEntityArchetypes; // 各 Entity の一時 Archetype
    std::vector<Archetype>                                      m_entityToArchetype;
    std::unordered_map<CompID, int>                             m_deletePriority;   // デフォルトは 0 (CompID 昇順になる)
    std::unordered_map<CompID, int>                             m_copyPriority;   // コピー時の優先度マップ
    std::vector<std::function<void()>>                          m_deferredCommands;
    std::vector<std::weak_ptr<IEntityEventListener>>            m_entityListeners;
    std::vector<std::unique_ptr<ISystem>>                       m_systems;
    std::vector<std::weak_ptr<IIComponentEventListener>>        m_componentListeners;
    std::unordered_map<Archetype, EntityContainer>              m_archToEntities;
    std::unordered_map<CompID, std::shared_ptr<IComponentPool>> m_typeToComponents;
    double                                                      m_lastTotalUpdateTimeMs = 0.0; // 最後に計測した全システム更新の所要時間（ms）
    std::unordered_map<std::type_index, double>                 m_lastSystemUpdateTimeMs; // 各システム型ごとに最後に計測した更新時間（ms）

    // initialize／finalize の計測用
    double m_lastTotalInitializeTimeMs = 0.0;
    double m_lastTotalFinalizeTimeMs = 0.0;
    double m_lastTotalAwakeTimeMs = 0.0;

    std::unordered_map<std::type_index, double>                 m_lastSystemInitializeTimeMs;
    std::unordered_map<std::type_index, double>                 m_lastSystemFinalizeTimeMs;
    std::unordered_map<std::type_index, double>                 m_lastSystemAwakeTimeMs;

    // SystemごとのUpdate直前に処理する初期化対象格納用
    std::unordered_map<std::type_index, std::vector<Entity>>    m_pendingInitBeforeUpdate;
    // 新規に追加された Entity を記録するバッファ
    std::vector<Entity>                                         m_newEntitiesLastFrame;
};

struct IComponentEventListener : public IIComponentEventListener
{
    friend class ECSManager;
public:
    IComponentEventListener() = default;
    virtual ~IComponentEventListener()
    {
        m_onAddSingle.clear();
    }

    // 依存する ECSManager のポインタをセット
    void set_ecs_manager(ECSManager* ecs) { m_pEcs = ecs; }

    // 単一用
    template<ComponentType T>
    void register_on_add(std::function<void(Entity, T*)> f)
    {
        static_assert(!IsMultiComponent<T>::value, "Use singleComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onAddSingle[id].push_back(
            [f](Entity e, IComponentTag* raw) {
                f(e, static_cast<T*>(raw));
            }
        );
    }
    // マルチ用
    template<ComponentType T>
    void register_on_add(std::function<void(Entity, T*, size_t)> f)
    {
        static_assert(IsMultiComponent<T>::value, "Use multiComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onAddMulti[id].push_back(
            [f](Entity e, void* rawVec, size_t idx) {
                auto* vec = static_cast<std::vector<T>*>(rawVec);
                f(e, &(*vec)[idx], idx);
            }
        );
    }

    // 単一コンポーネント用
    template<ComponentType T>
    void register_on_copy(std::function<void(Entity, Entity, T*)> f)
    {
        static_assert(!IsMultiComponent<T>::value, "Use singleComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onCopySingle[id].push_back(
            [f](Entity src, Entity dst, IComponentTag* raw) {
                f(src, dst, static_cast<T*>(raw));
            }
        );
    }

    // マルチコンポーネント用
    template<ComponentType T>
    void register_on_copy(std::function<void(Entity, Entity, T*, size_t)> f)
    {
        static_assert(IsMultiComponent<T>::value, "Use multiComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onCopyMulti[id].push_back(
            [f](Entity src, Entity dst, void* rawVec, size_t idx) {
                auto* vec = static_cast<std::vector<T>*>(rawVec);
                f(src, dst, &(*vec)[idx], idx);
            }
        );
    }

    // 単一コンポーネント用
    template<ComponentType T>
    void register_on_remove(std::function<void(Entity, T*)> f)
    {
        static_assert(!IsMultiComponent<T>::value, "Use singleComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onRemoveSingle[id].push_back(
            [f](Entity e, IComponentTag* raw) {
                f(e, static_cast<T*>(raw));
            }
        );
    }

    // マルチコンポーネント用（インスタンスごとに index 付き）
    template<ComponentType T>
    void register_on_remove(std::function<void(Entity, T*, size_t)> f)
    {
        static_assert(IsMultiComponent<T>::value, "Use multiComponent");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onRemoveMulti[id].push_back(
            [f](Entity e, void* rawVec, size_t idx) {
                auto* vec = static_cast<std::vector<T>*>(rawVec);
                f(e, &(*vec)[idx], idx);
            }
        );
    }
    // 単一コンポーネント向け
    template<ComponentType T>
    void register_on_restore(std::function<void(Entity, T*)> f)
    {
        static_assert(!IsMultiComponent<T>::value, "Use multi for multi-component");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onRestoreSingle[id].push_back(
            [f](Entity e, IComponentTag* raw) {
                f(e, static_cast<T*>(raw));
            }
        );
    }

    // マルチコンポーネント向け
    template<ComponentType T>
    void register_on_restore(std::function<void(Entity, T*, size_t)> f)
    {
        static_assert(IsMultiComponent<T>::value, "Use single for single-component");
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_onRestoreMulti[id].push_back(
            [f](Entity e, void* rawVec, size_t idx) {
                auto* vec = static_cast<std::vector<T>*>(rawVec);
                f(e, &(*vec)[idx], idx);
            }
        );
    }
private:
    // ECS側から呼ばれる
    void on_component_added(Entity e, CompID compType) override
    {
        // 1) 単一コンポーネント向け
        if (auto it = m_onAddSingle.find(compType); it != m_onAddSingle.end())
        {
            void* raw = m_pEcs->get_raw_component_pool(compType)->get_raw_component(e);
            for (auto& cb : it->second)
            {
                cb(e, static_cast<IComponentTag*>(raw));
            }
        }

        // 2) マルチコンポーネント向け
        if (auto it2 = m_onAddMulti.find(compType); it2 != m_onAddMulti.end())
        {
            ECSManager::IComponentPool* pool = m_pEcs->get_raw_component_pool(compType);
            void* rawVec = pool->get_raw_component(e);
            size_t count = pool->get_component_count(e);

            // **新しく追加されたインスタンスの index = count-1**
            size_t idxNew = (count == 0 ? 0 : count - 1);

            for (auto& cb : it2->second)
            {
                cb(e, rawVec, idxNew);
            }
        }
    }
    void on_component_copied(Entity src, Entity dst, CompID compType)override
    {
        // 単一コンポーネント向け
        if (auto it = m_onCopySingle.find(compType); it != m_onCopySingle.end())
        {
            void* raw = m_pEcs->get_raw_component_pool(compType)->get_raw_component(dst);
            for (auto& cb : it->second)
            {
                cb(src, dst, static_cast<IComponentTag*>(raw));
            }
        }

        // マルチコンポーネント向け
        if (auto it2 = m_onCopyMulti.find(compType); it2 != m_onCopyMulti.end())
        {
            ECSManager::IComponentPool* pool = m_pEcs->get_raw_component_pool(compType);
            void* rawVec = pool->get_raw_component(dst);
            size_t count = pool->get_component_count(dst);

            for (size_t idx = 0; idx < count; ++idx)
            {
                for (auto& cb : it2->second)
                {
                    cb(src, dst, rawVec, idx);
                }
            }
        }
    }
    void on_component_removed(Entity e, CompID compType)override
    {
        // 1) 単一コンポーネント向け
        if (auto it = m_onRemoveSingle.find(compType); it != m_onRemoveSingle.end())
        {
            void* raw = m_pEcs->get_raw_component_pool(compType)
                ->get_raw_component(e);
            for (auto& cb : it->second)
            {
                cb(e, static_cast<IComponentTag*>(raw));
            }
        }

        // 2) マルチコンポーネント向け
        if (auto it2 = m_onRemoveMulti.find(compType); it2 != m_onRemoveMulti.end())
        {
            ECSManager::IComponentPool* pool = m_pEcs->get_raw_component_pool(compType);
            void* rawVec = pool->get_raw_component(e);
            size_t count = pool->get_component_count(e);

            for (size_t idx = 0; idx < count; ++idx)
            {
                for (auto& cb : it2->second)
                {
                    cb(e, rawVec, idx);
                }
            }
        }
    }
    void on_component_removed_instance(Entity e, CompID compType, void* rawVec, size_t idx)override
    {
        if (auto it = m_onRemoveMulti.find(compType); it != m_onRemoveMulti.end())
        {
            for (auto& cb : it->second)
            {
                cb(e, rawVec, idx);
            }
        }
    }

    void on_component_restored_from_prefab(Entity e, CompID compType) override
    {
        // 単一
        if (auto it = m_onRestoreSingle.find(compType); it != m_onRestoreSingle.end())
        {
            void* raw = m_pEcs->get_raw_component_pool(compType)->get_raw_component(e);
            for (auto& cb : it->second)
            {
                cb(e, static_cast<IComponentTag*>(raw));
            }
        }
        // マルチ
        if (auto it2 = m_onRestoreMulti.find(compType); it2 != m_onRestoreMulti.end())
        {
            auto* pool = m_pEcs->get_raw_component_pool(compType);
            void* rawVec = pool->get_raw_component(e);
            size_t count = pool->get_component_count(e);
            for (size_t idx = 0; idx < count; ++idx)
            {
                for (auto& cb : it2->second)
                {
                    cb(e, rawVec, idx);
                }
            }
        }
    }
private:
    ECSManager* m_pEcs = nullptr;
protected:
    // 単一用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, IComponentTag*)>>>  m_onAddSingle;
    // マルチ用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, void*, size_t)>>>      m_onAddMulti;
    // 単一用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, Entity, IComponentTag*)>>> m_onCopySingle;
    // マルチ用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, Entity, void*, size_t)>>> m_onCopyMulti;
    // 単一用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, IComponentTag*)>>> m_onRemoveSingle;
    // マルチ用
    std::unordered_map<CompID, std::vector<std::function<void(Entity, void*, size_t)>>> m_onRemoveMulti;
    // Restore（Prefab 復元）用マップ
    std::unordered_map<CompID, std::vector<std::function<void(Entity, IComponentTag*)>>> m_onRestoreSingle;
    std::unordered_map<CompID, std::vector<std::function<void(Entity, void*, size_t)>>>    m_onRestoreMulti;
};

class IPrefab
{
public:
    IPrefab() = default;

    //――――――――――――――――――
    // 1) 事前登録：扱う型すべてに対して呼ぶ
    // Prefab::register_copy_func<YourComponent>();
    //――――――――――――――――――
    template<ComponentType T>
    static void register_copy_func()
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();

        if constexpr (IsMultiComponent<T>::value)
        {
            m_multiCopyFuncs[id] = [](Entity e, ECSManager& ecs, void* rawVec) {
                auto* vec = static_cast<std::vector<T>*>(rawVec);
                for (auto const& comp : *vec)
                {
                    T* dst = ecs.add_component<T>(e);
                    *dst = comp;
                }
                };
        }
        else
        {
            m_copyFuncs[id] = [](Entity e, ECSManager& ecs, void* raw) {
                T* dst = ecs.add_component<T>(e);
                *dst = *static_cast<T*>(raw);
                };
        }
    }

    template<ComponentType T>
    static void register_prefab_restore()
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();

        // 単一コンポーネント用
        m_prefabRestoreFuncs[id] = [](Entity e, ECSManager& ecs, void* raw) {
            ecs.prefab_add_component<T>(e, *static_cast<T*>(raw));
            };

        // マルチコンポーネント用
        m_prefabRestoreMultiFuncs[id] = [](Entity e, ECSManager& ecs, void* rawVec) {
            auto& vec = *static_cast<std::vector<T>*>(rawVec);
            for (auto& inst : vec)
            {
                ecs.prefab_add_component<T>(e, inst);
            }
            };
    }

    //――――――――――――――――――
    // 2) 既存エンティティから Prefab を作る
    //――――――――――――――――――
    static IPrefab from_entity(ECSManager& ecs, Entity e)
    {
        IPrefab prefab;
        const Archetype& arch = ecs.get_archetype(e);

        for (size_t id = 0; id < arch.size(); ++id)
        {
            if (!arch.test(id))
            {
                continue;
            }
            auto* pool = ecs.get_raw_component_pool(id);
            void* raw = pool->get_raw_component(e);
            if (!raw)
            {
                continue;
            }

            // 深いコピーを shared_ptr<void> で受け取る
            auto clonePtr = pool->clone_component(id, raw);
            if (pool->is_multi_component_trait(id))
            {
                prefab.m_multiComponents[id] = std::move(clonePtr);
            }
            else
                prefab.m_components[id] = std::move(clonePtr);

            prefab.m_archetype.set(id);
        }
        return prefab;
    }

    //――――――――――――――――――
    // 3) instantiate するとき
    //――――――――――――――――――
    Entity instantiate(ECSManager& ecs) const
    {
        // 1) エンティティ生成（派生で名前付けしたい場合はここをオーバーライド）
        Entity e = create_entity(ecs);

        // 2) コンポーネント復元（派生は基本的にそのまま使う）
        instantiate_components(e, ecs);

        // 3) 子 Prefab 再帰生成＆親子リンク
        instantiate_children(e, ecs);

        // 4) 完了フック（後始末的な処理があれば派生で）
        on_after_instantiate(e, ecs);

        return e;
    }

    //――――――――――――――――――
    // 4) Prefab にコンポーネントを追加するとき
    //――――――――――――――――――
    template<ComponentType T>
    void add_component(const T& comp)
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        if constexpr (IsMultiComponent<T>::value)
        {
            auto& ptr = m_multiComponents[id];
            if (!ptr)
            {
                ptr = std::make_shared<std::vector<T>>();
            }
            auto& vec = *std::static_pointer_cast<std::vector<T>>(ptr);
            vec.push_back(comp);
        }
        else
        {
            m_components[id] = std::make_shared<T>(comp);
        }
        m_archetype.set(id);
    }

    //――――――――――――――――――
    // 5) 子 Prefab を追加
    //――――――――――――――――――
    void add_sub_prefab(std::shared_ptr<IPrefab> child)
    {
        m_subPrefabs.push_back(std::move(child));
        // 必要なら m_archetype にもビットを立てる
    }

    //――――――――――――――――――
    // 6) 子 Prefab 一覧取得
    //――――――――――――――――――
    std::vector<std::shared_ptr<IPrefab>>& get_sub_prefabs() noexcept
    {
        return m_subPrefabs;
    }

    const std::vector<std::shared_ptr<IPrefab>>& get_sub_prefabs() const noexcept
    {
        return m_subPrefabs;
    }

    //――――――――――――――――――
    // コンポーネントを 1 つだけ持つ場合の取得（存在しなければ nullptr）
    //――――――――――――――――――
    template<ComponentType T>
    T* get_component() noexcept
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        auto it = m_components.find(id);
        if (it == m_components.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second).get();
    }

    //――――――――――――――――――
    // マルチコンポーネントをすべて取得（存在しなければ nullptr）
    //――――――――――――――――――
    template<ComponentType T>
    std::vector<T>* get_all_components() noexcept
        requires IsMultiComponent<T>::value
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        auto it = m_multiComponents.find(id);
        if (it == m_multiComponents.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<std::vector<T>>(it->second).get();
    }

    // 単一コンポーネント取得：存在しなければ nullptr
    template<ComponentType T>
    T* get_component_ptr() const noexcept
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        auto it = m_components.find(id);
        if (it == m_components.end())
        {
            return nullptr;
        }
        // shared_ptr<void> を shared_ptr<T> にキャストして生ポインタを返す
        auto ptr = std::static_pointer_cast<T>(it->second);
        return ptr.get();
    }

    // マルチコンポーネント取得：存在しなければ nullptr
    template<ComponentType T>
    std::vector<T>* get_all_components_ptr() const noexcept
        requires IsMultiComponent<T>::value
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        auto it = m_multiComponents.find(id);
        if (it == m_multiComponents.end())
        {
            return nullptr;
        }
        auto ptr = std::static_pointer_cast<std::vector<T>>(it->second);
        return ptr.get();
    }

    //――――――――――――――――――
    // 単一コンポーネントを上書きする
    //――――――――――――――――――
    template<ComponentType T>
    void set_component(const T& comp)
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_components[id] = std::make_shared<T>(comp);
        m_archetype.set(id);
    }

    //――――――――――――――――――
    // 単一コンポーネントを Prefab から削除
    //――――――――――――――――――
    template<ComponentType T>
    void remove_component()
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        // マップから単一コンポーネントのエントリを消す
        m_components.erase(id);
        // Archetype からビットをクリア
        m_archetype.reset(id);
    }

    //――――――――――――――――――
    // マルチコンポーネントの特定インスタンスを Prefab から削除
    //――――――――――――――――――
    template<ComponentType T>
    void remove_component_instance(size_t idx)
        requires IsMultiComponent<T>::value
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        auto it = m_multiComponents.find(id);
        if (it == m_multiComponents.end())
        {
            return;
        }

        auto& vec = *std::static_pointer_cast<std::vector<T>>(it->second);
        if (idx >= vec.size())
        {
            return;
        }
        vec.erase(vec.begin() + idx);

        // もし空になったら Prefab から完全に消す
        if (vec.empty())
        {
            m_multiComponents.erase(id);
            m_archetype.reset(id);
        }
    }

    //――――――――――――――――――
    // マルチコンポーネント全体を Prefab から削除
    //――――――――――――――――――
    template<ComponentType T>
    void clear_all_components()
        requires IsMultiComponent<T>::value
    {
        CompID id = ECSManager::ComponentPool<T>::get_id();
        m_multiComponents.erase(id);
        m_archetype.reset(id);
    }

protected:
    // —————— hooks ——————

    void populate_from_entity(ECSManager& ecs, Entity e)
    {
        const Archetype& arch = ecs.get_archetype(e);

        for (size_t id = 0; id < arch.size(); ++id)
        {
            if (!arch.test(id))
            {
                continue;
            }
            auto* pool = ecs.get_raw_component_pool(id);
            void* raw = pool->get_raw_component(e);
            if (!raw)
            {
                continue;
            }

            // 深いコピーを shared_ptr<void> で受け取る
            auto clonePtr = pool->clone_component(id, raw);
            if (pool->is_multi_component_trait(id))
            {
                m_multiComponents[id] = std::move(clonePtr);
            }
            else
                m_components[id] = std::move(clonePtr);

            m_archetype.set(id);
        }
    }

    // (1) まずエンティティを作る。追加の初期化／名前付けは here を override。
    virtual Entity create_entity(ECSManager& ecs) const
    {
        return ecs.generate_entity();
    }

    // (2) m_components/m_multiComponents を使って既存の復元ロジック
    virtual void instantiate_components(Entity e, ECSManager& ecs) const
    {
        // 単一コンポーネント
        for (auto const& [id, rawPtr] : m_components)
        {
            auto it = m_prefabRestoreFuncs.find(id);
            if (it != m_prefabRestoreFuncs.end())
            {
                it->second(e, ecs, rawPtr.get());
            }
        }

        // マルチコンポーネント
        for (auto const& [id, rawVec] : m_multiComponents)
        {
            auto it = m_prefabRestoreMultiFuncs.find(id);
            if (it != m_prefabRestoreMultiFuncs.end())
            {
                it->second(e, ecs, rawVec.get());
            }
        }
    }

    // (3) 子 Prefab を再帰的にインスタンス化し、リンク用フックを呼ぶ
    virtual void instantiate_children(Entity parent, ECSManager& ecs) const
    {
        for (auto const& child : m_subPrefabs)
        {
            Entity childEnt = child->instantiate(ecs);
            link_parent_child(parent, childEnt, ecs);
        }
    }

    // 親→子を結びつけたい場合はここを override
    virtual void link_parent_child(Entity /*parent*/, Entity /*child*/, ECSManager& /*ecs*/) const
    {
        // default: なにもしない
    }

    // (4) 全部終わったあとに追加処理したい場合はここ
    virtual void on_after_instantiate(Entity /*e*/, ECSManager& /*ecs*/) const
    {
        // default: なにもしない
    }

private:
    Archetype m_archetype;
    // void ポインタで型消去したストレージ
    std::unordered_map<CompID, std::shared_ptr<void>> m_components;
    std::unordered_map<CompID, std::shared_ptr<void>> m_multiComponents;

    // 追加：ネストされた Prefab のリスト
    std::vector<std::shared_ptr<IPrefab>> m_subPrefabs;

    // インスタンス化用マップ
    inline static std::unordered_map<CompID,
        std::function<void(Entity, ECSManager&, void*)>> m_copyFuncs;
    inline static std::unordered_map<CompID,
        std::function<void(Entity, ECSManager&, void*)>> m_multiCopyFuncs;
    // 復元処理マップ
    inline static std::unordered_map<CompID,
        std::function<void(Entity, ECSManager&, void*)>> m_prefabRestoreFuncs;
    inline static std::unordered_map<CompID,
        std::function<void(Entity, ECSManager&, void*)>> m_prefabRestoreMultiFuncs;
};

} // namespace 
