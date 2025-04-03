#pragma once

#include "zircon_command_definitions.h"

class zircon_world;
class zircon_factory;
enum zircon_component_type_t;

class zircon_editor_command_history : public kotek::core::ktkISDKCommandHistoryManager
{
public:
	zircon_editor_command_history(void);
	~zircon_editor_command_history(void);

	void initialize(kotek::core::ktkIFileSystem* p_filesystem,
		zircon_world* p_current_world,
		zircon_factory* p_factory_game,
		kotek::core::ktkIResourceManager* p_resource_manager);
	void shutdown(void);

	void ExecuteCommand(kotek::core::ktkISDKRedoUndo* p_command) override;

	void Undo() override;
	void Redo() override;

	void set_changed(bool status) noexcept;
	bool is_changed() const noexcept;

	const kotek::array_t<kotek::core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
	GetCommands(void) const noexcept;

	void update_dependent_commands(entt::entity id_what_will_be_deleted,
		entt::entity id_that_replaces_what_will_be_deleted) noexcept;

	unsigned char* allocate_memory_for_command(
		kotek::size_t size_of_class, const char* p_debug_type_name) noexcept;

	kotek::size_t get_current_index(void) const;
	kotek::ptrdiff_t get_cursor_index(void) const;

	// returns if component was serialized for id otherwise value will be empty
	bool get_serialized_component_by_entity_and_component_type_id(
		kotek::ktk::json::value&
			constructed_value_on_stack_based_on_placement_new_memory,
		entt::entity id, zircon_component_type_t type_id);

private:
	void unload_content();
	void unload_content_before();
	void unload_content_after(bool is_need_to_reopen);
	// данный метод вызывается дважды чтобы вставить контент которые относится к
	// блоку "до" и "после" очевидно что блок "после" идет после того как
	// сериализируются текущий фрейм
	void insert_content(kotek::size_t before_offset, kotek::size_t after_offset,
		kotek::size_t cursor_offset_current);
	void insert_content_exchange(kotek::size_t offset_cursor_exchange,
		kotek::size_t read_until_offset_in_file,
		kotek::size_t cursor_offset_of_file);

	bool is_contain_control_character(const char* p_buffer,
		kotek::size_t& how_much_time_control_character_repeats,
		kotek::size_t size_of_buffer, char control_character = '\n');

	ktk_filesystem_path get_full_path_of_file(
		const char* filename_with_extension);

	kotek::cfstream_t* reopen_current_file(kotek::cfstream_t* p_file);
	kotek::cfstream_t* reopen_exchange_file(kotek::cfstream_t* p_file);

	void clear_content_when_action_issued();
	kotek::size_t get_offset_of_current_index_in_file();
	kotek::size_t get_count_of_commands_in_file(kotek::size_t start_offset);
	void move_content_from_file_to_exchange(
		kotek::size_t start_offset_in_file, kotek::size_t end_offset_in_file);
	void move_content_from_exchange_to_file(
		kotek::size_t start_offset_in_file, kotek::size_t end_offset_in_file);

	void update_dependent_serialized_commands(
		entt::entity id_what_will_be_deleted,
		entt::entity id_that_replaces_what_will_be_deleted);
	bool check_json_entry_has_entity_id(entt::entity id_what_will_be_deleted,
		int real_size_for_json_data, kotek::size_t& current_offset,
		kotek::json::value& json);
	bool check_json_entry_has_entity_id_and_component_type_id(entt::entity id,
		zircon_component_type_t type_id, int real_size_for_json_data,
		kotek::size_t& current_offset, kotek::ktk::json::value& json);

private:
	bool m_is_changed;
	bool m_is_first_serialize_happened;
	bool m_is_action_issued;

	kotek::cfstream_t* m_p_file_temp;
	kotek::cfstream_t* m_p_file_exchange;

	Kotek::Core::ktkIFileSystem* m_p_filesystem;
	Kotek::Core::ktkIResourceManager* m_p_resource_manager;
	zircon_world* m_p_current_world;
	zircon_factory* m_p_factory_game;
	kotek::size_t m_index;
	kotek::ptrdiff_t m_cursor_index;
	// last time max value
	// todo: update this value when we deleted commands because of undo/redo
	// thing
	kotek::size_t m_max_index;
	kotek::size_t m_file_index;
	kotek::size_t m_current_file_offset;
	// показывает оффсет когда сделали переход на предыдущий фрейм за счет undo
	// чтобы когда редо делаем мы не пробегались по всему файлу а сразу
	// переместились когда начинали считывание на предыдущий фрейм чтобы сразу
	// же начать двигаться дальше, эдакая оптимизация
	kotek::size_t m_after_frame_file_offset;
	// показывает оффсет когда мы сделали undo на том месте что когда делаем
	// redo мы перезаписали результат undo
	kotek::size_t m_before_frame_file_offset;
	kotek::size_t m_end_of_previous_frame;
	kotek::size_t m_start_of_next_frame;
	// конец данных "после" фрейма, он же определяет конец before блока
	/*
	    после - означают данные которые выгружались после текущего фрейма
	    до - означает данные которые шли перед текущим фреймом

	    Допустим у нас 30 команд, один фрейм у нас 10 команд итого 3 фрейма
	    Теперь когда мы находимся на втором фрейма с 9 по 19 мы имеем блок
	   командо "до" и "после"

	    Таким образом файл exchange хранит информацию о "до" и "после" чтобы
	   вставлять в текущий файл
	*/
	kotek::size_t m_exchange_file_offset_after;
	// статистика о созданных на текущий момент команд, todo: стоит учитывать
	// факт удаления команд когда мы сделали действие то есть тупо сделали 10
	// команд делали несколько андо потом действие тут затирается информация
	// факт затирания информации есть ее потеря то есть удаление команд поэтому
	// мы должны вычитать то сколько мы удалили, поэтому эта переменная
	// показывает текущее количество команд
	kotek::size_t m_current_amount_of_created_commands;
	kotek::array_t<Kotek::Core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_commands;
	kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
		m_path_to_streaming_folder;
	unsigned char
		m_p_memory_for_stack_parser[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];
	unsigned char
		m_p_memory_for_stack_command_creation[zircon_DEF_MAXIMUM_COMMAND_SIZE];
	kotek::array_t<unsigned char[zircon_DEF_MAXIMUM_COMMAND_SIZE],
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_storage;
};
