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
#include "EditorLayer.h"

#include <Saturn/Project/Project.h>

#include <Saturn/ImGui/ImGuiAuxiliary.h>
#include <Saturn/Vulkan/SceneRenderer.h>
#include <Saturn/ImGui/TitleBar.h>
#include <Saturn/ImGui/MaterialAssetViewer.h>
#include <Saturn/ImGui/PrefabViewer.h>
#include <Saturn/ImGui/Panel/Panel.h>
#include <Saturn/ImGui/Panel/PanelManager.h>
#include <Saturn/ImGui/EditorIcons.h>

#include <Saturn/Serialisation/SceneSerialiser.h>
#include <Saturn/Serialisation/ProjectSerialiser.h>
#include <Saturn/Serialisation/EngineSettingsSerialiser.h>
#include <Saturn/Serialisation/AssetRegistrySerialiser.h>
#include <Saturn/Serialisation/AssetSerialisers.h>
#include <Saturn/Serialisation/AssetBundle.h>

#include <Saturn/Physics/PhysicsFoundation.h>

#include <Saturn/Vulkan/MaterialInstance.h>
#include <Saturn/Vulkan/ShaderBundle.h>
#include <Saturn/Vulkan/Renderer2D.h>

#include <Saturn/Core/EnvironmentVariables.h>

#include <ImGuizmo/ImGuizmo.h>

#include <imspinner/imspinner.h>

#include <Saturn/Core/Math.h>
#include <Saturn/Core/StringAuxiliary.h>
#include <Saturn/Core/EngineSettings.h>
#include <Saturn/Core/OptickProfiler.h>

#include <Saturn/Asset/AssetRegistry.h>
#include <Saturn/Asset/AssetManager.h>
#include <Saturn/Asset/Prefab.h>

#include <Saturn/GameFramework/Core/GameModule.h>

#include <Saturn/Core/Renderer/RenderThread.h>
#include <Saturn/Core/VirtualFS.h>

#include <Saturn/Audio/Sound2D.h>

#include <Saturn/Premake/Premake.h>
#include <Saturn/Core/Process.h>

#include <Ruby/RubyWindow.h>
#include <Ruby/RubyAuxiliary.h>

#include <typeindex>
#include <ranges>

#include <glm/gtc/type_ptr.hpp>

namespace Saturn {

	bool HasPremakePath = false;
	bool OpenAssetRegistryDebug = false;
	bool OpenLoadedAssetDebug = false;
	bool OpenAttributions = false;

	static constexpr inline bool operator==( const ImVec2& lhs, const ImVec2& rhs ) { return lhs.x == rhs.x && lhs.y == rhs.y; }
	static constexpr inline bool operator!=( const ImVec2& lhs, const ImVec2& rhs ) { return !( lhs == rhs ); }

	EditorLayer::EditorLayer() 
		: m_EditorCamera( 45.0f, 1280.0f, 720.0f, 0.1f, 1000.0f ), m_EditorScene( Ref<Scene>::Create() )
	{
		Scene::SetActiveScene( m_EditorScene.Get() );

		m_RuntimeScene = nullptr;
		
		// Create Panel Manager.
		m_PanelManager = Ref<PanelManager>::Create();
		
		m_PanelManager->AddPanel( Ref<SceneHierarchyPanel>::Create() );
		m_PanelManager->AddPanel( Ref<ContentBrowserPanel>::Create() );

		m_TitleBar = new TitleBar();
		m_TitleBar->AddMenuBarFunction( [&]() -> void
		{
			if( ImGui::BeginMenu( "File" ) )
			{
				if( ImGui::MenuItem( "Save Scene", "Ctrl+S" ) )          SaveFile();
				if( ImGui::MenuItem( "Save Scene As", "Ctrl+Shift+S" ) ) SaveFileAs();
				
				if( ImGui::MenuItem( "Save Project" ) )                  SaveProject();
				if( ImGui::MenuItem( "Close Project" ) )                 CloseEditorAndOpenPB();
				if( ImGui::MenuItem( "Exit", "Alt+F4" ) )                Application::Get().Close();

				ImGui::EndMenu();
			}

			if( ImGui::BeginMenu( "Saturn" ) )
			{
				if( ImGui::MenuItem( "Attributions" ) )
				{
					OpenAttributions = true;
				}

				ImGui::EndMenu();
			}

			if( ImGui::BeginMenu( "Project" ) )
			{
				if( ImGui::MenuItem( "Project settings" ) ) m_ShowUserSettings = !m_ShowUserSettings;

				if( ImGui::MenuItem( "Recreate project files" ) )
				{
					if( !Project::GetActiveProject()->HasPremakeFile() )
						Project::GetActiveProject()->CreatePremakeFile();

					Premake::Launch( Project::GetActiveProject()->GetRootDir().wstring() );
				}

				if( ImGui::MenuItem( "Setup Project for Distribution" ) )
				{
					Project::GetActiveProject()->PrepForDist();

					BuildShaderBundle();

					m_BlockingActionRunning = true;
					m_BlockingOperation = AssetBundle::GetBlockingOperation();
					m_BlockingOperation->OnComplete( [this]() { m_BlockingActionRunning = false; m_BlockingOperation = nullptr; } );

					m_BlockingOperation->SetJob( [this]() 
						{
							if( auto result = AssetBundle::BundleAssets(); result != AssetBundleResult::Success ) 
							{
								Application::Get().GetWindow()->FlashAttention();

								m_MessageBoxText = std::format( "Asset bundle failed to build error was: {0}", (int)result );
								m_ShowMessageBox = true;
							}
						} );

					m_BlockingOperation->Execute();
				}

				if( ImGui::MenuItem( "Build Shader Bundle" ) )
				{
					BuildShaderBundle();
				}

#if defined( SAT_DEBUG )
				if( ImGui::MenuItem( "DEBUG: Read Asset Bundle" ) )
				{
					Application::Get().GetSpecification().Flags |= ApplicationFlag_UseVFS;
					auto res = AssetBundle::ReadBundle();
				}

				if( ImGui::MenuItem( "DEBUG: Enable VFS Flag" ) )
				{
					Application::Get().GetSpecification().Flags |= ApplicationFlag_UseVFS;
				}
#endif

				if( ImGui::MenuItem( "Distribute project" ) )
				{
					Project::GetActiveProject()->Rebuild( ConfigKind::Dist );
					Project::GetActiveProject()->Distribute( ConfigKind::Dist );
				}

				ImGui::EndMenu();
			}

			if( ImGui::BeginMenu( "Settings" ) )
			{
				if( ImGui::MenuItem( "Project settings", "" ) ) m_ShowUserSettings ^= 1;
				if( ImGui::MenuItem( "Asset Registry Debug", "" ) ) OpenAssetRegistryDebug ^= 1;
				if( ImGui::MenuItem( "Loaded asset debug", "" ) ) OpenLoadedAssetDebug ^= 1;
				if( ImGui::MenuItem( "Editor Settings", "" ) ) m_OpenEditorSettings ^= 1;
				if( ImGui::MenuItem( "Show demo window", "" ) ) m_ShowImGuiDemoWindow ^= 1;
				if( ImGui::MenuItem( "Virtual File system debug", "" ) ) m_ShowVFSDebug ^= 1;

				ImGui::EndMenu();
			}
		} );

		Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();
		hierarchyPanel->SetContext( m_EditorScene );
		hierarchyPanel->SetSelectionChangedCallback( SAT_BIND_EVENT_FN( EditorLayer::SelectionChanged ) );

		m_EditorCamera.SetActive( true );
		
		m_CheckerboardTexture = Ref< Texture2D >::Create( "content/textures/editor/checkerboard.tga", AddressingMode::Repeat );

		m_StartRuntimeTexture = Ref< Texture2D >::Create( "content/textures/editor/Play.png", AddressingMode::ClampToEdge );
		m_EndRuntimeTexture   = Ref< Texture2D >::Create( "content/textures/editor/Stop.png", AddressingMode::ClampToEdge );

		m_TranslationTexture  = Ref< Texture2D >::Create( "content/textures/editor/Move.png", AddressingMode::ClampToEdge );
		m_RotationTexture     = Ref< Texture2D >::Create( "content/textures/editor/Rotate.png", AddressingMode::ClampToEdge );
		m_ScaleTexture        = Ref< Texture2D >::Create( "content/textures/editor/Scale.png", AddressingMode::ClampToEdge );
		m_SyncTexture         = Ref< Texture2D >::Create( "content/textures/editor/Sync.png", AddressingMode::ClampToEdge );
		m_PointLightTexture   = Ref< Texture2D >::Create( "content/textures/editor/Billboard_PointLight.png", AddressingMode::ClampToEdge, false );
		m_ExclamationTexture  = Ref< Texture2D >::Create( "content/textures/editor/Exclamation.png", AddressingMode::ClampToEdge );

		// Add all of our icons to the editor icons list so that we have use this anywhere else in the engine/editor.
		EditorIcons::AddIcon( m_CheckerboardTexture );
		EditorIcons::AddIcon( m_StartRuntimeTexture );
		EditorIcons::AddIcon( m_EndRuntimeTexture );
		EditorIcons::AddIcon( m_TranslationTexture );
		EditorIcons::AddIcon( m_RotationTexture );
		EditorIcons::AddIcon( m_ScaleTexture );
		EditorIcons::AddIcon( m_SyncTexture );
		EditorIcons::AddIcon( m_PointLightTexture );
		EditorIcons::AddIcon( m_ExclamationTexture );

		// Init Physics
		PhysicsFoundation* pPhysicsFoundation = new PhysicsFoundation();
		pPhysicsFoundation->Init();

		Ref<ContentBrowserPanel> contentBrowserPanel = m_PanelManager->GetPanel<ContentBrowserPanel>();

		auto& rUserSettings = EngineSettings::Get();

		ProjectSerialiser ps;
		ps.Deserialise( rUserSettings.FullStartupProjPath.string() );

		SAT_CORE_ASSERT( Project::GetActiveProject(), "No project was given." );
		
		VirtualFS::Get().MountBase( Project::GetActiveConfig().Name, rUserSettings.StartupProject );

		AssetManager* pAssetManager = new AssetManager();
		Project::GetActiveProject()->CheckMissingAssetRefs();
		CheckMissingEditorAssetRefs();

		// Setup content browser panel at project dir.
		contentBrowserPanel->ResetPath( rUserSettings.StartupProject );

		m_GameModule = new GameModule();

		OpenFile( Project::GetActiveProject()->GetConfig().StartupSceneID );

		// TODO: Do we really need to check this every time we load the editor?
		HasPremakePath = Auxiliary::HasEnvironmentVariable( "SATURN_PREMAKE_PATH" );
	}

