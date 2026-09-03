#include "zircon_factory.h"

zircon_factory::zircon_factory(void) :
	m_allocated_context_count{}, m_p_input{}
{
	//	this->register_components();
	this->register_components_and_their_enums();
	this->validate_get_component_type_of_all_components();
}

zircon_factory::~zircon_factory(void) {}

void zircon_factory::Initialize(
	zircon_config* p_config,
	kotek::core::ktkConsole* p_console,
	kotek::core::ktkIInput* p_input
)
{
	KOTEK_ASSERT(
		p_config,
		"you can't pass an invalid instance of zircon_config!"
	);
	KOTEK_ASSERT(
		p_console,
		"you can't pass an invalid instance of ktkConsole!"
	);
	KOTEK_ASSERT(
		p_input,
		"you can't pass an invalid instance of ktkIInput!"
	);

	this->m_p_console = p_console;
	this->m_p_input = p_input;

	for (unsigned char i = 0; i < ZIRCON_DEF_MAX_WORLD_COUNT;
	     ++i)
	{
		this->m_free_memory_ids[i] = true;
	}
}

void zircon_factory::Shutdown(void)
{
#ifdef KOTEK_DEBUG
	bool was_deallocated_all_worlds = true;

	for (unsigned char i = 0; i < ZIRCON_DEF_MAX_WORLD_COUNT;
	     ++i)
	{
		was_deallocated_all_worlds = this->m_free_memory_ids[i];

		if (was_deallocated_all_worlds == false)
			break;
	}

	KOTEK_ASSERT(
		was_deallocated_all_worlds,
		"you forgot to deallocate worlds!"
	);
#endif
}

zircon_ecs_context_t* zircon_factory::create_context(
	kotek::uint32_t entity_count_max_limit
) noexcept
{
	zircon_ecs_context_t* p_result = this->allocate_context();

	if (p_result)
	{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
		// todo: think how to use memory context for custom
		// allocations
		p_result->p_impl = reinterpret_cast<void*>(
			ecs_new(entity_count_max_limit, nullptr)
		);
		KOTEK_ASSERT(
			p_result->p_impl,
			"failed to allocate ecs context implementation"
		);

		if (p_result->p_impl == nullptr)
		{
			this->destroy_context(p_result);
			p_result = nullptr;
		}
#endif
	}

	if (p_result && p_result->p_impl)
	{
		this->register_components(p_result);
	}

	return p_result;
}

kotek::entity_t
zircon_factory::create_entity(zircon_ecs_context_t* p_context
) noexcept
{
	KOTEK_ASSERT(p_context, "expected to be valid at calling");

	if (p_context)
	{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);
		return ::ecs_create(p_casted);
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
	#error todo: provide impl
#endif
	}

	return kotek::ktk::kInvalidECSEntity;
}

void zircon_factory::destroy_entity(
	zircon_ecs_context_t* p_context, kotek::entity_t entity
) noexcept
{
	KOTEK_ASSERT(p_context, "expected to be valid");

	if (p_context)
	{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);
		return ::ecs_queue_destroy(p_casted, entity);
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
	#error todo: provide impl
#endif
	}
}

void zircon_factory::destroy_context(
	zircon_ecs_context_t* p_context
) noexcept
{
	KOTEK_ASSERT(p_context, "pass valid context");
	KOTEK_ASSERT(
		p_context->id != unsigned char(-1), "pass valid context"
	);

	if (p_context)
	{
		KOTEK_ASSERT(
			p_context->id < ZIRCON_DEF_MAX_WORLD_COUNT,
			"failed to lookup"
		);

		if (p_context->id < ZIRCON_DEF_MAX_WORLD_COUNT)
		{
			KOTEK_ASSERT(
				this->m_free_memory_ids[p_context->id] == false,
				"it was already deallocated otherwise memory "
				"corruption?"
			);
			this->m_free_memory_ids[p_context->id] = true;

			KOTEK_ASSERT(
				p_context->p_impl, "expected to be valid"
			);

			if (p_context->p_impl)
			{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
				ecs_free(
					reinterpret_cast<ecs_t*>(p_context->p_impl)
				);
#endif
			}
		}

		this->m_allocated_context_count--;
	}
}

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
bool zircon_factory::HasRequiredComponentsForCreation(
	entt::entity id, entt::id_type component_hash_id
) noexcept
{
	bool result{true};

	if (this->m_component_creation_restriction_by_hash.find(
			component_hash_id
		) !=
	    this->m_component_creation_restriction_by_hash.end())
	{
		const auto& required_components =
			this->m_component_creation_restriction_by_hash.at(
				component_hash_id
			);

		for (auto hash_id : required_components)
		{
			if (!this->HasComponent(id, hash_id))
			{
				result = false;

				if (this->m_p_config)
				{
					bool is_enabled =
						this->m_p_config->is_feature_enabled(
							eZirconSDKFeatures::
								kSDK_Feature_AddRequiredComponents_Automatically
						);

					if (is_enabled)
					{
						this->m_p_console->Execute_Command(
							static_cast<kotek::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CreateComponentForEntity
							),
							{{this->m_component_id_to_name
						          .at(hash_id)
						          .data()},
						     {static_cast<kotek::uint32_t>(id)}}
						);
						result = is_enabled;

						continue;
					}
				}

				break;
			}
		}
	}

	return result;
}

