#include "ECS.hpp"
#include <iostream>
#include <string>

struct GameObjectCmp : Cmp<GameObjectCmp> {
  GameObjectCmp() {}
  GameObjectCmp(const std::string &name, const std::string &tag)
      : name(name), tag(tag) {}
  std::string name;
  std::string tag;
};

struct GameObjectSys : System {
  void init(Entities &es, Settings s) {
    for (auto &e : es) {
      if (e.hasComponent<GameObjectCmp>()) {
        auto &goc = e.getComponent<GameObjectCmp>();
        std::cout << goc.name << ", " << goc.tag << "\n";
      }
    }
  }

  void update(Entities &es, float dt) {}

  void render(Entities &es, Renderer r) {}

  void clean(Entities &es) {}
};

WorldMgr worldMgr;

bool running = true;

void run() {
  Settings s;
  Renderer r;
  float dt = 0;

  worldMgr.init(s);
  while (running) {
    worldMgr.update(dt);
    worldMgr.render(r);
  }
  worldMgr.clean();
}

int main(int argc, char **args) {
  worldMgr.addSys<GameObjectSys>();

  Entity e = worldMgr.createEntity();
  e.addComponent<GameObjectCmp>("player", "test");
  worldMgr.addEntity(e);

  run();
  return 0;
}
