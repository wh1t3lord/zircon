#include "zircon_cancel_arbiter.h"

zircon_cancel_arbiter::zircon_cancel_arbiter(void) : m_consumers{}
{
}

zircon_cancel_arbiter::~zircon_cancel_arbiter(void)
{
}

bool zircon_cancel_arbiter::register_consumer(
	const zircon_cancel_consumer_t& consumer) noexcept
{
	KOTEK_ASSERT(consumer.pfn_is_active,
		"a cancel consumer without an is_active probe can never fire — "
		"pass both probes");
	KOTEK_ASSERT(consumer.pfn_dismiss,
		"a cancel consumer without a dismiss probe can not consume — "
		"pass both probes");

	if (consumer.pfn_is_active == nullptr ||
		consumer.pfn_dismiss == nullptr)
	{
		return false;
	}

	if (this->m_consumers.size() >= this->m_consumers.capacity())
	{
		// loud but never fatal (the same capacity-guard shape as the
		// pass-library manager): an assert here would make the guard
		// untestable — the unit suite drives exactly this overflow
		KOTEK_MESSAGE_ERROR(
			"[cancel]: the arbiter registry is full ({} consumers) — "
			"raise ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS",
			this->m_consumers.capacity());
		return false;
	}

	this->m_consumers.push_back(consumer);

	// keep the registry sorted by priority: one bubble pass from the
	// back over at most 16 PODs — cheaper than any ordered structure
	for (kotek::size_t index = this->m_consumers.size() - 1;
	     index > 0 &&
	     this->m_consumers[index].priority <
		     this->m_consumers[index - 1].priority;
	     --index)
	{
		zircon_cancel_consumer_t temporary = this->m_consumers[index - 1];
		this->m_consumers[index - 1] = this->m_consumers[index];
		this->m_consumers[index] = temporary;
	}

	return true;
}

bool zircon_cancel_arbiter::handle_cancel(
	const char** p_out_consumed_debug_name) noexcept
{
	if (p_out_consumed_debug_name)
	{
		*p_out_consumed_debug_name = nullptr;
	}

	for (auto& consumer : this->m_consumers)
	{
		if (consumer.pfn_is_active(consumer.p_owner) == false)
		{
			continue;
		}

		// exactly one dismissal per event: the first ACTIVE consumer's
		// dismiss runs and the event ends here, whatever dismiss
		// returns — a consumer that claimed active is expected to
		// consume (its false is reported to the caller, the event is
		// still spent)
		if (p_out_consumed_debug_name)
		{
			*p_out_consumed_debug_name = consumer.p_debug_name;
		}

		return consumer.pfn_dismiss(consumer.p_owner);
	}

	return false;
}

void zircon_cancel_arbiter::clear(void) noexcept
{
	this->m_consumers.clear();
}

kotek::uint8_t zircon_cancel_arbiter::get_consumer_count(
	void) const noexcept
{
	return static_cast<kotek::uint8_t>(this->m_consumers.size());
}
