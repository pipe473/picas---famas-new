using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated Server. Compilar con:
//   <UE>/Engine/Build/BatchFiles/RunUAT BuildCookRun -project=PicasyFamas.uproject -server -serverplatform=Linux -noclient -build -cook
public class PicasyFamasServerTarget : TargetRules
{
	public PicasyFamasServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("PicasyFamas");

		// El servidor no necesita audio ni render. Reduce el binario y el arranque.
		bUseLoggingInShipping = true;
	}
}
