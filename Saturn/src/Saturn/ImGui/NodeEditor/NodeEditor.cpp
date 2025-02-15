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

#include "sppch.h"
#include "NodeEditor.h"
#include "Saturn/Vulkan/Image2D.h"
#include "Saturn/Vulkan/Texture.h"
#include "Saturn/Asset/AssetManager.h"

#include "Saturn/ImGui/ImGuiAuxiliary.h"

// imgui_node_editor
#include "builders.h"
#include "Saturn/Vendor/widgets.h"
#include "Saturn/Vendor/Drawing.h"

#include <backends/imgui_impl_vulkan.h>

namespace util = ax::NodeEditor::Utilities;

namespace Saturn {

	static int s_ID = 1;

	static Ref<Texture2D> s_BlueprintBackground;
	static ImTextureID s_BlueprintBackgroundID;

	int GetNextID()
	{
		return s_ID++;
	}

	void BuildNode( Ref<Node>& rNode )
	{
		for( auto& input : rNode->Inputs )
		{
			input->Node = rNode;
			input->Kind = PinKind::Input;
		}

		for( auto& output : rNode->Outputs )
		{
			output->Node = rNode;
			output->Kind = PinKind::Output;
		}
	}

	struct SelectAssetInfo
	{
		ed::PinId   ID = 0;
		ed::NodeId  NodeID = 0;
		AssetID     Asset = 0;
		std::string AssetName = "";
		AssetType DesiredAssetType = AssetType::Unknown;
	};

	static SelectAssetInfo s_SelectAssetInfo;

	// TODO: What if we do want to add this node editor the the asset viewers list?
	NodeEditor::NodeEditor( AssetID ID )
		: AssetViewer()
	{
		m_AssetID = ID;

		CreateEditor();

		s_BlueprintBackground = Ref<Texture2D>::Create( "content/textures/BlueprintBackground.png", AddressingMode::Repeat );

		s_BlueprintBackgroundID = ImGui_ImplVulkan_AddTexture( s_BlueprintBackground->GetSampler(), s_BlueprintBackground->GetImageView(), s_BlueprintBackground->GetDescriptorInfo().imageLayout );
	}

	NodeEditor::NodeEditor()
		: AssetViewer()
	{
		m_Editor = nullptr;

		s_BlueprintBackground = Ref<Texture2D>::Create( "content/textures/BlueprintBackground.png", AddressingMode::Repeat );

		s_BlueprintBackgroundID = ImGui_ImplVulkan_AddTexture( s_BlueprintBackground->GetSampler(), s_BlueprintBackground->GetImageView(), s_BlueprintBackground->GetDescriptorInfo().imageLayout );
	}

	NodeEditor::~NodeEditor()
	{
		s_BlueprintBackground = nullptr;
	}

	void NodeEditor::CreateEditor()
	{
		ed::Config config;
		config.SettingsFile = nullptr;
		config.UserPointer = this;

		config.SaveSettings = []( const char* pData, size_t size, ed::SaveReasonFlags reason, void* pUserPointer ) -> bool
		{
			auto* pThis = static_cast< NodeEditor* >( pUserPointer );

			pThis->m_NodeEditorState = pData;

			return true;
		};

		config.LoadSettings = []( char* pData, void* pUserData ) -> size_t
		{
			auto* pThis = static_cast< NodeEditor* >( pUserData );

			const auto& State = pThis->m_NodeEditorState;

			if( !pData )
			{
				return State.size();
			}
			else
			{
				memcpy( pData, State.data(), State.size() );
				SAT_CORE_INFO( "Assigned Node editor data is: {0}", State );
			}

			return 0;
		};

		config.LoadNodeSettings = []( ed::NodeId nodeId, char* pData, void* pUserPointer ) -> size_t
		{
			auto* pThis = static_cast< NodeEditor* >( pUserPointer );

			auto pNode = pThis->FindNode( nodeId );

			if( !pNode )
				return 0;

			if( pData != nullptr )
				memcpy( pData, pNode->State.data(), pNode->State.size() );

			return pNode->State.size();
		};

		config.SaveNodeSettings = []( ed::NodeId nodeId, const char* pData, size_t size, ed::SaveReasonFlags reason, void* pUserPointer ) -> bool
		{
			auto* pThis = static_cast< NodeEditor* >( pUserPointer );

			auto pNode = pThis->FindNode( nodeId );

			if( !pNode )
				return false;

			pNode->State.assign( pData, size );

			SAT_CORE_INFO( "Assigned Node data is: {0}", pNode->State );

			return true;
		};

		m_Editor = ed::CreateEditor( &config );
		ed::SetCurrentEditor( m_Editor );
	}

