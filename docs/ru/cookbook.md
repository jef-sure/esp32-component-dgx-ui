# Рецепты

*[In English](../en/cookbook.md)*

На этой странице собраны рецепты: короткие куски кода для всего, из чего складывается интерфейс, — от кнопки до
всплывающего окна и касания. Рецепт — это несколько законченных кусков на C, которые переносятся в программу как
есть. Имена, которые рецепт не определяет, например `settings_save()`, — это функции самой программы. Рамка, в
которой работают все рецепты, — дисплей, очередь и цикл задачи, владеющей деревом, — написана в
[Начале работы](getting-started.md).

- [Общее для всех рецептов](#общее-для-всех-рецептов)
- [Строка и столбец под любой экран](#строка-и-столбец-под-любой-экран)
- [Одно сообщение для нескольких виджетов](#одно-сообщение-для-нескольких-виджетов)
- [Часы или что угодно время от времени](#часы-или-что-угодно-время-от-времени)
- [Кнопка](#кнопка)
- [Ползунок и переключатель](#ползунок-и-переключатель)
- [Список](#список)
- [Выпадающий список](#выпадающий-список)
- [Значок который показывает состояние](#значок-который-показывает-состояние)
- [Картинка из флеша](#картинка-из-флеша)
- [Картинка под виджетами](#картинка-под-виджетами)
- [Текст в несколько строк](#текст-в-несколько-строк)
- [Шрифт по размеру текста](#шрифт-по-размеру-текста)
- [Свои цвета и ночной режим](#свои-цвета-и-ночной-режим)
- [Показать скрыть отключить](#показать-скрыть-отключить)
- [Всплывающее окно](#всплывающее-окно)
- [Страницы и «назад»](#страницы-и-назад)
- [Кнопки платы](#кнопки-платы)
- [Касание с DGX-DI](#касание-с-dgx-di)
- [Поле ввода и клавиатура](#поле-ввода-и-клавиатура)
- [Ввод в полосе над клавиатурой](#ввод-в-полосе-над-клавиатурой)
- [Виджеты которые появляются и исчезают](#виджеты-которые-появляются-и-исчезают)
- [Большие данные: сообщение по ссылке](#большие-данные-сообщение-по-ссылке)
- [Сообщение из прерывания](#сообщение-из-прерывания)
- [Свой облик](#свой-облик)
- [Картинка, которая живёт между проходами](#картинка-которая-живёт-между-проходами)
- [График-развёртка](#график-развёртка)
- [Шкала по кругу с виджетами внутри](#шкала-по-кругу-с-виджетами-внутри)
- [Дисплей на 18 или 24 бита](#дисплей-на-18-или-24-бита)
- [Цикл, который спит до следующего момента](#цикл-который-спит-до-следующего-момента)
- [Зеркало экрана](#зеркало-экрана)
- [Поворот экрана](#поворот-экрана)
- [Проверка интерфейса на компьютере](#проверка-интерфейса-на-компьютере)

## Общее для всех рецептов

Заголовки, цвета и два стиля одного шрифта. Цвета — это номера в таблице, которую программа даёт отрисовке; рецепты
называют их по именам.

```c
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum { PAPER, INK, FILL, BORDER, ACCENT, ACCENT_DOWN, SHADOW, DIM, COLORS };

static dgx_ui_font_variant_t small[1];
static dgx_ui_text_style_t   st_left, st_centre;

static void styles_init(void)
{
    small[0]  = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_left   = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_centre = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
}
```

## Строка и столбец под любой экран

Ребёнок с флагом `DGX_UI_REST` занимает то, что осталось в его строке или столбце после остальных. Ширину не надо
считать руками, и то же дерево подходит экрану другой ширины, другому `unit` и повёрнутому экрану.

```c
static char             name_buf[16], value_buf[16], title_buf[16];
static dgx_ui_textbox_t name_box, value_box, title_box;

/* a row: the name takes what the value leaves */
static const dgx_ui_widget_t fit_row_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10, .gap = 4};
static const dgx_ui_widget_t name_w    = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &name_box};
static const dgx_ui_widget_t value_w   = {.kind = DGX_UI_LABEL, .text = &value_box, .want_w = 40};

/* a column: the content takes what the title and the buttons leave, so the buttons stand at the bottom */
static const dgx_ui_widget_t page_w    = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t title_w   = {.kind = DGX_UI_LABEL, .text = &title_box};
static const dgx_ui_widget_t content_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_REST, .gap = 2};
static const dgx_ui_widget_t bottom_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 18, .gap = 6};
```

- Несколько детей с этим флагом делят остаток поровну.
- Если не осталось ничего, такой ребёнок не получает размера и не виден.
- В контейнере, который не столбец и не строка, флаг ничего не делает.

## Одно сообщение для нескольких виджетов

Датчик шлёт температуру и влажность одним сообщением. Его читают две надписи; ни одна его не забирает, поэтому
видят обе.

```c
enum { MSG_CLIMATE = 10 };
typedef struct {
    int16_t t10; /* tenths of a degree */
    uint8_t rh;  /* percent */
} climate_t;
DGX_UI_MESSAGE(climate, MSG_CLIMATE, climate_t)

static bool temp_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    climate_t c;
    if (msg_decode_climate(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "%d.%d C", c.t10 / 10, c.t10 % 10);
    return false;
}

static bool rh_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    climate_t c;
    if (msg_decode_climate(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "%d %%", c.rh);
    return false;
}

static dgx_ui_textbox_t      temp_box, rh_box;
static const dgx_ui_widget_t temp_w = {.kind = DGX_UI_LABEL, .text = &temp_box, .take = temp_take};
static const dgx_ui_widget_t rh_w   = {.kind = DGX_UI_LABEL, .text = &rh_box, .take = rh_take};

/* the sensor's task */
static void climate_send(QueueHandle_t queue, int t10, int rh)
{
    const climate_t c = {.t10 = (int16_t)t10, .rh = (uint8_t)rh};
    if (!msg_post_climate(queue, &c)) {
        /* the queue is full and the value is lost: the next one comes anyway */
    }
}
```

- Текст сравнивается уже отформатированным. Датчик может прислать 21,53, потом 21,48: надпись оба раза показывает
  «21.5», и ничего не рисуется.
- Тип больше 12 байт в `DGX_UI_MESSAGE` не скомпилируется. Данные побольше идут
  [по ссылке](#большие-данные-сообщение-по-ссылке).
- Декодер — это `dgx_ui_msg_copy()` с номером и размером. Сообщение с нужным номером, но другого размера — ошибка
  программы, и она попадает в журнал.

## Часы или что угодно время от времени

Тикер шлёт сообщение с заданным номером раз в столько-то миллисекунд. Кому оно нужно, тот читает его в своей
`take()`.

```c
enum { MSG_MINUTE = 11 };
static dgx_ui_ticker_t minute_ticker; /* static: zeroed, and alive as long as its ticks */

static bool date_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_MINUTE) return false;
    const uint32_t minutes = (uint32_t)(esp_timer_get_time() / 60000000);
    dgx_ui_text_printf(ui, i, "up %u h %02u min", (unsigned)(minutes / 60), (unsigned)(minutes % 60));
    return true;
}

static bool minutes_start(QueueHandle_t queue)
{
    return dgx_ui_ticker_start(&minute_ticker, queue, MSG_MINUTE, 60 * 1000);
}
```

- Следующий тик уходит, только когда цикл вызвал `dgx_ui_msg_done()` для предыдущего. Цикл, который забыл этот вызов,
  получит один тик и больше ни одного.
- `dgx_ui_ticker_stop()` останавливает тикер. Его структура должна жить, пока не выброшен последний тик; статическая
  живёт всегда.
- Тикером удобно делать медленное во времени: кадры значка, мигающее значение. Между тиками задача спит.

## Кнопка

![Экран настроек: ползунок, два переключателя, значок, две кнопки](../images/settings.png)

Элемент управления называет свой облик. Текст кнопки — текстовое поле, как у надписи. Функции `on()` сообщается
`DGX_UI_ACTIVATED`, когда пользователь нажал кнопку пальцем или клавишей «выбрать», пока на кнопке фокус.

```c
void settings_save(void);

static char             save_buf[8];
static dgx_ui_textbox_t save_box;

static void save_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) settings_save();
}

static const dgx_ui_widget_t save_w = {
    .kind = DGX_UI_BUTTON, .text = &save_box, .want_w = 60, .want_h = 18, .look = &dgx_ui_look_button, .on = save_on};

static int save_add(dgx_ui_t *ui, int parent)
{
    dgx_ui_textbox_init(&save_box, save_buf, sizeof save_buf, 1, &st_centre);
    const int b = dgx_ui_add(ui, parent, &save_w);
    dgx_ui_text_set(ui, b, "Save");
    return b;
}
```

- Кнопка с текстом высотой в одну строку текста, если не сказано иначе: задайте `want_h`, а в строке — флаг
  `DGX_UI_STRETCH`, с ним кнопка высотой со строку.
- `dgx_ui_set_state(ui, b, DGX_UI_PRESSED, true)` рисует кнопку нажатой и ничего не запускает: так кнопка платы
  может отражаться на экране.
- `on()` может удалять виджеты, и саму кнопку тоже: диалог может закрыть себя прямо там.

## Ползунок и переключатель

Значение элемента управления лежит в его узле, и `on()` получает этот узел: `node->state_value.value` — значение,
которое выставил пользователь. Функции `on()` сообщается `DGX_UI_CHANGED`, когда значение изменил пользователь: у
ползунка — на каждом шаге пальца, у переключателя — по нажатию.

```c
void backlight_set(int percent);
void wifi_enable(bool on);

static void backlight_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_CHANGED) backlight_set(node->state_value.value);
}

static void wifi_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_CHANGED) wifi_enable(node->state_value.value != 0);
}

/* a slider of 0..100 by 5 that starts at 70, and a switch that starts off */
static const dgx_ui_widget_t backlight_w = {
    .kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_h = 12, .state = {.value = 70}, .look = &dgx_ui_look_slider, .on = backlight_on};
static const dgx_ui_widget_t wifi_w = {
    .kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .want_h = 12, .look = &dgx_ui_look_switch, .on = wifi_on};

/* the value came from elsewhere: shown, and on() is not told */
static void backlight_show(dgx_ui_t *ui, int slider, int percent)
{
    dgx_ui_set_value(ui, slider, percent);
}
```

- `dgx_ui_set_value()` держит значение в пределах `min..max` и `on()` не вызывает: свои собственные изменения
  программа обратно не слышит.
- С флагом `DGX_UI_SWIPE` ползунок двигается на столько, на сколько ушёл палец от места касания, а не туда, где
  палец: получается широкая полоса, в которую легко попасть.
- Клавиши «больше» и «меньше» двигают ползунок на его шаг и ставят переключатель.

## Список

![Список из четырёх строк, выбрана вторая, и рядом выпадающий список](../images/list.png)

Список — это `DGX_UI_LIST` с обликом `dgx_ui_look_list`. Его строки — это строки его текста, и одна из них выбрана:
это значение списка.

```c
void mode_set(int mode);

static char             mode_buf[32];
static dgx_ui_textbox_t mode_box;

static void mode_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) mode_set(node->state_value.value); /* a row clicked, or activated by a key */
}

/* four rows of 14 points each: the rows share the height of the list */
static const dgx_ui_widget_t mode_w = {
    .kind = DGX_UI_LIST, .text = &mode_box, .min = 0, .max = 3, .want_h = 56, .look = &dgx_ui_look_list, .on = mode_on};

static int mode_add(dgx_ui_t *ui, int parent)
{
    dgx_ui_textbox_init(&mode_box, mode_buf, sizeof mode_buf, 1, &st_left);
    const int list = dgx_ui_add(ui, parent, &mode_w);
    dgx_ui_text_set(ui, list, "Auto\nHeat\nCool\nOff"); /* a row for each line */
    return list;
}
```

- **Строки** — это строки текста, разделённые `\n`. Первая строка — значение `min`, следующая — `min + 1`, и так до
  `max`; строка за последней строкой текста пуста.
- **Высота.** Если все строки помещаются в список по строке текста на каждую, они делят его высоту между собой:
  здесь 56 пунктов на четыре строки по 14. Если не помещаются, список показывает окно из них и прокручивается:
  за раз видно столько строк, сколько строк текста помещается в его высоту. Двенадцать месяцев в списке высотой
  56 пикселей, шрифтом в 13 пикселей на строку, — это четыре строки за раз, а тонкая полоска у правого края
  показывает, где окно находится среди всех строк. `step` виджета — наименьшая высота строки в пунктах, чтобы в строку
  попадал палец; 0 значит строку текста.
- **Палец на списке, все строки которого видны.** Когда палец опускается на строку, она выбирается, и `on()`
  сообщается `DGX_UI_CHANGED`. Если вести палец по списку, выбор идёт за ним: выбрана та строка, что под пальцем.
  Палец, отпущенный вскоре на той же строке, на которую опустился, — это нажатие: `DGX_UI_ACTIVATED`. Если он с
  этой строки ушёл, отпускание уже не нажатие. Верхняя и нижняя двенадцатая часть строки не принадлежит ни одной
  строке, и палец там ничего не выбирает.
- **Палец на списке, который прокручивается.** Опущенный палец ничего не выбирает. Если сдвинуть его вверх или
  вниз на полстроки, он двигает выбор — на строку за каждую высоту строки, вверх значит дальше по списку, — а окно
  идёт за выбором. Палец, отпущенный без сдвига, — это нажатие на строку под ним: строка выбирается, и `on()`
  сообщаются `DGX_UI_CHANGED` и `DGX_UI_ACTIVATED`.
- **Клавиши.** «Дальше» и «назад» сначала ведут выбор по строкам, с `DGX_UI_CHANGED` на каждой, и окно идёт за
  ним, а с последней строки фокус уходит к следующему виджету. Клавиша «активировать» сообщает
  `DGX_UI_ACTIVATED`.
- **Окно** всегда содержит выбранную строку после того, как выбор изменился: клавишей, пальцем или вызовом
  `dgx_ui_set_value()`. `dgx_ui_text_scroll(ui, list, rows)` двигает одно окно, выбор остаётся на месте, — для
  своей полосы прокрутки.
- **Выбранная строка** выделена полосой, другого цвета, пока список в фокусе. Когда выбор сдвигается, на дисплей
  пишутся две строки: та, с которой он ушёл, и та, на которую пришёл.
- **Свои цвета** — это `dgx_ui_list_look_t` в облике, который рисует `dgx_ui_draw_list`, как в рецепте
  [Свои цвета](#свои-цвета-и-ночной-режим).

## Выпадающий список

![На выпадающий список нажали: под ним его список, клавишей выбран второй вариант](../images/dropdown.png)

Выпадающий список — это `DGX_UI_DROPDOWN` с обликом `dgx_ui_look_dropdown`: кнопка, которая показывает выбранный
вариант. Его варианты — это строки его текста, как строки у списка, а значение — выбранный вариант. Сам список —
всплывающее окно; программа открывает его одним вызовом, когда на выпадающий список нажали.

Возьмём вентилятор со скоростями Low, Medium и High; сейчас стоит Low:

1. Выпадающий список показывает «Low», его значение 0. Пользователь нажимает на него. Его `on()` получает
   `DGX_UI_ACTIVATED` и вызывает `dgx_ui_dropdown_open()`: под ним открывается список из трёх строк высотой с сам
   выпадающий список, «Low» выделена полосой.
2. Пользователь нажимает «High». Значение выпадающего списка становится 2, его `on()` получает `DGX_UI_CHANGED`,
   список закрывается, и на кнопке написано «High».
3. Если бы пользователь нажал мимо списка, список закрылся бы, значение осталось бы 0, а `on()` ничего бы не
   получила. То же при нажатии на «Low» — вариант, который уже стоял.

```c
#include "dgx_ui_dropdown.h"

void fan_set(int speed);

static char              fan_buf[32];
static dgx_ui_textbox_t  fan_box;
static dgx_ui_dropdown_t fan_list; /* the popup of the list: one serves every dropdown of the program */

static void fan_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) dgx_ui_dropdown_open(ui, i, &fan_list, NULL, NULL); /* clicked: its list opens */
    if (what == DGX_UI_CHANGED) fan_set(node->state_value.value);                     /* the user chose an option */
}

static const dgx_ui_widget_t fan_w = {
    .kind = DGX_UI_DROPDOWN, .text = &fan_box, .min = 0, .max = 2, .want_h = 16, .look = &dgx_ui_look_dropdown, .on = fan_on};

static int fan_add(dgx_ui_t *ui, int parent)
{
    dgx_ui_textbox_init(&fan_box, fan_buf, sizeof fan_buf, 1, &st_left);
    const int fan = dgx_ui_add(ui, parent, &fan_w);
    dgx_ui_text_set(ui, fan, "Low\nMedium\nHigh"); /* an option for each line */
    return fan;
}
```

- **Где встаёт список.** Под выпадающим списком, той же ширины; над ним, если внизу на экране нет места; иначе
  сдвигается вверх так, чтобы поместиться на экране, и ложится поверх самого выпадающего списка.
- **Строки** высотой с сам выпадающий список и не ниже строки текста: если его сделали под палец, пальцем
  попадёшь и в строку. Если все варианты не помещаются ни под выпадающим списком, ни над ним, список
  прокручивается: он встаёт там, где места больше, в столько строк, сколько туда помещается, его окно стоит на
  выбранном варианте, а сам выпадающий список он никогда не закрывает.
- **Клавиши.** Клавиша «активировать» на выпадающем списке сообщает `DGX_UI_ACTIVATED`, как и нажатие. В списке
  «дальше» и «назад» ведут выбор, «активировать» выбирает. Список модальный, поэтому клавиши остаются в нём;
  «возврат» достаётся программе, и она закрывает список вызовом `dgx_ui_popup_close(ui)` — значение остаётся
  прежним.
- **Палец, который ведут по списку,** ведёт за собой полосу, а отпущенный ничего не выбирает: список остаётся
  открытым, выбирает нажатие.
- **`dgx_ui_dropdown_t`** — это всплывающее окно списка, 264 байта ОЗУ программы на 32-битном чипе, обнулённые
  перед первым открытием, как любая `static`-переменная. Открыт бывает только один список, поэтому одной структуры
  хватает на все выпадающие списки.
- **Облики** окна — два последних аргумента, рамка и список; `NULL` значит облик библиотеки.
- **Дерево знает свой экран до того, как откроется первый список**: список ставится на экран, под который
  разложено дерево. Экран сообщает отрисовка при первом проходе или сама программа вызовом `dgx_ui_screen()`; до
  этого `dgx_ui_dropdown_open()` возвращает `false` и ничего не открывает.
- **Пока список открыт,** программа не меняет варианты и не удаляет выпадающий список: сначала она закрывает
  список.
- Если программе нужен другой список, например выбор на весь экран, она по `DGX_UI_ACTIVATED` открывает своё
  всплывающее окно.

## Значок который показывает состояние

Значок — виджет постоянного размера, картинку которого выбирает значение. При значении вне `min..max` картинки нет,
а место за виджетом остаётся, поэтому вокруг ничего не сдвигается, когда значок появляется и исчезает.

```c
/* 8x8, 1 bit: rows from a new byte, the leftmost pixel in the high bit */
static const uint8_t      bars1[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80};
static const uint8_t      bars2[8] = {0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0xA0, 0xA0};
static const uint8_t      bars3[8] = {0x00, 0x00, 0x08, 0x08, 0x28, 0x28, 0xA8, 0xA8};
static const uint8_t      bars4[8] = {0x02, 0x02, 0x0A, 0x0A, 0x2A, 0x2A, 0xAA, 0xAA};
static const dgx_image_t  signal_im[4] = {
    {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = bars1},
    {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = bars2},
    {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = bars3},
    {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = bars4},
};
static const dgx_ui_picture_t signal_pic[4] = {
    {.image = &signal_im[0]}, {.image = &signal_im[1]}, {.image = &signal_im[2]}, {.image = &signal_im[3]}};

enum { MSG_SIGNAL = 12 };
typedef struct {
    int8_t bars; /* 1..4; 0: no network */
} signal_t;
DGX_UI_MESSAGE(signal, MSG_SIGNAL, signal_t)

static bool signal_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    signal_t s;
    if (msg_decode_signal(msg, &s) == ESP_OK) dgx_ui_set_value(ui, i, s.bars);
    return false;
}

/* the pictures of the values 1..4; 0 is outside and shows none */
static const dgx_ui_widget_t signal_w = {
    .kind = DGX_UI_ICON, .picture = signal_pic, .min = 1, .max = 4, .look = &dgx_ui_look_picture, .take = signal_take};
```

Размер виджета — размер первой картинки. Картинка рисуется непрозрачной: пишутся и её чернила, и её бумага, поэтому
значок, нарисованный поверх себя, не мигает.

## Картинка из флеша

Картинка в 1 бит — это `dgx_image_t` из DGX с обликом `dgx_ui_look_picture`, как значок выше; её чернила — цвет из
таблицы. Цветная картинка — строки RGB565 с обликом `dgx_ui_look_rgb565`:

```c
extern const uint16_t logo_pixels[64 * 32]; /* rows of RGB565, high byte first */

static const dgx_ui_picture_t logo_pic = {.rgb565 = logo_pixels, .width = 64, .height = 32};
static const dgx_ui_widget_t  logo_w   = {.kind = DGX_UI_PICTURE, .picture = &logo_pic, .look = &dgx_ui_look_rgb565};
```

- Облику RGB565 нужны `CONFIG_DGX_ENABLE_VSCREEN=y` и 16-битный экран. На другом экране вместо картинки бумага.
- Для таких картинок отрисовка при первой надобности создаёт один небольшой объект экрана.
  `dgx_ui_paint_free(&paint)` возвращает его, когда программа перестаёт рисовать.

Картинка в несколько цветов таблицы — это `dgx_image_t` в 4 или 8 бит с обликом `dgx_ui_look_indexed`. Её значения
— не цвета, а номера, и её карта говорит, какой цвет таблицы отрисовки стоит за каждым номером:

```c
/* a battery of 12 by 6, 4 bits a pixel: a hex digit is a pixel, 0 a hole that shows the paper */
static const uint8_t battery_px[6 * 6] = {
    0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x12, 0x22, 0x23, 0x33, 0x31, 0x10,
    0x12, 0x22, 0x23, 0x33, 0x31, 0x10,
    0x12, 0x22, 0x23, 0x33, 0x31, 0x10,
    0x12, 0x22, 0x23, 0x33, 0x31, 0x10,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
};
static const dgx_image_t battery_im = {.width = 12, .height = 6, .format = DGX_IMAGE_4BPP, .data = battery_px};
/* the colours of the values 1, 2, 3: the case, the charge, the rest; the same data with another map when it is low */
static const uint8_t          battery_ok[15]  = {INK, ACCENT, FILL};
static const uint8_t          battery_low[15] = {INK, ACCENT_DOWN, FILL};
static const dgx_ui_picture_t battery_pics[2] = {{.image = &battery_im, .map = battery_ok}, {.image = &battery_im, .map = battery_low}};
static const dgx_ui_widget_t  battery_w = {
    .kind = DGX_UI_ICON, .picture = battery_pics, .min = 0, .max = 1, .want_w = 14, .want_h = 8, .look = &dgx_ui_look_indexed};

/* the painter's buffer: such a picture is drawn in it a strip of its rows at a time, a byte a pixel */
static uint8_t paint_buffer[256];

static void paint_buffer_give(dgx_ui_paint_t *paint)
{
    paint->buffer = paint_buffer, paint->buffer_size = sizeof paint_buffer;
}
```

- **Значения** идут через карту картинки, `map`, в номера цветов таблицы отрисовки: значение 1 — это `map[0]`,
  значение 2 — `map[1]`. Без карты номером служит само значение. Пиксель 0 — дыра: на его месте бумага.
- **Те же данные в других цветах** — это другая карта: батарея выше — одна картинка и две карты, значок с двумя
  состояниями.
- **Буфер.** Такую картинку отрисовка рисует в памяти, которую даёт программа, — `buffer` и `buffer_size` в
  структуре отрисовки, — полосой в несколько строк картинки за раз, и каждую полосу переносит через таблицу: каждый
  пиксель виджета пишется один раз. Хватит одной строки самой широкой картинки, по байту на пиксель; если памяти
  больше, полосы выше и их меньше. Без буфера или с буфером меньше строки на месте виджета бумага, и об этом один
  раз пишется в журнал.
- Нужны `CONFIG_DGX_ENABLE_VSCREEN=y` и 16-битный экран. На дисплее в 18 или 24 бита на месте такой картинки
  бумага.
- Для таких картинок отрисовка с первой из них держит небольшой объект экрана и таблицу на 512 байт;
  `dgx_ui_paint_free(&paint)` их возвращает.
- Картинку в 1 бит этот облик тоже рисует, поэтому значок, у которого картинки разной глубины, называет его.
  Программа, у которой картинки только в 1 бит, называет `dgx_ui_look_picture` и ничего из этого не несёт.

## Картинка под виджетами

![Надписи, рамка и кнопка поверх картинки с холмами и солнцем](../images/background.png)

Фон дерева — это цвет или картинка, которую рисует программа. Программа даёт отрисовке функцию, которая кладёт на
экран прямоугольник её картинки, и каждый виджет, у которого бумага — «то, что лежит подо мной», показывает на этом
месте картинку. Возьмём карту погоды со столбцом надписей поверх неё; ветер сейчас «5 m/s»:

1. Отрисовка рисует надпись. Её облик кладёт кусок карты под свой прямоугольник функцией программы и пишет буквы поверх,
   без поля вокруг них: между буквами видна карта.
2. Ветер становится «6 m/s». Если названы полосы, кусок карты и новые буквы складываются в буфере и уходят на
   дисплей один раз. Надпись не мигает.
3. Программа меняет карту на карту с дождём и сообщает отрисовке, где картинка изменилась. Отрисовка сразу кладёт
   там новую карту — везде, где она видна, — и заново рисует поверх неё надписи. Под рамкой с температурой не
   пишется ничего: рамка заливает себя, и карта сквозь неё не видна.

```c
#include "dgx_vscreen.h"

extern const uint16_t map_pixels[320 * 240]; /* the map, as large as the screen: rows of RGB565, high byte first */
extern const uint16_t rain_pixels[320 * 240];

static dgx_screen_t *map_screen;               /* a virtual screen over the map's pixels: made once */
static uint8_t       map_buffer[320 * 2 * 8];  /* eight rows of 16 bits: what lies on the map goes out through it */

/* the program's function: the piece x, y, w, h of the map put where the painter draws */
static void map_draw(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, ctx, x, y, w, h, 0);
}

/* at the start, before the first pass */
static bool map_under(dgx_ui_paint_t *paint)
{
    map_screen = dgx_vscreen_from_array((void *)map_pixels, 320, 240, 16, DgxScreenRGB);
    if (!map_screen) return false;
    dgx_ui_background_set(paint, map_draw, map_screen);
    paint->buffer = map_buffer, paint->buffer_size = sizeof map_buffer, paint->strips = &dgx_ui_strips;
    return true;
}

/* another map: the same screen pointed at other pixels, and the painter told where the picture changed */
static void map_rain(dgx_ui_t *ui, dgx_ui_paint_t *paint)
{
    dgx_vscreen_array_set(map_screen, (void *)rain_pixels, 320, 240);
    dgx_ui_background_changed(ui, paint, 0, 0, 320, 240);
}
```

- **Функция** кладёт прямоугольник `x, y, w, h` экрана дерева на `paint->scr` в точку `x - paint->ox`,
  `y - paint->oy`. Отсечение по прямоугольнику отрисовка уже поставила, поэтому функция, которая рисует больше, чем
  просили, ничего не портит. Экран — это дисплей или полоса буфера той же глубины, что у дисплея: на дисплее в 18
  или 24 бита картинка должна быть этой глубины.
- **`dgx_ui_background_set()`** задаёт функцию и то, с чем её вызывать. `NULL` убирает картинку; после этого
  `dgx_ui_redraw(ui, 0)` рисует дерево снова на цвете. Функция, вписанная в структуру отрисовки руками, не
  вызывается: именно этот вызов приносит в программу укладку картинки.
- **Что показывает картинку**: надпись и текст в несколько строк — между буквами, кнопка — вокруг своей грани,
  ползунок — вокруг дорожки, картинка в 1 бит и картинка в 4 или 8 бит — в своих дырах, контейнер — везде. **Что не
  показывает**: рамка и всё, что в ней, потому что она заливает себя; облик, в параметрах которого названа своя
  бумага.
- **Называйте полосы**, если картинка лежит на дисплее за шиной, и на 16-битном тоже. Нарисованная прямо, надпись
  поверх картинки пишется дважды — карта, потом буквы — и мигает; а мягкие края букв, которые прочитать дисплей не
  могут, смешиваются с цветом `background`. Через полосы она уходит один раз, и края смешиваются с картой. В буфер
  должна помещаться хотя бы строка самого широкого такого виджета: его ширина на 2 байта при 16 битах.
- **`background`** остаётся цветом, который сообщается облику там, где картинку положить нельзя. Дайте ему основной
  цвет картинки.
- **`dgx_ui_background_changed()`** рисует сразу и ничего не хранит: картинку в прямоугольнике там, где она видна,
  потом виджеты на ней. Игра с диалогом поверх кадра вызывает её для каждого изменившегося куска кадра — там, где
  раньше отправляла кусок на дисплей; внутри одного пакета DGX куски уходят вместе.
- **Чего это стоит**: около 0,5 КБ в dgx-ui для программы, которая вызывает `dgx_ui_background_set()`, и то, что
  тянет из DGX её собственная функция. Программа с цветом вместо картинки ничего из этого не несёт.

## Текст в несколько строк

![Важный текст, прокрученный на строку, и текст, обрезанный многоточием](../images/lines.png)

Облик `dgx_ui_look_lines` раскладывает текст по строкам своего поля, с переносом по словам. Поле говорит, сколько
строк оно показывает; стиль — что делать со слишком длинным текстом.

```c
static char                news_buf[200], hint_buf[120];
static dgx_ui_textbox_t    news_box, hint_box;
static dgx_ui_text_style_t st_important;

static const dgx_ui_widget_t news_w = {.kind = DGX_UI_LABEL, .text = &news_box, .look = &dgx_ui_look_lines};
static const dgx_ui_widget_t hint_w = {.kind = DGX_UI_LABEL, .text = &hint_box, .look = &dgx_ui_look_lines};

static void texts_add(dgx_ui_t *ui, int parent)
{
    /* an important text too long scrolls a line up every 2 s; one that is not is cut with an ellipsis */
    st_important = (dgx_ui_text_style_t){.variants = small, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 2000};
    dgx_ui_textbox_init(&news_box, news_buf, sizeof news_buf, 3, &st_important);
    dgx_ui_textbox_init(&hint_box, hint_buf, sizeof hint_buf, 2, &st_left);
    const int news = dgx_ui_add(ui, parent, &news_w);
    const int hint = dgx_ui_add(ui, parent, &hint_w);
    dgx_ui_text_set(ui, news, "The filter has worked for 2000 hours. Replace it and hold the button for three seconds.");
    dgx_ui_text_set(ui, hint, "Swipe from the left edge to open the menu.");
    dgx_ui_text_appear(ui, hint, 30); /* its letters come one by one, 30 ms each */
}
```

- Пока текст прокручивается или появляются его буквы, `dgx_ui_paint()` возвращает `true`, и цикл возвращается снова
  и снова. Когда текст дошёл до конца и все буквы появились, он времени больше не просит.
- Новый текст начинается с первой строки и появляется заново, если это было задано.
- `dgx_ui_text_scroll(ui, i, 1)` сдвигает неважный текст на строку вверх вручную, `-1` — на строку вниз.
- Слово шире строки режется по буквам; `\n` заканчивает строку.
- Программа, которая называет этот облик, несёт примерно на 2,5 КБ кода больше; программа с однострочными
  надписями — нет.

## Шрифт по размеру текста

Стиль перечисляет шрифты, которыми можно нарисовать текст, начиная с лучшего. Отрисовка берёт первый, в который
текст помещается целиком, а иначе тот, что показывает больше букв. Короткое название города встаёт крупным шрифтом,
длинное — мелким.

```c
static dgx_ui_font_variant_t city_fonts[2];
static dgx_ui_text_style_t   st_city;

static void city_style_init(void)
{
    city_fonts[0] = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()};
    city_fonts[1] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_city = (dgx_ui_text_style_t){.variants = city_fonts, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
}
```

- Высота надписи — строка её первого шрифта, поэтому размер надписи не зависит от того, какой шрифт выбран.
- Меньший шрифт стоит на базовой линии первого. С флагом `DGX_UI_TEXT_MIDDLE` он стоит посередине высоты.
- Шрифты — из DGX, матричные и точечные. Такой шрифт не масштабируется. Рукописный шрифт пишется на любой
  высоте: следующий рецепт.

## Рукописный текст

В DGX есть рукописные шрифты: их буквы — штрихи, которые перо пишет на любой высоте. Вариант такого шрифта называет
рукописный текст библиотеки, `&dgx_ui_handwriting`, и однострочный облик текста его пишет. Программа, которая его не
называет, ничего из него не несёт.

Пример — приветствие шрифтом `font0ss` из DGX: высотой 32 пикселя, когда помещается, суженное, когда почти
помещается, высотой 20 пикселей, когда оно длинное, и матричным шрифтом, когда оно ещё длиннее.

```c
#include "dgx_hw_font.h"
#include "fonts/font0ss.h"

static dgx_ui_font_variant_t greeting_fonts[3];
static dgx_ui_text_style_t   greeting_style;
static char                  greeting_text[64];
static dgx_ui_textbox_t      greeting_box;

static void greeting_init(void)
{
    /* DGX's handwritten face at two heights, then a bitmap font for a text too long even so */
    greeting_fonts[0] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 32, .narrow = 70, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
    greeting_fonts[1] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 20, .narrow = 70, .joined = true, .pen = 1, .hand = &dgx_ui_handwriting};
    greeting_fonts[2] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    greeting_style    = (dgx_ui_text_style_t){.variants = greeting_fonts, .count = 3, .align = DGX_FONT_ALIGN_LEFT};
    dgx_ui_textbox_init(&greeting_box, greeting_text, sizeof greeting_text, 1, &greeting_style);
}

static const dgx_ui_widget_t greeting_w = {.kind = DGX_UI_LABEL, .text = &greeting_box};

/* a new text, written by the pen: all of it in as long as its letters at 60 ms each */
static void greeting_say(dgx_ui_t *ui, int greeting, const char *text)
{
    dgx_ui_text_set(ui, greeting, text);
    dgx_ui_text_appear(ui, greeting, 60);
}
```

Что происходит в надписи шириной 200 пикселей:

1. `"Hello"` при высоте 32 занимает 50 пикселей: первый вариант, как есть.
2. `"Good afternoon to you"` — 213 пикселей: не помещается. `narrow = 70` разрешает сузить буквы по ширине до 70%.
   Сужение идёт целыми процентами, берётся самое широкое, что помещается. Высота остаётся, перо своей ширины не
   меняет.
3. `"Good afternoon, dear guests of the house"` — 390 пикселей, не помещается и при 70%: второй вариант, высотой 20
   пикселей. Он стоит на базовой линии первого, поэтому текст не прыгает рядом с соседями.
4. `"Good afternoon, dear guests of the house on the hill"` не помещается и при высоте 20: матричный шрифт, с
   многоточием, на той же базовой линии. Без этого последнего варианта был бы взят рукописный, и текст обрезался
   бы по прямоугольнику надписи в конце, без многоточия.
5. `greeting_say(ui, greeting, "Hello")`: текст пишется штрих за штрихом, весь — за то время, что заняли бы его пять
   букв по 60 мс, то есть за 300 мс. Пока он пишется, облик просит проход каждые `DGX_UI_HAND_FRAME_MS`, 20 мс, а
   между проходами цикл спит (`dgx_ui_wake()`). Проход пишет только новые штрихи. Проход, который пришёл раньше, из-за
   другого виджета, дописывает до своего времени. Текст, заданный, пока пишется другой, начинается заново.

Что говорит вариант:

- **`height`** — высота строки в пикселях; она же высота надписи, если вариант в стиле первый.
- **`narrow`** — до скольких процентов можно сузить буквы по ширине; 0 — нельзя.
- **`joined`** — буквы соединены, как в письме от руки.
- **`pen`** — ширина пера в пикселях; 0 — волосная линия.
- **Только однострочный облик** пишет рукописный текст, то есть надпись. В кнопке, поле ввода, списке и тексте в
  несколько строк рукописный вариант пропускается.
- **Чего это стоит.** Если вариант его назвал — около 31 КБ: шрифт `font0ss` — 17,8 КБ, перо DGX — 10 КБ, dgx-ui —
  1,6 КБ. Если не назвал — около 100 байт. Вариант занимает 16 байт, рукописный он или нет.
- **Время прохода.** Перо между проходами ничего не хранит: каждый проход проходит уже написанные штрихи, не
  рисуя их, и пишет новые. На ESP32 с частотой 240 МГц и дисплеем 320 на 240 на SPI проход занимает от 5 до 9 мс,
  пока пишется строка из 12–30 букв высотой 24 пикселя; из них меньше 3 мс уходит на пройденные штрихи. Нарисовать
  всю надпись заново стоило бы от 23 до 41 мс.

У мягкого пера края сходят на нет, и ему нужно место, где хранится количество чернил: холст виджета. Вариант
называет `&dgx_ui_handwriting_canvas`, а у надписи стоит флаг `DGX_UI_CANVAS`:

```c
static dgx_ui_font_variant_t signature_fonts[1];
static dgx_ui_text_style_t   signature_style;
static char                  signature_text[24];
static dgx_ui_textbox_t      signature_box;

static void signature_init(void)
{
    signature_fonts[0] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 32, .pen = 3 | DGX_PEN_SOFT, .hand = &dgx_ui_handwriting_canvas};
    signature_style    = (dgx_ui_text_style_t){.variants = signature_fonts, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    dgx_ui_textbox_init(&signature_box, signature_text, sizeof signature_text, 1, &signature_style);
}

/* the canvas of the label holds the amounts of ink: a byte of RAM for each of its pixels */
static const dgx_ui_widget_t signature_w = {.kind = DGX_UI_LABEL, .flags = DGX_UI_CANVAS, .text = &signature_box, .want_w = 120};
```

Холсту нужны `CONFIG_DGX_ENABLE_VSCREEN=y` и дисплей в 16 бит. Если писать напрямую, через `&dgx_ui_handwriting`,
мягкое перо становится жёстким той же ширины, и об этом один раз пишется в журнал. Программа, которая называет
только `&dgx_ui_handwriting`, холста не несёт.

## Свои цвета и ночной режим

У обликов библиотеки есть параметры: какой цвет таблицы у лицевой стороны, у текста, у тени. Облик в других
цветах — это рисование библиотеки со своими параметрами. Он константа и лежит во флеше:

```c
static const dgx_ui_button_look_t blue_button = {
    .face = ACCENT, .pressed = ACCENT_DOWN, .focused = ACCENT_DOWN, .disabled = FILL,
    .text = INK, .text_disabled = DIM, .shadow = SHADOW, .depth = 1};
/* the library's drawing of a button, in these colours */
static const dgx_ui_look_t look_blue_button = {.draw = dgx_ui_draw_button, .params = &blue_button, .fill = -1};

static const dgx_ui_widget_t ok_w = {.kind = DGX_UI_BUTTON, .want_w = 60, .want_h = 18, .look = &look_blue_button};
```

Ночной режим — другая таблица тех же цветов. Её дают отрисовке, и корень рисуется целиком:

```c
static void colors_set(dgx_ui_t *ui, dgx_ui_paint_t *paint, const uint32_t *table)
{
    paint->colors = table;
    dgx_ui_redraw(ui, 0); /* the root whole, and with it everything under it */
}
```

Таблица заполняется под экран вызовами `dgx_screen_rgb(scr, r, g, b)`, как в [Начале работы](getting-started.md).

## Показать скрыть отключить

```c
static void advanced_show(dgx_ui_t *ui, int box, int button, bool expert)
{
    dgx_ui_show(ui, box, expert);                           /* hidden, it takes no place: the rest moves up */
    dgx_ui_set_state(ui, button, DGX_UI_DISABLED, !expert); /* stays where it is, drawn dim, takes no finger */
}
```

- Скрытый виджет по-прежнему получает сообщения, поэтому при показе он свежий.
- Скрытый или отключённый виджет теряет фокус, и нажатие пальца на нём заканчивается.
- Чтобы что-то появлялось и исчезало, не двигая соседей, не скрывайте его: возьмите
  [значок](#значок-который-показывает-состояние) со значением без картинки или надпись с пустым текстом.

## Всплывающее окно

![Всплывающее окно поверх настроек](../images/popup.png)

Всплывающее окно — отдельное дерево: свои узлы, свой корень, те же виджеты и облики. Оно открывается поверх дерева в
прямоугольнике экрана.

```c
static char             ask_buf[32], yes_buf[8], no_buf[8];
static dgx_ui_textbox_t ask_box, yes_box, no_box;
static dgx_ui_node_t    ask_nodes[6];
static dgx_ui_t         ask_ui;
static dgx_ui_t        *ask_over; /* the tree it is open over */

static void yes_on(dgx_ui_t *popup, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return;
    settings_save();
    dgx_ui_popup_close(ask_over);
}

static void no_on(dgx_ui_t *popup, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) dgx_ui_popup_close(ask_over);
}

static const dgx_ui_widget_t ask_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 5, .gap = 5};
static const dgx_ui_widget_t ask_w      = {.kind = DGX_UI_LABEL, .text = &ask_box, .want_h = 12};
static const dgx_ui_widget_t ask_row_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 18, .gap = 6};
/* the two buttons share the row: as high as it (STRETCH), each a half of what it has (REST) */
static const dgx_ui_widget_t yes_w = {
    .kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &yes_box, .look = &dgx_ui_look_button, .on = yes_on};
static const dgx_ui_widget_t no_w = {
    .kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &no_box, .look = &dgx_ui_look_button, .on = no_on};

static void ask_open(dgx_ui_t *ui)
{
    dgx_ui_textbox_init(&ask_box, ask_buf, sizeof ask_buf, 1, &st_centre);
    dgx_ui_textbox_init(&yes_box, yes_buf, sizeof yes_buf, 1, &st_centre);
    dgx_ui_textbox_init(&no_box, no_buf, sizeof no_buf, 1, &st_centre);
    dgx_ui_init(&ask_ui, ask_nodes, 6, 2);
    const int root = dgx_ui_add(&ask_ui, -1, &ask_root_w);
    dgx_ui_text_set(&ask_ui, dgx_ui_add(&ask_ui, root, &ask_w), "Save the settings?");
    const int row = dgx_ui_add(&ask_ui, root, &ask_row_w);
    dgx_ui_text_set(&ask_ui, dgx_ui_add(&ask_ui, row, &yes_w), "Yes");
    dgx_ui_text_set(&ask_ui, dgx_ui_add(&ask_ui, row, &no_w), "No");
    ask_over = ui;
    dgx_ui_popup_open(ui, &ask_ui, 50, 70, 220, 92, DGX_UI_POPUP_MODAL); /* pixels of the screen */
}
```

- Программа по-прежнему вызывает `dgx_ui_take()`, `dgx_ui_pointer()`, `dgx_ui_key()` и `dgx_ui_paint()` для дерева
  под окном; до окна они доходят сами.
- Модальное окно забирает каждый палец и каждую клавишу. Клавишу «отмена» не берёт никто, поэтому программа может
  по ней закрыть окно.
- С флагом `DGX_UI_POPUP_CLOSE_OUTSIDE` окно закрывает палец, опустившийся снаружи: так ведёт себя список выбора.
- Когда окно закрывается, отрисовка заново рисует дерево там, где оно лежало.
- Прямоугольник задан в пикселях экрана, как он стоит сейчас. При повороте экрана закройте окно или откройте заново
  в новых пикселях.

Окно — это дерево, поэтому над ним в свою очередь можно открыть окно: вопрос поверх диалога или список выбора,
который выступает за его край.

```c
static dgx_ui_node_t sure_nodes[6];
static dgx_ui_t      sure_ui; /* built as ask_ui above: a frame, a label, two buttons */

/* a second popup, over the first: it is opened over the popup's own tree */
static bool sure_open(void)
{
    return dgx_ui_popup_open(&ask_ui, &sure_ui, 80, 110, 200, 80, DGX_UI_POPUP_MODAL);
}
```

- Одно над другим могут стоять до `DGX_UI_POPUP_DEPTH`, то есть 8, окон. Для девятого `dgx_ui_popup_open()`
  возвращает `false`, как и для дерева, которое уже открыто как окно.
- Палец и клавиши сначала идут в верхнее окно. Палец на той части верхнего окна, что выступает за нижнее, достаётся
  верхнему.
- `dgx_ui_popup_close(ui)` закрывает окно этого дерева и все окна над ним.

## Страницы и «назад»

Программе с несколькими экранами ничего нового не нужно: страница — это всплывающее окно размером с экран. Возьмём
часы со страницей настроек и, из неё, страницей «о программе»:

1. Часы — первое дерево. Пользователь просит настройки: программа открывает дерево настроек поверх него, на весь
   экран, модальным окном. Часы больше не рисуются, а палец и клавиши идут в настройки.
2. Пока показаны настройки, приходит значение датчика. Сообщение предлагается каждому дереву, поэтому надпись
   часов берёт его, под страницей.
3. Из настроек пользователь открывает «о программе»: вторая страница поверх первой.
4. «Назад» закрывает верхнюю страницу: снова видны настройки. Ещё раз «назад»: часы, уже со значением, которое
   пришло на шаге 2.

```c
/* a page is a tree of its own, built at the start as any tree, with its own nodes */
static dgx_ui_node_t page_settings_nodes[8], page_about_nodes[4];
static dgx_ui_t      page_settings, page_about;

/* a page opened over the one that is on top now: the whole screen, modal */
static bool page_open(dgx_ui_t *home, dgx_ui_t *page, int screen_w, int screen_h)
{
    dgx_ui_t *top = home;
    while (top->popup) top = top->popup;
    return dgx_ui_popup_open(top, page, 0, 0, screen_w, screen_h, DGX_UI_POPUP_MODAL);
}

/* "back": the page on top closed; false when the first tree is on top, and there is nowhere to go back to */
static bool page_back(dgx_ui_t *home)
{
    dgx_ui_t *under = home;
    if (!under->popup) return false;
    while (under->popup->popup) under = under->popup;
    dgx_ui_popup_close(under);
    return true;
}
```

- **Стек** страниц — это окна одно над другим, до `DGX_UI_POPUP_DEPTH`, то есть 8, над первым деревом.
- **«Назад»** — дело программы: клавишу `DGX_UI_KEY_BACK` не берёт ни один виджет, а кнопка «назад» на странице
  вызывает ту же `page_back()`.
- **Страницы под верхней** получают сообщения через `dgx_ui_take()` и рисуются, когда снова становятся видны; их
  холсты сохраняются. Закрытая страница ничего не стоит, пока её не откроют снова, а её виджеты сохраняют свои
  значения.
- **Корень страницы** закрывает экран бумагой, если это контейнер, или своей заливкой, если это рамка.
- Страницам одного уровня, которые сменяют друг друга, — вкладкам — окно не нужно: это дети одного контейнера,
  из которых скрыты все, кроме одного (`dgx_ui_show()`).

## Кнопки платы

Тот, кто читает кнопки, шлёт, какие из них удерживаются сейчас, битами геймпада; цикл передаёт их дереву.

```c
enum { MSG_BUTTONS = 13 };
typedef struct {
    uint32_t held; /* DGX_UI_PAD_* */
} buttons_t;
DGX_UI_MESSAGE(buttons, MSG_BUTTONS, buttons_t)

void menu_back(void);

/* in the loop, for a message no widget claimed */
static void buttons_message(dgx_ui_t *ui, const dgx_ui_msg_t *msg, uint32_t now_ms)
{
    buttons_t b;
    if (msg_decode_buttons(msg, &b) != ESP_OK) return;
    const bool taken = dgx_ui_buttons(ui, b.held, now_ms);
    if (!taken && (b.held & (DGX_UI_PAD_BACK | DGX_UI_PAD_B))) menu_back();
}
```

- Вверх и вниз двигают фокус, влево и вправо меняют ползунок или переключатель в фокусе, A выбирает, B и BACK
  достаются программе.
- Удерживаемые вверх, вниз, влево и вправо повторяются через 500 мс, каждые 100 мс. Повтор делает
  `dgx_ui_time()`, которую цикл вызывает перед `dgx_ui_paint()`. Пока кнопка удерживается, она возвращает `true`, и
  цикл не спит.
- Одна клавиша диалога — это `dgx_ui_key(ui, DGX_UI_KEY_NEXT)` и ей подобные.

**Какую клавишу даёт кнопка**, определяет таблица. По умолчанию:

| Кнопка | Её клавиша по умолчанию | Что делает клавиша |
| ------ | ----------------------- | ------------------ |
| `DGX_UI_PAD_UP` | `DGX_UI_KEY_PREV` | фокус переходит к предыдущему виджету |
| `DGX_UI_PAD_DOWN` | `DGX_UI_KEY_NEXT` | фокус переходит к следующему виджету |
| `DGX_UI_PAD_LEFT` | `DGX_UI_KEY_LESS` | ползунок в фокусе на шаг вниз, переключатель выключается |
| `DGX_UI_PAD_RIGHT` | `DGX_UI_KEY_MORE` | ползунок в фокусе на шаг вверх, переключатель включается |
| `DGX_UI_PAD_A` | `DGX_UI_KEY_ACTIVATE` | срабатывает кнопка, строка или выпадающий список в фокусе, переключатель меняет положение, полю ввода сообщается `DGX_UI_EDIT` |
| `DGX_UI_PAD_BACK`, `DGX_UI_PAD_B` | `DGX_UI_KEY_BACK` | виджеты её не берут: достаётся программе |
| `DGX_UI_PAD_START` | никакая | кнопка достаётся программе |

Таблица по умолчанию подходит столбцу настроек: вверх и вниз переходят от одной к другой, влево и вправо меняют ту,
что в фокусе.

Она не подходит кнопкам, которые стоят в ряд. Возьмём диалог с тремя кнопками в строке: «Назад», «OK» и «Дальше»,
фокус на «Назад». Пользователь нажимает вправо, чтобы перейти на «OK». Вправо — это «больше»; у кнопки нет значения,
которое можно увеличить, поэтому не происходит ничего. Фокус двигается только кнопками вверх и вниз, о чём, глядя на
ряд кнопок, не догадается никто.

Своя таблица программы даёт кнопкам влево и вправо клавиши, которые двигают фокус:

```c
/* three buttons in a row, "Back", "OK", "Next": left and right move the focus along the row */
static const uint8_t row_keys[8] = {
    DGX_UI_KEY_PREV,     /* DGX_UI_PAD_LEFT:  the focus to the button on the left; by default LESS */
    DGX_UI_KEY_NEXT,     /* DGX_UI_PAD_RIGHT: the focus to the button on the right; by default MORE */
    DGX_UI_KEY_PREV,     /* DGX_UI_PAD_UP:    the focus back, as by default */
    DGX_UI_KEY_NEXT,     /* DGX_UI_PAD_DOWN:  the focus on, as by default */
    DGX_UI_KEY_ACTIVATE, /* DGX_UI_PAD_A:     the focused button activated, as by default */
    0xff,                /* DGX_UI_PAD_START: no key, the button is the program's, as by default */
    DGX_UI_KEY_BACK,     /* DGX_UI_PAD_BACK:  back, which no widget takes: the program's, as by default */
    DGX_UI_KEY_BACK,     /* DGX_UI_PAD_B:     the same */
};

static void dialog_keys(dgx_ui_t *ui, bool buttons_in_a_row)
{
    dgx_ui_button_keys(ui, buttons_in_a_row ? row_keys : NULL); /* NULL: the default table again */
}
```

- В таблице 8 записей, по одной на кнопку, в порядке битов `DGX_UI_PAD_*`: LEFT, RIGHT, UP, DOWN, A, START, BACK, B.
- Запись — это `DGX_UI_KEY_*` или 0xff: кнопка не даёт клавиши и остаётся программе.
- Дерево запоминает указатель и ничего не копирует: дайте константу.
- Таблицу можно задать заново в любой момент, например когда диалог-столбец сменился диалогом-строкой. `NULL`
  возвращает таблицу по умолчанию.
- Это таблица того дерева, которое передано в `dgx_ui_buttons()`. Клавиши, которые она даёт, идут и в окна над этим
  деревом, как любая клавиша.

## Касание с DGX-DI

`dgx_ui_di.h` — для программы, в которой есть [DGX-DI](https://github.com/jef-sure/esp32-component-dgx-di). Касание
читается в задаче, владеющей дисплеем, поэтому её будит тикер, здесь каждые 20 мс:

```c
#include "dgx_ui_di.h"

enum { MSG_TOUCH = 14 };

void menu_open(void);

/* in the loop, for the ticker's message MSG_TOUCH; the pointer and the touch are set up as DGX-DI tells */
static void touch_message(dgx_ui_t *ui, dgx_di_pointer_t *pointer, dgx_di_touch_t *touch, uint32_t now_ms)
{
    dgx_ui_di_gestures_t g;
    dgx_di_pointer_read(pointer, now_ms);
    /* the keyboard and the pad of DGX-DI, if the program has them, take their fingers here */
    dgx_ui_di_pointer(ui, pointer, touch, now_ms, &g);
    if (g.edge & DGX_DI_EDGE_LEFT) menu_open(); /* swiped from the left edge */
}
```

- Виджет становится нажатым, как только на него опустился палец, в том числе у края.
- Пока палец держит виджет, обычный свайп и долгое нажатие программе не сообщаются.
- Свайп от края программе сообщается, если только палец не на ползунке, списке или кнопке с `DGX_UI_DRAG`: эти
  владеют пальцем.
- Палец, который опустился мимо виджетов, отдаёт программе все свои жесты. `g.taken` говорит, взял ли нажатие
  виджет.
- В окне на компьютере палец — это мышь, через `dgx_di_host.h` из DGX-DI. Окно DGX читает мышь и клавиши в
  `vTaskDelay()` задачи, которая им владеет, а цикл, который спит на очереди, её не вызывает. Для этого и нужен
  тикер: на тике, который читает касание, сначала вызовите `vTaskDelay(0)`. Так сделано в примере
  [controls](../../examples/controls/main/controls.c).
- [boards](https://github.com/jef-sure/esp32-component-boards) даёт программе дисплей её платы и уже откалиброванный
  указатель DGX-DI, а на компьютере — окно вместо платы. Пример controls берёт оттуда и то и другое.

## Поле ввода и клавиатура

Поле ввода — это `DGX_UI_INPUT` с обликом `dgx_ui_look_input`. Набранное идёт в поле, на котором фокус.

```c
#include "dgx_di_keyboard.h"

void wifi_connect(const char *password);

static char             pass_buf[33];
static dgx_ui_textbox_t pass_box;

static void pass_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) wifi_connect(pass_buf); /* "done" was typed */
    if (what == DGX_UI_BACK) dgx_ui_text_set(ui, i, "");  /* "cancel" was typed */
}

static const dgx_ui_widget_t pass_w = {.kind = DGX_UI_INPUT, .text = &pass_box, .want_h = 14, .look = &dgx_ui_look_input, .on = pass_on};

/* in the loop, after the pointer was read and before the tree gets the finger: the keyboard of DGX-DI takes its
   fingers first and types into the focused field */
static void keyboard_pass(dgx_ui_t *ui, dgx_di_keyboard_t *kb, dgx_di_pointer_t *pointer, uint32_t now_ms)
{
    uint32_t  typed[8];
    const int n = dgx_di_keyboard_take(kb, pointer, now_ms, typed, 8);
    if (n > 0) dgx_ui_typed(ui, typed, n);
}

/* in the loop, right before dgx_ui_paint(): the painter leaves the keyboard's band alone while it is open */
static void keyboard_cover(dgx_ui_paint_t *paint, const dgx_di_keyboard_t *kb)
{
    paint->cover = kb->open ? dgx_ui_rect(kb->x, kb->y, kb->w, kb->h) : dgx_ui_rect(0, 0, 0, 0);
}
```

- Размер буфера — это сколько поле вмещает; буква, которой нет места, не набирается.
- «Стереть» убирает последнюю букву целиком, в UTF-8.
- Функции `on()` сообщается `DGX_UI_CHANGED` на каждую букву, `DGX_UI_FOCUS`, когда поле получает фокус, и
  `DGX_UI_EDIT`, когда на поле нажали пальцем или клавишей, пока оно в фокусе: здесь и открывать клавиатуру.
- Полосу клавиатуры отрисовке называют прямо перед рисованием, а не там, где берут буквы: между ними `on()` мог
  открыть или закрыть клавиатуру, и виджет, нарисованный в этот проход, лёг бы поверх её клавиш.
- Когда клавиатура закрывается и полоса снова свободна, отрисовка сама рисует там дерево.
- Символы из любого другого источника идут тем же путём: `dgx_ui_typed(ui, code_points, count)`.
- Текст стоит у верхнего края поля; с флагом `DGX_UI_TEXT_MIDDLE` в стиле — посередине его высоты.
- Клавиши клавиатуры DGX-DI размером с палец, в миллиметрах стекла. На маленьком дисплее, как 2,8 дюйма у CYD,
  даже цифровая клавиатура занимает весь экран, и поле не видно, пока клавиатура не закроется. Для такого дисплея —
  следующий рецепт.

## Ввод в полосе над клавиатурой

![PIN набирают в полосе вверху дисплея, под ней клавиатура](../images/strip.png)

На маленьком дисплее клавиатура закрывает поле, и не видно, что набрано. Поэтому поле правят в другом месте: в
полосе вверху дисплея, а под ней стоит своя клавиатура программы. Полоса — это всплывающее окно, в котором лежит
копия текста поля. На картинке — `examples/controls` на дисплее CYD, 320 на 240; в PIN пока набраны две цифры.

Пусть в поле «Код» записано `123`:

1. Пользователь нажимает на поле. Его `on()` получает `DGX_UI_EDIT` и вызывает `edit_open()`.
2. `edit_open()` открывает клавиатуру и называет ей поле высотой в один пиксель у самого верха дисплея. Клавиатура
   становится в стороне от названного ей поля, поэтому её полоса уходит вниз; `keypad_kb.y` — где эта полоса
   начинается.
3. От верха дисплея до полосы клавиатуры открывается всплывающее окно: контейнер с одним полем ввода. Это поле
   получает фокус окна и копию текста, так что в полосе видно `123` и курсор после него.
4. Пользователь набирает `9`. `dgx_ui_typed()` сначала отдаёт букву верхнему всплывающему окну, поэтому она
   попадает в копию: в полосе `1239`, в поле под ней по-прежнему `123`.
5. «Готово» на клавиатуре: `on()` полосы получает `DGX_UI_ACTIVATED`, и `edit_close(true)` отдаёт копию полю — в
   нём теперь `1239`. Если вместо этого нажать «отмену», придёт `DGX_UI_BACK`, и `edit_close(false)` только закроет
   полосу. В само поле ничего не писали, поэтому в нём осталось `123` и отменять нечего.

```c
/* the program's keypad: a layout of DGX-DI made a C array by its tools/keyboard_blob.py --c */
extern const uint8_t keypad[];
extern const size_t  keypad_size;

/* a secret field: a star for each letter; the text box keeps the letters */
static const dgx_ui_input_look_t pin_colors = {.ink = INK, .caret = ACCENT, .paper = DGX_UI_PAPER, .secret = true};
static const dgx_ui_look_t       look_pin   = {.draw = dgx_ui_draw_input, .params = &pin_colors, .fill = -1};

static void strip_on(dgx_ui_t *strip, int i, const dgx_ui_node_t *node, int what, void *ctx);

static char             edit_buf[33]; /* the copy that is typed into */
static dgx_ui_textbox_t edit_box;
static const dgx_ui_widget_t strip_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 4};
static const dgx_ui_widget_t strip_in_w = {
    .kind = DGX_UI_INPUT, .text = &edit_box, .flags = DGX_UI_STRETCH | DGX_UI_REST, .look = &dgx_ui_look_input, .on = strip_on};
static const dgx_ui_widget_t strip_pin_w = {
    .kind = DGX_UI_INPUT, .text = &edit_box, .flags = DGX_UI_STRETCH | DGX_UI_REST, .look = &look_pin, .on = strip_on};

static dgx_ui_node_t     strip_nodes[2];
static dgx_ui_t          strip_ui;
static dgx_di_keyboard_t keypad_kb;
static dgx_ui_t         *edit_ui;              /* the tree of the fields; set at the start, with the next three */
static dgx_screen_t     *edit_scr;
static int               edit_mm_w, edit_mm_h; /* the glass in millimetres: the keys are sized by it */
static int               edit_field = -1;      /* the field being edited; -1: none */

static void edit_open(int field)
{
    if (edit_field >= 0) return;
    /* the keyboard keeps away from the field it is given: one at the very top sends its band to the bottom */
    const dgx_di_rect_t top = {.x = 0, .y = 0, .w = (int16_t)edit_scr->width, .h = 1};
    if (!dgx_di_keyboard_open(&keypad_kb, edit_scr, edit_mm_w, edit_mm_h, keypad, keypad_size, TerminusTTFMedium12(), &top)) return;
    const dgx_ui_widget_t *w = dgx_ui_widget(edit_ui, field);
    /* the copy: no longer than the field's own buffer, in the field's style and look */
    const int size = w->text->size < sizeof edit_buf ? w->text->size : (int)sizeof edit_buf;
    dgx_ui_textbox_init(&edit_box, edit_buf, size, 1, w->text->style);
    dgx_ui_init(&strip_ui, strip_nodes, 2, 1);
    dgx_ui_add(&strip_ui, -1, &strip_w);
    const int in = dgx_ui_add(&strip_ui, 0, w->look == &look_pin ? &strip_pin_w : &strip_in_w);
    dgx_ui_text_set(&strip_ui, in, w->text->buf);
    /* the strip: from the top of the display down to the keyboard's band */
    dgx_ui_popup_open(edit_ui, &strip_ui, 0, 0, edit_scr->width, keypad_kb.y, DGX_UI_POPUP_MODAL);
    dgx_ui_focus(&strip_ui, in); /* what is typed goes to the copy */
    edit_field = field;
}

static void edit_close(bool keep)
{
    if (edit_field < 0) return;
    if (keep) dgx_ui_text_set(edit_ui, edit_field, edit_buf); /* done: the copy given to the field */
    dgx_di_keyboard_close(&keypad_kb);
    dgx_ui_popup_close(edit_ui); /* the focus stays on the field: the keys go on from it */
    edit_field = -1;
}

/* the on() of every field edited so: clicked, or activated by a key while it has the focus */
static void field_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EDIT) edit_open(i);
}

static void strip_on(dgx_ui_t *strip, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) edit_close(true); /* "done" on the keypad */
    if (what == DGX_UI_BACK) edit_close(false);     /* "cancel": the field keeps what it had */
}

static char             code_buf[9], pin_buf[9];
static dgx_ui_textbox_t code_box, pin_box;
static const dgx_ui_widget_t code_w = {.kind = DGX_UI_INPUT, .text = &code_box, .want_h = 14, .look = &dgx_ui_look_input, .on = field_on};
static const dgx_ui_widget_t pin_w  = {.kind = DGX_UI_INPUT, .text = &pin_box, .want_h = 14, .look = &look_pin, .on = field_on};
```

- **Цикл** тот же, что в рецепте выше: `keyboard_pass(ui, &keypad_kb, pointer, now_ms)` до того, как палец отдан
  дереву, и `keyboard_cover(paint, &keypad_kb)` перед `dgx_ui_paint()`. Программа вызывает их с деревом полей;
  набранное само находит полосу.
- **Клавиатура** — своя раскладка программы: готовая цифровая клавиатура DGX-DI на CYD не оставляет места для
  полосы. В `examples/controls` это `main/keypad.json`: ряды высотой 7,5 мм, над которыми на CYD остаётся полоса в
  36 пикселей, и нижний ряд из «отмены», нуля, «стереть» и «готово».
- **Копия** — причина, по которой отмена ничего не стоит: в текст поля пишут один раз, по «готово». Копия не
  длиннее буфера поля, поэтому набранное всегда в него помещается.
- **Секретное поле** называет облик, в параметрах которого стоит `.secret = true`: вместо каждой буквы рисуется
  звёздочка, и в поле, и в полосе, а сами буквы остаются в текстовом поле. В шрифте должна быть звёздочка. В
  шрифте, сделанном при сборке под цифры PIN, её нет, и поле тогда ничего не показывает: добавьте `*` к знакам
  запроса.
- **`DGX_UI_EDIT` — это нажатие**, как у кнопки: палец отпущен вскоре и без свайпа. Свайп, начатый на поле, ничего
  не открывает, а повторное нажатие на поле открывает полосу снова.
- **Фокус остаётся на поле**, когда полоса закрывается. Обход клавишами проходит поля и ничего не открывает,
  поэтому клавиши идут с поля дальше, к следующему виджету; клавиша «активировать» на поле открывает его полосу.
- **Кнопку платы, которая значит «готово»**, программа обрабатывает после того, как `dgx_ui_buttons()` получила
  кнопки этого тика, а не вместо этого вызова. Полоса модальная и забирает клавишу. Кнопка, нажатия которой
  `dgx_ui_buttons()` не видела, на следующем тике станет новым нажатием: клавишей «активировать» на поле, которое
  в фокусе, — и полоса откроется снова.

## Виджеты которые появляются и исчезают

```c
enum { TAG_ALARM = 7 };

static char             alarm_buf[40];
static dgx_ui_textbox_t alarm_box;
static const dgx_ui_widget_t alarm_w = {.kind = DGX_UI_LABEL, .text = &alarm_box, .tag = TAG_ALARM};

static void alarm_set(dgx_ui_t *ui, int parent, const char *text)
{
    int a = dgx_ui_find(ui, parent, TAG_ALARM);
    if (text && a < 0) {
        dgx_ui_textbox_init(&alarm_box, alarm_buf, sizeof alarm_buf, 1, &st_left);
        a = dgx_ui_insert(ui, parent, dgx_ui_node(ui, parent)->child, &alarm_w); /* before the first child */
    }
    if (text) dgx_ui_text_set(ui, a, text);
    else if (a >= 0) dgx_ui_remove(ui, a);
}
```

- Дерево запоминает указатель на виджет и ничего не копирует. Дайте ему объект `static const` или память программы,
  которая живёт, пока виджет в дереве. Никогда не давайте составной литерал, созданный внутри функции: он исчезает,
  когда функция возвращается.
- Номер удалённого виджета отдаётся следующему добавленному. Виджет, который появляется и исчезает, ищите по тегу;
  теги начинаются с 1, потому что 0 — тег любого виджета, которому тега не дали.
- `dgx_ui_remove()` удаляет и всё, что под виджетом.
- Виджет, который программа держит в своей памяти, можно менять: его размеры, его картинку. После этого вызовите
  `dgx_ui_relayout()` и для него `dgx_ui_redraw()`.

## Большие данные: сообщение по ссылке

Больше 12 байт идут по ссылке. Сообщение владеет данными от отправки до того, как цикл вызовет
`dgx_ui_msg_done()`: этот вызов один раз вызывает `done()` отправителя, в задаче, владеющей деревом.

```c
#include <stdlib.h>
#include <string.h>

enum { MSG_HEADLINE = 15 };

static void headline_done(void *data, size_t size)
{
    free(data);
}

/* any task: the text is the message's from here on */
static void headline_send(QueueHandle_t queue, const char *text)
{
    char *copy = strdup(text);
    if (copy && !dgx_ui_post_ref(queue, MSG_HEADLINE, copy, strlen(copy) + 1, headline_done)) free(copy); /* not sent: still ours */
}

static bool headline_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_HEADLINE) return false;
    size_t size;
    dgx_ui_text_set(ui, i, dgx_ui_msg_data(msg, &size)); /* copied into the text box now */
    return true;
}
```

- Данные действительны, пока сообщение не выброшено. Виджет, которому они нужны дольше, вызывает в своей `take()`
  функцию `dgx_ui_msg_keep(ui, &copy)`, а когда данные больше не нужны — `dgx_ui_msg_done(&copy)`. Кто оставил
  данные себе, тот забрал сообщение.
- Данные, которые живут вечно, то есть константу, шлёт `dgx_ui_post_const()`: с ними ничего не делается.

## Сообщение из прерывания

```c
enum { MSG_KNOB = 16 };
typedef struct {
    int8_t steps;
} knob_t;

static QueueHandle_t knob_queue;

static void IRAM_ATTR knob_isr(void *arg)
{
    const knob_t k     = {.steps = 1};
    BaseType_t   woken = pdFALSE;
    dgx_ui_post_from_isr(knob_queue, MSG_KNOB, &k, sizeof k, &woken);
    if (woken) portYIELD_FROM_ISR();
}
```

Только по значению. Вызов лежит в IRAM, поэтому им может пользоваться прерывание, которое работает при выключенном
кеше флеша.

## Свой облик

Облик — это функция, которая рисует виджет, параметры своего типа и цвет, который она оставляет под детьми виджета.
Здесь — полоса уровня: значение виджета в виде закрашенной части его прямоугольника.

```c
typedef struct {
    uint8_t bar, rest; /* indices of the table */
} level_look_t;

static bool level_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t   *n    = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *w    = n->widget;
    const level_look_t    *p    = look->params;
    const int              span = w->max > w->min ? w->max - w->min : 1;
    const int              full = n->w * (n->state_value.value - w->min) / span;
    const int              x = n->x - paint->ox, y = n->y - paint->oy;
    if (full > 0) dgx_fill_rectangle(paint->scr, x, y, full, n->h, dgx_ui_color(paint, p->bar));
    if (full < n->w) dgx_fill_rectangle(paint->scr, x + full, y, n->w - full, n->h, dgx_ui_color(paint, p->rest));
    return false; /* no more time asked */
}

static const level_look_t    level_colors = {.bar = ACCENT, .rest = FILL};
static const dgx_ui_look_t   level_look   = {.draw = level_draw, .params = &level_colors, .fill = -1};
static const dgx_ui_widget_t level_w      = {.kind = DGX_UI_OWN, .min = 0, .max = 100, .want_h = 6, .look = &level_look};
```

Правила, которых держится облик:

- **Где.** Он рисует прямоугольник виджета, `n->x, n->y, n->w, n->h`, за вычетом `paint->ox, paint->oy`: экраном может быть
  буфер, в котором лежит часть картины. Отсечение уже поставлено, и облик не меняет его насовсем.
- **Каждый пиксель один раз.** Пишите каждый пиксель того, что рисуете, один раз, и на дисплее за шиной виджет не
  мигает. Полоса выше — два прямоугольника, которые не перекрываются.
- **Весь прямоугольник, когда `paint->whole`.** Тогда на экране от виджета нет ничего. Облик, который рисует только
  разницу, обязан в этом случае нарисовать всё. Облики библиотеки рисуют весь прямоугольник каждый раз.
- **Цвета** берутся из `dgx_ui_color(paint, index)`. `paint->paper` — номер цвета под виджетом.
- **Вызов по частям.** Когда на части виджета лежит всплывающее окно или клавиатура, облик вызывается по разу для
  каждой видимой части, с отсечением по ней. В каждом вызове он должен рисовать одно и то же. Облик, который помнит,
  что нарисовал, записывает это только в последнем: когда `paint->part == paint->parts - 1`, а `paint->composing`
  ложно. На дисплее в 18 или 24 бита, когда названы полосы, облик вызывается ещё и по разу на каждую полосу части,
  с установленным `paint->whole`, поэтому каждый раз рисует весь свой прямоугольник; `paint->composing` стоит во всех
  полосах, кроме последней, так что проверка остаётся той же.
- **Время.** Облик с анимацией возвращает `true`, и его вызывают снова на следующем проходе. Где он находится, он
  считает по `now_ms`.
- **Его заливка.** `fill` — номер цвета, который облик оставляет под детьми виджета, или -1, если он оставляет то,
  что было. У рамки это её заливка.
- **Его бумага.** Там, где бумага облика — то, что лежит под виджетом, он кладёт её вызовом
  `dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y, w, h)`, а не своей заливкой: это цвет `paint->paper` или, когда
  стоит `paint->on_background`, картинка программы. На картинке облик рисует буквы и картинки без своей бумаги: без
  поля текста, без `DGX_FONT_FILL` и без `DGX_IMAGE_OPAQUE`.

## Свой виджет, которым управляют палец и клавиши

Виджет вида `DGX_UI_OWN` рисуется своим обликом, и только: `dgx_ui_hit()` его не находит, нажатие на него никому
не достаётся, клавиши его обходят. Если облик называет **поведение**, виджет становится элементом управления.
Общее для всех элементов делает ядро: находит виджет под пальцем, ставит `DGX_UI_PRESSED`, даёт ему фокус, решает,
было ли отпускание щелчком, не трогает скрытый и выключенный виджет. Поведение — четыре функции для того, что у
виджета своё; любая может быть `NULL`.

Пример — площадка с двумя осями: x пальца — значение виджета, y — число в данных программы.

```c
void point_set(int x, int y); /* the program's: what it does with the two axes, each 0..100 */

/* a pad of two axes: x is the widget's value, 0..100; y lies in the program's data, 0..100 */
typedef struct {
    int16_t y;
} xy_data_t;

enum { XY_KNOB = 6 }; /* the side of the knob, in pixels */

static bool xy_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    const xy_data_t     *d = n->widget->own;
    const int            x = n->x - paint->ox, y = n->y - paint->oy;
    const int            kx = (n->w - XY_KNOB) * n->state_value.value / 100, ky = (n->h - XY_KNOB) * d->y / 100;
    const bool           lit   = n->state_value.bits & (DGX_UI_PRESSED | DGX_UI_FOCUSED);
    const uint32_t       field = dgx_ui_color(paint, FILL), knob = dgx_ui_color(paint, lit ? ACCENT : INK);
    /* the field in four rectangles round the knob, then the knob: every pixel once */
    if (ky > 0) dgx_fill_rectangle(paint->scr, x, y, n->w, ky, field);
    if (ky + XY_KNOB < n->h) dgx_fill_rectangle(paint->scr, x, y + ky + XY_KNOB, n->w, n->h - ky - XY_KNOB, field);
    if (kx > 0) dgx_fill_rectangle(paint->scr, x, y + ky, kx, XY_KNOB, field);
    if (kx + XY_KNOB < n->w) dgx_fill_rectangle(paint->scr, x + kx + XY_KNOB, y + ky, n->w - kx - XY_KNOB, XY_KNOB, field);
    dgx_fill_rectangle(paint->scr, x + kx, y + ky, XY_KNOB, XY_KNOB, knob);
    return false;
}

/* the finger at x, y of the widget's own pixels: both axes set as the user sets them */
static void xy_at(dgx_ui_t *ui, int i, int x, int y)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    xy_data_t           *d = n->widget->own;
    /* while it is held the finger may be anywhere, outside the widget too */
    const int nx = x * 100 / (n->w - 1), ny = y <= 0 ? 0 : y >= n->h - 1 ? 100 : y * 100 / (n->h - 1);
    if (ny != d->y) {
        d->y = (int16_t)ny;
        dgx_ui_redraw(ui, i);               /* data of its own changed: to be drawn */
        dgx_ui_tell(ui, i, DGX_UI_CHANGED); /* and on() told, as for a value */
    }
    dgx_ui_user_value(ui, i, nx); /* kept within min..max; on() told when it changed */
}

static bool xy_move(dgx_ui_t *ui, int i, int x, int y)
{
    xy_at(ui, i, x, y);
    return true; /* a drag: no click when it is let go, and a swipe from an edge is not the program's */
}

static bool xy_key(dgx_ui_t *ui, int i, int key)
{
    if (key != DGX_UI_KEY_MORE && key != DGX_UI_KEY_LESS) return false; /* the rest is the core's */
    dgx_ui_user_value(ui, i, dgx_ui_node(ui, i)->state_value.value + (key == DGX_UI_KEY_MORE ? 5 : -5));
    return true;
}

static const dgx_ui_behavior_t xy_behavior = {.flags = DGX_UI_BEHAVIOR_FOCUS, .down = xy_at, .move = xy_move, .key = xy_key};
static const dgx_ui_look_t     xy_look     = {.draw = xy_draw, .fill = -1, .behavior = &xy_behavior};

static xy_data_t xy_data;
static void      on_xy(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_CHANGED) point_set(node->state_value.value, xy_data.y);
}
static const dgx_ui_widget_t xy_w = {
    .kind = DGX_UI_OWN, .min = 0, .max = 100, .want_w = 40, .want_h = 40, .look = &xy_look, .own = &xy_data, .on = on_xy};
```

Что происходит по шагам на площадке 40 на 40 пикселей:

1. Палец опускается в точку 30, 10 виджета. Ядро ставит `DGX_UI_PRESSED`, даёт виджету фокус и вызывает
   `down(ui, i, 30, 10)`. `xy_at()` записывает 25 в `xy_data.y`, помечает виджет и сообщает `on()` событие
   `DGX_UI_CHANGED`; затем `dgx_ui_user_value()` ставит значение 76, и `on()` вызывается ещё раз.
2. Палец движется, куда угодно: виджет держит его, пока палец не поднят, поэтому x и y бывают отрицательными и
   больше размеров виджета. `move()` вызывается при каждом опросе указателя. Она возвращает `true`: это
   перетаскивание. После этого отпускание — уже не щелчок, а свайп от края, начатый на площадке, достаётся ей, а не
   программе (`dgx_ui_dragging()`).
3. Палец поднят: `up()`, если она есть, вызывается с `click`, равным `false`. Если бы виджет не перетаскивали и
   отпустили раньше `DGX_UI_CLICK_MS`, `click` был бы `true`, и после `up()` ядро сообщило бы `on()` событие
   `DGX_UI_ACTIVATED`.
4. Клавиши: `key()` виджета в фокусе спрашивают первой, о каждой клавише. Площадка берёт `DGX_UI_KEY_MORE` и
   `DGX_UI_KEY_LESS`. С тем, на что она ответила `false`, ядро поступает как обычно: «следующий» и «предыдущий»
   переводят фокус дальше, `DGX_UI_KEY_ACTIVATE` сообщает `on()` событие `DGX_UI_ACTIVATED`, «назад» остаётся
   программе.

На что опирается код:

- **`dgx_ui_user_value()`** ставит значение так, как его ставит пользователь: в пределах `min`..`max`, виджет
  помечен, `on()` получает `DGX_UI_CHANGED`, если значение изменилось, и ничего, если оно то же.
  `dgx_ui_set_value()` — установка самой программой, о ней `on()` не сообщают.
- **`dgx_ui_tell()`** сообщает `on()` событие, о котором ядро не знает: здесь это вторая ось. Свои данные виджета,
  которые рисует его облик, помечают вызовом `dgx_ui_redraw()`.
- **Щелчок или перетаскивание.** Площадка возвращает `true` на каждое движение, поэтому щелчка у неё не бывает.
  Виджет, который и перетаскивают, и щёлкают, возвращает `true` только когда палец ушёл достаточно далеко от
  места, где опустился. У виджета, который только нажимают, `move()` нет.
- **Только палец.** Без `DGX_UI_BEHAVIOR_FOCUS` виджет не берёт фокус: клавиши его обходят, а нажатие на него
  оставляет фокус там, где он был. Такой виджет — клавиша своей экранной клавиатуры: поле, в которое она печатает,
  сохраняет фокус. Для него хватает поведения из одних `NULL`, как у полосы ниже.
- **Только для `DGX_UI_OWN`.** Поведение, названное обликом кнопки или ползунка, не вызывается: они работают как
  свой вид.
- **Пока нет.** Набранные символы идут только в `DGX_UI_INPUT`. Виджет нажимается по всему своему прямоугольнику,
  круглый — и в углах.

Виджет только для пальца — полоса уровня из рецепта выше, которую можно нажать:

```c
/* the level bar of the recipe above, tapped: pressed and clicked, the focus left where it is */
static const dgx_ui_behavior_t tap_behavior = {0};
static const dgx_ui_look_t     tap_look     = {.draw = level_draw, .params = &level_colors, .fill = -1, .behavior = &tap_behavior};
static const dgx_ui_widget_t   tap_w        = {.kind = DGX_UI_OWN, .min = 0, .max = 100, .want_h = 6, .look = &tap_look};
```

## Картинка, которая живёт между проходами

Облик рисует свой виджет заново при каждом изменении. У виджета с флагом `DGX_UI_CANVAS` вместо этого есть холст:
экран DGX в 8 бит размером с его прямоугольник, который отрисовка хранит от прохода к проходу. Облик дорисовывает в нём
только новое и кладёт его на дисплей. Возьмём график последних значений CO2, по столбику на каждое:

1. Приходит первое значение. Облик просит холст; он новый, `made` истинно, и облик рисует всё, что есть в графике:
   один столбик. Потом кладёт холст на дисплей.
2. Приходит второе значение. Холст такой, каким его оставил прошлый проход, поэтому облик дорисовывает ещё один
   столбик, второй, и снова кладёт холст.
3. Над графиком открывается и закрывается всплывающее окно. Отрисовка вызывает облик, чтобы график целиком снова
   оказался на дисплее. Облик ничего не рисует: новых столбиков нет. Он кладёт холст, и график на месте.

```c
enum { HISTORY = 120 };
static uint8_t history_level[HISTORY]; /* the height of each column of the graph, in pixels */

static bool history_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n     = dgx_ui_node(ui, i);
    const uint8_t       *level = n->widget->own;
    bool                 made;
    dgx_screen_t        *c = dgx_ui_canvas(paint, ui, i, &made);
    if (c) {
        /* a canvas just made, or the graph begun again: all of it; else only the columns that came since it was drawn */
        const int count = n->state_value.value, was = n->state_drawn.value;
        const int from  = made || count < was ? 0 : was;
        if (from == 0) dgx_fill_rectangle(c, 0, 0, c->width, c->height, 0);
        for (int k = from; k < count && k < c->width; ++k) dgx_fill_rectangle(c, k, c->height - level[k], 1, level[k], ACCENT);
    }
    dgx_ui_canvas_put(paint, ui, i, -1); /* on the screen: its values are colours of the table, 0 the paper */
    return false;
}

static const dgx_ui_look_t   history_look = {.draw = history_draw, .fill = -1};
static const dgx_ui_widget_t history_w    = {
    .kind = DGX_UI_OWN, .flags = DGX_UI_CANVAS, .max = HISTORY, .want_h = 20, .own = history_level, .look = &history_look};

/* a new value: one more column; the graph begins again when it is full */
static void history_add(dgx_ui_t *ui, int graph, int ppm)
{
    int count = dgx_ui_node(ui, graph)->state_value.value;
    if (count >= HISTORY) count = 0;
    history_level[count] = (uint8_t)(ppm >= 2000 ? 40 : ppm / 50);
    dgx_ui_set_value(ui, graph, count + 1);
}
```

- **`dgx_ui_canvas()`** даёт холст виджета; он создаётся при первой надобности после раскладки и заполнен нулями.
  `made` истинно, когда холст новый или создан заново в другом размере: в нём ничего нет, и облик рисует всё. Иначе
  облик рисует только то, что изменилось. Что именно, он считает по тому, что ему дано, — здесь по значению
  виджета сейчас и по тому, каким оно было нарисовано, — чтобы дважды за проход нарисовать одно и то же.
- **`dgx_ui_canvas_put()`** кладёт холст на экран, а остаток прямоугольника — бумагой. С -1 значения холста — номера
  цветов таблицы отрисовки, 0 — бумага. С номером цвета таблицы значения — количества этих чернил поверх бумаги,
  0 — нет, 255 — сплошь; так их оставляет мягкое перо DGX.
- **Экономится** рисование, а не передача: холст уходит на дисплей целиком, один раз, поэтому ничего не мигает.
- **Чего это стоит.** Байт ОЗУ на пиксель прямоугольника — 9600 байт для графика 240 на 40 пикселей — и около 1,5 КБ
  кода для программы, у которой есть холст. Нужны `CONFIG_DGX_ENABLE_VSCREEN=y` и 16-битный экран; на другом
  прямоугольник виджета — бумага.
- **Если памяти на холст нет**, прямоугольник виджета — бумага, об этом пишется в журнал, а `on()` виджета получает
  `DGX_UI_NO_MEMORY`, когда проход закончен, один раз для этого размера. Новая попытка делается каждый раз, когда
  облик просит холст. Холст, который не удалось создать большего размера, остаётся прежним и обрезается по
  прямоугольнику.
- **Одна структура отрисовки — одно дерево.** Холсты принадлежат структуре отрисовки, для дерева, которое она
  рисует, и окон над ним; холст освобождается, когда его виджет уходит из дерева или его дерево больше не
  рисуется. Двум деревьям, которые по очереди рисуются на одном экране, нужно по своей структуре отрисовки.

## График-развёртка

График из рецепта выше при каждом значении уходит на дисплей целиком, а его холст занимает байт ОЗУ на пиксель.
Графику, который только растёт, не нужно ни то ни другое: его облик пишет один новый столбик. Возьмём тот же
график CO2:

1. Приходит значение. Значение виджета — число столбиков — меняется с 5 на 6. Облик вызывается со значением,
   какое оно сейчас и каким было нарисовано, и пишет один столбик, пятый: 40 пикселей дисплея.
2. Над графиком закрывается всплывающее окно. Облик вызывается с `paint->whole` и пишет все шесть столбиков и
   бумагу после них.

```c
enum { TRACE = 120 };
static uint8_t trace_level[TRACE]; /* the height of each column, in pixels */

static bool trace_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n     = dgx_ui_node(ui, i);
    const uint8_t       *level = n->widget->own;
    const int            count = n->state_value.value < n->w ? n->state_value.value : n->w, was = n->state_drawn.value;
    /* drawn whole, or begun again: every column and the paper after them; else only the columns that came since */
    const bool all  = paint->whole || count < was;
    for (int k = all ? 0 : was; k < count; ++k) {
        const int h = level[k] < n->h ? level[k] : n->h;
        dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x + k, n->y, 1, n->h - h);
        if (h) dgx_fill_rectangle(paint->scr, n->x + k - paint->ox, n->y + n->h - h - paint->oy, 1, h, dgx_ui_color(paint, ACCENT));
    }
    if (all) dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x + count, n->y, n->w - count, n->h);
    return false;
}

static const dgx_ui_look_t   trace_look = {.draw = trace_draw, .fill = -1};
static const dgx_ui_widget_t trace_w    = {.kind = DGX_UI_OWN, .max = TRACE, .want_h = 20, .own = trace_level, .look = &trace_look};

/* a new value: one more column; the graph begins again when it is full */
static void trace_add(dgx_ui_t *ui, int graph, int ppm)
{
    int count = dgx_ui_node(ui, graph)->state_value.value;
    if (count >= TRACE) count = 0;
    trace_level[count] = (uint8_t)(ppm >= 2000 ? 40 : ppm / 50);
    dgx_ui_set_value(ui, graph, count + 1);
}
```

- **На дисплей** при новом значении уходит один столбик. Холст из рецепта выше ради того же отправляет 9600
  пикселей.
- **Хранится** по байту на столбик, в массиве самой программы. Когда виджет рисуется целиком, облик рисует по
  нему.
- **Правило** то же, что у любого облика: при `paint->whole` пишется весь виджет, каждый пиксель один раз.
- Холст лучше, когда картинку дорого рисовать заново или её рисует мягкое перо; этот способ — когда она
  восстанавливается так же дёшево, как здесь. Графики с осями и подписями — дело DGX-LVGL.

## Шкала по кругу с виджетами внутри

![Станция CO2: цветная шкала по кругу, внутри число и два значения](../images/ring.png)
![То же с раскладкой: прямоугольники виджетов показаны штрихами](../images/ring-layout.png)

Шкала, которая идёт по кругу экрана, с виджетами внутри — это одно дерево: кольцо — виджет собственного вида
программы, а остальные виджеты — его дети, как у рамки. Всплывающее окно для этого не нужно. Штрихи на второй
картинке показывают, что дала раскладка: квадрат, который отступ кольца оставляет внутри круга, и в нём прямоугольники
числа, его единицы, пустого контейнера, который занимает остаток, и строки с двумя надписями.

Облик кольца рисует `dgx_gauge_t` из DGX:

```c
#include "dgx_gauge.h"

/* the colours of the scale, packed for the screen at the start */
static uint32_t ring_low, ring_mid, ring_high, ring_off;

static uint32_t ring_color(int ppm)
{
    return ppm < 800 ? ring_low : ppm < 1200 ? ring_mid : ring_high;
}

typedef struct {
    uint8_t width; /* of the ring, in points */
} ring_look_t;

/* what the ring remembers: the gauge of DGX, which draws only the steps that changed */
typedef struct {
    dgx_gauge_t gauge;
    bool        made;
} ring_t;

static bool ring_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t   *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *w = n->widget;
    const ring_look_t     *p = look->params;
    ring_t                *r = w->own;
    const int thick = p->width * ui->unit, outer = (n->w < n->h ? n->w : n->h) / 2 - 2;
    if (paint->whole || !r->made) {
        /* nothing of it is on this screen: its paper around the children, the empty ring, the steps up to the value */
        dgx_gauge_t fresh;
        dgx_ui_fill_around(paint, ui, i, dgx_ui_rect(n->x, n->y, n->w, n->h), dgx_ui_color(paint, paint->paper));
        dgx_gauge_init(&fresh, paint->scr, n->x + n->w / 2 - paint->ox, n->y + n->h / 2 - paint->oy, outer - thick, thick, 225.0f, 270,
                       w->min, w->max, ring_off, ring_color);
        dgx_gauge_set_value(&fresh, n->state_value.value);
        if (!paint->composing) r->gauge = fresh, r->made = true; /* remembered only by the painter's own pass */
    } else if (paint->part == 0) {
        dgx_gauge_set_value(&r->gauge, n->state_value.value); /* only the steps that changed */
    } else {
        dgx_gauge_redraw(&r->gauge); /* another part of it, beside a popup: all of the ring there */
    }
    return false;
}
```

Виджеты. Одно сообщение со значением получают и кольцо, и число. Две надписи внизу — те же, что в рецепте
[Одно сообщение для нескольких виджетов](#одно-сообщение-для-нескольких-виджетов).

```c
enum { MSG_PPM = 17 };
typedef struct {
    int16_t ppm;
} ppm_t;
DGX_UI_MESSAGE(ppm, MSG_PPM, ppm_t)

static bool ring_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    ppm_t v;
    if (msg_decode_ppm(msg, &v) == ESP_OK) dgx_ui_set_value(ui, i, v.ppm);
    return false; /* the number inside reads the same message */
}

static bool ppm_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    ppm_t v;
    if (msg_decode_ppm(msg, &v) == ESP_OK) dgx_ui_text_printf(ui, i, "%d", v.ppm);
    return false;
}

static char                ppm_buf[8], ppm_unit_buf[12], ring_temp_buf[12], ring_rh_buf[8];
static dgx_ui_textbox_t    ppm_box, ppm_unit_box;
static dgx_ui_text_style_t st_ring_right;
static ring_t              ring_data;
static const ring_look_t   ring_params = {.width = 14};
static const dgx_ui_look_t ring_look   = {.draw = ring_draw, .params = &ring_params, .fill = -1};

/* the ring is a column: its pad keeps its children inside the circle */
static const dgx_ui_widget_t ring_w = {
    .kind = DGX_UI_OWN, .flags = DGX_UI_COLUMN, .pad = 47, .gap = 2, .min = 400, .max = 2000,
    .look = &ring_look, .own = &ring_data, .take = ring_take};
static const dgx_ui_widget_t ppm_w       = {.kind = DGX_UI_LABEL, .text = &ppm_box, .want_h = 44, .take = ppm_take};
static const dgx_ui_widget_t ppm_unit_w  = {.kind = DGX_UI_LABEL, .text = &ppm_unit_box};
static const dgx_ui_widget_t ring_rest_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
static const dgx_ui_widget_t ring_row_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 13};
static const dgx_ui_widget_t ring_temp_w = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &temp_box, .take = temp_take};
static const dgx_ui_widget_t ring_rh_w   = {.kind = DGX_UI_LABEL, .text = &rh_box, .want_w = 50, .take = rh_take};

/* the tree of a round screen of 240: the ring is its root */
static int ring_add(dgx_ui_t *ui, dgx_screen_t *scr)
{
    ring_low  = dgx_screen_rgb(scr, 60, 200, 90);
    ring_mid  = dgx_screen_rgb(scr, 240, 200, 40);
    ring_high = dgx_screen_rgb(scr, 230, 60, 50);
    ring_off  = dgx_screen_rgb(scr, 40, 46, 60);
    st_ring_right = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    dgx_ui_textbox_init(&ppm_box, ppm_buf, sizeof ppm_buf, 1, &st_city); /* the large font, or the small one */
    dgx_ui_textbox_init(&ppm_unit_box, ppm_unit_buf, sizeof ppm_unit_buf, 1, &st_centre);
    dgx_ui_textbox_init(&temp_box, ring_temp_buf, sizeof ring_temp_buf, 1, &st_left);
    dgx_ui_textbox_init(&rh_box, ring_rh_buf, sizeof ring_rh_buf, 1, &st_ring_right);
    const int ring = dgx_ui_add(ui, -1, &ring_w);
    dgx_ui_add(ui, ring, &ppm_w);
    dgx_ui_text_set(ui, dgx_ui_add(ui, ring, &ppm_unit_w), "ppm CO2");
    dgx_ui_add(ui, ring, &ring_rest_w); /* pushes the row to the bottom of the square */
    const int row = dgx_ui_add(ui, ring, &ring_row_w);
    dgx_ui_add(ui, row, &ring_temp_w);
    dgx_ui_add(ui, row, &ring_rh_w);
    return ring;
}
```

- **Отступ** держит детей внутри круга. На экране 240 внешний радиус кольца 118, ширина 14, значит, радиус
  внутреннего круга 104. У квадрата внутри этого круга сторона 104 x 1,41 = 147, поэтому отступ равен
  (240 - 147) / 2 = 47.
- **Новое значение** вызывает облик кольца один раз, и шкала DGX рисует только те деления, которые изменились.
  Число пишет своё поле. Больше ничего не трогается.
- **Целиком**, при первой отрисовке и после поворота, кольцо заливает свой прямоугольник вокруг детей вызовом
  `dgx_ui_fill_around()` и рисует себя поверх этой бумаги: пиксели кольца в этом случае пишутся дважды, пиксели
  детей — один раз.
- **Под всплывающим окном** часть кольца может быть видна рядом с ним. Тогда облик вызывается для каждой части;
  после первой шкала уже помнит свои деления, поэтому остальные части рисуются целиком.
- Если ребёнка скрыли или показали, кольцо рисуется целиком вместе с детьми: вокруг оставшихся на месте заливают
  только контейнер и рамка.

## Дисплей на 18 или 24 бита

На 16-битном дисплее текст уходит за один проход: буквы и бумага вокруг них вместе. Дисплей за шиной на 18 или 24
бита принимает его иначе: сначала поле заливается бумагой, потом поверх пишутся буквы. Возьмём термометр, у
которого надпись меняется с «21.5 C» на «21.6 C»:

1. Если рисовать прямо на дисплей, поле надписи заливается и на мгновение остаётся пустым, потом появляются буквы.
   Каждый пиксель буквы пишется дважды, и число мигает при каждой смене.
2. Если дать буфер и назвать полосы, отрисовка рисует надпись в буфере, полосой во столько строк, сколько в него
   помещается, и каждую полосу отправляет на дисплей одним окном. Каждый пиксель дисплея пишется один раз, ничего
   не мигает.

```c
/* eight rows of a display 320 pixels wide, 3 bytes a pixel; on a chip, memory its bus can send by DMA */
static uint8_t deep_buffer[320 * 3 * 8];

static void deep_paint_init(dgx_ui_paint_t *paint)
{
    paint->buffer      = deep_buffer;
    paint->buffer_size = sizeof deep_buffer;
    paint->strips      = &dgx_ui_strips; /* a widget without children goes through the buffer, strip by strip */
}
```

- **Через буфер идёт** каждый виджет, у которого нет детей: надпись, элемент управления, картинка в 1 бит.
  Контейнер или рамка с детьми рисуются прямо: они только заливают место вокруг детей, каждый пиксель один раз при
  любой глубине.
- **Чего это стоит на шине.** Изменившийся виджет отправляется целиком, всем прямоугольником; без буфера писалось бы
  только его поле.
- **Чего это стоит в ОЗУ.** Не меньше строки самого широкого виджета: его ширина в пикселях на 3 байта. Чем больше
  строк, тем меньше передач. Виджет, строка которого в буфер не помещается, рисуется прямо и мигает; об этом один
  раз пишется в журнал.
- **Чего это стоит во флеше.** Около 0,6 КБ в dgx-ui и 5 КБ переносов DGX — для программы, которая назвала
  полосы. Программа, которая их не назвала, ничего из этого не несёт; поэтому их и называют, а не включают сами.
- **16-битный дисплей** рисуется прямо, что бы ни было названо: его текст сначала не заливается.
- Нужен `CONFIG_DGX_ENABLE_VSCREEN=y`; если назвать полосы без него, программа не соберётся.
- Картинка RGB565 и картинка в 4 или 8 бит на таком дисплее — бумага: их обликам нужен 16-битный экран.

## Цикл, который спит до следующего момента

Цикл из [Начала работы](getting-started.md) возвращается через один тик, пока какой-нибудь виджет просит время.
Возьмём важный текст, который каждые две секунды поднимается на строку:

1. С тем циклом облик текста просит время все две секунды. Задача просыпается каждый тик — двести раз при тике в
   10 мс, — и каждый раз облик сразу возвращается: строке ещё не время.
2. Облик знает, когда строке время, и называет этот момент. `dgx_ui_wake()` отдаёт циклу ближайший из названных
   моментов: 2000, потом меньше, по мере того как время идёт. Цикл, который ждёт столько, просыпается ради строки
   один раз.

```c
/* the loop of the task that owns the display, asleep until the next moment a look named or the next message */
static void ui_loop_asleep(dgx_ui_t *ui, dgx_ui_paint_t *paint, QueueHandle_t queue)
{
    TickType_t wait = 0;
    for (;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            dgx_ui_take(ui, &msg);
            dgx_ui_msg_done(&msg);
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(ui, now);
        dgx_ui_paint(ui, paint, now);
        const uint32_t ms = dgx_ui_wake(paint, ui, now);
        /* nobody asks: till a message comes; else so long, a tick at least */
        wait = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
```

- **`dgx_ui_wake()`** возвращает, сколько миллисекунд цикл может ждать: наименьшее из того, что назвали облики на
  последнем проходе, считая от `now`, и из следующего повтора удерживаемой клавиши. `DGX_UI_WAKE_NONE` значит, что
  никто не просит.
- **0 значит «на следующем проходе»**: облик попросил время и момента не назвал. Ждать тогда надо один тик, а не
  нисколько: цикл, который совсем не ждёт, крутится без остановки.
- **Сообщение будит цикл раньше**; на следующем проходе моменты называются заново.
- **Свой облик** называет свой момент вызовом `dgx_ui_wake_in(paint, ms)` перед тем, как вернуть `true`. Без этого
  его вызывают на следующем проходе, как раньше.
- Цикл, который смотрит только на то, что вернули `dgx_ui_paint()` и `dgx_ui_time()`, работает как работал.

## Зеркало экрана

Дисплей за шиной прочитать нельзя, поэтому его картинка — для снимка экрана или для показа на компьютере —
рисуется заново. Отрисовка собирает любой прямоугольник экрана от корня вниз, а `dgx_ui_paint_area_to()` делает
это на другой экран: полосу в несколько строк поверх небольшого массива.

```c
void mirror_send(const uint16_t *pixels, int count); /* the program's: a strip to a computer, a file */

static uint16_t mirror_rows[320 * 8]; /* eight rows of RGB565 */

/* the picture of a display 320 by 240, strip by strip; kb: the keyboard of DGX-DI when the program has one, else NULL */
static void mirror(dgx_ui_t *ui, dgx_ui_paint_t *paint, const dgx_di_keyboard_t *kb)
{
    dgx_screen_t *band = dgx_vscreen_from_array(mirror_rows, 320, 8, 16, DgxScreenRGB);
    if (!band) return;
    for (int y = 0; y < 240; y += 8) {
        dgx_ui_paint_area_to(ui, paint, band, NULL, 0, y, 320, 8);
        if (kb) dgx_di_keyboard_draw_to(kb, band, 0, y); /* the keyboard over the tree, where it is open */
        mirror_send(mirror_rows, 320 * 8);
    }
    dgx_screen_destroy(&band);
}
```

- **Структура отрисовки** даётся та же, которой рисуется дисплей: холсты виджетов принадлежат ей, и зеркало
  показывает их как есть. Перед возвратом вызов ставит экран структуры обратно.
- **Всплывающие окна**, открытые над деревом, в картинку попадают, каждое поверх того, что под ним.
- **Цвета** — таблица структуры отрисовки, если у зеркала та же глубина, что у дисплея; зеркало другой глубины
  даёт свою таблицу вместо `NULL`.
- **То, что движется**, показано так, как его нарисовал последний проход: обликам даётся время последнего
  `dgx_ui_paint()`.
- **Клавиатура и пад DGX-DI** рисуют себя сами, мимо дерева, поэтому отрисовка в картинку их не кладёт. DGX-DI
  начиная с 0.1.7 рисует их на ту же полосу, после дерева: клавиатуру — как в коде выше, ей даётся пиксель дисплея
  в углу полосы, а пад — перед ней, вызовом `dgx_di_pad_draw_to(pad, band, 0, y)`. В `main/idf_component.yml`
  программы тогда нужен `jef-sure/dgx-di` версии 0.1.7 или новее.

## Дерево как слой в чужих полосах

Программе, которая сама шлёт на дисплей свою картинку полосами — игра, карта, видео, — нужно, чтобы виджеты
попадали в те же полосы, а не рисовались поверх них потом. Это делают два вызова. `dgx_ui_changed()` делает тот же
проход, что `dgx_ui_paint()`, но ничего не пишет: каждый изменившийся прямоугольник он сообщает функции программы
и отмечает нарисованным. `dgx_ui_paint_area_to()` потом собирает любой прямоугольник дерева на экран программы.
В примере сцена — фон-картинка дерева, поэтому одна сборка кладёт и сцену, и виджеты поверх неё.

```c
/* the compositor's own: a piece of its picture laid into a screen, and a strip sent to the display */
void scene_piece(dgx_screen_t *to, int to_x, int to_y, int x, int y, int w, int h);
void strip_send(const uint16_t *pixels, int top, int rows);

enum { LAYER_W = 320, LAYER_H = 240, STRIP_ROWS = 16, STRIPS = LAYER_H / STRIP_ROWS };

static uint16_t strip_pixels[LAYER_W * STRIP_ROWS]; /* a strip of RGB565 */
static bool     strip_marked[STRIPS];               /* to be composed and sent */

/* the scene is the background of the tree: the painter asks for its pieces where the tree shows what lies under it */
static void scene_under(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    scene_piece(paint->scr, x - paint->ox, y - paint->oy, x, y, w, h);
}

/* told a rectangle that changed, by the tree or by the scene itself: the strips it meets go out again */
static void layer_changed(void *ctx, dgx_ui_rect_t rect)
{
    for (int number = rect.y < 0 ? 0 : rect.y / STRIP_ROWS; number < STRIPS && number * STRIP_ROWS < rect.y + rect.h; ++number) strip_marked[number] = true;
}

static void layer_init(dgx_ui_paint_t *paint)
{
    dgx_ui_background_set(paint, scene_under, NULL);
}

/* in the loop, where dgx_ui_paint() would be; true while a look asks for time */
static bool layer_show(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms)
{
    const bool    busy  = dgx_ui_changed(ui, paint, now_ms, layer_changed, NULL);
    dgx_screen_t *strip = dgx_vscreen_from_array(strip_pixels, LAYER_W, STRIP_ROWS, 16, DgxScreenRGB);
    if (!strip) return busy;
    for (int number = 0; number < STRIPS; ++number) {
        if (!strip_marked[number]) continue;
        strip_marked[number] = false;
        /* the scene and the tree over it composed together: every pixel of the strip is written once */
        dgx_ui_paint_area_to(ui, paint, strip, NULL, 0, number * STRIP_ROWS, LAYER_W, STRIP_ROWS);
        strip_send(strip_pixels, number * STRIP_ROWS, STRIP_ROWS);
    }
    dgx_screen_destroy(&strip);
    return busy;
}
```

Что происходит на дисплее 320 на 240, который шлют полосами по 16 строк:

1. Первый вызов сообщает один прямоугольник, весь экран: собираются и отправляются все 15 полос.
2. Ничего не меняется: вызов ничего не сообщает, ничего не отправляется.
3. Пользователь двигает ползунок, который лежит в строках с 100 по 111. Сообщается его прямоугольник, полоса 6
   помечается, собирается — кусок сцены с ползунком поверх — и отправляется: 16 строк, а не 240, и ни один пиксель
   не пишется дважды.
4. Где-то меняется сцена. Компоновщик помечает полосы сам, той же `layer_changed()`. Сборка рисует дерево таким,
   какое оно сейчас, поэтому спрашивать перед ней, что изменилось, не нужно.
5. Текст, у которого появляются буквы, сообщается при каждом вызове, пока он просит время; `layer_show()`
   возвращает `true`, а `dgx_ui_wake()` говорит, когда звать снова.

Чего держаться:

- **Одно из двух.** Дерево ведёт либо `dgx_ui_paint()`, либо `dgx_ui_changed()`, но не оба: после
  `dgx_ui_changed()` пометки сняты, а на экране отрисовки ничего нет.
- **Экран отрисовки** — сам дисплей. По нему дерево раскладывается под размер и поворот, писать на него никто не
  будет, поэтому экран размером с дисплей не нужен.
- **Когда звать.** Тогда, когда цикл рисовал бы: после ввода и сообщений и когда `dgx_ui_wake()` говорит, что
  пришёл момент облика. Облик, который просит время, сообщается при каждом вызове, пришёл его момент или нет.
- **Что сообщается.** Виджет, который рисуется целиком, сообщается один раз, его дети — нет. Сообщается и место,
  где закрылось окно и где была полоса клавиатуры. То, что лежит под окном или под полосой, не сообщается, пока не
  покажется.
- **Функция** может запомнить прямоугольник, как здесь, или собрать его сразу на свой экран, в буфер кадра. Если
  вместо функции дать `NULL`, никому ничего не сообщается, а дерево отмечается нарисованным: это для компоновщика,
  который только что собрал его целиком.
- **Клавиатура и пад DGX-DI** кладутся в ту же полосу после дерева, как в рецепте
  [Зеркало экрана](#зеркало-экрана).
- **Чего это стоит.** Около 0,6 КБ у программы, которая вызывает `dgx_ui_changed()`, и 0,5 КБ фона-картинки; у
  той, что не вызывает, — ничего.

## Поворот экрана

```c
static void screen_turn(dgx_screen_t *scr, bool landscape)
{
    if (landscape) dgx_screen_orientation(scr, DgxScreenRightLeft, DgxScreenTopBottom, true);
    else dgx_screen_orientation(scr, DgxScreenLeftRight, DgxScreenTopBottom, false);
}
```

Больше ничего: отрисовка на каждом проходе сравнивает размер и ориентацию экрана, раскладывает дерево заново и
рисует его целиком. Размеры в точках остаются прежними; строка, которой хватало ширины, после поворота может не
поместиться, если только один из её детей не [занимает остаток](#строка-и-столбец-под-любой-экран).

## Проверка интерфейса на компьютере

Дереву, сообщениям и состоянию дисплей не нужен. Тест строит дерево, посылает то, что послали бы задачи, и делает то,
что сделал бы пользователь:

```c
#include <assert.h>

static int  saved;
static void test_save_on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) ++saved;
}

static void interface_test(void)
{
    static char                  buf[16];
    static dgx_ui_textbox_t      box;
    static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
    static const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .text = &box, .take = temp_take};
    static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = test_save_on};
    static dgx_ui_node_t         nodes[4];
    dgx_ui_t                     ui;

    styles_init();
    dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &st_left);
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_add(&ui, 0, &label_w);
    const int button = dgx_ui_add(&ui, 0, &button_w);
    dgx_ui_screen(&ui, 240, 320, 0); /* what the painter would tell it */

    /* a message as the sensor's task sends it */
    dgx_ui_msg_t    msg;
    const climate_t c = {.t10 = 215, .rh = 40};
    dgx_ui_msg_value(&msg, MSG_CLIMATE, &c, sizeof c);
    dgx_ui_take(&ui, &msg);
    dgx_ui_msg_done(&msg);
    assert(strcmp(buf, "21.5 C") == 0);

    /* a tap in the middle of the button */
    const dgx_ui_node_t *b = dgx_ui_node(&ui, button);
    dgx_ui_layout(&ui);
    const int16_t x = (int16_t)(b->x + b->w / 2), y = (int16_t)(b->y + b->h / 2);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = 1000});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = 1100});
    assert(saved == 1);
}
```

Тест двигает элемент управления, как это сделал бы пользователь, вызовом `dgx_ui_user_value(&ui, slider, 7)`:
значение ставится в пределах диапазона, и `on()` получает `DGX_UI_CHANGED`, чего `dgx_ui_set_value()` не делает.
`dgx_ui_tell()` сообщает `on()` любое событие.

`dgx_ui_dump(&ui)` печатает дерево, которое построил тест: по строке на виджет — номер, вид, место и размер,
значение, состояние и начало текста, — а потом сколько узлов занято и выделен ли блок сверх массива программы.

`dgx_ui.c`, `dgx_ui_text.c`, `dgx_ui_rect.c` и `dgx_ui_msg.c` — обычный C и собираются любым компилятором. Заглушки
тех немногих заголовков ESP-IDF, которые они подключают, идут вместе с DGX, в её `host/stubs`. Так собраны тесты
репозитория, `test/host`.
