#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_sdk_camera.h"
		#include "../../ecs/zircon_component_sdk_input.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../editor/session/zircon_editor_sdk_camera.h"
		#include "../../world/zircon_world.h"

		#include <kotek.core.console/include/kotek_console.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_CAMERA_SDK
			#define ZIRCON_DEF_UNIT_TEST_CAMERA_SDK 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_CAMERA_SDK == 1

// functional proofs for task Z20 (the editor camera chain): the drive
// math of BOTH rotation representations is the exact free-function code
// the session driver runs (zircon_sdk_camera_drive_euler /
// zircon_sdk_camera_drive_quaternion), the bootstrap helper runs against
// a real factory+world, the console marshaling pins the exact-type
// variant contract behind the 2026-09-04 uint32 fix, and the config
// roundtrip proves the new feature key persists

namespace
{
	/// @brief \~english the headless ecs environment (the same shape as
	/// the Z6/Z19 fixtures minus the session): a real factory and world,
	/// no imgui, no window
	struct zircon_test_camera_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);

			this->factory.Initialize(
				&this->engine_config, &this->console, &this->input);

			this->world.initialize("zircon_z20_test_world",
				&this->engine_config, &this->console, &this->input,
				&this->factory, 65536);
		}

		void shutdown(void)
		{
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
			this->filesystem.Shutdown();
		}

		kotek::uint32_t entity_count(void)
		{
			kotek::entity_t entities
				[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};

			return this->factory.get_all_entities(
				this->world.get_ecs_context(),
				this->world.get_entity_count_max_limit(),
				entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);
		}
	};
} // namespace

TEST(Zircon_Editor, CameraSdkEulerQuaternionSameDeltasSameFront)
{
	float yaw = -90.0f;
	float pitch = 0.0f;
	kotek::ktk::math::quatf_t rotation(0.0f, 0.0f, 0.0f, 1.0f);

	// a representative mouse-drag sequence in degrees (already
	// sensitivity-scaled, as the driver passes them)
	const float deltas[][2] = {
		{12.5f, 3.0f},
		{-40.0f, 8.5f},
		{7.25f, -15.0f},
		{130.0f, 0.0f},
		{-3.5f, -44.0f},
		{22.0f, 10.0f},
	};

	for (const auto& delta : deltas)
	{
		kotek::ktk::math::vec3f_t front_euler;
		kotek::ktk::math::vec3f_t front_quat;

		zircon_sdk_camera_drive_euler(
			yaw, pitch, delta[0], delta[1], front_euler);
		zircon_sdk_camera_drive_quaternion(
			rotation, delta[0], delta[1], front_quat);

		EXPECT_NEAR(front_euler.x(), front_quat.x(), 1e-3f);
		EXPECT_NEAR(front_euler.y(), front_quat.y(), 1e-3f);
		EXPECT_NEAR(front_euler.z(), front_quat.z(), 1e-3f);
	}

	// the same fronts must build the same view (the driver feeds both
	// paths into the one look_at)
	const kotek::ktk::math::vec3f_t position(1.0f, 2.0f, 3.0f);

	kotek::ktk::math::vec3f_t front_euler;
	kotek::ktk::math::vec3f_t front_quat;
	zircon_sdk_camera_drive_euler(yaw, pitch, 0.0f, 0.0f, front_euler);
	zircon_sdk_camera_drive_quaternion(
		rotation, 0.0f, 0.0f, front_quat);

	kotek::ktk::math::mat4x4f_t view_euler = kotek::ktk::math::look_at(
		position, position + front_euler,
		kotek::ktk::math::vec3f_t(0.0f, 1.0f, 0.0f));
	kotek::ktk::math::mat4x4f_t view_quat = kotek::ktk::math::look_at(
		position, position + front_quat,
		kotek::ktk::math::vec3f_t(0.0f, 1.0f, 0.0f));

	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(
				view_euler[column][row], view_quat[column][row], 1e-3f);
		}
	}
}

