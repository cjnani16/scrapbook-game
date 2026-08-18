// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Scrapbook : ModuleRules
{
	public Scrapbook(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"OnlineSubsystem", 
			"OnlineSubsystemUtils", 
			"Steamworks",
            "ApplicationCore",
			"GeometryCore",
            "GeometryAlgorithms",
            "GeometryFramework",
            "ProceduralMeshComponent",
            "DesktopPlatform",
			"DynamicMesh",
            "ModelingComponents",
            "GeometryScriptingCore"
        });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
    }
}


