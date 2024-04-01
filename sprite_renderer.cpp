#include "sprite_renderer.h"
#include <algorithm>

SpriteRenderer::SpriteRenderer() {
    const size_t bufferSize = sizeof(DrawCommand) * MAX_DRAW_COMMANDS;
    drawCommands = (DrawCommand*)malloc(bufferSize);
    drawCommandNum = 0;
    gpuUploadBuffer = gfx::createBuffer(L"SpriteRenderer::uploadBuffer", bufferSize, gfx::UPLOAD_BUFFER);
    for (uint32_t index = 0; index < FRAME_COUNT; ++index) {
        gpuDrawCommands[index] = gfx::createBuffer(L"SpriteRenderer::drawCommands", bufferSize, gfx::UNORDERED_BUFFER);
    }
    images = (gfx::Image2D**)malloc(MAX_DESCRIPTORS * sizeof(gfx::Image2D*));
    imageNum = 0;
    gpuCounterZero = gfx::createBuffer(L"SpriteRenderer::counterZero", sizeof(uint32_t), gfx::UPLOAD_BUFFER, true);
    buildSpriteGen();
    buildSpriteRender();
}

SpriteRenderer::~SpriteRenderer() {
    free(images);
    free(drawCommands);
    D3D_RELEASE(gpuDrawCommandSignature);
    D3D_RELEASE(gpuCounterZero.resource);
    D3D_RELEASE(gpuUploadBuffer.resource);
    for (uint32_t index = 0; index < FRAME_COUNT; ++index) {
        D3D_RELEASE(gpuDrawCommands[index].resource);
    }
    D3D_RELEASE(gpuClearIndirectCommandBuffer.resource);
    D3D_RELEASE(gpuIndirectCommandBuffer.resource);
    D3D_RELEASE(gpuSpriteVertices.resource);
    D3D_RELEASE(gpuSpriteRenderRootSignature);
    D3D_RELEASE(gpuSpriteRenderPSO);
    D3D_RELEASE(gpuSpriteGenRootSignature);
    D3D_RELEASE(gpuSpriteGenPSO);
    D3D_RELEASE(gpuVisibleList.resource);
}