TEST(Zircon_Editor, CameraSdkPitchClampHoldsOnBothPaths)
{
	const float max_front_y = 0.9998477f; // sin(89 deg)

	// far past the pole in one step
	{
		float yaw = -90.0f;
		float pitch = 0.0f;
		kotek::ktk::math::vec3f_t front;

		zircon_sdk_camera_drive_euler(yaw, pitch, 0.0f, 500.0f, front);

		EXPECT_FLOAT_EQ(pitch, ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES);
		EXPECT_NEAR(front.y(), max_front_y, 1e-4f);

		zircon_sdk_camera_drive_euler(yaw, pitch, 0.0f, -1000.0f, front);

		EXPECT_FLOAT_EQ(pitch, -ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES);
		EXPECT_NEAR(front.y(), -max_front_y, 1e-4f);
	}

	// the quaternion path: the clamp shrinks the applied delta so the
	// result lands on the same window edge and moving back works
	// immediately (no stuck-at-the-limit)
	{
		kotek::ktk::math::quatf_t rotation(0.0f, 0.0f, 0.0f, 1.0f);
		kotek::ktk::math::vec3f_t front;

		zircon_sdk_camera_drive_quaternion(rotation, 0.0f, 500.0f, front);

		EXPECT_NEAR(front.y(), max_front_y, 1e-3f);

		// still a pure rotation (normalized) after the clamped step
		const auto& q = rotation;
		EXPECT_NEAR(
			q.x() * q.x() + q.y() * q.y() + q.z() * q.z() + q.w() * q.w(),
			1.0f, 1e-3f);

		zircon_sdk_camera_drive_quaternion(rotation, 0.0f, -1000.0f, front);

		EXPECT_NEAR(front.y(), -max_front_y, 1e-3f);
	}
}

TEST(Zircon_Editor, CameraSdkQuaternionLongDragNoNaNNoFlip)
{
	kotek::ktk::math::quatf_t rotation(0.0f, 0.0f, 0.0f, 1.0f);
	float yaw = -90.0f;
	float pitch = 0.0f;

	const float max_front_y = 0.9998477f; // sin(89 deg)

	kotek::ktk::math::vec3f_t front_quat;
	kotek::ktk::math::vec3f_t front_euler;

	// hundreds of small deltas through straight-up and back: up past
	// the clamp, down past the opposite clamp, then back — every step
	// must stay a finite unit forward inside the pitch window, and both
	// representations must track each other (a flip would diverge)
	for (int phase = 0; phase < 3; ++phase)
	{
		float direction = (phase == 1) ? -1.0f : 1.0f;

		for (int step = 0; step < 200; ++step)
		{
			float delta_yaw = 0.35f * direction;
			float delta_pitch = 1.0f * direction;

			zircon_sdk_camera_drive_euler(
				yaw, pitch, delta_yaw, delta_pitch, front_euler);
			zircon_sdk_camera_drive_quaternion(
				rotation, delta_yaw, delta_pitch, front_quat);

			EXPECT_TRUE(front_quat.x() == front_quat.x());
			EXPECT_TRUE(front_quat.y() == front_quat.y());
			EXPECT_TRUE(front_quat.z() == front_quat.z());

			float length_squared = front_quat.x() * front_quat.x() +
				front_quat.y() * front_quat.y() +
				front_quat.z() * front_quat.z();

			EXPECT_NEAR(length_squared, 1.0f, 1e-3f);
			EXPECT_LE(front_quat.y(), max_front_y + 1e-3f);
			EXPECT_GE(front_quat.y(), -max_front_y - 1e-3f);

			EXPECT_NEAR(front_euler.x(), front_quat.x(), 1e-2f);
			EXPECT_NEAR(front_euler.y(), front_quat.y(), 1e-2f);
			EXPECT_NEAR(front_euler.z(), front_quat.z(), 1e-2f);
		}
	}
}

