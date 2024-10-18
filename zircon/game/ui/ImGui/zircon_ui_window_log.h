#pragma once

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE

class ktkMainManager;

KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_sdk_ui_window_log : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_window_log();
	~zircon_sdk_ui_window_log();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
};
