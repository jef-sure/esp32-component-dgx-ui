// The description of a page as the builder holds it. Read from its bytes by a reader of JSON of its own, which keeps
// every object's entries in their order, a key twice too, and the text of every number and string as written; the
// widgets in pre-order, the order dgx_ui_load adds them, each by its path, the indices of its children from the root;
// the operations of the builder; and the writer, which gives the keys of a widget in one order and the span of bytes
// of each widget in what it wrote. It judges nothing of the format: the loader of the preview does. The reader takes
// what the loader's reader of JSON takes (src/dgx_ui_json.c) and refuses what it refuses.
//
// Positions: the reader's `at` counts the UTF-16 units of the text decoded, as JavaScript and a text field count; the
// loader's offset, and `byte` here, count the bytes of its UTF-8. byteOfUnit() and unitOfByte() turn one into the other.
'use strict';

const TOP_ORDER = ['dgx-ui', 'name', 'editor', 'tree'];
const WIDGET_ORDER = ['kind', 'id', 'flags', 'state', 'look', 'style', 'text', 'text_size', 'lines', 'value', 'picture', 'min', 'max', 'step', 'tag',
                      'pad', 'gap', 'at', 'want', 'on_event', 'take', 'ctx', 'editor', 'children'];
const ONE_LINE = 100; // a widget with no children whose line is at most so long stays on one line
const BUILDER = 'dgx-ui-builder'; // the builder's key in "editor"

// ---- reading ----

// a node: {t: 'object', entries: [[key, value], ...]} with key a string's node, {t: 'array', items}, {t: 'string', value,
// raw}, {t: 'number', raw}, {t: 'true'}, {t: 'false'}, {t: 'null'}; raw is the text as written, absent once edited
const WHITE = new Set([' ', '\t', '\n', '\r']);

