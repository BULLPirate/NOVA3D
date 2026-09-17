#pragma once

#include <string>
#include <vector>

namespace Nova::Editor {

void AttachEditorLogSink();
void DetachEditorLogSink();
std::vector<std::string> CopyEditorLogLines();
void ClearEditorLog();

} // namespace Nova::Editor
