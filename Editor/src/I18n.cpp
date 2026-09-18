#include <Editor/I18n.h>

#include <cstring>

namespace Nova::Editor {

namespace {

struct Pair {
    const char* Key;
    const char* En;
    const char* Ru;
};

constexpr Pair kTable[] = {
    {"file", "File", "Файл"},
    {"open_project", "Open project", "Открыть проект"},
    {"new_project", "New project", "Новый проект"},
    {"open_scene", "Open scene", "Открыть сцену"},
    {"scenes_in_project", "Scenes", "Сцены"},
    {"save_scene", "Save scene", "Сохранить сцену"},
    {"save_scene_as", "Save scene as", "Сохранить сцену как"},
    {"reload", "Reload", "Перезагрузить"},
    {"new_empty_scene", "New empty scene", "Новая пустая сцена"},
    {"save_prefab", "Save prefab", "Сохранить префаб"},
    {"prefabs_in_project", "Prefabs", "Префабы"},
    {"instantiate_prefab", "Place prefab", "Вставить префаб"},
    {"game", "Create", "Создать"},
    {"new_playable", "New playable level", "Новый игровой уровень"},
    {"create_player", "Create character", "Создать персонажа"},
    {"import_character", "Import character", "Импорт персонажа"},
    {"import_item", "Import item", "Импорт предмета"},
    {"import_map", "Import map", "Импорт карты"},
    {"play_menu", "Play", "Играть"},
    {"play_scene", "Play scene", "Запустить сцену"},
    {"stop", "Stop", "Стоп"},
    {"run_standalone", "Run game", "Запустить игру"},
    {"play", "Play", "Play"},
    {"tab_scene", "Scene", "Сцена"},
    {"tab_editor", "Editor", "Редактор"},
    {"tab_engine", "Engine", "Движок"},
    {"tab_audio", "Audio", "Аудио"},
    {"tab_settings", "Settings", "Настройки"},
    {"brand_sub", "engine", "движок"},
    {"hierarchy", "Hierarchy", "Иерархия"},
    {"inspector", "Inspector", "Инспектор"},
    {"project", "Project", "Проект"},
    {"assets", "Assets", "Ассеты"},
    {"console", "Console", "Консоль"},
    {"viewport", "World", "Мир"},
    {"language", "Language", "Язык"},
    {"english", "English", "English"},
    {"russian", "Russian", "Русский"},
    {"master_volume", "Master volume", "Громкость"},
    {"controls", "Controls", "Управление"},
    {"forward", "Forward", "Вперёд"},
    {"back", "Back", "Назад"},
    {"left", "Left", "Влево"},
    {"right", "Right", "Вправо"},
    {"jump", "Jump", "Прыжок"},
    {"sprint", "Sprint", "Бег"},
    {"crouch", "Crouch", "Присед"},
    {"attack", "Attack", "Атака"},
    {"dodge", "Dodge", "Уклон"},
    {"interact", "Interact", "Взаимодействие"},
    {"invert_y", "Invert look Y", "Инверсия взгляда по Y"},
    {"mouse_sens", "Look sensitivity", "Чувствительность взгляда"},
    {"place_in_world", "Click a file to place it in the world.", "Нажмите файл — он появится в мире."},
    {"save_settings", "Save settings", "Сохранить настройки"},
    {"builtin_sounds", "Built-in sounds", "Встроенные звуки"},
    {"preview", "Play sound", "Прослушать"},
    {"import_wav", "Import WAV", "Импорт WAV"},
    {"script", "Script", "Скрипт"},
    {"apply", "Apply", "Применить"},
    {"axes", "Axes", "Оси"},
    {"content_help",
     "Create the world with code: character, item path, map path, ground, cube, sphere, plane. Press Apply.",
     "Создавайте мир кодом: character, item путь, map путь, ground, cube, sphere, plane. Нажмите Применить."},
    {"engine_help",
     "Game rules: gravity, speed, jump, clear r g b, volume, bind action Key. Play logic: on start / on update, play sound, if jump play jump.",
     "Правила игры: gravity, speed, jump, clear r g b, volume, bind действие Клавиша. Логика: on start / on update, play звук, if jump play jump."},
    {"unsaved", "Save changes before continuing?", "Сохранить изменения?"},
    {"save", "Save", "Сохранить"},
    {"discard", "Discard", "Не сохранять"},
    {"cancel", "Cancel", "Отмена"},
    {"viewport_hint",
     "RMB inspect around the selected object, W/S approach, A/D and Q/E pan. LMB drag to place on the map. Alt+LMB rotate. Axes only if checked.",
     "ПКМ — осмотр выбранного, W/S ближе/дальше, A/D и Q/E сдвиг. ЛКМ — перенос по карте. Alt+ЛКМ — вращение. Оси только с галочкой."},
    {"editor_opacity", "Editor opacity", "Прозрачность в редакторе"},
    {"editor_opacity_help",
     "Developer preview only. Play and the built game always render at 100% opacity unless a mesh slider is below 1.",
     "Только для разработчика. В Play и в игре непрозрачность всегда 100%, если у объекта ползунок не ниже 1."},
    {"opacity", "Opacity", "Непрозрачность"},
};

} // namespace

const char* Tr(UiLanguage language, const char* key) {
    for (const Pair& row : kTable) {
        if (std::strcmp(row.Key, key) == 0) {
            return language == UiLanguage::English ? row.En : row.Ru;
        }
    }
    return key;
}

} // namespace Nova::Editor
