#include "Items/DropTableSchema.h"

#include "Items/SectionId.h"

namespace psr {

namespace {

    std::vector<FieldSchema> BuildEntryFields()
    {
        std::vector<FieldSchema> fields;
        fields.push_back(FieldSchema{"item_prefab_id", FieldKind::NameId});
        fields.push_back(FieldSchema{"weight", FieldKind::Number});

        FieldSchema section_weights{"section_id_weights", FieldKind::Object};
        for (const auto& [name, value] : EnumNames<SectionId>::kValues)
            section_weights.children.push_back(FieldSchema{std::string(name), FieldKind::Number});
        fields.push_back(std::move(section_weights));

        return fields;
    }

    FieldSchema BuildEntryListField(const std::string& name)
    {
        FieldSchema list{name, FieldKind::Array};
        FieldSchema item{"item", FieldKind::Object};
        item.children = BuildEntryFields();
        list.children.push_back(std::move(item));
        return list;
    }

} // namespace

DropTableSchemaModel BuildDropTableSchemaModel()
{
    DropTableSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(BuildEntryListField("common_entries"));
    model.fields.push_back(BuildEntryListField("rare_entries"));

    FieldSchema guaranteed{"guaranteed_item_ids", FieldKind::Array};
    guaranteed.children.push_back(FieldSchema{"item", FieldKind::NameId});
    model.fields.push_back(std::move(guaranteed));

    model.fields.push_back(FieldSchema{"rare_roll_chance_percent", FieldKind::Number});
    model.fields.push_back(FieldSchema{"meseta_min", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"meseta_max", FieldKind::Integer});

    return model;
}

} // namespace psr
