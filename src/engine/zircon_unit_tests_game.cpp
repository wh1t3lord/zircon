#include "zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "zircon_resource_manager.h"

		#if ZIRCON_DEF_UNIT_TEST_RESOURCE_MANAGER == 1

// note: gtest defines for each thread/task own stack size so it
// is hard to use full stack and if you have a really low RAM it
// is better to disable/enable tests related to resource
// manager, by default it is unreal to test resource manager
// using only stack allocation since it exceeds 1Mb size :c
TEST(Zircon_Game, ResourceManagerCtorDtor)
{
	zircon_resource_manager* p_resource_manager =
		new zircon_resource_manager();
	delete p_resource_manager;
}

TEST(Zircon_Game, ResourceManagerInitShutdown)
{
	kotek::core::ktkFrameworkConfig cfg;

	kotek::core::ktkFileSystem fs;

	fs.Initialize(&cfg);

	kotek::core::ktkMainManager main_manager;

	main_manager.Set_FileSystem(&fs);
	main_manager.Set_FrameworkConfig(&cfg);

	zircon_resource_manager* instance =
		new zircon_resource_manager();
	instance->initialize(&main_manager);
	instance->shutdown();

	fs.Shutdown();

	delete instance;
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceNoCache)
{
	kotek::core::ktkFrameworkConfig cfg;

	kotek::core::ktkFileSystem fs;

	fs.Initialize(&cfg);

	kotek::static_path_t test_path;
	fs.Make_Path(
		test_path,
		kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests
	);

	test_path /= "rsltrnc.json";

	constexpr const char _kContent[] =
		R"({"test_name": "ResourceManagerLoadTextResourceNoCache"})";

	// note: sizeof on a char ARRAY is the payload length + the NUL —
	// this used to be a const char* and wrote 7 bytes of json (never
	// observed: the pre-B3 load only allocated, never read)
	bool fs_status = fs.Write_File(
		test_path, _kContent, sizeof(_kContent) - 1
	);

	KOTEK_ASSERT(
		fs_status, "failed to write file by path: {}", test_path
	);

	kotek::core::ktkMainManager main_manager;

	main_manager.Set_FileSystem(&fs);
	main_manager.Set_FrameworkConfig(&cfg);

	zircon_resource_manager* p_rm =
		new zircon_resource_manager();

	p_rm->initialize(&main_manager);

	kotek::shared_ptr_t<zircon_resource_t> result =
		p_rm->load(test_path, eZirconResourceLoadingFlags::kSync);

	// B3: the kText branch does REAL IO through the streaming API now —
	// the load must land the parsed json in the view, not just allocate
	ASSERT_TRUE(result.get() != nullptr);

	const zircon_resource_desc_t* p_desc =
		p_rm->get_desc(result->desc_id);

	ASSERT_TRUE(p_desc != nullptr);
	EXPECT_TRUE(p_desc->is_loaded);
	EXPECT_TRUE(p_desc->type == eZirconResourceType::kText);

	const zircon_view_handle_t* p_view =
		p_rm->get_view(result->view_id);

	ASSERT_TRUE(p_view != nullptr);
	ASSERT_TRUE(p_view->p_view != nullptr);

	const auto* p_text_view =
		static_cast<const kotek::core::ktkResourceViewText*>(
			p_view->p_view
		);

	EXPECT_TRUE(p_text_view->Is_KeyExist("test_name"));

	const auto loaded_value =
		p_text_view->Get<kotek::static_cstring_t<96>>("test_name");

	EXPECT_TRUE(loaded_value == "ResourceManagerLoadTextResourceNoCache");

	// the handle's control block lives in the manager's own pmr arena —
	// drop it BEFORE the manager dies (the pre-B3 test discarded the
	// returned shared_ptr at the semicolon for the same reason)
	result.reset();

	p_rm->shutdown();

	fs.Shutdown();

	delete p_rm;
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceThroughWorkerStream)
{
	kotek::core::ktkFrameworkConfig cfg;

	kotek::core::ktkFileSystem fs;

	fs.Initialize(&cfg);

	kotek::static_path_t test_path;
	fs.Make_Path(
		test_path,
		kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests
	);

	test_path /= "rmworkerstream.json";

	// ~3 KB of json: big enough that the parsed DOM can only exist if
	// the whole file really streamed through the worker's chunked read
	char content[3072];

	int used = snprintf(
		content, sizeof(content),
		R"({"test_name": "ResourceManagerLoadTextResourceThroughWorkerStream", "payload": ")"
	);

	ASSERT_TRUE(used > 0 && used < 128);

	constexpr int kPayloadLength = 2800;

	for (int i = 0; i < kPayloadLength; ++i)
		content[used + i] = static_cast<char>('a' + (i % 26));

	used += kPayloadLength;

	content[used++] = '"';
	content[used++] = '}';
	content[used] = '\0';

	ASSERT_TRUE(
		fs.Write_File(test_path, content, static_cast<size_t>(used))
	);

	kotek::core::ktkMainManager main_manager;

	main_manager.Set_FileSystem(&fs);
	main_manager.Set_FrameworkConfig(&cfg);

	zircon_resource_manager* p_rm =
		new zircon_resource_manager();

	p_rm->initialize(&main_manager);

	// the ASYNC path: the worker thread performs the real streamed IO
	kotek::shared_ptr_t<zircon_resource_t> result =
		p_rm->load(test_path, eZirconResourceLoadingFlags::kAsync);

	ASSERT_TRUE(result.get() != nullptr);
	ASSERT_TRUE(result->desc_id != _kZirconInvalidResourceID);
	ASSERT_TRUE(result->view_id != _kZirconInvalidResourceID);

	// bounded wait for the worker (the same plain-flag spin pattern as
	// the manager's own worker handshake)
	bool is_loaded = false;

	for (kotek::uint32_t spin = 0; spin < 100000000; ++spin)
	{
		const zircon_resource_desc_t* p_desc =
			p_rm->get_desc(result->desc_id);

		if (p_desc && p_desc->is_loaded)
		{
			is_loaded = true;
			break;
		}
	}

	ASSERT_TRUE(is_loaded);

	// the content through the worker's streamed read: the DOM carries
	// both fields with the exact bytes
	const zircon_view_handle_t* p_view =
		p_rm->get_view(result->view_id);

	ASSERT_TRUE(p_view != nullptr);
	ASSERT_TRUE(p_view->p_view != nullptr);

	const auto* p_text_view =
		static_cast<const kotek::core::ktkResourceViewText*>(
			p_view->p_view
		);

	ASSERT_TRUE(p_text_view->Is_KeyExist("test_name"));
	ASSERT_TRUE(p_text_view->Is_KeyExist("payload"));

	const auto loaded_name =
		p_text_view->Get<kotek::static_cstring_t<96>>("test_name");

	EXPECT_TRUE(
		loaded_name == "ResourceManagerLoadTextResourceThroughWorkerStream"
	);

	const auto loaded_payload =
		p_text_view->Get<kotek::static_cstring_t<4096>>("payload");

	EXPECT_TRUE(
		loaded_payload.size() ==
		static_cast<size_t>(kPayloadLength)
	);

	bool payload_matches = loaded_payload.size() ==
		static_cast<size_t>(kPayloadLength);

	for (int i = 0;
	     payload_matches && i < kPayloadLength; ++i)
	{
		if (loaded_payload[i] != static_cast<char>('a' + (i % 26)))
			payload_matches = false;
	}

	EXPECT_TRUE(payload_matches);

	// the direct one-shot read of the same file agrees byte-for-byte
	// (the worker's streamed content matches the one-shot contract)
	kotek::uint8_t oneshot[3072 + 16];
	kotek::uint8_t* p_oneshot = oneshot;
	kotek::size_t oneshot_size = sizeof(oneshot);

	ASSERT_TRUE(fs.Read_File(test_path, p_oneshot, oneshot_size));
	EXPECT_TRUE(oneshot_size == static_cast<size_t>(used));

	bool oneshot_matches =
		oneshot_size == static_cast<size_t>(used);

	for (int i = 0; oneshot_matches && i < used; ++i)
	{
		if (oneshot[i] != static_cast<kotek::uint8_t>(content[i]))
			oneshot_matches = false;
	}

	EXPECT_TRUE(oneshot_matches);

	// the handle's control block lives in the manager's own pmr arena —
	// drop it BEFORE the manager dies
	result.reset();

	p_rm->shutdown();

	fs.Shutdown();

	delete p_rm;
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceCached) {}

TEST(Zircon_Game, ResourceManagerLoadTextResourceCacheAndUnload)
{
}

TEST(
	Zircon_Game,
	ResourceManagerLoadTextResourceCacheAndUnloadAndDestroy
)
{
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceMultithreading)
{
}

TEST(Zircon_Game, ResourceManagerLoadTextureResource) {}

TEST(Zircon_Game, ResourceManagerLoadSoundResource) {}

TEST(Zircon_Game, ResourceManagerLoad3DModel) {}

TEST(Zircon_Game, ResourceManagerLoad3DModelAnimation) {}

TEST(Zircon_Game, ResourceManagerLoadUI) {}

		#endif

void zircon_register_unit_tests_game() {}
	#endif
#endif