function readText(text) {
    let at = 0;
    const fail = (why) => ({ ok: false, at, why });
    const white = () => {
        while (at < text.length && WHITE.has(text[at])) ++at;
    };
    const string = () => {
        const from = at++;
        let value = '';
        for (;;) {
            if (at >= text.length) return null;
            const c = text[at++];
            if (c === '"') break;
            if (c < ' ') return null;
            if (c !== '\\') {
                value += c;
                continue;
            }
            const e = text[at++];
            const simple = { '"': '"', '\\': '\\', '/': '/', b: '\b', f: '\f', n: '\n', r: '\r', t: '\t' }[e];
            if (simple !== undefined) {
                value += simple;
                continue;
            }
            if (e !== 'u') return null;
            const hex = (k) => (/^[0-9a-fA-F]{4}$/.test(text.slice(k, k + 4)) ? parseInt(text.slice(k, k + 4), 16) : -1);
            let code = hex(at);
            if (code < 0 || (code >= 0xdc00 && code <= 0xdfff)) return null;
            at += 4;
            if (code >= 0xd800 && code <= 0xdbff) {
                if (text[at] !== '\\' || text[at + 1] !== 'u') return null;
                const low = hex(at + 2);
                if (low < 0xdc00 || low > 0xdfff) return null;
                at += 6;
                code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
            }
            if (code === 0) return null; // the loader's strings are C's: no NUL within one
            value += String.fromCodePoint(code);
        }
        return { t: 'string', value, raw: text.slice(from, at) };
    };
    // a number by the grammar the loader reads it by: a sign, digits with no leading 0 but a lone one, a fraction and an
    // exponent each with digits; the rest judged as what follows it
    const number = () => {
        const from = at;
        const digits = () => {
            const first = at;
            while (at < text.length && text[at] >= '0' && text[at] <= '9') ++at;
            return at - first;
        };
        if (text[at] === '-') ++at;
        const lead = at, whole = digits();
        if (!whole || (whole > 1 && text[lead] === '0')) return null;
        if (text[at] === '.') {
            ++at;
            if (!digits()) return null;
        }
        if (text[at] === 'e' || text[at] === 'E') {
            ++at;
            if (text[at] === '+' || text[at] === '-') ++at;
            if (!digits()) return null;
        }
        return { t: 'number', raw: text.slice(from, at) };
    };
    const word = (w, node) => {
        if (text.slice(at, at + w.length) !== w) return null;
        at += w.length;
        return node;
    };
    const scalar = () => {
        const c = text[at];
        if (c === '"') return string();
        if (c === '-' || (c >= '0' && c <= '9')) return number();
        if (c === 't') return word('true', { t: 'true' });
        if (c === 'f') return word('false', { t: 'false' });
        if (c === 'n') return word('null', { t: 'null' });
        return null;
    };
    // the value, iteratively: any depth is read, the loader's bound of depth being its own verdict
    white();
    if (text[at] !== '{') return fail('not an object');
    const stack = [];
    let value = null;
    for (;;) {
        white();
        const c = text[at];
        if (c === '{' || c === '[') {
            ++at;
            white();
            const node = c === '{' ? { t: 'object', entries: [] } : { t: 'array', items: [] };
            if (text[at] === (c === '{' ? '}' : ']')) {
                ++at;
                value = node;
            } else {
                if (c === '{') {
                    if (text[at] !== '"') return fail('a key that is no string');
                    const key = string();
                    if (!key) return fail('a string not as JSON writes one');
                    white();
                    if (text[at++] !== ':') return fail('no colon');
                    stack.push({ node, key });
                } else {
                    stack.push({ node });
                }
                continue;
            }
        } else {
            value = scalar();
            if (!value) return fail('not a value of JSON');
        }
        // a value read: into the brackets open, then the next member or their end
        for (;;) {
            if (!stack.length) {
                white();
                return at === text.length ? { ok: true, doc: value } : fail('more after the top');
            }
            const top = stack[stack.length - 1];
            if (top.node.t === 'object') top.node.entries.push([top.key, value]);
            else top.node.items.push(value);
            white();
            const close = top.node.t === 'object' ? '}' : ']';
            if (text[at] === ',') {
                ++at;
                if (top.node.t === 'object') {
                    white();
                    if (text[at] !== '"') return fail('a key that is no string');
                    const key = string();
                    if (!key) return fail('a string not as JSON writes one');
                    white();
                    if (text[at++] !== ':') return fail('no colon');
                    top.key = key;
                }
                break;
            }
            if (text[at] !== close) return fail('no comma nor end');
            ++at;
            stack.pop();
            value = top.node;
        }
    }
}

// ---- positions ----

// the bytes of UTF-8 before a position counted in UTF-16 units; a surrogate with no pair is three, as TextEncoder writes it
function byteOfUnit(text, unit) {
    let bytes = 0;
    for (let k = 0; k < unit && k < text.length; ++k) {
        const code = text.charCodeAt(k);
        if (code < 0x80) bytes += 1;
        else if (code < 0x800) bytes += 2;
        else if (code >= 0xd800 && code <= 0xdbff && k + 1 < unit && (text.charCodeAt(k + 1) & 0xfc00) === 0xdc00) bytes += 4, ++k;
        else bytes += 3;
    }
    return bytes;
}

// the UTF-16 units before a byte of the text's UTF-8; a byte within a character counts as that character's start
function unitOfByte(text, byte) {
    let bytes = 0, k = 0;
    while (k < text.length) {
        const code = text.charCodeAt(k);
        const pair = code >= 0xd800 && code <= 0xdbff && (text.charCodeAt(k + 1) & 0xfc00) === 0xdc00;
        const size = code < 0x80 ? 1 : code < 0x800 ? 2 : pair ? 4 : 3;
        if (bytes + size > byte) break;
        bytes += size, k += pair ? 2 : 1;
    }
    return k;
}

