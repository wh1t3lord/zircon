#include "zircon_csg_editor_rebuild_scheduler.h"

#include <kotek.core.types.numerics/include/kotek_std_alias_numerics.h>

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../ecs/zircon_component_csg_primitive.h"

namespace
{
	// the production clock: the steady clock through the kotek chrono
	// alias (wrap-safe elapsed math lives at the call site)
	kotek::uint32_t zircon_csg_scheduler_steady_now_ms(void*) noexcept
	{
		return static_cast<kotek::uint32_t>(
			kotek::chrono::duration_cast<kotek::chrono::milliseconds>(
				kotek::chrono::steady_clock::now().time_since_epoch())
				.count());
	}

	// wrap-safe elapsed comparison: true when now - last >= interval
	// for a u32 millisecond clock (the signed difference absorbs the
	// ~49-day wrap — monotonicity is all the debounce needs)
	bool zircon_csg_scheduler_elapsed(
		kotek::uint32_t now, kotek::uint32_t last,
		kotek::uint32_t interval_ms) noexcept
	{
		return static_cast<kotek::int32_t>(now - last) >=
			static_cast<kotek::int32_t>(interval_ms);
	}
} // namespace

zircon_csg_editor_rebuild_scheduler::zircon_csg_editor_rebuild_scheduler(
	void) :
	m_pfn_now_ms{nullptr},
	m_p_now_ms_owner{nullptr},
	m_slot_compound_id{0},
	m_slot_status{0},
	m_slot_sliver_drops{0},
	m_slot_capacity_drops{0},
	m_last_history_epoch{},
	m_scheduled_count{0},
	m_completed_count{0},
	m_is_epoch_baselined{false},
	m_is_slot_full{false},
	m_is_slot_checked_out{false},
	m_is_stop_requested{false},
	m_is_synchronous{false},
	m_is_initialized{false}
{
	// heap-only per the ~6 MB evaluation-context fixture rule
	this->m_p_evaluation = new zircon_csg_evaluation_t<
		zircon_csg_scalar_t>();
	this->m_p_slot = new result_slot_t();
}

zircon_csg_editor_rebuild_scheduler::~zircon_csg_editor_rebuild_scheduler(
	void)
{
	KOTEK_ASSERT(this->m_is_initialized == false,
		"you forgot to call shutdown!");

	delete this->m_p_slot;
	delete this->m_p_evaluation;
}

void zircon_csg_editor_rebuild_scheduler::initialize(
	now_ms_fn_t pfn_now_ms,
	void* p_now_ms_owner)
{
	KOTEK_ASSERT(this->m_is_initialized == false,
		"double initialize — shutdown first");

	this->m_pfn_now_ms = pfn_now_ms ?
		pfn_now_ms :
		&zircon_csg_scheduler_steady_now_ms;
	this->m_p_now_ms_owner = p_now_ms_owner;

	this->m_is_stop_requested = false;

	if (this->m_is_synchronous == false)
	{
		this->m_worker = kotek::mt::thread_t(
			&zircon_csg_editor_rebuild_scheduler::worker_main, this);
	}

	this->m_is_initialized = true;
}

void zircon_csg_editor_rebuild_scheduler::shutdown(void)
{
	if (this->m_is_initialized == false)
		return;

	{
		kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
		this->m_is_stop_requested = true;
	}

	this->m_queue_cv.notify_all();
	this->m_slot_cv.notify_all();

	if (this->m_worker.joinable())
		this->m_worker.join();

	{
		kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
		this->m_tracked.clear();
		this->m_queue.clear();
		this->m_is_slot_full = false;
		this->m_is_slot_checked_out = false;
		this->m_scheduled_count = 0;
		this->m_completed_count = 0;
		this->m_last_history_epoch = 0;
		this->m_is_epoch_baselined = false;
		this->m_slot_compound_id = 0;
	}

	this->m_is_initialized = false;
}

