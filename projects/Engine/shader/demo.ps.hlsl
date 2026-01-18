#include "demo.hlsli"

struct PSOut
{
    float4 color : SV_Target;
};

PSOut PSMain(VSOut input)
{
    PSOut output;

    output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return output;
}
