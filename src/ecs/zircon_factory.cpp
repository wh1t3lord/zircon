#include "zircon_factory.h"

zircon_factory::zircon_factory(void) :
	m_allocated_context_count{}, m_p_input{}
{
	this->register_components();
	this->register_components_restrictions();
	this->register_components_and_their_enums();
	this->register_lookuptable_component_enum_and_id_type();
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
	Kotek::entity_t id,
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

#include "zircon_factory_create_component.cpp"
#include "zircon_factory_get_component_name_by_enum.cpp"
#include "zircon_validate_get_component_type_of_all_components.cpp"
#include "zircon_serialize_deserialize_component.cpp"