void zircon_csg_editor_rebuild_scheduler::update(
	zircon_factory* p_factory,
	zircon_ecs_context_t* p_context,
	kotek::uint32_t entity_count_max_limit,
	kotek::uint64_t history_epoch) noexcept
{
	if (p_factory == nullptr || p_context == nullptr)
		return;

	const kotek::uint32_t now = this->now_ms();

	kotek::entity_t entities[ZIRCON_DEF_CSG_EDITOR_MAX_ENTITY_SCAN_COUNT];

	const kotek::uint32_t entity_count = p_factory->get_all_entities(
		p_context, entity_count_max_limit, entities,
		ZIRCON_DEF_CSG_EDITOR_MAX_ENTITY_SCAN_COUNT);

	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);

	const bool is_batch_boundary = this->m_is_epoch_baselined &&
		history_epoch != this->m_last_history_epoch;
	this->m_last_history_epoch = history_epoch;
	this->m_is_epoch_baselined = true;

	// the full scan happened only when the world fit the scan cap —
	// past the cap the "not seen" verdict is unknown, so the
	// validity-based reclaim below is skipped (documented loud-drop
	// class)
	const bool is_full_scan =
		entity_count < ZIRCON_DEF_CSG_EDITOR_MAX_ENTITY_SCAN_COUNT;

	// pass 1: watch the dirty flags
	for (kotek::uint32_t entity_index = 0; entity_index < entity_count;
		 ++entity_index)
	{
		if (p_factory->has_component(p_context, entities[entity_index],
				eZirconComponentType::kzircon_component_csg) == false)
		{
			continue;
		}

		zircon_component_csg* p_compound =
			static_cast<zircon_component_csg*>(
				p_factory->get_component_by_enum(p_context,
					entities[entity_index],
					eZirconComponentType::kzircon_component_csg));

		if (p_compound == nullptr)
			continue;

		const kotek::uint8_t dirty_generation = p_compound->is_dirty();

		if (dirty_generation == 0)
			continue;

		const kotek::uint64_t compound_id = entities[entity_index].id;

		kotek::uint32_t tracked_index = 0;

		for (; tracked_index < this->m_tracked.size(); ++tracked_index)
		{
			if (this->m_tracked[tracked_index].m_compound_id ==
				compound_id)
				break;
		}

		if (tracked_index == this->m_tracked.size())
		{
			if (this->m_tracked.full())
			{
				KOTEK_MESSAGE_WARNING(
					"[csg] the rebuild watch table is full ({}), "
					"compound {} waits for a free slot",
					ZIRCON_DEF_CSG_EDITOR_MAX_TRACKED_COMPOUNDS,
					static_cast<kotek::uint32_t>(compound_id));
				continue;
			}

			tracked_compound_t tracked{};
			tracked.m_compound_id = compound_id;
			tracked.m_last_dirty_ms = now;
			tracked.m_state = static_cast<kotek::uint8_t>(
				eTrackedState::kWaiting);
			tracked.m_seen_dirty = dirty_generation;
			this->m_tracked.push_back(tracked);
			continue;
		}

		tracked_compound_t& tracked = this->m_tracked[tracked_index];

		switch (static_cast<eTrackedState>(tracked.m_state))
		{
		case eTrackedState::kIdle:
		{
			// a clean compound edited again: the debounce clock
			// restarts from this observation
			tracked.m_last_dirty_ms = now;
			tracked.m_seen_dirty = dirty_generation;
			tracked.m_state = static_cast<kotek::uint8_t>(
				eTrackedState::kWaiting);
			break;
		}
		case eTrackedState::kWaiting:
		{
			// the SAME still-set flag must NOT reset the timer (the
			// flag is a level — every update would restart the
			// debounce forever); only a NEW generation (a re-edit
			// while dirty) resets it — one rebuild for the burst
			if (tracked.m_seen_dirty != dirty_generation)
			{
				tracked.m_last_dirty_ms = now;
				tracked.m_seen_dirty = dirty_generation;
			}
			break;
		}
		case eTrackedState::kDispatched:
		{
			// an edit while the previous result is still queued or
			// in flight: the consumed result would be stale — one
			// more debounced rebuild follows the consumption
			tracked.m_redirty = 1;
			tracked.m_seen_dirty = dirty_generation;
			break;
		}
		default:
			break;
		}
	}

	// pass 2: the two triggers
	for (kotek::uint32_t tracked_index = 0;
		 tracked_index < this->m_tracked.size(); ++tracked_index)
	{
		tracked_compound_t& tracked = this->m_tracked[tracked_index];

		if (static_cast<eTrackedState>(tracked.m_state) !=
			eTrackedState::kWaiting)
			continue;

		const bool is_quiet = zircon_csg_scheduler_elapsed(now,
			tracked.m_last_dirty_ms,
			ZIRCON_DEF_CSG_EDITOR_REBUILD_DEBOUNCE_MS);

		if (is_batch_boundary == false && is_quiet == false)
			continue;

		this->dispatch_locked(p_factory, p_context,
			tracked.m_compound_id);

		tracked.m_state = static_cast<kotek::uint8_t>(
			eTrackedState::kDispatched);
	}

	// pass 3: reclaim the watch slots of dead compounds (a compound
	// the world no longer reports and the scan was complete for)
	if (is_full_scan)
	{
		for (kotek::uint32_t tracked_index = this->m_tracked.size();
			 tracked_index > 0; --tracked_index)
		{
			tracked_compound_t& tracked =
				this->m_tracked[tracked_index - 1];

			if (static_cast<eTrackedState>(tracked.m_state) ==
				eTrackedState::kDispatched)
				continue;

			bool is_seen = false;

			for (kotek::uint32_t entity_index = 0;
				 entity_index < entity_count; ++entity_index)
			{
				if (entities[entity_index].id ==
					tracked.m_compound_id)
				{
					is_seen = true;
					break;
				}
			}

			if (is_seen || p_factory->is_valid_entity(p_context,
					kotek::entity_t{tracked.m_compound_id}))
				continue;

			this->m_tracked.erase(this->m_tracked.begin() +
				(static_cast<kotek::ptrdiff_t>(tracked_index) - 1));
		}
	}

	// the synchronous test mode drains the queue inline (single
	// thread — no worker exists in this mode)
	if (this->m_is_synchronous)
		this->drain_queue_synchronous_locked();
}

