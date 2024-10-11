#pragma once

class zircon_interface_session
{
public:
	virtual ~zircon_interface_session(void) {}

	virtual void shutdown(void) = 0;
	virtual void update(void) = 0;
};