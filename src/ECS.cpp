#include "ECS.hpp"

// void Entity::addComponent(Component *cmp) {
// m_cmpMgr.addComponent(*this, cmp);
// m_cmps.emplace_back(cmp->id());
//}

// void Entity::removeComponent(size_t id) {
// m_cmpMgr.removeComponent(*this, id);
// m_cmps.erase(std::remove(m_cmps.begin(), m_cmps.end(), id), m_cmps.end());
//}

// void ComponentMgr::addComponent(Entity e, Component *cmp) {
// m_cmpMap[cmp->id()][e.id()] = cmp;
//}

void ComponentMgr::removeComponent(Entity e, size_t id) {
  delete m_cmpMap[id][e.id()];
  m_cmpMap[id].erase(e.id());
}

// void WorldMgr::addSys(System *s) { m_ss.emplace_back(s); }

// void WorldMgr::removeSys(System *s) {
// m_ss.erase(std::remove(m_ss.begin(), m_ss.end(), s), m_ss.end());
//}

Entity WorldMgr::createEntity() {
  static size_t s_entityCounter = 0;
  return Entity(++s_entityCounter, m_cmpMgr);
}

void WorldMgr::addEntity(Entity e) { m_es.emplace_back(e); }

void WorldMgr::removeEntity(Entity e) {
  // m_es.erase(std::remove(m_es.begin(), m_es.end(), e), m_es.end());
}

void WorldMgr::init(Settings s) {
  for (auto &sys : m_ss) {
    sys->init(m_es, s);
  }
}

void WorldMgr::update(float dt) {
  for (auto &sys : m_ss) {
    sys->update(m_es, dt);
  }
}

void WorldMgr::render(Renderer r) {
  for (auto &sys : m_ss) {
    sys->render(m_es, r);
  }
}

void WorldMgr::clean() {
  for (auto &sys : m_ss) {
    sys->clean(m_es);
  }
}
