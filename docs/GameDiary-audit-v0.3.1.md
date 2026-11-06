# Технический аудит GameDiary перед PSP Library v0.3.1

Дата аудита: 7 ноября 2026 г.

## 1. Объём и исходные точки

GameDiary был получен в отдельную директорию `D:\PSP Library\_audit\GameDiary`; исходный код не смешивался с PSP Library и не переносился в рабочие `src/` или `include/`.

Проверенные ревизии:

- GameDiary: `3f2d5d4` (`docs: update README to include stats dashboard and enhance web tools section`), 31 октября 2026 г.
- PSP Library: `cfbceee` (`Simplify PSP navigation state transitions`), 7 ноября 2026 г.

Проверены фактические реализации, заголовки, Makefile, генераторы ресурсов и текущие модули PSP Library. README использовался только как дополнительный источник контекста.

Короткий вывод: GameDiary полезен как источник отдельных технических приёмов, но не как готовый графический слой для переноса. Наиболее сильные идеи — заранее запечённые SDF-атласы, разделение layout/component/domain, асинхронное окно загрузки и многоступенчатое определение PSP/PS1 metadata. При этом текущие `image.c`, PSPLIB 2 и навигация PSP Library уже безопаснее и проще для своего назначения.

## 2. Сводная матрица решений

| Компонент | Как реализовано в GameDiary | Что уже есть в PSP Library | Решение |
|---|---|---|---|
| GU/framebuffer | 512-stride, два 480x272 RGBA buffer в VRAM, depth-buffer, единый 1 MiB display list, sync + VBlank перед swap | Два GU display list по 1 MiB, два framebuffer, depth reservation, cache writeback, sync + VBlank | Адаптировать только проверенные инварианты; не заменять текущий `gfx.c` целиком |
| Текст | UTF-8 decoder + BMP glyph map + T8 SDF atlas/CLUT; fallback на `intraFont` | 5x5 ASCII bitmap, прочие UTF-8 последовательности превращаются в `?` | Использовать уже в v0.3.1 узкий offline-атлас Latin+Cyrillic; не переносить весь SDF/CJK/intraFont stack |
| PNG/обложки | `texture_load_png()` полностью декодирует PNG до 512x512; carousel — окно из 5 текстур и worker thread | Streaming decode, downsample до 128x176, 8-slot LRU, failed-entry cache, декодирование между GU frames | Сохранить нашу `image.c`; взять только generation-token/state-machine как пример для будущей async-версии |
| UI layout | `Rect`, helpers, reusable components, отдельные screen callbacks, carousel domain отдельно от render | Всё в одном `ui.c`, но текущая навигация уже исправлена и покрыта core-тестами | Адаптировать theme/layout boundary; не копировать carousel и внешний вид |
| Ресурсы | 51 встроенный RGBA resource; все fonts и изображения compile-in | Только внешние PNG covers; маленький runtime renderer | Не переносить чужие изображения и большие splash/QR ресурсы |
| Tracker | PRX: apitype, SFO/PBP/UMD metadata, ICON0 capture, session thread и atomic storage | Только identity/session adapter без kernel hooks | Сохранить GameDiary как reference для отдельного будущего PRX; сейчас не реализовывать |

## 3. Шрифты — главный результат аудита

### 3.1 Как GameDiary рисует текст

Связка находится в `src/app/render/font.c` и `src/app/render/sdf_font.c`.

1. `font_init()` запускает `sdf_font_init()`, затем инициализирует `intraFont` и загружает `flash0:/font/jpn0.pgf` как fallback.
2. `sdf_font.c` декодирует UTF-8 в Unicode codepoint и ограничивает runtime lookup BMP (`0..65535`). Невалидные/неподдержанные символы в итоге заменяются на `?`.
3. Каждый атлас — PNG-маска + бинарный metadata blob. В blob есть заголовок, `uint16_t lookup[65536]` и массив метрик glyph.
4. PNG переводится в 8-bit T8 texture: в buffer сохраняется только первый канал distance field, а CLUT превращает значение в alpha. Это не RGBA-атлас, несмотря на устаревший комментарий около `decode_png()`.
5. При рисовании используется `GU_PSM_T8`, CLUT, bilinear filtering, alpha test и один GU sprite на glyph. Текстура переустанавливается только при смене страницы атласа.
6. `font_draw_game_name()` при наличии хотя бы одного отсутствующего glyph переводит всю строку на `intraFont`; это не per-glyph смешанный fallback.

