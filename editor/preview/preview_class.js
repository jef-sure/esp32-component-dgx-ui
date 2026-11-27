/*
 * DgxUiPreview: the calls of dgx_ui_preview.h on the module DGX_UI_PREVIEW_MODULE, with the strings, numbers and
 * objects of JavaScript. preview_build.py puts the module's bytes before this text and both into dgx_ui_preview.js,
 * which a page loads by a <script> from a file, with no server, and Node by require():
 *
 *     const preview = await DgxUiPreview.create({pageText: name => pages[name]});   // the pages the action page opens
 *     preview.screen(320, 240, 16, 1);                                             // the board's panel and unit
 *     preview.names(namesFile);                                                    // null: by the rule
 *     await preview.fonts(index, file => readFile(file));  // dgx_fonts-files.txt of DGX and its files
 *     preview.appearance(appearance, itsNames);          // its own names file; null by the rule; none, the page's
 *     const verdict = preview.page(description);          // {code, offset, name, said}: code 0, taken
 *     if (preview.paint(now)) context.putImageData(preview.image(), 0, 0);
 *
 * A fault inside the module stops it: the call throws an Error whose message begins "the preview stopped", and every
 * call after it throws the same; a new preview is made by create() again. A pageText that throws gives the module no
 * text, as one that gives null, and its error goes to the log.
 */
const DGX_UI_WAKE_NONE = 0xffffffff; // the module's wake of none, as dgx_ui_paint.h names it

class DgxUiPreview {
    /** A preview of its own: the module compiled and made ready. */
    static async create(options = {}) {
        const preview      = new DgxUiPreview(options);
        const {instance}   = await WebAssembly.instantiate(DgxUiPreview.bytesOf(DGX_UI_PREVIEW_MODULE), preview.imports());
        preview.exports    = instance.exports;
        preview.memory     = instance.exports.memory;
        preview.call(() => preview.exports._initialize());
        return preview;
    }

    /* base64 to bytes, in a page and in Node */
    static bytesOf(text) {
        if (typeof atob === 'function') {
            const chars = atob(text), bytes = new Uint8Array(chars.length);
            for (let k = 0; k < chars.length; ++k) bytes[k] = chars.charCodeAt(k);
            return bytes;
        }
        return new Uint8Array(Buffer.from(text, 'base64'));
    }

    constructor(options) {
        this.pageText = options.pageText || (() => null);
        this.log      = options.log || (line => console.log(line));
        this.encoder  = new TextEncoder();
        this.decoder  = new TextDecoder();
        this.logDecoder = new TextDecoder(); /* the log's, a character split between two writes joined again */
        this.width    = 0;
        this.height   = 0;
        this.logged   = '';
        this.stopped  = null;
    }

    /* what the module imports: the three calls of WASI its C library makes for the log, and the pages' texts */
    imports() {
        const self = this;
        const wasi = {
            fd_write(fd, vectors, count, written) {
                const view = new DataView(self.memory.buffer);
                let total  = 0;
                for (let k = 0; k < count; ++k) {
                    const at = view.getUint32(vectors + 8 * k, true), length = view.getUint32(vectors + 8 * k + 4, true);
                    self.logged += self.logDecoder.decode(new Uint8Array(self.memory.buffer, at, length), {stream: true});
                    total += length;
                }
                const lines = self.logged.split('\n');
                self.logged = lines.pop();
                for (const line of lines) self.log(line);
                view.setUint32(written, total, true);
                return 0;
            },
            fd_close: () => 0,
            fd_seek: () => 70, /* ESPIPE: no file to seek in */
        };
        return {wasi_snapshot_preview1: wasi, env: {preview_page_text: (name, out, size) => self.pageTextOf(name, out, size)}};
    }

    pageTextOf(name, out, size) {
        const named = this.cString(name, 24);
        let text;
        try {
            text = this.pageText(named);
        } catch (error) {
            this.log(`the text of the page "${named}": ${error && error.message ? error.message : error}`);
            return 0;
        }
        if (text === null || text === undefined) return 0;
        const bytes = this.encoder.encode(text);
        if (out && size >= bytes.length) new Uint8Array(this.memory.buffer).set(bytes, out);
        return bytes.length;
    }

    /* a call of the module: a fault in it stops the preview for good */
    call(run) {
        if (this.stopped) throw new Error('the preview stopped: ' + this.stopped);
        try {
            return run();
        } catch (error) {
            this.stopped = error.message;
            throw new Error(error.message.startsWith('the preview stopped') ? error.message : 'the preview stopped: ' + error.message);
        }
    }

