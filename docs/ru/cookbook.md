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
- [Кнопки платы](#кнопки-платы)
- [Касание с DGX-DI](#касание-с-dgx-di)
- [Поле ввода и клавиатура](#поле-ввода-и-клавиатура)
- [Ввод в полосе над клавиатурой](#ввод-в-полосе-над-клавиатурой)
- [Виджеты которые появляются и исчезают](#виджеты-которые-появляются-и-исчезают)
- [Большие данные: сообщение по ссылке](#большие-данные-сообщение-по-ссылке)
- [Сообщение из прерывания](#сообщение-из-прерывания)
- [Свой облик](#свой-облик)
- [Шкала по кругу с виджетами внутри](#шкала-по-кругу-с-виджетами-внутри)
- [Дисплей на 18 или 24 бита](#дисплей-на-18-или-24-бита)
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
- **Высоту** списка строки делят между собой: здесь 56 пунктов на четыре строки по 14. Список не прокручивается,
  все его строки помещаются в его коробке.
- **Палец.** Когда палец опускается на строку, она выбирается, и `on()` сообщается `DGX_UI_CHANGED`. Если вести
  палец по списку, выбор идёт за ним: выбрана та строка, что под пальцем. Палец, отпущенный вскоре на той же
  строке, на которую опустился, — это нажатие: `DGX_UI_ACTIVATED`. Если он с этой строки ушёл, отпускание уже не
  нажатие. Верхняя и нижняя двенадцатая часть строки не принадлежит ни одной строке, и палец там ничего не
  выбирает.
- **Клавиши.** «Дальше» и «назад» сначала ведут выбор по строкам, с `DGX_UI_CHANGED` на каждой, а с последней
  строки фокус уходит к следующему виджету. Клавиша «активировать» сообщает `DGX_UI_ACTIVATED`.
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
  попадёшь и в строку. Список не прокручивается: на экран помещается столько вариантов, сколько раз высота строки
  укладывается в высоту экрана, а если их больше, они делят высоту экрана между собой.
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

1. Отрисовка рисует надпись. Её облик кладёт кусок карты под свою коробку функцией программы и пишет буквы поверх,
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
- Шрифты — из DGX, матричные и точечные. Шрифт не масштабируется.

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
Здесь — полоса уровня: значение виджета в виде закрашенной части его коробки.

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

- **Где.** Он рисует коробку виджета, `n->x, n->y, n->w, n->h`, за вычетом `paint->ox, paint->oy`: экраном может быть
  буфер, в котором лежит часть картины. Отсечение уже поставлено, и облик не меняет его насовсем.
- **Каждый пиксель один раз.** Пишите каждый пиксель того, что рисуете, один раз, и на дисплее за шиной виджет не
  мигает. Полоса выше — два прямоугольника, которые не перекрываются.
- **Всю коробку, когда `paint->whole`.** Тогда на экране от виджета нет ничего. Облик, который рисует только
  разницу, обязан в этом случае нарисовать всё. Облики библиотеки рисуют всю коробку каждый раз.
- **Цвета** берутся из `dgx_ui_color(paint, index)`. `paint->paper` — номер цвета под виджетом.
- **Вызов по частям.** Когда на части виджета лежит всплывающее окно или клавиатура, облик вызывается по разу для
  каждой видимой части, с отсечением по ней. В каждом вызове он должен рисовать одно и то же. Облик, который помнит,
  что нарисовал, записывает это только в последнем: когда `paint->part == paint->parts - 1`, а `paint->composing`
  ложно. На дисплее в 18 или 24 бита, когда названы полосы, облик вызывается ещё и по разу на каждую полосу части,
  с установленным `paint->whole`, поэтому каждый раз рисует всю свою коробку; `paint->composing` стоит во всех
  полосах, кроме последней, так что проверка остаётся той же.
- **Время.** Облик с анимацией возвращает `true`, и его вызывают снова на следующем проходе. Где он находится, он
  считает по `now_ms`.
- **Его заливка.** `fill` — номер цвета, который облик оставляет под детьми виджета, или -1, если он оставляет то,
  что было. У рамки это её заливка.
- **Его бумага.** Там, где бумага облика — то, что лежит под виджетом, он кладёт её вызовом
  `dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y, w, h)`, а не своей заливкой: это цвет `paint->paper` или, когда
  стоит `paint->on_background`, картинка программы. На картинке облик рисует буквы и картинки без своей бумаги: без
  поля текста, без `DGX_FONT_FILL` и без `DGX_IMAGE_OPAQUE`.

## Шкала по кругу с виджетами внутри

![Станция CO2: цветная шкала по кругу, внутри число и два значения](../images/ring.png)
![То же с раскладкой: коробки виджетов показаны штрихами](../images/ring-layout.png)

Шкала, которая идёт по кругу экрана, с виджетами внутри — это одно дерево: кольцо — виджет собственного вида
программы, а остальные виджеты — его дети, как у рамки. Всплывающее окно для этого не нужно. Штрихи на второй
картинке показывают, что дала раскладка: квадрат, который отступ кольца оставляет внутри круга, и в нём коробки
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
- **Целиком**, при первой отрисовке и после поворота, кольцо заливает свою коробку вокруг детей вызовом
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
- **Чего это стоит на шине.** Изменившийся виджет отправляется целиком, всей коробкой; без буфера писалось бы
  только его поле.
- **Чего это стоит в ОЗУ.** Не меньше строки самого широкого виджета: его ширина в пикселях на 3 байта. Чем больше
  строк, тем меньше передач. Виджет, строка которого в буфер не помещается, рисуется прямо и мигает; об этом один
  раз пишется в журнал.
- **Чего это стоит во флеше.** Около 0,6 КБ в dgx-ui и 5 КБ переносов DGX — для программы, которая назвала
  полосы. Программа, которая их не назвала, ничего из этого не несёт; поэтому их и называют, а не включают сами.
- **16-битный дисплей** рисуется прямо, что бы ни было названо: его текст сначала не заливается.
- Нужен `CONFIG_DGX_ENABLE_VSCREEN=y`; если назвать полосы без него, программа не соберётся.
- Картинка RGB565 и картинка в 4 или 8 бит на таком дисплее — бумага: их обликам нужен 16-битный экран.

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

`dgx_ui.c`, `dgx_ui_text.c`, `dgx_ui_rect.c` и `dgx_ui_msg.c` — обычный C и собираются любым компилятором. Заглушки
тех немногих заголовков ESP-IDF, которые они подключают, идут вместе с DGX, в её `host/stubs`. Так собраны тесты
репозитория, `test/host`.
