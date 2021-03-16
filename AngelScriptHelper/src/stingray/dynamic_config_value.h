#pragma once
/*
#include "../error/assert.h"
#include "../collection/string.h"
#include "../collection/vector.h"
#include "../collection/array.h"
#include "../collection/map.h"
#include "../collection/id_string.h"
*/

#include <assert.h>
#include <vector>
#include <string>
#include <map>

namespace stingray {

namespace sjson {struct ErrorState;}

enum class ReferenceFormat
{
	BOTH,		///< Allow both string references and the new table format.
	TABLE 		///< Only allow the new table reference format.
};

/// Represents a "dynamic" config data item, i.e. once that can be modified during
/// runtime. Dynamic config data items uses regular maps() and vectors() to store
/// the data, rather than storing everything in a single memory block. They are
/// therefore considerably less efficient than the const config data items. For that
/// reason, you should use const config data items wherever possible.
class DynamicConfigValue
{
public:
	typedef sjson::ErrorState ErrorState;

	struct Resource
	{
		const char *type;
		const char *name;
	};

	DynamicConfigValue() : _type(NIL), _tag(0), _data(nullptr), _array(nullptr), _object(nullptr){}
	~DynamicConfigValue();

	DynamicConfigValue(const DynamicConfigValue &o);
	DynamicConfigValue(DynamicConfigValue &&o);
	void operator=(const DynamicConfigValue &o);
	void operator=(DynamicConfigValue &&o);

	/// Tagging

	typedef unsigned Tag;

	static const unsigned TYPE_BITS = 3;
	static const unsigned TAG_BITS = 29;
	static const unsigned TAG_MASK = (1 << TAG_BITS) - 1;

	Tag tag() const { return _tag; }
	void set_tag(Tag tag) { _tag = tag & TAG_MASK; }

	/// Queries

	bool is_nil() const 	{return _type == NIL;}
	bool is_some() const    {return !is_nil();}
	bool is_bool() const 	{return _type == BOOL;}
	bool is_false() const 	{return _type == BOOL && !_bool;}
	bool is_true() const 	{return _type == BOOL && _bool;}
	bool is_integer() const {return _type == INTEGER;}
	bool is_float() const 	{return _type == FLOAT;}
	bool is_number() const	{return _type == FLOAT || _type == INTEGER;}
	bool is_string() const 	{return _type == STRING;}
	bool is_resource(ReferenceFormat rf = ReferenceFormat::BOTH) const;
	bool is_resource(const char *type, ReferenceFormat rf = ReferenceFormat::BOTH) const;
	bool is_data() const	{return _type == DATA;}
	bool is_array() const 	{return _type == ARRAY;}
	bool is_object() const 	{return _type == OBJECT;}

	/// Unsafe conversion functions. These will crash the engine if the object is not of the right type.
	/// You should only use these when you KNOW that the object is of the right type.
	bool to_bool_unsafe() const					{assert(is_bool()); 	return _bool;}
	int to_integer_unsafe() const				{assert(is_integer()); return _integer;}
	float to_float_unsafe()  const				{						return is_float() ? _float : float(to_integer_unsafe());}
	const char *to_string_unsafe() const		{assert(is_string()); 	return _data->data();}
	const char *to_string_unsafe(unsigned& sz) const { assert(is_string()); return sz = _data->size() - 1, _data->data(); }
	const std::vector<char> &to_string_data_unsafe() const { assert(is_string()); return *_data; }
	const char *to_resource_unsafe(const char *type, ReferenceFormat rf = ReferenceFormat::BOTH) const;
	const std::vector<char> &to_data_unsafe() const	{ assert(is_data()); 	return *_data; }