	EditorLayer::~EditorLayer()
	{
		delete m_TitleBar;
		
		AssetViewer::Terminate();
		EditorIcons::Clear();

		m_CheckerboardTexture = nullptr;
		m_PointLightTexture = nullptr;

		m_TitleBar = nullptr;
	
		m_PanelManager = nullptr;

		Application::Get().PrimarySceneRenderer().SetCurrentScene( nullptr );
		
		if( m_RuntimeScene ) 
		{	
			m_RuntimeScene->OnRuntimeEnd();
			m_RuntimeScene = nullptr;
		}

		m_EditorScene = nullptr;

		VirtualFS::Get().UnmountBase( Project::GetActiveConfig().Name );

		// I would free the game DLL, however, there is some threading issues with Tracy.
		//delete m_GameModule;
	}

	void EditorLayer::OnUpdate( Timestep time )
	{
		SAT_PF_EVENT();

		Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();

		if( m_RequestRuntime )
		{
			if( !m_RuntimeScene )
			{
				m_RuntimeScene = Ref<Scene>::Create();
				Scene::SetActiveScene( m_RuntimeScene.Get() );

				m_EditorScene->CopyScene( m_RuntimeScene );

				m_RuntimeScene->OnRuntimeStart();

				hierarchyPanel->SetContext( m_RuntimeScene );

				Application::Get().PrimarySceneRenderer().SetCurrentScene( m_RuntimeScene.Get() );
			}
		}
		else
		{
			if( m_RuntimeScene && m_RuntimeScene->m_RuntimeRunning )
			{
				m_RuntimeScene->OnRuntimeEnd();
				Scene::SetActiveScene( m_EditorScene.Get() );

				hierarchyPanel->SetContext( m_EditorScene );

				m_RuntimeScene = nullptr;

				Application::Get().PrimarySceneRenderer().SetCurrentScene( m_EditorScene.Get() );
			}
		}

		if( m_RuntimeScene ) 
		{
			m_RuntimeScene->OnUpdate( time );
			m_RuntimeScene->OnRenderRuntime( time, Application::Get().PrimarySceneRenderer() );
		}
		else 
		{
			m_EditorCamera.SetActive( m_AllowCameraEvents );
			m_EditorCamera.OnUpdate( time );

			m_EditorScene->OnUpdate( time );
			m_EditorScene->OnRenderEditor( m_EditorCamera, time, Application::Get().PrimarySceneRenderer() );
		}

		if( Input::Get().MouseButtonPressed( RubyMouseButton::Right ) && !m_StartedRightClickInViewport && m_ViewportFocused && m_MouseOverViewport )
			m_StartedRightClickInViewport = true;

		if( !Input::Get().MouseButtonPressed( RubyMouseButton::Right ) )
			m_StartedRightClickInViewport = false;

		Input::Get().SetCanSetCursorMode( m_AllowCameraEvents );

		// Render scenes in other asset viewers
		AssetViewer::Update( time );
	}

