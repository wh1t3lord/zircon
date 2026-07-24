#pragma once

#include "../core/zircon_config.h"

#include "zircon_component_camera.h"
#include "zircon_component_actor.h"
#include "zircon_component_geometry.h"
#include "zircon_component_input.h"
#include "zircon_component_transform.h"
#include "zircon_component_frustum.h"
#include "zircon_component_animation.h"
#include "zircon_component_terrain_cbt.h"
#include "zircon_component_terrain_gcm.h"
#include "zircon_component_terrain_static.h"

// render
#include "zircon_component_bounding_sphere.h"

#include "zircon_component_ui_camera.h"
#include "zircon_component_ui_surface.h"

// sdk
#include "zircon_component_sdk_scene_name.h"
#include "zircon_component_sdk_camera.h"
#include "zircon_component_sdk_input.h"

#include "zircon_factory_definitions.h"

class zircon_config;
class zircon_session_game_manager;
class zircon_session_editor_manager;

inline constexpr unsigned char kZirconFactory_ECSContext_InvalidID =
	unsigned char(-1);

struct zircon_ecs_context_t
{
	friend class zircon_factory;

private:
	unsigned char id = kZirconFactory_ECSContext_InvalidID;
	void* p_impl;
#ifdef KOTEK_USE_ECS_BACKEND_PICO
	/// @brief \~english indexing of this vector is based on eZirconComponentType enum
	kotek::static_vector_t<
		ecs_comp_t,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
		m_components_definitions;
#endif
};

void zircon_deserialize_component(
	const kotek::ktk::json::value& serialized_data,
	zircon_component_interface* p_result
) noexcept;

kotek::ktk::json::value zircon_serialize_component(
	zircon_component_interface* p_data
) noexcept;

kotek::ktk::json::value zircon_serialize_component(
	zircon_component_interface* p_data,
	unsigned char* p_raw_memory,
	kotek::size_t memory_size
) noexcept;

// todo: make entt in same design as pico and it is should be
// same by method calling and etc
/// @brief \~english factory doesn't rule the allocation so we
/// have context and that context contains the world
class zircon_factory
{
	using lock_guard =
		kotek::ktk::mt::lock_guard<kotek::ktk::mt::mutex>;

public:
	zircon_factory(void);

	~zircon_factory(void);

	void Initialize(
		zircon_config* p_config,
		kotek::core::ktkConsole* p_console,
		kotek::core::ktkIInput* p_input
	);
	void Shutdown(void);

	zircon_ecs_context_t*
	create_context(kotek::uint32_t entity_count_max_limit
	) noexcept;

	kotek::entity_t
	create_entity(zircon_ecs_context_t* p_context) noexcept;

	void destroy_entity(
		zircon_ecs_context_t* p_context, kotek::entity_t entity
	) noexcept;

	void destroy_context(zircon_ecs_context_t* p_context
	) noexcept;

	// todo: delete outdated methods signatures that under
	// define KOTEK_USE_ECS_BACKEND_ENTT
#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	bool IsValidEntity(entt::entity id) noexcept
	{
		return this->m_registry.valid(
			static_cast<entt::entity>(id)
		);
	}

	template <typename ComponentType>
	const ComponentType& GetComponent(entt::entity id
	) const noexcept
	{
		return this->m_registry.get<ComponentType>(
			static_cast<entt::entity>(id)
		);
	}

	template <typename ComponentType>
	ComponentType& GetComponent(entt::entity id) noexcept
	{
		return this->m_registry.get<ComponentType>(
			static_cast<entt::entity>(id)
		);
	}

	void* GetComponentByName(
		entt::entity id,
		const kotek::static_cstring_view_t& component_name
	) noexcept
	{
		KOTEK_ASSERT(
			component_name.empty() == false,
			"you can't pass an empty string here"
		);

		auto hashed_type =
			this->m_component_name_to_id.at(component_name);

		auto* p_existed_storage =
			this->m_registry.storage(hashed_type);

		if (p_existed_storage)
		{
			if (p_existed_storage->contains(
					static_cast<entt::entity>(id)
				))
			{
				auto p_result = p_existed_storage->value(
					static_cast<entt::entity>(id)
				);

				return p_result;
			}
		}

		return nullptr;
	}

