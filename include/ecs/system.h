#pragma once

#include "sdlpp/events.h"

#include "ecs/world.h"

enum class DidConsume: bool
{
	No, Yes
};

class System
{
public:
	explicit System(World& world_): world{world_} {}

	virtual ~System() = default;

	System(System const&) = default;
	System(System&&) = default;

	System& operator=(System const&) = delete;
	System& operator=(System&&) = delete;

	virtual void update() {}
	virtual DidConsume tryConsumeEvent(SDL::Event) { return DidConsume::No; }

protected:
	World& world;
};