    cString(at, most = Infinity) {
        if (!at) return null;
        const bytes = new Uint8Array(this.memory.buffer);
        let end     = at;
        while (end - at < most && bytes[end]) ++end;
        return this.decoder.decode(bytes.subarray(at, end));
    }

    /* a text into the module's memory, ended by a 0, for the call */
    withText(text, run) {
        if (text === null || text === undefined) return run(0, 0);
        return this.withBytes(this.encoder.encode(text), run);
    }

    withBytes(bytes, run) {
        const at   = this.call(() => this.exports.preview_text(bytes.length + 1));
        const view = new Uint8Array(this.memory.buffer);
        view.set(bytes, at), view[at + bytes.length] = 0;
        return run(at, bytes.length);
    }

    verdictAt(at) {
        const view = new DataView(this.memory.buffer);
        return {code: view.getInt32(at, true), offset: view.getInt32(at + 4, true), name: this.cString(at + 8, 24), said: this.cString(at + 32, 512)};
    }

    /** What it was built of: {dgx_ui, dgx, toolchain, sources}. */
    version() {
        return JSON.parse(this.cString(this.call(() => this.exports.preview_version())));
    }

    /**
     * The format of a description as the loader reads it, from its own tables: {version, kinds, keys: [{key, type, kinds,
     * and words, min and max, table, null where they are said}]}, as preview_format() says.
     */
    format() {
        return JSON.parse(this.cString(this.call(() => this.exports.preview_format())));
    }

    /** The names of the tables the last page given was read on: {look: [...], value: [...], ...}, as preview_tables() says. */
    tables() {
        return JSON.parse(this.cString(this.call(() => this.exports.preview_tables())));
    }

    /**
     * What the appearance given may say, for its forms, as preview_appearance_format() says: {draws: [{draw, own, kinds,
     * fields: [{name, type, and min, max, words where they are said}], defaults where the draw is the library's}], fonts,
     * hands, by_rule, style: [fields], variant: [fields]}.
     */
    appearanceFormat() {
        return JSON.parse(this.cString(this.call(() => this.exports.preview_appearance_format())));
    }

    /** What a names file may say, as preview_names_format() says: {tables, keys, appearance, words, pictures, bare}. */
    namesFormat() {
        return JSON.parse(this.cString(this.call(() => this.exports.preview_names_format())));
    }

    /**
     * The names the page given uses that a names file, its text given, has not, each with the C the rule gives it:
     * [{table, name, c, bare, and number, page, count}], as preview_names_missing() says.
     */
    namesMissing(text) {
        return JSON.parse(this.cString(this.withText(text ?? '', (at, length) => this.call(() => this.exports.preview_names_missing(at, length)))));
    }

    /** DGX's colours by their names, as its dgx_colors.h has them: [['aliceblue', '#f0f8ff'], ...]. */
    static colorNames() {
        return DGX_UI_COLOR_NAMES;
    }

    /**
     * Whether a text, or bytes, is JSON as the loader's reader of JSON reads it: 1 JSON, 0 not, 2 deeper than the reader
     * goes. Bytes are given as they are, ill-formed UTF-8 too; a text is given as UTF-8.
     */
    json(textOrBytes) {
        const bytes = typeof textOrBytes === 'string' ? this.encoder.encode(textOrBytes) : DgxUiPreview.bytesView(textOrBytes);
        if (!bytes) throw new TypeError('json(): a text or bytes');
        return this.withBytes(bytes, (at, length) => this.call(() => this.exports.preview_json(at, length)));
    }

    /**
     * The board's panel: width, height, depth 16, 18 or 24, and the pixels a point of its program, each an integer as the
     * module takes it; 0, or -2 refused, which leaves no panel.
     */
    screen(width, height, depth, pixelsAPoint = 1) {
        const across = width | 0, down = height | 0;
        const said   = this.call(() => this.exports.preview_screen(across, down, depth | 0, pixelsAPoint | 0));
        this.width = said === 0 ? across : 0, this.height = said === 0 ? down : 0;
        return said;
    }

    /** The names file, or null by the rule: {code, offset, name, said}, code -1 refused in said's words. */
    names(text) {
        return this.verdictAt(this.withText(text, (at, length) => this.call(() => this.exports.preview_names(at, length))));
    }

