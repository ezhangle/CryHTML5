/* CryHTML5 - for licensing and copyright see license.txt */

#include "StdAfx.h"
#include "FullscreenTriangleDrawer.h"
#include "CPluginHTML5.h"

#include <d3d9.h>
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>

namespace HTML5Plugin
{
    // An incomplete DX11 state guard class that saves and restores the
    // states and resources that will be modified during the drawing in the hooked function
    class CDX11StateGuard
    {
        public:
            CDX11StateGuard()
                : m_VertexShaderClassInstancesCount( D3D11_SHADER_MAX_INTERFACES )
                , m_PixelShaderClassInstancesCount( D3D11_SHADER_MAX_INTERFACES )
            {
                ID3D11Device* pDevice = static_cast<ID3D11Device*>( gD3DSystem->GetDevice() );
                ID3D11DeviceContext* pContext = NULL;
                pDevice->GetImmediateContext( &pContext );

                pContext->OMGetBlendState( &m_pBlendState, m_BlendFactor, &m_SampleMask );
                pContext->RSGetState( &m_pRasterizerState );
                pContext->IAGetPrimitiveTopology( &m_PrimitiveTopology );
                pContext->IAGetIndexBuffer( &m_pIndexBuffer, &m_IndexBufferFormat, &m_IndexBufferOffset );
                pContext->IAGetInputLayout( &m_pInputLayout );
                pContext->IAGetVertexBuffers( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, m_pVertexBuffers, m_pVertexBufferStrides, m_pVertexBufferOffsets );
                pContext->VSGetShader( &m_pVertexShader, m_ppVertexShaderClassInstances, &m_VertexShaderClassInstancesCount );
                pContext->PSGetShader( &m_pPixelShader, m_ppPixelShaderClassInstances, &m_PixelShaderClassInstancesCount );
                pContext->PSGetSamplers( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_ppPixelShaderSamplers );
                pContext->PSGetShaderResources( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_ppPixelShaderResources );
            }

            ~CDX11StateGuard()
            {
                ID3D11Device* pDevice = static_cast<ID3D11Device*>( gD3DSystem->GetDevice() );
                ID3D11DeviceContext* pContext = NULL;
                pDevice->GetImmediateContext( &pContext );

                // Apply saved state
                pContext->OMSetBlendState( m_pBlendState, m_BlendFactor, m_SampleMask );
                pContext->RSSetState( m_pRasterizerState );
                pContext->IASetPrimitiveTopology( m_PrimitiveTopology );
                pContext->IASetIndexBuffer( m_pIndexBuffer, m_IndexBufferFormat, m_IndexBufferOffset );
                pContext->IASetInputLayout( m_pInputLayout );
                pContext->IASetVertexBuffers( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, m_pVertexBuffers, m_pVertexBufferStrides, m_pVertexBufferOffsets );
                pContext->VSSetShader( m_pVertexShader, m_ppVertexShaderClassInstances, m_VertexShaderClassInstancesCount );
                pContext->PSSetShader( m_pPixelShader, m_ppPixelShaderClassInstances, m_PixelShaderClassInstancesCount );
                pContext->PSSetSamplers( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_ppPixelShaderSamplers );
                pContext->PSSetShaderResources( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_ppPixelShaderResources );

                // Release references
                SAFE_RELEASE( m_pBlendState );
                SAFE_RELEASE( m_pRasterizerState );
                SAFE_RELEASE( m_pIndexBuffer );
                SAFE_RELEASE( m_pInputLayout );

                for ( UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i )
                {
                    SAFE_RELEASE( m_pVertexBuffers[i] );
                }

                SAFE_RELEASE( m_pVertexShader );

                for ( UINT i = 0; i < m_VertexShaderClassInstancesCount; ++i )
                {
                    SAFE_RELEASE( m_ppVertexShaderClassInstances[i] );
                }

                SAFE_RELEASE( m_pPixelShader );

                for ( UINT i = 0; i < m_PixelShaderClassInstancesCount; ++i )
                {
                    SAFE_RELEASE( m_ppPixelShaderClassInstances[i] );
                }

                for ( UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i )
                {
                    SAFE_RELEASE( m_ppPixelShaderSamplers[i] );
                }

                for ( UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i )
                {
                    SAFE_RELEASE( m_ppPixelShaderResources[i] );
                }
            }