	template <typename ComponentType>
	bool HasComponent(entt::entity id) noexcept
	{
		return this->m_registry.all_of<ComponentType>(id);
	}

	template <typename... ComponentTypes>
	bool HasComponents(entt::entity id) noexcept
	{
		return this->m_registry.all_of<ComponentTypes...>(id);
	}

	bool HasRequiredComponentsForCreation(
		entt::entity id, entt::id_type component_hash_id
	) noexcept;

	bool HasRequiredComponentsForCreation(
		entt::entity id,
		const kotek::static_cstring_view_t& component_name
	) noexcept;

	bool HasComponent(
		entt::entity id, entt::id_type hashed_type
	) noexcept
	{
		bool result{};

		auto* p_existed = this->m_registry.storage(hashed_type);

		auto component_name_from_preprocessor =
			this->m_component_id_to_name.at(hashed_type);

		if (p_existed)
		{
			if (p_existed->contains(static_cast<entt::entity>(id
			    )))
			{
				KOTEK_ASSERT(
					p_existed->value(
						static_cast<entt::entity>(id)
					),
					"must return a non-nullptr type otherwise "
					"it means something is broken and you "
					"obtain a valid entity that has storage "
					"but the data was deleted, probably..."
				);

				result = true;
			}
		}

		return result;
	}

	/// @brief searching from registered components if type_hash
	/// presented in entt means your entity has component
	/// otherwise you component doesn't have the specified
	/// component
	/// @param id
	/// @param component_name_from_preprocessor
	/// @return
	bool HasComponent(
		entt::entity id,
		const kotek::static_cstring_view_t&
			component_name_from_preprocessor
	) noexcept
	{
		KOTEK_ASSERT(
			component_name_from_preprocessor.empty() == false,
			"you can't pass an empty component name, because "
			"if "
			"you pass an empty string we can't understand what "
			"component you are looking for!!!!!!!!"
		);

		bool result{};

		if (this->m_registry.valid(static_cast<entt::entity>(id)
		    ) == false)
			return result;

		if (this->m_component_name_to_id.find(
				component_name_from_preprocessor
			) != this->m_component_name_to_id.end())
		{
			auto hashed_type = this->m_component_name_to_id.at(
				component_name_from_preprocessor
			);

			result = this->HasComponent(id, hashed_type);
		}

		return result;
	}

	template <
		typename ComponentType,
		typename... ArgumentsForConstruction>
	void CreateComponent(
		entt::entity id, ArgumentsForConstruction&&... args
	) noexcept
	{
		KOTEK_ASSERT(
			this->m_component_name_to_id.find(
				ComponentType::GetComponentName()
			) != this->m_component_name_to_id.end(),
			"you forgot to register your component: {}",
			ComponentType::GetComponentName()
		);

		constexpr auto hash_id =
			entt::type_hash<ComponentType>::value();

		if (this->HasRequiredComponentsForCreation(
				id, hash_id
			) == false)
			return;

		this->m_registry.emplace<ComponentType>(
			static_cast<entt::entity>(id),
			std::forward<ArgumentsForConstruction>(args)...
		);
	}