void zircon_csg_editor_rebuild_scheduler::dispatch_locked(
	zircon_factory* p_factory,
	zircon_ecs_context_t* p_context,
	kotek::uint64_t compound_id) noexcept
{
	// coalesce: a queued job for the same compound already carries
	// the latest state worth drawing (the member list is snapshotted
	// at dispatch; an edit inside the queue window becomes the
	// re-dirty path)
	for (const rebuild_job_t& job : this->m_queue)
	{
		if (job.m_compound_id == compound_id)
			return;
	}

	if (this->m_queue.full())
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] the rebuild queue is full ({}), compound {} is "
			"dropped (the next edit re-triggers it)",
			ZIRCON_DEF_CSG_EDITOR_REBUILD_QUEUE_SIZE,
			static_cast<kotek::uint32_t>(compound_id));
		return;
	}

	zircon_component_csg* p_compound =
		static_cast<zircon_component_csg*>(
			p_factory->get_component_by_enum(p_context,
				kotek::entity_t{compound_id},
				eZirconComponentType::kzircon_component_csg));

	if (p_compound == nullptr)
		return;

	rebuild_job_t job{};
	job.m_compound_id = compound_id;

	const auto& members = p_compound->get_primitive_entities();

	kotek::uint32_t out_count = 0;

	for (kotek::uint32_t member_index = 0;
		 member_index < members.size(); ++member_index)
	{
		const kotek::entity_t primitive_id = members[member_index];

		zircon_component_csg_primitive* p_primitive =
			static_cast<zircon_component_csg_primitive*>(
				p_factory->get_component_by_enum(p_context,
					primitive_id,
					eZirconComponentType::
						kzircon_component_csg_primitive));

		if (p_primitive == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg] compound {} lists a missing primitive {}, "
				"the rebuild skips it",
				static_cast<kotek::uint32_t>(compound_id),
				static_cast<kotek::uint32_t>(primitive_id.id));
			continue;
		}

		p_primitive->fill_desc(job.m_primitives[out_count]);
		++out_count;
	}

	job.m_primitive_count = out_count;

	// the dirty flag is cleared at dispatch: the job snapshot is the
	// source of truth from here; further edits re-dirty and re-enter
	// the cycle
	p_compound->clear_dirty();

	this->m_queue.push_back(job);
	++this->m_scheduled_count;

	if (this->m_is_synchronous == false)
		this->m_queue_cv.notify_all();
}