	void NodeEditor::Reload()
	{
		ed::SetCurrentEditor( nullptr );

		ed::DestroyEditor( m_Editor );

		m_Editor = nullptr;

		CreateEditor();
	}

	void NodeEditor::Close()
	{
		m_OnCompile();

		ed::DestroyEditor( m_Editor );
		ed::SetCurrentEditor( nullptr );

		m_Nodes.clear();
		m_Links.clear();
		m_NodeEditorState = "";

		m_OnClose();
	}

	Ref<Node> NodeEditor::AddNode( NodeSpecification& spec, ImVec2 position )
	{
		Ref<Node> node = Ref<Node>::Create( GetNextID(), spec.Name.c_str(), spec.Color );
		m_Nodes.emplace_back( node );

		for( auto& rOutput : spec.Outputs ) 
		{
			Ref<Pin> pin = Ref<Pin>::Create( GetNextID(), rOutput.Name.c_str(), rOutput.Type, node->ID );
			node->Outputs.push_back( pin );
		}

		for( auto& rInput : spec.Inputs ) 
		{
			Ref<Pin> pin = Ref<Pin>::Create( GetNextID(), rInput.Name.c_str(), rInput.Type, node->ID );
			node->Inputs.push_back( pin );

			// This should be more than enough data for one pin, holds 16 floats.
			pin->ExtraData.Allocate( 64 );
			pin->ExtraData.Zero_Memory();
		}

		BuildNode( node );

		if( position.x != 0.0f && position.y != 0.0f )
			ed::SetNodePosition( node->ID, position );

		node->ExtraData.Allocate( 1024 );
		node->ExtraData.Zero_Memory();

		return m_Nodes.back();
	}

	bool NodeEditor::IsPinLinked( ed::PinId id )
	{
		if( !id )
			return false;

		for( auto& link : m_Links )
			if( link->StartPinID == id || link->EndPinID == id )
				return true;

		return false;
	}

	bool NodeEditor::CanCreateLink( const Ref<Pin>& a, const Ref<Pin>& b )
	{
		if( !a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node )
			return false;

		return true;
	}

	Ref<Pin> NodeEditor::FindPin( ed::PinId id )
	{
		if( !id )
			return nullptr;

		for( const auto& node : m_Nodes )
		{
			for( const auto& pin : node->Inputs )
			{
				if( pin->ID == id )
				{
					return pin;
				}
			}

			for( const auto& pin : node->Outputs )
			{
				if( pin->ID == id )
				{
					return pin;
				}
			}
		}

		return nullptr;
	}

	const Ref<Pin>& NodeEditor::FindPin( ed::PinId id ) const
	{
		if( !id )
			return nullptr;

		for( const auto& node : m_Nodes )
		{
			for( const auto& pin : node->Inputs )
			{
				if( pin->ID == id )
				{
					return pin;
				}
			}

			for( const auto& pin : node->Outputs )
			{
				if( pin->ID == id )
				{
					return pin;
				}
			}
		}

		return nullptr;
	}

	Ref<Link> NodeEditor::FindLink( ed::LinkId id )
	{
		for( auto& link : m_Links )
			if( link->ID == id )
				return link;

		return nullptr;
	}

	const Ref<Link>& NodeEditor::FindLink( ed::LinkId id ) const
	{
		for( auto& link : m_Links )
			if( link->ID == id )
				return link;

		return nullptr;
	}

	Ref<Node> NodeEditor::FindNode( ed::NodeId id )
	{
		for( auto& node : m_Nodes )
			if( node->ID == id )
				return node;

		return nullptr;
	}

	const Ref<Node>& NodeEditor::FindNode( ed::NodeId id ) const
	{
		for( auto& node : m_Nodes )
			if( node->ID == id )
				return node;

		return nullptr;
	}

	Saturn::Ref<Node> NodeEditor::FindNode( const std::string& rName )
	{
		for( auto& node : m_Nodes ) 
		{
			if( node->Name == rName )
				return node;
		}

		return nullptr;
	}

