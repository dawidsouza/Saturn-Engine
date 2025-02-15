/********************************************************************************************
*                                                                                           *
*                                                                                           *
*                                                                                           *
* MIT License                                                                               *
*                                                                                           *
* Copyright (c) 2020 - 2024 BEAST                                                           *
*                                                                                           *
* Permission is hereby granted, free of charge, to any person obtaining a copy              *
* of this software and associated documentation files (the "Software"), to deal             *
* in the Software without restriction, including without limitation the rights              *
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell                 *
* copies of the Software, and to permit persons to whom the Software is                     *
* furnished to do so, subject to the following conditions:                                  *
*                                                                                           *
* The above copyright notice and this permission notice shall be included in all            *
* copies or substantial portions of the Software.                                           *
*                                                                                           *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR                *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,                  *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE               *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER                    *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,             *
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE             *
* SOFTWARE.                                                                                 *
*********************************************************************************************
*/

#pragma once

#include "Texture.h"
#include "Shader.h"

#include <string>

namespace Saturn {

	class StaticMesh;
	class Submesh;
	class MaterialInstance;

	class Material : public RefTarget
	{
	public:
		 Material( const Ref< Saturn::Shader >& Shader, const std::string& MateralName );
		~Material();

		void Initialise( const std::string& rMaterialName );

		void Copy( Ref<Material>& rOther );

		void Bind( const Ref< StaticMesh >& rMesh, Submesh& rSubmsh, Ref< Shader >& Shader );
		void Bind( VkCommandBuffer CommandBuffer, Ref< Shader >& Shader );
		void BindDS( VkCommandBuffer CommandBuffer, VkPipelineLayout Layout );
		
		void RN_Update();
		void RN_Clean();

		void SetResource( const std::string& Name, const Ref< Saturn::Texture2D >& Texture );
		void SetResource( const std::string& Name, const Ref< Saturn::Texture2D >& Texture, uint32_t Index );

		template<typename Ty>
		void Set( const std::string& Name, const Ty& Value ) 
		{
			for ( auto& Uniform : m_Uniforms )
			{
				if ( Uniform.Name == Name )
				{
					if( Uniform.IsPushConstantData )
					{
						m_PushConstantData.Write( ( uint8_t* ) &Value, Uniform.Size, Uniform.Offset );
					}
					else
					{
						Uniform.Data.Write( ( uint8_t* ) &Value, Uniform.Size, Uniform.Offset );
					}
					
					m_AnyValueChanged = true;

					break;
				}
			}
		}
		
		template<typename Ty>
		Ty& Get( const std::string& Name ) 
		{
			auto Itr = std::find_if( m_Uniforms.begin(), m_Uniforms.end(), 
				[&]( auto& uniform ) { return uniform.Name == Name; } 
			);

			if( Itr != m_Uniforms.end() )
			{
				auto& Uniform = *( Itr );

				if( Uniform.IsPushConstantData )
				{
					return m_PushConstantData.Read< Ty >( Uniform.Offset );
				}
				else
				{
					return Uniform.Data.Read< Ty >( Uniform.Offset );
				}
			}

			return *( Ty* )nullptr;
		}
		
		Ref< Texture2D > GetResource( const std::string& Name );

		VkDescriptorSet GetDescriptorSet(uint32_t index = 0) { return m_DescriptorSets[index]; }

		bool HasAnyValueChanged() { return m_AnyValueChanged; };

		void SetName( const std::string& rName ) { m_Name = rName; }

	public:

		Ref< Saturn::Shader >& GetShader() { return m_Shader; }
		
		std::string& GetName() { return m_Name; }
		const std::string& GetName() const { return m_Name; }

	private:

		std::unordered_map< std::string, Ref<Texture2D> >& GetTextures() { return m_Textures; }
		const std::unordered_map< std::string, Ref<Texture2D> >& GetTextures() const { return m_Textures; }

		void WriteDescriptor( VkWriteDescriptorSet& rWDS );

	private:
		std::string m_Name = "";
		Ref< Saturn::Shader > m_Shader;

		bool m_AnyValueChanged = false;

		bool m_Updated[ MAX_FRAMES_IN_FLIGHT ];

		Buffer m_PushConstantData;
		
		std::vector< ShaderUniform > m_Uniforms;
		std::unordered_map< std::string, Ref<Texture2D> > m_Textures;

		// Binding Name -> Textures
		std::unordered_map< std::string, std::vector< Ref<Texture2D> > > m_TextureArrays;

		VkDescriptorSet m_DescriptorSets[ MAX_FRAMES_IN_FLIGHT ];

	private:
		friend class MaterialInstance;
		friend class MaterialAsset;
	};
}