void zircon_csg_editor_rebuild_scheduler::
	drain_queue_synchronous_locked(void) noexcept
{
	while (this->m_queue.empty() == false)
	{
		// backpressure: the consumer has not returned the previous
		// result — leave the job queued; the next update retries
		if (this->m_is_slot_full)
			return;

		rebuild_job_t job = this->m_queue.front();
		this->m_queue.erase(this->m_queue.begin());

		const eZirconCsgEvaluationStatus status =
			zircon_csg_evaluate_compound(job.m_primitives,
				job.m_primitive_count, *this->m_p_evaluation);

		result_slot_t& slot = *this->m_p_slot;

		slot.m_positions.assign(
			this->m_p_evaluation->m_mesh.m_positions.begin(),
			this->m_p_evaluation->m_mesh.m_positions.end());
		slot.m_normals.assign(
			this->m_p_evaluation->m_mesh.m_normals.begin(),
			this->m_p_evaluation->m_mesh.m_normals.end());
		slot.m_indices.assign(
			this->m_p_evaluation->m_mesh.m_indices.begin(),
			this->m_p_evaluation->m_mesh.m_indices.end());

		this->m_slot_compound_id = job.m_compound_id;
		this->m_slot_status = static_cast<kotek::uint32_t>(status);
		this->m_slot_sliver_drops =
			this->m_p_evaluation->m_sliver_drop_count;
		this->m_slot_capacity_drops =
			this->m_p_evaluation->m_capacity_drop_count;

		this->m_is_slot_full = true;
		++this->m_completed_count;
	}
}

bool zircon_csg_editor_rebuild_scheduler::pop_completed(
	zircon_csg_rebuild_result_view_t& out_view) noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);

	if (this->m_is_slot_full == false || this->m_is_slot_checked_out)
		return false;

	this->fill_view_locked(out_view);
	this->m_is_slot_checked_out = true;
	return true;
}

bool zircon_csg_editor_rebuild_scheduler::wait_pop_completed(
	zircon_csg_rebuild_result_view_t& out_view,
	kotek::uint32_t timeout_ms) noexcept
{
	kotek::mt::unique_lock_t<kotek::mt::mutex_t> lock(this->m_mutex);

	if (this->m_is_slot_full == false)
	{
		this->m_slot_cv.wait_for(lock,
			kotek::chrono::milliseconds(timeout_ms), [this](void) {
				return this->m_is_slot_full;
			});
	}

	if (this->m_is_slot_full == false || this->m_is_slot_checked_out)
		return false;

	this->fill_view_locked(out_view);
	this->m_is_slot_checked_out = true;
	return true;
}

void zircon_csg_editor_rebuild_scheduler::return_completed(
	kotek::uint64_t compound_id) noexcept
{
	const kotek::uint32_t now = this->now_ms();

	{
		kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(
			this->m_mutex);

		if (this->m_is_slot_checked_out == false)
			return;

		this->m_is_slot_full = false;
		this->m_is_slot_checked_out = false;

		for (tracked_compound_t& tracked : this->m_tracked)
		{
			if (tracked.m_compound_id != compound_id)
				continue;

			if (tracked.m_redirty)
			{
				tracked.m_redirty = 0;
				tracked.m_last_dirty_ms = now;
				tracked.m_state = static_cast<kotek::uint8_t>(
					eTrackedState::kWaiting);
			}
			else
			{
				tracked.m_state = static_cast<kotek::uint8_t>(
					eTrackedState::kIdle);
			}
			break;
		}
	}

	this->m_slot_cv.notify_all();
}

void zircon_csg_editor_rebuild_scheduler::set_synchronous_execution(
	bool status) noexcept
{
	this->m_is_synchronous = status;
}