TEST(Zircon_Editor, SdkBootstrapCreatesExactlyOneEntityAndIsIdempotent)
{
	// heap allocated like every history fixture (the console alone is
	// ~1 MB of stack)
	zircon_test_camera_env& env = *new zircon_test_camera_env();
	env.initialize();

	ASSERT_EQ(env.entity_count(), 0u);

	kotek::entity_t id = zircon_editor_ensure_sdk_bootstrap_entity(
		&env.factory, env.world.get_ecs_context(),
		env.world.get_entity_count_max_limit());

	ASSERT_FALSE(ecs_is_invalid_entity(id));
	EXPECT_EQ(env.entity_count(), 1u);

	// exactly sdk_camera + sdk_input + transform, nothing else
	kotek::uint32_t component_count{};

	for (int component_type = 0;
	     component_type < static_cast<int>(eZirconComponentType::kunknown);
	     ++component_type)
	{
		if (env.factory.has_component(env.world.get_ecs_context(), id,
				static_cast<eZirconComponentType>(component_type)))
		{
			++component_count;
		}
	}

	EXPECT_EQ(component_count, 3u);
	EXPECT_TRUE(env.factory.has_component(env.world.get_ecs_context(), id,
		eZirconComponentType::kzircon_component_sdk_camera));
	EXPECT_TRUE(env.factory.has_component(env.world.get_ecs_context(), id,
		eZirconComponentType::kzircon_component_sdk_input));
	EXPECT_TRUE(env.factory.has_component(env.world.get_ecs_context(), id,
		eZirconComponentType::kzircon_component_transform));

	// the driver-visible vantage (2026-09-04): the bootstrap spawns at the
	// render passes' default orbit — position (4,3,4), yaw=-135, pitch
	// =asin(-3/sqrt(41)) — so a fresh scene shows the grid (a camera at
	// the origin sits ON the grid plane and sees nothing)
	auto* p_camera = static_cast<zircon_component_sdk_camera*>(
		env.factory.get_component_by_enum(env.world.get_ecs_context(), id,
			eZirconComponentType::kzircon_component_sdk_camera));

	ASSERT_NE(p_camera, nullptr);
	EXPECT_FLOAT_EQ(p_camera->get_camera().get_yaw(),
		ZIRCON_DEF_SDK_CAMERA_BOOTSTRAP_YAW_DEGREES);
	EXPECT_FLOAT_EQ(p_camera->get_camera().get_pitch(),
		ZIRCON_DEF_SDK_CAMERA_BOOTSTRAP_PITCH_DEGREES);

	auto* p_transform = static_cast<zircon_component_transform*>(
		env.factory.get_component_by_enum(env.world.get_ecs_context(), id,
			eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);
	EXPECT_FLOAT_EQ(p_transform->get_position().x(),
		ZIRCON_DEF_SDK_CAMERA_BOOTSTRAP_POSITION_X);
	EXPECT_FLOAT_EQ(p_transform->get_position().y(),
		ZIRCON_DEF_SDK_CAMERA_BOOTSTRAP_POSITION_Y);
	EXPECT_FLOAT_EQ(p_transform->get_position().z(),
		ZIRCON_DEF_SDK_CAMERA_BOOTSTRAP_POSITION_Z);

	// both rotation representations must aim at the origin from the
	// vantage: forward == normalize(-4,-3,-4) in euler AND in quat (the
	// quat was synced through the driver's own recurrence)
	float yaw = p_camera->get_camera().get_yaw();
	float pitch = p_camera->get_camera().get_pitch();
	kotek::ktk::math::vec3f_t front_euler;

	zircon_sdk_camera_drive_euler(yaw, pitch, 0.0f, 0.0f, front_euler);

	kotek::ktk::math::quatf_t rotation =
		p_camera->get_camera().get_rotation_quaternion();
	kotek::ktk::math::vec3f_t front_quat;

	zircon_sdk_camera_drive_quaternion(rotation, 0.0f, 0.0f, front_quat);

	constexpr float _kInvSqrt41 = 1.0f / 6.40312423743284869f;

	EXPECT_NEAR(front_euler.x(), -4.0f * _kInvSqrt41, 1e-3f);
	EXPECT_NEAR(front_euler.y(), -3.0f * _kInvSqrt41, 1e-3f);
	EXPECT_NEAR(front_euler.z(), -4.0f * _kInvSqrt41, 1e-3f);

	EXPECT_NEAR(front_quat.x(), front_euler.x(), 1e-3f);
	EXPECT_NEAR(front_quat.y(), front_euler.y(), 1e-3f);
	EXPECT_NEAR(front_quat.z(), front_euler.z(), 1e-3f);

	// the second call is a no-op returning the same entity
	kotek::entity_t id_second =
		zircon_editor_ensure_sdk_bootstrap_entity(&env.factory,
			env.world.get_ecs_context(),
			env.world.get_entity_count_max_limit());

	EXPECT_TRUE(id_second.id == id.id);
	EXPECT_EQ(env.entity_count(), 1u);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Editor, SdkBootstrapPreExistingCameraIsNoOp)
{
	zircon_test_camera_env& env = *new zircon_test_camera_env();
	env.initialize();

	// a world that already has a camera (e.g. a scene that loaded its
	// own) — the helper must not create a second one nor complete the
	// set on it
	kotek::entity_t id_existing =
		env.factory.create_entity(env.world.get_ecs_context());

	ASSERT_FALSE(ecs_is_invalid_entity(id_existing));

	EXPECT_TRUE(env.factory.create_component(env.world.get_ecs_context(),
		id_existing, eZirconComponentType::kzircon_component_sdk_camera));

	kotek::entity_t id_result =
		zircon_editor_ensure_sdk_bootstrap_entity(&env.factory,
			env.world.get_ecs_context(),
			env.world.get_entity_count_max_limit());

	EXPECT_TRUE(id_result.id == id_existing.id);
	EXPECT_EQ(env.entity_count(), 1u);
	EXPECT_FALSE(env.factory.has_component(env.world.get_ecs_context(),
		id_existing, eZirconComponentType::kzircon_component_sdk_input));
	EXPECT_FALSE(env.factory.has_component(env.world.get_ecs_context(),
		id_existing, eZirconComponentType::kzircon_component_transform));

	env.shutdown();
	delete &env;
}

TEST(Zircon_Editor, ComponentCameraQuaternionJsonRoundTrip)
{
	// pins the new field's serialization (both tag_invokes) with a
	// non-symmetric quat — a component swap in value_from/value_to would
	// fail this
	zircon_component_camera camera;
	camera.set_yaw(12.5f);
	camera.set_pitch(-30.0f);
	camera.set_rotation_quaternion(
		kotek::ktk::math::quatf_t(0.1f, -0.2f, 0.3f, 0.9f));

	kotek::ktk::json::value serialized =
		kotek::ktk::json::value_from(camera);

	zircon_component_camera restored =
		kotek::ktk::json::value_to<zircon_component_camera>(serialized);

	EXPECT_FLOAT_EQ(restored.get_yaw(), 12.5f);
	EXPECT_FLOAT_EQ(restored.get_pitch(), -30.0f);

	const auto& rotation = restored.get_rotation_quaternion();
	EXPECT_FLOAT_EQ(rotation.x(), 0.1f);
	EXPECT_FLOAT_EQ(rotation.y(), -0.2f);
	EXPECT_FLOAT_EQ(rotation.z(), 0.3f);
	EXPECT_FLOAT_EQ(rotation.w(), 0.9f);
}

TEST(Zircon_Game, ConsoleEntityIdUint32ArgInvokesTheCommand)
{
	// the 2026-09-04 marshaling contract: the five editor entity
	// commands take uint32 (what every call site passes and the only
	// thing the text parser can produce) — drive a REAL console through
	// Register_Command + Execute_Command with the house-convention
	// signature
	// heap allocated like every console-touching fixture (the console
	// alone is ~1 MB of stack)
	kotek::core::ktkConsole* p_console = new kotek::core::ktkConsole();

	bool ran{};
	kotek::uint32_t received{};

	p_console->Register_Command(
		[&](const char* p_name, kotek::uint32_t entity) -> bool
		{
			ran = true;
			received = entity;
			return true;
		},
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_DeleteEntity));

	kotek::ktk::console_command_base_t args;
	args.push_back(kotek::ktk::console_command_variant_t{
		static_cast<const char*>("zircon_component_transform")});
	args.push_back(
		kotek::ktk::console_command_variant_t{kotek::uint32_t{42}});

	p_console->Execute_Command(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_DeleteEntity),
		args);

	EXPECT_TRUE(ran);
	EXPECT_EQ(received, 42u);

	// the console owns the registered vfunctions — Shutdown deallocates
	// them (the dtor asserts on a non-empty storage)
	p_console->Shutdown();
	delete p_console;
}