        private:
            ID3D11BlendState* m_pBlendState;
            FLOAT m_BlendFactor[4];
            UINT m_SampleMask;

            ID3D11RasterizerState* m_pRasterizerState;

            ID3D11VertexShader* m_pVertexShader;
            ID3D11ClassInstance* m_ppVertexShaderClassInstances[D3D11_SHADER_MAX_INTERFACES];
            UINT m_VertexShaderClassInstancesCount;

            ID3D11PixelShader* m_pPixelShader;
            ID3D11ClassInstance* m_ppPixelShaderClassInstances[D3D11_SHADER_MAX_INTERFACES];
            UINT m_PixelShaderClassInstancesCount;
            ID3D11SamplerState* m_ppPixelShaderSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
            ID3D11ShaderResourceView* m_ppPixelShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

            ID3D11InputLayout* m_pInputLayout;

            ID3D11Buffer* m_pIndexBuffer;
            DXGI_FORMAT m_IndexBufferFormat;
            UINT m_IndexBufferOffset;

            ID3D11Buffer* m_pVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            UINT m_pVertexBufferStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            UINT m_pVertexBufferOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

            D3D11_PRIMITIVE_TOPOLOGY m_PrimitiveTopology;
    };


    CFullscreenTriangleDrawer::CFullscreenTriangleDrawer()
        : m_pVertexDeclaration( NULL )
        , m_pVertexBuffer( NULL )
        , m_pVertexShader9( NULL )
        , m_pPixelShader9( NULL )
        , m_pStateBlock( NULL )
        , m_pVertexShader11( NULL )
        , m_pPixelShader11( NULL )
        , m_pBlendState11( NULL )
    {
        switch ( gD3DSystem->GetType() )
        {
            case D3DPlugin::D3D_DX9:
                {
                    CreateDX9Resources();
                    break;
                }

            case D3DPlugin::D3D_DX11:
                {
                    CreateDX11Resources();
                    break;
                }
        }
    }