void SpriteRenderer::buildSpriteRender() {
    gfx::RootSignatureDescriptorRange rootSigRanges;
    rootSigRanges.addRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_DESCRIPTORS, 0, 0);
    gfx::RootSignatureBuilder rootSigBuilder;
    rootSigBuilder.addRootParameterDescriptorTable(rootSigRanges, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSigBuilder.addStaticSampler(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    gpuSpriteRenderRootSignature = rootSigBuilder.build(false);
    gpuSpriteRenderRootSignature->SetName(L"SpriteRenderer::spriteRenderRootSig");

    FileReader vertexShaderFile(OUTPUT_PATH "SpriteRender_VS.cso");
    FileReader pixelShaderFile(OUTPUT_PATH "SpriteRender_PS.cso");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = gpuSpriteRenderRootSignature;
    psoDesc.VS = { *vertexShaderFile, vertexShaderFile.getSize() };
    psoDesc.PS = { *pixelShaderFile, pixelShaderFile.getSize() };
    psoDesc.BlendState.AlphaToCoverageEnable = false;
    psoDesc.BlendState.IndependentBlendEnable = false;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
    psoDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = !false;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = false;
    psoDesc.RasterizerState.MultisampleEnable = false;
    psoDesc.RasterizerState.AntialiasedLineEnable = false;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    psoDesc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    psoDesc.DepthStencilState.BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {};
    inputElementDesc[0].SemanticName = "POSITION";
    inputElementDesc[0].SemanticIndex = 0;
    inputElementDesc[0].Format = DXGI_FORMAT_R32G32_FLOAT; 
    inputElementDesc[0].InputSlot = 0;
    inputElementDesc[0].AlignedByteOffset = offsetof(SpriteVertex, position);
    inputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDesc[0].InstanceDataStepRate = 0;

    inputElementDesc[1].SemanticName = "TEXCOORD";
    inputElementDesc[1].SemanticIndex = 0;
    inputElementDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDesc[1].InputSlot = 0;
    inputElementDesc[1].AlignedByteOffset = offsetof(SpriteVertex, texCoord);
    inputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDesc[1].InstanceDataStepRate = 0;

    inputElementDesc[2].SemanticName = "COLOR";
    inputElementDesc[2].SemanticIndex = 0;
    inputElementDesc[2].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    inputElementDesc[2].InputSlot = 0;
    inputElementDesc[2].AlignedByteOffset = offsetof(SpriteVertex, color);
    inputElementDesc[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDesc[2].InstanceDataStepRate = 0;

    inputElementDesc[3].SemanticName = "TEXCOORD";
    inputElementDesc[3].SemanticIndex = 1;
    inputElementDesc[3].Format = DXGI_FORMAT_R32_UINT;
    inputElementDesc[3].InputSlot = 0;
    inputElementDesc[3].AlignedByteOffset = offsetof(SpriteVertex, textureId);
    inputElementDesc[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDesc[3].InstanceDataStepRate = 0;

    psoDesc.InputLayout.pInputElementDescs = inputElementDesc;
    psoDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);

    gpuSpriteRenderPSO = gfx::createGraphicsPipelineState(L"SpriteRenderer::spriteRenderPSO", psoDesc);
}

void SpriteRenderer::buildSpriteGen() {
    gfx::RootSignatureDescriptorRange rootSigRanges;
    gfx::RootSignatureBuilder rootSigBuilder;
    rootSigBuilder.addRootParameterConstant(0, 0, 4, D3D12_SHADER_VISIBILITY_ALL);
    rootSigBuilder.addRootParameterDescriptorTable(
        rootSigRanges
        .addRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0),
        D3D12_SHADER_VISIBILITY_ALL);

    gpuSpriteGenRootSignature = rootSigBuilder.build(true);
    gpuSpriteGenRootSignature->SetName(L"SpriteRenderer::spriteGenRootSig");

    FileReader shaderFile(OUTPUT_PATH "SpriteGen_CS.cso");
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = gpuSpriteGenRootSignature;
    psoDesc.CS = { *shaderFile, shaderFile.getSize() };
    psoDesc.NodeMask = 0;
    psoDesc.CachedPSO = {};
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    gpuSpriteGenPSO = gfx::createComputePipelineState(L"SpriteRenderer::spriteGen_CS", psoDesc);
    gpuSpriteVertices = gfx::createBuffer(L"SpriteRenderer::spriteVertices", MAX_DRAW_COMMANDS * (sizeof(SpriteVertex) * SPRITE_VERTEX_COUNT), gfx::UNORDERED_BUFFER);

    D3D12_INDIRECT_ARGUMENT_DESC argumentsDesc[1] = {};
    argumentsDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
    commandSignatureDesc.pArgumentDescs = argumentsDesc;
    commandSignatureDesc.NumArgumentDescs = sizeof(argumentsDesc) / sizeof(argumentsDesc[0]);
    commandSignatureDesc.ByteStride = sizeof(IndirectCommand);
    D3D_ASSERT(gfx::getDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&gpuDrawCommandSignature)), "Failed to create command signature");
    gpuDrawCommandSignature->SetName(L"SpriteRenderer::drawCommandSignature");
    gpuIndirectCommandBuffer = gfx::createBuffer(L"SpriteRenderer::indirectCommandBuffer", sizeof(IndirectCommand), gfx::UNORDERED_BUFFER, true);
    gpuClearIndirectCommandBuffer = gfx::createBuffer(L"SpriteRenderer::clearIndirectCommandBuffer", sizeof(IndirectCommand), gfx::UPLOAD_BUFFER, true);
    void* data = nullptr;
    D3D_ASSERT(gpuClearIndirectCommandBuffer.resource->Map(0, nullptr, &data), "Failed to map clear indirect draw command buffer");
    IndirectCommand emptyCommand = {};
    emptyCommand.draw.InstanceCount = 1;
    emptyCommand.draw.StartVertexLocation = 6;
    memcpy(data, &emptyCommand, sizeof(IndirectCommand));
    gpuClearIndirectCommandBuffer.resource->Unmap(0, nullptr);
    gpuSpriteVerticesCounter = gfx::createBuffer(L"SpriteRenderer::spriteVertexCounter", sizeof(uint32_t), gfx::UNORDERED_BUFFER, true);
    gpuVisibleList = gfx::createBuffer(L"SpriteRenderer::spriteCounter", sizeof(uint32_t) * MAX_DRAW_COMMANDS, gfx::UNORDERED_BUFFER, true);
    gpuPerLaneOffset = gfx::createBuffer(L"SpriteRenderer::spriteCounter", sizeof(uint32_t) * MAX_DRAW_COMMANDS, gfx::UNORDERED_BUFFER, true);
}

