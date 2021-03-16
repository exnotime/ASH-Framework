
#include <angelscript.h>
#include <AngelScriptExporter.h>
#include "AngelScript/scriptbuilder/scriptbuilder.h"
#include "scriptstdstring.h"
#include "scriptfile.h"
#include "scriptfilesystem.h"
#include "scriptarray.h"

#include "stingray/asif_dynamic_config.h"

#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h> 

#include <fstream>
#include <string>
#include <vector>
#include <assert.h>
#include <Windows.h>

using namespace AngelScript;

enum OPERATION {
	REFLECT_MODULE = 0x1,
	COMPILE_FILE = 0x2,
	PRINT_HELP = 0x4
};

struct AppArguments {
	std::string interfaceFile;
	std::string sourceFile;
	std::vector<std::string> additionalIncludeDirs; //TODO
	std::vector<std::string> defines; //TODO
	std::string includeHandler;
	std::string output; //output path of the module reflection
	uint32_t operation = PRINT_HELP;
};


std::string ParseArgs(int argc, char** argv, AppArguments& args) {
	for (int i = 0; i < argc; ++i) {
		if (strcmp(argv[i], "-r") == 0) {
			args.operation |= REFLECT_MODULE;
			i++;
			args.output = argv[i];
		}
		if (strcmp(argv[i], "-c") == 0) {
			args.operation |= COMPILE_FILE;
		}
		if(strcmp(argv[i], "-i") == 0) {
			i++;
			args.interfaceFile = argv[i];
		}
		if (strcmp(argv[i], "-s") == 0) {
			i++;
			args.sourceFile = argv[i];
		}
		if (strcmp(argv[i], "-ih") == 0) {
			i++;
			args.includeHandler = argv[i];
		}
		if (strcmp(argv[i], "-d") == 0) {
			i++;
			args.defines.push_back(argv[i]);
		}
	}
	if (args.operation & (REFLECT_MODULE | COMPILE_FILE) && args.sourceFile.empty()) {
		return "Missing source file";
	}
	return "";
}

struct Message {
	uint32_t type;
	std::string section;
	uint32_t line;
	uint32_t column;
	std::string message;
};

std::vector<Message> g_Messages;

void MessageCallback(const AngelScript::asSMessageInfo* msg, void* param) {
	Message m;
	m.line = msg->row;
	m.column = msg->col;
	m.type = msg->type;
	m.section = msg->section;
	m.message = msg->message;
	g_Messages.push_back(m);
}

void FormatAndPrintMessages() {
	using namespace rapidjson;

	Document output;
	output.SetObject();
	Value errors = Value(kArrayType);
	Value warnings = Value(kArrayType);
	Value infos = Value(kArrayType);

	for (auto& m : g_Messages) {
		Value message = Value(kObjectType);
		message.AddMember("Line", Value(m.line), output.GetAllocator());
		message.AddMember("Column", Value(m.column), output.GetAllocator());
		message.AddMember("Message", Value(m.message.c_str(), output.GetAllocator()), output.GetAllocator());
		message.AddMember("Section", Value(m.section.c_str(), output.GetAllocator()), output.GetAllocator());
		if (m.type == asEMsgType::asMSGTYPE_ERROR) {
			errors.PushBack(message, output.GetAllocator());
		} else if (m.type == asEMsgType::asMSGTYPE_WARNING) {
			warnings.PushBack(message, output.GetAllocator());
		} else if (m.type == asEMsgType::asMSGTYPE_INFORMATION) {
			infos.PushBack(message, output.GetAllocator());
		}
	}
	output.AddMember("Errors", errors, output.GetAllocator());
	output.AddMember("Warnings", warnings, output.GetAllocator());
	output.AddMember("Infos", infos, output.GetAllocator());

	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	output.Accept(writer);

	printf("%s", buffer.GetString());
}


struct IncludeHandler {
	AngelScript::asIScriptEngine* engine;
	AngelScript::asIScriptContext* ctx;
	AngelScript::asIScriptModule* script;
	AngelScript::asIScriptFunction* func;
};

void print(std::string str) {
	printf("%s\n", str.c_str());
}

void IncludeMessageCallback(const AngelScript::asSMessageInfo* msg, void* param) {
	printf("%s\n", msg->message);
}

