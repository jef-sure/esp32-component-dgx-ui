/*
 * dgx_ui_idle.c
 *
 *  The user's idle, one for the program (dgx_ui_idle()): the input calls of every tree tell it of the user through
 *  dgx_ui_idle_hook, home's dgx_ui_time() judges it and tells home's trees by a message of the program's number, 1 when
 *  it comes and 0 when the user wakes it; the input that wakes it spent unless the program says it acts. Its moment
 *  lies in home's repeat_key and repeat_at whenever no repeat holds them, so dgx_ui_wake() gives it as a repeat's. A
 *  program that names none carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_msg.h"
#include "dgx_ui_priv.h"

/* the program's idle; trees of other tasks write its time and its flags, a word at a time */
static dgx_ui_idle_t *named;

/* the idle's moment in home's two members while no repeat holds them, and while it is still to come */
static void moment(dgx_ui_idle_t *idle)
{
    dgx_ui_t *home = idle->home;
    if (home->repeat_key >= 0 && home->repeat_key != DGX_UI_REPEAT_IDLE) return; /* a repeat's: the sooner */
    if (idle->idle || !idle->after_ms) {
        home->repeat_key = -1;
        return;
    }
    home->repeat_key = DGX_UI_REPEAT_IDLE, home->repeat_at = idle->last_ms + idle->after_ms;
}

/* the user did something: at ms (known), else counted at the next step. True: it wakes the idle and is spent */
static bool user(dgx_ui_idle_t *idle, dgx_ui_t *ui, bool timed, uint32_t ms)
{
    if (timed) idle->last_ms = ms, idle->unstamped = false;
    else idle->unstamped = true;
    if (ui == idle->home && timed) moment(idle); /* the moment moved on, in home's task */
    if (!idle->idle) return false;
    idle->idle = false, idle->woken = true;
    return !idle->wake_acts;
}

static void tell(dgx_ui_idle_t *idle, uint8_t byte)
{
    dgx_ui_msg_t msg = {.what = idle->what, .count = 1};
    msg.bytes[0]     = byte;
    dgx_ui_take(idle->home, &msg);
    dgx_ui_msg_done(&msg);
}

/* home's step: what came without time counted now; a wake told 0, the idle come told 1; its moment put again */
static void step(dgx_ui_idle_t *idle, uint32_t now_ms)
{
    if (idle->unstamped) idle->last_ms = now_ms, idle->unstamped = false;
    if (idle->finger_down || idle->buttons_held) idle->last_ms = now_ms; /* the user there, as the last report has it */
    if (idle->woken) {
        idle->woken = false;
        if (idle->told) idle->told = false, tell(idle, 0);
    }
    if (named != idle) return; /* a take() named another, or none */
    if (!idle->idle && idle->after_ms && (int32_t)(now_ms - idle->last_ms) >= (int32_t)idle->after_ms) {
        idle->idle = idle->told = true;
        tell(idle, 1);
        if (named != idle) return;
    }
    moment(idle);
}

/* the finger: a gesture spent from the press that woke it to its release, to a new press, or to a report of no
   finger; else the user's if a finger is on the screen, went down or went up. On the screen or not as the last report
   has it, so that a source that reports changes alone keeps the idle away under a finger held */
static bool finger(dgx_ui_idle_t *idle, dgx_ui_t *ui, const dgx_ui_pointer_t *p)
{
    const bool on       = p->down || p->pressed || p->released;
    idle->finger_down   = (p->down || p->pressed) && !p->released;
    if (idle->spending) {
        idle->spending = false;
        if (!on) return false; /* no finger: the gesture ended unreported; not the user */
        if (!p->pressed) {
            user(idle, ui, true, p->ms);
            idle->spending = !p->released; /* spent up to its release */
            return true;
        }
        /* a new press: the release of the one before never came; this one is the user's as any */
    }
    if (!on) return false; /* no finger: not the user */
    if (!user(idle, ui, true, p->ms)) return false;
    idle->spending = !p->released;
    return true;
}

static bool on_input(dgx_ui_t *ui, int input, uint32_t ms, const dgx_ui_pointer_t *p, uint32_t *held)
{
    dgx_ui_idle_t *idle = named;
    if (!idle) return false;
    switch (input) {
    case DGX_UI_INPUT_REPEAT_ENDED:
        if (ui == idle->home) moment(idle);
        return false;
    case DGX_UI_INPUT_FINGER: return finger(idle, ui, p);
    case DGX_UI_INPUT_BUTTONS: { /* true in the call that spends them */
        bool       spent    = false;
        const bool was_held = idle->buttons_held; /* by the last report of any tree, the spent ones too */
        idle->buttons_held  = *held != 0;
        idle->spent_buttons &= (uint8_t)*held; /* let go: no longer spent */
        if ((*held || was_held) && user(idle, ui, true, ms)) idle->spent_buttons = (uint8_t)*held, spent = *held != 0; /* held, or let go now */
        *held &= ~(uint32_t)idle->spent_buttons;
        return spent;
    }
    default: /* a key, code points: no time of their own */
        return user(idle, ui, false, 0);
    }
}

/* home's dgx_ui_time() */
static void on_step(dgx_ui_t *ui, uint32_t now_ms)
{
    if (named && ui == named->home) step(named, now_ms);
}

void dgx_ui_idle(dgx_ui_t *home, dgx_ui_idle_t *idle, uint32_t now_ms)
{
    dgx_ui_idle_t *was = named;
    if (was && was->home && was->home->repeat_key == DGX_UI_REPEAT_IDLE) was->home->repeat_key = -1;
    if (was && was != idle) was->idle = false; /* dropped: no longer idle to whoever reads it */
    named = idle;
    if (!idle) return;
    const bool told = was == idle && idle->told; /* the same named again while told: 0 at the next step */
    idle->home = home, idle->last_ms = now_ms, idle->idle = false, idle->unstamped = false;
    idle->spending = false, idle->spent_buttons = 0, idle->told = told, idle->woken = told;
    idle->finger_down = idle->buttons_held = false;
    dgx_ui_idle_hook = on_input, dgx_ui_idle_step = on_step;
    moment(idle);
}

uint32_t dgx_ui_idle_for(const dgx_ui_idle_t *idle, uint32_t now_ms)
{
    return idle->unstamped ? 0 : now_ms - idle->last_ms;
}
