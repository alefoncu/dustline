using UnrealBuildTool;
public class Dustline : ModuleRules
{
	public Dustline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Landscape", "ProceduralMeshComponent", "Slate", "SlateCore", "Json", "JsonUtilities", "AudioMixer" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "LandscapeEditor", "MeshDescription", "StaticMeshDescription", "AssetRegistry" });
		}
	}
}