	const Saturn::Ref<Node>& NodeEditor::FindNode( const std::string& rName ) const
	{
		for( auto& node : m_Nodes ) 
		{
			if( node->Name == rName )
				return node;
		}

		return nullptr;
	}

	Ref<Link> NodeEditor::FindLinkByPin( ed::PinId id )
	{
		if( !id )
			return nullptr;

		if( !IsPinLinked( id ) )
			return nullptr;

		for( auto& link : m_Links )
			if( link->StartPinID == id || link->EndPinID == id )
				return link;

		return nullptr;
	}

	Ref<Node> NodeEditor::FindNodeByPin( ed::PinId id )
	{
		const auto& Pin = FindPin( id );
		const auto& Link = FindLinkByPin( id );

		return Pin->Node;
	}

	ImColor GetIconColor( PinType type )
	{
		switch( type )
		{
			default:
			case PinType::Flow:     return ImColor( 255, 255, 255 ); // Same as Material_Sampler2D
			case PinType::Bool:     return ImColor( 220, 48, 48 );
			case PinType::Int:      return ImColor( 68, 201, 156 );
			case PinType::Float:    return ImColor( 147, 226, 74 );
			case PinType::String:   return ImColor( 124, 21, 153 );
			case PinType::Object:   return ImColor( 51, 150, 215 );
			case PinType::Function: return ImColor( 218, 0, 183 );
			case PinType::Delegate: return ImColor( 255, 48, 48 );
			case PinType::AssetHandle: return ImColor( 0, 0, 255 );
		}
	}

	void DrawPinIcon( const Ref<Pin>& pin, bool connected, int alpha )
	{
		ax::Drawing::IconType type;
		ImColor color = GetIconColor( pin->Type );
		color.Value.w = alpha / 255.0f;

		switch( pin->Type )
		{
			case PinType::Flow:				  type = ax::Drawing::IconType::Flow;   break;
			case PinType::Bool:				  type = ax::Drawing::IconType::Circle; break;
			case PinType::Int:				  type = ax::Drawing::IconType::Circle; break;
			case PinType::Float:			  type = ax::Drawing::IconType::Circle; break;
			case PinType::String:			  type = ax::Drawing::IconType::Circle; break;
			case PinType::Object:			  type = ax::Drawing::IconType::Circle; break;
			case PinType::Function:			  type = ax::Drawing::IconType::Circle; break;
			case PinType::Material_Sampler2D: type = ax::Drawing::IconType::Circle; break;
			case PinType::AssetHandle:        type = ax::Drawing::IconType::Circle; break;
			case PinType::Delegate:           type = ax::Drawing::IconType::Square; break;
			default:
				return;
		}

		const float PIN_ICON_SIZE = 24;

		auto size = ImVec2( static_cast< float >( PIN_ICON_SIZE ), static_cast< float >( PIN_ICON_SIZE ) );

		if( ImGui::IsRectVisible( size ) ) 
		{
			auto cursorPos = ImGui::GetCursorScreenPos();
			auto drawList  = ImGui::GetWindowDrawList();

			ax::Drawing::DrawIcon( drawList, cursorPos, cursorPos + size, type, connected, color, ImColor( 32, 32, 32, alpha ) );
		}

		ImGui::Dummy( size );
	}

