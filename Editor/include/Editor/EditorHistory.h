#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <string>
#include <vector>

namespace Nova::Editor {

class EditorHistory {
public:
    void Push(const Scene& scene) {
        m_Undo.push_back(SerializeSceneToString(scene));
        m_Redo.clear();
        if (m_Undo.size() > kMaxEntries) {
            m_Undo.erase(m_Undo.begin());
        }
    }

    bool CanUndo() const { return !m_Undo.empty(); }
    bool CanRedo() const { return !m_Redo.empty(); }

    bool Undo(Scene& current) {
        if (m_Undo.empty()) {
            return false;
        }
        m_Redo.push_back(SerializeSceneToString(current));
        const std::string snap = std::move(m_Undo.back());
        m_Undo.pop_back();
        return DeserializeSceneFromString(snap, current).Ok;
    }

    bool Redo(Scene& current) {
        if (m_Redo.empty()) {
            return false;
        }
        m_Undo.push_back(SerializeSceneToString(current));
        const std::string snap = std::move(m_Redo.back());
        m_Redo.pop_back();
        return DeserializeSceneFromString(snap, current).Ok;
    }

    void Clear() {
        m_Undo.clear();
        m_Redo.clear();
    }

private:
    static constexpr std::size_t kMaxEntries = 64;
    std::vector<std::string> m_Undo;
    std::vector<std::string> m_Redo;
};

} // namespace Nova::Editor
