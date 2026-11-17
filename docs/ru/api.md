# Справочник API

*[In English](../en/api.md)*

Все публичные типы, функции и константы, по заголовочным файлам. Как они работают вместе, описано на странице
[Как это устроено](how-it-works.md); [Рецепты](cookbook.md) показывают их в деле.

| Заголовок | Что в нём |
| --------- | --------- |
| [`dgx_ui.h`](#dgx_uih) | прямоугольники, состояние, виджет и его узел, дерево, раскладка, указатель, клавиши, фокус, всплывающее окно |
| [`dgx_ui_text.h`](#dgx_ui_texth) | шрифты и стили текста, текстовое поле, задание текста |
| [`dgx_ui_msg.h`](#dgx_ui_msgh) | сообщение, как предложить его дереву, декодеры, набранные символы |
| [`dgx_ui_post.h`](#dgx_ui_posth) | отправка сообщения через очередь FreeRTOS |
| [`dgx_ui_ticker.h`](#dgx_ui_tickerh) | сообщение раз в столько-то миллисекунд |
| [`dgx_ui_paint.h`](#dgx_ui_painth) | отрисовка, облики и их параметры |
| [`dgx_ui_di.h`](#dgx_ui_dih) | касание DGX-DI как указатель дерева |

Все публичные имена начинаются с `dgx_ui_` или `DGX_UI_`. В каждом заголовке есть `extern "C"` для C++. Версия 0.0.1:
имена и сигнатуры ещё могут измениться.

Деревом владеет одна задача. Все вызовы ниже делает она, кроме вызовов `dgx_ui_post.h`, которые можно делать из
любой задачи.

## dgx_ui.h

### Прямоугольники

```c
typedef struct dgx_ui_rect_t {
    int16_t x, y;
    int16_t w, h;
} dgx_ui_rect_t;
```

Прямоугольник экрана в пикселях. Без ширины или без высоты он пуст.

| Вызов | Что делает |
| ----- | ---------- |
| `dgx_ui_rect_t dgx_ui_rect(int x, int y, int w, int h)` | Прямоугольник из угла и размера. |
| `bool dgx_ui_rect_empty(const dgx_ui_rect_t *r)` | Пуст ли он. |
| `bool dgx_ui_rect_intersect(const dgx_ui_rect_t *a, const dgx_ui_rect_t *b, dgx_ui_rect_t *out)` | Общая часть двух прямоугольников, в `out`; им может быть `a` или `b`. `false` и нули, если общей части нет. |
| `int dgx_ui_rect_subtract(const dgx_ui_rect_t *box, const dgx_ui_rect_t *cut, dgx_ui_rect_t out[4])` | Части `box` вне `cut`: до четырёх прямоугольников, которые не перекрываются, первой идёт полоса сверху. Возвращает их число; 0, если `box` пуст или закрыт целиком. `x + w` и `y + h` у `box` не больше 32767. |

### Состояние

```c
typedef struct dgx_ui_state_t {
    uint8_t bits;
    int16_t value;
} dgx_ui_state_t;
```

| Бит | Что значит |
| --- | ---------- |
| `DGX_UI_HIDDEN` | не показан, не занимает места в строке или столбце, в него нельзя попасть, не получает фокус |
| `DGX_UI_DISABLED` | показан так, как его рисует облик в этом состоянии; в него нельзя попасть, не получает фокус |
| `DGX_UI_FOCUSED` | клавиши идут к нему |
| `DGX_UI_PRESSED` | нажат: пальцем или программой |

`value` — значение элемента управления, строка списка, картинка значка.

### Виджет

```c
typedef struct dgx_ui_widget_t {
    uint8_t        kind;
    uint8_t        flags;
    uint8_t        pad, gap;
    int16_t        at_x, at_y;
    int16_t        want_w, want_h;
    int16_t        min, max, step;
    int16_t        tag;
    dgx_ui_state_t state;
    const dgx_ui_picture_t *picture;
    dgx_ui_textbox_t       *text;
    const dgx_ui_look_t    *look;
    bool (*take)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg);
    void (*on)(dgx_ui_t *ui, int i, int what, void *ctx);
    void *ctx;
    void *own;
} dgx_ui_widget_t;
```

То, что программа говорит о виджете. Библиотека в него не пишет. На 32-битном чипе он занимает 52 байта. Ноль —
значение по умолчанию у каждого поля.

| Поле | Что значит |
| ---- | ---------- |
| `kind` | что это: см. виды ниже |
| `flags` | как он устроен: см. флаги ниже |
| `pad`, `gap` | у контейнера: сколько точек от его краёв до детей и между детьми |
| `at_x`, `at_y` | где он просит стоять, в точках от внутреннего края родителя; отрицательное — от дальнего края. В столбце учитывается только `at_x`, в строке — только `at_y` |
| `want_w`, `want_h` | размер в точках; 0 — по содержимому, а поперёк укладки — во весь контейнер |
| `min`, `max`, `step` | диапазон элемента управления и его шаг; первая и последняя строка списка; значения картинок значка |
| `tag` | номер, который дала ему программа; по нему ищет `dgx_ui_find()`. 0 — тега нет |
| `state` | состояние, с которого он начинает: `DGX_UI_HIDDEN`, `DGX_UI_DISABLED`, первое значение |
| `picture` | у картинки — одна; у значка — массив для значений `min..max` |
| `text` | его текстовое поле, в памяти |
| `look` | как он рисуется; `NULL` — облик его вида, такой есть у контейнера, рамки и надписи |
| `take` | вызывается для каждого сообщения, предложенного дереву; `true` забирает сообщение |
| `on` | ей сообщается, что сделал с виджетом пользователь: см. события ниже |
| `ctx` | передаётся в `on()` |
| `own` | данные виджета собственного вида программы |

**Виды**

| Вид | Что это |
| --- | ------- |
| `DGX_UI_BOX` | контейнер: ставит детей столбцом или строкой, иначе они стоят там, где просят |
| `DGX_UI_FRAME` | рамка: контейнер с заливкой и границей |
| `DGX_UI_LABEL` | надпись: текст |
| `DGX_UI_PICTURE` | одна картинка |
| `DGX_UI_ICON` | значок: картинку из массива выбирает значение; вне `min..max` картинки нет |
| `DGX_UI_LIST` | список: строки, одна выбрана — это значение, `min` — первая строка. Облика у него пока нет |
| `DGX_UI_SWITCH` | переключатель: значение 0 или 1 |
| `DGX_UI_SLIDER` | ползунок: значение `min..max` с шагом `step` |
| `DGX_UI_BUTTON` | кнопка: срабатывает по нажатию |
| `DGX_UI_INPUT` | поле ввода |
| `DGX_UI_DROPDOWN` | выпадающий список: кнопка, которая показывает выбранное значение. Самого списка пока нет: когда кнопка сработала, программа открывает своё всплывающее окно |
| `DGX_UI_OWN` | свой вид: рисуется обликом программы, данные лежат в `own` |

**Флаги**

| Флаг | Что значит |
| ---- | ---------- |
| `DGX_UI_COLUMN` | контейнер ставит детей одного под другим |
| `DGX_UI_ROW` | контейнер ставит детей рядом |
| `DGX_UI_STRETCH` | шириной со столбец или высотой со строку, какой бы размер у него ни был |
| `DGX_UI_SWIPE` | ползунок двигается на столько, на сколько ушёл палец от места касания; вся ширина — весь диапазон. Короткое касание без движения считается нажатием |
| `DGX_UI_DRAG` | кнопку можно не только нажать, но и тащить вверх или вниз, как ручку полосы прокрутки: `on()` получает `DGX_UI_CHANGED` при каждом движении |
| `DGX_UI_CANVAS` | оставлен под холст виджета; пока не используется |

**События**: что сообщается функции `on()` в `what`.

| Событие | Когда |
| ------- | ----- |
| `DGX_UI_ACTIVATED` | кнопку, строку списка или выпадающий список нажали пальцем или клавишей; в поле ввода набрали «готово» |
| `DGX_UI_CHANGED` | пользователь изменил значение: пальцем, клавишей или набранной буквой |
| `DGX_UI_FOCUS` | виджет получил фокус |
| `DGX_UI_BACK` | в поле ввода набрали «отмена» |
| `DGX_UI_NO_MEMORY` | оставлено под холст виджета; пока не сообщается |

`on()` сообщается только то, что сделал пользователь, и никогда то, что выставила программа. Внутри неё можно менять
и удалять виджеты, в том числе тот, для которого она вызвана.

### Картинка

```c
typedef struct dgx_ui_picture_t {
    const dgx_image_t *image;
    const uint16_t    *rgb565;
    const uint8_t     *map;
    int16_t            width, height;
} dgx_ui_picture_t;
```

Картинка DGX в `image`: её рисует `dgx_ui_look_picture`, если она в 1 бит. Или строки RGB565, старший байт первым, в
`rgb565`, с шириной `width` и высотой `height`: их рисует `dgx_ui_look_rgb565`. `map` — для картинок в 4 и 8 бит,
которые пока не рисуются.

`void dgx_ui_picture_size(const dgx_ui_picture_t *p, int *w, int *h)` даёт размер и той и другой.

### Узел

```c
typedef struct dgx_ui_node_t {
    const dgx_ui_widget_t *widget;
    int16_t                parent;
    int16_t                child, next;
    int16_t                x, y, w, h;
    dgx_ui_state_t         state_value;
    dgx_ui_state_t         state_drawn;
    uint8_t                marks;
} dgx_ui_node_t;
```

Виджет в дереве, 28 байт памяти на 32-битном чипе. Программа его читает, но не пишет.

| Поле | Что значит |
| ---- | ---------- |
| `widget` | то, что о нём сказала программа; `NULL` — место свободно |
| `parent`, `child`, `next` | номера родителя, первого ребёнка и следующего брата; -1, если такого нет |
| `x`, `y`, `w`, `h` | куда он разложен, в пикселях экрана |
| `state_value` | его состояние, какое есть: `state_value.value` — значение элемента управления |
| `state_drawn` | его состояние, каким оно было нарисовано в последний раз: принадлежит отрисовке |
| `marks` | пометки библиотеки: см. ниже |

| Пометка | Что значит |
| ------- | ---------- |
| `DGX_UI_MARK_CHANGED` | изменилось содержимое |
| `DGX_UI_MARK_WHOLE` | нужно нарисовать целиком |
| `DGX_UI_MARK_FIT` | текст нужно заново подобрать под поле |
| `DGX_UI_MARK_TIME` | облик попросил время: его вызовут снова на следующем проходе |
| `DGX_UI_MARK_HARD_SIZE` | оставлена под холст виджета; пока не используется |

### Дерево

`dgx_ui_t` — дерево и то, что с ним сейчас делают указатель и клавиши, 96 байт. Программа заполняет его вызовом
`dgx_ui_init()` и меняет только через вызовы. Поля, которые программа может читать: `focus` — номер виджета в фокусе,
`pressed` — номер виджета, на котором удерживается указатель (оба -1, если такого нет), `popup` — открытое поверх
дерева окно.

`DGX_UI_BLOCK` равно 64: узлы сверх массива программы выделяются блоками такого размера.

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_init(dgx_ui_t *ui, dgx_ui_node_t *first, int first_size, int unit)` | Дерево в массиве программы на `first_size` узлов; `NULL` и 0, если все узлы должны выделяться блоками. `unit` — сколько пикселей в точке. |
| `void dgx_ui_free(dgx_ui_t *ui)` | Все виджеты удалены, блоки возвращены. |
| `int dgx_ui_add(dgx_ui_t *ui, int parent, const dgx_ui_widget_t *w)` | Виджет добавлен последним ребёнком `parent`; первый добавленный становится корнем, каким бы ни был `parent`. Возвращает его номер; -1, если такого родителя нет или нет памяти на блок. Дерево запоминает указатель: виджет должен жить, пока он в дереве. |
| `int dgx_ui_insert(dgx_ui_t *ui, int parent, int before, const dgx_ui_widget_t *w)` | То же, но перед ребёнком `before`; -1 — последним. |
| `void dgx_ui_remove(dgx_ui_t *ui, int i)` | Виджет удалён со всем, что под ним. Фокус и нажатие внутри него сбрасываются, никому ничего не сообщается. Его номер достанется виджету, добавленному позже. |
| `int dgx_ui_find(const dgx_ui_t *ui, int parent, int tag)` | Первый виджет с таким тегом под `parent`, в порядке дерева; `parent` -1 — во всём дереве. -1, если такого нет. |
| `dgx_ui_node_t *dgx_ui_node(const dgx_ui_t *ui, int i)` | Узел по номеру; `NULL` за пределами дерева. |
| `const dgx_ui_widget_t *dgx_ui_widget(const dgx_ui_t *ui, int i)` | Виджет по номеру; `NULL`, если под этим номером виджета нет. |

### Раскладка

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_set_root(dgx_ui_t *ui, int x, int y, int w, int h)` | Прямоугольник, в который раскладывается корень, в пикселях. Без этого вызова корень занимает весь экран. |
| `void dgx_ui_relayout(dgx_ui_t *ui)` | Дерево будет разложено заново перед следующей отрисовкой или проверкой указателя: вызывается после того, как программа изменила виджет, который держит в своей памяти. |
| `void dgx_ui_screen(dgx_ui_t *ui, int width, int height, int turn)` | Экран, на котором показано дерево. Отрисовка вызывает её на каждом проходе; программа — в тесте без отрисовки. При другом размере или другом `turn` дерево раскладывается заново и рисуется целиком. |
| `bool dgx_ui_layout(dgx_ui_t *ui)` | Раскладывает дерево сейчас, если нужно; `true`, если разложила. Её вызывают отрисовка и указатель. |

### Смена состояния

Ни один из этих вызовов не сообщает ничего функции `on()`.

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_set_value(dgx_ui_t *ui, int i, int value)` | Задаёт значение; оно остаётся в пределах `min..max`, если `min < max`. Значение значка не ограничивается: вне диапазона он не показывает картинку. |
| `void dgx_ui_set_state(dgx_ui_t *ui, int i, unsigned bits, bool on)` | Ставит или снимает `DGX_UI_DISABLED` или `DGX_UI_PRESSED`. Отключённый виджет теряет фокус, и нажатие пальца на нём заканчивается. |
| `void dgx_ui_show(dgx_ui_t *ui, int i, bool shown)` | Показывает или скрывает; его строка или столбец раскладывается заново. Скрытый виджет теряет фокус и нажатие пальца. |
| `void dgx_ui_redraw(dgx_ui_t *ui, int i)` | На следующем проходе виджет рисуется целиком, со всем, что под ним. |
| `bool dgx_ui_hidden(const dgx_ui_t *ui, int i)` | Скрыт ли он: сам или виджетом над ним. |

### Указатель

```c
typedef struct dgx_ui_pointer_t {
    int16_t  x, y;
    bool     down, pressed, released;
    bool     swipe;
    uint32_t ms;
} dgx_ui_pointer_t;
```

Указатель сейчас: где он, в пикселях экрана; `down` — опущен; `pressed` и `released` — опустился и поднялся с
прошлого раза; `swipe` — сейчас сообщён свайп; `ms` — время по часам программы, тем же, что передаются в
`dgx_ui_time()`.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_pointer(dgx_ui_t *ui, const dgx_ui_pointer_t *pointer)` | Передаёт указатель дереву, а сначала его всплывающему окну. Виджет, на который он опустился, нажат, пока указатель не поднимется, куда бы он ни ушёл. Если указатель отпущен не позже чем через `DGX_UI_CLICK_MS`, без свайпа и без перетаскивания, это нажатие. Возвращает `true`, если указатель взял виджет. |
| `int dgx_ui_hit(dgx_ui_t *ui, int x, int y)` | Самый глубокий виджет под пикселем, который отвечает на указатель; -1, если такого нет. |
| `bool dgx_ui_holding(const dgx_ui_t *ui)` | Держит ли указатель виджет дерева или его окна. |
| `bool dgx_ui_dragging(const dgx_ui_t *ui)` | Тащат ли виджет, который держит указатель: ползунок, список, кнопку с `DGX_UI_DRAG`. Такой виджет владеет пальцем. |

`DGX_UI_CLICK_MS` равно 500. `DGX_UI_KNOB` равно 3: ширина ручки ползунка в точках. `DGX_UI_ROW_GAP` равно 12:
верхняя и нижняя двенадцатая часть строки списка не принадлежат ни одной строке.

### Клавиши и фокус

| Клавиша | Что делает |
| ------- | ---------- |
| `DGX_UI_KEY_NEXT`, `DGX_UI_KEY_PREV` | фокус переходит к следующему или предыдущему виджету, который его принимает; в списке сначала перебираются строки |
| `DGX_UI_KEY_ACTIVATE` | срабатывает кнопка, строка или выпадающий список в фокусе, переключатель меняет положение |
| `DGX_UI_KEY_MORE`, `DGX_UI_KEY_LESS` | ползунок в фокусе на шаг вверх или вниз, переключатель включён или выключен |
| `DGX_UI_KEY_BACK` | виджеты её не берут: достаётся программе |

`DGX_UI_KEYS` — сколько всего клавиш.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_key(dgx_ui_t *ui, int key)` | Передаёт клавишу дереву, а сначала его окну; `true`, если её взял виджет. |
| `bool dgx_ui_buttons(dgx_ui_t *ui, uint32_t held, uint32_t now_ms)` | Кнопки, которые удерживаются сейчас. Каждая, которую только что нажали, становится своей клавишей; «дальше», «назад», «больше» и «меньше» повторяются, пока удерживаются. `true`, если клавишу взял виджет. |
| `bool dgx_ui_time(dgx_ui_t *ui, uint32_t now_ms)` | Шаг времени логики: удерживаемая клавиша повторяется, когда пришло её время. `true`, пока такая клавиша удерживается: цикл просит время. |
| `void dgx_ui_focus(dgx_ui_t *ui, int i)` | Ставит фокус на виджет, которому сообщается `DGX_UI_FOCUS`; -1 — ни на какой. |

Кнопки — биты геймпада, те же, что в DGX-DI и DGX-GE:

| Кнопка | Клавиша |
| ------ | ------- |
| `DGX_UI_PAD_UP`, `DGX_UI_PAD_DOWN` | «назад», «дальше» |
| `DGX_UI_PAD_LEFT`, `DGX_UI_PAD_RIGHT` | «меньше», «больше» |
| `DGX_UI_PAD_A` | «выбрать» |
| `DGX_UI_PAD_B`, `DGX_UI_PAD_BACK` | «отмена» |
| `DGX_UI_PAD_START` | никакая |

Удерживаемая клавиша повторяется через `DGX_UI_REPEAT_FIRST_MS`, 500 мс, после нажатия и дальше каждые
`DGX_UI_REPEAT_MS`, 100 мс.

### Всплывающее окно

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_popup_open(dgx_ui_t *ui, dgx_ui_t *popup, int x, int y, int w, int h, unsigned flags)` | Открывает одно дерево поверх другого в прямоугольнике экрана, в пикселях; уже открытое окно сначала закрывается. |
| `void dgx_ui_popup_close(dgx_ui_t *ui)` | Закрывает окно; на следующем проходе дерево под ним рисуется там заново. |

| Флаг | Что значит |
| ---- | ---------- |
| `DGX_UI_POPUP_MODAL` | окно забирает каждый указатель и каждую клавишу; «отмена» остаётся программе |
| `DGX_UI_POPUP_CLOSE_OUTSIDE` | палец, опустившийся снаружи, закрывает окно |

### Прочее

`int dgx_ui_textbox_height(const dgx_ui_textbox_t *box)`: высота, которую просит текстовое поле, — его строки,
умноженные на высоту строки первого шрифта. Её вызывает раскладка.

## dgx_ui_text.h

### Шрифты и стили

```c
typedef struct dgx_ui_font_variant_t {
    dgx_font_t *font;
    int16_t     height;
    uint8_t     narrow;
} dgx_ui_font_variant_t;

typedef struct dgx_ui_text_style_t {
    const dgx_ui_font_variant_t *variants;
    uint8_t                      count;
    uint8_t                      flags;
    dgx_font_align_t             align;
    uint16_t                     scroll_ms;
} dgx_ui_text_style_t;
```

Вариант — шрифт, которым можно нарисовать текст: матричный или точечный шрифт DGX. `height` и `narrow` — для
рукописных шрифтов, которые пока не рисуются; оставьте их нулевыми.

Стиль перечисляет варианты, начиная с лучшего, и их число `count`. `align` — `DGX_FONT_ALIGN_LEFT`, `_CENTER` или
`_RIGHT` из DGX. `scroll_ms` — сколько важный текст показывает строку, прежде чем прокрутиться вверх; 0 — он
прокручивается вручную.

| Флаг | Что значит |
| ---- | ---------- |
| `DGX_UI_TEXT_MIDDLE` | текст в меньшем варианте стоит посередине высоты, а не на базовой линии первого варианта |
| `DGX_UI_TEXT_IMPORTANT` | текст, который не помещается в свои строки, прокручивается, а не обрезается; для `dgx_ui_look_lines` |

`int dgx_ui_variant_line(const dgx_ui_font_variant_t *v)`: высота строки варианта.

### Текстовое поле

```c
struct dgx_ui_textbox_t {
    char                      *buf;
    uint16_t                   size;
    uint8_t                    lines;
    uint8_t                    variant;
    const dgx_ui_text_style_t *style;
    uint16_t                   top;
    uint8_t                    letter_ms;
    uint8_t                    flags;
    uint32_t                   since_ms;
    uint16_t                   letters_drawn;
    uint16_t                   total_lines, total_letters;
};
```

Строка виджета и то, как она раскладывается, 28 байт. Программа задаёт `buf`, `size`, `lines` и `style` через
`dgx_ui_textbox_init()` и может читать `buf`. Остальное принадлежит библиотеке: вариант, в который подобран текст,
первая показанная строка и то, что считает облик текста в несколько строк. Флаги `DGX_UI_TEXTBOX_START`,
`DGX_UI_TEXTBOX_CAME`, `DGX_UI_TEXTBOX_ENDED` и `DGX_UI_TEXTBOX_COUNTED` тоже принадлежат библиотеке.

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_textbox_init(dgx_ui_textbox_t *box, char *buf, int size, int lines, const dgx_ui_text_style_t *style)` | Текстовое поле поверх буфера программы на `size` байт, пустое, на столько-то строк. Заданный текст обрезается до `size - 1` байт. |
| `bool dgx_ui_text_printf(dgx_ui_t *ui, int i, const char *format, ...)` | Задаёт текст виджета как `printf()`; он обрезается по буферу на целой букве UTF-8. `true`, если текст изменился. При буфере до 256 байт тот же самый текст ничего не меняет; при буфере длиннее текст считается изменённым каждый раз. |
| `bool dgx_ui_text_vprintf(dgx_ui_t *ui, int i, const char *format, va_list args)` | То же с `va_list`. |
| `bool dgx_ui_text_set(dgx_ui_t *ui, int i, const char *text)` | Задаёт текст как есть. |
| `void dgx_ui_text_appear(dgx_ui_t *ui, int i, int letter_ms)` | Со следующего прохода текст появляется по буквам, буква раз в столько-то миллисекунд; 0 — весь сразу. Текст, заданный позже, тоже появляется. Для `dgx_ui_look_lines`. |
| `void dgx_ui_text_scroll(dgx_ui_t *ui, int i, int lines)` | Сдвигает неважный текст на столько-то строк вверх, а при отрицательном числе вниз, пока есть строки. Для `dgx_ui_look_lines`. |

Задание текста ничего не рисует и не измеряет: отрисовка подбирает шрифт и рисует текст на следующем проходе.

## dgx_ui_msg.h

```c
struct dgx_ui_msg_t {
    int     what;
    uint8_t flags;
    uint8_t count;
    union {
        uint8_t bytes[DGX_UI_MSG_BYTES];
        struct {
            void  *data;
            size_t size;
            void (*done)(void *data, size_t size);
        } ref;
    };
};
```

Сообщение, 20 байт на 32-битном чипе: элемент очереди программы. `what` — номер, который выбрала программа. По
значению `count` байт лежат в `bytes`, не больше `DGX_UI_MSG_BYTES`, то есть 12. По ссылке во `flags` стоит
`DGX_UI_MSG_REF`, а в `ref` лежат данные, их размер и функция, которая вызывается, когда сообщение выбрасывают.
`DGX_UI_MSG_KEPT` помечает сообщение, данные которого виджет оставил себе.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_msg_value(dgx_ui_msg_t *msg, int what, const void *data, size_t size)` | Заполняет сообщение по значению; `false`, если данных больше 12 байт. |
| `void dgx_ui_msg_ref(dgx_ui_msg_t *msg, int what, void *data, size_t size, void (*done)(void *, size_t))` | Заполняет сообщение по ссылке. |
| `bool dgx_ui_take(dgx_ui_t *ui, dgx_ui_msg_t *msg)` | Предлагает сообщение каждому виджету дерева в его порядке, скрытым тоже, потом виджетам его окна, пока кто-нибудь не заберёт. `true`, если забрали. Сообщение при этом не выбрасывается. |
| `void dgx_ui_msg_done(dgx_ui_msg_t *msg)` | Выбрасывает сообщение: для данных по ссылке один раз вызывает `done()`. Для сообщения по значению и для того, которое оставил себе виджет, не делает ничего. Цикл вызывает её один раз для каждого сообщения. |
| `void dgx_ui_msg_keep(dgx_ui_t *ui, dgx_ui_msg_t *copy)` | Вызывается в `take()` виджета: данные остаются у виджета и после обхода. Сообщение копируется в `copy`; эту копию виджет отдаёт в `dgx_ui_msg_done()`, когда данные больше не нужны. Кто оставил данные себе, тот забрал сообщение. Второй вызов и вызов вне `take()` дают сообщение без данных. |
| `esp_err_t dgx_ui_msg_copy(const dgx_ui_msg_t *msg, int what, void *out, size_t size)` | Копирует наружу данные сообщения с номером `what`. `ESP_OK`; `ESP_ERR_NOT_FOUND` — номер другой; `ESP_ERR_INVALID_SIZE` — номер тот, а размер данных другой, это попадает в журнал. |
| `const void *dgx_ui_msg_data(const dgx_ui_msg_t *msg, size_t *size)` | Данные сообщения и их размер, там, где они лежат; действительны, пока сообщение не выброшено. |
| `bool dgx_ui_typed(dgx_ui_t *ui, const uint32_t *code_points, int count)` | Набранные символы — в поле ввода с фокусом, сначала в окне, потом в дереве. `true`, если их взяло поле. |

`DGX_UI_DECODER(name, number, type)` определяет `esp_err_t msg_decode_<name>(const dgx_ui_msg_t *msg, type *out)`:
данные сообщения с этим номером и размером этого типа копируются наружу.

Символы редактирования, какими их набирает клавиатура DGX-DI: `DGX_UI_CP_ERASE` (U+0008) убирает последнюю букву,
`DGX_UI_CP_DONE` (U+000A) сообщает полю `DGX_UI_ACTIVATED`, `DGX_UI_CP_CANCEL` (U+001B) сообщает ему `DGX_UI_BACK`.

## dgx_ui_post.h

Отправка, через очередь FreeRTOS, которую программа создаёт с элементами размером `sizeof(dgx_ui_msg_t)`. Ни один
вызов не ждёт: если очередь полна, сообщение теряется, а вызов возвращает `false`.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size)` | Сообщение по значению, до 12 байт, из любой задачи. |
| `bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *data, size_t size))` | Сообщение по ссылке. Данные принадлежат сообщению, пока не вызвана `done()`, в задаче, владеющей деревом. Если сообщение не отправлено, `done()` не вызывается и данные остаются у отправителя. |
| `bool dgx_ui_post_const(QueueHandle_t q, int what, const void *data, size_t size)` | Сообщение по ссылке на данные, которые живут вечно. |
| `bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken)` | Сообщение по значению из прерывания; `*woken` — как его ставит `xQueueSendFromISR()`. Лежит в IRAM. |

`DGX_UI_MESSAGE(name, number, type)` связывает тип с номером: определяет
`bool msg_post_<name>(QueueHandle_t q, const type *value)` и декодер `msg_decode_<name>()`. Тип больше 12 байт не
скомпилируется.

## dgx_ui_ticker.h

```c
typedef struct dgx_ui_ticker_t {
    void            *timer;
    QueueHandle_t    queue;
    int              what;
    volatile uint8_t on_its_way;
} dgx_ui_ticker_t;
```

Сообщение с заданным номером раз в столько-то миллисекунд, в очереди не больше одного: следующее уходит, только
когда для предыдущего вызвана `dgx_ui_msg_done()`. На чипе таймер — `esp_timer`, на цели `linux` ESP-IDF — таймер
FreeRTOS.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_ticker_start(dgx_ui_ticker_t *ticker, QueueHandle_t queue, int what, uint32_t period_ms)` | Тики начиная с этого момента. Перед первым запуском структура должна быть обнулена; статическая обнулена всегда. `false` — таймер создать не удалось или тикер уже идёт. |
| `void dgx_ui_ticker_stop(dgx_ui_ticker_t *ticker)` | Тиков больше нет. Структура должна жить, пока после этого `on_its_way` не станет 0: тик, который ещё лежит в очереди, пишет в неё, когда его выбрасывают. |

## dgx_ui_paint.h

### Отрисовка

```c
typedef struct dgx_ui_paint_t {
    dgx_screen_t   *scr;
    const uint32_t *colors;
    uint16_t        color_count;
    uint8_t         background;
    dgx_ui_rect_t   cover;
    int16_t         ox, oy;
    /* the painter's */
    uint8_t         paper;
    bool            whole;
    bool            composing;
    uint8_t         part, parts;
    dgx_ui_rect_t   popup_cover;
    dgx_screen_t   *rgb565;
    dgx_ui_rect_t   cover_was;
} dgx_ui_paint_t;
```

Как дерево рисуется на экране. Программа заполняет первые поля и хранит структуру от прохода к проходу.

| Поле | Что значит |
| ---- | ---------- |
| `scr` | экран DGX |
| `colors`, `color_count` | таблица цветов: номер из облика в цвет экрана, как его упаковывает `dgx_screen_rgb()` |
| `background` | номер цвета под деревом |
| `cover` | прямоугольник, в котором не рисуют: полоса клавиатуры DGX-DI, пока она открыта; иначе пустой |
| `ox`, `oy` | пиксель экрана дерева, который лежит в точке (0, 0) экрана `scr`; 0 или место буфера при сборке прямоугольника |

Поля после них принадлежат отрисовке. Четыре из них читает облик, пока рисует: `paper` — номер цвета под виджетом;
`whole` — на экране от виджета нет ничего правильного; `composing` — рисует `dgx_ui_paint_area()`, и запоминать
ничего не надо; `part` и `parts` — для какой части коробки виджета этот вызов и сколько частей всего.

| Вызов | Что делает |
| ----- | ---------- |
| `bool dgx_ui_paint(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms)` | Раскладывает дерево, если нужно, потом рисует то, что изменилось, одним пакетом, и поверх — его окно. Отсечение экрана возвращается таким, каким было. `true`, если какой-нибудь виджет просит ещё времени. |
| `void dgx_ui_paint_area(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)` | Собирает прямоугольник экрана дерева от корня вниз: фон, потом каждый виджет, который его задевает, целиком, под отсечением. Ничего не помечается как нарисованное. Пиксели могут писаться больше одного раза. |
| `void dgx_ui_paint_free(dgx_ui_paint_t *paint)` | Возвращает то, что отрисовка создала для себя: объект экрана для облика RGB565. Структурой можно пользоваться дальше. |
| `uint32_t dgx_ui_color(const dgx_ui_paint_t *paint, int index)` | Цвет из таблицы; 0 за её концом. |

### Облик

```c
struct dgx_ui_look_t {
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
    const void *params;
    int16_t     fill;
};
```

Как рисуется виджет. `draw` рисует виджет `i` в его состоянии и возвращает `true`, чтобы её вызвали снова на
следующем проходе. `params` — параметры своего типа у каждого облика. `fill` — номер цвета, который облик оставляет
под детьми виджета; -1 — то, что там лежало. Правила, которых держится `draw`, описаны в рецепте
[Свой облик](cookbook.md#свой-облик).

`DGX_UI_PAPER`, 0xff, на месте бумаги в параметрах облика означает цвет под виджетом.

### Облики библиотеки

Их цвета — номера в таблице; по умолчанию 0 — бумага, 1 — чернила, 2 — заливка, 3 — граница.

| Облик | Для чего | Параметры |
| ----- | -------- | --------- |
| `dgx_ui_look_box` | контейнер: его бумага, когда он рисуется целиком. Облик `DGX_UI_BOX`, если не назван другой | нет |
| `dgx_ui_look_frame` | рамка: граница и заливка. Облик `DGX_UI_FRAME`, если не назван другой | `dgx_ui_frame_look_t` |
| `dgx_ui_look_text` | текст в одну строку, слишком длинный обрезается с многоточием. Облик `DGX_UI_LABEL`, если не назван другой | `dgx_ui_text_look_t` |
| `dgx_ui_look_lines` | текст в несколько строк: с переносом по словам, обрезается или прокручивается, буквы могут появляться по одной | `dgx_ui_text_look_t` |
| `dgx_ui_look_button` | кнопка и выпадающий список: лицевая сторона над тенью, текст посередине | `dgx_ui_button_look_t` |
| `dgx_ui_look_switch` | переключатель: дорожка с ручкой на одном конце | `dgx_ui_switch_look_t` |
| `dgx_ui_look_slider` | ползунок: дорожка, закрашенная до ручки | `dgx_ui_slider_look_t` |
| `dgx_ui_look_input` | поле ввода: текст в одну строку и, когда оно в фокусе, курсор после него | `dgx_ui_input_look_t` |
| `dgx_ui_look_picture` | картинка или значок в 1 бит, посередине коробки | `dgx_ui_picture_look_t` |
| `dgx_ui_look_rgb565` | картинка или значок RGB565. Нужны `CONFIG_DGX_ENABLE_VSCREEN` и 16-битный экран | `dgx_ui_picture_look_t` |

Облик, который не назвал ни один виджет, в программу не попадает.

```c
typedef struct dgx_ui_frame_look_t {
    uint8_t border; /* the colour of the border; the fill is the look's fill */
    uint8_t width;  /* of the border, in points */
} dgx_ui_frame_look_t;

typedef struct dgx_ui_text_look_t {
    uint8_t ink;   /* the letters */
    uint8_t paper; /* under them; DGX_UI_PAPER: what lies under the widget */
} dgx_ui_text_look_t;

typedef struct dgx_ui_button_look_t {
    uint8_t face, pressed, focused, disabled; /* the face in each state */
    uint8_t text, text_disabled;              /* the text */
    uint8_t shadow;                           /* right of the face and below it */
    uint8_t depth;                            /* of the shadow, in points; 0: a flat button */
} dgx_ui_button_look_t;

typedef struct dgx_ui_switch_look_t {
    uint8_t off, on, knob, focused; /* the track off and on, the knob, the knob when focused */
    uint8_t disabled;               /* the knob when disabled */
} dgx_ui_switch_look_t;

typedef struct dgx_ui_slider_look_t {
    uint8_t track, fill, knob, focused; /* the track, its filled part, the knob, the knob when focused */
    uint8_t disabled;                   /* the knob when disabled */
    uint8_t track_h;                    /* the height of the track, in points */
} dgx_ui_slider_look_t;

typedef struct dgx_ui_picture_look_t {
    uint8_t ink;   /* the pixels of a picture of 1 bit */
    uint8_t paper; /* the rest; DGX_UI_PAPER: what lies under the widget */
} dgx_ui_picture_look_t;

typedef struct dgx_ui_input_look_t {
    uint8_t ink, caret; /* the text, and the mark where the next letter goes */
    uint8_t paper;      /* under them; DGX_UI_PAPER: what lies under the widget */
} dgx_ui_input_look_t;
```

## dgx_ui_di.h

Для программы, в которой есть [DGX-DI](https://github.com/jef-sure/esp32-component-dgx-di): без неё заголовок пуст.
Всё в нём — `static inline`; dgx-ui от DGX-DI не зависит.

```c
typedef struct dgx_ui_di_gestures_t {
    uint8_t edge;       /* DGX_DI_EDGE_*: swiped from that edge */
    uint8_t swipe;      /* DGX_DI_SWIPE_*: swiped that way */
    bool    long_press; /* held long */
    bool    taken;      /* a widget took the finger: it went down on one */
} dgx_ui_di_gestures_t;
```

Жесты касания, которые достаются программе.

| Вызов | Что делает |
| ----- | ---------- |
| `void dgx_ui_di_pointer(dgx_ui_t *ui, const dgx_di_pointer_t *pointer, dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)` | Берёт первый палец уже прочитанного указателя, оставшийся после клавиатуры и пада DGX-DI, передаёт его в касание и отдаёт дереву. |
| `void dgx_ui_di_touch(dgx_ui_t *ui, const dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)` | Отдаёт дереву касание, которое программа сама приняла вызовом `dgx_di_touch_take()`. |

Пока палец держит виджет, `swipe` и `long_press` не сообщаются. `edge` сообщается, если только виджет, который держит
палец, не тащат.