TEST(Zircon_Game, ConsoleEntityIdVariantValidationIsExactType)
{
	// the regression pin behind the fix: check_args (through the
	// vfunction the console stores) validates the variant alternatives
	// EXACTLY per position — a (const char*, kotek::entity_t) command
	// does NOT validate against {const char*, uint32} args (the PICO-era
	// signatures made every UI button a silent no-op), the uint32
	// signature against the SAME args does. Driven through the vfunction
	// directly because ktkConsole::Execute_Command asserts on the false
	// result the mismatch produces — the assert is the release-note
	// behavior, the silent skip is what this pins
	bool ran_entity{};

	// named locals: vfunction's ctor takes the callable by non-const
	// lvalue reference, so make_vfunction cannot bind a prvalue lambda
	auto lambda_entity =
		[&](const char* p_name, kotek::entity_t entity) -> bool
		{
			ran_entity = true;
			return true;
		};

	auto command_entity =
		kotek::ktk::make_vfunction<kotek::ktk::console_command_variant_t>(
			lambda_entity);

	kotek::ktk::console_command_base_t args;
	args.push_back(kotek::ktk::console_command_variant_t{
		static_cast<const char*>("zircon_component_transform")});
	args.push_back(
		kotek::ktk::console_command_variant_t{kotek::uint32_t{42}});

	EXPECT_FALSE(command_entity(args));
	EXPECT_FALSE(ran_entity);

	bool ran_uint{};
	kotek::uint32_t received_uint{};

	auto lambda_uint32 =
		[&](const char* p_name, kotek::uint32_t entity) -> bool
		{
			ran_uint = true;
			received_uint = entity;
			return true;
		};

	auto command_uint32 =
		kotek::ktk::make_vfunction<kotek::ktk::console_command_variant_t>(
			lambda_uint32);

	EXPECT_TRUE(command_uint32(args));
	EXPECT_TRUE(ran_uint);
	EXPECT_EQ(received_uint, 42u);
}