kotek::uint32_t
zircon_csg_editor_rebuild_scheduler::get_scheduled_count(
	void) const noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
	return this->m_scheduled_count;
}

kotek::uint32_t
zircon_csg_editor_rebuild_scheduler::get_completed_count(
	void) const noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
	return this->m_completed_count;
}

kotek::uint32_t
zircon_csg_editor_rebuild_scheduler::get_pending_count(
	void) const noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);

	kotek::uint32_t count = 0;

	for (const tracked_compound_t& tracked : this->m_tracked)
	{
		if (static_cast<eTrackedState>(tracked.m_state) ==
			eTrackedState::kWaiting)
			++count;
	}

	return count;
}

kotek::uint32_t
zircon_csg_editor_rebuild_scheduler::get_tracked_count(
	void) const noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
	return this->m_tracked.size();
}

bool zircon_csg_editor_rebuild_scheduler::is_worker_running(
	void) const noexcept
{
	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(this->m_mutex);
	return this->m_is_initialized && this->m_is_synchronous == false;
}

kotek::uint32_t zircon_csg_editor_rebuild_scheduler::now_ms(
	void) const noexcept
{
	return this->m_pfn_now_ms ?
		this->m_pfn_now_ms(this->m_p_now_ms_owner) :
		0;
}

void zircon_csg_editor_rebuild_scheduler::fill_view_locked(
	zircon_csg_rebuild_result_view_t& out_view) const noexcept
{
	const result_slot_t& slot = *this->m_p_slot;

	out_view.m_compound_id = this->m_slot_compound_id;
	out_view.m_status = this->m_slot_status;
	out_view.m_position_count = slot.m_positions.size();
	out_view.m_triangle_count = slot.m_normals.size();
	out_view.m_sliver_drop_count = this->m_slot_sliver_drops;
	out_view.m_capacity_drop_count = this->m_slot_capacity_drops;
	out_view.m_p_positions = slot.m_positions.data();
	out_view.m_p_normals = slot.m_normals.data();
	out_view.m_p_indices = slot.m_indices.data();
}

void zircon_csg_editor_rebuild_scheduler::worker_main(void)
{
	kotek::mt::unique_lock_t<kotek::mt::mutex_t> lock(this->m_mutex);

	while (this->m_is_stop_requested == false)
	{
		if (this->m_queue.empty())
		{
			this->m_queue_cv.wait(lock);
			continue;
		}

		rebuild_job_t job = this->m_queue.front();
		this->m_queue.erase(this->m_queue.begin());

		// backpressure: ONE completion slot — an un-consumed result
		// waits (the frame thread is the only consumer; a stalled
		// frame stalls the rebuilds, never the other way)
		while (this->m_is_slot_full &&
			this->m_is_stop_requested == false)
		{
			this->m_slot_cv.wait(lock);
		}

		if (this->m_is_stop_requested)
			break;

		lock.unlock();

		const eZirconCsgEvaluationStatus status =
			zircon_csg_evaluate_compound(job.m_primitives,
				job.m_primitive_count, *this->m_p_evaluation);

		lock.lock();

		// shutdown may have arrived while evaluating
		if (this->m_is_stop_requested)
			break;

		result_slot_t& slot = *this->m_p_slot;

		slot.m_positions.assign(
			this->m_p_evaluation->m_mesh.m_positions.begin(),
			this->m_p_evaluation->m_mesh.m_positions.end());
		slot.m_normals.assign(
			this->m_p_evaluation->m_mesh.m_normals.begin(),
			this->m_p_evaluation->m_mesh.m_normals.end());
		slot.m_indices.assign(
			this->m_p_evaluation->m_mesh.m_indices.begin(),
			this->m_p_evaluation->m_mesh.m_indices.end());

		this->m_slot_compound_id = job.m_compound_id;
		this->m_slot_status = static_cast<kotek::uint32_t>(status);
		this->m_slot_sliver_drops =
			this->m_p_evaluation->m_sliver_drop_count;
		this->m_slot_capacity_drops =
			this->m_p_evaluation->m_capacity_drop_count;

		this->m_is_slot_full = true;
		++this->m_completed_count;

		this->m_slot_cv.notify_all();
	}
}
