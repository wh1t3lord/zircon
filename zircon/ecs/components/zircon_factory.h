#pragma once

#include "zircon_component_camera.h"
#include "zircon_component_actor.h"
#include "zircon_component_geometry.h"
#include "zircon_component_input.h"
#include "zircon_component_model.h"
#include "zircon_component_transform.h"
#include "zircon_component_visibility.h"
#include "zircon_component_frustum.h"

// render
#include "zircon_component_bounding_sphere.h"

#include "zircon_component_ui_camera.h"
#include "zircon_component_ui_surface.h"

// sdk
#include "zircon_component_sdk_scene_name.h"
#include "zircon_component_sdk_camera.h"
#include "zircon_component_sdk_input.h"

class zircon_config;

class zircon_factory_game
{
	using lock_guard = kotek::ktk::mt::lock_guard<kotek::ktk::mt::mutex>;

public:
	zircon_factory_game(void);

	~zircon_factory_game(void);

	void Initialize(
		zircon_config* p_config, kotek::core::ktkConsole* p_console);
	void Shutdown(void);

	bool IsValidEntity(entt::entity id) noexcept
	{
		return this->m_registry.valid(static_cast<entt::entity>(id));
	}

	template <typename ComponentType>
	const ComponentType& GetComponent(entt::entity id) const noexcept
	{
		return this->m_registry.get<ComponentType>(
			static_cast<entt::entity>(id));
	}

	template <typename ComponentType>
	ComponentType& GetComponent(entt::entity id) noexcept
	{
		return this->m_registry.get<ComponentType>(
			static_cast<entt::entity>(id));
	}

	void* GetComponentByName(entt::entity id,
		const Kotek::ktk::cstring& component_name) noexcept
	{
		KOTEK_ASSERT(component_name.empty() == false,
			"you can't pass an empty string here");

		auto hashed_type = this->m_component_name_to_id.at(component_name);

		auto* p_existed_storage = this->m_registry.storage(hashed_type);

		if (p_existed_storage)
		{
			if (p_existed_storage->contains(static_cast<entt::entity>(id)))
			{
				auto p_result =
					p_existed_storage->value(static_cast<entt::entity>(id));

				return p_result;
			}
		}

		return nullptr;
	}

	template <typename ComponentType>
	bool HasComponent(entt::entity id) noexcept
	{
		return this->m_registry.all_of<ComponentType>(
			static_cast<entt::entity>(id));
	}

	bool HasRequiredComponentsForCreation(
		entt::entity id, entt::id_type component_hash_id) noexcept;

	bool HasRequiredComponentsForCreation(entt::entity id,
		const Kotek::ktk::cstring& component_name) noexcept;

	bool HasComponent(
		entt::entity id, entt::id_type hashed_type) noexcept
	{
		bool result{};

		auto* p_existed = this->m_registry.storage(hashed_type);

		auto component_name_from_preprocessor =
			this->m_component_id_to_name.at(hashed_type);

		if (p_existed)
		{
			if (p_existed->contains(static_cast<entt::entity>(id)))
			{
				KOTEK_ASSERT(p_existed->value(static_cast<entt::entity>(id)),
					"must return a non-nullptr type otherwise "
					"it means something is broken and you "
					"obtain a valid entity that has storage "
					"but the data was deleted, probably...");

				result = true;
			}
		}

		return result;
	}

	/// @brief searching from registered components if type_hash
	/// presented in entt means your entity has component otherwise
	/// you component doesn't have the specified component
	/// @param id
	/// @param component_name_from_preprocessor
	/// @return
	bool HasComponent(entt::entity id,
		const Kotek::ktk::cstring& component_name_from_preprocessor) noexcept
	{
		KOTEK_ASSERT(component_name_from_preprocessor.empty() == false,
			"you can't pass an empty component name, because if "
			"you pass an empty string we can't understand what "
			"component you are looking for!!!!!!!!");

		bool result{};

		if (this->m_registry.valid(static_cast<entt::entity>(id)) == false)
			return result;

		if (this->m_component_name_to_id.find(
				component_name_from_preprocessor) !=
			this->m_component_name_to_id.end())
		{
			auto hashed_type = this->m_component_name_to_id.at(
				component_name_from_preprocessor);

			result = this->HasComponent(id, hashed_type);
		}

		return result;
	}

	template <typename ComponentType, typename... ArgumentsForConstruction>
	void CreateComponent(
		entt::entity id, ArgumentsForConstruction&&... args) noexcept
	{
		KOTEK_ASSERT(this->m_component_name_to_id.find(
						 ComponentType::GetComponentName()) !=
				this->m_component_name_to_id.end(),
			"you forgot to register your component: {}",
			ComponentType::GetComponentName());



		constexpr auto hash_id = entt::type_hash<ComponentType>::value();

		if (this->HasRequiredComponentsForCreation(id, hash_id) == false)
			return;

		this->m_registry.emplace<ComponentType>(static_cast<entt::entity>(id),
			std::forward<ArgumentsForConstruction>(args)...);
	}

