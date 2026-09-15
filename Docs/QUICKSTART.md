# NOVA3D — быстрый старт

## Сборка

```bash
cmake -S . -B build
cmake --build build --target NovaEditor Nova3D NovaTests
```

## Запуск (создание игры — через редактор)

| Что | Команда |
|-----|---------|
| **NOVA3D Editor** (сцены, ассеты, Play) | `cd ~/Desktop/NOVA3D && ./Tools/run.sh editor` |
| **Тесты** | `cd ~/Desktop/NOVA3D && ./Tools/run.sh test` |
| **Сборка** | `cd ~/Desktop/NOVA3D && ./Tools/run.sh build` |

Если окна нет: Mission Control / другой рабочий стол; или вручную:

```bash
cmake --build build --target NovaEditor
build/bin/NovaEditor.app/Contents/MacOS/NovaEditor
```

Лог: `~/Library/Logs/NOVA3D.log` — строка `Window created` значит редактор стартовал.

Runtime с другой сценой или проектом:

```bash
build/bin/Nova3D.app/Contents/MacOS/Nova3D --project /path/to/game
build/bin/Nova3D.app/Contents/MacOS/Nova3D --scene /path/to/scene.scene.json
```

## Проект

Корень игры — папка с `.nova/project.json` (в репозитории это сам `NOVA3D/`).

- **File → Open Project…** — открыть существующий проект.
- **File → New Project…** — выбрать пустую папку; создаётся `Assets/Scenes/main.scene.json`.
- **File → Open Scene…** / **Save Scene As…** — сцены в `Assets/Scenes/`.
- **File → Save Scene** (Cmd+S) — сохраняет сцену и обновляет `lastOpenedScene` в проекте.

## Редактор — управление

- **Hierarchy** — Cube / Sun / Camera / Delete; теги `[Cam]` `[Light]` `[Mesh]`.
- **Viewport** (большая панель по центру — кликни в **серую область** под подсказками):
  - **Move (M)** — перетаскивание объекта (Free / X / Y / Z), цветные оси на объекте.
  - **Rotate (R)** — ЛКМ вращает меш.
  - **ПКМ** — орбита камеры.
  - **Колёсико** — приближение/отдаление камеры.
- **Play** (кнопка в меню или **F5**) — копия сцены с автовращением куба; **Esc** / **Stop** — выход.
- **Play → Run Standalone Game** (**Cmd+Shift+G**) — сохраняет сцену и открывает `Nova3D.app` с текущим файлом.
- **Dup** / **Cmd+D** — дубликат выбранного объекта; имя в Inspector редактируется.

Свет: entity **Sun** в Hierarchy → Inspector (Ray Direction, Ambient).

Модели: Inspector → **Asset (OBJ/glTF)** — пути вроде `Assets/Models/triangle.gltf` или `Assets/Models/pyramid.obj`.

Лог: `~/Library/Logs/NOVA3D.log`