TEST(Zircon_Game, ConfigSdkCameraRotationQuaternionRoundTrip)
{
	// heap allocated like every fixture that touches these classes (the
	// filesystem's reserved read buffer alone is ~1 MB of stack)
	kotek::core::ktkFrameworkConfig* p_framework_config =
		new kotek::core::ktkFrameworkConfig();
	kotek::core::ktkFileSystem* p_filesystem =
		new kotek::core::ktkFileSystem();
	p_filesystem->Initialize(p_framework_config);

	ktk_filesystem_path path_to_file;
	p_filesystem->Make_Path(
		path_to_file, kotek::core::eFolderIndex::kFolderIndex_DataUser);
	path_to_file /= kZirconConfig_FileName;

	if (p_filesystem->Is_Exists(path_to_file) == false)
	{
		p_filesystem->Shutdown();
		delete p_filesystem;
		delete p_framework_config;
		GTEST_SKIP() << "game_config.json is absent — the roundtrip "
						"needs the real file to preserve";
	}

	// backup the working copy — the serialize under test writes the
	// real data_user/game_config.json, the bytes go back at the end so
	// the test never drifts the user's settings
	kotek::array_t<unsigned char, 2048> backup{};
	kotek::ktk::size_t backup_size = backup.size();
	unsigned char* p_backup_data = backup.data();

	bool status =
		p_filesystem->Read_File(path_to_file, p_backup_data, backup_size);
	ASSERT_TRUE(status);
	ASSERT_LT(backup_size, backup.size());

	// the default is euler (off)
	{
		zircon_config config_default;
		EXPECT_FALSE(config_default.is_feature_enabled(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion));
	}

	// on -> persists
	{
		zircon_config config_write;
		config_write.set_feature(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion,
			true);
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.deserialize(p_filesystem);

		EXPECT_TRUE(config_read.is_feature_enabled(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion));
		// no cross-talk with the neighboring bool keys
		EXPECT_TRUE(config_read.is_feature_enabled(
			eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart));
	}

	// off -> persists as a written false (an absent key would keep the
	// in-memory true below — the deserialize must actually read it)
	{
		zircon_config config_write;
		config_write.set_feature(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion,
			false);
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.set_feature(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion,
			true);
		config_read.deserialize(p_filesystem);

		EXPECT_FALSE(config_read.is_feature_enabled(
			eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion));
	}

	// restore the user's bytes
	status = p_filesystem->Write_File(path_to_file,
		reinterpret_cast<const char*>(backup.data()), backup_size);
	EXPECT_TRUE(status);

	p_filesystem->Shutdown();
	delete p_filesystem;
	delete p_framework_config;
}