    /**
     * The file of a font, a .dgxf of DGX, for the name of C the names or the rule take it by (preview_font()); null
     * lets that name's file go. {code, offset, name, said}: code 0, or -3 refused in said's words, a name holding a NUL
     * among them, refused here, before the module would end it there.
     */
    font(cName, bytes) {
        if (String(cName).includes('\u0000')) return {code: -3, offset: 0, name: '', said: 'a name of C holding a NUL'};
        const name = this.encoder.encode(String(cName));
        const data = bytes === null || bytes === undefined ? null : DgxUiPreview.bytesView(bytes);
        if (bytes !== null && bytes !== undefined && !data) return {code: -3, offset: 0, name: '', said: `${cName}: no bytes`};
        const at   = this.call(() => this.exports.preview_text(name.length + 1 + (data ? data.length : 0)));
        const view = new Uint8Array(this.memory.buffer);
        view.set(name, at), view[at + name.length] = 0;
        if (data) view.set(data, at + name.length + 1);
        return this.verdictAt(this.call(() => this.exports.preview_font(at, data ? at + name.length + 1 : 0, data ? data.length : 0)));
    }

    /**
     * The file of pictures of DGX, a .dgxi as the build makes it, for the asset's name a names file's line
     * `picture <name> @<asset>` takes (preview_picture()); null lets that name's file go. {code, offset, name, said}:
     * code 0, or -4 refused in said's words.
     */
    picture(asset, bytes) {
        if (String(asset).includes('\u0000')) return {code: -4, offset: 0, name: '', said: 'an asset\'s name holding a NUL'};
        const name = this.encoder.encode(String(asset));
        const data = bytes === null || bytes === undefined ? null : DgxUiPreview.bytesView(bytes);
        if (bytes !== null && bytes !== undefined && !data) return {code: -4, offset: 0, name: '', said: `${asset}: no bytes`};
        const at   = this.call(() => this.exports.preview_text(name.length + 1 + (data ? data.length : 0)));
        const view = new Uint8Array(this.memory.buffer);
        view.set(name, at), view[at + name.length] = 0;
        if (data) view.set(data, at + name.length + 1);
        return this.verdictAt(this.call(() => this.exports.preview_picture(at, data ? at + name.length + 1 : 0, data ? data.length : 0)));
    }

    /**
     * Every file of pictures of an index DGX's build writes, dgx_images-files.txt: a line `<asset> <file>`, the file of
     * the index's directory, read by read(file) as fonts() reads its files. A Promise of [{name, verdict}], a line each.
     */
    async pictures(index, read) {
        return this.filesOf(index, read, (name, data) => this.picture(name, data), -4);
    }

    /* bytes as a Uint8Array: of one, of an ArrayBuffer or of a view; null for anything else */
    static bytesView(bytes) {
        if (bytes instanceof Uint8Array) return bytes;
        if (bytes instanceof ArrayBuffer) return new Uint8Array(bytes);
        if (ArrayBuffer.isView(bytes)) return new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        return null;
    }

    /**
     * Every font of an index DGX writes beside a project's fonts, dgx_fonts-files.txt: a line `<name of C> <file>`, the
     * file of the index's directory. read(file) gives its bytes, or a Promise of them, as File System Access and fetch
     * do. A Promise of [{name, verdict}], a line each; refused in the result, not thrown: a line not of two words, a name
     * given twice, a file that is absolute or climbs out by "..", a read that fails or gives no bytes.
     */
    async fonts(index, read) {
        return this.filesOf(index, read, (name, data) => this.font(name, data), -3);
    }

    /* the files of an index of DGX's, given one by one by give(name, bytes); a refusal of a line its verdict of code */
    async filesOf(index, read, give, code) {
        const results = [], seen = new Set();
        const refuse  = (name, said) => results.push({name, verdict: {code, offset: 0, name: '', said}});
        for (const line of String(index).split('\n').map(text => text.trim()).filter(text => text)) {
            const words = line.split(/\s+/);
            if (words.length !== 2) {
                refuse(words[0], `not a line of a name and a file: "${line}"`);
                continue;
            }
            const [name, file] = words;
            if (seen.has(name)) {
                refuse(name, `${name}: given twice in the index`);
                continue;
            }
            seen.add(name);
            if (/^([\/\\]|[A-Za-z]:)/.test(file) || file.split(/[\/\\]/).includes('..')) {
                refuse(name, `${name}: the file "${file}" is not of the index's directory`);
                continue;
            }
            let bytes;
            try {
                bytes = await read(file);
            } catch (error) {
                refuse(name, `${name}: ${file}: ${error && error.message ? error.message : error}`);
                continue;
            }
            const data = bytes === null || bytes === undefined ? null : DgxUiPreview.bytesView(bytes);
            if (!data) {
                refuse(name, `${name}: ${file}: no bytes`);
                continue;
            }
            results.push({name, verdict: give(name, data)});
        }
        return results;
    }