	void* CreateComponentByName(
		entt::entity id, const char* component_name) noexcept
	{
		KOTEK_ASSERT(
			component_name, "you can't pass an invalid component name");
		KOTEK_ASSERT(strlen(component_name), "you can't pass an empty string!");
		KOTEK_ASSERT(this->m_component_name_to_id.find(component_name) !=
				this->m_component_name_to_id.end(),
			"you forgot to register your component: {}", component_name);

		if (this->HasComponent(id, component_name))
			return nullptr;

		auto hashed_type = this->m_component_name_to_id.at(component_name);

		if (this->HasRequiredComponentsForCreation(id, hashed_type) == false)
			return nullptr;

		auto* p_existed_storage = this->m_registry.storage(hashed_type);

		if (p_existed_storage)
		{
	
			auto status =
				p_existed_storage->push(static_cast<entt::entity>(id));
			
			if (status == p_existed_storage->end())
			{
				KOTEK_MESSAGE(
					"failed to add the component: {}", component_name);

				return nullptr;
			}
			else
			{
				auto p_result =
					p_existed_storage->value(static_cast<entt::entity>(id));

				return p_result;
			}
		}
		else
		{
			auto hash_component_name =
				Kotek::ktk::hash<Kotek::ktk::cstring>{}(component_name);

			if (hash_component_name ==
				zircon_component_actor::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_actor>(id);
			}
			else if (hash_component_name ==

				zircon_component_camera::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_camera>(id);
			}
			else if (hash_component_name ==

				zircon_component_geometry::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_geometry>(id);
			}
			else if (hash_component_name ==
				zircon_component_input::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_input>(id);
			}
			else if (hash_component_name ==

				zircon_component_transform::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_transform>(id);
			}
			else if (hash_component_name ==

				zircon_component_visibility::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_visibility>(id);
			}
			else if (hash_component_name ==

				zircon_component_sdk_scene_name::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_sdk_scene_name>(id);
			}
			else if (hash_component_name ==
				zircon_component_ui_camera::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_ui_camera>(id);
			}
			else if (hash_component_name ==
				zircon_component_sdk_input::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_sdk_input>(id);
			}
			else if (hash_component_name ==
				zircon_component_sdk_camera::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_sdk_camera>(id);
			}
			else if (hash_component_name ==
				zircon_component_frustum::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_frustum>(id);
			}
			else if (hash_component_name ==
				zircon_component_bounding_sphere::GetComponentNameHash())
			{
				this->CreateComponent<zircon_component_bounding_sphere>(id);
			}
			else
			{
				KOTEK_ASSERT(false,
					"can't be you forgot to update this if "
					"statement (in case where you added a new "
					"component)");
			}
		}

		return this->GetComponentByName(id, component_name);
	}

	Kotek::ktk::json::value SerializeComponentByNameToJSON(
		entt::entity id,
		const Kotek::ktk::cstring& component_name) noexcept
	{
		Kotek::ktk::json::value result;

		KOTEK_ASSERT(component_name.empty() == false,
			"you can't pass an empty string here");

		void* p_data = this->GetComponentByName(id, component_name);

		if (p_data)
		{
			zircon_component_interface* p_component =
				static_cast<zircon_component_interface*>(p_data);

			result = p_component->Serialize();
		}

		return result;
	}

	Kotek::ktk::json::value SerializeComponentByNameToJSON(
		entt::entity id, const char* p_component_name,
		unsigned char* p_raw_memory,
		Kotek::ktk::size_t raw_memory_size) noexcept
	{
		Kotek::ktk::json::value result;

		KOTEK_ASSERT(p_component_name, "invalid string");
		KOTEK_ASSERT(strlen(p_component_name), "empty string");
		KOTEK_ASSERT(p_raw_memory, "invalid part of memory!");
		KOTEK_ASSERT(raw_memory_size, "can't be zero!");

		void* p_data = this->GetComponentByName(id, p_component_name);

		if (p_data)
		{
			zircon_component_interface* p_component =
				static_cast<zircon_component_interface*>(p_data);

			result = p_component->Serialize(p_raw_memory, raw_memory_size);
		}

		return result;
	}

	template <typename ComponentType>
	void RemoveComponent(entt::entity id) noexcept
	{


		if (this->m_registry.valid(static_cast<entt::entity>(id)))
		{
			if (this->HasComponent<ComponentType>(id))
			{
				this->m_registry.erase<ComponentType>(
					static_cast<entt::entity>(id));
			}
		}
	}