TEST(Zircon_Game, ConfigSdkCameraInputBootstrapRoundTrip)
{
	// the bootstrap gate (task Z20, owner clarification): default TRUE,
	// a written false must override the in-memory default, a written
	// true must persist — same backup/restore discipline as the
	// quaternion-key roundtrip above
	kotek::core::ktkFrameworkConfig* p_framework_config =
		new kotek::core::ktkFrameworkConfig();
	kotek::core::ktkFileSystem* p_filesystem =
		new kotek::core::ktkFileSystem();
	p_filesystem->Initialize(p_framework_config);

	ktk_filesystem_path path_to_file;
	p_filesystem->Make_Path(
		path_to_file, kotek::core::eFolderIndex::kFolderIndex_DataUser);
	path_to_file /= kZirconConfig_FileName;

	if (p_filesystem->Is_Exists(path_to_file) == false)
	{
		p_filesystem->Shutdown();
		delete p_filesystem;
		delete p_framework_config;
		GTEST_SKIP() << "game_config.json is absent — the roundtrip "
						"needs the real file to preserve";
	}

	kotek::array_t<unsigned char, 2048> backup{};
	kotek::ktk::size_t backup_size = backup.size();
	unsigned char* p_backup_data = backup.data();

	bool status =
		p_filesystem->Read_File(path_to_file, p_backup_data, backup_size);
	ASSERT_TRUE(status);
	ASSERT_LT(backup_size, backup.size());

	// default ON (opt-out)
	{
		zircon_config config_default;
		EXPECT_TRUE(config_default.is_feature_enabled(
			eZirconSDKFeatures::
				kSDK_Feature_AddSdkCameraInputBootstrap_Automatically));
	}

	// off written -> the read flag is false even though the fresh
	// config's default is true (proves the key is actually read)
	{
		zircon_config config_write;
		config_write.set_feature(
			eZirconSDKFeatures::
				kSDK_Feature_AddSdkCameraInputBootstrap_Automatically,
			false);
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.deserialize(p_filesystem);

		EXPECT_FALSE(config_read.is_feature_enabled(
			eZirconSDKFeatures::
				kSDK_Feature_AddSdkCameraInputBootstrap_Automatically));
	}

	// on written -> persists
	{
		zircon_config config_write;
		config_write.set_feature(
			eZirconSDKFeatures::
				kSDK_Feature_AddSdkCameraInputBootstrap_Automatically,
			true);
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.deserialize(p_filesystem);

		EXPECT_TRUE(config_read.is_feature_enabled(
			eZirconSDKFeatures::
				kSDK_Feature_AddSdkCameraInputBootstrap_Automatically));
		// no cross-talk with the neighboring bool keys
		EXPECT_TRUE(config_read.is_feature_enabled(
			eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart));
	}

	// restore the user's bytes
	status = p_filesystem->Write_File(path_to_file,
		reinterpret_cast<const char*>(backup.data()), backup_size);
	EXPECT_TRUE(status);

	p_filesystem->Shutdown();
	delete p_filesystem;
	delete p_framework_config;
}

		#endif

	#endif
#endif