README называет систему MSDF, но фактический pipeline в `tools/font_pipeline/font_generator.py` вызывает `msdf-atlas-gen -type sdf`. То есть это обычный SDF, не MSDF.

### 3.2 Атласы и фактические размеры

В checkout есть 2 Latin/Cyrillic page, 1 Symbols page и по 5 CJK page для JP и Simplified Chinese. PNG имеют размеры 188..284 px и после загрузки округляются до POT 256 или 512.

Оценка runtime для одного активного языка:

- Latin/Cyrillic: 2 x 256² x 1 byte = 128 KiB T8 texture.
- Symbols: 256² x 1 byte = 64 KiB.
- Один CJK group: 2 x 256² + 3 x 512² = 896 KiB T8 texture.
- Metadata BIN: примерно 268 KiB для Latin/Cyrillic, 133 KiB для Symbols и 673 KiB для одной CJK group. Большая часть — обязательная таблица `65536 * 2` байт на каждую страницу.
- Все compile-in font data (PNG + BIN, без текстового overhead C) — примерно 2.12 MiB: JP 840 KiB, SC 840 KiB, Latin/Cyrillic 293 KiB, Symbols 145 KiB.

В `image_resources.c` 51 встроенный resource; после POT/RGBA-конвертации они занимают около 3,289,088 байт. Два splash resource по 1 MiB каждый особенно плохо подходят как постоянные compile-in данные для PSP 1000.

Это рабочая система, но для PSP Library она избыточна: полные CJK page и обе языковые группы не нужны для первой версии, а `65536`-entry lookup на страницу можно заменить более компактной таблицей диапазонов или отсортированным массивом glyph.

### 3.3 Сравнение с текущим `src/psp/text.c`

Текущий renderer — намеренно минимальный 5x5 bitmap font. ASCII рисуется через GU rectangles; 1 glyph = до 25 маленьких `fill_rect`. UTF-8 sequence пропускается целиком, но вместо неё рисуется `?`. Это объясняет, почему кириллические названия сейчас не читаются, хотя байты в data model сохраняются.

Преимущества GameDiary:

- нормальные метрики и ширина glyph;
- UTF-8/кириллица без изменения PSPLIB 2;
- SDF сохраняет читаемость при нескольких размерах;
- T8 вместо RGBA экономит примерно 75% texture memory;
- атласы строятся offline, поэтому PSP не парсит TTF/OTF.

Недостатки:

- два renderer path (`sdf_font` и `intraFont`) и неясно лицензируемая runtime dependency;
- значительная compile-in память из-за CJK и больших lookup tables;
- сложная генерация ресурсов и дополнительные Python/fonttools/Pillow/MSDF build dependencies;
- fallback целой строки может менять визуальный стиль и метрики;
- UTF-8 decoder не является строгим валидатором Unicode и поддержка ограничена BMP.

### 3.4 Рекомендация по шрифтам

**Использовать уже в v0.3.1:** идею offline baked SDF/T8 atlas, но только для нужного набора: ASCII + кириллица + используемые UI symbols. Сохранить публичный API `pspl_text_width/draw/draw_clipped/draw_centered`, чтобы не менять UI/navigation и не затронуть storage.

Рациональный вариант:

- добавить небольшой `font_atlas.c/.h` рядом с `src/psp/text.c`;
- оставить один основной Latin+Cyrillic atlas, максимум две 256x256 T8 pages;
- сделать компактные метрики/lookup только для реально включённых codepoint;
- fallback оставить на текущий `?`, без `intraFont`;
- добавить host-тесты для UTF-8 width/clip на кириллице и проверку отсутствующего glyph;
- цвета glyph получать из theme API, а не хранить в font module.

**Не переносить:** `font.c`/`sdf_font.c` целиком, CJK fallback chain, `flash0:/font/jpn0.pgf`, большие embedded C arrays и `intraFont` dependency. Если позже понадобится CJK, включать только выбранную language pack и измерять PSP 1000 отдельно.

## 4. GU, VRAM и lifetime ресурсов

### 4.1 GameDiary

`renderer.c` использует:

- draw buffer offset `0x00000`;
- display buffer offset `0x88000`;
- depth buffer offset `0x110000`;
- stride 512, экран 480x272.

VRAM budget примерно такой:

- два RGBA framebuffer: `2 * 512 * 272 * 4` = 1,088 KiB;
- depth buffer: `512 * 272 * 2` = 272 KiB;
- всего около 1.33 MiB VRAM до прочих GU allocations.

На кадр GameDiary делает `sceGuStart(GU_DIRECT, list)`, vertices получает через `sceGuGetMemory()`, затем `sceGuFinish()`, `sceGuSync()`, VBlank и `sceGuSwapBuffers()`. Vertex cache writeback делается для CPU-RAM vertices; для texture resources writeback вызывается перед `sceGuTexImage()`.

Полезные инварианты:

- не переиспользовать/освобождать RAM texture до окончания GU list;
- не менять display list после `sceGuStart()` без завершения текущего списка;
- synchronise перед swap и перед освобождением данных, которые могли попасть в GU list;
- texture data держать 16-byte aligned и делать writeback.

Опасные места, которые нельзя переносить вслепую:

- `renderer_swap_buffers()` сам по себе только меняет буферы; caller обязан заранее сделать finish/sync и, где нужно, VBlank;
- `renderer.c` не проверяет все ошибки `sceGuStart()`/allocations;
- API `texture_draw()` не кодирует lifetime ownership, поэтому безопасность обеспечивается дисциплиной screen/cache layer, а не типами.

### 4.2 PSP Library

Текущий `src/psp/gfx.c` уже делает более защитную последовательность: два 1 MiB command lists, явный clear обоих framebuffer, `sceKernelDcacheWritebackAll()` перед sync/swap и проверки init/start. Это хорошо защищает от ранее найденных проблем с display list.

Но память command lists — это около 2 MiB обычной RAM, а не VRAM. Поэтому при будущей оптимизации её можно уменьшать только после измерения реального числа GU commands; сейчас это не причина переписывать renderer.

`src/psp/image.c` декодирует в обычную RAM, а `pspl_gfx_draw_texture()` привязывает texture только внутри активного кадра. Cache освобождается после завершения UI loop, что соответствует безопасному ownership model.

### 4.3 Решение

**Адаптировать как checklist:** explicit framebuffer offsets/stride, finish → cache writeback → sync → VBlank → swap, 16-byte alignment, vertices only from current list, texture lifetime at least until frame sync.

**Оставить нашу реализацию:** весь `gfx.c`, двойные lists и текущую модель `pspl_gfx_begin/end`. Не заменять рабочий код на GameDiary renderer: преимущество GameDiary здесь не настолько велико, чтобы оправдать повторное внесение ошибок lifetime.

## 5. Обложки и изображения

### 5.1 GameDiary

`texture_load_png()`:

- проверяет PNG signature;
- использует `png_read_png()`, то есть сначала получает полный декодированный image rows;
- принимает только изображения до 512x512;
- выделяет POT RGBA8888 texture без downsample;
- делает cache writeback;
- при ошибке освобождает libpng и texture memory.

Carousel cache в `carousel_state.c` имеет пять слотов (`CAROUSEL_CACHE_RADIUS=2`) и грузит окно `[current-2, current+2]`. Worker thread имеет очередь на 64 задания и 48 KiB stack. Mutex защищает slots, state проходит `EMPTY → PENDING → QUEUED → LOADED`, а `generation` отбрасывает поздний результат после смены фильтра. Это хорошая модель для асинхронной загрузки, но она добавляет thread/semaphore/mutex/shutdown paths.

### 5.2 PSP Library

`src/psp/image.c` уже лучше соответствует ограниченным PSP-ресурсам:

- PNG читает построчно;
- исходник ограничен 2048x2048, но результат downsample до 128x176;
- кеш имеет 8 слотов, LRU-like replacement и negative cache для missing/corrupt PNG;
- декодирование происходит только между синхронизированными кадрами;
- при ошибке приложение рисует platform placeholder и продолжает работу;
- PSPLIB 2 cover path остаётся source of truth.

Важная поправка к текущей оценке памяти: при POT-высоте worst-case 128x256 RGBA один cover занимает до 128 KiB, поэтому 8 слотов могут достигать примерно 1 MiB texture data, а не только ~720 KiB. Это всё равно существенно безопаснее потенциальных 5 x 512² RGBA у GameDiary.

### 5.3 Решение

**Не переписывать кеш.** GameDiary не даёт преимущества, которое оправдывает замену streaming/downsample cache. Взять только две идеи на будущее:

1. explicit state machine и generation token, если появится worker-thread PNG loading;
2. окно вокруг текущего выбранного элемента вместо preload всей библиотеки.