	/// Safe conversion functions, these will set err=true if the object is not of the right type.
	/// This error must be handled.
	bool to_bool(bool &e) const					{if (is_bool()) return _bool;				e=true; return false;}
	int to_integer(bool &e) const				{if (is_integer()) return _integer;			e=true; return 0;}
	float to_float(bool &e)  const				{if (is_float()) return _float;				return float(to_integer(e));}
	const char *to_string(bool &e) const		{if (is_string()) return _data->data();	e=true; return "";}
	const char *to_resource(const char *type, bool &e, ReferenceFormat rf = ReferenceFormat::BOTH) const;
	const std::vector<char> *to_data() const {
		if (is_data()) 
			return _data;
		
		return nullptr;
	}

	#ifdef CAN_COMPILE
		/// Conversion functions that set an error message in the ErrorState object on error.
		bool to_bool(ErrorState &e) const
		{
				if (is_bool()) return _bool;
				set_error(e, "Expected a bool");
				return false;
		}
		int to_integer(ErrorState &e) const
		{
			if (is_integer()) return _integer;
			set_error(e, "Expected an integer");
			return 0;
		}
		float to_float(ErrorState &e)  const
		{
			if (is_float()) return _float;
			if (is_integer()) return float(_integer);
			set_error(e, "Expected a float");
			return 0.0f;
		}
		const char *to_string(ErrorState &e) const
		{
			if (is_string()) return _data->begin();
			set_error(e, "Expected a string");
			return "expected a string";
		}
		ConstString to_const_string(ErrorState &e) const 	{return ConstString(to_string(e));}
		IdString32 to_idstring32(ErrorState &e) const		{return IdString32(to_string(e));}
		IdString64 to_idstring64(ErrorState &e) const		{return IdString64(to_string(e));}
		const char *to_resource(const char *type, ErrorState &e, ReferenceFormat rf = ReferenceFormat::BOTH) const;
		Resource to_resource(Allocator &ta, ErrorState &e, ReferenceFormat rf = ReferenceFormat::BOTH) const;

