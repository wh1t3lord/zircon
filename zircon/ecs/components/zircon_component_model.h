#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// TODO: delete it because you need to specify real backends
// like texture and shader
class ktkComponentModel
{
	//		KOTEK_COMPONENT(ktkComponentModel)

public:
	ktkComponentModel(void);
	~ktkComponentModel(void);

	void Clear(void) noexcept;
	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept;
};