	void NodeEditor::OnImGuiRender()
	{
		// Safety
		ed::SetCurrentEditor( m_Editor );

		if( !m_Open )
			return;

		bool WasOpen = m_Open;

		ImGui::Begin( m_Name.c_str(), &m_Open );

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;

		ImGui::BeginHorizontal( "##TopbarItems" );

		if( ImGui::Button( "Zoom to content" ) )
			ed::NavigateToContent( false );

		if( ImGui::Button( "Show flow" ) )
			for( auto& link : m_Links )
				ed::Flow( link->ID );

		if( ImGui::Button( "Compile & Save" ) ) 
		{
			if( m_OnCompile )
			{
				m_OnCompile();

				for( auto& link : m_Links )
					ed::Flow( link->ID );
			}
			else
				SAT_CORE_ASSERT( false, "A compile callback function must be set!" );
		}

		ImGui::EndHorizontal();

		ImGui::SameLine( 0.0f, 12.0f );

		ed::Begin( "Node Editor" );

		auto cursorTopLeft = ImGui::GetCursorScreenPos();

		util::BlueprintNodeBuilder builder( s_BlueprintBackgroundID, s_BlueprintBackground->Width(), s_BlueprintBackground->Height() );

		bool OpenAssetPopup = false;
		bool OpenAssetColorPicker = false;

		for( auto& node : m_Nodes )
		{
			if( node->Type != NodeType::Blueprint && node->Type != NodeType::Simple )
				continue;

			const auto isSimple = node->Type == NodeType::Simple;

			builder.Begin( node->ID );

			if( !isSimple )
			{
				builder.Header( node->Color );

				ImGui::Spring( 0 );
				ImGui::TextUnformatted( node->Name.c_str() );
				ImGui::Spring( 1 );
				ImGui::Dummy( ImVec2( 0, 28 ) );
				ImGui::Spring( 0 );

				builder.EndHeader();
			}

			uint32_t pinIndex = 0;
			for( auto& input : node->Inputs )
			{
				auto alpha = ImGui::GetStyle().Alpha;

				builder.Input( input->ID );

				ImGui::PushStyleVar( ImGuiStyleVar_Alpha, alpha );

				DrawPinIcon( input, IsPinLinked( input->ID ), (int)(alpha * 255) );

				ImGui::Spring( 0 );

				if( !input->Name.empty() )
				{
					ImGui::TextUnformatted( input->Name.c_str() );
					ImGui::Spring( 0 );
				}

				if( input->Type == PinType::Bool )
				{
					ImGui::Button( "Hello" );
					ImGui::Spring( 0 );
				}

				if( input->Type == PinType::Float )
				{
					float value = input->ExtraData.Read<float>( pinIndex );

					ImGui::SetNextItemWidth( 25.0f );

					ImGui::PushID( input->ID.Get() );
					
					if( ImGui::DragFloat( "##floatinput", &value ) )
					{
						input->ExtraData.Write( &value, sizeof( float ), pinIndex );
					}

					ImGui::PopID();

					ImGui::Spring( 0 );
				}

				ImGui::PopStyleVar();

				builder.EndInput();
				pinIndex++;
			}

			if( isSimple )
			{
				builder.Middle();

				ImGui::Spring( 1, 0 );
				ImGui::TextUnformatted( node->Name.c_str() );
				ImGui::Spring( 1, 0 );
			}

			for( auto& output : node->Outputs )
			{
				if( !isSimple && output->Type == PinType::Delegate )
					continue;

				auto alpha = ImGui::GetStyle().Alpha;

				if( m_NewLinkPin && !CanCreateLink( m_NewLinkPin, output ) && output != m_NewLinkPin )
					alpha = alpha * ( 48.0f / 255.0f );

				ImGui::PushStyleVar( ImGuiStyleVar_Alpha, alpha );

				builder.Output( output->ID );

				if( !output->Name.empty() )
				{
					ImGui::Spring( 0 );
					ImGui::TextUnformatted( output->Name.c_str() );

					// Check if output has is an AssetHandle
					// TODO: Allow for certain asset types.
					if( output->Type == PinType::AssetHandle )
					{
						auto& rSavedUUID = node->ExtraData.Read<UUID>( 0 );
						
						std::string name = "Select Asset";

						if( rSavedUUID != 0 )
							name = std::to_string( rSavedUUID );
						else if( !s_SelectAssetInfo.AssetName.empty() )
							name = s_SelectAssetInfo.AssetName;

						if( ImGui::Button( name.c_str() ) )
						{
							OpenAssetPopup = true;
							s_SelectAssetInfo.ID     = output->ID;
							s_SelectAssetInfo.NodeID = node->ID;
						}
					}
					else if( node->Name == "Color Picker" && output->Type == PinType::Material_Sampler2D )
					{
						ImGui::BeginHorizontal( "PickerH" );

						if( ImGui::Button( "Color" ) ) 
						{
							OpenAssetColorPicker = true;

							s_SelectAssetInfo.ID = output->ID;
							s_SelectAssetInfo.NodeID = node->ID;
						}

						Auxiliary::DrawColoredRect( { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }, node->ExtraData.Read<ImVec4>( 0 ) );
						
						ImGui::EndHorizontal();
					}
				}

				ImGui::Spring( 0 );
				DrawPinIcon( output, IsPinLinked( output->ID ), ( int ) ( alpha * 255 ) );

				builder.EndOutput();
				ImGui::PopStyleVar();
			}

			builder.End();
		}

		ed::Suspend();

		if( OpenAssetPopup )
			ImGui::OpenPopup( "AssetFinderPopup" );

		if( OpenAssetColorPicker )
			ImGui::OpenPopup( "AssetColorPicker" );

		ImGui::SetNextWindowSize( { 250.0f, 0.0f } );
		if( ImGui::BeginPopup( "AssetFinderPopup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ) )
		{
			bool PopupModified = false;

			if( ImGui::BeginListBox( "##ASSETLIST", ImVec2( -FLT_MIN, 0.0f ) ) )
			{
				for( const auto& [assetID, rAsset] : AssetManager::Get().GetCombinedAssetMap() )
				{
					if( rAsset->Type != s_SelectAssetInfo.DesiredAssetType && s_SelectAssetInfo.DesiredAssetType != AssetType::Unknown )
						continue;

					bool Selected = ( s_SelectAssetInfo.Asset == assetID );

					if( ImGui::Selectable( rAsset->GetName().c_str(), Selected ) )
					{
						s_SelectAssetInfo.Asset = assetID;
						s_SelectAssetInfo.AssetName = rAsset->GetName();

						Ref<Node> Node = nullptr;

						for( auto& rNode : m_Nodes )
						{
							if( rNode->ID == s_SelectAssetInfo.NodeID )
							{
								Node = rNode;
								break;
							}
						}

						if( Node )
						{
							// Write asset id
							Node->ExtraData.Write( ( uint8_t* ) &assetID, sizeof( UUID ), 0 );
						}

						PopupModified = true;
					}

					if( Selected )
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndListBox();
			}

			if( PopupModified )
			{
				ImGui::CloseCurrentPopup();

				s_SelectAssetInfo.Asset     = 0;
				s_SelectAssetInfo.ID        = 0;
				s_SelectAssetInfo.NodeID    = 0;
				s_SelectAssetInfo.AssetName = "";
				s_SelectAssetInfo.DesiredAssetType = AssetType::Unknown;
			}

			ImGui::EndPopup();
		}

		ImGui::SetNextWindowSize( { 350.0f, 0.0f } );
		if( ImGui::BeginPopup( "AssetColorPicker", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ) )
		{
			bool PopupModified = false;

			ImVec4 color = ImVec4( 114.0f / 255.0f, 144.0f / 255.0f, 154.0f / 255.0f, 200.0f / 255.0f );

			Ref<Node> Node = nullptr;
			Node = FindNode( s_SelectAssetInfo.NodeID );

			color = Node->ExtraData.Read<ImVec4>( 0 );

			if( color.x == 0 && color.y == 0 && color.z == 0  && color.w == 0 )
				color = ImVec4( 114.0f / 255.0f, 144.0f / 255.0f, 154.0f / 255.0f, 200.0f / 255.0f );

			if( ImGui::ColorPicker3( "Color Picker", (float*)&color ) ) 
			{
				Node->ExtraData.Write( (uint8_t*)&color, sizeof( ImVec4 ), 0 );

				PopupModified = true;
			}
			else
			{
				if( PopupModified )
				{
					ImGui::CloseCurrentPopup();

					s_SelectAssetInfo.Asset = 0;
					s_SelectAssetInfo.ID = 0;
					s_SelectAssetInfo.NodeID = 0;
					s_SelectAssetInfo.AssetName = "";
					s_SelectAssetInfo.DesiredAssetType = AssetType::Unknown;
				}
			}

			/*
			if( PopupModified )
			{
				ImGui::CloseCurrentPopup();

				s_SelectAssetInfo.Asset = 0;
				s_SelectAssetInfo.ID = 0;
				s_SelectAssetInfo.NodeID = 0;
				s_SelectAssetInfo.AssetName = "";
			}
			*/

			ImGui::EndPopup();
		}

		ed::Resume();

		// Link the links
		for( auto& link : m_Links )
			ed::Link( link->ID, link->StartPinID, link->EndPinID, link->Color, 2.0f );

		if( !m_CreateNewNode )
		{
			if( ed::BeginCreate( ImColor( 255, 255, 255 ), 2.0f ) )
			{
				auto showLabel = []( const char* label, ImColor color )
				{
					ImGui::SetCursorPosY( ImGui::GetCursorPosY() - ImGui::GetTextLineHeight() );
					auto size = ImGui::CalcTextSize( label );

					auto padding = ImGui::GetStyle().FramePadding;
					auto spacing = ImGui::GetStyle().ItemSpacing;

					ImGui::SetCursorPos( ImGui::GetCursorPos() + ImVec2( spacing.x, -spacing.y ) );

					auto rectMin = ImGui::GetCursorScreenPos() - padding;
					auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

					auto drawList = ImGui::GetWindowDrawList();
					drawList->AddRectFilled( rectMin, rectMax, color, size.y * 0.15f );
					ImGui::TextUnformatted( label );
				};

				ed::PinId StartPinId = 0;
				ed::PinId EndPinId = 0;

				if( ed::QueryNewLink( &StartPinId, &EndPinId ) )
				{
					Ref<Pin> StartPin = nullptr; 
					Ref<Pin> EndPin   = nullptr;

					StartPin = FindPin( StartPinId );
					EndPin = FindPin( EndPinId );

					m_NewLinkPin = StartPin ? StartPin : EndPin;

					if( StartPin->Kind == PinKind::Input ) 
					{
						std::swap( StartPin, EndPin );
						std::swap( StartPinId, EndPinId );
					}

					if( StartPin && EndPin )
					{
						// Pin is the same, reject.
						if( EndPin == StartPin )
						{
							ed::RejectNewItem( ImColor( 225, 0, 0 ), 2.0f );
						}
						else if( EndPin->Kind == StartPin->Kind )  // Same kind, input/output into input/output.
						{
							showLabel( "x Incompatible Pin Kind, input/output into input/output", ImColor( 45, 32, 32, 180 ) );

							ed::RejectNewItem( ImColor( 225, 0, 0 ), 2.0f );
						}
						else if( EndPin->Type != StartPin->Type )
						{
							showLabel( "x Incompatible Pin Type", ImColor( 45, 32, 32, 180 ) );

							ed::RejectNewItem( ImColor( 225, 128, 128 ), 2.0f );
						}
						else // Vaild type, accept
						{
							showLabel( "+ Create Link", ImColor( 32, 45, 32, 180 ) );
							if( ed::AcceptNewItem( ImColor( 128, 255, 128 ), 4.0f ) )
							{
								m_Links.emplace_back( Ref<Link>::Create( GetNextID(), StartPinId, EndPinId ) );
								m_Links.back()->Color = GetIconColor( StartPin->Type );
							}
						}
					}
				}
			
				// If the link is not connected, user maybe want to create a new node rather than link it.
				ed::PinId id = 0;
				if( ed::QueryNewNode( &id ) ) 
				{
					m_NewLinkPin = FindPin( id );

					if( m_NewLinkPin  )
						showLabel( "+ Create Node", ImColor( 32, 45, 32, 180 ) );

					if( ed::AcceptNewItem() ) 
					{
						m_CreateNewNode = true;
						
						m_NewNodeLinkPin = FindPin( id );
						m_NewLinkPin = nullptr;

						ed::Suspend();
						ImGui::OpenPopup( "Create New Node" );
						ed::Resume();
					}
				}
			}
			else
				m_NewLinkPin = nullptr;

			ed::EndCreate();

			if( ed::BeginDelete() )
			{
				ed::LinkId linkId = 0;
				while( ed::QueryDeletedLink( &linkId ) )
				{
					if( ed::AcceptDeletedItem() )
					{
						auto id = std::find_if( m_Links.begin(), m_Links.end(), [linkId]( auto& link ) { return link->ID == linkId; } );
						if( id != m_Links.end() )
							m_Links.erase( id );
					}
				}

				ed::NodeId nodeId = 0;
				while( ed::QueryDeletedNode( &nodeId ) )
				{
					if( ed::AcceptDeletedItem() )
					{
						auto id = std::find_if( m_Nodes.begin(), m_Nodes.end(), [nodeId]( auto& node ) { return node->ID == nodeId; } );
						if( id != m_Nodes.end() )
							m_Nodes.erase( id );

						DeleteDeadLinks( nodeId );
					}
				}
			}
			ed::EndDelete();
		}

		ImGui::SetCursorScreenPos( cursorTopLeft );

		ed::Suspend();

		auto openPopupPosition = ImGui::GetMousePos();
		if( ed::ShowBackgroundContextMenu() )
		{
			ImGui::OpenPopup( "Create New Node" );
			m_NewNodeLinkPin = nullptr;
		}
		ed::Resume();

		ed::Suspend();

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 8 ) );
		if( ImGui::BeginPopup( "Create New Node" ) )
		{
			auto mousePos = ed::ScreenToCanvas( ImGui::GetMousePosOnOpeningCurrentPopup() );

			Ref<Node> node = nullptr;

			if( m_CreateNewNodeFunction )
				node = m_CreateNewNodeFunction();

			if( node )
			{
				BuildNode( node );

				m_CreateNewNode = false;

				ed::SetNodePosition( node->ID, mousePos );

				if( auto& startPin = m_NewNodeLinkPin ) 
				{
					auto& pins = startPin->Kind == PinKind::Input ? node->Outputs : node->Inputs;

					for( auto& pin : pins ) 
					{
						if( CanCreateLink( startPin, pin ) )
						{
							auto& endPin = pin;

							if( startPin->Kind == PinKind::Input )
								std::swap( startPin, endPin );

							m_Links.emplace_back( Ref<Link>::Create( GetNextID(), startPin->ID, endPin->ID ) );
							m_Links.back()->Color = GetIconColor( startPin->Type );

							break;
						}
					}
				}
			}

			ImGui::EndPopup();
		}
		else
			m_CreateNewNode = false;

		ImGui::PopStyleVar();
		ed::Resume();

		ed::End();

		ImGui::End(); // NODE_EDITOR

		//if( WasOpen && !m_Open )
		//	Close();
	}

	void NodeEditor::LinkPin( ed::PinId Start, ed::PinId End )
	{
		const Ref<Pin>& pin = FindPin( Start );

		Ref<Link> link = Ref<Link>::Create( GetNextID(), Start, End );
		link->Color = GetIconColor( pin->Type );

		m_Links.emplace_back( link );
	}

	NodeEditorCompilationStatus NodeEditor::ThrowError( const std::string & rMessage )
	{
		SAT_CORE_ERROR( rMessage );

		return NodeEditorCompilationStatus::Failed;
	}

	void NodeEditor::ThrowWarning( const std::string& rMessage )
	{
		SAT_CORE_WARN( rMessage );
	}

	void NodeEditor::DeleteDeadLinks( ed::NodeId id )
	{
		auto wasConnectedToTheNode = [&]( const Ref<Link>& link )
		{
			return ( !FindPin( link->StartPinID ) ) || ( !FindPin( link->EndPinID ) )
				|| FindPin( link->StartPinID )->Node->ID == id
				|| FindPin( link->EndPinID )->Node->ID == id;
		};

		auto removeIt = std::remove_if( m_Links.begin(), m_Links.end(), wasConnectedToTheNode );
		const bool linkRemoved = removeIt != m_Links.end();

		m_Links.erase( removeIt, m_Links.end() );
	}

	void NodeEditor::SerialiseData( std::ofstream& rStream )
	{
		RawSerialisation::WriteString( m_Name, rStream );
		RawSerialisation::WriteString( m_NodeEditorState, rStream );

		size_t mapSize = m_Nodes.size();
		RawSerialisation::WriteObject( mapSize, rStream );

		for( auto& rNode : m_Nodes )
		{
			Node::Serialise( rNode, rStream );
		}

		mapSize = m_Links.size();
		RawSerialisation::WriteObject( mapSize, rStream );

		for( auto& rLinks : m_Links )
		{
			Link::Serialise( rLinks, rStream );
		}
	}

	void NodeEditor::DeserialiseData( std::ifstream& rStream )
	{
		m_Name = RawSerialisation::ReadString( rStream );
		m_NodeEditorState = RawSerialisation::ReadString( rStream );

		CreateEditor();

		size_t mapSize = 0;
		RawSerialisation::ReadObject( mapSize, rStream );

		m_Nodes.resize( mapSize );

		for( size_t i = 0; i < mapSize; i++ )
		{
			Ref<Node> node = Ref<Node>::Create();

			Node::Deserialise( node, rStream );

			m_Nodes[ i ] = node;
		}

		mapSize = 0;
		RawSerialisation::ReadObject( mapSize, rStream );

		m_Links.resize( mapSize );

		for( size_t i = 0; i < mapSize; i++ )
		{
			Ref<Link> link = Ref<Link>::Create();

			Link::Deserialise( link, rStream );

			m_Links[ i ] = link;
		}
	}

}