#include "parse_simplified_json.h"

#include <math.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>

#include "dynamic_config_value.h"

#if defined(PS4) || defined(LINUXPC) || defined(_GAMING_XBOX)
	#define omp_get_thread_num()	0
#else
	#include <omp.h>
#endif

namespace {

	inline char* utf8_encode(int c, char* utf8)
	{
		if (c < 0x80) {
			*utf8 = c;
			return utf8 + 1;
		} else if (c < 0x800) {
			utf8[0] = (c >> 6) | 0xC0;
			utf8[1] = (c & 0x3F) | 0x80;
			return utf8 + 2;
		} else if (c < 0x10000) {
			utf8[0] = (c >> 12) | 0xE0;
			utf8[1] = ((c >> 6) & 0x3F) | 0x80;
			utf8[2] = (c & 0x3F) | 0x80;
			return utf8 + 3;
		} else if (c < 0x110000) {
			utf8[0] = (c >> 18) | 0xf0;
			utf8[1] = ((c >> 12) & 0x3F) | 0x80;
			utf8[2] = ((c >> 6) & 0x3F) | 0x80;
			utf8[3] = (c & 0x3F) | 0x80;
			return utf8 + 4;
		} else {
			assert("Cannot encode character");
		}
		return utf8;
	}

	constexpr uint32_t MAX_THREADS = 64;
	jmp_buf _error_jump[MAX_THREADS];	// max number of threads. Set by omp_set_num_threads in data_compiler.cpp
	char _error_message[2048];
	const char *_error_where;

	void handle_parse_error(int line, const char *message, const char *where, ...)
	{
		va_list args;
		va_start(args, where);

		vsnprintf(_error_message, sizeof(_error_message), message, args);
		_error_where = where;
		va_end(args);

		longjmp(_error_jump[omp_get_thread_num()], 1);
	}

	struct Line {int number; const char *start; int length;};
	Line line(const char *s, const char *e, const char *p)
	{
		Line l;
		l.number = 1;
		l.start = s;
		l.length = 0;
		while (s < p) {
			if (*s == '\n') {++l.number; l.start = s+1;}
			++s;
		}
		while (s < e && *s != 0 && *s != '\n')
			++s;
		l.length = (int)(s-l.start);
		return l;
	}

	#define parse_error(m, s, e, ...) handle_parse_error(__LINE__, m, s, ## __VA_ARGS__)
	#define parse_assert(b, m, s, e, ...) do {if (!(b)) parse_error(m, s, e, ## __VA_ARGS__);} while(0)
}

namespace stingray {

typedef DynamicConfigValue Value;

namespace common
{
	void skip_whitespace(const char *&start, const char *end);
	void skip_whitespace_no_error(const char *&start, const char *end);
	void consume(const char *&start, const char *end, char c);
	void skip_bom(const char *&start, const char *end);
	void parse_string(const char *&start, const char *end, Value &string);
	void parse_identifier(const char *&start, const char *end, Value &string);
	void parse_number(const char *&start, const char *end, Value &number);
	void parse_true(const char *&start, const char *end, Value &number);
	void parse_false(const char *&start, const char *end, Value &number);
	void parse_null(const char *&start, const char *end, Value &number);
	void parse_data(const char *&start, const char *end, Value &data);
} // namespace common

namespace parse
{
	void parse_value(const char *&start, const char *end, Value &value);
	void parse_object(const char *&start, const char *end, Value &object);
	void parse_root_object(const char *&start, const char *end, Value &object);
	void parse_array(const char *&start, const char *end, Value &array);

} // namespace parse

namespace {
	/// A callback function passed the start of the parsed source as well as the start and end of the current value the callback function is called for.
	typedef unsigned(*TagDynamicConfigValueFunction)(const char *source, const char *node_start, const char *node_end, void *user_data);
}

#ifdef CAN_COMPILE
	namespace parse_tagged
	{
		struct ParseData {
			const char *start;
			const char *end;
			const char *source;
			TagDynamicConfigValueFunction tag;
			void *user_data;
		};

		void parse_value(ParseData &d, Value &value);
		void parse_object(ParseData &d, Value &object);
		void parse_root_object(ParseData &d, Value &object);
		void parse_array(ParseData &d, Value &array);
	} // namespace parse_tagged
#endif

namespace sjson
{
	const char *generate_error_message(const char *start, const char *end)
	{
		static char buffer[2048] = {0};

		Line line = ::line(start, end, _error_where);
		char text[81] = {0};
		int to_copy = line.length;
		if (to_copy > 80)
			to_copy = 80;
		memcpy(text, line.start, to_copy);

		sprintf(buffer, "Parse error '%s' at line %i:\n\n%s", _error_message, line.number, text);
		return buffer;
	}

