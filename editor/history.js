// Undo and redo of the builder: snapshots of a description's text, each with the path selected in it. Every action
// records the state before it; typing into one property of one widget folds into one step until another key or
// widget is touched, or a second passes, so that an undo takes back a word typed, not a letter.
'use strict';

const HISTORY_LIMIT = 200;
const FOLD_MS = 1000;

class History {
    constructor(limit = HISTORY_LIMIT) {
        this.limit = limit;
        this.past = [];
        this.future = [];
        this.fold = null; // {key, at}: the step typing goes on folding into
    }

    // the state before an action; fold, a key such as "path:key" for typing into one field; now, the clock's ms
    record(before, fold = null, now = Date.now()) {
        if (fold !== null && this.fold && this.fold.key === fold && now - this.fold.at < FOLD_MS) {
            this.fold.at = now;
            return;
        }
        this.past.push(before);
        if (this.past.length > this.limit) this.past.shift();
        this.future = [];
        this.fold = fold === null ? null : { key: fold, at: now };
    }

    // the state to go back to, given the one now; null with nothing to undo
    undo(now) {
        if (!this.past.length) return null;
        this.future.push(now);
        this.fold = null;
        return this.past.pop();
    }

    redo(now) {
        if (!this.future.length) return null;
        this.past.push(now);
        this.fold = null;
        return this.future.pop();
    }

    get canUndo() {
        return this.past.length > 0;
    }

    get canRedo() {
        return this.future.length > 0;
    }

    clear() {
        this.past = [], this.future = [], this.fold = null;
    }
}

if (typeof module !== 'undefined') module.exports = { History, HISTORY_LIMIT, FOLD_MS };
else window.DgxUiHistory = { History, HISTORY_LIMIT, FOLD_MS };