// the first byte of the first sequence that is not well-formed UTF-8, as Unicode's table of well-formed sequences
// and TextDecoder's fatal decoding judge it: a byte no sequence starts with, a sequence cut, overlong, of a surrogate
// or past U+10FFFF; -1 when there is none
function illFormedAt(bytes) {
    for (let k = 0; k < bytes.length;) {
        const lead = bytes[k];
        if (lead < 0x80) {
            ++k;
            continue;
        }
        let size, low = 0x80, high = 0xbf;
        if (lead >= 0xc2 && lead <= 0xdf) size = 2;
        else if (lead >= 0xe0 && lead <= 0xef) size = 3, lead === 0xe0 ? (low = 0xa0) : lead === 0xed ? (high = 0x9f) : 0;
        else if (lead >= 0xf0 && lead <= 0xf4) size = 4, lead === 0xf0 ? (low = 0x90) : lead === 0xf4 ? (high = 0x8f) : 0;
        else return k;
        if (k + size > bytes.length) return k;
        if (bytes[k + 1] < low || bytes[k + 1] > high) return k;
        for (let c = 2; c < size; ++c) if (bytes[k + c] < 0x80 || bytes[k + c] > 0xbf) return k;
        k += size;
    }
    return -1;
}

// the bytes of a file read: refused when not well-formed UTF-8, as the loader refuses them, at the first byte that is
// not, and the units of the text before it; a byte order mark kept, and refused as the loader refuses it. A text given
// is read as it is. {ok: true, doc, text} or {ok: false, at, byte, why}
function read(bytes) {
    let text;
    if (typeof bytes === 'string') {
        text = bytes;
    } else {
        try {
            text = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true }).decode(bytes);
        } catch {
            const byte = Math.max(0, illFormedAt(bytes));
            return { ok: false, at: new TextDecoder('utf-8', { ignoreBOM: true }).decode(bytes.subarray(0, byte)).length, byte, why: 'not UTF-8' };
        }
    }
    const said = readText(text);
    if (said.ok) said.text = text;
    else said.byte = byteOfUnit(text, said.at);
    return said;
}

// ---- the document ----

const entryOf = (object, key) => (object && object.t === 'object' ? object.entries.find(([k]) => k.value === key) : undefined);
const valueOf = (object, key) => entryOf(object, key)?.[1];
const stringNode = (value) => ({ t: 'string', value });
// a value of JavaScript as a node: what the builder sets. A number not finite, NaN or an infinity, is refused by a
// throw, never written as null; an integer past 2 ** 53 is written as JavaScript holds it, rounded, which no field of
// the format reaches, the largest being ctx's 32 bits
const plain = (value) => {
    if (value === null) return { t: 'null' };
    if (value === true) return { t: 'true' };
    if (value === false) return { t: 'false' };
    if (typeof value === 'number') {
        if (!Number.isFinite(value)) throw new RangeError(`a number that is not finite, ${value}, is no value of JSON`);
        return { t: 'number', raw: JSON.stringify(value) };
    }
    if (typeof value === 'string') return stringNode(value);
    if (Array.isArray(value)) return { t: 'array', items: value.map(plain) };
    if (value && value.t) return value; // a node already
    return { t: 'object', entries: Object.entries(value).map(([k, v]) => [stringNode(k), plain(v)]) };
};
// a node as a value of JavaScript, to show
const shown = (node) => {
    if (!node) return undefined;
    switch (node.t) {
    case 'object': return Object.fromEntries(node.entries.map(([k, v]) => [k.value, shown(v)]));
    case 'array': return node.items.map(shown);
    case 'string': return node.value;
    case 'number': return Number(node.raw);
    case 'true': return true;
    case 'false': return false;
    default: return null;
    }
};

const treeOf = (doc) => valueOf(doc, 'tree');
const childrenOf = (widget) => {
    const children = valueOf(widget, 'children');
    return children && children.t === 'array' ? children : null;
};

// the widget at a path, or undefined
function widgetAt(doc, path) {
    let widget = treeOf(doc);
    for (const k of path) {
        const children = childrenOf(widget);
        widget = children ? children.items[k] : undefined;
        if (!widget || widget.t !== 'object') return undefined;
    }
    return widget && widget.t === 'object' ? widget : undefined;
}

