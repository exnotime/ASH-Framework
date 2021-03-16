#include "asif_dynamic_config.h"
#include <angelscript.h>

#include "dynamic_config_value.h"
#include "parse_simplified_json.h"

#include <fstream>
#include <sstream>

using namespace AngelScript;

namespace stingray {

	struct ScriptConfig {
		ScriptConfig(): _ref_counter(0), dcv(){}

		bool parse_file(std::string file) {
			std::ifstream t(file.c_str());
			std::stringstream buffer;
			buffer << t.rdbuf();
			return parse_string(buffer.str());
		}

		bool parse_string(std::string str) {
			Buffer buf;
			buf.len = str.size();
			buf.p = &str[0];
			const char* error = sjson::parse(buf, dcv);
			//TODO: print error
			return error == nullptr;
		}

		bool is_array() { return dcv.is_array(); }
		bool is_bool() { return dcv.is_bool(); }
		bool is_data() { return dcv.is_data(); }
		bool is_float() { return dcv.is_float(); }
		bool is_integer() { return dcv.is_integer(); }
		bool is_nil() { return dcv.is_nil(); }
		bool is_number() { return dcv.is_number(); }
		bool is_object() { return dcv.is_object(); }
		bool is_string() { return dcv.is_string(); }
		bool is_some() { return dcv.is_some(); }

		float to_float() { return dcv.to_float_unsafe(); }
		bool to_bool() { return dcv.to_bool_unsafe(); }
		int to_integer() { return dcv.to_integer_unsafe(); }
		std::string to_string() { 
			return dcv.to_string_unsafe();
		}

		int size() {
			return dcv.size();
		}

		bool has(std::string str) {
			return dcv.has(str.c_str());
		}

		ScriptConfig* index(uint32_t i) {
			ScriptConfig* c = new ScriptConfig();
			c->increment();
			c->dcv = dcv[i];
			return c;
		}

		ScriptConfig* index(std::string s) {
			ScriptConfig* c = new ScriptConfig();
			c->increment();
			c->dcv = dcv[s.c_str()];
			return c;
		}

		ScriptConfig* assign(const ScriptConfig& other) {
			this->dcv = other.dcv;
			return this;
		}

		void increment() {
			_ref_counter++;
		}
		void release() {
			_ref_counter--;
			if (_ref_counter == 0)
				delete this;
		}

		DynamicConfigValue dcv;
		int _ref_counter;
	};

	ScriptConfig* ConstructScriptConfig() {
		return new ScriptConfig();
	}

	namespace asif_dynamic_config {

		bool register_interface(asIScriptEngine* engine) {
			
			int r = engine->RegisterObjectType("Config", sizeof(ScriptConfig), AngelScript::asOBJ_REF);
			r = engine->RegisterObjectBehaviour("Config", asBEHAVE_FACTORY, "Config@ f()", asFUNCTION(ConstructScriptConfig), asCALL_CDECL);
			r = engine->RegisterObjectBehaviour("Config", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptConfig, increment), asCALL_THISCALL);
			r = engine->RegisterObjectBehaviour("Config", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptConfig, release), asCALL_THISCALL);

			r = engine->RegisterObjectMethod("Config", "bool parse_file(string f)", asMETHOD(ScriptConfig, parse_file), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool parse_string(string str)", asMETHOD(ScriptConfig, parse_string), asCALL_THISCALL);

			r = engine->RegisterObjectMethod("Config", "bool is_array()", asMETHOD(ScriptConfig, is_array), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_bool()", asMETHOD(ScriptConfig, is_bool), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_data()", asMETHOD(ScriptConfig, is_data), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_float()", asMETHOD(ScriptConfig, is_float), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_integer()", asMETHOD(ScriptConfig, is_integer), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_nil()", asMETHOD(ScriptConfig, is_nil), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_number()", asMETHOD(ScriptConfig, is_number), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_object()", asMETHOD(ScriptConfig, is_object), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_string()", asMETHOD(ScriptConfig, is_string), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool is_some()", asMETHOD(ScriptConfig, is_some), asCALL_THISCALL);

			r = engine->RegisterObjectMethod("Config", "float to_float()", asMETHOD(ScriptConfig, to_float), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool to_bool()", asMETHOD(ScriptConfig, to_bool), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "int to_integer()", asMETHOD(ScriptConfig, to_integer), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "string to_string()", asMETHOD(ScriptConfig, to_string), asCALL_THISCALL);

			r = engine->RegisterObjectMethod("Config", "int size()", asMETHOD(ScriptConfig, size), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "bool has(string name)", asMETHOD(ScriptConfig, has), asCALL_THISCALL);

			r = engine->RegisterObjectMethod("Config", "Config@ opIndex(uint i)", asMETHODPR(ScriptConfig, index, (uint32_t), ScriptConfig*), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "Config@ opIndex(string str)", asMETHODPR(ScriptConfig, index, (std::string), ScriptConfig*), asCALL_THISCALL);
			r = engine->RegisterObjectMethod("Config", "Config@ opAssign(const Config &in other)", asMETHODPR(ScriptConfig, assign, (const ScriptConfig&), ScriptConfig*), asCALL_THISCALL);
			return true;
		}
	}
}