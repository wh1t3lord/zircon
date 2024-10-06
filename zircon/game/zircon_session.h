#pragma once

class zircon_interface_session
{
public:
	virtual ~zircon_interface_session(void) {}

	virtual void Shutdown(void) = 0;
	virtual void Update(void) = 0;
};