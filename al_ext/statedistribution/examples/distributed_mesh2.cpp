//#include <string>

#include "al/app/al_DistributedApp.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/scene/al_DistributedScene.hpp"

#include "al_ext/statedistribution/al_CuttleboneStateSimulationDomain.hpp"
#include "al_ext/statedistribution/al_Serialize.hpp"

// This example shows using shared distributed state to control voices from
// DynamicScene
using namespace al;

const size_t maxMeshDataSize = 256;
const size_t maxVoices = 3;

struct SerializedMesh {
  uint16_t id = 0;
  char meshData[maxMeshDataSize];
  size_t meshVertices = 0;
  size_t meshIndeces = 0;
  size_t meshColors = 0;
};

struct SharedState {
  SerializedMesh meshes[maxVoices];
};

struct MeshVoice : public PositionedVoice {

  Mesh mesh;

  void init() override { mesh.primitive(Mesh::TRIANGLE_STRIP); }

  void onTriggerOn() override { setPose(Vec3d(0, 0, 0)); }

  void update(double dt) override {
    auto p = pose();
    p.pos().x = p.pos().x + 0.02;
    if (p.pos().x >= 2) {
      free();
    }
    setPose(p);
  }

  void onProcess(Graphics &g) override {
    g.polygonFill();
    g.meshColor();
    g.draw(mesh); // Draw the mesh
  }
};

class MyApp : public DistributedAppWithState<SharedState> {
public:
  DistributedScene scene{TimeMasterMode::TIME_MASTER_FREE};

  void onCreate() override {
    // Set the camera to view the scene
    nav().pos(Vec3d(0, 0, 8));

    navControl().active(false);

    scene.registerSynthClass<MeshVoice>();
    registerDynamicScene(scene);

    auto cuttleboneDomain =
        CuttleboneStateSimulationDomain<SharedState>::enableCuttlebone(this);
    if (!cuttleboneDomain) {
      std::cerr << "ERROR: Could not start Cuttlebone. Quitting." << std::endl;
      quit();
    }
  }

  void onAnimate(double dt) override {
    scene.update(dt);
    if (isPrimary()) {
      // We can get away with this here as the master clock is the graphics
      // clock. There is no guarantee otherwise that this linked list will not
      // change while we are using it.
      auto *voice = scene.getActiveVoices();
      size_t counter = 0;
      while (voice && counter < maxVoices) {
        ser::serializeMesh(((MeshVoice *)voice)->mesh,
                           state().meshes[counter].meshData,
                           state().meshes[counter].meshVertices,
                           state().meshes[counter].meshIndeces,
                           state().meshes[counter].meshColors, maxMeshDataSize);
        state().meshes[counter].id = voice->id();
        voice = voice->next;
        counter++;
      }
    } else {

      auto *voice = scene.getActiveVoices();

      while (voice) {
        SerializedMesh *m = nullptr;
        for (size_t i = 0; i < maxVoices; i++) {
          if (state().meshes[i].id == voice->id()) {
            m = &state().meshes[i];
            break;
          }
        }
        if (m) {
          ser::deserializeMesh(((MeshVoice *)voice)->mesh, m->meshData,
                               m->meshVertices, m->meshIndeces, m->meshColors);

        } else {
          std::cerr << "ERROR: unexpeceted voice id" << std::endl;
        }
        voice = voice->next;
      }
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0);
    scene.render(g);
    scene.processVoices();
    // Turn off voices
    scene.processVoiceTurnOff();
    scene.processVoiceTurnOff();
  }

  bool onKeyDown(Keyboard const &k) override {
    if (k.key() == ' ') {
      // The space bar will turn off omni rendering
      if (omniRendering) {
        omniRendering->drawOmni = !omniRendering->drawOmni;
      } else {
        std::cout << "Not doing omni rendering" << std::endl;
      }
    } else {

      auto *voice = scene.getVoice<MeshVoice>();
      voice->mesh.reset();

      for (int i = 0; i < 4; i++) {
        voice->mesh.vertex(rnd::uniformS(), rnd::uniformS(), rnd::uniformS());
        voice->mesh.color(rnd::uniform(), rnd::uniform(), rnd::uniform());
      }

      scene.triggerOn(voice);
    }
    return true;
  }

private:
};

int main() {
  MyApp app;
  app.start();
  return 0;
}
