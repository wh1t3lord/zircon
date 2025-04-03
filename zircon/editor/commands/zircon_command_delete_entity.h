#pragma once

#include "zircon_command_definitions.h"
#include "../../ecs/components/zircon_factory_definitions.h"

class zircon_factory;
class zircon_editor_command_history;
class zircon_world;

enum zircon_component_type_t;

// TODO: implement streaming of json size of 30k+ while we are limited in our
// storage
class zircon_command_delete_entity : public kotek::core::ktkISDKRedoUndo
{
public:
	zircon_command_delete_entity(zircon_editor_command_history* p_history,
		zircon_world* p_scene, zircon_factory* p_factory,
		entt::entity entity_to_delete);

	~zircon_command_delete_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::uint32_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::uint32_t id) noexcept override;

	kotek::enum_base_t GetCommandType() noexcept override;
	kotek::size_t Serialize(kotek::cfstream_t* p_file,
		kotek::core::ktkIResourceManager* p_resource_manager) noexcept override;
	void Deserialize(const kotek::json::object& json_data) noexcept;

private:
	zircon_editor_command_history* m_p_history;
	zircon_world* m_p_scene;
	zircon_factory* m_p_factory;
	entt::entity m_entity_created;
	entt::entity m_entity_previous_id;
	kotek::static_vector_t<zircon_component_type_t,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
		m_components;
	char m_p_serialized_json_as_string
		[ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
	unsigned char m_p_placement_new_memory
		[(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON -
			 ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE) -
			sizeof(m_components)];
};
