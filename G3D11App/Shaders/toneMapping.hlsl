//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_INPUT
{
	float4 position		: POSITION;
	float2 texcoord		: TEXCOORD0;
};

struct VS_OUTPUT
{
	float2 texcoord	: TEXCOORD;
	float4 position : SV_POSITION;
};


VS_OUTPUT VS_Main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.texcoord = Input.texcoord;
	Output.position = Input.position;

	return Output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

SamplerState SS_Linear : register(s0);
Texture2D TX_BackBufferHDR : register(t0);

struct PS_INPUT
{
	float2 texcoord	: TEXCOORD;
};

float3 ReinhardTonemap(float3 x) {
	x = x * 1.4f; //exposure correction
	return x / (1.0f + x); // Reinhard
}

float3 Uncharted2Tonemap(float3 x) {
	x = x * 5.0f; //exposure correction
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	// Uncharted2 (may need parameter adjustment)
	return ((x*(A*x + C*B) + D*E) / (x*(A*x + B) + D*F)) - E / F; 
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
	float brightness = 0.0f; // [-1, 1], default = 0, additive
	float contrast = 1.0f; // [0, 100], default = 1, multiplicative

	// sample linear
	float4 color = TX_BackBufferHDR.Sample(SS_Linear, input.texcoord);

	// This could be calculated by luminance of the picture to get eye adjustment effect.
	float exposure = 1.0f; 

	// apply tonemapping (just skip for linear mapping)
	//color = float4(ReinhardTonemap(color.rgb * exposure), 1.0f);
	color = float4(Uncharted2Tonemap(color.rgb * exposure), 1.0f);

	// apply contrast
	color = color * contrast;

	// apply brightness
	color = color + brightness;

	// TODO gamma delta adjust ?
 
	return color;
}