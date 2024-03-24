#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include <DU/type_utils.h>

struct Entity
{
	unsigned entityId;
	friend auto operator<=>(Entity const&, Entity const&) = default; 
};

template <typename F, typename T>
concept TakesOnlyLvalue = std::is_invocable_v<F, T&> && not std::is_invocable_v<F, T>;

class World
{
public:
	template <typename Component, typename... Ts>
	void assign(Entity entity, Ts&&... args)
	{
		ensureStorage<Component>();
		bool isCreated = not has<Component>(entity);
		getStorage<Component>().insert_or_assign(entity, Component{args...});
		if (isCreated)
		{
			onCreate<Component>().publish(*this, entity);
		}
		else
		{
			onUpdate<Component>().publish(*this, entity);
		}
	}

	template <typename Component, typename Callable>
	void transform(Entity entity, Callable f)
		requires requires (Component& c) { c = f(c); }
	{
		auto& component = getStorage<Component>().at(entity);
		component = f(component);
		onUpdate<Component>().publish(*this, entity);
	}

	template <typename Component, typename Callable>
	void patch(Entity entity, Callable f)
		requires TakesOnlyLvalue<Callable, Component>
	{
		auto& component = getStorage<Component>().at(entity);
		f(component);
		onUpdate<Component>().publish(*this, entity);
	}

	template <typename Component>
	Component const& get(Entity entity) const
	{
		return getStorage<Component>().at(entity);
	}

	template <typename Component>
	void remove(Entity entity)
	{
		onRemove<Component>().publish(*this, entity);
		getStorage<Component>().erase(entity);
	}

	std::size_t size() const
	{
		return entities.size();
	}

	Entity createEntity()
	{
		auto newEntity = Entity{nextEntityId++};
		entities.insert(newEntity);
		return newEntity;
	}

	void destroyEntity(Entity entity)
	{
		for (auto&& [_, storage]: componentStorage)
		{
			storage->erase(entity);
		}
		entities.erase(entity);
	}

	template <typename Component>
	bool has(Entity entity) const
	{
		if (not hasStorage<Component>())
		{
			return false;
		}
		return getStorage<Component>().contains(entity);
	}

	template <typename... Components>
	auto view() const
	{
		if (not (hasStorage<Components>() && ...))
		{
			return View<Components...>{Storage<Components>{}...};
		}
		return View<Components...>{getStorage<Components>()...};
	}

	template <typename Component>
	auto& onCreate()
	{
		ensureStorage<Component>();
		return getStorage<Component>().createEventDispatcher;
	}

	template <typename Component>
	auto& onUpdate()
	{
		ensureStorage<Component>();
		return getStorage<Component>().updateEventDispatcher;
	}

	template <typename Component>
	auto& onRemove()
	{
		ensureStorage<Component>();
		return getStorage<Component>().removeEventDispatcher;
	}

private:
	unsigned nextEntityId = 0;

	struct StorageBase
	{
		virtual ~StorageBase() = default;

		virtual void erase(Entity) = 0;

		std::set<Entity> entities;
	};

	template <typename Component>
	struct Storage: public StorageBase, public std::map<Entity, Component>
	{
		void erase(Entity entity) override
		{
			std::map<Entity, Component>::erase(entity);
			entities.erase(entity);
		}

		void insert_or_assign(Entity entity, Component component)
		{
			std::map<Entity, Component>::insert_or_assign(entity, component);
			entities.insert(entity);
		}

		struct EventDispatcher
		{
			unsigned nextCallbackId = 0;

			using Callback = std::function<void(World&, Entity)>;

			unsigned connect(Callback f)
			{
				auto id = nextCallbackId++;
				callbacks[id] = f;
				return id;
			}
			
			void disconnect(unsigned callbackId)
			{
				callbacks.erase(callbackId);
			}

			void publish(World& world, Entity entity)
			{
				for (auto& item: callbacks)
				{
					auto& callback = item.second;
					callback(world, entity);
				}
			}

			std::unordered_map<unsigned, Callback> callbacks;
		};

		EventDispatcher createEventDispatcher;
		EventDispatcher updateEventDispatcher;
		EventDispatcher removeEventDispatcher;
	};

	std::set<Entity> entities;
	std::unordered_map<std::type_index, std::unique_ptr<StorageBase>> componentStorage;

	template <typename Component>
	void ensureStorage()
	{
		if (not hasStorage<Component>())
		{
			componentStorage[typeid(Component)].reset(new Storage<Component>{});
		}
	}

	template <typename Component>
	auto& getStorage()
	{
		return *static_cast<Storage<Component>*>(componentStorage.at(typeid(Component)).get());
	}
	
	template <typename Component>
	auto const& getStorage() const
	{
		return *static_cast<Storage<Component>*>(componentStorage.at(typeid(Component)).get());
	}
	
	template <typename Component>
	bool hasStorage() const
	{
		return componentStorage.contains(typeid(Component));
	}
	
	template <typename... Components>
	class View
	{
	public:
		View(Storage<Components>... storages_)
		{
			(storages.insert({typeid(Components), std::make_unique<Storage<Components>>(std::move(storages_))}), ...);
		}

		template <typename Callable>
		void each(Callable f)
		{
			auto& base = storages.begin()->second;
			for (auto entity: base->entities)
			{
				if ((getStorage<Components>().contains(entity) && ...))
				{
					f(entity, getStorage<Components>().at(entity)...);
				}
			}
		}

		class Iterator
		{
		public:
			Iterator(View<Components...>* container_, std::set<Entity>::iterator iterator, std::set<Entity>::iterator end)
				: container{container_}
				, entityIterator{iterator}
				, endIterator{end}
			{}

			auto operator*()
			{
				auto entity = *entityIterator;
				return std::tie(entity, container->getStorage<Components>().at(entity)...);
			}

			bool operator!=(Iterator const& other) const
			{
				return entityIterator != other.entityIterator;
			}

			Iterator& operator++()
			{
				while (entityIterator != endIterator)
				{
					++entityIterator;
					auto entity = *entityIterator;
					if ((container->getStorage<Components>().contains(entity) && ...))
					{
						break;
					}
				}
				return *this;
			}

		private:
			View<Components...>* container;
			std::set<Entity>::iterator entityIterator;
			std::set<Entity>::iterator endIterator;
		};

		Iterator begin()
		{
			auto& base = storages.begin()->second;
			return {this, base->entities.begin(), base->entities.end()};
		}

		Iterator end()
		{
			auto& base = storages.begin()->second;
			return {this, base->entities.end(), base->entities.end()};
		}

	private:
		std::unordered_map<std::type_index, std::unique_ptr<StorageBase>> storages;

		template <typename Component>
		auto const& getStorage()
		{
			return *static_cast<Storage<Component>*>(storages[typeid(std::remove_const_t<Component>)].get());
		}
	};
};