	const char *parse(const Buffer b, DynamicConfigValue &value, bool error)
	{
		if (MAX_THREADS < omp_get_thread_num())
			return "MAX_THREADS threads is lower than the current thread number";

		if (setjmp(_error_jump[omp_get_thread_num()])) {
			if (error) {
				return generate_error_message(b.p, b.p + b.len);
			} else {
				return "Error!";
			}
			
		}

		if (b.len == 0)
			return nullptr;

		const char *start = b.p;
		const char *end = b.p + b.len;

		common::skip_bom(start, end);
		parse::parse_root_object(start, end, value);
		return nullptr;
	}

#ifdef CAN_COMPILE
	namespace {stingray::logging::System SIMPLIFIED_JSON_PARSER = {"SimplifiedJsonParser"};}

	/// Overload where the supplied callback @p tag_callback is called for every value in the source.
	/// The first 29 bits of the return value is set as the tag (see DynamicConfigValue::set_tag()/tag()).
	const char *parse(const Buffer b, DynamicConfigValue &value, TagDynamicConfigValueFunction tag_callback, void *user_data)
	{
		if (MAX_THREADS < omp_get_thread_num())
			return "MAX_THREADS threads is lower than the current thread number";

		if (setjmp(_error_jump[omp_get_thread_num()]))
			return generate_error_message(b.p, b.p+b.len);

		if (b.len == 0)
			return nullptr;

		parse_tagged::ParseData d = {b.p, b.p + b.len, b.p, tag_callback, user_data};
		common::skip_bom(d.start, d.end);
		parse_tagged::parse_root_object(d, value);
		return nullptr;
	}


	namespace {
		inline bool is_newline(const char* p) {
			if (p[0] == '\n')
				return true;

			return false;
		}

		inline unsigned columns(char c) {
			if (c == '\r')
				return 0;
			if (c == '\t')
				return sjson::TAB_WIDTH;
			return 1;
		}
	}

	unsigned tag_line_column(const char* source, const char* node_start, const char* node_end, void* user_data)
	{
		return (unsigned)(node_start - source);
	}

	SourceLocation location(const DynamicConfigValue& node, const Buffer source)
	{
		unsigned at = node.tag();
		if (at >= source.len) {
			logging::error(SIMPLIFIED_JSON_PARSER, "Couldn't determine JSON line number, wrong SourceInfo used?");
			SourceLocation loc = {1, 1};
			return loc;
		}

		SourceLocation loc = {1, 1};
		const char* end = source.p + at;
		for (const char *p = source.p; p < end; ++p) {
			loc.column += columns(*p);
			if (is_newline(p)) {
				loc.column = 1;
				++loc.line;
			}
		}
		return loc;
	}

	ErrorState parse_traced(const char *file, Buffer sjson, DynamicConfigValue& result)
	{
		ErrorState es;
		es.file = file;
		es.source = sjson;
		es.error = sjson::parse(sjson, result, tag_line_column, nullptr);
		return es;
	}

	const char *format_error(const ErrorState &es, const DynamicConfigValue &node, const char *msg, ...)
	{
		if (es.error)
			return es.error;

		va_list args;
		va_start(args, msg);
		const char *error = vformat_error(es, node, msg, args);
		va_end(args);

		return error;
	};

	const char *vformat_error(const ErrorState &es, const DynamicConfigValue &node, const char *msg, va_list args)
	{
		if (es.error)
			return es.error;

		static char buffer[2048] = {0};
		sjson::SourceLocation loc = sjson::location(node, es.source);
		int o = snprintf(buffer, 2048, "Failure while parsing `%s`: \n%s(%d:%d): ", es.file, es.file, loc.line, loc.column);
		vsnprintf(buffer+o, 2048-o, msg, args);

		return buffer;
	};

	void add_error(ErrorState &es, const DynamicConfigValue &node, const char *msg, ...)
	{
		if (es.error)
			return;

		va_list args;
		va_start(args, msg);
		es.error = vformat_error(es, node, msg, args);
		va_end(args);
	};
#endif

} // namespace parse_simplified_json

namespace common
{
	inline void set_object_key(const char *start, const char *end, Value& object, const char* key, Value& value)
	{
		parse_assert(!object.has(key), "Object already has key '%s'.", start, end, key);
		object[key] = value;
	}