bool zircon_factory::HasRequiredComponentsForCreation(
	entt::entity id,
	const kotek::static_cstring_view_t& component_name
) noexcept
{
	bool result{};

	if (this->m_component_name_to_id.find(component_name) ==
	    this->m_component_name_to_id.end())
		return result;

	result = this->HasRequiredComponentsForCreation(
		id, this->m_component_name_to_id.at(component_name)
	);

	return result;
}
#elif defined(KOTEK_USE_ECS_BACKEND_PICO)
zircon_component_interface*
zircon_factory::get_component_by_enum(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	eZirconComponentType component_type
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid");
	KOTEK_ASSERT(p_context->p_impl, "must be valid");

	zircon_component_interface* p_result = nullptr;

	if (p_context)
	{
		ecs_t* p_casted_context =
			static_cast<ecs_t*>(p_context->p_impl);

		if (this->has_component(p_context, id, component_type))
		{
			p_result = static_cast<zircon_component_interface*>(
				::ecs_get(
					p_casted_context,
					id,
					p_context->m_components_definitions
						[component_type]
				)
			);
		}
	}

	return p_result;
}
#endif

zircon_ecs_context_t*
zircon_factory::allocate_context() noexcept
{
	zircon_ecs_context_t* p_result = nullptr;

	if (this->m_allocated_context_count ==
	    ZIRCON_DEF_MAX_WORLD_COUNT)
	{
		KOTEK_MESSAGE_WARNING(
			"you try to allocate context but you exceeded "
			"limit: {}",
			ZIRCON_DEF_MAX_WORLD_COUNT
		);
		return p_result;
	}

	unsigned char free_id;

	for (unsigned char i = 0; i < ZIRCON_DEF_MAX_WORLD_COUNT;
	     ++i)
	{
		if (this->m_free_memory_ids[i])
		{
			free_id = i;
			this->m_free_memory_ids[i] = false;
			break;
		}
	}

	p_result = new (this->m_p_raw_memory[free_id])
		zircon_ecs_context_t;
	p_result->id = this->m_allocated_context_count;
	++this->m_allocated_context_count;

	return p_result;
}

bool zircon_factory::is_valid_entity(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid context");

	if (p_context)
	{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
		// id != 0 is NOT validity: a queued-destroy entity keeps its id
		// but pico flips ready=false inside ecs_queue_destroy, and every
		// pico consumer (ecs_has/ecs_get/ecs_queue_*) asserts on non-
		// ready entities — they must read as invalid here (the
		// 2026-09-03 undo-of-create with a stale selection abort)
		if (ecs_is_invalid_entity(id))
		{
			return false;
		}

		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		return ::ecs_is_ready(
			static_cast<ecs_t*>(p_context->p_impl), id);
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
	#error todo: provide impl
#endif
	}

	return false;
}

