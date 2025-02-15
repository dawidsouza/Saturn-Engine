-- Game Project premake template.
-- __PROJECT_NAME__

workspace "__PROJECT_NAME__"
	architecture "x64"
	startproject "__PROJECT_NAME__"
	warnings "Off"

	configurations { "Debug", "Release", "Dist" }

	flags { "MultiProcessorCompile" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}

group "Game"
project "__PROJECT_NAME__"
	kind "Makefile"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"Source/**.cs"
	}

	removefiles 
	{ 
		"**.Gen.cpp", 
		"**.Gen.h" 
	}
	
	filter "system:windows"
		systemversion "latest"

		filter "configurations:Debug"
			runtime "Debug"
			symbols "on"

			buildcommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /BUILD /%{prj.name} /Win64 /Debug /%{prj.location}"
			}

			rebuildcommands 
			{
				"__SATURN_BT_DIR__/RT/Run.bat /REBUILD /%{prj.name} /Win64 /Debug /%{prj.location}"
			}

			cleancommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /CLEAN /%{prj.name} /Win64 /Debug /%{prj.location}"
			}

		filter "configurations:Release"
			runtime "Release"
			optimize "on"

			buildcommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /BUILD /%{prj.name} /Win64 /Release /%{prj.location}"
			}

			rebuildcommands 
			{
				"__SATURN_BT_DIR__/RT/Run.bat /REBUILD /%{prj.name} /Win64 /Release /%{prj.location}"
			}

			cleancommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /CLEAN /%{prj.name} /Win64 /Release /%{prj.location}"
			}

		filter "configurations:Dist"
			runtime "Release"
			symbols "on"

			buildcommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /BUILD /%{prj.name} /Win64 /Dist /%{prj.location}"
			}

			rebuildcommands 
			{
				"__SATURN_BT_DIR__/RT/Run.bat /REBUILD /%{prj.name} /Win64 /Dist /%{prj.location}"
			}

			cleancommands
			{
				"__SATURN_BT_DIR__/RT/Run.bat /CLEAN /%{prj.name} /Win64 /Dist /%{prj.location}"
			}