# GpuPipeline 仕様書（v0.2 / 清書）
更新日: 2026-01-23  
対象: DirectX 12 / 自作エンジン（GraphicsCore + GpuPipeline + FrameGraph）

---

## 0. 目的
**GpuPipeline** は「GPUに投げる全処理（レンダリング目的 / 非レンダリングCompute / Copy）を Pass 単位で定義し、依存解決・バリア・キュー同期を構築して実行する」ための統合パイプラインである。

- Pass を登録する（Render / Compute / Copy）
- FrameGraph を内部に持ち、依存解決・リソース寿命・バリア・同期を生成して実行する
- PSO/RootSignature/Shader など **デバイス寿命の生成とキャッシュ**は GraphicsCore（RenderDevice）が担当する

---

## 1. 非目的（やらないこと）
- ECS / ゲームロジックを持たない（正本は GameCore）
- PSO / RootSignature の「内容（どの技法を採用するか）」を GraphicsCore 側で決めない
- Persistent（永続）リソースの所有を FrameGraph に押し込まない
- Pass に EntityId 対応表や free-list、世代管理を持たせない

---

## 2. レイヤ構成と依存方向
依存は一方向。逆流させると破綻する。

```
GameCore/ECS  ──(CPUスナップショット/更新リスト)──>  GpuPipeline  ──>  GraphicsCore(RenderDevice)  ──>  D3D12
                                             └─(内部) FrameGraph
```

---

## 3. 用語
- **Pass**: GPU作業の最小単位（RenderPass / ComputePass / CopyPass）
- **FrameGraph**: Pass 間依存を解析し、リソース寿命・バリア・同期を構築して実行する仕組み
- **Transient**: そのフレーム内寿命の中間リソース（FrameGraph が所有）
- **Persistent**: フレームを跨ぐ永続リソース（WorldResource / ViewResource が所有）
- **Imported**: 外部から持ち込むリソース（SwapChain backbuffer 等）
- **RenderHandle**: Renderable登録で得るハンドル（index + generation）
- **PipelineKey**: PSO/RS/ShaderPermutation を一意にするキー（ハッシュ）

---

## 4. 主要コンポーネント

### 4.1 GraphicsCore（RenderDevice）
RenderDevice は「D3D12 Create* を呼ぶ唯一の場所」とする。

**責務**
- ID3D12Device 所有
- リソース生成（Buffer/Texture）
- View生成（SRV/UAV/CBV/RTV/DSV）
- ShaderLibrary（コンパイル結果のキャッシュ）
- RootSignatureCache（生成 + キャッシュ）
- PipelineStateCache（Graphics/Compute PSO 生成 + キャッシュ）
- Descriptor allocator / heap（基盤）
- 必要なら PipelineLibrary 永続化（任意）

**禁止**
- Forward/Deferred/Hybrid 等の方式決定
- Pass 構成、FrameGraph 構築
- EntityId 対応、Transform 統合運用（これは WorldResource）

---

### 4.2 GpuPipeline
**方式決定 + Pass登録 + FrameGraph build/execute を統制**する層。

**責務（確定）**
- FramesInFlight（1/2/3）を起動時に選べる
  - `1` は **デバッグ用途**として扱う（仕様として明記）
- RenderMode（Forward/Deferred/Hybrid）を起動時に選べる
- TransparencyMode（通常ブレンド / OIT 等）を起動時に選べる（実装は段階導入）
- TransformBufferMode（下記）を起動時に選べる
- Pass を登録して FrameGraph を構築し実行する
- Async Compute は **明示許可された Pass のみ**
- CopyQueue は **オプション**（同期モデルが完成している場合のみ）

---

### 4.3 FrameGraph（GpuPipeline 内部）
**Passを並べ、Transientを割当て、バリア/同期を構築して実行**する。

**責務（確定）**
- Transient リソースの生成/寿命/別名化（aliasing）
- Imported / Persistent の取り込み（import）
- リソース状態トラッキング（D3D12_RESOURCE_STATES）
- UAV barrier / aliasing barrier
- QueueType に応じた実行計画（Graphics/Compute/Copy）
- キュー間同期（Fence）生成（CopyQueue/AsyncCompute を使う場合）