    void CFullscreenTriangleDrawer::CreateDX9Resources()
    {
        HRESULT hr = S_OK;

        float VertexData[] =
        {
            -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
            3.0f,  1.0f, 0.0f, 2.0f, 0.0f,
            -1.0f, -3.0f, 0.0f, 0.0f, 2.0f,
        };

        IDirect3DDevice9* pDevice = static_cast<IDirect3DDevice9*>( gD3DSystem->GetDevice() );

        // Vertex buffer
        hr = pDevice->CreateVertexBuffer( sizeof( VertexData ), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL );
        CRY_ASSERT( SUCCEEDED( hr ) );

        float* pVertexBuffer = NULL;
        hr = m_pVertexBuffer->Lock( 0, 0, ( void** )&pVertexBuffer, 0 );
        CRY_ASSERT( SUCCEEDED( hr ) );
        memcpy( pVertexBuffer, VertexData, sizeof( VertexData ) );
        m_pVertexBuffer->Unlock();

        // Vertex declaration
        D3DVERTEXELEMENT9 VertexElements[] =
        {
            {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()
        };
        hr = pDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDeclaration );
        CRY_ASSERT( SUCCEEDED( hr ) );

        // Vertex shader

        /*
        // FSTriangle.vs
        // fxc /O2 /T vs_2_0 /E VSMain
        float Width : register(c0);
        float Height : register(c1);

        struct VertexPT
        {
            float4 PositionL : POSITION0;
            float2 TexCoord  : TEXCOORD0;
        };

        VertexPT VSMain(VertexPT vsIn)
        {
            vsIn.PositionL += float4(-0.5 / Width, 0.5 / Height, 0, 0);
            return vsIn;
        }
        */

        const char CompiledVS[] =
        {
            0x00, 0x02, 0xfe, 0xff, 0xfe, 0xff, 0x28, 0x00, 0x43, 0x54, 0x41, 0x42,
            0x1c, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfe, 0xff,
            0x02, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0xc1, 0x00, 0x00,
            0x62, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x06, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x5c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
            0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x65, 0x69, 0x67,
            0x68, 0x74, 0x00, 0xab, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x69, 0x64, 0x74,
            0x68, 0x00, 0x76, 0x73, 0x5f, 0x32, 0x5f, 0x30, 0x00, 0x4d, 0x69, 0x63,
            0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74, 0x20, 0x28, 0x52, 0x29, 0x20, 0x48,
            0x4c, 0x53, 0x4c, 0x20, 0x53, 0x68, 0x61, 0x64, 0x65, 0x72, 0x20, 0x43,
            0x6f, 0x6d, 0x70, 0x69, 0x6c, 0x65, 0x72, 0x20, 0x39, 0x2e, 0x32, 0x39,
            0x2e, 0x39, 0x35, 0x32, 0x2e, 0x33, 0x31, 0x31, 0x31, 0x00, 0xab, 0xab,
            0x51, 0x00, 0x00, 0x05, 0x02, 0x00, 0x0f, 0xa0, 0x00, 0x00, 0x00, 0xbf,
            0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x1f, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x0f, 0x90,
            0x1f, 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x80, 0x01, 0x00, 0x0f, 0x90,
            0x06, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0xa0,
            0x05, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x80,
            0x02, 0x00, 0x00, 0xa0, 0x06, 0x00, 0x00, 0x02, 0x01, 0x00, 0x01, 0x80,
            0x01, 0x00, 0x00, 0xa0, 0x05, 0x00, 0x00, 0x03, 0x00, 0x00, 0x02, 0x80,
            0x01, 0x00, 0x00, 0x80, 0x02, 0x00, 0x55, 0xa0, 0x01, 0x00, 0x00, 0x02,
            0x00, 0x00, 0x0c, 0x80, 0x02, 0x00, 0xaa, 0xa0, 0x02, 0x00, 0x00, 0x03,
            0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0xe4, 0x80, 0x00, 0x00, 0xe4, 0x90,
            0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03, 0xe0, 0x01, 0x00, 0xe4, 0x90,
            0xff, 0xff, 0x00, 0x00,
        };

        hr = pDevice->CreateVertexShader( ( DWORD* )CompiledVS, &m_pVertexShader9 );
        CRY_ASSERT( SUCCEEDED( hr ) );

        // Pixel shader

        /*
        // FSTriangle.ps
        // fxc /O2 /T ps_2_0 /E PSMain
        texture Texture : register(s0);

        sampler PointSampler = sampler_state
        {
            Texture = <Texture>;
            MinFilter = Point;
            MagFilter = Point;
            MipFilter = Point;
            MaxAnisotropy = 1;
            AddressU  = CLAMP;
            AddressV  = CLAMP;
        };

        struct VertexPT
        {
            float4 PositionL : POSITION0;
            float2 TexCoord  : TEXCOORD0;
        };

        float4 PSMain(VertexPT psIn) : COLOR
        {
            return tex2D(PointSampler, psIn.TexCoord);
        }

        */

        const char CompiledPS[] =
        {
            0x00, 0x02, 0xff, 0xff, 0xfe, 0xff, 0x23, 0x00, 0x43, 0x54, 0x41, 0x42,
            0x1c, 0x00, 0x00, 0x00, 0x57, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff,
            0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0xc1, 0x00, 0x00,
            0x50, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x50, 0x6f, 0x69, 0x6e, 0x74, 0x53, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x72,
            0x00, 0xab, 0xab, 0xab, 0x04, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x73, 0x5f, 0x32,
            0x5f, 0x30, 0x00, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74,
            0x20, 0x28, 0x52, 0x29, 0x20, 0x48, 0x4c, 0x53, 0x4c, 0x20, 0x53, 0x68,
            0x61, 0x64, 0x65, 0x72, 0x20, 0x43, 0x6f, 0x6d, 0x70, 0x69, 0x6c, 0x65,
            0x72, 0x20, 0x39, 0x2e, 0x32, 0x39, 0x2e, 0x39, 0x35, 0x32, 0x2e, 0x33,
            0x31, 0x31, 0x31, 0x00, 0x1f, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80,
            0x00, 0x00, 0x03, 0xb0, 0x1f, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x90,
            0x00, 0x08, 0x0f, 0xa0, 0x42, 0x00, 0x00, 0x03, 0x00, 0x00, 0x0f, 0x80,
            0x00, 0x00, 0xe4, 0xb0, 0x00, 0x08, 0xe4, 0xa0, 0x01, 0x00, 0x00, 0x02,
            0x00, 0x08, 0x0f, 0x80, 0x00, 0x00, 0xe4, 0x80, 0xff, 0xff, 0x00, 0x00,
        };
        hr = pDevice->CreatePixelShader( ( DWORD* )CompiledPS, &m_pPixelShader9 );
        CRY_ASSERT( SUCCEEDED( hr ) );

        hr = pDevice->CreateStateBlock( D3DSBT_ALL, &m_pStateBlock );
        CRY_ASSERT( SUCCEEDED( hr ) );
    }

