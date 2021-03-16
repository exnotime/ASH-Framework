#pragma once

	#pragma once

#include <cstdarg>

namespace stingray {

	struct Buffer
	{
		Buffer() : p(0), len(0) {}
		Buffer(char* p, unsigned len) : p(p), len(len) {}
		Buffer(const char* p, unsigned len) : p(const_cast<char*>(p)), len(len) {}

		char* p;
		unsigned len;
	};

class DynamicConfigValue;

/// Parses simplified JSON data.
namespace sjson
{
	/// Parses a file in the simplified JSON format to a dynamic config value. For example:
	///
	/// firstName = "John"
	/// lastName = "Smith"
	/// adress = {
	/// 	streetAddress = "21 2nd Street"
	/// 	city = "New York"
	/// 	state = "NY"

	/// 	postalCode = "10021"
	/// }
	/// /* list of all phone numbers */
	/// phoneNumbers = [
	/// 	{type = "home" number = "212 555-1234"}
	/// 	{type = "fax" number = "646 555-4567"}
	/// ]
	///
	/// C/C++ style comments can be used in the file. The simplified JSON format is backwards compatible
	/// with regular JSON.
	///
	/// If there is an error with parsing, an error message will be returned. Otherwise the function will
	/// return nullptr.
	const char *parse(Buffer b, DynamicConfigValue &value, bool error = true);

	#ifdef CAN_COMPILE
		/// Used to track errors during parsing.
		struct ErrorState
		{
			ErrorState() : file(nullptr), source(), error(nullptr) {}
			ErrorState(const char *f, Buffer s) : file(f), source(s), error(nullptr) {}
			const char *file;
			Buffer source;
			const char *error;
		};

		/// Parses the SJSON @p sjson with line & columnn info enabled and places the results in @p result.
		/// The location information can be retrived by the location() function.
		/// Returns nullptr on success and an error message on error.
		ErrorState parse_traced(const char *file, Buffer sjson, DynamicConfigValue& result);

		/// The assumed tab-width used when reporting column numbers.
		static const unsigned TAB_WIDTH = 4;

		/// Returns the line and column where the definition of @p node started in the source code @p source.
		/// Note that this may be a costly operation.
		struct SourceLocation {
			unsigned line;
			unsigned column;
		};
		SourceLocation location(const DynamicConfigValue& node, Buffer source);

		/// Formats and returns an error message found while interpreting the JSON document.
		const char *format_error(const ErrorState &es, const DynamicConfigValue &node, const char *msg, ...);
		const char *vformat_error(const ErrorState &es, const DynamicConfigValue &node, const char *msg, va_list args);

		void add_error(ErrorState &es, const DynamicConfigValue &node, const char *msg, ...);

	#endif
}

} // namespace stingray
