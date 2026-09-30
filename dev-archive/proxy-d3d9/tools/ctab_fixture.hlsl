// Test fixture for ctab_test.c: named constants at known registers, in the
// Far Cry 2 / Blood Dragon naming. Compiled with fxc /T vs_3_0 (and vs_2_0).
float4x4 ViewMatrix : register(c12);
float4x4 ProjectionMatrix : register(c16);
float4x4 InvViewMatrix : register(c36);
float4 CameraPositionFractions : register(c40);
float4x4 ModelViewProjWithAVeryLongNameThatGoesPastTheFortyEightCharacterLimit : register(c60);

float4 main(float4 pos : POSITION) : POSITION
{
    float4 v = mul(pos, ViewMatrix);
    v = mul(v, ProjectionMatrix);
    v += mul(pos, InvViewMatrix) * CameraPositionFractions;
    return v + mul(pos, ModelViewProjWithAVeryLongNameThatGoesPastTheFortyEightCharacterLimit);
}