	void EditorLayer::OnImGuiRender()
	{
		SAT_PF_EVENT();

		// Draw dockspace.
		ImGuiIO& io = ImGui::GetIO();
		ImGuiViewport* pViewport = ImGui::GetWindowViewport();

		ImGui::DockSpaceOverViewport( pViewport );
		
		if( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ( ImGui::IsMouseClicked( ImGuiMouseButton_Right ) && !m_StartedRightClickInViewport ) )
		{
			if( !m_RuntimeScene )
			{
				ImGui::FocusWindow( GImGui->HoveredWindow );
				Input::Get().SetCursorMode( RubyCursorMode::Normal );
			}
		}

		m_TitleBar->Draw();
		AssetViewer::Draw();

		m_PanelManager->DrawAllPanels();
		
		ImGui::Begin( "Scene Renderer" );

		Application::Get().PrimarySceneRenderer().ImGuiRender();

		if( Auxiliary::TreeNode( "Shaders", false ) ) 
		{
			ImGui::BeginVertical( "shadersV" );

			for( auto& [name, shader] : ShaderLibrary::Get().GetShaders() )
			{
				ImGui::Columns( 2 );
				ImGui::SetColumnWidth( 0, 125.0f );
				ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );

				ImGui::BeginHorizontal( name.c_str() );

				ImGui::Text( name.c_str() );

				ImGui::PopItemWidth();

				ImGui::NextColumn();

				if( ImGui::Button( "Recompile" ) )
				{
					if( !shader->TryRecompile() ) 
					{
						Application::Get().GetWindow()->FlashAttention();
					
						m_MessageBoxText = std::format( "Shader '{0}' failed to recompile. Defaulting back to last successful build.", shader->GetName() );
						m_ShowMessageBox = true;
					}
				}

				ImGui::PopItemWidth();

				ImGui::Columns( 1 );

				ImGui::EndHorizontal();
			}

			ImGui::EndVertical();

			Auxiliary::EndTreeNode();
		}

		ImGui::End();

		if( OpenAttributions )
		{
			if( ImGui::Begin( "Attributions", &OpenAttributions ) )
			{
				ImGui::Text("All icons in the engine are provided by icons8 via https://icons8.com/\nUsing the Tanah Basah set.");

				ImGui::End();
			}
		}

		if( m_ShowImGuiDemoWindow )  ImGui::ShowDemoWindow( &m_ShowImGuiDemoWindow );
		if( m_ShowUserSettings )     UI_Titlebar_UserSettings();
		if( OpenAssetRegistryDebug ) DrawAssetRegistryDebug();
		if( OpenLoadedAssetDebug ) 	 DrawLoadedAssetsDebug();
		if( m_OpenEditorSettings )   DrawEditorSettings();
		if( m_ShowVFSDebug )         DrawVFSDebug();

		ImGui::Begin( "Renderer" );

		ImGui::Text( "Frame Time: %.2f ms", Application::Get().Time().Milliseconds() );

		for( const auto& devices : VulkanContext::Get().GetPhysicalDeviceProperties() )
		{
			ImGui::Text( "Device Name: %s", devices.DeviceProps.deviceName );
			ImGui::Text( "API Version: %i", devices.DeviceProps.apiVersion );
			ImGui::Text( "Vendor ID: %i", devices.DeviceProps.vendorID );
			ImGui::Text( "Vulkan Version: 1.2.128" );
		}
		
		ImGui::End();

		if( m_BlockingActionRunning )
		{
			ImGui::OpenPopup( "Blocking Action" );
		}

		if( ImGui::BeginPopupModal( "Blocking Action", &m_BlockingActionRunning ) )
		{
			ImGui::BeginHorizontal( "##ItemsH" );

			ImSpinner::SpinnerAng( "##OPERATION_SPINNER", 25.0f, 2.0f, ImSpinner::white, ImSpinner::half_white, 8.6F );

			ImGui::Spring();

			if( m_BlockingOperation->GetTitle().empty() )
				ImGui::Text( "Please wait for the operation to complete..." );
			else
				ImGui::Text( m_BlockingOperation->GetTitle().c_str() );

			ImGui::EndHorizontal();
			
			if( float percent = m_BlockingOperation->GetProgress(); percent >= 1.0f )
			{
				ImGui::ProgressBar( percent / 100 );
			}

			if( std::string status = m_BlockingOperation->GetStatus(); !status.empty() )
			{
				ImGui::Text( status.c_str() );
			}

			ImGui::EndPopup();
		}
		
		ImGui::Begin( "Materials" );

		DrawMaterials();

		ImGui::End();

		DrawViewport();

