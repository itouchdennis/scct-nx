// Chaos Theory's game patches start empty: the host's console and level select have no commands for it yet
#include "game/console.h"

namespace game {

void QueueCommand(std::string, Route) {}
void RunCommands(X86Context&, uint32_t, uint32_t) {}
std::vector<std::string> TakeOutput() { return {}; }
const std::vector<LevelList>& Levels() {
    static const std::vector<LevelList> none;
    return none;
}

}  // namespace game