// the widgets in pre-order, as the loader adds them: [{path, widget}]
function widgets(doc) {
    const out = [];
    const root = treeOf(doc);
    if (!root || root.t !== 'object') return out;
    const walk = [{ widget: root, path: [] }];
    while (walk.length) {
        const { widget, path } = walk.pop();
        out.push({ path, widget });
        const children = childrenOf(widget);
        if (!children) continue;
        for (let k = children.items.length - 1; k >= 0; --k)
            if (children.items[k].t === 'object') walk.push({ widget: children.items[k], path: [...path, k] });
    }
    return out;
}

const samePath = (a, b) => a && b && a.length === b.length && a.every((k, i) => k === b[i]);
const indexOf = (doc, path) => widgets(doc).findIndex((one) => samePath(one.path, path));
const pathOf = (doc, index) => widgets(doc)[index]?.path ?? null;

// ---- the operations: each changes the document in place and gives the path to select after it ----

function setKey(doc, path, key, value) {
    const widget = widgetAt(doc, path);
    if (!widget) return null;
    const entry = entryOf(widget, key);
    if (entry) entry[1] = plain(value);
    else widget.entries.push([stringNode(key), plain(value)]);
    return path;
}

function removeKey(doc, path, key) {
    const widget = widgetAt(doc, path);
    if (!widget) return null;
    const k = widget.entries.findIndex(([name]) => name.value === key);
    if (k >= 0) widget.entries.splice(k, 1);
    return path;
}

// a child of a kind, at an index among the children or last; extra: its other keys, such as a look the kind's word has not
function addChild(doc, path, kind, extra = {}, at = undefined) {
    const widget = widgetAt(doc, path);
    if (!widget) return null;
    let children = childrenOf(widget);
    if (!children) {
        children = { t: 'array', items: [] };
        const entry = entryOf(widget, 'children');
        if (entry) entry[1] = children;
        else widget.entries.push([stringNode('children'), children]);
    }
    const index = at === undefined ? children.items.length : Math.max(0, Math.min(at, children.items.length));
    children.items.splice(index, 0, plain({ kind, ...extra }));
    return [...path, index];
}

function remove(doc, path) {
    if (!path.length) return null; // the root stays
    const parent = widgetAt(doc, path.slice(0, -1)), children = childrenOf(parent), k = path[path.length - 1];
    if (!children || !children.items[k]) return null;
    children.items.splice(k, 1);
    if (children.items.length > k) return path;
    if (k > 0) return [...path.slice(0, -1), k - 1];
    return path.slice(0, -1);
}

function move(doc, path, by) {
    if (!path.length) return null;
    const parent = widgetAt(doc, path.slice(0, -1)), children = childrenOf(parent), k = path[path.length - 1], to = k + by;
    if (!children || to < 0 || to >= children.items.length) return null;
    [children.items[k], children.items[to]] = [children.items[to], children.items[k]];
    return [...path.slice(0, -1), to];
}

// the widget in a box of its own, in its place
function wrap(doc, path) {
    const widget = widgetAt(doc, path);
    if (!widget) return null;
    const box = plain({ kind: 'box', children: [] });
    box.entries[1][1].items.push(widget);
    if (!path.length) {
        entryOf(doc, 'tree')[1] = box;
    } else {
        const children = childrenOf(widgetAt(doc, path.slice(0, -1)));
        children.items[path[path.length - 1]] = box;
    }
    return path;
}

// the builder's own entry in "editor": {} when there is none
function builderOf(doc) {
    return shown(valueOf(valueOf(doc, 'editor'), BUILDER)) ?? {};
}

function setBuilder(doc, value) {
    let editor = valueOf(doc, 'editor');
    if (!editor || editor.t !== 'object') {
        editor = { t: 'object', entries: [] };
        const entry = entryOf(doc, 'editor');
        if (entry) entry[1] = editor;
        else doc.entries.push([stringNode('editor'), editor]);
    }
    const entry = entryOf(editor, BUILDER);
    if (entry) entry[1] = plain(value);
    else editor.entries.push([stringNode(BUILDER), plain(value)]);
}

