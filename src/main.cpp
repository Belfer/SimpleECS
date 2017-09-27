#include "ECS.hpp"
#include <iostream>
#include <string>

struct SystemEvent {};

struct SystemEventsReceiver : Receiver {
  SystemEventsReceiver(EventMgr &ev) { ev.subscribe<SystemEvent>(*this); }

  void receive(const SystemEvent &e) {
    std::cout << "SystemEventsReceiver - Received event from systems!\n";
  }
};

struct GameObjectCmp : Cmp<GameObjectCmp> {
  GameObjectCmp() {}
  GameObjectCmp(const std::string &name, const std::string &tag)
      : name(name), tag(tag) {}
  std::string name;
  std::string tag;
};

struct TransformCmp : Cmp<TransformCmp> {
  TransformCmp() {}
  TransformCmp(float pX, float pY, float sX, float sY, float rot)
      : pX(pX), pY(pY), sX(sX), sY(sY), rot(rot) {}
  float pX = 0, pY = 0;
  float sX = 0, sY = 0;
  float rot = 0;
};

struct GameObjectSys : System {
  void init(EntityMgr &es, Settings s) {
    std::cout << "INIT\n";

    for (auto &e : es.entities()) {
      if (e.hasComponent<GameObjectCmp>()) {
        auto &goc = e.getComponent<GameObjectCmp>();
        goc.name = "playerMod";
        goc.tag = "testMod";

        std::cout << goc.name << ", " << goc.tag << "\n";
      }
    }

    std::cout << "\n";
  }

  void update(EntityMgr &es, float dt) {
    std::cout << "UPDATE\n";

    for (auto &e : es.entities()) {
      if (e.hasComponent<GameObjectCmp>() && e.hasComponent<TransformCmp>()) {
        auto &goc = e.getComponent<GameObjectCmp>();
        auto &trx = e.getComponent<TransformCmp>();

        std::cout << trx.pY << "\n";
        std::cout << goc.name << ", " << goc.tag << "\n";

        goc.name = "player";
        goc.tag = "test";

        trx.pY += 1;
      }
    }

    std::cout << "\n";
  }

  void render(EntityMgr &es, Renderer r) { std::cout << "RENDER\n\n"; }

  void clean(EntityMgr &es) {
    getEventMgr().broadcast<SystemEvent>();
    std::cout << "CLEAN\n\n";
  }
};

EventMgr eventMgr;
EntityMgr entityMgr;
SystemMgr systemMgr(entityMgr, eventMgr);

SystemEventsReceiver receiver(eventMgr);

bool running = true;

void run() {
  Settings s;
  Renderer r;
  float dt = 1.f / 60.f;

  size_t ticks = 0;

  systemMgr.init(s);
  while (running) {
    systemMgr.update(dt);
    systemMgr.render(r);

    ticks++;
    if (ticks >= 10)
      running = false;
  }
  systemMgr.clean();
}

int main(int argc, char **args) {
  systemMgr.addSys<GameObjectSys>();

  Entity e = entityMgr.createEntity();
  e.addComponent<GameObjectCmp>("player", "test");
  e.addComponent<TransformCmp>();
  entityMgr.addEntity(e);

  run();
  return 0;
}