**禁止**
- Persistent リソースの所有
- EntityId 対応表・free-list・世代管理

---

## 5. リソース分類（確定）

### 5.1 Transient（FrameGraph所有）
フレーム内でのみ生きる中間リソース。
- SSAO中間RT、Bloom chain、Hi-Z、テンポラリBuffer など

### 5.2 Persistent（所有者: WorldResource / ViewResource）
フレームを跨ぐ永続リソース。FrameGraphは import して使うだけ。

- **WorldResource**（Scene/World寿命）
  - Transform統合バッファ（EntityId→index、世代、free-list、activeCount、resize）
  - Instance/Material/Light の統合テーブル（段階導入）
  - IndirectArgs プール（必要になってから）

- **ViewResource**（Camera/Viewport寿命）
  - TAA/TSR/SSR などの履歴バッファ
  - velocity/露出等の履歴（必要になってから）
  - 解像度変更に伴う再生成管理

### 5.3 Imported（外部持ち込み）
SwapChain backbuffer、外部Texture、Readback 等。
- FrameGraph に import し、バリア/依存だけ解決させる。

---

## 6. ECS と同期（確定）
### 6.1 所有関係
- ECS は **GameCore が保持**
- Entity は **SceneManager 内の Scene が保持**
- ObjectID と EntityID は別（描画しないObjectが存在するため）
- RenderHandle は ECS の RenderComponent に保持
- 世代管理と対応表は **WorldResource が保持**

### 6.2 境界ルール（重要・確定）
- **ECS は GPU リソースに触らない**
- ECS System は Transform 等を更新し、GpuPipeline へ渡すのは以下のみ
  - CPUスナップショット配列（SoA/AoS）または更新リスト（dirty）
  - RenderHandle と更新データ（Transform 等）
- GPU 反映（Upload/Copy/Fence）は **WorldResource の責務**

> 仕様上、ECS が UploadBuffer を Map して memcpy するような設計は禁止。

---

## 7. バッファリング仕様（確定）

### 7.1 FramesInFlight
- `FramesInFlight = 1/2/3` を起動時に指定可能
- `1` は **デバッグ用途**。CPU/GPU の待ちが増える前提で扱う。

### 7.2 TransformBufferMode（描画元）
起動時に選択可能。

- **UploadOnly（選択肢として残す）**
  - UploadHeap 上のバッファをそのまま描画/Computeで参照する
  - 用途: デバッグ、互換、初期実装、低負荷シーン
  - 仕様: 起動時に警告ログ（性能上の注意）を出す

- **DefaultWithStaging（推奨デフォルト）**
  - CPU → Upload(staging) に書き込み
  - Upload → Default にコピー
  - 描画/Compute は Default を参照する

### 7.3 Upload→Defaultコピーのキュー方針（確定）
- **デフォルト**: GraphicsQueue でコピー（同期が単純）
- **オプション**: CopyQueue を使用可能
  - 使用する場合、FrameGraph が CopyPass と Fence依存を生成できることが前提
  - この条件が満たせないなら CopyQueue 使用は禁止

---

## 8. Pass 仕様（確定）

### 8.1 Pass種別
- RenderPass（GraphicsQueue）
- ComputePass（ComputeQueue）
- CopyPass（CopyQueue または GraphicsQueue の Copy実行）

### 8.2 Passが提供する関数（概念）
- `setup(builder)`
  - read/write 宣言
  - Transient の作成要求（format/size/clear 等）
  - Imported/Persistent の参照宣言（import済みを読み書き）
- `pipeline_requests(out)`
  - PSO/RootSignature/ShaderPermutation の要求（Desc/Key）
- `execute(context)`
  - 実コマンド記録（SetPSO/SetRootSig/Draw/Dispatch/Copy）

### 8.3 Passが持ってはいけないもの（確定）
- EntityId→GPU index 対応表
- free-list、世代管理、永続バッファ所有権
- 毎フレームPSO/RootSig生成（キャッシュ無し）

