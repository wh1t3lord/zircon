#pragma once

#include "zircon_command_definitions.h"

class zircon_scene_manager;

class zircon_command_history : public Kotek::Core::ktkISDKCommandHistoryManager
{
public:
	zircon_command_history(void);
	~zircon_command_history(void);

	void initialize(Kotek::Core::ktkIFileSystem* p_filesystem,
		zircon_scene_manager* p_scene_manager,
		Kotek::Core::ktkIResourceManager* p_resource_manager);
	void shutdown(void);

	void ExecuteCommand(Kotek::Core::ktkISDKRedoUndo* p_command) override;

	void Undo() override;
	void Redo() override;

	void set_changed(bool status) noexcept;
	bool is_changed() const noexcept;

	const Kotek::ktk::array<Kotek::Core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
	GetCommands(void) const noexcept;

	void update_dependent_commands(Kotek::ktk::entity_t id_what_will_be_deleted,
		Kotek::ktk::entity_t id_that_replaces_what_will_be_deleted) noexcept;

	unsigned char* allocate_memory_for_command(
		Kotek::ktk::size_t size_of_class) noexcept;

private:

	void unload_content();
	void unload_content_before();
	void unload_content_after(bool is_need_to_reopen);
	// данный метод вызывается дважды чтобы вставить контент которые относится к блоку "до" и "после"
	// очевидно что блок "после" идет после того как сериализируются текущий фрейм
	void insert_content(Kotek::ktk::size_t before_offset, Kotek::ktk::size_t after_offset, Kotek::ktk::size_t cursor_offset_current);
	bool is_contain_control_character(const char* p_buffer, Kotek::ktk::size_t& how_much_time_control_character_repeats, Kotek::ktk::size_t size_of_buffer, char control_character = '\n');

	ktk_filesystem_path get_full_path_of_file(const char* filename_with_extension);

	void reopen_current_file(Kotek::ktk::uint32_t file_id);
	void reopen_exchange_file(Kotek::ktk::uint32_t file_id);

private:
	bool m_is_changed;
	bool m_is_first_serialize_happened;
	Kotek::ktk::uint32_t m_file_resource_handle_id;
	Kotek::ktk::uint32_t m_file_exchange_resource_handle_id;
	Kotek::Core::ktkIFileSystem* m_p_filesystem;
	Kotek::Core::ktkIResourceManager* m_p_resource_manager;
	zircon_scene_manager* m_p_scene_manager;
	Kotek::ktk::size_t m_index;
	Kotek::ktk::ptrdiff_t m_cursor_index;
	// last time max value
	// todo: update this value when we deleted commands because of undo/redo
	// thing
	Kotek::ktk::size_t m_max_index;
	Kotek::ktk::size_t m_file_index;
	Kotek::ktk::size_t m_current_file_offset;
	// показывает оффсет когда сделали переход на предыдущий фрейм за счет undo
	// чтобы когда редо делаем мы не пробегались по всему файлу а сразу переместились когда начинали считывание на предыдущий фрейм чтобы сразу же начать двигаться дальше, эдакая оптимизация
	Kotek::ktk::size_t m_after_frame_file_offset;
	// показывает оффсет когда мы сделали undo на том месте что когда делаем redo мы перезаписали результат undo
	Kotek::ktk::size_t m_before_frame_file_offset;
	Kotek::ktk::size_t m_end_of_previous_frame;
	Kotek::ktk::size_t m_start_of_next_frame;
	// конец данных "после" фрейма, он же определяет конец before блока
	/*
		после - означают данные которые выгружались после текущего фрейма 
		до - означает данные которые шли перед текущим фреймом 

		Допустим у нас 30 команд, один фрейм у нас 10 команд итого 3 фрейма
		Теперь когда мы находимся на втором фрейма с 9 по 19 мы имеем блок командо "до" и "после"

		Таким образом файл exchange хранит информацию о "до" и "после" чтобы вставлять в текущий файл
	*/
	Kotek::ktk::size_t m_exchange_file_offset_after;
	// статистика о созданных на текущий момент команд, todo: стоит учитывать факт удаления команд когда мы сделали действие то есть тупо сделали 10 команд делали несколько андо потом действие тут затирается информация факт затирания информации есть ее потеря то есть удаление команд поэтому мы должны вычитать то сколько мы удалили, поэтому эта переменная показывает текущее количество команд
	Kotek::ktk::size_t m_current_amount_of_created_commands;
	Kotek::ktk::array<Kotek::Core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_commands;
	Kotek::ktk::static_cstring<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
		m_path_to_streaming_folder;
	unsigned char
		m_p_memory_for_stack_parser[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];
	Kotek::ktk::array<unsigned char[zircon_DEF_MAXIMUM_COMMAND_SIZE],
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_storage;
};
