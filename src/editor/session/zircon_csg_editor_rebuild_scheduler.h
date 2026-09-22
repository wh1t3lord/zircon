#pragma once

// zircon_csg_editor_rebuild_scheduler — the editor CSG compound
// rebuild scheduler (task Z25 A2). The journaled primitive commands
// mark the compound dirty (the component's u8 flag); this scheduler
// watches the flag per frame and schedules ONE evaluation per edit
// burst, on a dedicated worker thread, so the frame loop never
// blocks on a merge:
//
//   - COMMAND BOUNDARY: when the caller reports the history moved
//     since the last update (a journaled batch completed — a button
//     click, an undo), every watched-but-unsent dirty compound is
//     scheduled IMMEDIATELY (the action is complete, there is
//     nothing mid-drag to wait for).
//   - INPUT QUIET: a dirty compound seen WITHOUT a history move is
//     a live/direct edit (a drag preview mutating the component
//     outside the journal); it is scheduled once the clock has been
//     quiet for ZIRCON_DEF_CSG_EDITOR_REBUILD_DEBOUNCE_MS, and each
//     re-dirty inside the window resets the timer (one rebuild for
//     the whole burst).
//
// A compound already queued/in-flight that gets edited again raises
// a re-dirty flag: when its current result is consumed, one more
// debounced rebuild follows — a rapid command burst (a slider
// issuing a command per frame) coalesces into an immediate rebuild
// for the first command and a single trailing rebuild for the rest.
//
// The evaluation itself runs on ONE worker thread (own bounded queue
// — the resource manager's B3 worker is a private text-load queue of
// a fixed request shape, not a general job queue; riding it would
// churn the resource manager's contract for zero gain, so the CSG
// queue replicates the B3 discipline: bounded, single consumer,
// forward-only). The job snapshot (the primitive descriptors) is
// taken on the scheduling thread; the evaluation context (~6 MB) is
// heap-allocated and reused; the completed mesh lands in ONE
// completion slot — the consumer (the editor CSG pass, render
// thread) checks it out by view and returns it; an un-consumed slot
// BACKPRESSURES the worker (waits), which is safe because the queue
// only ever holds the latest state worth drawing.
//
// The clock is INJECTABLE (fnptr + owner, the house seam): tests
// advance a counter, production uses the steady clock. Wrap-safe
// elapsed math (signed 32-bit difference) — a u32 millisecond clock
// wraps every ~49 days and monotonicity is all the debounce needs.
//
// Threading: the tracked-compound table, the job queue and the
// completion slot share ONE mutex; the worker evaluates WITHOUT the
// mutex (the world snapshot lives in the job, not in the world);
// commands and update() run on the main thread, the consumer runs on
// the render thread — every cross-thread touch is a short critical
// section, never a nested lock.

#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.containers.multithreading.thread/include/kotek_core_containers_multithreading_thread.h>
#include <kotek.core.containers.multithreading.mutex/include/kotek_core_containers_multithreading_mutex.h>
#include <kotek.core.containers.multithreading.condition_variable/include/kotek_core_containers_multithreading_condition_variable.h>

#include "../../ecs/zircon_csg_evaluate.h"

class zircon_factory;
struct zircon_ecs_context_t;

// the input-quiet debounce of one edit burst (named per the
// memory-budget rule; the plan's ~150 ms, tunable)
#define ZIRCON_DEF_CSG_EDITOR_REBUILD_DEBOUNCE_MS 150

// the dirty-watch table: the editor's live compound count is far
// below this; overflow is a loud drop (the compounds past the cap
// simply wait for a free slot), never a growth
#define ZIRCON_DEF_CSG_EDITOR_MAX_TRACKED_COMPOUNDS 64

// the rebuild job queue: a burst of edits on MANY compounds at once
// is bounded by the tracked-table size; overflow coalesces into the
// re-dirty path (loud, never a growth)
#define ZIRCON_DEF_CSG_EDITOR_REBUILD_QUEUE_SIZE 8

// the per-frame compound scan bound (the sdk camera lives among the
// first entities; a world hiding compounds past the cap waits, same
// loud-drop class as the tracked-table cap)
#define ZIRCON_DEF_CSG_EDITOR_MAX_ENTITY_SCAN_COUNT 1024

// the completed-rebuild payload view handed to the consumer (the
// editor CSG pass): every pointer aliases scheduler-owned storage
// that stays stable from pop_completed until return_completed —
// never copied (a full compound mesh is ~2.3 MB; the checkout
// contract is cheaper). Positions are the WELDED mesh positions;
// normals are per TRIANGLE; indices are 3 per triangle addressing
// the position array
struct zircon_csg_rebuild_result_view_t
{
	kotek::uint64_t m_compound_id;
	kotek::uint32_t m_status; // eZirconCsgEvaluationStatus
	kotek::uint32_t m_position_count;
	kotek::uint32_t m_triangle_count;
	kotek::uint32_t m_sliver_drop_count;
	kotek::uint32_t m_capacity_drop_count;
	const zircon_csg_vec3_t<zircon_csg_scalar_t>* m_p_positions;
	const zircon_csg_vec3_t<zircon_csg_scalar_t>* m_p_normals;
	const kotek::uint32_t* m_p_indices;
};

class zircon_csg_editor_rebuild_scheduler
{
public:
	// the millisecond clock seam (injectable for the tests — they
	// advance a counter, no sleeping); nullptr = the steady clock
	using now_ms_fn_t = kotek::uint32_t (*)(void* p_owner);

	zircon_csg_editor_rebuild_scheduler(void);
	~zircon_csg_editor_rebuild_scheduler(void);

