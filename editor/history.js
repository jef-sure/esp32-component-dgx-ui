// Undo and redo of the builder: snapshots of a file's text, each with what was selected in it, one history a file.
// Every action records the state before it; typing into one field folds into one step until another field is touched,
// or a second passes, so that an undo takes back a word typed, not a letter. A step that changed several files at once,
// a look renamed in its appearance and in the pages that name it, is recorded in each with one link, never folded;
// the store undoes it in all of them together.
'use strict';

// in a function of its own, so that a page that hosts the builder meets no name of it but window.DgxUiHistory (and module.exports
// under node)
(() => {
    const HISTORY_LIMIT = 200;
    const FOLD_MS = 1000;

    class History {
        constructor(limit = HISTORY_LIMIT) {
            this.limit = limit;
            this.past = [];
            this.future = [];
            this.fold = null; // {key, at}: the step typing goes on folding into
        }

        // the state before an action; fold, a key such as "path:key" for typing into one field; now, the clock's ms; link,
        // the step of several files this one is of
        record(before, fold = null, now = Date.now(), link = null) {
            if (link !== null) before = { ...before, link }, fold = null;
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

        // the link of the step an undo or a redo would take back or do again; null, none or a step of this file alone
        get undoLink() {
            return this.past[this.past.length - 1]?.link ?? null;
        }

        get redoLink() {
            return this.future[this.future.length - 1]?.link ?? null;
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
})();
