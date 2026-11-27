#include "zircon_command_text_state.h"

#include "../../ecs/zircon_factory.h"

bool zircon_command_text_state::capture(
	zircon_component_interface* p_component) noexcept
{
	if (p_component == nullptr)
		return false;

	kotek::array_t<char, zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON> buffer{};
	const kotek::size_t text_size = zircon_serialize_component_to_buffer(
		p_component, buffer.data(), buffer.size());

	if (text_size == 0)
		return false;

	this->m_text.assign(buffer.data(), text_size);
	return true;
}

bool zircon_command_text_state::assign_text(
	const kotek::cstring_view_t& text,
	kotek::ktk::json::error_code& parse_error) noexcept
{
	if (text.empty())
		return false;

	const kotek::ktk::json::value parsed =
		kotek::ktk::json::parse(text, parse_error);

	if (parse_error)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to parse a serialized component state: {}",
			parse_error.message());
		return false;
	}

	this->m_text.assign(text.data(), text.size());
	return true;
}

bool zircon_command_text_state::assign_json(
	const kotek::ktk::json::value& state) noexcept
{
	const auto text = kotek::ktk::json::serialize(state);
	if (text.size() >= this->m_text.capacity())
		return false;

	this->m_text.assign(text.data(), text.size());
	return true;
}

bool zircon_command_text_state::apply(zircon_factory* p_factory,
	zircon_component_interface* p_component) const noexcept
{
	if (p_factory == nullptr || p_component == nullptr ||
		this->m_text.empty())
	{
		return false;
	}

	kotek::ktk::json::error_code parse_error;
	const kotek::ktk::json::value parsed = kotek::ktk::json::parse(
		kotek::cstring_view_t(this->m_text.c_str(), this->m_text.size()),
		parse_error);

	if (parse_error)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to parse a serialized component state: {}",
			parse_error.message());
		return false;
	}

	p_factory->DeserializeComponent(p_component, parsed);
	return true;
}

bool zircon_command_text_state::create_component(zircon_factory* p_factory,
	zircon_ecs_context_t* p_context, kotek::entity_t id,
	eZirconComponentType type) const noexcept
{
	if (p_factory == nullptr || p_context == nullptr)
		return false;

	p_factory->create_component(p_context, id, type);

	zircon_component_interface* p_component =
		p_factory->get_component_by_enum(p_context, id, type);

	return this->apply(p_factory, p_component);
}

const char* zircon_command_text_state::c_str(void) const noexcept
{
	return this->m_text.c_str();
}

kotek::cstring_view_t zircon_command_text_state::text(void) const noexcept
{
	return kotek::cstring_view_t(this->m_text.c_str(), this->m_text.size());
}

kotek::size_t zircon_command_text_state::size(void) const noexcept
{
	return this->m_text.size();
}

bool zircon_command_text_state::empty(void) const noexcept
{
	return this->m_text.empty();
}