std::string GetEnvVariable(std::string s) {
	char buffer[256];
	GetEnvironmentVariableA(s.c_str(), buffer, 256);
	return std::string(buffer);
}

void SetupIncludeHandler(std::string& file, IncludeHandler& ih) {
	ih.engine = asCreateScriptEngine();
	AngelScript::RegisterStdString(ih.engine);
	AngelScript::RegisterScriptArray(ih.engine, true);
	AngelScript::RegisterScriptFile(ih.engine);
	AngelScript::RegisterScriptFileSystem(ih.engine);
	ih.engine->RegisterGlobalFunction("void print(string s)", asFUNCTION(print), asCALL_CDECL);
	ih.engine->RegisterGlobalFunction("string GetEnvVar(string s)", asFUNCTION(GetEnvVariable), asCALL_CDECL);
	stingray::asif_dynamic_config::register_interface(ih.engine);

	CScriptBuilder builder;
	int r = 0;
	r = ih.engine->SetMessageCallback(asFUNCTION(IncludeMessageCallback), 0, asCALL_CDECL);
	r = builder.StartNewModule(ih.engine, file.c_str());
	r = builder.AddSectionFromFile(file.c_str());
	builder.BuildModule();
	ih.script = builder.GetModule();
	AngelScript::asIScriptFunction* func = ih.script->GetFunctionByDecl("string IncludeFile(string includeFile, string fromFile)");
	if (!func) {
		printf("Include handler has to define a function with declaration \"string IncludeFile(string includeFile, string fromFile)\"");
	}
	ih.func = func;
	ih.ctx = ih.engine->CreateContext();
	//run initialize
	AngelScript::asIScriptFunction* initFunc =  ih.script->GetFunctionByDecl("bool InitializeIncluder()");
	ih.ctx->Prepare(initFunc);
	if (ih.ctx->Execute() == AngelScript::asEXECUTION_FINISHED) {

	}
}

int CustomInclude(const char* include, const char* from, CScriptBuilder* builder, void* userParam) {
	IncludeHandler* includer = (IncludeHandler*)userParam;
	includer->ctx->Prepare(includer->func);
	std::string includeFile(include);
	std::string fromFile(from);
	includer->ctx->SetArgObject(0, &includeFile);
	includer->ctx->SetArgObject(1, &fromFile);
	int rCode = includer->ctx->Execute();
	if (rCode == AngelScript::asEXECUTION_FINISHED) {

	} else if (rCode == AngelScript::asEXECUTION_EXCEPTION) {
		//printf exception
		return -1;
	}
	std::string* fileContent = (std::string*)includer->ctx->GetAddressOfReturnValue();
	if (*fileContent != "error") {
		builder->AddSectionFromFile(fileContent->c_str());
	} else {
		return -1;
	}
	
	return 0;
}

int main(int argc, char** argv) {
	AppArguments args;

	//while (!IsDebuggerPresent()){ Sleep(10); }
	std::string error = ParseArgs(argc, argv, args);
	if (error.empty()) {
		//Create Engine
		AngelScript::asIScriptEngine* engine = asCreateScriptEngine();
		int r = 0;
		r = engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
		r = engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, false);
		//load interface
		if (!args.interfaceFile.empty()) {
			AngelScriptExporter::ImportEngineFromJson(args.interfaceFile.c_str(), engine);
		}
		
		//build script
		CScriptBuilder builder;
		IncludeHandler includer;
		if (!args.includeHandler.empty()) {
			SetupIncludeHandler(args.includeHandler, includer);
			builder.SetIncludeCallback(CustomInclude, &includer);
		}
		r = builder.StartNewModule(engine, args.sourceFile.c_str());
		r = builder.AddSectionFromFile(args.sourceFile.c_str());
		
		//TODO: Add defines and include dirs
		//This will compile the file and "print" any errors into the message list
		r = builder.BuildModule();
		asIScriptModule* module = builder.GetModule();
		//Perform operation
		if (args.operation & COMPILE_FILE) {
			FormatAndPrintMessages();
		}
		//only export module reflection if the compile succeeds
		if (r >= 0 && args.operation & REFLECT_MODULE) {
			AngelScriptExporter::ExportModuleAsJSON(args.output.c_str(), module, engine);
		}
	} else {
		printf("%s\n", error.c_str());
	}
	return 0;
}