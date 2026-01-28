#include "demo.hlsli"

struct ViewProjection
{
    float4x4 view;
    float4x4 projection;
};

struct Object
{
    uint id;
    uint visible;
    uint modelId;
    uint transformId;
};

struct Transform
{
    float4x4 worldMatrix;
};

ConstantBuffer<ViewProjection> cbViewProjection : register(b0);
cbuffer ObjectIndexCB : register(b1)
{
    uint objectIndex;
};
StructuredBuffer<Object> sbObjects : register(t0);
StructuredBuffer<Transform> sbTransforms : register(t1);

struct VSIn
{
    float4 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    uint index = objectIndex;
    Object obj = sbObjects[index];
    Transform transform = sbTransforms[obj.transformId];

    float4 localPos = input.position;
    float4 worldPos = mul(localPos, transform.worldMatrix);
    float4 viewPos = mul(worldPos, cbViewProjection.view);
    float4 projPos = mul(viewPos, cbViewProjection.projection);

    output.position = projPos;
    output.normal = mul(float4(input.normal, 0.0f), transform.worldMatrix).xyz;
    output.uv = input.uv;
    
    return output;
}
