#pragma once


struct zircon_asset_handle_t
{
	kotek::uint32_t id;
};


class zircon_asset_manager_interface 
{
	virtual ~zircon_asset_manager_interface() {}

	virtual void initialize() = 0;
	virtual void shutdown() = 0;

	
};