	void RemoveComponentByName(entt::entity id,
		const Kotek::ktk::cstring& component_name) noexcept
	{
		KOTEK_ASSERT(component_name.empty() == false,
			"you can't pass an empty string here");



		if (this->HasComponent(id, component_name))
		{
			auto hashed_type = this->m_component_name_to_id.at(component_name);

			auto* p_existed_storage = this->m_registry.storage(hashed_type);

			if (p_existed_storage)
			{
				if (p_existed_storage->contains(static_cast<entt::entity>(id)))
				{
					p_existed_storage->erase(static_cast<entt::entity>(id));
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
			this->m_registry.destroy(static_cast<entt::entity>(id));

			return true;
		}

		return false;
	}

	const Kotek::ktk::unordered_map<Kotek::ktk::cstring, entt::id_type>&
	GetRegisteredComponents(void) const noexcept
	{
		return this->m_component_name_to_id;
	}

	void DeserializeComponent(void* p_raw_data,
		const Kotek::ktk::json::value& serialized_data) noexcept
	{
		KOTEK_ASSERT(p_raw_data,
			"you can't pass an invalid component here; Also you "
			"must pass a component created from storage!");

		zircon_component_interface* p_component =
			static_cast<zircon_component_interface*>(p_raw_data);

		p_component->Deserialize(serialized_data);
	}

	void CreateAllComponents(entt::entity entity_id,
		const Kotek::ktk::vector<Kotek::ktk::pair<Kotek::ktk::cstring,
			Kotek::ktk::json::value>>& serialized_components) noexcept
	{
		if (serialized_components.empty() == false)
		{
			for (const auto& [component_name, serialized_data] :
				serialized_components)
			{
				KOTEK_ASSERT(component_name.empty() == false,
					"can't be, you must got a valid string that "
					"represents your component for creation, see "
					"GetAllComponentsOfEntity method. Something is "
					"wrong! Data is corrupted");

				auto p_raw_data = this->CreateComponentByName(
					entity_id, component_name.c_str());

				this->DeserializeComponent(p_raw_data, serialized_data);
			}
		}
	}

	const entt::registry& GetRegistry(void) const noexcept
	{
		return this->m_registry;
	}

	entt::registry& GetRegistry(void) noexcept { return this->m_registry; }

	Kotek::ktk::vector<
		Kotek::ktk::pair<Kotek::ktk::cstring, Kotek::ktk::json::value>>
	GetAllComponentsOfEntity(entt::entity entity_id) noexcept
	{
		Kotek::ktk::vector<
			Kotek::ktk::pair<Kotek::ktk::cstring, Kotek::ktk::json::value>>
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
						this->m_component_id_to_name.at(component_id);

					KOTEK_ASSERT(component_name.empty() == false,
						"you can't register a component that has "
						"empty name! (See preprocessor otherwise "
						"you implemented own method that doesn't "
						"have a valid string)");

					auto p_raw_data =
						this->GetComponentByName(entity_id, component_name);

					if (p_raw_data)
					{
						auto* p_casted =
							static_cast<zircon_component_interface*>(
								p_raw_data);

						if (p_casted)
						{
							result.push_back(
								{component_name, p_casted->Serialize()});
						}
					}
				}
			}
		}

		return result;
	}

private:
	template <typename ComponentType>
	Kotek::ktk::cstring GetComponentTypeName(void) const noexcept
	{
#ifdef KOTEK_PLATFORM_WINDOWS
		return typeid(ComponentType).raw_name();
#elif KOTEK_PLATFORM_LINUX
		return typeid(ComponentType).name();
#endif
	}

	void register_components();
	void register_components_restrictions();

	void register_components_game();
	void register_components_sdk();

	void register_components_restrictions_game();
	void register_components_restrictions_sdk();

	void validate_components_restrictions();

private:
	zircon_config* m_p_config;
	kotek::core::ktkConsole* m_p_console;
	kotek::ktk::unordered_map<kotek::ktk::cstring, entt::id_type>
		m_component_name_to_id;
	kotek::ktk::unordered_map<entt::id_type, kotek::ktk::cstring>
		m_component_id_to_name;

	// for each component (if it is needed) you specify hash types
	// of what components it depends. For example
	// component_ui_camera will be created if component_camera
	// exists in entity.
	kotek::ktk::unordered_map<kotek::ktk::cstring,
		Kotek::ktk::vector<entt::id_type>>
		m_component_creation_restriction_by_component_name;

	kotek::ktk::unordered_map<entt::id_type,
		kotek::ktk::vector<entt::id_type>>
		m_component_creation_restriction_by_hash;

	kotek::ktk::mt::mutex m_mutex;

	entt::registry m_registry;
};