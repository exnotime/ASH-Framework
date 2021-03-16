#pragma once

namespace AngelScript {
	class asIScriptEngine;
}
namespace stingray {
	namespace asif_dynamic_config {
		bool register_interface(AngelScript::asIScriptEngine* engine);
	}
}