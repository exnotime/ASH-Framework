namespace stingray {

inline DynamicConfigValue::DynamicConfigValue(const DynamicConfigValue &o)
{
	_type = o._type;
	_tag = o._tag;

	switch (_type) {
		case NIL: break;
		case BOOL: _bool = o._bool; break;
		case INTEGER: _integer = o._integer; break;
		case FLOAT: _float = o._float; break;
		case STRING: case DATA:
			_data = new std::vector<char>();
			*_data = *o._data;
			break;
		case ARRAY:
			_array = new std::vector<DynamicConfigValue>();
			*_array = *o._array;
			break;
		case OBJECT:
			_object = new Map();
			*_object = *o._object;
			break;
	}
}

inline DynamicConfigValue::DynamicConfigValue(DynamicConfigValue &&o)
	: _type(o._type)
	, _tag(o._tag)
{
	switch (_type) {
		case NIL:
			break;
		case BOOL:
			_bool = o._bool;
			break;
		case INTEGER:
			_integer = o._integer;
			break;
		case FLOAT:
			_float = o._float;
			break;
		case STRING:
		case DATA:
			_data = o._data;
			break;
		case ARRAY:
			_array = o._array;
			break;
		case OBJECT:
			_object = o._object;
			break;
	}
	o._type = NIL;
	o._tag = 0;
}

inline void DynamicConfigValue::operator=(const DynamicConfigValue &o)
{
	destroy();

	_type = o._type;
	_tag = o._tag;

	switch (_type) {
		case NIL: break;
		case BOOL: _bool = o._bool; break;
		case INTEGER: _integer = o._integer; break;
		case FLOAT: _float = o._float; break;
		case STRING: case DATA:
			_data = new std::vector<char>();
			*_data = *o._data;
			break;
		case ARRAY:
			_array = new std::vector<DynamicConfigValue>();
			*_array = *o._array;
			break;
		case OBJECT:
			_object = new Map();
			*_object = *o._object;
			break;
	}
}

inline void DynamicConfigValue::operator=(DynamicConfigValue &&o)
{
	destroy();

	_type = o._type;
	_tag = o._tag;

	switch (_type) {
		case NIL:
			break;
		case BOOL:
			_bool = o._bool;
			break;
		case INTEGER:
			_integer = o._integer;
			break;
		case FLOAT:
			_float = o._float;
			break;
		case STRING:
		case DATA:
			_data = o._data;
			break;
		case ARRAY:
			_array = o._array;
			break;
		case OBJECT:
			_object = o._object;
			break;
	}
	o._type = NIL;
	o._tag = 0;
}

inline DynamicConfigValue::~DynamicConfigValue()
{
	destroy();
}

inline void DynamicConfigValue::destroy()
{
	if (_type == STRING || _type == DATA)
		if(_data) delete _data;
	else if (_type == ARRAY)
		if (_array) delete _array;
	else if (_type == OBJECT)
		if (_object) delete _object;
	_type = NIL;
}


inline void DynamicConfigValue::set_nil()
{
	destroy();
}

inline void DynamicConfigValue::set_false()
{
	destroy();
	_type = BOOL;
	_bool = false;
}

inline void DynamicConfigValue::set_true()
{
	destroy();
	_type = BOOL;
	_bool = true;
}

inline void DynamicConfigValue::set_bool(bool b)
{
	destroy();
	_type = BOOL;
	_bool = b;
}

inline void DynamicConfigValue::set_integer(int i)
{
	destroy();
	_type = INTEGER;
	_integer = i;
}

inline void DynamicConfigValue::set_float(float f)
{
	destroy();
	_type = FLOAT;
	_float = f;
}

inline void DynamicConfigValue::set_string(const char *s)
{
	destroy();
	if (s == nullptr) {
		_type = STRING;
		_data = new std::vector<char>();
		(*_data)[0] = '\0';
		return;
	}
	const unsigned string_len = strlen(s);
	_type = STRING;
	_data = new std::vector<char>(string_len+1);
	memmove(&(*_data)[0], s, string_len+1);
}

inline void DynamicConfigValue::set_data(void *p, unsigned len)
{
	destroy();
	_type = DATA;
	_data = new std::vector<char>(len);
	memmove(&(*_data)[0], p, len);
}

inline void DynamicConfigValue::set_empty_array()
{
	destroy();
	_type = ARRAY;
	_array =new  std::vector<DynamicConfigValue>();
}

inline void DynamicConfigValue::set_empty_object()
{
	destroy();
	_type = OBJECT;
	_object = new Map();
}

inline DynamicConfigValue & DynamicConfigValue::operator[](unsigned i)
{
	if (!is_array()) {
		assert(is_nil());
		set_empty_array();
	}
	if (i >= size())
		_array->resize(i+1);
	return (*_array)[i];
}

inline unsigned DynamicConfigValue::num_keys()
{
	if (!is_object()) {
		assert(is_nil());
		return 0;
	}
	return _object->size();
}

inline DynamicConfigValue & DynamicConfigValue::operator[](const char *s)
{
	if (!is_object()) {
		assert(is_nil());
		set_empty_object();
	}
	return (*_object)[s];
}

inline DynamicConfigValue & DynamicConfigValue::push()
{
	if (!is_array()) {
		assert(is_nil());
		set_empty_array();
	}
	_array->push_back( DynamicConfigValue() );
	return _array->back();
}

inline DynamicConfigValue::iterator DynamicConfigValue::begin()
{
	return is_object() ? iterator(_object->begin()) : iterator();
}

inline DynamicConfigValue::iterator DynamicConfigValue::end()
{
	return is_object() ? iterator(_object->end()) : iterator();
}

inline DynamicConfigValue::const_iterator DynamicConfigValue::begin() const
{
	return is_object() ? const_iterator(_object->begin()) : const_iterator();
}

inline DynamicConfigValue::const_iterator DynamicConfigValue::end() const
{
	return is_object() ? const_iterator(_object->end()) : const_iterator();
}

inline void DynamicConfigValue::remove_key(const char *s)
{
	assert(is_object());
	assert(_object->find(s) != _object->end());
	_object->erase(s);
}

} // namespace stingray
