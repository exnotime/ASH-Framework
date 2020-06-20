
#include <angelscript.h>
#include <AngelScriptExporter.h>
#include "AngelScript/scriptbuilder/scriptbuilder.h"

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

int main(int argc, char** argv) {
	AppArguments args;

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
		//only export module reflection if the compile succeds
		if (r >= 0 && args.operation & REFLECT_MODULE) {
			AngelScriptExporter::ExportModuleAsJSON(args.output.c_str(), module, engine);
		}
	} else {
		printf("%s\n", error.c_str());
	}
	return 0;
}