Не переносить `texture.c` и `carousel_state.c` в v0.3.1. Не переносить пятислотовый лимит: текущие 8 слотов нужны grid/list/detail и уже покрывают отображаемый кадр.

## 6. UI и layout

### 6.1 Что технически хорошо у GameDiary

`ui_layout.c` содержит маленькие чистые helpers `rect_padding`, `rect_center`, `rect_column`, `rect_row`. `ui_components.c` собирает cards, titles, menu items, hints и graphs из renderer/font/texture primitives. Screen modules имеют `init/update/draw/destroy` callbacks.

`game_list.c` отделяет carousel state от rendering: domain хранит infinite index, easing, filter mapping и cache slots; screen строит до семи visible items, сортирует их по depth и рисует. `game_details.c` отделяет загрузку icon/stats в init от draw/update, обрабатывает длинное имя через auto-fit/ellipsis и показывает несколько statistics pages.

Это полезнее как архитектурный образец, чем как визуальный шаблон. Внешний вид GameDiary переносить нельзя: у PSP Library должен остаться собственный dark/light card design, тёплые accents и семь accent palettes.

### 6.2 Сравнение с PSP Library

Текущий `src/psp/ui.c` проще: grid 3x2, list 6 rows, detail screen, paging and sorting through existing navigation API. Это соответствует v0.3 и не тащит в presentation layer worker thread, graph animation или carousel state.

Слабое место — colors и размеры собраны локальными `#define` в `ui.c`, а future Dark/Light + seven accents потребуют централизованного источника. Это не требует GameDiary code copy.

### 6.3 Решение

**Использовать в v0.3.1:**

- адаптировать `Rect` и чистые layout helpers как небольшие portable helpers;
- ввести `pspl_theme_t`/`pspl_colors_t` с `background`, `surface`, `surface_alt`, `text`, `muted`, `border`, `accent` и alpha variants;
- оставить `pspl_ui_run()` и существующую navigation state machine;
- добавить component-like helpers поверх текущих `pspl_gfx_*`: card, selected card, badge/pill, title row, stat row;
- использовать текущую clipping/ellipsis логику как контракт для длинных UTF-8 названий.

**Использовать как пример:** separation `init/update/draw/destroy`, depth sorting и bounded visible window.

**Не переносить:** GameDiary carousel, fixed coordinates, graphs, animation timings и screen appearance. Они дублируют/меняют наше решение и будут мешать сохранению исправленной навигации.

## 7. Будущий PRX Tracker

### 7.1 Реальные механизмы GameDiary

| Задача | Фактический механизм | Оценка для будущего Tracker |
|---|---|---|
| PSP/PS1/HB category | `sceKernelInitApitype()` + `apitype_detect_category()` | Сохранить как reference, таблицу apitype проверять на реальном CFW |
| PSP ID/title | `sctrlGetInitPARAM("DISC_ID"/"TITLE")` | Полезно, но kernel/CFW-specific |
| Executable path | `kuKernelInitFileName()` | Полезно для PBP/icon/SFO, требует PRX |
| PS1/POPS | apitype `0x144`, `0x155`, fallback `0x143`; parse embedded PBP SFO | Самая готовая часть для PS1 |
| Physical/virtual UMD | `kuKernelGetUmdFile()`, UMD readiness lookup, `disc0:/UMD_DATA.BIN`, `PARAM.SFO` | Нужны late retry и safety delay; нельзя читать disc0 слишком рано |
| ICON0 | `disc0:/PSP_GAME/ICON0.PNG`, PBP ICON0 section, sidecar `ICON0.PNG`/`icon0.png` | Переиспользовать алгоритм источников, но вынести в отдельный PRX/storage adapter |
| Session time | tracker thread, PSP timestamp, periodic flush, sleep gap 120 s, midnight split | Сохранить policy и тесты; storage format не переносить |
| Atomic persistence | temp file + rename/sync, chunked IO and yields | Сохранить принцип, адаптировать под PSPLIB 2 |
| GBA/other emulator ROM | в коде нет ROM scanner, emulator hook или GBA detection | Готового решения нет |

`src/plugin/tracker.c` ждёт framebuffer и initial settle, затем запускает периодический tick. Для UMD metadata может быть 2 секунды дополнительного ожидания и до 60 попыток с 3-секундным интервалом. Это объясняет, почему GameDiary не создаёт UNKNOWN database entry во время медленной UMD загрузки.

