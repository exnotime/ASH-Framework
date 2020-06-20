#pragma once
namespace AngelScript {
	class asIScriptEngine;
	class asIScriptModule;
}
namespace AngelScriptExporter {
	void ExportEngineAsJSON(const char* file, AngelScript::asIScriptEngine* engine);
	void ImportEngineFromJson(const char* file, AngelScript::asIScriptEngine* engine);
	void ExportModuleAsJSON(const char* file, AngelScript::asIScriptModule* module, AngelScript::asIScriptEngine* engine);
}