void zircon_factory::get_all_components_of_entity(
	zircon_ecs_context_t* p_context,
	kotek::entity_t entity_id,
	kotek::static_vector_t<
		eZirconComponentType,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>& result
)
{

	result.clear();

	KOTEK_ASSERT(p_context, "valid context should be");
	KOTEK_ASSERT(
		this->is_valid_entity(p_context, entity_id),
		"must be valid entity"
	);

	if (p_context)
	{
#ifdef KOTEK_USE_ECS_BACKEND_PICO
		// pico offers no per-entity component enumeration — probe each
		// registered type instead (a 16-entry lookup-table scan, house
		// rule 2; the result capacity 64 covers the whole enum by
		// construction)
		for (int component_index = 0;
		     component_index < static_cast<int>(eZirconComponentType::kunknown);
		     ++component_index)
		{
			const eZirconComponentType component_type =
				static_cast<eZirconComponentType>(component_index);

			if (this->has_component(p_context, entity_id, component_type))
			{
				result.push_back(component_type);
			}
		}
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
	#error todo: provide impl
#endif
	}


}

#include "zircon_factory_create_component.cpp"
#include "zircon_factory_get_component_name_by_enum.cpp"
#include "zircon_validate_get_component_type_of_all_components.cpp"
#include "zircon_serialize_deserialize_component.cpp"

#ifdef KOTEK_USE_ECS_BACKEND_PICO
// pico stores components as raw memory, so construction and
// destruction of C++ component types must go through these
// callbacks (placement new / explicit destructor call)
template <typename ComponentType>
static void zircon_component_pico_construct(
	ecs_t* p_ecs, ecs_entity_t entity, void* p_component,
	void* p_args
)
{
	new (p_component) ComponentType();
}

template <typename ComponentType>
static void zircon_component_pico_destruct(
	ecs_t* p_ecs, ecs_entity_t entity, void* p_component
)
{
	static_cast<ComponentType*>(p_component)->~ComponentType();
}

template <typename ComponentType>
static ecs_comp_t zircon_define_component(ecs_t* p_ecs)
{
	return ::ecs_define_component(
		p_ecs,
		sizeof(ComponentType),
		&zircon_component_pico_construct<ComponentType>,
		&zircon_component_pico_destruct<ComponentType>
	);
}