`icon_capture.c` намеренно не использует PIC0/PIC1 и firmware RCO resources; отсутствие ICON0 оставляет файл отсутствующим. Такой fail-safe лучше, чем сохранять неподтверждённое изображение.

### 7.2 Ограничения совместимости

GameDiary собирается против PSPSDK с kernel libraries, `kubridge`, `systemctrl` и CFW-specific exports. В README заявлены PSP 1000/2000/3000/Go и протестированные CFW, но в checkout нет достаточного доказательства совместимости именно с PSP Street E1000. Для Street, PRO/ME/ARK-4 и POPS нужен отдельный hardware/PPSSPP test matrix.

### 7.3 Рекомендация

**Сохранить для группы B:** detector/apitype/metadata repository, PBP/SFO parser, ICON0 source order, timestamp/sleep/midnight policy и atomic file write pattern.

**Не делать сейчас:** PRX hooks, tracker thread, icon capture, changes to PSPLIB 2 or navigation. Текущий `src/tracker/psp_tracker.c` оставить как portable adapter, пока не будет отдельного решения по kernel boundary и permissions.

## 8. Лицензии и происхождение ресурсов

| Компонент | Что найдено | Условия/решение |
|---|---|---|
| GameDiary source | repository `LICENSE`: MIT, copyright OniMock 2026 | При заимствовании кода сохранить MIT notice и copyright; до решения код не копировать |
| cJSON | `lib/cJSON` содержит собственный MIT notice | В PSP Library не нужен для v0.3.1; не переносить без необходимости |
| msdf-atlas-gen | upstream `LICENSE.txt`: MIT; `msdfgen` также MIT | Можно использовать только как build-time tool при сохранении notices; runtime tool в EBOOT не нужен |
| Noto Sans / Noto CJK / Noto Symbols | upstream Noto LICENSE: SIL OFL 1.1 | При embed/bundle включить OFL и copyright notice; не продавать font software отдельно; проверить exact font file/version |
| DejaVu Sans | официальный DejaVu license: Bitstream Vera/Arev terms | Сохранить notices; modified font нельзя распространять под зарезервированными именами |
| `tools/fonts/arial.ttf` | в GameDiary checkout нет license file/attribution, подтверждающей redistribution right | Не переносить и не использовать в PSP Library |
| `intraFont`/`flash0:/font/jpn0.pgf` | не vendored и не лицензирован отдельным файлом GameDiary; приходит из PSP environment/SDK | Не делать зависимость v0.3.1; fallback заменить на собственный `?`/запечённый atlas |
| `assets/image/*`, SVG, WAV, QR/splash/logo | per-file provenance/license в checkout не найден; README credits этого не закрывает | Не переносить. Для собственных ресурсов PSP Library использовать собственные/явно лицензированные assets |