		if( m_ShowMessageBox )       ShowMessageBox();
		CheckMissingEnv();
	}

	void EditorLayer::OnEvent( RubyEvent& rEvent )
	{
		if( m_MouseOverViewport )
			m_EditorCamera.OnEvent( rEvent );

		AssetViewer::ProcessEvent( rEvent );

		if( rEvent.Type == RubyEventType::KeyPressed )
			OnKeyPressed( (RubyKeyEvent&)rEvent );
	}

	void EditorLayer::SaveFileAs()
	{
		// TODO: Support Saving scene as!

		auto res = Application::Get().SaveFile( "Saturn Scene file (*.scene, *.sc)\0*.scene; *.sc\0" );

		SceneSerialiser serialiser( m_EditorScene );
		serialiser.Serialise();
	}

	void EditorLayer::SaveFile()
	{
		auto fullPath = Project::GetActiveProject()->FilepathAbs( m_EditorScene->Path );

		if( std::filesystem::exists( fullPath ) )
		{
			SceneSerialiser ss( m_EditorScene );
			ss.Serialise();
		}
		else
		{
			SaveFileAs();
		}
	}

	void EditorLayer::OpenFile( AssetID id )
	{
		Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();

		Ref<Scene> newScene = Ref<Scene>::Create();
		GActiveScene = newScene.Get();

		hierarchyPanel->ClearSelection();
		hierarchyPanel->SetContext( nullptr );

		Ref<Asset> asset = id == 0 ? nullptr : AssetManager::Get().FindAsset( id );
		
		if( id != 0 )
		{
			SceneSerialiser serialiser( newScene );
			serialiser.Deserialise( asset->Path );
		}

		m_EditorScene = newScene;

		if( asset )
		{
			m_EditorScene->Name = asset->Name;
			m_EditorScene->Path = asset->Path;
			m_EditorScene->ID = asset->ID;
			m_EditorScene->Type = asset->Type;
			m_EditorScene->Flags = asset->Flags;
		}
	
		GActiveScene = m_EditorScene.Get();

		hierarchyPanel->SetContext( m_EditorScene );
		newScene = nullptr;

		Application::Get().PrimarySceneRenderer().SetCurrentScene( m_EditorScene.Get() );
	}

	void EditorLayer::SaveProject()
	{
		ProjectSerialiser ps( Project::GetActiveProject() );
		ps.Serialise( Project::GetActiveProject()->GetConfig().Path );

		AssetRegistrySerialiser ars;
		ars.Serialise( AssetManager::Get().GetAssetRegistry() );
	}

	void EditorLayer::SelectionChanged( Ref<Entity> e )
	{
	}

	void EditorLayer::ViewportSizeCallback( uint32_t Width, uint32_t Height )
	{
	}

	bool EditorLayer::OnKeyPressed( RubyKeyEvent& rEvent )
	{
		switch( rEvent.GetScancode() )
		{
			case RubyKey::Delete:
			{
				Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();

				if( hierarchyPanel )
				{
					// Because of our ref system, the entity will be deleted when we clear the selections.
					// What we are really doing here is freeing it from the registry.

					for( auto& rEntity : hierarchyPanel->GetSelectionContexts() )
					{
						GActiveScene->DeleteEntity( rEntity );
					}

					hierarchyPanel->ClearSelection();
				}
			} break;

			case RubyKey::Q:
				m_GizmoOperation = -1;
				break;
			case RubyKey::W:
				m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
				break;
			case RubyKey::E:
				m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
				break;
			case RubyKey::R:
				m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
				break;
		}

		if( Input::Get().KeyPressed( RubyKey::Ctrl ) && !m_RuntimeScene )
		{
			switch( rEvent.GetScancode() )
			{
				case RubyKey::D:
				{
					Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();
					
					if( hierarchyPanel ) 
					{
						for( const auto& rEntity : hierarchyPanel->GetSelectionContexts() )
						{
							GActiveScene->DuplicateEntity( rEntity );
						}
					}

				} break;

				// TODO: Support more than one selection.
				case RubyKey::F:
				{
					Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();

					if( hierarchyPanel )
					{
						Ref<Entity> selectedEntity = hierarchyPanel->GetSelectionContext();

						if( selectedEntity )
						{
							// TODO: This should be it's own separate keybind what if we don't want to use the parents position.
							//       This also does not account if the parent has a parent and so on.
							if( selectedEntity->HasParent() )
							{
								Ref<Entity> parent = GActiveScene->FindEntityByID( selectedEntity->GetParent() );
								auto transform = GActiveScene->GetWorldSpaceTransform( parent );

								m_EditorCamera.Focus( transform.Position );
							}
							else
							{
								m_EditorCamera.Focus( selectedEntity->GetComponent<TransformComponent>().Position );
							}
						}
					}
				} break;

				case RubyKey::S:
				{
					SaveFile();
				} break;
			}

			if( Input::Get().KeyPressed( RubyKey::Shift ) )
			{
				switch( rEvent.GetScancode() )
				{
					case RubyKey::S:
					{
						SaveFileAs();
					} break;
				}
			}
		}

		return true;
	}

	static bool s_OpenAssetFinderPopup = false;
	static AssetID s_AssetFinderID = 0;

	void EditorLayer::UI_Titlebar_UserSettings()
	{
		static bool ShouldSaveProject = false;

		ImGuiIO& rIO = ImGui::GetIO();

		auto& userSettings = EngineSettings::Get();

		Ref<Project> ActiveProject = Project::GetActiveProject();

		auto& startupProject = userSettings.StartupProject;
		auto& startupScene = ActiveProject->GetConfig().StartupSceneID;

		Ref<Asset> asset = AssetManager::Get().FindAsset( startupScene );

		ImGui::SetNextWindowPos( ImVec2( rIO.DisplaySize.x * 0.5f - 150.0f, rIO.DisplaySize.y * 0.5f - 150.0f ), ImGuiCond_Once );

		ImGui::Begin( "Project settings", &m_ShowUserSettings );

		ImGui::Text( "Startup Scene:" );
		
		if( s_OpenAssetFinderPopup )
			ImGui::OpenPopup( "AssetFinderPopup" );
		
		ImGui::SameLine();
		startupScene == 0 ? ImGui::Text( "None" ) : ImGui::Text( asset->Name.c_str() );
		ImGui::SameLine();
		
		if( ImGui::Button( "...##scene" ) )
			s_OpenAssetFinderPopup = true;

		ImGui::SetNextWindowSize( { 250.0f, 0.0f } );
		if( ImGui::BeginPopup( "AssetFinderPopup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ) )
		{
			bool PopupModified = false;

			if( ImGui::BeginListBox( "##ASSETLIST", ImVec2( -FLT_MIN, 0.0f ) ) )
			{
				for( const auto& [assetID, rAsset] : AssetManager::Get().GetAssetRegistry()->GetAssetMap() )
				{
					bool Selected = ( s_AssetFinderID == assetID );

					if( rAsset->GetAssetType() == AssetType::Scene || rAsset->GetAssetType() == AssetType::Unknown )
					{
						if( ImGui::Selectable( rAsset->GetName().c_str() ) )
						{
							ActiveProject->GetConfig().StartupSceneID = rAsset->ID;
							
							s_AssetFinderID = assetID;

							PopupModified = true;
						}
					}

					if( Selected )
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndListBox();
			}

			if( PopupModified )
			{
				s_OpenAssetFinderPopup = false;
				ShouldSaveProject = true;

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		auto boldFont = rIO.Fonts->Fonts[ 1 ];
		ImGui::PushFont( boldFont );

		// Scene Renderer Settings

		ImGui::Text( "Ambient Occlusion Rendering Technique" );
		ImGui::Separator();

		ImGui::PopFont();

		constexpr const char* AOTechniques[] = { "SSAO", "HBAO", "None" };
		AOTechnique selectedTech = Application::Get().PrimarySceneRenderer().GetAOTechnique();

		const char* preview = AOTechniques[ ( int ) selectedTech ];

		ImGui::SetNextItemWidth( 130.0f );
		if( ImGui::BeginCombo( "##aotechniques", preview, ImGuiComboFlags_HeightSmall ) )
		{
			for( int i = 0; i < IM_ARRAYSIZE( AOTechniques ); i++ )
			{
				AOTechnique technique = ( AOTechnique ) i;
				bool selected = ( selectedTech == technique );

				ImGui::SetNextItemWidth( 130.0f );
				if( ImGui::Selectable( AOTechniques[ i ], selected ) )
					Application::Get().PrimarySceneRenderer().ChangeAOTechnique( technique );
			}

			ImGui::EndCombo();
		}

		ImGui::PushFont( boldFont );
		ImGui::Text( "Action Bindings" );
		ImGui::Separator();

		ImGui::PopFont();

		for( auto rIt = ActiveProject->GetActionBindings().begin(); rIt != ActiveProject->GetActionBindings().end(); )
		{
			auto& rBinding = *( rIt );

			char buffer[ 256 ];
			memset( buffer, 0, 256 );
			memcpy( buffer, rBinding.Name.data(), rBinding.Name.length() );

			std::string id = "##" + std::to_string( rBinding.ID );

			ImGui::SetNextItemWidth( 130.0f );
			if( ImGui::InputText( id.data(), buffer, 256 ) ) 
			{
				rBinding.Name = std::string( buffer );
			}

			ImGui::SameLine(); // HACK, There seems to bug with the ImGui Layout as the InputText works fine when it's not in a Horizontal layout. (Update) Seems to be with certain IDs/labels

			ImGui::BeginHorizontal( rBinding.Name.data() );

			ImGui::SetNextItemWidth( 130.0f );
			if( ImGui::BeginCombo( "##KEYLIST", rBinding.ActionName.data() ) )
			{
				for( int i = 0; i < RubyKey::EnumSize; i++ )
				{
					const auto& result = Ruby_KeyToString( (RubyKey) i );

					// This is here because of how we do our loop, some keys will be empty because the values to do not match up.
					if( result.empty() )
						continue;

					bool IsSelected = ( rBinding.ActionName == result );

					ImGui::PushID( i );

					ImGui::SetNextItemWidth( 130.0f );
					if( ImGui::Selectable( result.data(), IsSelected ) )
					{
						rBinding.Key = ( RubyKey ) i;
						rBinding.Type = ActionBindingType::Key;
						rBinding.ActionName = result;

						ShouldSaveProject = true;
					}

					if( IsSelected )
						ImGui::SetItemDefaultFocus();

					ImGui::PopID();
				}

				ImGui::EndCombo();
			}

			if( ImGui::SmallButton( "-" ) )
			{
				rIt = ActiveProject->GetActionBindings().erase( rIt );
				ShouldSaveProject = true;
			}
			else
			{
				++rIt;
			}

			ImGui::EndHorizontal();
		}

		if( ImGui::SmallButton( "+" ) ) 
		{
			ActionBinding ab;
			ab.Name = "Empty Binding";

			int count = 0;
			// Find all other actions bindings with the same name.
			for( const auto& bindings : ActiveProject->GetActionBindings() ) 
			{
				if( bindings.Name.contains( "Empty Binding" ) ) 
					count++;
			}

			if( count >= 1 ) 
			{
				ab.Name += " ";
				ab.Name += std::to_string( count );
			}

			ActiveProject->AddActionBinding( ab );
			ShouldSaveProject = true;
		}


		// This does not matter because the editor is not designed to run in Dist, however, right now I want to keep this in release builds.
#if !defined(SAT_DIST)
		ImGui::PushFont( boldFont );
		ImGui::Text( "Project Debug information" );
		ImGui::PopFont();

		ImGui::BeginVertical( "##PRJDBGV" );

		auto drawDebugText = [](const std::string& id, const std::string& key, const std::string& value) 
		{
			ImGui::BeginHorizontal( id.c_str() );
			ImGui::Text( key.c_str() );
			ImGui::Text( " : " );
			ImGui::Text( value.c_str() );
			ImGui::EndHorizontal();
		};

		drawDebugText( "##PRJD1", "Root Path", ActiveProject->GetRootDir().string() );
		drawDebugText( "##PRJD2", ".sproject Path", ActiveProject->GetConfig().Path.string() );
		drawDebugText( "##PRJD3", "Assets Path", ActiveProject->GetFullAssetPath().string() );
		drawDebugText( "##PRJD4", "Premake filename", ActiveProject->GetPremakeFile().string() );
		drawDebugText( "##PRJD5", "Temp Path", ActiveProject->GetTempDir().string() );
		drawDebugText( "##PRJD6", "Bin Path", ActiveProject->GetBinDir().string() );
		drawDebugText( "##PRJD7", "Cache Path", ActiveProject->GetFullCachePath().string() );

		ImGui::EndVertical();
#endif

		ImGui::End();

		if( ShouldSaveProject && !m_ShowUserSettings )
		{
			ProjectSerialiser ps;
			ps.Serialise( Project::GetActiveProject()->GetRootDir().string() );
		}
	}

	void EditorLayer::HotReloadGame()
	{
		SAT_CORE_ASSERT(false, "EditorLayer::HotReloadGame not implemented.");
	}

	void EditorLayer::CheckMissingEditorAssetRefs()
	{
		std::vector<std::string> DisallowedAssetExtensions = 
		{
			{ ".fbx"      },
			{ ".gltf"     },
			{ ".bin"      },
			{ ".glb"      },
			{ ".wav"      },
			{ ".lib"      },
			{ ".ttf"      },
			{ ".txt"      },
			{ ".blend"    },
			{ ".blend1"   },
			{ ".cpp"      },
			{ ".h"        },
			{ ".cs"       },
			{ ".lua"      },
			{ ".glsl"     },
			{ ".sproject" },
		};

		std::filesystem::path AssetPath = Application::Get().GetRootContentDir().parent_path();

		bool FileChanged = false;

		for( auto& rEntry : std::filesystem::recursive_directory_iterator( AssetPath ) )
		{
			if( rEntry.is_directory() )
				continue;

			std::filesystem::path filepath = std::filesystem::relative( rEntry.path(), AssetPath.parent_path() );
			auto filepathString = filepath.extension().string();

			if( filepath.extension() == ".sreg" || filepath.extension() == ".eng" )
				continue;

			Ref<Asset> asset = AssetManager::Get().FindAsset( filepath, AssetRegistryType::Editor );

			if( std::find( DisallowedAssetExtensions.begin(), DisallowedAssetExtensions.end(), filepathString ) != DisallowedAssetExtensions.end() )
				continue; // Extension is forbidden.

			const auto& assetReg = AssetManager::Get().GetEditorAssetRegistry()->GetAssetMap();
			if( asset == nullptr )
			{
				SAT_CORE_INFO( "Found an asset that exists in the system filesystem, however not in the asset registry, creating new asset." );

				auto type = AssetTypeFromExtension( filepathString );
				auto id = AssetManager::Get().CreateAsset( type, AssetRegistryType::Editor );
				asset = AssetManager::Get().FindAsset( id, AssetRegistryType::Editor );

				asset->SetPath( rEntry.path() );

				FileChanged = true;
			}
		}

		if( FileChanged )
		{
			AssetManager::Get().Save( AssetRegistryType::Editor );
		}
	}

	void EditorLayer::DrawAssetRegistryDebug()
	{
		if( ImGui::Begin( "Asset Manager", &OpenAssetRegistryDebug ) )
		{
			static ImGuiTextFilter Filter;

			ImGui::Text( "Search" );
			ImGui::SameLine();
			Filter.Draw( "##search" );

			ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoBordersInBody;
			if( ImGui::BeginTable( "##FileTable", 5, TableFlags, ImVec2( ImGui::GetWindowSize().x, ImGui::GetWindowSize().y ) ) )
			{
				ImGui::TableSetupColumn( "Asset Name" );
				ImGui::TableSetupColumn( "ID" );
				ImGui::TableSetupColumn( "Type" );
				ImGui::TableSetupColumn( "Is Editor Asset" );
				ImGui::TableSetupColumn( "Path" );

				ImGui::TableHeadersRow();

				int TableRow = 0;

				for( auto&& [id, asset] : AssetManager::Get().GetCombinedAssetMap() )
				{
					if( !Filter.PassFilter( asset->GetName().c_str() ) )
						continue;

					TableRow++;

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex( 0 );
					ImGui::Selectable( asset->GetName().c_str(), false );

					ImGui::TableSetColumnIndex( 1 );
					ImGui::Selectable( std::to_string( id ).c_str(), false );

					ImGui::TableSetColumnIndex( 2 );
					ImGui::Selectable( AssetTypeToString( asset->GetAssetType() ).c_str(), false );

					ImGui::TableSetColumnIndex( 3 );
					ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
					bool value = asset->IsFlagSet( AssetFlag::Editor );
					ImGui::Checkbox( "##editor", &value );
					ImGui::PopItemFlag();

					ImGui::TableSetColumnIndex( 4 );
					ImGui::Text( asset->Path.string().c_str() );
				}

				ImGui::EndTable();
			}

			ImGui::End();
		}
	}

	void EditorLayer::DrawLoadedAssetsDebug()
	{
		if( ImGui::Begin( "Loaded Assets", &OpenLoadedAssetDebug ) )
		{
			static ImGuiTextFilter Filter;

			ImGui::Text( "Search for assets..." );
			ImGui::SameLine();
			Filter.Draw( "##search" );

			ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoBordersInBody;
			if( ImGui::BeginTable( "##FileTable", 4, TableFlags, ImVec2( ImGui::GetWindowSize().x, ImGui::GetWindowSize().y * 0.85f ) ) )
			{
				ImGui::TableSetupColumn( "Asset Name" );
				ImGui::TableSetupColumn( "ID" );
				ImGui::TableSetupColumn( "Type" );
				ImGui::TableSetupColumn( "Is Editor Asset" );

				ImGui::TableHeadersRow();

				int TableRow = 0;

				for( auto&& [id, asset] : AssetManager::Get().GetCombinedLoadedAssetMap() )
				{
					if( !Filter.PassFilter( asset->GetName().c_str() ) )
						continue;

					TableRow++;

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex( 0 );
					ImGui::Selectable( asset->GetName().c_str(), false );

					ImGui::TableSetColumnIndex( 1 );
					ImGui::Selectable( std::to_string( id ).c_str(), false );

					ImGui::TableSetColumnIndex( 2 );
					ImGui::Selectable( AssetTypeToString( asset->GetAssetType() ).c_str(), false );

					ImGui::TableSetColumnIndex( 3 );
					ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
					bool value = asset->IsFlagSet( AssetFlag::Editor );
					ImGui::Checkbox( "##editor", &value );
					ImGui::PopItemFlag();
				}

				ImGui::EndTable();
			}

			ImGui::End();
		}
	}

	void EditorLayer::DrawEditorSettings()
	{
		auto& rIO = ImGui::GetIO();

		ImGui::SetNextWindowSize( ImVec2( 750.0f, 750.0f ), ImGuiCond_Appearing );
		if( ImGui::Begin( "Editor Settings", &m_OpenEditorSettings ) )
		{
			auto boldFont = rIO.Fonts->Fonts[ 1 ];
			auto italicsFont = rIO.Fonts->Fonts[ 2 ];

			ImGui::PushFont( boldFont );
			ImGui::Text( "Saturn Editor Settings" );
			ImGui::PopFont();

			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 0.7f } );
			ImGui::PushFont( italicsFont );
			ImGui::Text( "Saturn Engine Version: " );
			ImGui::SameLine();
			ImGui::Text( SAT_CURRENT_VERSION_STRING );
			ImGui::PopFont();
			ImGui::PopStyleColor();

			ImGui::PushStyleColor( ImGuiCol_Separator, ImVec4{ 0.7f, 0.7f, 0.7f, 0.7f } );
			ImGui::Separator();
			ImGui::PopStyleColor();

			ImGui::BeginVertical( "##MainSettings" );

			ImGui::Spring();

			ImGui::BeginHorizontal( "##MSAA_Horiz" );

			ImGui::Text( "Default Editor MSAA Samples:" );
			ImGui::Spring();

			// TODO: Come back to this.

			constexpr const char* items[] = { "0x", "1x", "2x", "4x", "8x", "16x", "32x", "64x" };
			static VkSampleCountFlagBits count;
			if( ImGui::BeginCombo( "##samples", "", ImGuiComboFlags_NoPreview ) )
			{
				auto maxUsable = VulkanContext::Get().GetMaxUsableMSAASamples();

				for( int i = 1; i < IM_ARRAYSIZE( items ); i++ )
				{
					if( i > maxUsable )
						break;

					if( ImGui::Selectable( items[ i ] ) )
					{

					}
				}

				ImGui::EndCombo();
			}

			ImGui::Button( "Test" );

			ImGui::EndHorizontal();

			ImGui::Spring();

			ImGui::EndVertical();

			ImGui::End();
		}
	}

	void EditorLayer::DrawMaterials()
	{
		Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();

		if( hierarchyPanel->GetSelectionContexts().size() > 0 )
		{
			Ref<Entity> rSelection = hierarchyPanel->GetSelectionContext();

			if( rSelection->HasComponent<StaticMeshComponent>() )
			{
				if( auto& mesh = rSelection->GetComponent<StaticMeshComponent>().Mesh )
				{
					ImGui::TextDisabled( "%llx", rSelection->GetComponent<IdComponent>().ID );

					ImGui::Separator();

					for( auto& rMaterial : mesh->GetMaterialAssets() )
					{
						if( ImGui::CollapsingHeader( rMaterial->GetName().c_str() ) )
						{
							ImGui::PushID( static_cast< int >( rMaterial->GetAssetID() ) );

							ImGui::Text( "Mesh name: %s", mesh->FilePath().c_str() );
							ImGui::Text( "Asset ID: %llu", ( uint64_t ) rMaterial->GetAssetID() );

							ImGui::Separator();

							UUID id = rMaterial->GetAssetID();
							Auxiliary::DrawAssetDragDropTarget<MaterialAsset>( "Change asset", rMaterial->GetName().c_str(), id,
								[rMaterial]( Ref<MaterialAsset> asset ) mutable
								{
									rMaterial->SetMaterial( asset->GetMaterial() );
								} );

							ImGui::Separator();

							auto drawItemValue = [&]( const char* name, const char* property )
							{
								ImGui::Text( name );

								ImGui::Separator();

								float v = rMaterial->Get< float >( property );

								ImGui::PushID( name );

								ImGui::DragFloat( "##drgflt", &v, 0.01f, 0.0f, 10000.0f );

								ImGui::PopID();

								if( v != rMaterial->Get<float>( property ) )
									rMaterial->Set( property, v );
							};

							auto displayItemMap = [&]( const char* property )
							{
								Ref< Texture2D > v = rMaterial->GetResource( property );

								if( v && v->GetDescriptorSet() )
									ImGui::Image( v->GetDescriptorSet(), ImVec2( 100, 100 ) );
								else
									ImGui::Image( m_CheckerboardTexture->GetDescriptorSet(), ImVec2( 100, 100 ) );
							};

							ImGui::Text( "Albedo" );

							ImGui::Separator();

							displayItemMap( "u_AlbedoTexture" );

							ImGui::SameLine();

							if( ImGui::Button( "...##opentexture", ImVec2( 50, 20 ) ) )
							{
								std::string file = Application::Get().OpenFile( "Texture File (*.png *.tga)\0*.tga; *.png\0" );

								if( !file.empty() )
								{
									rMaterial->SetResource( "u_AlbedoTexture", Ref<Texture2D>::Create( file, AddressingMode::Repeat ) );
								}
							}

							glm::vec3 color = rMaterial->Get<glm::vec3>( "u_Materials.AlbedoColor" );

							bool changed = ImGui::ColorEdit3( "##Albedo Color", glm::value_ptr( color ), ImGuiColorEditFlags_NoInputs );

							if( changed )
								rMaterial->Set<glm::vec3>( "u_Materials.AlbedoColor", color );

							drawItemValue( "Emissive", "u_Materials.Emissive" );

							ImGui::Text( "Normal" );

							ImGui::Separator();

							bool UseNormalMap = rMaterial->Get< float >( "u_Materials.UseNormalMap" );

							if( UseNormalMap )
								displayItemMap( "u_NormalTexture" );

							if( ImGui::Checkbox( "Use Normal Map", &UseNormalMap ) )
								rMaterial->Set( "u_Materials.UseNormalMap", UseNormalMap ? 1.0f : 0.0f );

							// Roughness Value
							drawItemValue( "Roughness", "u_Materials.Roughness" );

							// Roughness map
							displayItemMap( "u_RoughnessTexture" );

							// Metalness value
							drawItemValue( "Metalness", "u_Materials.Metalness" );

							// Metalness map
							displayItemMap( "u_MetallicTexture" );

							ImGui::PopID();
						}
					}
				}
			}
		}
	}

	void EditorLayer::DrawVFSDebug()
	{
		VirtualFS& rVirtualFS = VirtualFS::Get();

		ImGui::Begin( "Virtual File system" );

		if( Auxiliary::TreeNode( "VFS Info", false ) )
		{
			ImGui::Text( "Mount Bases: %i", rVirtualFS.GetMountBases() );
			ImGui::Text( "Mounts: %i", rVirtualFS.GetMounts() );
		
			Auxiliary::EndTreeNode();
		}

		rVirtualFS.ImGuiRender();

		ImGui::End();
	}

	void EditorLayer::DrawViewport()
	{
		// Viewport Image & Drag and drop handling
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

		ImGui::Begin( "Viewport", 0, flags );

		if( m_ViewportSize != ImGui::GetContentRegionAvail() )
		{
			m_ViewportSize = ImGui::GetContentRegionAvail();

			Application::Get().PrimarySceneRenderer().SetViewportSize( ( uint32_t ) m_ViewportSize.x, ( uint32_t ) m_ViewportSize.y );
			Renderer2D::Get().SetViewportSize( ( uint32_t ) m_ViewportSize.x, ( uint32_t ) m_ViewportSize.y );
			m_EditorCamera.SetViewportSize( ( uint32_t ) m_ViewportSize.x, ( uint32_t ) m_ViewportSize.y );
		}

		ImGui::PushID( "VIEWPORT_IMAGE" );

		// In the editor we only should flip the image UV, we don't have to flip anything else.
		Auxiliary::Image( Application::Get().PrimarySceneRenderer().CompositeImage(), m_ViewportSize, { 0, 1 }, { 1, 0 } );

		if( ImGui::BeginDragDropTarget() )
		{
			if( auto payload = ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_ITEM_SCENE" ) )
			{
				const wchar_t* path = ( const wchar_t* ) payload->Data;
				
				std::filesystem::path p = path;
				Ref<Asset> asset = AssetManager::Get().FindAsset( p );

				OpenFile( asset->ID );
			}

			if( auto payload = ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_ITEM_PREFAB" ) )
			{
				const wchar_t* path = ( const wchar_t* ) payload->Data;

				std::filesystem::path p = path;

				Ref<Asset> asset = AssetManager::Get().FindAsset( p );
				// Make sure to load the prefab.
				Ref<Prefab> prefabAsset = AssetManager::Get().GetAssetAs<Prefab>( asset->GetAssetID() );

				m_EditorScene->CreatePrefab( prefabAsset );
			}

			if( auto payload = ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_ITEM_MODEL" ) )
			{
				const wchar_t* path = ( const wchar_t* ) payload->Data;

				std::filesystem::path p = path;

				// We have now that path to the *.stmesh but we need to path to the fbx/gltf.

				Ref<Asset> asset = AssetManager::Get().FindAsset( p );
				Ref<StaticMesh> meshAsset = AssetManager::Get().GetAssetAs<StaticMesh>( asset->GetAssetID() );

				Ref<Entity> entity = Ref<Entity>::Create();
				entity->SetName( asset->Name );

				entity->AddComponent<StaticMeshComponent>().Mesh = meshAsset;
				entity->AddComponent<StaticMeshComponent>().MaterialRegistry = Ref<MaterialRegistry>::Create( meshAsset );
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();

		// Viewport Gizmo controls
		Viewport_Gizmo();

		// Viewport Runtime controls
		Viewport_RTControls();

		//// Render the real gizmo

		ImVec2 minBound = ImGui::GetWindowPos();
		ImVec2 maxBound = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_MouseOverViewport = ImGui::IsWindowHovered();

		m_AllowCameraEvents = ImGui::IsMouseHoveringRect( minBound, maxBound ) && m_ViewportFocused || m_StartedRightClickInViewport;

		Ref<Scene> ActiveScene = m_RuntimeScene ? m_RuntimeScene : m_EditorScene;

		Ref<SceneHierarchyPanel> hierarchyPanel = m_PanelManager->GetPanel<SceneHierarchyPanel>();
		std::vector<Ref<Entity>>& selectedEntities = hierarchyPanel->GetSelectionContexts();

		// Calc center of transform.
		glm::vec3 Positions = {};
		glm::quat Rotations = {};
		glm::vec3 Scales = {};

		for( const auto& rEntity : selectedEntities )
		{
			TransformComponent worldSpace = ActiveScene->GetWorldSpaceTransform( rEntity );
			Positions += worldSpace.Position;
			Rotations += worldSpace.GetRotation();
			Scales += worldSpace.Scale;
		}

		Positions /= selectedEntities.size();
		Rotations /= selectedEntities.size();
		Scales /= selectedEntities.size();

		glm::mat4 centerPoint = glm::translate( glm::mat4( 1.0f ), Positions ) * glm::toMat4( Rotations ) * glm::scale( glm::mat4( 1.0f ), Scales );
		glm::mat4 offsetTransform( 1.0f );

		///////////////////

		if( selectedEntities.size() )
		{
			ImGuizmo::SetOrthographic( false );
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect( minBound.x, minBound.y, m_ViewportSize.x, m_ViewportSize.y );

			const glm::mat4 Projection = m_EditorCamera.ProjectionMatrix();
			const glm::mat4 View = m_EditorCamera.ViewMatrix();

			ImGuizmo::Manipulate( glm::value_ptr( View ), glm::value_ptr( Projection ), ( ImGuizmo::OPERATION ) m_GizmoOperation, ImGuizmo::LOCAL, glm::value_ptr( centerPoint ), glm::value_ptr( offsetTransform ) );

			if( ImGuizmo::IsUsing() )
			{
				for( Ref<Entity>& entity : selectedEntities )
				{
					glm::mat4 transform = ActiveScene->GetTransformRelativeToParent( entity );
					auto& tc = entity->GetComponent<TransformComponent>();

					glm::vec3 translation;
					glm::vec3 rotation;
					glm::vec3 scale;

					Math::DecomposeTransform( transform * offsetTransform, translation, rotation, scale );

					glm::vec3 DeltaRotation = rotation - tc.GetRotationEuler();

					tc.Position = translation;
					tc.SetRotation( tc.GetRotationEuler() += DeltaRotation );
					tc.Scale = scale;
				}
			}
		}

		ImGui::PopStyleVar();
		ImGui::End();
	}

	void EditorLayer::Viewport_Gizmo()
	{
		ImVec2 minBound = ImGui::GetWindowPos();
		ImVec2 maxBound = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

		// Viewport Gizmo toolbar
		ImGui::PushID( "VP_GIZMO" );

		constexpr float windowHeight = 32.0f;
		constexpr float icons = 3.0f;
		constexpr float neededSpace = 48.0f * icons - 10.0f;

		// For 4 icons
		//const float windowWidth = 166.0f;

		// For 3 icons
		// Formula is 24 * x - 10.0f (for item spacing)
		// Where x is number of icons
		constexpr float windowWidth = neededSpace - 10.0f;

		ImGui::SetNextWindowPos( ImVec2( minBound.x + 5.0f, minBound.y + 5.0f ) );
		ImGui::SetNextWindowSize( ImVec2( windowWidth, windowHeight ) );
		ImGui::Begin( "##viewport_tools", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking );

		ImGui::BeginVertical( "##v_gizmoV", { windowWidth, ImGui::GetContentRegionAvail().y } );
		ImGui::BeginHorizontal( "##v_gizmoH", { windowWidth, ImGui::GetContentRegionAvail().y } );

		ImGui::PushStyleColor( ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f } );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 5.0f * 2.0f, 0 ) );

		if( Auxiliary::ImageButton( m_TranslationTexture, { 24.0f, 24.0f } ) ) m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
		if( Auxiliary::ImageButton( m_RotationTexture, { 24.0f, 24.0f } ) ) m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
		if( Auxiliary::ImageButton( m_ScaleTexture, { 24.0f, 24.0f } ) ) m_GizmoOperation = ImGuizmo::OPERATION::SCALE;

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		ImGui::Spring();
		ImGui::EndHorizontal();
		ImGui::Spring();
		ImGui::EndVertical();

		ImGui::End();

		ImGui::PopID();
	}

	void EditorLayer::Viewport_RTControls()
	{
		ImVec2 minBound = ImGui::GetWindowPos();
		ImVec2 maxBound = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

		constexpr float windowHeight = 32.0f;
		constexpr float icons = 1.0f;
		constexpr float neededSpace = 48.0f * icons - 10.0f;
		constexpr float windowWidth = neededSpace - 10.0f;

		float runtimeCenterX = minBound.x + m_ViewportSize.x * 0.5f - windowWidth * 0.5f;

		// Runtime Controls
		ImGui::SetNextWindowPos( ImVec2( runtimeCenterX, minBound.y + 5.0f ) );
		ImGui::SetNextWindowSize( ImVec2( windowWidth, windowHeight ) );

		ImGui::Begin( "##viewport_center_rt", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking );

		ImGui::BeginVertical( "##centerRTv", { windowWidth, ImGui::GetContentRegionAvail().y } );
		ImGui::BeginHorizontal( "##centerRTh", { windowWidth, ImGui::GetContentRegionAvail().y } );

		ImGui::PushStyleColor( ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f } );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 5.0f * 2.0f, 0 ) );

		Ref<Texture2D> texture = m_RequestRuntime ? m_EndRuntimeTexture : m_StartRuntimeTexture;

		if( Auxiliary::ImageButton( texture, ImVec2( 24.0f, 24.0f ) ) ) 
			m_RequestRuntime ^= 1;

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		ImGui::Spring();
		ImGui::EndHorizontal();
		ImGui::Spring();
		ImGui::EndVertical();

		ImGui::End();
	}

	void EditorLayer::CloseEditorAndOpenPB()
	{
		SaveProject();
		SaveFile();

#if defined(SAT_WINDOWS)
		STARTUPINFOA StartupInfo = {};
		StartupInfo.cb = sizeof( StartupInfo );
		
		PROCESS_INFORMATION ProcessInfo;

		if( Auxiliary::HasEnvironmentVariable( "SATURN_DIR" ) )
		{
			std::string SaturnDir = Auxiliary::GetEnvironmentVariable( "SATURN_DIR" );
			std::string WorkingDir = SaturnDir + "/ProjectBrowser";

#if defined( SAT_DEBUG )
			SaturnDir += "/bin/Debug-windows-x86_64/ProjectBrowser/ProjectBrowser.exe";
#else
			SaturnDir += "/bin/Release-windows-x86_64/ProjectBrowser/ProjectBrowser.exe";
#endif

			bool res = CreateProcessA( nullptr, SaturnDir.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, WorkingDir.data(), &StartupInfo, &ProcessInfo );

			CloseHandle( ProcessInfo.hThread );
			CloseHandle( ProcessInfo.hProcess );
		}
#endif

		Application::Get().Close();
	}

	void EditorLayer::ShowMessageBox()
	{
		ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );
		if( ImGui::BeginPopupModal( "Error##MsgBox", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove ) )
		{
			ImGui::BeginHorizontal( "##MsgBoxH" );

			Auxiliary::Image( m_ExclamationTexture, ImVec2( 72, 72 ) );

			ImGui::Text( m_MessageBoxText.c_str() );

			ImGui::EndHorizontal();

			ImGui::BeginHorizontal( "##MsgBoxOpts" );

			if( ImGui::Button( "OK" ) ) 
			{
				ImGui::CloseCurrentPopup();
				m_ShowMessageBox = false;
			} 
			
			ImGui::EndHorizontal();

			ImGui::EndPopup();
		}

		ImGui::OpenPopup( "Error##MsgBox" );
	}

	void EditorLayer::CheckMissingEnv()
	{
		if( !HasPremakePath )
		{
			if( ImGui::BeginPopupModal( "Missing Environment Variable", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
			{
				ImGui::Text( "The environment variable SATURN_PREMAKE_PATH is not set." );
				ImGui::Text( "This is required in order to build projects." );

				ImGui::Separator();

				static std::string path = "";

				ImGui::InputText( "##path", ( char* ) path.c_str(), 1024, ImGuiInputTextFlags_ReadOnly );
				ImGui::SameLine();
				if( ImGui::Button( "..." ) )
				{
					path = Application::Get().OpenFile( ".exe\0*.exe;\0" );
				}

				if( !path.empty() )
				{
					if( ImGui::Button( "Close" ) )
					{
						ImGui::CloseCurrentPopup();

						Auxiliary::SetEnvironmentVariable( "SATURN_PREMAKE_PATH", path.c_str() );

						HasPremakePath = true;
					}
				}

				ImGui::EndPopup();
			}

			ImGui::OpenPopup( "Missing Environment Variable" );
		}
	}

	void EditorLayer::BuildShaderBundle()
	{
		// Make sure we include the Texture Pass shader.
		// Texture Pass shader is only ever loaded in Dist and we are not on Dist at this point.
		Ref<Shader> TexturePass = ShaderLibrary::Get().FindOrLoad( "TexturePass", "content/shaders/TexturePass.glsl" );

		if( auto shaderRes = ShaderBundle::BundleShaders(); shaderRes != ShaderBundleResult::Success )
		{
			m_MessageBoxText = std::format( "Shader bundle failed to build error was: {0}", ( int ) shaderRes );
			m_ShowMessageBox = true;
		}

		Application::Get().GetWindow()->FlashAttention();

		ShaderLibrary::Get().Remove( TexturePass );
		TexturePass = nullptr;
	}

}