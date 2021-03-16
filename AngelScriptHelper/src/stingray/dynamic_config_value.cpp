#include "dynamic_config_value.h"
#include "parse_simplified_json.h"

#ifdef CAN_COMPILE
	namespace { stingray::logging::System CONFIG = { "Config" }; }
#endif

namespace
{
	const char *RESOURCE_NAME = "$resource_name";
	const char *RESOURCE_TYPE = "$resource_type";
}

namespace stingray {

void DynamicConfigValue::find(const char *key, std::vector<DynamicConfigValue> &found) const
{
	if (is_object()) {
		DynamicConfigValue::const_iterator it = begin(), e = end();
		for (; it!=e; ++it) {
			if (strcmp(it.key(),key) == 0)
				found.push_back(it.value());
			it.value().find(key, found);
		}
	}

	if (is_array()) {
		for (unsigned i=0; i<size(); ++i)
			(*this)[i].find(key, found);
	}
}

bool DynamicConfigValue::is_resource(ReferenceFormat rf) const
{
	return (rf == ReferenceFormat::BOTH && is_string()) || (*this)[RESOURCE_NAME].is_string();
}

bool DynamicConfigValue::is_resource(const char *type, ReferenceFormat rf) const
{
	if (!is_resource(rf)) return false;
	if (is_object()) {
		if (type == nullptr) return true;
		auto rtype = (*this)[RESOURCE_TYPE].to_string_unsafe();
		return strcmp(type, rtype) == 0;
	}
	return true;
}

const char *DynamicConfigValue::to_resource_unsafe(const char *type, ReferenceFormat rf) const
{
	assert(is_resource(type, rf));
	if (rf == ReferenceFormat::BOTH && is_string()) {
		// logging::warning(CONFIG, eprintf("Old resource reference `%s`: `%s`.", type, to_string_unsafe()));
		auto s = _data->data();
		auto dot = strchr(s, '.');
		if (dot)
			*dot = 0;
		return s;
	}
	return (*this)[RESOURCE_NAME].to_string_unsafe();
}

const char *DynamicConfigValue::to_resource(const char *type, bool &e, ReferenceFormat rf) const
{
	if (is_resource(type, rf))
		return to_resource_unsafe(type, rf);
	else {
		e = true;
		return "expected a resource";
	}
}


#ifdef CAN_COMPILE
	const char *DynamicConfigValue::to_resource(const char *type, ErrorState &e, ReferenceFormat rf) const
	{
		if (is_resource(type, rf))
			return to_resource_unsafe(type, rf);
		set_error(e, eprintf("Expected a resource of type `%s`", type));
		return "expected a resource";
	}

	DynamicConfigValue::Resource DynamicConfigValue::to_resource(Allocator &ta, ErrorState &e, ReferenceFormat rf) const
	{
		if (is_resource(rf))
			return to_resource_unsafe(ta, rf);
		set_error(e, eprintf("Expected a resource "));
		Resource r = {"expected a resource", "expected a resource"};
		return r;
	}

	const char *DynamicConfigValue::get_resource(const char *key, const char *type, ErrorState &e, ReferenceFormat rf) const
	{
		const auto &child = (*this)[key];
		if (child.is_resource(type, rf))
			return child.to_resource_unsafe(type, rf);
		set_error(e, key, eprintf("Expected a resource of type `%s`", type));
		return "expected a resource";
	}

	DynamicConfigValue::Resource DynamicConfigValue::get_resource(const char *key, Allocator &ta, ErrorState &e, ReferenceFormat rf) const
	{
		const auto &child = (*this)[key];
		if (child.is_resource(rf))
			return child.to_resource_unsafe(ta, rf);
		set_error(e, key, eprintf("Expected a resource"));
		Resource r = { "expected a resource", "expected a resource" };
		return r;
	}

	void DynamicConfigValue::set_error(ErrorState &es, const char *msg) const
	{
		if (!es.error)
			es.error = sjson::format_error(es, *this, "%s", msg);
	}

	void DynamicConfigValue::set_error(ErrorState &es, const char *key, const char *msg) const
	{
		if (!es.error)
			es.error = sjson::format_error(es, *this, "%s for key `%s`", msg, key);
	}

	void DynamicConfigValue::set_error(ErrorState &es, unsigned key, const char *msg) const
	{
		if (!es.error)
			es.error = sjson::format_error(es, *this, "%s for key `%i`", msg, key);
	}
#endif

} // namespace stingray
