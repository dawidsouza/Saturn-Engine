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
#include "StaticMeshAssetViewer.h"

#include "Saturn/Core/Renderer/RenderThread.h"

#include "Saturn/Asset/AssetRegistry.h"
#include "Saturn/Vulkan/SceneRenderer.h"

#include "Saturn/ImGui/ImGuiAuxiliary.h"
#include "Saturn/Scene/Components.h"

#include "Saturn/Physics/PhysicsCooking.h"

namespace Saturn {

	static inline bool operator==( const ImVec2& lhs, const ImVec2& rhs ) { return lhs.x == rhs.x && lhs.y == rhs.y; }
	static inline bool operator!=( const ImVec2& lhs, const ImVec2& rhs ) { return !( lhs == rhs ); }

	StaticMeshAssetViewer::StaticMeshAssetViewer( AssetID id )
		: AssetViewer( id ), m_Camera( 45.0f, 1280.0f, 720.0f, 0.1f, 1000.0f )
	{
		m_Camera.SetActive( true );

		m_Scene = Ref<Scene>::Create();

		SceneRendererFlags flags = SceneRendererFlag_RenderGrid;
		m_SceneRenderer = Ref<SceneRenderer>::Create( flags );

		m_SceneRenderer->SetDynamicSky( 2.0f, 0.0f, 0.0f );
		m_SceneRenderer->SetCurrentScene( m_Scene.Get() );

		AddMesh();
	}

	StaticMeshAssetViewer::~StaticMeshAssetViewer()
	{
		m_SceneRenderer = nullptr;
		m_Scene = nullptr;
	}

	void StaticMeshAssetViewer::OnImGuiRender()
	{
		// Root Window.
		ImGuiWindowFlags RootWindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
		ImGui::Begin( "Static Mesh Asset Viewer", &m_Open, RootWindowFlags );

		// Create custom dockspace.
		ImGuiID dockID = ImGui::GetID( "StaticMeshDckspc" );
		ImGui::DockSpace( dockID, ImVec2( 0.0f, 0.0f ), ImGuiDockNodeFlags_None );

		//////////////////////////////////////////////////////////////////////////

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

		if( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ( ImGui::IsMouseClicked( ImGuiMouseButton_Right ) && !m_StartedRightClickInViewport ) )
		{
			ImGui::FocusWindow( GImGui->HoveredWindow );
			Input::Get().SetCursorMode( RubyCursorMode::Normal );
		}

		// Viewport

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
		std::string Name = "##" + std::to_string( m_AssetID );
		ImGui::Begin( Name.c_str(), 0, flags );

		ImGui::PushID( static_cast< int >( m_AssetID ) );
		
		if( m_ViewportSize != ImGui::GetContentRegionAvail() )
		{
			m_ViewportSize = ImGui::GetContentRegionAvail();

			m_SceneRenderer->SetViewportSize( ( uint32_t ) m_ViewportSize.x, ( uint32_t ) m_ViewportSize.y );
			m_Camera.SetViewportSize( ( uint32_t ) m_ViewportSize.x, ( uint32_t ) m_ViewportSize.y );
		}

		Auxiliary::Image( m_SceneRenderer->CompositeImage(), m_ViewportSize, { 0, 1 }, { 1, 0 } );

		ImGui::PopID();

		ImVec2 minBound = ImGui::GetWindowPos();
		ImVec2 maxBound = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_MouseOverViewport = ImGui::IsWindowHovered();

		m_AllowCameraEvents = ImGui::IsMouseHoveringRect( minBound, maxBound ) && m_ViewportFocused || m_StartedRightClickInViewport;

		ImGui::End();

		ImGui::Begin( "Sidebar" );

		if( Auxiliary::TreeNode( "Physics" ) )
		{
			ShapeType type = m_Mesh->GetAttachedShape();
			
			constexpr const char* pItems[] = { "None", "Box", "Sphere", "Capsule", "Convex Mesh", "Triangle Mesh" };
			static ShapeType SelectedEnum = type;
			static const char* Selected = pItems[ (int)SelectedEnum ];

			ImGui::Text( "Select Physics Shape Type:" );
			ImGui::SameLine();

			if( ImGui::BeginCombo( "##setshape", Selected ) )
			{
				for( int i = 0; i < IM_ARRAYSIZE( pItems ); i++ )
				{
					bool IsSelected = ( Selected == pItems[ i ] );

					if( ImGui::Selectable( pItems[ i ], IsSelected ) ) 
					{
						SelectedEnum = (ShapeType)i;
						Selected = pItems[ i ];

						m_Mesh->SetAttachedShape( SelectedEnum );
					}

					if( IsSelected )
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			if( SelectedEnum == ShapeType::TriangleMesh || SelectedEnum == ShapeType::ConvexMesh )
			{
				if( ImGui::Button( "Generate Mesh Collider" ) )
				{
					bool Result = PhysicsCooking::Get().CookMeshCollider( m_Mesh, SelectedEnum );

					// TODO: Show a dialog box of what failed.
				}
			}

			ImGui::Text( "Set Physics Material" );
			ImGui::SameLine();
			
			static AssetID id;
			static bool s_Open = true;

			if( ImGui::Button( "...##openmesh", ImVec2( 50, 20 ) ) )
			{
				s_Open = !s_Open;
			}
			
			if( Auxiliary::DrawAssetFinder( AssetType::PhysicsMaterial, &s_Open, id ) ) 
			{
				m_Mesh->SetPhysicsMaterial( id );
			}

			Auxiliary::EndTreeNode();
		}

		ImGui::End();

		ImGui::Begin( "##Toolbar" );

		ImGui::BeginVertical( "##tbv" );

		if( ImGui::Button( "Save", ImVec2( 50, 50 ) ) ) 
		{
			StaticMeshAssetSerialiser sma;
			sma.Serialise( m_Mesh );
		}

		ImGui::EndVertical();

		ImGui::End();

		ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding
		ImGui::End();

		if( m_Open == false )
		{
			AssetViewer::DestroyViewer( m_AssetID );
		}
	}

	void StaticMeshAssetViewer::OnUpdate( Timestep ts )
	{
		m_Camera.SetActive( m_AllowCameraEvents );
		m_Camera.OnUpdate( ts );

		// Update Scene for rendering (on main thread).
		m_Scene->OnRenderEditor( m_Camera, ts, *m_SceneRenderer );

		RenderThread::Get().Queue( [=]()
			{
				m_SceneRenderer->RenderScene();
			} );

		if( Input::Get().MouseButtonPressed( RubyMouseButton::Right ) && !m_StartedRightClickInViewport && m_ViewportFocused && m_MouseOverViewport )
			m_StartedRightClickInViewport = true;

		if( !Input::Get().MouseButtonPressed( RubyMouseButton::Right ) )
			m_StartedRightClickInViewport = false;
	}

	void StaticMeshAssetViewer::OnEvent( RubyEvent& rEvent )
	{
		if( m_MouseOverViewport && m_AllowCameraEvents )
			m_Camera.OnEvent( rEvent );
	}

	void StaticMeshAssetViewer::AddMesh()
	{
		Ref<StaticMesh> mesh = AssetManager::Get().GetAssetAs<StaticMesh>( m_AssetID );

		m_Mesh = mesh;

		m_Open = true;

		Ref<Entity> e = Ref<Entity>::Create( m_Scene.Get() );
		e->SetName( "InternalViewerEntity" );

		e->AddComponent<StaticMeshComponent>().Mesh = mesh;
	}

}