    void CFullscreenTriangleDrawer::CreateDX11Resources()
    {
        ID3D11Device* pDevice = static_cast<ID3D11Device*>( gD3DSystem->GetDevice() );

        HRESULT hr = S_OK;

        /*
        // fxc /O2 /T vs_4_0 /E VSMain

        struct VSScreenQuadOutput
        {
            float4 Position : SV_POSITION;
            float2 TexCoords0 : TEXCOORD0;
        };

        VSScreenQuadOutput VSMain(uint VertexID: SV_VertexID)
        {
            VSScreenQuadOutput output = (VSScreenQuadOutput)0;

            output.TexCoords0 = float2( (VertexID << 1) & 2, VertexID & 2 );
            output.Position = float4( output.TexCoords0 * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );

            return output;
        }

        */
        const char CompiledVS[] =
        {
            0x44, 0x58, 0x42, 0x43, 0x0d, 0x62, 0xaf, 0x9b, 0xa7, 0xca, 0xdb, 0xb0,
            0xc3, 0x92, 0xc3, 0xc1, 0x99, 0xd5, 0x59, 0xe6, 0x01, 0x00, 0x00, 0x00,
            0xb4, 0x02, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
            0x8c, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x18, 0x01, 0x00, 0x00,
            0x38, 0x02, 0x00, 0x00, 0x52, 0x44, 0x45, 0x46, 0x50, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x1c, 0x00, 0x00, 0x00, 0x00, 0x04, 0xfe, 0xff, 0x00, 0xc1, 0x00, 0x00,
            0x1c, 0x00, 0x00, 0x00, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66,
            0x74, 0x20, 0x28, 0x52, 0x29, 0x20, 0x48, 0x4c, 0x53, 0x4c, 0x20, 0x53,
            0x68, 0x61, 0x64, 0x65, 0x72, 0x20, 0x43, 0x6f, 0x6d, 0x70, 0x69, 0x6c,
            0x65, 0x72, 0x20, 0x39, 0x2e, 0x32, 0x39, 0x2e, 0x39, 0x35, 0x32, 0x2e,
            0x33, 0x31, 0x31, 0x31, 0x00, 0xab, 0xab, 0xab, 0x49, 0x53, 0x47, 0x4e,
            0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
            0x53, 0x56, 0x5f, 0x56, 0x65, 0x72, 0x74, 0x65, 0x78, 0x49, 0x44, 0x00,
            0x4f, 0x53, 0x47, 0x4e, 0x50, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0f, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x03, 0x0c, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54,
            0x49, 0x4f, 0x4e, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44,
            0x00, 0xab, 0xab, 0xab, 0x53, 0x48, 0x44, 0x52, 0x18, 0x01, 0x00, 0x00,
            0x40, 0x00, 0x01, 0x00, 0x46, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x04,
            0x12, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
            0x67, 0x00, 0x00, 0x04, 0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, 0x32, 0x20, 0x10, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
            0x29, 0x00, 0x00, 0x07, 0x12, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0a, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07, 0x12, 0x00, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07,
            0x42, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x10, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x56, 0x00, 0x00, 0x05, 0x32, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x86, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0f,
            0x32, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
            0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x05,
            0x32, 0x20, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x46, 0x00, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x08, 0xc2, 0x20, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f,
            0x3e, 0x00, 0x00, 0x01, 0x53, 0x54, 0x41, 0x54, 0x74, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };

        hr = pDevice->CreateVertexShader( CompiledVS, sizeof( CompiledVS ), NULL, &m_pVertexShader11 );
        CRY_ASSERT( SUCCEEDED( hr ) );

        /*
        // fxc /O2 /T ps_4_0 /E PSMain

        Texture2D txDiffuse : register(t0);
        SamplerState texSampler : register(s0);

        struct VSScreenQuadOutput
        {
            float4 Position : SV_POSITION;
            float2 TexCoords0 : TEXCOORD0;
        };

        float4 PSMain(VSScreenQuadOutput input) : SV_Target
        {
            // Switch the red and blue channels for DX11 since CryEngine does
            // not provide the exact texture format required by Coherent UI
            return txDiffuse.Sample(texSampler, input.TexCoords0).bgra;
        }
        */
        const char CompiledPS[] =
        {
            0x44, 0x58, 0x42, 0x43, 0x2a, 0xce, 0xec, 0x64, 0xe0, 0xa8, 0xf3, 0xcd,
            0xc7, 0x9e, 0x5d, 0xcb, 0x86, 0xb6, 0x78, 0x94, 0x01, 0x00, 0x00, 0x00,
            0x70, 0x02, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
            0xe0, 0x00, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x6c, 0x01, 0x00, 0x00,
            0xf4, 0x01, 0x00, 0x00, 0x52, 0x44, 0x45, 0x46, 0xa4, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x1c, 0x00, 0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0x00, 0xc1, 0x00, 0x00,
            0x71, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x67, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x53,
            0x61, 0x6d, 0x70, 0x6c, 0x65, 0x72, 0x00, 0x74, 0x78, 0x44, 0x69, 0x66,
            0x66, 0x75, 0x73, 0x65, 0x00, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f,
            0x66, 0x74, 0x20, 0x28, 0x52, 0x29, 0x20, 0x48, 0x4c, 0x53, 0x4c, 0x20,
            0x53, 0x68, 0x61, 0x64, 0x65, 0x72, 0x20, 0x43, 0x6f, 0x6d, 0x70, 0x69,
            0x6c, 0x65, 0x72, 0x20, 0x36, 0x2e, 0x33, 0x2e, 0x39, 0x36, 0x30, 0x30,
            0x2e, 0x31, 0x36, 0x33, 0x38, 0x34, 0x00, 0xab, 0x49, 0x53, 0x47, 0x4e,
            0x50, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
            0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00,
            0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00,
            0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0xab, 0xab, 0xab,
            0x4f, 0x53, 0x47, 0x4e, 0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0f, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65,
            0x74, 0x00, 0xab, 0xab, 0x53, 0x48, 0x44, 0x52, 0x80, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x03,
            0x00, 0x60, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x18, 0x00, 0x04,
            0x00, 0x70, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00,
            0x62, 0x10, 0x00, 0x03, 0x32, 0x10, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x65, 0x00, 0x00, 0x03, 0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x68, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x09,
            0xf2, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x10, 0x10, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x46, 0x7e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x60, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x05,
            0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x0c, 0x10, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x01, 0x53, 0x54, 0x41, 0x54,
            0x74, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };

        hr = pDevice->CreatePixelShader( CompiledPS, sizeof( CompiledPS ), NULL, &m_pPixelShader11 );
        CRY_ASSERT( SUCCEEDED( hr ) );

        // Create a One/InvSrcAlpha blend state
        D3D11_BLEND_DESC blendDesc = { 0 };
        blendDesc.AlphaToCoverageEnable  = false;
        blendDesc.IndependentBlendEnable = false;

        blendDesc.RenderTarget[0].BlendEnable            = true;
        blendDesc.RenderTarget[0].BlendOp                = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha           = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend              = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha         = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask  = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend               = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].SrcBlendAlpha          = D3D11_BLEND_ONE;