	void parse_string(const char *&start, const char *end, Value &value)
	{
		std::vector<char> string;

		consume(start, end, '"');
		while (true) {
			parse_assert(start < end, "Reached end of string while parsing", start, end);
			char c = *start;
			++start;
			if (c == '"')
				break;
			else if (c == '\\') {
				c = *start;
				++start;
				if (c == '"' || c == '\\' || c == '/')
					string.push_back(c);
				else if (c == 'b')	string.push_back('\b');
				else if (c == 'f')	string.push_back('\f');
				else if (c == 'n')	string.push_back('\n');
				else if (c == 'r')	string.push_back('\r');
				else if (c == 't')	string.push_back('\t');
				else if (c == 'u')	{
					char hex[5] = {0};
					hex[0] = start[0];
					hex[1] = start[1];
					hex[2] = start[2];
					hex[3] = start[3];
					start += 4;
					int unicode;
					sscanf(hex, "%x", &unicode);
					char utf8[5] = {0};
					utf8_encode(unicode, utf8);
					for (unsigned i = 0; utf8[i] !=0; ++i)
						string.push_back(utf8[i]);
				} else
					parse_error("Unknown escape character", start, end);
			} else {
				string.push_back(c);
			}
		}
		string.push_back(0);

		value.set_string(string.data());
	}

	void parse_data(const char *&start, const char *end, Value &value) {
		consume(start, end, '"');
		consume(start, end, '"');
		consume(start, end, '"');

		std::vector<char> string;

		while (true) {
			parse_assert(start+2 < end, "Reached end of data while parsing", start, end);

			if (start[0]=='"' && start[1]=='"' && start[2]=='"' && !(start+3<end && start[3] == '"')) {
				consume(start, end, '"');
				consume(start, end, '"');
				consume(start, end, '"');
				break;
			}
			string.push_back(*start);
			++start;
		}
		string.push_back(0);
		value.set_string(string.data());
	}

	void parse_identifier(const char *&start, const char *end, Value &value)
	{
		if (*start == '"') {
			parse_string(start, end, value);
			return;
		}

		std::vector<char> string;

		while (true) {
			parse_assert(start < end, "Reached end of string while parsing", start, end);
			char c = *start;
			if (c == ' ' || c == '\t' || c == '\n' || c == '=' || c == ':')
				break;
			++start;
			string.push_back(c);
		}
		string.push_back(0);

		value.set_string(string.data());
	}


	void parse_number(const char *&start, const char *end, Value &number)
	{
		bool negative = false;
		int integer = 0;
		float fraction = 0;
		bool exponent_negative = false;
		int exponent = 0;

		parse_assert(start < end, "Reached end of string while parsing", start, end);
		if (*start == '-') {
			negative = true;
			++start;
		}
		parse_assert(start < end, "Reached end of string while parsing", start, end);
		if (*start == '0') {
			integer = 0;
			++start;
		} else {
			parse_assert(*start >= '1' && *start <= '9', "Expected number between 1 and 9", start, end);
			integer = *start - '0';
			++start;
			while (start < end && *start >= '0' && *start <= '9') {
				integer = integer * 10 + (*start - '0');
				++start;
			}
		}
		if (negative)
			integer = -integer;

		if (start < end && *start == '.') {
			++start;
			int divisor = 10;
			parse_assert(start < end && *start >= '0' && *start <= '9', "Expected number between 1 and 9", start, end );
			while (start < end && *start >= '0' && *start <= '9') {
				fraction += float(*start - '0') / float(divisor);
				++start;
				divisor *= 10;
			}
			if (negative)
				fraction = -fraction;
		}
		if (start < end && (*start == 'e' || *start == 'E')) {
			++start;
			if (start < end && *start == '-') {
				++start;
				exponent_negative = true;
			} else if (start < end && *start == '+') {
				++start;
			}
			parse_assert(*start >= '0' && *start <= '9', "Expected number between 1 and 9", start, end );
			exponent = *start - '0';
			++start;
			while (start < end && *start >= '0' && *start <= '9') {
				exponent = exponent * 10 + (*start - '0');
				++start;
			}
			if (exponent_negative)
				exponent = -exponent;
		}

		if (fraction == 0 && exponent == 0)
			number.set_integer(integer);
		else
			number.set_float( (float(integer) + fraction) * powf(10.0f, float(exponent) ) );
	}