	void* CreateComponentByName(
		entt::entity id, const char* component_name
	) noexcept
	{
		KOTEK_ASSERT(
			component_name,
			"you can't pass an invalid component name"
		);
		KOTEK_ASSERT(
			strlen(component_name),
			"you can't pass an empty string!"
		);
		KOTEK_ASSERT(
			this->m_component_name_to_id.find(component_name) !=
				this->m_component_name_to_id.end(),
			"you forgot to register your component: {}",
			component_name
		);
		KOTEK_ASSERT(
			this->m_p_input,
			"you forgot to call Initialize method!"
		);

		if (this->HasComponent(id, component_name))
			return nullptr;

		auto hashed_type =
			this->m_component_name_to_id.at(component_name);

		if (this->HasRequiredComponentsForCreation(
				id, hashed_type
			) == false)
			return nullptr;

		auto* p_existed_storage =
			this->m_registry.storage(hashed_type);

		if (p_existed_storage)
		{
			auto status = p_existed_storage->push(
				static_cast<entt::entity>(id)
			);

			if (status == p_existed_storage->end())
			{
				KOTEK_MESSAGE(
					"failed to add the component: {}",
					component_name
				);

				return nullptr;
			}
			else
			{
				auto p_result = p_existed_storage->value(
					static_cast<entt::entity>(id)
				);

				KOTEK_ASSERT(
					p_result,
					"must be valid pointer otherwise operation "
					"for push for "
					"storage of component[{}] failed and it "
					"returned a "
					"nullptr!",
					component_name
				);

				if (p_result)
				{
					zircon_component_interface* p_interface =
						static_cast<
							zircon_component_interface*>(
							p_result
						);

					if (p_interface)
					{
						p_interface->register_managers(
							this->m_p_manager_session_game,
							this->m_p_manager_session_editor
						);

						switch (p_interface->get_component_type(
						))
						{
						case kComponentTypezircon_component_sdk_input:
						{
							zircon_component_sdk_input*
								p_casted = static_cast<
									zircon_component_sdk_input*>(
									p_interface
								);

							p_casted->get_input()
								.register_input(this->m_p_input
							    );

							break;
						}
						case kComponentTypezircon_component_input:
						{
							zircon_component_input* p_casted =
								static_cast<
									zircon_component_input*>(
									p_interface
								);

							p_casted->register_input(
								this->m_p_input
							);

							break;
						}
						case kComponentTypezircon_component_animation:
						{
							KOTEK_ASSERT(
								this->HasComponent<
									zircon_component_geometry>(
									id
								),
								"must have this component "
								"otherwise you broke "
								"the order of adding "
								"components first geometry "
								"then animation"
							);
							break;
						}
						default:
						{
							break;
						}
						}
					}
				}

				return p_result;
			}
		}
		else
		{
	#include "zircon_factory_create_component_by_name.cpp"
		}

		return this->GetComponentByName(id, component_name);
	}

	Kotek::ktk::json::value SerializeComponentByNameToJSON(
		entt::entity id,
		const kotek::static_cstring_view_t& component_name
	) noexcept
	{
		Kotek::ktk::json::value result;

		KOTEK_ASSERT(
			component_name.empty() == false,
			"you can't pass an empty string here"
		);

		void* p_data =
			this->GetComponentByName(id, component_name);

		if (p_data)
		{
			zircon_component_interface* p_component =
				static_cast<zircon_component_interface*>(p_data
			    );

			result = p_component->serialize();
		}

		return result;
	}

	kotek::ktk::json::value SerializeComponentByNameToJSON(
		entt::entity id,
		const char* p_component_name,
		unsigned char* p_raw_memory,
		Kotek::ktk::size_t raw_memory_size
	) noexcept
	{
		KOTEK_ASSERT(p_component_name, "invalid string");
		KOTEK_ASSERT(strlen(p_component_name), "empty string");
		KOTEK_ASSERT(p_raw_memory, "invalid part of memory!");
		KOTEK_ASSERT(raw_memory_size, "can't be zero!");

		kotek::ktk::json::value result;

		void* p_data =
			this->GetComponentByName(id, p_component_name);

		if (p_data)
		{
			zircon_component_interface* p_component =
				static_cast<zircon_component_interface*>(p_data
			    );

			result = p_component->serialize(
				p_raw_memory, raw_memory_size
			);
		}

		return result;
	}

	template <typename ComponentType>
	void RemoveComponent(entt::entity id) noexcept
	{
		if (this->m_registry.valid(static_cast<entt::entity>(id)
		    ))
		{
			if (this->HasComponent<ComponentType>(id))
			{
				this->m_registry.erase<ComponentType>(
					static_cast<entt::entity>(id)
				);
			}
		}
	}

	void RemoveComponentByName(
		entt::entity id,
		const kotek::static_cstring_view_t& component_name
	) noexcept
	{
		KOTEK_ASSERT(
			component_name.empty() == false,
			"you can't pass an empty string here"
		);

		if (this->HasComponent(id, component_name))
		{
			auto hashed_type =
				this->m_component_name_to_id.at(component_name);

			auto* p_existed_storage =
				this->m_registry.storage(hashed_type);

			if (p_existed_storage)
			{
				if (p_existed_storage->contains(
						static_cast<entt::entity>(id)
					))
				{
					p_existed_storage->erase(
						static_cast<entt::entity>(id)
					);
				}
			}
		}
	}

