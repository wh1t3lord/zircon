#pragma once

/// the cancel-consumer registry is tiny and fixed by design (task Z19):
/// the editor registers 4 today (gizmo drag, popup/modal, text input,
/// selection — the drag-drop payload slot 30 stays reserved, see the
/// session's registration) and the game layer adds a handful later — 16
/// is ~3x headroom, raise on demand (memory-budget rule 9)
#define ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS 16

/// consumer priorities (lower runs earlier) as named defines — a new
/// consumer picks a free slot; the gaps leave room for later insertions
/// without renumbering
#define ZIRCON_DEF_CANCEL_ARBITER_PRIORITY_GIZMO_DRAG 10
#define ZIRCON_DEF_CANCEL_ARBITER_PRIORITY_POPUP_MODAL 20
/// reserved: drag-drop payload dismiss needs a ClearDragDrop-style entry
/// on ktkIImguiWrapper, which does not exist — the consumer lands with
/// the wrapper addition, the priority slot stays fixed
#define ZIRCON_DEF_CANCEL_ARBITER_PRIORITY_DRAG_DROP_PAYLOAD 30
#define ZIRCON_DEF_CANCEL_ARBITER_PRIORITY_TEXT_INPUT 40
#define ZIRCON_DEF_CANCEL_ARBITER_PRIORITY_SELECTION 50

/// one probe of the consumer seam (fnptr+void*, the same seam shape as
/// the pass-library manager — no std::function, no statics): is_active is
/// polled at event time and reports whether the consumer's transient
/// state is live right now (a modal open, a drag in progress, a
/// selection present); dismiss asks the consumer to end that state and
/// returns true = the event was consumed
using zircon_cancel_probe_pfn_t = bool (*)(void* p_owner);

/// the arbiter's consumer entry — plain POD, the owner pointer carries
/// the consumer's state (the session's ui_state, the main manager, ...)
struct zircon_cancel_consumer_t
{
	zircon_cancel_probe_pfn_t pfn_is_active;
	zircon_cancel_probe_pfn_t pfn_dismiss;
	void* p_owner;
	kotek::uint8_t priority;
	const char* p_debug_name;
};

/// @brief \~english the ESC/cancel arbiter (task Z19, owner decision
/// 2026-09-03: ESC dismisses transient state only, ordinary windows are
/// NEVER closed by it). Consumers are registered ONCE at session init;
/// handle_cancel polls them in priority order and lets the FIRST ACTIVE
/// one dismiss — activeness is derived from the real state at event time,
/// so there is no push/pop stack to drift out of sync (the classic
/// escape-stack failure). The class is UI-backend-free: it consumes a
/// semantic cancel event, never a key — the key source lives in the
/// caller's adapter (phase 1: the editor imgui pass's OnUpdate; phase 2:
/// kotek.core.input, adapter-only change). One arbiter per session, like
/// its command history. Zero allocation, zero dynamic containers.
class zircon_cancel_arbiter
{
public:
	zircon_cancel_arbiter(void);
	~zircon_cancel_arbiter(void);

	/// inserts the consumer keeping the registry sorted by priority
	/// (N<=16, a bubble from the back is the whole sort). False — with a
	/// loud error, never an assert — when the registry is full, so the
	/// capacity guard is testable; asserts on null probes (programmer
	/// error at init time)
	bool register_consumer(
		const zircon_cancel_consumer_t& consumer) noexcept;

	/// the semantic cancel event: the first active consumer (priority
	/// order) dismisses and the event ends there — exactly ONE dismissal
	/// per event, ESC never falls through to the next consumer. Returns
	/// false when nothing was active (the caller just continues working).
	/// p_out_consumed_debug_name (optional) receives the fired consumer's
	/// debug name for the caller's log line
	bool handle_cancel(
		const char** p_out_consumed_debug_name = nullptr) noexcept;

	/// session shutdown drops every registration (the owners die with
	/// the session — a stale entry would probe freed state)
	void clear(void) noexcept;

	/// test/introspection surface: the live registration count
	kotek::uint8_t get_consumer_count(void) const noexcept;

private:
	/// kept sorted by priority on insert (rule 2: lookup-table-on-vector
	/// over a handful of entries, never a map)
	kotek::static_vector_t<zircon_cancel_consumer_t,
		ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS>
		m_consumers;
};