Источники проверки: [GameDiary repository](https://github.com/OniMock/GameDiary), [GameDiary LICENSE](https://github.com/OniMock/GameDiary/blob/main/LICENSE), [msdf-atlas-gen MIT license](https://raw.githubusercontent.com/Chlumsky/msdf-atlas-gen/master/LICENSE.txt), [msdfgen MIT license](https://raw.githubusercontent.com/Chlumsky/msdfgen/master/LICENSE.txt), [Noto OFL 1.1](https://github.com/notofonts/noto-fonts/blob/main/LICENSE), [DejaVu license](https://dejavu-fonts.github.io/License.html), [cJSON license](https://github.com/DaveGamble/cJSON/blob/master/LICENSE).

## 9. Рекомендации по группам

### A. Стоит использовать уже в v0.3.1

1. **Offline SDF/T8 font atlas, узкий scope.**
   - Файлы-источники: GameDiary `src/app/render/sdf_font.c`, `tools/font_pipeline/font_generator.py`, `tools/font_pipeline/embed_generator.py`.
   - Перенос: только принцип/формат после уменьшения glyph set; интеграция в `src/psp/text.c` за API-compatible facade.
   - Причина: решает кириллицу и масштабирование без изменения PSPLIB 2.
   - RAM: ориентир 128–256 KiB texture data для Latin+Cyrillic pages плюс компактные metrics; CJK исключить.

2. **Централизованная theme palette.**
   - Файлы-источники: GameDiary `src/app/config/config.c`, `include/app/ui/ui_style.h`, `src/app/ui/ui_components.c`.
   - Перенос: собственный `theme.h/.c`, не GameDiary colors.
   - Причина: готовит Dark/Light и 7 accents, убирает локальные `COLOR_*` из `ui.c`.

3. **Малые layout/component helpers.**
   - Файлы-источники: GameDiary `src/app/ui/ui_layout.c/.h`, `ui_components.c`.
   - Перенос: только `Rect` helpers и минимальные card/title/stat primitives поверх `pspl_gfx`.
   - Причина: уменьшает дублирование и не затрагивает navigation.

4. **GU safety checklist.**
   - Файлы-источники: GameDiary `src/app/render/renderer.c`, `texture.c`.
   - Перенос: документация/invariants и тестовый checklist в PSP Library; не переписывать `gfx.c`.
   - Причина: предотвращает повтор display-list/texture-lifetime ошибок.

### B. Сохранить для будущего PRX Tracker

1. `src/plugin/apitype.c` — category mapping, включая POPS.
2. `src/plugin/metadata_repository.c` — layered metadata resolution, late UMD retry and spoof rejection.
3. `src/common/sfo_parser.c` — standalone SFO и embedded PBP SFO parser.
4. `src/plugin/icon_capture.c` + `src/common/utils.c` — ICON0 source order and bounded copy/extract.
5. `src/plugin/tracker.c` — timestamp policy, periodic flush, sleep-gap detection, midnight split.
6. `src/common/storage.c` / `src/plugin/tracker_db.c` — chunked IO, temp/rename/sync and no-large-stack approach.

Группа B — это reference и набор идей, не разрешение переносить файлы сейчас. Формат PSPLIB 2 не менять.

### C. Не стоит переносить

1. Full GameDiary renderer: дублирует текущий `src/psp/gfx.c` и снова открывает display-list/lifetime surface.
2. Full `sdf_font.c` + all CJK pages: слишком большой memory/dependency footprint для первой версии.
3. `intraFont` runtime fallback: dependency and licensing/provenance не закрыты.
4. GameDiary `texture.c`/5-slot worker carousel: текущий `src/psp/image.c` уже stream/downsample/fail-safe и работает без thread races.
5. GameDiary embedded image resources, splash, QR, logos, sound and SVG: нет достаточной per-resource provenance; внешний вид PSP Library другой.
6. Carousel UI, graphs and fixed screen coordinates: это дизайн GameDiary, а не техническая необходимость; также меняет существующую navigation model.
7. Any GBA/other emulator detector: в GameDiary готового механизма нет.

## 10. Обновлённый план PSP Library v0.3.1

1. **Зафиксировать baseline.** Не менять PSPLIB 2, `src/core/navigation.c`, tracker adapter и текущий image cache. Добавить memory/lifetime checklist.
2. **Theme boundary.** Вынести цвета из `src/psp/ui.c` в собственный theme module: Dark/Light + seven accent definitions, без изменения layout semantics.
3. **Typography.** Собрать минимальный Noto-based Latin+Cyrillic SDF/T8 atlas offline. Сохранить текущий `pspl_text_*` API, добавить UTF-8 width/clip tests, лицензии и attribution files.
4. **Cards/components.** Добавить небольшие `Rect`/card/title/stat helpers; сохранить текущие grid/list/details screens и исправленную навигацию.
5. **Textures.** Ничего не переписывать. Проверить текущий cache worst-case (~1 MiB texture data на 8 POT slots), corrupted PNG, missing file, repeated navigation и detail transition.
6. **GU QA.** Проверить framebuffer pair, two command lists, cache writeback, VBlank and freeing after sync в PPSSPP и на реальном PSP. При любой оптимизации lists сначала измерить command usage.
7. **Resource/legal QA.** Не брать GameDiary images/fonts без точной лицензии. Для выбранного Noto subset положить OFL notice и copyright рядом с generated atlas.
8. **Tracker research only.** После v0.3.1 отдельно спроектировать PRX boundary и compatibility matrix для PSP 1000/2000/3000/Go/Street, PRO/ME/ARK-4, UMD/ISO/POPS/Homebrew. GBA и другие эмуляторы считать отдельным исследованием, а не обещанной функцией.

Итоговое техническое решение: **адаптировать узкий SDF/T8 font pipeline и theme/layout boundaries; сохранить текущие GU, image cache, PSPLIB 2 и navigation; GameDiary PRX использовать как reference для будущего Tracker, но не переносить его сейчас.**