	entt::entity CreateEntity(void) noexcept
	{
		auto result = this->m_registry.create();

		return static_cast<entt::entity>(result);
	}

	auto GetAllEntities(void) const noexcept
	{
		return this->m_registry.view<entt::entity>();
	}

	bool RemoveEntity(entt::entity id) noexcept
	{
		if (this->m_registry.valid((entt::entity)id))
		{
			this->m_registry.destroy(
				static_cast<entt::entity>(id)
			);

			return true;
		}

		return false;
	}

	const kotek::unordered_map_t<
		kotek::static_cstring_view_t,
		entt::id_type>&
	GetRegisteredComponents(void) const noexcept
	{
		return this->m_component_name_to_id;
	}
#endif

	bool is_valid_entity(
		zircon_ecs_context_t* p_context, kotek::entity_t id
	) noexcept;
	zircon_component_interface* get_component_by_name(
		zircon_ecs_context_t* p_context,
		Kotek::entity_t id,
		const kotek::cstring_view_t& component_name
	) noexcept;
	zircon_component_interface* get_component_by_enum(
		zircon_ecs_context_t* p_context,
		Kotek::entity_t id,
		eZirconComponentType component_type
	) noexcept;

	void DeserializeComponent(
		void* p_raw_data,
		const Kotek::ktk::json::value& serialized_data
	) noexcept
	{
		KOTEK_ASSERT(
			p_raw_data,
			"you can't pass an invalid component here; Also "
			"you "
			"must pass a component created from storage!"
		);

		zircon_component_interface* p_component =
			static_cast<zircon_component_interface*>(p_raw_data
		    );

		zircon_deserialize_component(
			serialized_data, p_component
		);
	}

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	void CreateAllComponents(
		entt::entity entity_id,
		const Kotek::ktk::vector<Kotek::ktk::pair<
			Kotek::ktk::cstring,
			Kotek::ktk::json::value>>& serialized_components
	) noexcept
	{
		if (serialized_components.empty() == false)
		{
			for (const auto& [component_name, serialized_data] :
			     serialized_components)
			{
				KOTEK_ASSERT(
					component_name.empty() == false,
					"can't be, you must got a valid string "
					"that "
					"represents your component for creation, "
					"see "
					"GetAllComponentsOfEntity method. "
					"Something is "
					"wrong! Data is corrupted"
				);

				auto p_raw_data = this->CreateComponentByName(
					entity_id, component_name.c_str()
				);

				this->DeserializeComponent(
					p_raw_data, serialized_data
				);
			}
		}
	}

	const entt::registry& GetRegistry(void) const noexcept
	{
		return this->m_registry;
	}

	entt::registry& GetRegistry(void) noexcept
	{
		return this->m_registry;
	}

	kotek::static_vector_t<
		kotek::pair_t<
			kotek::static_cstring_view_t,
			Kotek::ktk::json::value>,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
	GetAllComponentsOfEntity(entt::entity entity_id) noexcept
	{
		kotek::static_vector_t<
			kotek::pair_t<
				kotek::static_cstring_view_t,
				Kotek::ktk::json::value>,
			zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
			result;

		entt::entity id = static_cast<entt::entity>(entity_id);

		if (this->m_registry.valid(id))
		{
			for (const auto& pair : this->m_registry.storage())
			{
				const auto& storage = pair.second;

				if (storage.contains(id))
				{
					auto component_id = pair.first;

					const auto& component_name =
						this->m_component_id_to_name.at(
							component_id
						);

					KOTEK_ASSERT(
						component_name.empty() == false,
						"you can't register a component that "
						"has "
						"empty name! (See preprocessor "
						"otherwise "
						"you implemented own method that "
						"doesn't "
						"have a valid string)"
					);

					auto p_raw_data = this->GetComponentByName(
						entity_id, component_name.data()
					);

					if (p_raw_data)
					{
						auto* p_casted = static_cast<
							zircon_component_interface*>(
							p_raw_data
						);

						if (p_casted)
						{
							result.push_back(
								{component_name,
							     p_casted->serialize()}
							);
						}
					}
				}
			}
		}

		return result;
	}
#endif

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	kotek::static_vector_t<
		zircon_component_type_t,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
	get_all_components_of_entity(entt::entity id) noexcept
	{
		kotek::static_vector_t<
			zircon_component_type_t,
			zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
			result;

		if (this->IsValidEntity(id))
		{
			for (const auto& [component_name, component_hashed_type] :
			     this->m_component_name_to_id)
			{
				if (this->HasComponent(
						id, component_hashed_type
					))
				{
					result.push_back(
						this->get_component_type_id_by_component_name(
							component_name
						)
					);
				}
			}
		}

		return result;
	}

