#pragma once
#include "model.h"
#include "state_store.h"

namespace explicit_ui::app_state {
// Persistence keys/types are generated from the model's explicit `persist` list.
// Widget-specific input policies belong to the host and must be checked before restore.
inline state_store::State capture(const Model& model) {
    state_store::State state;
    for(const auto& update:model.persistent_state())
        state.emplace(update.id,std::visit([](const auto& value)->state_store::Value{return value;},update.value));
    return state;
}
inline bool restore(Model& model,const state_store::State& state) {
    std::vector<Update> updates;updates.reserve(state.size());
    for(const auto& entry:state)
        updates.push_back({entry.first,std::visit([](const auto& value)->Value{return value;},entry.second)});
    return model.restore_persistent_state(updates);
}
} // namespace explicit_ui::app_state
