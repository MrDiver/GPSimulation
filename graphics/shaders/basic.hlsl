[numthreads(16, 16, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID,
          uint3 GroupThreadID : SV_GroupThreadID)
{
    RWTexture2D<float4> image : register(u0, space0);

    uint2 texelCoord = DispatchThreadID.xy;
    uint2 size;
    image.GetDimensions(size.x, size.y);

    if (texelCoord.x < size.x && texelCoord.y < size.y)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (GroupThreadID.x != 0 && GroupThreadID.y != 0)
        {
            color.x = float(texelCoord.x) / size.x;
            color.y = float(texelCoord.y) / size.y;
        }

        image[texelCoord] = color;
    }
}
