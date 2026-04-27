#pragma once

#include "zircon_command_definitions.h"
#include "../../ecs/zircon_factory_definitions.h"

class zircon_factory;
class zircon_editor_command_history;
class zircon_world;
class zircon_session_editor_manager;

enum eZirconComponentType;

// TODO: implement streaming of json size of 30k+ while we are
// limited in our storage
class zircon_command_delete_entity
	: public kotek::core::ktkISDKRedoUndo
{
public:
	zircon_command_delete_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t entity_to_delete
	);

	~zircon_command_delete_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::entity_t id) noexcept override;

	kotek::enum_base_t GetCommandType() noexcept override;
	kotek::size_t Serialize(kotek::core::ktkFileHandleType file
	) noexcept override;
	void Deserialize(const kotek::json::object& json_data
	) noexcept;

private:
	kotek::entity_t m_entity_created;
	kotek::entity_t m_entity_previous_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::static_vector_t<
		eZirconComponentType,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
		m_components;
	char m_p_serialized_json_as_string
		[ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
	unsigned char m_p_placement_new_memory
		[(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON -
	      ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE
	     ) -
	     sizeof(m_components)];
};