        hr = pDevice->CreateBlendState( &blendDesc, &m_pBlendState11 );
        CRY_ASSERT( SUCCEEDED( hr ) );
    }

    CFullscreenTriangleDrawer::~CFullscreenTriangleDrawer()
    {
        SAFE_RELEASE( m_pVertexDeclaration );
        SAFE_RELEASE( m_pVertexBuffer );
        SAFE_RELEASE( m_pVertexShader9 );
        SAFE_RELEASE( m_pPixelShader9 );
        SAFE_RELEASE( m_pStateBlock );

        SAFE_RELEASE( m_pVertexShader11 );
        SAFE_RELEASE( m_pPixelShader11 );
        SAFE_RELEASE( m_pBlendState11 );
    }

    void CFullscreenTriangleDrawer::Draw( void* pTexture )
    {
        switch ( gD3DSystem->GetType() )
        {
            case D3DPlugin::D3D_DX9:
                {
                    DrawDX9( static_cast<IDirect3DTexture9*>( pTexture ) );
                    break;
                }

            case D3DPlugin::D3D_DX11:
                {
                    DrawDX11( static_cast<ID3D11ShaderResourceView*>( pTexture ) );
                    break;
                }
        }
    }

    void CFullscreenTriangleDrawer::DrawDX9( IDirect3DTexture9* texture )
    {
        float width = ( float )gEnv->pRenderer->GetWidth();
        float height = ( float )gEnv->pRenderer->GetHeight();

        IDirect3DDevice9* pDevice = static_cast<IDirect3DDevice9*>( gD3DSystem->GetDevice() );

        m_pStateBlock->Capture();

        pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

        pDevice->SetVertexDeclaration( m_pVertexDeclaration );
        pDevice->SetStreamSource( 0, m_pVertexBuffer, 0, 5 * sizeof( float ) );

        pDevice->SetVertexShader( m_pVertexShader9 );
        pDevice->SetVertexShaderConstantF( 0, &width, 1 );
        pDevice->SetVertexShaderConstantF( 1, &height, 1 );

        pDevice->SetPixelShader( m_pPixelShader9 );
        pDevice->SetTexture( 0, texture );

        pDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 3 );

        m_pStateBlock->Apply();
    }

    void CFullscreenTriangleDrawer::DrawDX11( ID3D11ShaderResourceView* pTextureSRV )
    {
        ID3D11Device* pDevice = static_cast<ID3D11Device*>( gD3DSystem->GetDevice() );
        ID3D11DeviceContext* pContext = NULL;
        pDevice->GetImmediateContext( &pContext );

        CDX11StateGuard stateGuard;

        pContext->IASetInputLayout( NULL );
        pContext->IASetIndexBuffer( NULL, DXGI_FORMAT_UNKNOWN, 0 );
        pContext->IASetVertexBuffers( 0, 0, NULL, NULL, NULL );
        pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        pContext->VSSetShader( m_pVertexShader11, NULL, 0 );
        pContext->PSSetShader( m_pPixelShader11, NULL, 0 );
        ID3D11SamplerState* pNullSampler[] = { NULL };
        pContext->PSSetSamplers( 0, 1, pNullSampler );

        pContext->PSSetShaderResources( 0, 1, &pTextureSRV );

        pContext->OMSetBlendState( m_pBlendState11, NULL, 0xFFFFFFFF );

        // Draw
        pContext->Draw( 3, 0 );
    }
}