	void parse_true(const char *&start, const char *end, Value &value) {
		value.set_true();
		consume(start, end, 't');
		consume(start, end, 'r');
		consume(start, end, 'u');
		consume(start, end, 'e');
	}

	void parse_false(const char *&start, const char *end, Value &value)
	{
		value.set_false();
		consume(start, end, 'f');
		consume(start, end, 'a');
		consume(start, end, 'l');
		consume(start, end, 's');
		consume(start, end, 'e');
	}

	void parse_null(const char *&start, const char *end, Value &value)
	{
		value.set_nil();
		consume(start, end, 'n');
		consume(start, end, 'u');
		consume(start, end, 'l');
		consume(start, end, 'l');
	}

	void skip_bom(const char *&start, const char *end)
	{
		if (end - start < 3)
			return;

		if (start[0] == (char)0xEF
			&& start[1] == (char)0xBB
			&& start[2] == (char)0xBF)
		{
			start += 3;
		}
	}

	void consume(const char *&start, const char *end, char c)
	{
		parse_assert(start < end, "Reached end while looking for character %c", start, end, c);
		parse_assert(*start == c, "Character %c did not match expectation %c", start, end, *start, c);
		++start;
	}

	void skip_comment(const char *&start, const char *end)
	{
		if (start[1] == '/') {
			while (start+1 < end && *start != '\n')
				++start;
			++start;
		} else if (start[1] == '*') {
			while (start+2 < end && (start[0] != '*' || start[1] != '/'))
				++start;
			start += 2;
		} else
			parse_error("Bad comment", start, end);
	}

	void skip_whitespace(const char *&start, const char *end)
	{
		while (start < end) {
			const char start_char = *start;
			if (start_char == '/')
				skip_comment(start, end);
			else if (start_char == ' ' || start_char == '\t' || start_char == '\n' || start_char == '\r' || start_char == ',')
				++start;
			else
				break;
		}
	}

	void skip_whitespace_no_error(const char *&start, const char *end)
	{
		while (start < end) {
			const char start_char = *start;
			if (start_char == '/')
				skip_comment(start, end);
			else if (start_char == ' ' || start_char == '\t' || start_char == '\n' || start_char == '\r' || start_char == ',')
				++start;
			else
				break;
		}
	}
}

namespace parse
{
	using namespace common;
	void parse_value(const char *&start, const char *end, Value &value)
	{
		skip_whitespace(start, end);

		char c = *start;
		if (c == '{')
			parse_object(start, end, value);
		else if (c == '[')
			parse_array(start, end, value);
		else if (c == '"') {
			if (start+2 < end && (start[1] == '"' && start[2] == '"'))
				parse_data(start, end, value);
			else
				parse_string(start, end, value);
		} else if (c == '-' || (c >= '0' && c <= '9'))
			parse_number(start, end, value);
		else if (c == 't')
			parse_true(start, end, value);
		else if (c == 'f')
			parse_false(start, end, value);
		else if (c == 'n')
			parse_null(start, end, value);
		else
			parse_error("Unexpected character", start, end);
	}

	void parse_object(const char *&start, const char *end, Value &object)
	{
		object.set_empty_object();
		consume(start, end, '{');
		skip_whitespace(start, end);
		if (*start == '}') {
			consume(start, end, '}');
			return;
		}

		while (true) {
			Value key;
			Value value;
			parse_identifier(start, end, key);
			skip_whitespace(start, end);
			if (*start == ':')
				consume(start, end, ':');
			else
				consume(start, end, '=');
			skip_whitespace(start, end);
			parse_value(start, end, value);
			set_object_key(start, end, object, key.to_string_unsafe(), value);
			skip_whitespace(start, end);
			if (*start == '}')
				break;
		}
		consume(start, end, '}');
	}

	void parse_root_object(const char *&start, const char *end, Value &object)
	{
		skip_whitespace_no_error(start, end);
		if (*start  == '{') {
			parse_object(start, end, object);
			return;
		}

		if (start == end)
			return;

		object.set_empty_object();
		while (true) {
			Value key;
			Value value;
			parse_identifier(start, end, key);
			skip_whitespace(start, end);
			if (*start == ':')
				consume(start, end, ':');
			else
				consume(start, end, '=');
			skip_whitespace(start, end);
			parse_value(start, end, value);
			set_object_key(start, end, object, key.to_string_unsafe(), value);
			skip_whitespace_no_error(start, end);
			if (start == end)
				return;
		}
	}

