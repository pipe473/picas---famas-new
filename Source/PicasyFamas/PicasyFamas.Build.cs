using UnrealBuildTool;

public class PicasyFamas : ModuleRules
{
	public PicasyFamas(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"NetCore"        // Push Model (MARK_PROPERTY_DIRTY_FROM_NAME)
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// El nucleo de reglas (Source/PicasyFamas/Core) es C++ puro sin dependencias del motor:
		// se compila tambien fuera de Unreal para tests rapidos con clang/gcc.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