void SpriteRenderer::reset() {
    imageNum = 0;
    drawCommandNum = 0;
}

void SpriteRenderer::drawImage(float x, float y, float width, float height, uint32_t color, gfx::Image2D* image) {
    ASSERT(drawCommandNum + 1 <= MAX_DRAW_COMMANDS, "Reached limit of draw commands");
    ASSERT(image != nullptr, "Image can't be null");
    DrawCommand& cmd = drawCommands[drawCommandNum++];
    memcpy(cmd.transform, &matrixStack.current, sizeof(float) * 4);
    if ((image->state & GFX_IMAGE_STATE_BOUND) == 0) {
        image->textureId = TEXTURE_ID_OFFSET + imageNum;
        images[imageNum++] = image;
        image->state |= GFX_IMAGE_STATE_BOUND;
    }
    cmd.image[0] = x;
    cmd.image[1] = y;
    cmd.image[2] = width;
    cmd.image[3] = height;
    cmd.color = color;
    cmd.textureId = image->textureId;
}

void SpriteRenderer::flushCommands(gfx::RenderFrame& frame) {

    if (drawCommandNum == 0) return;

    ID3D12GraphicsCommandList* commandList = frame.commandList;
    uint32_t frameIndex = frame.frameIndex;
    gfx::ResourceBarrierBatcher<10> barriers;

    void* gpuUploadBufferData = nullptr;
    D3D_ASSERT(gpuUploadBuffer.resource->Map(0, nullptr, &gpuUploadBufferData), "Failed to map draw command upload buffer");
    memcpy(gpuUploadBufferData, drawCommands, drawCommandNum * sizeof(DrawCommand));
    D3D12_RANGE writtenRange = { 0, drawCommandNum * sizeof(DrawCommand) };
    gpuUploadBuffer.resource->Unmap(0, &writtenRange);

    barriers.transition(&gpuDrawCommands[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    barriers.transition(&gpuIndirectCommandBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
    barriers.transition(&gpuSpriteVerticesCounter, D3D12_RESOURCE_STATE_COPY_DEST);
    //barriers.transition(&gpuPerLaneOffset, D3D12_RESOURCE_STATE_COPY_DEST);
    barriers.flush(commandList);
    commandList->CopyBufferRegion(gpuDrawCommands[frameIndex].resource, 0, gpuUploadBuffer.resource, 0, drawCommandNum * sizeof(DrawCommand));
    commandList->CopyResource(gpuSpriteVerticesCounter.resource, gpuCounterZero.resource);
    //commandList->CopyResource(gpuPerLaneOffset.resource, gpuCounterZero.resource);
    commandList->CopyResource(gpuIndirectCommandBuffer.resource, gpuClearIndirectCommandBuffer.resource);
    barriers.transition(&gpuDrawCommands[frameIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers.transition(&gpuIndirectCommandBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers.transition(&gpuSpriteVerticesCounter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    //barriers.transition(&gpuPerLaneOffset, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    barriers.flush(commandList);

    commandList->SetPipelineState(gpuSpriteGenPSO);
    commandList->SetComputeRootSignature(gpuSpriteGenRootSignature);


    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.NumElements = drawCommandNum;
    uavDesc.Buffer.StructureByteStride = sizeof(DrawCommand);
    gfx::getDevice()->CreateUnorderedAccessView(gpuDrawCommands[frameIndex].resource, nullptr, &uavDesc, frame.descriptorTable.allocate().cpuHandle);

    uavDesc.Buffer.NumElements = MAX_DRAW_COMMANDS;
    uavDesc.Buffer.StructureByteStride = sizeof(SpriteQuad);
    gfx::getDevice()->CreateUnorderedAccessView(gpuSpriteVertices.resource, nullptr, &uavDesc, frame.descriptorTable.allocate().cpuHandle);

    uavDesc.Buffer.NumElements = 1;
    uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
    gfx::getDevice()->CreateUnorderedAccessView(gpuIndirectCommandBuffer.resource, nullptr, &uavDesc, frame.descriptorTable.allocate().cpuHandle);

    uavDesc.Buffer.NumElements = MAX_DRAW_COMMANDS;
    uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    gfx::getDevice()->CreateUnorderedAccessView(gpuVisibleList.resource, nullptr, &uavDesc, frame.descriptorTable.allocate().cpuHandle);

    uavDesc.Buffer.NumElements = MAX_DRAW_COMMANDS;
    uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    gfx::getDevice()->CreateUnorderedAccessView(gpuPerLaneOffset.resource, nullptr, &uavDesc, frame.descriptorTable.allocate().cpuHandle);

    struct { float resolution[2]; uint32_t drawCommandNum; uint32_t operationId; } constantData = { { gfx::getViewWidth(), gfx::getViewHeight() }, drawCommandNum, OP_CULL_SPRITES };
    commandList->SetComputeRoot32BitConstants(0, sizeof(constantData) / sizeof(uint32_t), &constantData, 0);

    commandList->SetComputeRootDescriptorTable(1, frame.descriptorTable.gpuBaseHandle);
    uint32_t disapatchSize = (drawCommandNum / THREAD_GROUP_SIZE) + ((drawCommandNum % THREAD_GROUP_SIZE > 0) ? 1 : 0);
    commandList->Dispatch(disapatchSize, 1, 1);

    constantData = { { 1.0f / gfx::getViewWidth(), 1.0f / gfx::getViewHeight() }, drawCommandNum, OP_UPDATE_COMMANDS };
    commandList->SetComputeRoot32BitConstants(0, sizeof(constantData) / sizeof(uint32_t), &constantData, 0);
    commandList->Dispatch(disapatchSize, 1, 1);

    //constantData = { { 1.0f / gfx::getViewWidth(), 1.0f / gfx::getViewHeight() }, drawCommandNum, OP_GENERATE_SPRITES };
    //commandList->SetComputeRoot32BitConstants(0, sizeof(constantData) / sizeof(uint32_t), &constantData, 0);
    //commandList->Dispatch(disapatchSize, 1, 1);


    // Render
    gfx::Resource tempRT = { gfx::getCurrentBackbuffer(), D3D12_RESOURCE_STATE_PRESENT };
    barriers.transition(&tempRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers.transition(&gpuSpriteVertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    barriers.transition(&gpuIndirectCommandBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    barriers.flush(commandList);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = gfx::getRenderTargetViewCPUHandle();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    gfx::getDevice()->CreateRenderTargetView(gfx::getCurrentBackbuffer(), &rtvDesc, rtvHandle);
    
    commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);
    float clearColor[4] = { 0, 0, 0, 1 };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->SetPipelineState(gpuSpriteRenderPSO);
    commandList->SetGraphicsRootSignature(gpuSpriteRenderRootSignature);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, frame.descriptorTable.gpuBaseHandle);

    // Set images in descriptor table (reuse previous pass descriptor table)
    for (uint32_t index = 0; index < imageNum; ++index) {
        gfx::Image2D* image = images[index];
        image->state &= ~GFX_IMAGE_STATE_BOUND;
        image->textureId = ~0u;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        gfx::getDevice()->CreateShaderResourceView(image->texture.resource, &srvDesc, frame.descriptorTable.allocate().cpuHandle);
    }

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = gfx::getViewWidth();
    viewport.Height = gfx::getViewHeight();
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = (uint32_t)gfx::getViewWidth();
    scissor.bottom = (uint32_t)gfx::getViewHeight();
    commandList->RSSetScissorRects(1, &scissor);

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = gpuSpriteVertices.resource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = MAX_DRAW_COMMANDS * (sizeof(SpriteVertex) * SPRITE_VERTEX_COUNT);
    vertexBufferView.StrideInBytes = sizeof(SpriteVertex);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    commandList->ExecuteIndirect(gpuDrawCommandSignature, 1, gpuIndirectCommandBuffer.resource, 0, nullptr, 0);
    //commandList->DrawInstanced(drawCommandNum * 6, 1, 0, 0);

    barriers.transition(&gpuSpriteVertices, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers.transition(&tempRT, D3D12_RESOURCE_STATE_PRESENT);
    barriers.flush(commandList);
}