// ---- writing ----

const encoder = new TextEncoder();
const bytesOf = (text) => encoder.encode(text).length;

// the entries of an object in the order given: the known keys first, each its first time, then the rest as they stood
function ordered(entries, order) {
    const first = new Map();
    entries.forEach(([key], k) => {
        if (order.includes(key.value) && !first.has(key.value)) first.set(key.value, k);
    });
    const known = order.filter((key) => first.has(key)).map((key) => entries[first.get(key)]);
    return [...known, ...entries.filter((entry, k) => !(order.includes(entry[0].value) && first.get(entry[0].value) === k))];
}

const keyText = (key) => key.raw ?? JSON.stringify(key.value);

// a value on one line, its own text where it has one; the keys of a widget in the widget's order, nothing else moved
function line(node, order = null) {
    switch (node.t) {
    case 'object': {
        const entries = order ? ordered(node.entries, order) : node.entries;
        return entries.length ? `{${entries.map(([k, v]) => `${keyText(k)}: ${line(v)}`).join(', ')}}` : '{}';
    }
    case 'array': return `[${node.items.map((item) => line(item)).join(', ')}]`;
    case 'string': return node.raw ?? JSON.stringify(node.value);
    case 'number': return node.raw;
    default: return node.t;
    }
}

// the text of a document, and the span of bytes of each widget in it: {text, spans: [{path, start, end}]}
function write(doc) {
    const parts = [];
    let bytes = 0;
    const spans = [];
    const put = (text) => {
        parts.push(text);
        bytes += bytesOf(text);
    };
    const widget = (node, path, indent) => {
        const start = bytes;
        const children = childrenOf(node);
        const entries = ordered(node.entries, WIDGET_ORDER);
        const one = line(node, WIDGET_ORDER);
        if ((!children || !children.items.length) && one.length <= ONE_LINE) {
            put(one);
            // the spans of children written on the one line: none, there are none
        } else {
            const inner = indent + '  ';
            put('{\n');
            entries.forEach(([key, value], k) => {
                put(`${inner}${keyText(key)}: `);
                if (value === children && key.value === 'children' && children.items.length) {
                    put('[\n');
                    children.items.forEach((child, c) => {
                        put(inner + '  ');
                        if (child.t === 'object') widget(child, [...path, c], inner + '  ');
                        else put(line(child));
                        put(c + 1 < children.items.length ? ',\n' : '\n');
                    });
                    put(`${inner}]`);
                } else {
                    put(line(value));
                }
                put(k + 1 < entries.length ? ',\n' : '\n');
            });
            put(`${indent}}`);
        }
        spans.push({ path, start, end: bytes });
    };
    const entries = ordered(doc.entries, TOP_ORDER);
    put('{\n');
    entries.forEach(([key, value], k) => {
        put(`  ${keyText(key)}: `);
        if (key.value === 'tree' && value.t === 'object' && value === treeOf(doc)) widget(value, [], '  ');
        else put(line(value));
        put(k + 1 < entries.length ? ',\n' : '\n');
    });
    put('}\n');
    spans.sort((a, b) => a.start - b.start);
    return { text: parts.join(''), spans };
}

// the widget whose object holds a byte of a text written: the deepest span that holds it
function pathAtByte(spans, at) {
    let found = null;
    for (const span of spans) if (span.start <= at && at < span.end && (!found || span.end - span.start <= found.end - found.start)) found = span;
    return found ? found.path : null;
}

const api = { read, readText, write, widgets, widgetAt, indexOf, pathOf, samePath, setKey, removeKey, addChild, remove, move, wrap, builderOf,
              setBuilder, shown, valueOf, entryOf, plain, stringNode, line, keyText, ordered, bytesOf, pathAtByte, byteOfUnit, unitOfByte,
              illFormedAt, TOP_ORDER, WIDGET_ORDER, ONE_LINE, BUILDER };
if (typeof module !== 'undefined') module.exports = api;
else window.DgxUiDescription = api;
