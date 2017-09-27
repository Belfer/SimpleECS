#pragma once

#include <memory>
#include <stdlib.h>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Utility
 */

struct NonCopyable {
protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

private:
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

/**
 * @brief Entities
 */

struct ICmp {
protected:
  static size_t regId() {
    static size_t s_cmpCounter = -1;
    return ++s_cmpCounter;
  }
};

template <typename E> struct Cmp : ICmp {
  static size_t id() {
    static const size_t s_id = regId();
    return s_id;
  }
};

class EntityMgr;

struct Entity {
  Entity(size_t id, EntityMgr &entityMgr) : m_id(id), m_entityMgr(entityMgr) {}

  template <typename C> inline void addComponent(const C &cmp) const;

  template <typename C, typename... Args>
  inline void addComponent(Args... args) const;

  template <typename C> inline bool hasComponent() const;

  template <typename C> inline C &getComponent() const;

  template <typename C> inline void removeComponent() const;

  inline size_t id() const { return m_id; }

private:
  size_t m_id;
  std::vector<size_t> m_cmps;
  EntityMgr &m_entityMgr;
};

using Entities = std::vector<Entity>;

class EntityMgr {
public:
  Entity createEntity() {
    static size_t s_entityCounter = -1;
    return Entity(++s_entityCounter, *this);
  }

  void addEntity(Entity e) { m_entities.emplace_back(e); }

  void removeEntity(Entity e) {}

  template <typename C> void addComponent(size_t eId, const C &cmp) {
    m_cmpMap[cmp.id()][eId] = cmp;
  }

  template <typename C, typename... Args>
  void addComponent(size_t eId, Args... args) {
    m_cmpMap[Cmp<C>::id()][eId] = new C(args...);
  }

  template <typename C> bool hasComponent(size_t eId) {
    for (auto c : m_cmpMap[Cmp<C>::id()]) {
      if (c.first == eId)
        return true;
    }
    return false;
  }

  template <typename C> C &getComponent(size_t eId) {
    return *static_cast<C *>(m_cmpMap[Cmp<C>::id()][eId]);
  }

  template <typename C> void removeComponent(Entity e) {
    // delete m_cmpMapCmp<C>::id()[e.id()];
    // m_cmpMap[Cmp<C>::id()].erase(e.id());
  }

  inline Entities &entities() { return m_entities; }

private:
  using CmpPool = std::unordered_map<size_t, ICmp *>;
  using CmpMap = std::unordered_map<size_t, CmpPool>;
  CmpMap m_cmpMap;
  Entities m_entities;
};

template <typename C> inline void Entity::addComponent(const C &cmp) const {
  m_entityMgr.addComponent(id(), cmp);
}

template <typename C, typename... Args>
inline void Entity::addComponent(Args... args) const {
  m_entityMgr.addComponent<C>(id(), std::forward<Args>(args)...);
}

template <typename C> inline bool Entity::hasComponent() const {
  return m_entityMgr.hasComponent<C>(id());
}

template <typename C> inline C &Entity::getComponent() const {
  return m_entityMgr.getComponent<C>(id());
}

template <typename C> inline void Entity::removeComponent() const {
  m_entityMgr.removeComponent<C>();
}

/**
 * @brief Systems
 */

struct Settings {};
struct Renderer {};

struct System {
  virtual void init(EntityMgr &es, Settings s) = 0;
  virtual void update(EntityMgr &es, float dt) = 0;
  virtual void render(EntityMgr &es, Renderer r) = 0;
  virtual void clean(EntityMgr &es) = 0;
};

using Systems = std::vector<System *>;

class SystemMgr {
public:
  SystemMgr(EntityMgr &entityMgr) : m_entityMgr(entityMgr) {}

  template <typename Sys, typename... Args> void addSys(Args... args) {
    m_systems.emplace_back(new Sys(args...));
  }

  template <typename Sys> void removeSys() {}

  inline void init(Settings s) {
    for (auto &sys : m_systems) {
      sys->init(m_entityMgr, s);
    }
  }

  inline void update(float dt) {
    for (auto &sys : m_systems) {
      sys->update(m_entityMgr, dt);
    }
  }

  inline void render(Renderer r) {
    for (auto &sys : m_systems) {
      sys->render(m_entityMgr, r);
    }
  }

  inline void clean() {
    for (auto &sys : m_systems) {
      sys->clean(m_entityMgr);
    }
  }

private:
  EntityMgr &m_entityMgr;
  Systems m_systems;
};

/**
 * @brief Events
 */

struct ISig {
  virtual ~ISig() {}
  virtual void operator()(const void *p) = 0;

protected:
  static size_t regId() {
    static size_t counter = -1;
    return ++counter;
  }
};

template <typename E> struct Sig : public ISig {
  explicit Sig(std::function<void(const E &)> sigFn) : m_sigFn(sigFn) {}
  virtual void operator()(const void *p) {
    m_sigFn(*(static_cast<const E *>(p)));
  }

  static size_t id() {
    static const size_t id = regId();
    return id;
  }

private:
  std::function<void(const E &)> m_sigFn;
};

using SigHandle = std::pair<size_t, size_t>;

struct Receiver {
  ~Receiver() {
    if (m_sigHandles.size() > 0)
      m_clearSigFn(m_sigHandles);
  }

private:
  friend class EventMgr;
  std::function<void(std::vector<SigHandle> &)> m_clearSigFn;
  std::vector<SigHandle> m_sigHandles;
};

class EventMgr : NonCopyable {
public:
  using SigSPtr = std::shared_ptr<ISig>;

  EventMgr() {}
  ~EventMgr() {
    // TODO, clean memory
  }

  template <typename E, typename Receiver> void subscribe(Receiver &receiver) {
    if (receiver.m_sigHandles.size() == 0) {
      receiver.m_clearSigFn =
          std::bind(&EventMgr::clearSignals, this, std::placeholders::_1);
    }

    void (Receiver::*receive)(const E &) = &Receiver::receive;
    auto signal =
        new Sig<E>(std::bind(receive, &receiver, std::placeholders::_1));

    auto &sigSlots = slotsFor(Sig<E>::id());
    sigSlots.second[sigSlots.first] = SigSPtr(signal);

    receiver.m_sigHandles.emplace_back(
        std::make_pair(Sig<E>::id(), sigSlots.first));

    sigSlots.first++;
  }

  template <typename E, typename Receiver>
  void unsubscribe(Receiver &receiver) {
    auto &sigSlots = slotsFor(Sig<E>::id());
    for (auto handle : receiver.m_sigHandles) {
      if (handle.first == Sig<E>::id())
        sigSlots.second.erase(handle.second);
    }
  }

  template <typename E, typename... Args> void broadcast(Args... args) {
    broadcast(E(args...));
  }

  template <typename E> void broadcast(const E &event) {
    auto &sigSlots = slotsFor(Sig<E>::id());
    for (auto sig : sigSlots.second) {
      (*sig.second)(static_cast<const void *>(&event));
    }
  }

private:
  using SigSlots = std::unordered_map<size_t, SigSPtr>;

  std::pair<size_t, SigSlots> &slotsFor(size_t eID) {
    if (eID >= m_bus.size())
      m_bus.resize(eID + 1);
    return m_bus[eID];
  }

  void clearSignals(std::vector<SigHandle> &sigHandles) {
    for (auto handle : sigHandles) {
      auto &sigSlots = slotsFor(handle.first);
      sigSlots.second.erase(handle.second);
    }
  }

  std::vector<std::pair<size_t, SigSlots>> m_bus;
};