		/// Conversion functions that access a sub key and set the appropriate error message.
		bool get_bool(const char *key, ErrorState &e) const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_bool()) return child._bool;
			set_error(e, key, "Expected a bool");
			return false;
		}
		int get_integer(const char *key, ErrorState &e) const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_integer()) return child._integer;
			set_error(e, key, "Expected an integer");
			return 0;
		}
		float get_float(const char *key, ErrorState &e)  const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_float()) return child._float;
			if (child.is_integer()) return float(child._integer);
			set_error(e, key, "Expected a float");
			return 0.0f;
		}
		float get_float(unsigned key, ErrorState &e)  const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_float()) return child._float;
			if (child.is_integer()) return float(child._integer);
			set_error(e, key, "Expected a float");
			return 0.0f;
		}
		const char *get_string(const char *key, ErrorState &e) const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_string()) return child._data->begin();
			set_error(e, key, "Expected a string");
			return "expected a string";
		}
		ConstString get_const_string(const char *key, ErrorState &e) const 	{return ConstString(get_string(key, e));}
		IdString32 get_idstring32(const char *key, ErrorState &e) const		{return IdString32(get_string(key, e));}
		IdString64 get_idstring64(const char *key, ErrorState &e) const		{return IdString64(get_string(key, e));}
		const char *get_resource(const char *key, const char *type, ErrorState &e, ReferenceFormat rf = ReferenceFormat::BOTH) const;
		Resource get_resource(const char *key, Allocator &ta, ErrorState &e, ReferenceFormat rf = ReferenceFormat::BOTH) const;
		const DynamicConfigValue &get_array(const char *key, ErrorState &e) const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_array()) return child;
			set_error(e, key, "Expected an array");
			return child;
		}
		const DynamicConfigValue &get_object(const char *key, ErrorState &e) const
		{
			const DynamicConfigValue &child = (*this)[key];
			if (child.is_object()) return child;
			set_error(e, key, "Expected an object");
			return child;
		}
	#endif

	bool operator||(bool b) const						{return is_bool() ? to_bool_unsafe() : b;}
	int operator||(int i) const							{return is_integer() ? to_integer_unsafe() : i;}
	unsigned operator||(unsigned i) const				{return is_integer() ? unsigned(to_integer_unsafe()) : i;}
	float operator||(float f) const						{return is_number() ? to_float_unsafe() : f;}
	const char *operator||(const char *s) const			{return is_string() ? to_string_unsafe() : s;}
	const char *to_resource(const char *type, const char *def, ReferenceFormat rf = ReferenceFormat::BOTH) const {return is_resource(type, rf) ? to_resource_unsafe(type, rf) : def;}

	bool equals_string(const char *s) const             {return is_string() && strcmp(to_string_unsafe(), s) == 0;}

	unsigned size() const {return is_array() ?  _array->size() : 0;}
	const DynamicConfigValue &operator[](int i) const {return (*this)[unsigned(i)];}
	const DynamicConfigValue &operator[](unsigned i) const {
		if (!is_array() || i >= size())
			return static_nil();
		else
			return (*_array)[i];
	}

	unsigned num_keys() const {return is_object() ? _object->size() : 0;}
	const DynamicConfigValue &operator[](const char *s) const {
		if (!is_object() || !(_object->find(s) != _object->end()))
			return static_nil();
		else
			return (*_object)[s];
	}
	bool has(const char *s) const {
		return is_object() && (_object->find(s) != _object->end());
	}

	static const DynamicConfigValue& static_nil()
	{
		static DynamicConfigValue static_nil;
		return static_nil;
	}

	typedef std::map<std::string, DynamicConfigValue> Map;

	class iterator {
	public:
		iterator() : _it(nullptr, 0) {}
		iterator(Map::iterator it) : _it(it) {}
		void operator++() {++_it;}
		const char *key() {return _it->first.c_str();}
		DynamicConfigValue &value() {return _it->second;}
		Map::value_type &operator*() { return *_it; }
		bool operator!=(const iterator &it) const {return _it != it._it;}
	private:
		Map::iterator _it;
	};

	class const_iterator {
	public:
		const_iterator() : _it(nullptr, 0) {}
		const_iterator(Map::const_iterator it) : _it(it) {}
		void operator++() {++_it;}
		const char *key() {return _it->first.c_str();}
		const DynamicConfigValue &value() {return _it->second;}
		const Map::value_type &operator*() { return *_it; }
		bool operator!=(const const_iterator &it) const {return _it != it._it;}
	private:
		Map::const_iterator _it;
	};

	const_iterator begin() const;
	const_iterator end() const;

	/// Find all instances of the specified key in this node or its children and add them to found.
	void find(const char *key, std::vector<DynamicConfigValue> &found) const;

	/// Modifications

	void set_nil();
	void set_false();
	void set_true();
	void set_bool(bool b);
	void set_integer(int i);
	void set_float(float f);
	void set_string(const char *s);
	void set_data(void *p, unsigned len);
	void set_empty_array();
	void set_empty_object();
	void remove_key(const char *s);

	DynamicConfigValue &operator[](int i) {return (*this)[unsigned(i)];}
	DynamicConfigValue &operator[](unsigned i);
	unsigned num_keys();
	DynamicConfigValue &operator[](const char *s);

	iterator begin();
	iterator end();

	void operator=(bool b)				{set_bool(b);}
	void operator=(int i)				{set_integer(i);}
	void operator=(float f)				{set_float(f);}
	void operator=(const char *s)		{set_string(s);}

	DynamicConfigValue &push();

private:
	#ifdef CAN_COMPILE
		void set_error(ErrorState &es, const char *msg) const;
		void set_error(ErrorState &es, const char *key, const char *msg) const;
		void set_error(ErrorState &es, unsigned key, const char *msg) const;
	#endif

	enum ValueType {NIL, BOOL, INTEGER, FLOAT, STRING, DATA, ARRAY, OBJECT};

	struct {
		unsigned _type : TYPE_BITS;
		unsigned _tag : TAG_BITS;
	};

	union {
		Map *_object = nullptr;
		std::vector<DynamicConfigValue> *_array;
		std::vector<char> *_data;
		float _float;
		int _integer;
		bool _bool;
	};

	void destroy();
};

} // namespace stingray

#include "dynamic_config_value.inl"