	inline entt::id_type
	get_type_hash_by_enum(zircon_component_type_t id
	) const noexcept
	{
		entt::id_type result = -1;

		if (id >= zircon_component_type_t::
		              kComponentTypeUnknown ||
		    id < 0)
			return result;

		result =
			this->m_lookuptable_id_types_by_component_enum[id];

		return result;
	}
#endif

	void get_all_components_of_entity(
		zircon_ecs_context_t* p_context,
		kotek::entity_t entity_id,
		kotek::static_vector_t<
			eZirconComponentType,
			zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>& result
	);

	/// @brief \~english pico entity ids are dense indices in
	/// [0, entity_count_max_limit), so enumeration is a scan
	/// with ecs_is_ready; writes at most result_capacity
	/// entries and returns the amount written
	kotek::uint32_t get_all_entities(
		zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_count_max_limit,
		kotek::entity_t* p_result,
		kotek::uint32_t result_capacity
	) noexcept;

	bool create_component(
		zircon_ecs_context_t* p_context,
		kotek::entity_t id,
		eZirconComponentType component_type_id,
		kotek::ktk::json::value& serialized_component
	);

	bool has_component(
		zircon_ecs_context_t* p_context,
		kotek::entity_t id,
		eZirconComponentType component_type
	) noexcept;
	bool create_component(
		zircon_ecs_context_t* p_context,
		kotek::entity_t id,
		eZirconComponentType component_type
	) noexcept;

	void remove_component(
		zircon_ecs_context_t* p_context,
		kotek::entity_t id,
		eZirconComponentType component_type
	) noexcept;

	/// @brief \~english linear search over
	/// eZirconComponentType using get_component_name_by_enum,
	/// returns kunknown when name is not registered
	eZirconComponentType get_component_enum_by_name(
		const kotek::cstring_view_t& component_name
	) const noexcept;

	const char* get_component_name_by_enum(
		eZirconComponentType component_type
	) const noexcept;

	void validate_get_component_type_of_all_components() const;

private:
	template <typename ComponentType>
	inline const char* GetComponentTypeName(void) const noexcept
	{
#ifdef KOTEK_PLATFORM_WINDOWS
		return typeid(ComponentType).raw_name();
#elif KOTEK_PLATFORM_LINUX
		return typeid(ComponentType).name();
#endif
	}

	zircon_ecs_context_t* allocate_context() noexcept;

	void register_components(zircon_ecs_context_t* p_context);
	void register_components_and_their_enums();

private:
	unsigned char m_allocated_context_count;

	kotek::core::ktkConsole* m_p_console;
	kotek::core::ktkIInput* m_p_input;

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	kotek::unordered_map_t<
		kotek::static_cstring_view_t,
		entt::id_type>
		m_component_name_to_id;
	kotek::unordered_map_t<
		entt::id_type,
		kotek::static_cstring_view_t>
		m_component_id_to_name;
#endif

	bool m_free_memory_ids[ZIRCON_DEF_MAX_WORLD_COUNT];
	/// @brief how many contexts we can use for application
	unsigned char m_p_raw_memory[ZIRCON_DEF_MAX_WORLD_COUNT]
								[sizeof(zircon_ecs_context_t)];

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	// for each component (if it is needed) you specify hash
	// types of what components it depends. For example
	// component_ui_camera will be created if component_camera
	// exists in entity.
	kotek::unordered_map_t<
		kotek::static_cstring_view_t,
		kotek::vector_t<entt::id_type>>
		m_component_creation_restriction_by_component_name;

	kotek::unordered_map_t<
		entt::id_type,
		kotek::vector_t<entt::id_type>>
		m_component_creation_restriction_by_hash;
#endif

	kotek::mt::mutex_t m_mutex;

#ifdef KOTEK_USE_ECS_BACKEND_ENTT
	entt::registry m_registry;
#endif
};