	void parse_array(const char *&start, const char *end, Value &array)
	{
		array.set_empty_array();
		consume(start, end, '[');
		skip_whitespace(start, end);
		if (*start == ']') {
			consume(start, end, ']');
			return;
		}

		while (true) {
			Value data;
			parse_value(start, end, data);
			skip_whitespace(start, end);
			array.push() = data;
			skip_whitespace(start, end);
			if (*start == ']')
				break;
			skip_whitespace(start, end);
		}
		consume(start, end, ']');
	}
} // namespace parse

#ifdef CAN_COMPILE
namespace parse_tagged
{
	using namespace common;
	void parse_array(ParseData &d, Value &array)
	{
		array.set_empty_array();
		consume(d.start, d.end, '[');
		skip_whitespace(d.start, d.end);
		if (*d.start == ']') {
			consume(d.start, d.end, ']');
			return;
		}

		while (true) {
			Value data(*array.allocator());
			parse_value(d, data);
			skip_whitespace(d.start, d.end);
			array.push() = data;
			skip_whitespace(d.start, d.end);
			if (*d.start == ']')
				break;
			skip_whitespace(d.start, d.end);
		}
		consume(d.start, d.end, ']');
	}

	void parse_value(ParseData &d, Value &value)
	{
		skip_whitespace(d.start, d.end);

		const char *value_start = d.start;
		char c = *d.start;
		if (c == '{')
			parse_object(d, value);
		else if (c == '[')
			parse_array(d, value);
		else if (c == '"') {
			if (d.start+2 < d.end && (d.start[1] == '"' && d.start[2] == '"'))
				parse_data(d.start, d.end, value);
			else
				parse_string(d.start, d.end, value);
		} else if (c == '-' || (c >= '0' && c <= '9'))
			parse_number(d.start, d.end, value);
		else if (c == 't')
			parse_true(d.start, d.end, value);
		else if (c == 'f')
			parse_false(d.start, d.end, value);
		else if (c == 'n')
			parse_null(d.start, d.end, value);
		else
			parse_error("Unexpected character", d.start, d.end);

		value.set_tag(d.tag(d.source, value_start, d.start, d.user_data));
	}

	void parse_object(ParseData &d, Value &object)
	{
		const char *object_start = d.start;
		object.set_empty_object();
		consume(d.start, d.end, '{');
		skip_whitespace(d.start, d.end);
		if (*d.start == '}') {
			consume(d.start, d.end, '}');
			object.set_tag(d.tag(d.source, object_start, d.start, d.user_data));
			return;
		}

		while (true) {
			Value key(*object.allocator());
			Value value(*object.allocator());
			parse_identifier(d.start, d.end, key);
			skip_whitespace(d.start, d.end);
			if (*d.start == ':')
				consume(d.start, d.end, ':');
			else
				consume(d.start, d.end, '=');
			skip_whitespace(d.start, d.end);
			parse_value(d, value);
			set_object_key(d.start, d.end, object, key.to_string_unsafe(), value);
			skip_whitespace(d.start, d.end);
			if (*d.start == '}')
				break;
		}
		consume(d.start, d.end, '}');
	}

	void parse_root_object(ParseData &d, Value &object)
	{
		skip_whitespace_no_error(d.start, d.end);

		const char* object_start = d.start;
		if (*d.start  == '{') {
			parse_object(d, object);
			object.set_tag(d.tag(d.source, object_start, d.start, d.user_data));
			return;
		}

		object.set_empty_object();
		if (d.start == d.end) {
			object.set_tag(d.tag(d.source, object_start, d.start, d.user_data));
			return;
		}

		while (true) {
			Value key(*object.allocator());
			Value value(*object.allocator());
			parse_identifier(d.start, d.end, key);
			skip_whitespace(d.start, d.end);
			if (*d.start == ':')
				consume(d.start, d.end, ':');
			else
				consume(d.start, d.end, '=');
			skip_whitespace(d.start, d.end);
			parse_value(d, value);
			set_object_key(d.start, d.end, object, key.to_string_unsafe(), value);
			skip_whitespace_no_error(d.start, d.end);

			if (d.start == d.end) {
				object.set_tag(d.tag(d.source, object_start, d.start, d.user_data));
				return;
			}
		}
	}
} // namespace parse_tagged
#endif

} // namespace stingray