void zircon_factory::register_components(
	zircon_ecs_context_t* p_context
)
{
	KOTEK_ASSERT(p_context, "must be valid");

	if (p_context)
	{
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);

		p_context->m_components_definitions.clear();

		// todo: this switch must be kept in sync with
		// eZirconComponentType (see generated
		// zircon_ecs_auto_enum_components.h), ideally it
		// should be generated by CMake like the other
		// per-component switches
		for (int i = 0;
		     i < static_cast<int>(eZirconComponentType::kunknown);
		     ++i)
		{
			eZirconComponentType component_type =
				static_cast<eZirconComponentType>(i);

			ecs_comp_t component_definition{};

			switch (component_type)
			{
			case eZirconComponentType::kzircon_component_camera:
			{
				component_definition =
					zircon_define_component<zircon_component_camera>(
						p_casted
					);
				break;
			}
			case eZirconComponentType::kzircon_component_input:
			{
				component_definition =
					zircon_define_component<zircon_component_input>(
						p_casted
					);
				break;
			}
			case eZirconComponentType::kzircon_component_transform:
			{
				component_definition = zircon_define_component<
					zircon_component_transform>(p_casted);
				break;
			}
			case eZirconComponentType::kzircon_component_geometry:
			{
				component_definition = zircon_define_component<
					zircon_component_geometry>(p_casted);
				break;
			}
			case eZirconComponentType::kzircon_component_actor:
			{
				component_definition =
					zircon_define_component<zircon_component_actor>(
						p_casted
					);
				break;
			}
			case eZirconComponentType::
				kzircon_component_terrain_cbt:
			{
				component_definition = zircon_define_component<
					zircon_component_terrain_cbt>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_terrain_static:
			{
				component_definition = zircon_define_component<
					zircon_component_terrain_static>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_terrain_gcm:
			{
				component_definition = zircon_define_component<
					zircon_component_terrain_gcm>(p_casted);
				break;
			}
			case eZirconComponentType::kzircon_component_frustum:
			{
				component_definition = zircon_define_component<
					zircon_component_frustum>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_bounding_sphere:
			{
				component_definition = zircon_define_component<
					zircon_component_bounding_sphere>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_ui_surface:
			{
				component_definition = zircon_define_component<
					zircon_component_ui_surface>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_ui_camera:
			{
				component_definition = zircon_define_component<
					zircon_component_ui_camera>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_animation:
			{
				component_definition = zircon_define_component<
					zircon_component_animation>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_sdk_scene_name:
			{
				component_definition = zircon_define_component<
					zircon_component_sdk_scene_name>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_sdk_camera:
			{
				component_definition = zircon_define_component<
					zircon_component_sdk_camera>(p_casted);
				break;
			}
			case eZirconComponentType::
				kzircon_component_sdk_input:
			{
				component_definition = zircon_define_component<
					zircon_component_sdk_input>(p_casted);
				break;
			}
			default:
			{
				KOTEK_ASSERT(
					false,
					"unregistered component type, did you "
					"forget to update this switch?"
				);
				break;
			}
			}

			p_context->m_components_definitions.push_back(
				component_definition
			);
		}
	}
}

void zircon_factory::register_components_and_their_enums()
{
	// nothing to register under pico backend: entt-era
	// name<->hash lookup tables were removed, enum<->name
	// translation is done by generated
	// zircon_translate_component_type_enum_to_string /
	// get_component_name_by_enum
}

bool zircon_factory::has_component(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	eZirconComponentType component_type
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid context");

	bool result{};

	if (p_context)
	{
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		if (this->is_valid_entity(p_context, id) == false)
			return result;

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);

		result = ::ecs_has(
			p_casted,
			id,
			p_context->m_components_definitions[component_type]
		);
	}

	return result;
}

bool zircon_factory::create_component(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	eZirconComponentType component_type
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid context");

	bool result{};

	if (p_context)
	{
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		if (this->is_valid_entity(p_context, id) == false)
			return result;

		if (this->has_component(p_context, id, component_type))
			return result;

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);

		void* p_data = ::ecs_add(
			p_casted,
			id,
			p_context->m_components_definitions[component_type],
			nullptr
		);

		result = p_data != nullptr;
	}

	return result;
}

void zircon_factory::remove_component(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	eZirconComponentType component_type
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid context");

	if (p_context)
	{
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		if (this->has_component(p_context, id, component_type) ==
		    false)
			return;

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);

		::ecs_remove(
			p_casted,
			id,
			p_context->m_components_definitions[component_type]
		);
	}
}

kotek::uint32_t zircon_factory::get_all_entities(
	zircon_ecs_context_t* p_context,
	kotek::uint32_t entity_count_max_limit,
	kotek::entity_t* p_result,
	kotek::uint32_t result_capacity
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid context");
	KOTEK_ASSERT(p_result, "must be valid storage");
	KOTEK_ASSERT(
		entity_count_max_limit, "must be a non-zero limit"
	);

	kotek::uint32_t result{};

	if (p_context && p_result)
	{
		KOTEK_ASSERT(p_context->p_impl, "must be valid");

		ecs_t* p_casted =
			static_cast<ecs_t*>(p_context->p_impl);

		for (kotek::uint32_t i = 0;
		     i < entity_count_max_limit &&
		     result < result_capacity;
		     ++i)
		{
			kotek::entity_t entity{i};

			if (::ecs_is_ready(p_casted, entity))
			{
				p_result[result] = entity;
				++result;
			}
		}
	}

	return result;
}

eZirconComponentType zircon_factory::get_component_enum_by_name(
	const kotek::cstring_view_t& component_name
) const noexcept
{
	KOTEK_ASSERT(
		component_name.empty() == false,
		"you can't pass an empty string here"
	);

	eZirconComponentType result =
		eZirconComponentType::kunknown;

	for (int i = 0;
	     i < static_cast<int>(eZirconComponentType::kunknown);
	     ++i)
	{
		eZirconComponentType current =
			static_cast<eZirconComponentType>(i);

		if (component_name ==
		    this->get_component_name_by_enum(current))
		{
			result = current;
			break;
		}
	}

	return result;
}

zircon_component_interface* zircon_factory::get_component_by_name(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	const kotek::cstring_view_t& component_name
) noexcept
{
	KOTEK_ASSERT(p_context, "must be valid");
	KOTEK_ASSERT(
		component_name.empty() == false,
		"you can't pass an empty string here"
	);

	zircon_component_interface* p_result = nullptr;

	eZirconComponentType component_type =
		this->get_component_enum_by_name(component_name);

	if (component_type != eZirconComponentType::kunknown)
	{
		p_result = this->get_component_by_enum(
			p_context, id, component_type
		);
	}

	return p_result;
}
#endif