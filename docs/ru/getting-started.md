# Начало работы

*[In English](../en/getting-started.md)*

dgx-ui — это лёгкий слой виджетов на [DGX](https://github.com/jef-sure/esp32-component-dgx), графической библиотеке
для ESP32. В этом руководстве мы добавим её в проект ESP-IDF и напишем первую программу, в три шага:

1. подключим компонент к проекту;
2. напишем программу из одного файла: значение CO2, которое присылает другая задача, показано в рамке, а под ним
   идут часы;
3. соберём её и запустим — на плате или в окне на компьютере.

В конце на дисплее будет вот это. Значение меняется пять раз в секунду, часы — раз в секунду, а между изменениями
задача, которая владеет дисплеем, спит:

![Значение в рамке, под ним часы и подпись](../images/first.png)

Нужно уметь собирать и прошивать проект ESP-IDF. Знать dgx-ui или DGX не требуется: каждая часть программы
объясняется там, где она появляется.

## Что нужно

- ESP-IDF 5.2 или новее.
- Дисплей, с которым работает [DGX](https://github.com/jef-sure/esp32-component-dgx). Можно и без дисплея: та же
  программа работает в окне на компьютере.
- DGX 0.11.6 или новее из ветки 0.11. Она подтянется сама вместе с dgx-ui.

## 1. Подключите компонент

Версия 0.0.32 — это тег в репозитории, в реестре компонентов ESP её пока нет. В `main/idf_component.yml` проекта:

```yaml
dependencies:
  dgx-ui:
    git: https://github.com/jef-sure/esp32-component-dgx-ui.git
    version: "0.0.32"
```

При следующей сборке менеджер компонентов возьмёт dgx-ui с GitHub, а DGX — из реестра, потому что dgx-ui её
требует.

DGX собирает только те шины и драйверы дисплеев, которые включены в проекте. Для ILI9341 на SPI в
`sdkconfig.defaults`:

```
CONFIG_DGX_ENABLE_SPI=y
CONFIG_DGX_ENABLE_SPI_ILI_9341=y
```

Остальные дисплеи и шины описаны в разделе
[Getting started у DGX](https://github.com/jef-sure/esp32-component-dgx#getting-started).

## 2. Напишите первую программу

Ниже `main/main.c` по частям: части в этом порядке и есть весь файл. Пример
[thin_paint](../../examples/thin_paint/main/thin_paint.c) из репозитория устроен так же, только дисплей у него стоит
вертикально, 240 на 320.

```c
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"
#include "esp_timer.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

### Дисплей

dgx-ui рисует на экране DGX, и ей всё равно, на каком. Здесь это ILI9341 платы CYD (ESP32-2432S028R), а если
программа собрана для компьютера — окно того же размера:

```c
#ifdef DGX_HOST_WINDOW
#include "dgx_host_screen.h"
static dgx_screen_t *display_init(void)
{
    return dgx_host_screen_init(320, 240, 16, NULL);
}
#else
#include "bus/dgx_spi_esp32.h"
#include "drivers/ili9341.h"
/* CYD: MOSI 13, MISO 12, CLK 14, CS 15, DC 2, the backlight 21, SPI at 40 MHz */
static dgx_screen_t *display_init(void)
{
    dgx_bus_protocols_t *bus = dgx_spi_init(SPI2_HOST, SPI_DMA_CH_AUTO, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_15,
                                            GPIO_NUM_2, 40 * 1000 * 1000, 0);
    dgx_screen_t        *scr = bus ? dgx_ili9341_init(bus, GPIO_NUM_NC, GPIO_NUM_21, 16, DgxScreenRGB) : NULL;
    if (scr) dgx_screen_orientation(scr, DgxScreenRightLeft, DgxScreenTopBottom, true); /* landscape, 320x240 */
    return scr;
}
#endif
```

### Сообщения

Сообщение — это номер, который выбирает программа, и до 12 байт данных. dgx-ui не занимает ни одного номера и
внутрь сообщения не заглядывает. `DGX_UI_MESSAGE` связывает тип с номером и даёт две функции: `msg_post_co2()`
отправляет сообщение, `msg_decode_co2()` читает его. Второе сообщение, «прошла секунда», данных не несёт.

```c
enum { MSG_CO2 = 1, MSG_SECOND = 2 };
typedef struct {
    int16_t ppm;
} co2_t;
DGX_UI_MESSAGE(co2, MSG_CO2, co2_t)
```

### Тексты

Надпись показывает текст текстового поля. Поле лежит в памяти: в нём буфер программы под строку и стиль. Стиль
перечисляет шрифты, которыми можно нарисовать текст, начиная с лучшего. Отрисовка берёт первый, в который текст
помещается целиком.

```c
static char                  co2_buf[24], clock_buf[16], note_buf[24];
static dgx_ui_textbox_t      co2_box, clock_box, note_box;
static dgx_ui_font_variant_t big[2], small[1]; /* the fonts are got by functions: filled at the start */
static dgx_ui_text_style_t   st_big, st_left, st_right;
```

### Что виджет делает с сообщением

Каждое сообщение предлагается каждому виджету, в порядке дерева. Функция `take()` виджета читает из сообщения
то, что ей нужно, и возвращает `false`. Если она вернёт `true`, виджет забрал сообщение, и следующие виджеты его не
увидят.

`dgx_ui_text_printf()` сравнивает новый текст с прежним. Если значение пришло то же самое, ничего не меняется и
ничего не рисуется.

```c
static bool co2_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "CO2 %d ppm", c.ppm);
    return false; /* read, not claimed: another widget may want it too */
}

static bool clock_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_SECOND) return false;
    const uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000); /* since the start: a real clock gives the time of day */
    dgx_ui_text_printf(ui, i, "%02u:%02u:%02u", (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    return true; /* nobody else wants the second */
}
```

### Виджеты

Виджет — константа: какого он вида, какого размера, что показывает и что делает. Библиотека в него не пишет, поэтому
он лежит во флеше. Размеры заданы в точках; сколько пикселей в точке, говорится в `dgx_ui_init()`.

- Корень — контейнер, который ставит детей столбцом, в 4 точках от своих краёв и в 4 точках друг от друга.
- Рамка высотой 40 точек, шириной во весь столбец. В ней надпись со значением.
- В строке под ней две надписи: часы шириной 50 точек и подпись, которая занимает остаток строки.

```c
static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 40};
static const dgx_ui_widget_t co2_w   = {.kind = DGX_UI_LABEL, .text = &co2_box, .want_h = 34, .take = co2_take};
static const dgx_ui_widget_t row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10};
static const dgx_ui_widget_t clock_w = {.kind = DGX_UI_LABEL, .text = &clock_box, .want_w = 50, .take = clock_take};
static const dgx_ui_widget_t note_w  = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &note_box};
```

Три правила о размерах стоит запомнить:

- У контейнера своего размера нет. В столбце он шириной со столбец, а высоту ему задаёт `want_h`.
- Надпись без `want_h` высотой в свои строки текста; у большого числа выше он задан. Своей ширины у неё нет: в столбце она берёт ширину столбца, а в строке
  ей нужен `want_w`.
- Ребёнок с флагом `DGX_UI_REST` занимает то, что осталось в его строке или столбце после остальных. Подпись выше
  именно такая, поэтому строка подходит экрану любой ширины.

### Дерево, очередь и датчик

То, что меняется во время работы, хранит дерево: на каждый виджет узел в 28 байт, в массиве, который даёт программа.
Деревом и дисплеем владеет одна задача. Все остальные задачи только шлют сообщения.

```c
static dgx_ui_node_t   nodes[8];
static dgx_ui_t        ui;
static QueueHandle_t   queue;
static dgx_ui_ticker_t second;

