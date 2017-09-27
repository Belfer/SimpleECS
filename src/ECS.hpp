#pragma once

#include <memory>
#include <stdlib.h>
#include <unordered_map>
#include <utility>
#include <vector>

struct NonCopyable {
protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

private:
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

struct ICmp {
protected:
  static size_t regId() {
    static size_t s_cmpCounter = 0;
    return ++s_cmpCounter;
  }
};

template <typename E> struct Cmp : ICmp {
  static size_t id() {
    static const size_t s_id = regId();
    return s_id;
  }
};

struct Entity;

class ComponentMgr {
public:
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

  void removeComponent(Entity e, size_t id);

private:
  using CmpPool = std::unordered_map<size_t, ICmp *>;
  using CmpMap = std::unordered_map<size_t, CmpPool>;
  CmpMap m_cmpMap;
};

struct Entity {
  Entity(size_t id, ComponentMgr &cmpMgr) : m_id(id), m_cmpMgr(cmpMgr) {}

  template <typename C> inline void addComponent(const C &cmp) {
    m_cmpMgr.addComponent(id(), cmp);
  }

  template <typename C, typename... Args>
  inline void addComponent(Args... args) {
    m_cmpMgr.addComponent<C>(id(), std::forward<Args>(args)...);
  }

  template <typename C> inline bool hasComponent() const {
    return m_cmpMgr.hasComponent<C>(id());
  }

  template <typename C> inline C &getComponent() const {
    return m_cmpMgr.getComponent<C>(id());
  }

  inline void removeComponent(size_t cId);

  inline size_t id() const { return m_id; }

private:
  size_t m_id;
  std::vector<size_t> m_cmps;
  ComponentMgr &m_cmpMgr;
};

using Entities = std::vector<Entity>;

struct Settings {};
struct Renderer {};

struct System {
  virtual void init(Entities &es, Settings s) = 0;
  virtual void update(Entities &es, float dt) = 0;
  virtual void render(Entities &es, Renderer r) = 0;
  virtual void clean(Entities &es) = 0;
};

using Systems = std::vector<System *>;

class EntityMgr {
public:
  Entity createEntity();

  void addEntity(Entity e);

  void removeEntity(Entity e);

private:
  Entities m_es;
};

class SystemMgr {
public:
  template <typename Sys, typename... Args> void addSys(Args... args) {
    m_ss.emplace_back(new Sys(args...));
  }

  // template <typename Sys> void removeSys(Sys s) { m_ss.emplace_back(s); }

private:
  Systems m_ss;
};

class WorldMgr {
public:
  void init(Settings s);
  void update(float dt);
  void render(Renderer r);
  void clean();

private:
  EntityMgr m_es;
  SystemMgr m_ss;
  EventMgr m_ev;
};

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
