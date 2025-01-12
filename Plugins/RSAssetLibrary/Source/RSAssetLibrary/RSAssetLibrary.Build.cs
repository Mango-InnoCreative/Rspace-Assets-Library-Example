// Copyright (c) 2024 Hunan MangoXR Tech Co., Ltd. All Rights Reserved.

using UnrealBuildTool;

public class RSAssetLibrary : ModuleRules
{
	public RSAssetLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"CoreUObject", 
				"Engine", 
				"InputCore", 
				"Slate", 
				"SlateCore", 
				"UMG", 
				"RSpaceAssetLibApi",
				"HTTP",
				"ImageWrapper",
				"UserSessionManager",
				"AssetTools",
				"DesktopPlatform",
				"AudioMixer"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore", 
				"RSpaceAssetLibApi",
				"HTTP", 
				"Json", 
				"UserSessionManager", 
				"EditorScriptingUtilities", 
				"MediaAssets", 
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