/* a sensor in a task of its own: a value every 200 ms, sent and forgotten */
static void sensor_task(void *arg)
{
    for (int k = 0;; ++k) {
        const co2_t c = {.ppm = (int16_t)(400 + (k * 37) % 900)};
        msg_post_co2(queue, &c);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

### Запуск и цикл

```c
void app_main(void)
{
    dgx_screen_t *scr = display_init();
    if (!scr) return;

    /* the colours of the looks are indices of this table: 0 the paper, 1 the ink, 2 a fill, 3 a border */
    const uint32_t colors[] = {
        dgx_screen_rgb(scr, 16, 20, 28),
        dgx_screen_rgb(scr, 230, 230, 220),
        dgx_screen_rgb(scr, 32, 48, 72),
        dgx_screen_rgb(scr, 120, 160, 220),
    };

    /* the value in the large font, or in the small one when it does not go in */
    big[0]   = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()};
    big[1]   = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    small[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_big   = (dgx_ui_text_style_t){.variants = big, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
    st_left  = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    dgx_ui_textbox_init(&co2_box, co2_buf, sizeof co2_buf, 1, &st_big);
    dgx_ui_textbox_init(&clock_box, clock_buf, sizeof clock_buf, 1, &st_left);
    dgx_ui_textbox_init(&note_box, note_buf, sizeof note_buf, 1, &st_right);

    /* the tree: a point is 2 pixels; the first widget added is the root */
    dgx_ui_init(&ui, nodes, 8, 2);
    dgx_ui_add(&ui, -1, &root_w);
    const int frame = dgx_ui_add(&ui, 0, &frame_w);
    dgx_ui_add(&ui, frame, &co2_w);
    const int row = dgx_ui_add(&ui, 0, &row_w);
    dgx_ui_add(&ui, row, &clock_w);
    const int note = dgx_ui_add(&ui, row, &note_w);
    dgx_ui_text_set(&ui, note, "living room");

    /* the queue of the task that owns the display, and who sends into it */
    queue = xQueueCreate(8, sizeof(dgx_ui_msg_t));
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 4, NULL);
    dgx_ui_ticker_start(&second, queue, MSG_SECOND, 1000);

    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4, .background = 0};
    TickType_t     wait  = 0;
    for (;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            if (!dgx_ui_take(&ui, &msg)) {
                /* no widget claimed it: the program's own messages are handled here */
            }
            dgx_ui_msg_done(&msg); /* once, whatever handled it: the ticker may send the next */
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(&ui, now);
        dgx_ui_paint(&ui, &paint, now);
        /* asleep until a message comes, or until the moment a widget or a held key asks for: a tick at least */
        const uint32_t ms = dgx_ui_wake(&paint, &ui, now);
        wait              = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
```

Что делает цикл:

1. Спит на очереди. Его будит сообщение; он забирает все пришедшие сообщения и каждое предлагает виджетам.
2. `dgx_ui_msg_done()` вызывается один раз для каждого сообщения, кто бы его ни обработал. Тикер шлёт следующий тик,
   только когда предыдущий выброшен, поэтому тики в очереди не копятся.
3. `dgx_ui_paint()` раскладывает дерево, если нужно, и рисует то, что изменилось с прошлого прохода. Здесь это поле
   значения пять раз в секунду и поле часов раз в секунду. Рамка, подпись и фон рисуются один раз.
4. `dgx_ui_wake()` говорит, сколько времени делать нечего. Виджет с анимацией называет момент, когда его надо
   нарисовать снова, удерживаемая клавиша — момент своего повтора: цикл ждёт сообщения не дольше этого. Если никто
   не просит, `DGX_UI_WAKE_NONE`, он ждёт сообщения бесконечно. В этой программе времени никто не просит, и задача
   спит.

## 3. Соберите и запустите

Для платы:

```sh
idf.py set-target esp32
idf.py build flash monitor
```

На компьютере, в окне DGX, с целью `linux` ESP-IDF. В `CMakeLists.txt` и `sdkconfig.defaults` проекта нужны несколько
строк, описанных у DGX в разделе
[The linux target of ESP-IDF](https://github.com/jef-sure/esp32-component-dgx/blob/main/docs/host-window-en.md#the-linux-target-of-esp-idf-host-esp-idf);
в примере [thin_paint](../../examples/thin_paint/CMakeLists.txt) они есть. Собранная программа называется по
проекту, как он назван в `project()` его `CMakeLists.txt`: у примера это `thin_paint.elf`.

```sh
idf.py --preview set-target linux
idf.py build
./build/<project>.elf
```

Что вы должны увидеть — и на плате, и в окне: рамку со значением крупными буквами, сначала `CO2 400 ppm`, потом
каждые 200 мс другое число; под рамкой слева — время с запуска, `00:00:01`, `00:00:02`, а справа — подпись
`living room`. Меняются только числа: рамка, подпись и бумага рисуются один раз.

## 4. Измените её

Три маленьких изменения показывают, как части программы зависят друг от друга. Сделайте одно, соберите и запустите
снова:

- В `sensor_task()` замените `pdMS_TO_TICKS(200)` на `pdMS_TO_TICKS(1000)`. Значение теперь меняется раз в секунду.
  Больше ничто в программе не знает, как часто оно приходит: надпись узнаёт о нём, только когда пришло сообщение.
- В `root_w` замените `.gap = 4` на `.gap = 12`. Строка с часами сдвинется вниз от рамки на 8 пунктов, то есть на
  16 пикселей: столбец разложит детей заново, а координат в программе нет нигде.
- В `app_main()` замените `"living room"` на `"kitchen"`. Подпись задаётся один раз и больше не рисуется.

## Что дальше

- [Как это устроено](how-it-works.md): что такое дерево, отрисовка и облик и почему значение, нарисованное заново, не
  мигает.
- [Рецепты](cookbook.md): кнопки, ползунки, значки, длинные тексты, всплывающее окно, клавиши и касание.
- [Справочник API](api.md).