    /**
     * The appearance, or null: the loader's verdict. Its draws, fonts and hands are read on the page's names file when
     * names is not given, as at the start; on a names file of its own when names is its text, as dgx_ui_c -a reads an
     * appearance made by dgx_ui_appearance() with NAMES; by the rule when names is null, as one made with none
     * (preview_appearance_on()). A names file of its own refused: that verdict, code -1 in words, the appearance read by
     * the rule.
     */
    appearance(text, names = undefined) {
        const how = names === undefined ? 0 : names === null ? 2 : 1;
        const own = this.verdictAt(this.withText(how === 1 ? names : null,
                                                 (at, length) => this.call(() => this.exports.preview_appearance_on(how, at, length))));
        const verdict = this.verdictAt(this.withText(text, (at, length) => this.call(() => this.exports.preview_appearance(at, length))));
        return own.code ? own : verdict;
    }

    /** The page: the loader's verdict, {code, offset, name}; drawn whole at the next paint. */
    page(text) {
        return this.verdictAt(this.withText(text, (at, length) => this.call(() => this.exports.preview_page(at, length))));
    }

    pointer(x, y, down, nowMs) {
        this.call(() => this.exports.preview_pointer(x | 0, y | 0, down ? 1 : 0, nowMs >>> 0));
    }

    key(key) {
        this.call(() => this.exports.preview_key(key | 0));
    }

    buttons(held, nowMs) {
        this.call(() => this.exports.preview_buttons(held >>> 0, nowMs >>> 0));
    }

    /** A value written; false: the names have none of that name. */
    value(name, value) {
        return this.withText(name, at => this.call(() => this.exports.preview_value(at, value | 0))) === 0;
    }

    /** The time given and the page drawn where it changed: true when pixels were written. */
    paint(nowMs) {
        return this.call(() => this.exports.preview_paint(nowMs >>> 0)) !== 0;
    }

    /** The milliseconds after nowMs a widget asks to be drawn again; Infinity, none. */
    wake(nowMs) {
        const ms = this.call(() => this.exports.preview_wake(nowMs >>> 0)) >>> 0;
        return ms === DGX_UI_WAKE_NONE ? Infinity : ms;
    }

    /** The panel's pixels, red, green, blue, 255, row after row: a view of the module's memory, read before the next call. */
    frame() {
        const at = this.call(() => this.exports.preview_frame());
        return at && this.width ? new Uint8ClampedArray(this.memory.buffer, at, this.width * this.height * 4) : null;
    }

    /** The frame as an ImageData of a page. */
    image() {
        const frame = this.frame();
        return frame ? new ImageData(new Uint8ClampedArray(frame), this.width, this.height) : null;
    }

    /** The rectangle written since the last call: {x, y, w, h}, w 0 none. */
    changed() {
        const at = this.call(() => this.exports.preview_changed()), view = new DataView(this.memory.buffer);
        return {x: view.getInt32(at, true), y: view.getInt32(at + 4, true), w: view.getInt32(at + 8, true), h: view.getInt32(at + 12, true)};
    }

    /** The page on the screen: '' the page given, else the name of the page an action opened over it; null, no page. */
    shown() {
        return this.cString(this.call(() => this.exports.preview_shown()));
    }

    /** The widgets of the page on the screen. */
    count() {
        return this.call(() => this.exports.preview_count());
    }

    /** A widget of the page on the screen, as its tree has it after the layout, or null: {kind, parent, x, y, w, h, shown,
        bits, value, text}. */
    widget(i) {
        const at = this.call(() => this.exports.preview_widget(i | 0));
        if (!at) return null;
        const view = new DataView(this.memory.buffer), field = k => view.getInt32(at + 4 * k, true);
        return {kind: field(0), parent: field(1), x: field(2), y: field(3), w: field(4), h: field(5), shown: field(6) !== 0,
                bits: view.getUint32(at + 28, true), value: field(8), text: this.cString(view.getUint32(at + 36, true))};
    }

    /** The widget of the page on the screen a finger at x, y would take, -1 none. */
    widgetAt(x, y) {
        return this.call(() => this.exports.preview_widget_at(x | 0, y | 0));
    }

    /** What the handlers were told since the last call, a line each. */
    events() {
        return this.cString(this.call(() => this.exports.preview_events())).split('\n').filter(line => line);
    }

    /** The stand-ins the page takes, a line each. */
    notes() {
        return this.cString(this.call(() => this.exports.preview_notes())).split('\n').filter(line => line);
    }
}

globalThis.DgxUiPreview = DgxUiPreview;
if (typeof module === 'object' && module && module.exports) module.exports = DgxUiPreview;
