#pragma once

// amount of live command objects that the history keeps in RAM for
// instant undo/redo of the most recent actions; older commands are
// reconstructed from the journal on demand
#define zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE 64

/// the editor-facing command-history contract. Its consumers (commands,
/// windows and UI tests) link only zircon.core; the concrete journal stays
/// in zircon.editor.commands and the session owns one instance.
class zircon_editor_command_history_interface
	: public kotek::core::ktkISDKCommandHistoryManager
{
public:
	virtual ~zircon_editor_command_history_interface(void) {}

	virtual unsigned char* allocate_memory_for_command(
		kotek::size_t size_of_class,
		const char* p_debug_type_name) noexcept = 0;

	virtual kotek::core::ktkISDKRedoUndo* get_command_at_slot(
		kotek::uint8_t slot_index) const noexcept = 0;

	virtual kotek::ptrdiff_t get_cursor_index(void) const = 0;
	virtual kotek::uint64_t get_total_recorded_commands(void
	) const noexcept = 0;
	virtual kotek::entity_t get_live_entity_id(
		kotek::entity_t recorded_id) const noexcept = 0;
};
