#include "Engine/ECS/EntityDescriber.h"

#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cmath>
#include <cstdio>

namespace psr {

using namespace entt::literals;

namespace {

    // Every leaf kind that isn't a Vec2/Color/String rides entt's
    // arithmetic/enum conversion helper (the same mechanism JsonToMeta relies
    // on to *write* a double into an int/float/bool field) -- meta_any's plain
    // cast<T>() only succeeds on an exact type match, so the underlying
    // arithmetic width must be normalised through allow_cast<double>() first.
    double AsDouble(const entt::meta_any& raw)
    {
        entt::meta_any converted = raw.allow_cast<double>();
        return converted ? converted.cast<double>() : 0.0;
    }

    std::vector<FieldValue> BuildFields(const std::vector<FieldSchema>& fields, const entt::meta_type& type,
                                        const entt::meta_any& instance);

    FieldValue BuildFieldValue(const FieldSchema& field, const entt::meta_any& raw)
    {
        FieldValue result;
        result.name = field.name;
        result.kind = field.kind;

        switch (field.kind)
        {
        case FieldKind::Vec2:
        {
            const Vec2 value = raw.cast<Vec2>();
            result.text = std::to_string(value.x) + ", " + std::to_string(value.y);
            break;
        }
        case FieldKind::Color:
        {
            const Color value = raw.cast<Color>();
            char hex[10];
            std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", value.r, value.g, value.b, value.a);
            result.text = hex;
            break;
        }
        case FieldKind::String:
            result.text = raw.cast<std::string>();
            break;
        case FieldKind::Boolean:
            result.text = AsDouble(raw) != 0.0 ? "true" : "false";
            break;
        case FieldKind::Integer:
            result.text = std::to_string(static_cast<long long>(std::llround(AsDouble(raw))));
            break;
        case FieldKind::Number:
            result.text = std::to_string(AsDouble(raw));
            break;
        case FieldKind::NameId:
        {
            const auto hash = static_cast<std::uint32_t>(std::llround(AsDouble(raw)));
            if (std::optional<std::string> label = NameIdRegistry::Find(hash))
                result.text = *label;
            else
                result.text = "#" + std::to_string(hash);
            break;
        }
        case FieldKind::Enum:
        {
            const entt::meta_type enum_type = raw.type();
            result.text = "?";
            for (const std::string& name : field.enum_values)
            {
                entt::meta_data constant = enum_type.data(entt::hashed_string::value(name.c_str()));
                if (constant && constant.get({}) == raw)
                {
                    result.text = name;
                    break;
                }
            }
            break;
        }
        case FieldKind::Array:
        {
            entt::meta_sequence_container view = raw.as_sequence_container();
            std::string joined;
            if (view)
            {
                for (auto it = view.begin(), end = view.end(); it != end; ++it)
                {
                    FieldValue element = BuildFieldValue(field.ElementSchema(), *it);
                    if (!joined.empty())
                        joined += ", ";
                    joined += element.text;
                    result.children.push_back(std::move(element));
                }
            }
            result.text = joined;
            break;
        }
        case FieldKind::Object:
        {
            const entt::meta_type object_type = raw.type();
            result.children = BuildFields(field.children, object_type, raw);
            std::string joined;
            for (const FieldValue& child : result.children)
            {
                if (!joined.empty())
                    joined += ", ";
                joined += child.name + ": " + child.text;
            }
            result.text = joined;
            break;
        }
        }

        return result;
    }

    std::vector<FieldValue> BuildFields(const std::vector<FieldSchema>& fields, const entt::meta_type& type,
                                        const entt::meta_any& instance)
    {
        std::vector<FieldValue> result;
        result.reserve(fields.size());
        for (const FieldSchema& field : fields)
        {
            entt::meta_data data = type.data(entt::hashed_string::value(field.name.c_str()));
            if (!data)
                continue; // schema/meta out of sync -- skip rather than crash
            entt::meta_any raw = data.get(instance);
            if (!raw)
                continue;
            result.push_back(BuildFieldValue(field, raw));
        }
        return result;
    }

} // namespace

ComponentValue DescribeComponentValue(const ComponentSchema& schema, const entt::meta_type& type,
                                      const entt::meta_any& instance)
{
    ComponentValue result;
    result.component_id = schema.id;
    if (!schema.is_tag)
        result.fields = BuildFields(schema.fields, type, instance);
    return result;
}

} // namespace psr
