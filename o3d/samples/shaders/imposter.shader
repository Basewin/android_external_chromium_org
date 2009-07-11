/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This Imposter shader rotates around Y to face the camera.
// It doesn't have any lighting.

float4x4 world : World;
float4x4 viewInverse : ViewInverse;
float4x4 viewProjection : ViewProjection;

sampler texSampler0;

// input parameters for our vertex shader
struct VertexShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD;
};

// input parameters for our pixel shader
// also the output parameters for our vertex shader
struct PixelShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD;
};

/**
 * Vertex Shader - vertex shader for phong illumination
 */
PixelShaderInput vertexShaderFunction(VertexShaderInput input) {
  PixelShaderInput output;

  float3 toCamera = normalize(viewInverse[3].xyz - world[3].xyz);
  float3 yAxis = float3(0, 1, 0);
  float3 xAxis = cross(yAxis, toCamera);
  float3 zAxis = cross(xAxis, yAxis);
  
  float4x4 newWorld = float4x4(
      float4(xAxis, 0),
      float4(yAxis, 0),
      float4(xAxis, 0),
      world[3]);
  output.position = mul(input.position, mul(newWorld, viewProjection));

  output.texcoord = input.texcoord;
  return output;
}

/**
 * Pixel Shader
 */
float4 pixelShaderFunction(PixelShaderInput input): COLOR {
  return tex2D(texSampler0, input.texcoord);
}

// Here we tell our effect file *which* functions are
// our vertex and pixel shaders.

// #o3d VertexShaderEntryPoint vertexShaderFunction
// #o3d PixelShaderEntryPoint pixelShaderFunction
// #o3d MatrixLoadOrder RowMajor