	// starts the worker thread (unless the synchronous test mode was
	// requested before initialize) and allocates the evaluation
	// context + the completion slot (heap — the ~6 MB fixture rule)
	void initialize(
		now_ms_fn_t pfn_now_ms = nullptr,
		void* p_now_ms_owner = nullptr
	);
	// joins the worker (the join is the happens-before edge that
	// makes the context/slot teardown safe)
	void shutdown(void);

	// the per-frame drive (main thread): watches the dirty flags,
	// applies the two triggers, enqueues the snapshots
	void update(zircon_factory* p_factory,
		zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_count_max_limit,
		kotek::uint64_t history_epoch) noexcept;

	// the consumer (render thread): non-blocking checkout of the
	// completed slot; false = nothing ready. The view aliases the
	// slot until return_completed
	bool pop_completed(
		zircon_csg_rebuild_result_view_t& out_view
	) noexcept;
	// the consumer's ack for the checkout of compound_id (the view's
	// m_compound_id): the slot may be reused; a re-dirty that arrived
	// while the result was in flight re-arms the debounce
	void return_completed(kotek::uint64_t compound_id) noexcept;

	// the blocking variant for shutdown drains and the worker
	// handoff test: waits up to timeout_ms for a completion
	bool wait_pop_completed(
		zircon_csg_rebuild_result_view_t& out_view,
		kotek::uint32_t timeout_ms
	) noexcept;

	// tests: execute scheduled jobs inline on the calling thread (the
	// full schedule -> evaluate -> complete path, minus the thread);
	// must be set BEFORE initialize
	void set_synchronous_execution(bool status) noexcept;

	// diagnostics (test-pinned)
	kotek::uint32_t get_scheduled_count(void) const noexcept;
	kotek::uint32_t get_completed_count(void) const noexcept;
	kotek::uint32_t get_pending_count(void) const noexcept;
	kotek::uint32_t get_tracked_count(void) const noexcept;
	bool is_worker_running(void) const noexcept;

private:
	struct rebuild_job_t
	{
		kotek::uint64_t m_compound_id;
		kotek::uint32_t m_primitive_count;
		zircon_csg_primitive_desc_t<zircon_csg_scalar_t>
			m_primitives[ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND];
	};

	struct result_slot_t
	{
		kotek::static_vector_t<
			zircon_csg_vec3_t<zircon_csg_scalar_t>,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
			m_positions;
		kotek::static_vector_t<
			zircon_csg_vec3_t<zircon_csg_scalar_t>,
			ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION>
			m_normals;
		kotek::static_vector_t<
			kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION>
			m_indices;
	};

	struct tracked_compound_t
	{
		kotek::uint64_t m_compound_id;
		kotek::uint32_t m_last_dirty_ms;
		kotek::uint8_t m_state;   // eTrackedState
		kotek::uint8_t m_redirty;
		// the dirty GENERATION last observed (the component bumps a
		// u8 counter per mark_dirty — a level flag could not express
		// "edited again while still dirty"; raw inequality is
		// wrap-safe)
		kotek::uint8_t m_seen_dirty;
	};

	enum eTrackedState : kotek::uint8_t
	{
		kIdle = 0,    // in the table, not waiting for anything
		kWaiting,     // dirty observed, the debounce clock runs
		kDispatched,  // queued or in flight — edits become re-dirty
	};

private:
	void worker_main(void);
	kotek::uint32_t now_ms(void) const noexcept;
	void fill_view_locked(
		zircon_csg_rebuild_result_view_t& out_view
	) const noexcept;

	// snapshots the compound's member list into a job and enqueues
	// it (the caller holds the mutex); the compound's dirty flag is
	// cleared — further edits re-dirty and re-enter the cycle
	void dispatch_locked(
		zircon_factory* p_factory,
		zircon_ecs_context_t* p_context,
		kotek::uint64_t compound_id
	) noexcept;

	// the synchronous-test-mode drain: evaluates the queued jobs
	// inline on the calling thread (the caller holds the mutex; the
	// worker thread does not exist in this mode)
	void drain_queue_synchronous_locked(void) noexcept;

private:
	mutable kotek::mt::mutex_t m_mutex;
	kotek::mt::condition_variable_t m_queue_cv;
	kotek::mt::condition_variable_t m_slot_cv;
	kotek::mt::thread_t m_worker;

	kotek::static_vector_t<tracked_compound_t,
		ZIRCON_DEF_CSG_EDITOR_MAX_TRACKED_COMPOUNDS>
		m_tracked;
	kotek::static_vector_t<rebuild_job_t,
		ZIRCON_DEF_CSG_EDITOR_REBUILD_QUEUE_SIZE>
		m_queue;

	// heap-only (the ~6 MB evaluation context + the ~2.3 MB slot —
	// never members by value)
	zircon_csg_evaluation_t<zircon_csg_scalar_t>* m_p_evaluation;
	result_slot_t* m_p_slot;

	// the completion slot's metadata (the slot itself holds the
	// arrays only); written by the producer, read by fill_view_locked
	kotek::uint64_t m_slot_compound_id;
	kotek::uint32_t m_slot_status;
	kotek::uint32_t m_slot_sliver_drops;
	kotek::uint32_t m_slot_capacity_drops;

	now_ms_fn_t m_pfn_now_ms;
	void* m_p_now_ms_owner;

	kotek::uint64_t m_last_history_epoch;
	kotek::uint32_t m_scheduled_count;
	kotek::uint32_t m_completed_count;
	bool m_is_epoch_baselined;
	bool m_is_slot_full;
	bool m_is_slot_checked_out;
	bool m_is_stop_requested;
	bool m_is_synchronous;
	bool m_is_initialized;
};
