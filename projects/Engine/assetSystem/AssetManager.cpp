#include "pch.h"
#include "AssetManager.h"
#include "GraphicsCore/public/ResourceManager.h"
#include "GraphicsCore/public/GpuCommand.h"
#include <cstring>

namespace Drama::Asset
{
    void AssetManager::create_default_assets()
    {
        create_cube_model();
    }

    void AssetManager::create_cube_model()
    {
        // cube
        ModelData modelData;
        MeshData meshData;
        std::string name = "Cube";
        uint32_t verticesCount = 24;
        uint32_t indicesCount = 36;
        meshData.name = name;
        meshData.vertices.resize(verticesCount);
        meshData.indices.resize(indicesCount);
        // 頂点データ設定
#pragma region
        // 右面
        meshData.vertices[0] = { {0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} }; // 右上
        meshData.vertices[1] = { {0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} }; // 左上
        meshData.vertices[2] = { {0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} }; // 右下
        meshData.vertices[3] = { {0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} }; // 左下

        // 左面
        meshData.vertices[4] = { {-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} }; // 左上
        meshData.vertices[5] = { {-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} }; // 右上
        meshData.vertices[6] = { {-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} }; // 左下
        meshData.vertices[7] = { {-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} }; // 右下

        // 前面
        meshData.vertices[8] = { {-0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} }; // 左上
        meshData.vertices[9] = { { 0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} }; // 右上
        meshData.vertices[10] = { {-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} }; // 左下
        meshData.vertices[11] = { { 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} }; // 右下

        // 後面
        meshData.vertices[12] = { { 0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }; // 右上
        meshData.vertices[13] = { {-0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }; // 左上
        meshData.vertices[14] = { { 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }; // 右下
        meshData.vertices[15] = { {-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }; // 左下

        // 上面
        meshData.vertices[16] = { {-0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} }; // 左奥
        meshData.vertices[17] = { { 0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f} }; // 右奥
        meshData.vertices[18] = { {-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} }; // 左前
        meshData.vertices[19] = { { 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f} }; // 右前

        // 下面
        meshData.vertices[20] = { {-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} }; // 左前
        meshData.vertices[21] = { { 0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f} }; // 右前
        meshData.vertices[22] = { {-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f} }; // 左奥
        meshData.vertices[23] = { { 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f} }; // 右奥
#pragma endregion
        // インデックスデータ設定
#pragma region
        // 右面インデックス
        meshData.indices[0] = 0; meshData.indices[1] = 2; meshData.indices[2] = 1;
        meshData.indices[3] = 2; meshData.indices[4] = 3; meshData.indices[5] = 1;

        // 左面インデックス
        meshData.indices[6] = 4; meshData.indices[7] = 6; meshData.indices[8] = 5;
        meshData.indices[9] = 6; meshData.indices[10] = 7; meshData.indices[11] = 5;

        // 前面インデックス
        meshData.indices[12] = 8; meshData.indices[13] = 10; meshData.indices[14] = 9;
        meshData.indices[15] = 10; meshData.indices[16] = 11; meshData.indices[17] = 9;

        // 後面インデックス
        meshData.indices[18] = 12; meshData.indices[19] = 14; meshData.indices[20] = 13;
        meshData.indices[21] = 14; meshData.indices[22] = 15; meshData.indices[23] = 13;

        // 上面インデックス
        meshData.indices[24] = 16; meshData.indices[25] = 18; meshData.indices[26] = 17;
        meshData.indices[27] = 18; meshData.indices[28] = 19; meshData.indices[29] = 17;

        // 下面インデックス
        meshData.indices[30] = 20; meshData.indices[31] = 22; meshData.indices[32] = 21;
        meshData.indices[33] = 22; meshData.indices[34] = 23; meshData.indices[35] = 21;
#pragma endregion
        // GpuBuffer 作成
        // 1) UploadBuffer を作成して CPU データを書き込む
        // 2) Vertex/IndexBuffer を作成する
        // 3) CopyBufferRegion で Upload -> Default を反映して UploadBuffer を解放する
        uint32_t vertexUploadIndex = m_resourceManager.create_upload_buffer<VertexData>(
            verticesCount, L"CubeVertexUpload");
        uint32_t indexUploadIndex = m_resourceManager.create_upload_buffer<std::uint32_t>(
            indicesCount, L"CubeIndexUpload");
        auto* vertexUpload = m_resourceManager.get_gpu_buffer<Drama::Graphics::DX12::UploadBuffer<VertexData>>(vertexUploadIndex);
        auto* indexUpload = m_resourceManager.get_gpu_buffer<Drama::Graphics::DX12::UploadBuffer<std::uint32_t>>(indexUploadIndex);
        if (vertexUpload)
        {
            std::memcpy(
                vertexUpload->get_mapped_data().data(),
                meshData.vertices.data(),
                sizeof(VertexData) * meshData.vertices.size());
        }
        if (indexUpload)
        {
            std::memcpy(
                indexUpload->get_mapped_data().data(),
                meshData.indices.data(),
                sizeof(std::uint32_t) * meshData.indices.size());
        }
        meshData.vertexBufferIndex = m_resourceManager.create_vertex_buffer<VertexData>(
            verticesCount, L"CubeVertexBuffer");
        meshData.indexBufferIndex = m_resourceManager.create_index_buffer<std::uint32_t>(
            indicesCount, L"CubeIndexBuffer");
        auto* vertexBuffer = m_resourceManager.get_gpu_buffer<Drama::Graphics::DX12::VertexBuffer<VertexData>>(meshData.vertexBufferIndex);
        auto* indexBuffer = m_resourceManager.get_gpu_buffer<Drama::Graphics::DX12::IndexBuffer<std::uint32_t>>(meshData.indexBufferIndex);
        Drama::Graphics::DX12::RenderDevice& renderDevice = m_resourceManager.get_render_device();
        Drama::Graphics::DX12::QueuePool* queuePool = renderDevice.get_queue_pool();
        if (vertexUpload && indexUpload && vertexBuffer && indexBuffer && queuePool)
        {
            Drama::Graphics::DX12::CopyCommandContext copyContext(renderDevice);
            copyContext.reset();
            ID3D12GraphicsCommandList* commandList = copyContext.get_command_list();
            if (commandList)
            {
                commandList->CopyBufferRegion(
                    vertexBuffer->get_resource(), 0,
                    vertexUpload->get_resource(), 0,
                    vertexUpload->get_buffer_size());
                commandList->CopyBufferRegion(
                    indexBuffer->get_resource(), 0,
                    indexUpload->get_resource(), 0,
                    indexUpload->get_buffer_size());
            }
            copyContext.close();
            Drama::Graphics::DX12::CopyQueueContext* copyQueue = queuePool->get_copy_queue();
            copyQueue->execute(&copyContext);
            copyQueue->flush();
            queuePool->return_queue(copyQueue);
        }
        m_resourceManager.destroy_gpu_buffer(vertexUploadIndex);
        m_resourceManager.destroy_gpu_buffer(indexUploadIndex);
        modelData.meshes.push_back(std::move(meshData));
        modelDataContainer.add(name, std::move(modelData));
    }
} // namespace Drama::Asset