---

## 9. Descriptor戦略（確定）
- Persistent の SRV/UAV/CBV は **WorldResource / ViewResource が確保して保持**
- Transient の RTV/DSV は **FrameGraph が一時確保**
- Transient の SRV/UAV も **FrameGraph 実行時の一時アロケータ**で確保（フレーム寿命）
- Pass は「descriptor を永続保持」しない（必要なら index/handle を context から受け取る）

---

## 10. PSO / RootSignature / Shader のキャッシュ場所（確定）
**RenderDevice（GraphicsCoreのデバイス所有クラス）配下に単一所有者として置く。**

- ShaderLibrary: `ShaderDesc -> ShaderBytecode`
- RootSignatureCache: `RootSigDesc -> ID3D12RootSignature`
- PipelineStateCache: `PsoDesc -> ID3D12PipelineState`

公開APIは `get_or_create_*` のみ。キャッシュマップを外に晒さない。

---

## 11. キュー戦略と同期（確定）
- 基本: GraphicsQueue
- Async Compute:
  - **明示許可された Pass のみ** ComputeQueue へ
  - FrameGraph が依存/Fenceを生成する
- CopyQueue:
  - 上述の条件を満たす場合のみ
  - CopyPass を表現し、Fence依存を生成する

---

## 12. デバッグ計測（確定）
### 12.1 有効化条件
PIXイベント、GPUタイムライン、GPUタイムスタンプ等の **計測/可視化機能は `#ifndef NDEBUG` で囲う**。  
リリースビルド（NDEBUG）では無効化されることを仕様とする。

### 12.2 PIXイベント（GPUイベントマーカー）
目的: PIXでGPUキャプチャを取ったとき、タイムライン上でPass単位の区間を識別できるようにする。

- Pass execute の先頭/末尾で BeginEvent/EndEvent を挿入（基本は Pass粒度）
- PIX を起動していない通常実行でも基本的に安全（ただしデバッグビルドのみ有効）

#### 実装例（概念）
```cpp
#ifndef NDEBUG
class GpuEventScope
{
public:
    GpuEventScope(ID3D12GraphicsCommandList* commandList, const wchar_t* name)
        : m_commandList(commandList)
    {
        // 1) Begin event for PIX GPU capture timeline
        if (m_commandList != nullptr)
        {
            m_commandList->BeginEvent(0, name, static_cast<UINT>(wcslen(name) * sizeof(wchar_t)));
        }
    }

    ~GpuEventScope()
    {
        // 2) End event
        if (m_commandList != nullptr)
        {
            m_commandList->EndEvent();
        }
    }

private:
    ID3D12GraphicsCommandList* m_commandList = nullptr;
};
#endif
```

### 12.3 GPU Timeline（タイムスタンプ計測）
目的: PassごとのGPU実行時間を計測し、ログ/プロファイラUIに出す。

- Timestamp QueryHeap + ReadbackBuffer を使用
- FrameGraph 実行時に Pass前後で timestamp を打つ
- 結果の readback は FramesInFlight 分遅延して取得（CPU待ち回避）
- これも `#ifndef NDEBUG` で有効化

---

## 13. 受け入れ条件（最低テスト）
- FrameGraph の依存解決が正しい（read/write違反はエラーで停止）
- D3D12 debug layer / GPU-based validation で状態遷移エラーが出ない
- Transient がフレームごとに生成/破棄/別名化できる
- Persistent が Scene遷移・解像度変更・マルチViewで破綻しない
- PSO/RootSig が毎フレーム生成されていない（キャッシュヒットで確認）
- UploadOnly と DefaultWithStaging が両方動く（ただし性能は DefaultWithStaging が主経路）

---

## 14. 未決項目（今後）
- グラフ構造が同一のときに FrameGraph compile 結果をキャッシュする（最適化）
- Shader hot reload の無効化戦略（Shader変更→PSO再生成の連鎖）
- OIT等の透過表現（将来導入）
- GPU Driven（ExecuteIndirect/カリング）を Pass と Persistent にどう落とすか

---
