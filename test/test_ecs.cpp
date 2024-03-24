#include "catch.hpp"

#include "ecs/world.h"

TEST_CASE("World :: component storage", "[ECS]")
{
	World world;
	SECTION("A newly created world")
	{
		SECTION("has no entities")
		{
			CHECK(world.size() == 0ull);
		}
	}

	SECTION("A newly created entity in the world")
	{
		auto entity = world.createEntity();
		SECTION("makes world's size = 1")
		{
			CHECK(world.size() == 1ull);
		}
		SECTION("has no components by default")
		{
			CHECK_FALSE(world.has<int>(entity));
		}
		SECTION("can be associated with a component")
		{
			world.assign<int>(entity, 0);

			SECTION("")
			{
				REQUIRE(world.has<int>(entity));
				CHECK(world.get<int>(entity) == 0);
			}
			SECTION("which can be changed")
			{
				world.assign<int>(entity, 1);
				CHECK(world.get<int>(entity) == 1);
			}
			SECTION("which can be removed")
			{
				CHECK(world.get<int>(entity) == 0);
				world.remove<int>(entity);
				CHECK_FALSE(world.has<int>(entity));
			}
		}
	}
}

TEST_CASE("views", "[ECS]")
{
	World world;
	auto entity = world.createEntity();

	SECTION("view for non-existent storage")
	{
		REQUIRE_NOTHROW(world.view<int>().each([](Entity, int) {}));
	}

	SECTION("empty view")
	{
		world.assign<int>(entity, 0);
		world.remove<int>(entity);
		REQUIRE_FALSE(world.has<int>(entity));
		CHECK_NOTHROW(world.view<int>().each(
			[](Entity, int) { CHECK(false); /* should never reach */ }
		));
	}

	SECTION("simple view")
	{
		world.assign<int>(entity, 0);

		bool reached = false;
		world.view<int>().each(
			[&reached](Entity, int n)
			{
				reached = true;
				CHECK(n == 0);
			}
		);
		CHECK(reached);
	}

	SECTION("entity removal")
	{
		world.assign<int>(entity, 0);
		world.assign<float>(entity, 0.f);
		auto anotherEntity = world.createEntity();
		world.assign<int>(anotherEntity, 1);
		world.assign<float>(anotherEntity, 0.f);

		world.view<int, float>().each(
			[&world](Entity e, int, float)
			{
				world.remove<int>(e);
			}
		);
		world.view<int>().each(
			[](Entity, int)
			{
				CHECK(false);
			}
		);
	}

	SECTION("joint view")
	{
		world.assign<int>(entity, 0);
		world.assign<float>(entity, 0.f);

		bool reached = false;
		for (auto&& [e, n, f]: world.view<int, float>())
		{
			reached = true;
			CHECK(n == 0);
			CHECK(f == 0.f);
		}
		CHECK(reached);

		reached = false;
		world.view<int, float>().each(
			[&reached](Entity, int n, float f)
			{
				reached = true;
				CHECK(n == 0);
				CHECK(f == 0.f);
			}
		);
		CHECK(reached);
	}

	SECTION("joint view only shows entities that have both components")
	{
		world.assign<int>(entity, 0);
		world.assign<float>(entity, 0.f);
		auto anotherEntity = world.createEntity();
		world.assign<int>(anotherEntity, 1);

		bool reached = false;
		world.view<int, float>().each(
			[&reached](Entity, int n, float f)
			{
				reached = true;
				CHECK(n == 0);
				CHECK(f == 0.f);
			}
		);
		CHECK(reached);

		bool reached_one = false;
		bool reached_another = false;
		world.view<int>().each(
			[&](Entity e, int n)
			{
				if (e == entity)
				{
					reached_one = true;
					CHECK(n == 0);
				}
				else
				{
					reached_another = true;
					CHECK(n == 1);
				}
			}
		);
		CHECK(reached_one);
		CHECK(reached_another);
	}
}

TEST_CASE("const world", "[ECS]")
{
	World world;
	auto entity = world.createEntity();
	world.assign<int>(entity, 0);

	World const& constWorld = world;

	SECTION("const view smoke test")
	{
		world.assign<int>(entity, 0);

		bool reached = false;
		constWorld.view<int>().each(
			[&reached](Entity, int const& n)
			{
				reached = true;
				CHECK(n == 0);
			}
		);
		CHECK(reached);
	}
}

TEST_CASE("systems", "[ECS]")
{
	World world;
	auto entity = world.createEntity();
	world.assign<int>(entity, 0);

	SECTION("mutation through view")
	{
		world.view<int>().each(
			[&world](Entity e, int)
			{
				world.assign<int>(e, 1);
			}
		);
		CHECK(world.get<int>(entity) == 1);

		for (auto&& [e, n]: world.view<int>())
		{
			world.assign<int>(e, 2);
		}
		CHECK(world.get<int>(entity) == 2);
	}
}

TEST_CASE("World :: callbacks", "[ECS]")
{
	World world;
	SECTION("A newly created world")
	{
		SECTION("can register callbacks")
		{
			CHECK_NOTHROW(world.onCreate<int>().connect([](auto&, auto){}));
			CHECK_NOTHROW(world.onUpdate<int>().connect([](auto&, auto){}));
			CHECK_NOTHROW(world.onRemove<int>().connect([](auto&, auto){}));
		}
	}
	auto entity = world.createEntity();
	SECTION("Creation event callbacks")
	{
		bool called = false;
		auto callback = [&called](auto&, auto){ called = true; };
		auto callbackId = world.onCreate<int>().connect(callback);
		SECTION("get called")
		{
			world.assign<int>(entity, 0);
			CHECK(called);
		}
		SECTION("can be disconnected")
		{
			world.onCreate<int>().disconnect(callbackId);
			world.assign<int>(entity, 0);
			CHECK_FALSE(called);
		}
	}
}
