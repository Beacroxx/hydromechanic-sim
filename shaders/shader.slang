struct VSInput {
  float3 inPosition;
  float3 inNormal;
  float3 inColor;
  float2 inTexCoord;
};

struct UniformBuffer {
  float4x4 model;
  float4x4 view;
  float4x4 proj;

  float3 camera;
  float shininess;
  float3 light;
  float intensity;
  float3 color;
  float ambient;
  float3 spec;

  float __pad;

  float4x4 normal;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
  float4 pos : SV_Position;
  float3 worldPos;
  float3 normal;
  float3 color;
  float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  VSOutput output;

  output.worldPos = mul(ubo.model, float4(input.inPosition, 1.0)).xyz;
  output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
  output.normal = mul(ubo.normal, float4(input.inNormal, 0.0)).xyz;
  output.color = input.inColor;
  output.texCoord = input.inTexCoord;
  return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
  float3 albedo = texture.Sample(vertIn.texCoord).rgb * vertIn.color;

  float3 normal = normalize(vertIn.normal);
  float3 viewDir = normalize(ubo.camera - vertIn.worldPos);
  float3 lightDir = normalize(ubo.light - vertIn.worldPos);

  float3 ambient = ubo.ambient * ubo.color;

  // Lambertian diffuse
  float diffuseStrength = max(dot(normal, lightDir), 0.0);
  float3 diffuse = diffuseStrength * ubo.color;

  // Blinn-Phong specular
  float3 halfwayDir = normalize(lightDir + viewDir);
  float specularStrength = pow(max(dot(normal, halfwayDir), 0.0), ubo.shininess);
  float3 specular = ubo.spec * specularStrength * ubo.color;

  float3 litColor = albedo * (ambient + diffuse) + specular;

  return float4(litColor, 1.0);
}
