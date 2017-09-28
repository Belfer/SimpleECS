#pragma once

#include <assert.h>
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

struct IPool {
  virtual ~IPool() {}
  virtual void clear() = 0;
};

template <typename T> class Pool : public IPool {
public:
  Pool(int size = 100) { resize(size); }
  virtual ~Pool() {}

  inline bool empty() const { return m_data.empty(); }

  inline uint size() const { return m_data.size(); }

  inline void resize(int n) {
    const size_t dataSize = size();
    m_data.resize(n);
    if (size() > dataSize) {
      for (auto it = m_data.begin() + dataSize; it != m_data.end(); ++it) {
        it->first = false;
      }
    }
  }

  inline void clear() { m_data.clear(); }

  inline void remove(const T &object) {
    m_data.erase(std::remove(m_data.begin(), m_data.end(), object),
                 m_data.end());
  }

  inline void remove(uint index) { m_data.erase(m_data.begin() + index); }

  inline bool set(uint index, const T &object) {
    assert(index < size());
    m_data[index].first = true;
    m_data[index].second = object;
    return true;
  }

  inline T &get(uint index) {
    assert(index < size());
    return static_cast<T &>(m_data[index].second);
  }

  inline T &recycle() {
    for (auto &e : m_data) {
      if (!e.first) {
        e.first = true;
        return static_cast<T &>(e.second);
      }
    }

    resize(size() * 2);
    return recycle();
  }

  inline void add(const T &object) {
    auto &obj = recycle();
    obj = object;
  }

  inline T &operator[](uint index) { return m_data[index].second; }

  inline const T &operator[](uint index) const { return m_data[index].second; }

  inline std::vector<std::pair<bool, T>> &data() { return m_data; }

private:
  std::vector<std::pair<bool, T>> m_data;
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

template <typename E> struct Cmp : public ICmp {
  static size_t id() {
    static const size_t s_id = regId();
    return s_id;
  }
};

class EntityMgr;

struct Entity {
private:
  friend class EntityMgr;
  Entity(size_t id, EntityMgr &entityMgr) : m_id(id), m_entityMgr(entityMgr) {}

public:
  template <typename C> inline void addComponent(const C &cmp) const;

  template <typename C, typename... Args>
  inline void addComponent(Args... args) const;

  template <typename C> inline bool hasComponent() const;

  template <typename C> inline C &getComponent() const;

  template <typename C> inline void removeComponent() const;

  inline size_t id() const { return m_id; }

private:
  size_t m_id;
  EntityMgr &m_entityMgr;
  std::vector<size_t> m_cmps;
};

using Entities = std::vector<Entity>;
#include <iostream>
class EntityMgr {
private:
  template <typename C> using CmpPool = Pool<std::pair<size_t, C>>;
  using CmpMap = std::unordered_map<size_t, IPool *>;

  template <typename C> inline CmpPool<C> &getPool() {
    auto handle = static_cast<CmpPool<C> *>(m_cmpMap[Cmp<C>::id()]);
    if (handle == nullptr) {
      handle = new CmpPool<C>();
      m_cmpMap[Cmp<C>::id()] = handle;
    }
    return *handle;
  }

public:
  Entity createEntity() {
    static size_t s_entityCounter = -1;
    return Entity(++s_entityCounter, *this);
  }

  void addEntity(Entity e) { m_entities.emplace_back(e); }

  void removeEntity(Entity e) {}

  template <typename C> void addComponent(Entity e, const C &cmp) {
    auto &poolHandle = getPool<C>();
    poolHandle.add(std::make_pair(e.id(), cmp));
  }

  template <typename C, typename... Args>
  void addComponent(Entity e, Args... args) {
    auto &poolHandle = getPool<C>();
    poolHandle.add(std::make_pair(e.id(), C(args...)));
  }

  template <typename C> bool hasComponent(Entity e) {
    auto &poolHandle = getPool<C>();
    for (auto data : poolHandle.data()) {
      if (data.first && data.second.first == e.id()) {
        return true;
      }
    }
    return false;
  }

  template <typename C> C &getComponent(Entity e) {
    auto &poolHandle = getPool<C>();
    for (auto &data : poolHandle.data()) {
      if (data.first && data.second.first == e.id()) {
        return data.second.second;
      }
    }
    assert(false && "Entity doesn't have component!");
  }

  template <typename C> void removeComponent(Entity e) {
    auto poolHandle = getPool<C>();
    for (auto data : poolHandle.data()) {
      if (data.first == e.id()) {
        poolHandle.remove(data);
      }
    }
  }

  inline Entities &entities() { return m_entities; }

private:
  CmpMap m_cmpMap;
  Entities m_entities;
};

template <typename C> inline void Entity::addComponent(const C &cmp) const {
  m_entityMgr.addComponent(id(), cmp);
}

template <typename C, typename... Args>
inline void Entity::addComponent(Args... args) const {
  m_entityMgr.addComponent<C>(*this, std::forward<Args>(args)...);
}

template <typename C> inline bool Entity::hasComponent() const {
  return m_entityMgr.hasComponent<C>(*this);
}

template <typename C> inline C &Entity::getComponent() const {
  return m_entityMgr.getComponent<C>(*this);
}

template <typename C> inline void Entity::removeComponent() const {
  m_entityMgr.removeComponent<C>();
}

/**
 * @brief Systems
 */

class EventMgr;

struct ISys {
  virtual void init(EntityMgr &es) = 0;
  virtual void update(EntityMgr &es, float dt) = 0;
  virtual void render(EntityMgr &es) = 0;
  virtual void clean(EntityMgr &es) = 0;

protected:
  static size_t regId() {
    static size_t s_sysCounter = -1;
    return ++s_sysCounter;
  }
};

template <typename E> struct Sys : public ISys {
  static size_t id() {
    static const size_t s_id = regId();
    return s_id;
  }

  inline EventMgr &getEventMgr() { return *m_eventMgr; }

private:
  friend class SystemMgr;
  EventMgr *m_eventMgr;
};

using Systems = std::vector<ISys *>;

class SystemMgr {
public:
  SystemMgr(EntityMgr &entityMgr, EventMgr &eventMgr)
      : m_entityMgr(entityMgr), m_eventMgr(eventMgr) {}

  template <typename S, typename... Args> void addSys(Args... args) {
    auto sys = new S(args...);
    sys->m_eventMgr = &m_eventMgr;
    m_systems.emplace_back(sys);
  }

  template <typename Sys> void removeSys() {
    // TODO
  }

  inline void init() {
    for (auto &sys : m_systems) {
      sys->init(m_entityMgr);
    }
  }

  inline void update(float dt) {
    for (auto &sys : m_systems) {
      sys->update(m_entityMgr, dt);
    }
  }

  inline void render() {
    for (auto &sys : m_systems) {
      sys->render(m_entityMgr);
    }
  }

  inline void clean() {
    for (auto &sys : m_systems) {
      sys->clean(m_entityMgr);
    }
  }

private:
  EntityMgr &m_entityMgr;
  EventMgr &m_eventMgr;
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

  std::pair<size_t, SigSlots> &slotsFor(size_t eId) {
    if (eId >= m_bus.size())
      m_bus.resize(eId + 1);
    return m_bus[eId];
  }

  void clearSignals(std::vector<SigHandle> &sigHandles) {
    for (auto handle : sigHandles) {
      auto &sigSlots = slotsFor(handle.first);
      sigSlots.second.erase(handle.second);
    }
  }

  std::vector<std::pair<size_t, SigSlots>> m_bus;
};
