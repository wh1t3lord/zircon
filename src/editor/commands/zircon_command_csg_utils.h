#pragma once

// zircon_command_csg_utils.h — the shared glue of the CSG primitive
// commands (task Z25 A2): every command of the family resolves the
// session -> world -> factory chain, translates the recorded compound
// entity id through the history's reincarnation map (the Z6 chain —
// a journal-reconstructed command only knows its recorded id) and
// marks the COMPOUND dirty. The dirty flag is the rebuild trigger —
// the commands never touch the rebuild machinery itself (the
// scheduler polls the flag, task Z25 A2), so Execute/Undo/Redo stay
// journaled-world mutations only.

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_command_history.h"

/// @brief \~english resolves the current session's world +
/// factory; false (loud) when the chain is broken — the caller
/// then skips its mutation, exactly like the built-in commands
inline bool zircon_command_csg_resolve_world(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_session_editor** p_out_session,
	zircon_world** p_out_world,
	zircon_factory** p_out_factory
) noexcept
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	if (p_manager_session_editor == nullptr)
		return false;

	zircon_session_editor* p_session =
		p_manager_session_editor->get_session(
			p_manager_session_editor->get_current_session_id());

	if (p_session == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] failed to resolve the current editor session #{}",
			p_manager_session_editor->get_current_session_id());
		return false;
	}

	zircon_world* p_world = p_session->get_world();

	if (p_world == nullptr || p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] the session's world is not ready");
		return false;
	}

	zircon_factory* p_factory = p_world->get_factory();

	if (p_factory == nullptr)
	{
		KOTEK_MESSAGE_WARNING("[csg] the world has no factory");
		return false;
	}

	if (p_out_session)
		*p_out_session = p_session;
	if (p_out_world)
		*p_out_world = p_world;
	if (p_out_factory)
		*p_out_factory = p_factory;

	return true;
}

/// @brief \~english the compound component for a (possibly recorded)
/// entity id — the id is translated through the history's
/// reincarnation chain first; nullptr (loud) when the compound is
/// gone, which is a valid transient state during undo/redo replay
/// (the caller keeps its own mutation but skips the member-list and
/// dirty steps)
inline zircon_component_csg* zircon_command_csg_resolve_compound(
	zircon_session_editor* p_session,
	zircon_factory* p_factory,
	zircon_ecs_context_t* p_context,
	kotek::entity_t compound_id
) noexcept
{
	KOTEK_ASSERT(p_session, "must pass a valid session");
	KOTEK_ASSERT(p_factory, "must pass a valid factory");
	KOTEK_ASSERT(p_context, "must pass a valid ecs context");

	if (p_session == nullptr || p_factory == nullptr ||
		p_context == nullptr)
	{
		return nullptr;
	}

	const kotek::entity_t live_id =
		p_session->get_command_history()->get_live_entity_id(
			compound_id);

	if (p_factory->is_valid_entity(p_context, live_id) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] compound entity {} is not live, the member/"
			"dirty step is skipped",
			static_cast<kotek::uint32_t>(live_id.id));
		return nullptr;
	}

	return static_cast<zircon_component_csg*>(
		p_factory->get_component_by_enum(
			p_context,
			live_id,
			eZirconComponentType::kzircon_component_csg));
}
