// The appearance as the builder holds it: read by the reader of descriptions (description.js), which keeps every
// entry in its order, a key twice too, and the text of every number and string as written; changed by the operations
// of the forms, each at a path of keys and indices from the top, such as ['looks', 'slider', 'params', 'track']; and
// written in one form. It judges nothing of the format: the loader of the preview does. What a draw's parameters are
// comes from the module (preview_appearance_format()), the library's own description of them.
//
// The order of the colours, the looks and the styles is never changed here: a new one goes at the end of its object, a
// rename keeps its place. The painter's table takes the colours by their place, and a tree made C on the appearance
// takes its looks and styles by theirs.
'use strict';

const DgxUiAppearance = (() => {
    const description = typeof module !== 'undefined' ? require('./description.js') : window.DgxUiDescription;
    const { valueOf, entryOf, plain, stringNode, line, keyText, ordered, bytesOf, ONE_LINE } = description;

    const TOP_ORDER = ['dgx-ui', 'name', 'editor', 'colors', 'styles', 'looks'];
    const SECTIONS = ['colors', 'styles', 'looks'];
    const PAINTERS = ['paper', 'ink', 'fill', 'border']; // the indices 0 to 3 of the painter's table, by the format
    const C_NAME = /^[A-Za-z_][A-Za-z0-9_]*$/;
    const HEX = /^#[0-9a-f]{6}$/;

    // ---- the document ----

    // the node at a path, or undefined
    function nodeAt(doc, path) {
        let node = doc;
        for (const k of path) {
            if (typeof k === 'number') node = node && node.t === 'array' ? node.items[k] : undefined;
            else node = node && node.t === 'object' ? valueOf(node, k) : undefined;
            if (node === undefined) return undefined;
        }
        return node;
    }

    // the names of a section's entries in their order, a name twice once
    function namesOf(doc, section) {
        const object = valueOf(doc, section);
        if (!object || object.t !== 'object') return [];
        return [...new Set(object.entries.map(([key]) => key.value))];
    }

    // a value set at a path, the objects on the way made where they are not; an index past the last appends
    function setAt(doc, path, value) {
        let node = doc;
        for (let k = 0; k < path.length - 1; ++k) {
            const key = path[k];
            let next = typeof key === 'number' ? (node.t === 'array' ? node.items[key] : undefined) : node.t === 'object' ? valueOf(node, key) : undefined;
            if (next === undefined) {
                if (typeof key === 'number' || node.t !== 'object') throw new Error(`no place for ${path.join('.')}`);
                next = { t: 'object', entries: [] };
                node.entries.push([stringNode(key), next]);
            }
            node = next;
        }
        const last = path[path.length - 1];
        if (typeof last === 'number') {
            if (node.t !== 'array' || last < 0 || last > node.items.length) throw new Error(`no place for ${path.join('.')}`);
            node.items[last] = plain(value);
        } else {
            if (node.t !== 'object') throw new Error(`no place for ${path.join('.')}`);
            const entry = entryOf(node, last);
            if (entry) entry[1] = plain(value);
            else node.entries.push([stringNode(last), plain(value)]);
        }
        return true;
    }

    function removeAt(doc, path) {
        const parent = nodeAt(doc, path.slice(0, -1)), last = path[path.length - 1];
        if (typeof last === 'number') {
            if (!parent || parent.t !== 'array' || !parent.items[last]) return false;
            parent.items.splice(last, 1);
            return true;
        }
        if (!parent || parent.t !== 'object') return false;
        const k = parent.entries.findIndex(([key]) => key.value === last);
        if (k < 0) return false;
        parent.entries.splice(k, 1);
        return true;
    }

    // an item put into a list at an index, the list made where there is none
    function insertAt(doc, path, index, value) {
        let list = nodeAt(doc, path);
        if (list === undefined) setAt(doc, path, []), list = nodeAt(doc, path);
        if (!list || list.t !== 'array') throw new Error(`${path.join('.')} is not a list`);
        list.items.splice(Math.max(0, Math.min(index, list.items.length)), 0, plain(value));
        return true;
    }

    // an item of a list moved by one place
    function moveAt(doc, path, index, by) {
        const list = nodeAt(doc, path), to = index + by;
        if (!list || list.t !== 'array' || !list.items[index] || to < 0 || to >= list.items.length) return false;
        [list.items[index], list.items[to]] = [list.items[to], list.items[index]];
        return true;
    }

    // an entry of an object renamed in its place
    function renameKey(doc, path, from, to) {
        const object = nodeAt(doc, path), entry = entryOf(object, from);
        if (!entry || entryOf(object, to)) return false;
        entry[0] = stringNode(to);
        return true;
    }

    // ---- the colours and who names them ----

    // the colour fields of a draw, by the format of the module; null for a draw the format does not describe
    function colorFields(format, draw) {
        const described = format?.draws.find((one) => one.draw === draw);
        if (!described || described.own) return null;
        return described.fields.filter((field) => field.type === 'color' || field.type === 'color_or_under' || field.type === 'bands');
    }

    // the places of the looks that name a colour: [{look, path}], and the looks whose parameters the format does not
    // describe, a draw of the program's own, where a name of it may stand unseen: own
    function colorUsers(doc, name, format) {
        const users = [], own = [];
        const looks = valueOf(doc, 'looks');
        for (const [key, look] of looks && looks.t === 'object' ? looks.entries : []) {
            if (look.t !== 'object') continue;
            const fill = valueOf(look, 'fill');
            if (fill && fill.t === 'string' && fill.value === name) users.push({ look: key.value, path: ['looks', key.value, 'fill'] });
            const draw = valueOf(look, 'draw'), params = valueOf(look, 'params');
            const fields = draw && draw.t === 'string' ? colorFields(format, draw.value) : null;
            if (!fields) {
                if (params) own.push(key.value);
                continue;
            }
            for (const field of fields) {
                const value = params && params.t === 'object' ? valueOf(params, field.name) : undefined;
                if (!value) continue;
                if (field.type === 'bands') {
                    (value.t === 'array' ? value.items : []).forEach((band, k) => {
                        const color = band.t === 'array' ? band.items[1] : undefined;
                        if (color && color.t === 'string' && color.value === name)
                            users.push({ look: key.value, path: ['looks', key.value, 'params', field.name, k, 1] });
                    });
                } else if (value.t === 'string' && value.value === name) {
                    users.push({ look: key.value, path: ['looks', key.value, 'params', field.name] });
                }
            }
        }
        return { users, own };
    }

    // a colour renamed, and every look that names it: its fill, its parameters, its bands. The looks of a draw the format
    // does not describe are not changed, and are given back: own
    function renameColor(doc, from, to, format) {
        const { users, own } = colorUsers(doc, from, format);
        if (!renameKey(doc, ['colors'], from, to)) return null;
        for (const user of users) setAt(doc, user.path, to);
        return { changed: users.length, own };
    }

    // a colour as the file writes it: "#rrggbb" of small letters, from "#RRGGBB", "rrggbb", or a name of DGX's colours;
    // null for what is none of them
    function colorOf(text, names) {
        const said = String(text).trim().toLowerCase();
        if (/^#?[0-9a-f]{6}$/.test(said)) return said.startsWith('#') ? said : `#${said}`;
        const named = names.find(([name]) => name === said.replace(/[\s_-]/g, ''));
        return named ? named[1] : null;
    }

    // the names of DGX's colours of a value, "gray / grey" where two are the same
    function namesOfColor(hex, names) {
        return names.filter(([, value]) => value === String(hex).toLowerCase()).map(([name]) => name).join(' / ');
    }

    // a colour as a panel of a depth shows it: each part cut to the bits the depth keeps and spread back over eight, as a
    // screen of DGX packs it and the preview's panel unpacks it
    function colorAtDepth(hex, depth) {
        if (!HEX.test(hex)) return hex;
        const parts = [1, 3, 5].map((k) => parseInt(hex.slice(k, k + 2), 16));
        const bits = depth === 16 ? [5, 6, 5] : depth === 18 ? [6, 6, 6] : [8, 8, 8];
        const shown = parts.map((part, k) => {
            const kept = part >> (8 - bits[k]);
            return bits[k] === 8 ? kept : (kept << (8 - bits[k])) | (kept >> (2 * bits[k] - 8));
        });
        return '#' + shown.map((part) => part.toString(16).padStart(2, '0')).join('');
    }

    // ---- writing ----

    // the text of an appearance in the builder's form, and the span of bytes of each entry of its colours, styles and
    // looks: {text, spans: [{path: [section, name], start, end}]}. The top's keys in one order, the unknown after them as
    // they stood; each entry of a section on a line of its own, broken where it would pass ONE_LINE columns
    function write(doc) {
        const parts = [], spans = [];
        let bytes = 0;
        const put = (text) => {
            parts.push(text);
            bytes += bytesOf(text);
        };
        // a value on one line when it fits after its lead, else its entries or items one a line
        const fit = (node, indent, lead) => {
            const one = line(node);
            const broken = (node.t === 'object' && node.entries.length) || (node.t === 'array' && node.items.length);
            if (!broken || lead + one.length <= ONE_LINE) return put(one);
            const inner = indent + '  ';
            if (node.t === 'object') {
                put('{\n');
                node.entries.forEach(([key, value], k) => {
                    const head = `${inner}${keyText(key)}: `;
                    put(head);
                    fit(value, inner, head.length);
                    put(k + 1 < node.entries.length ? ',\n' : '\n');
                });
                put(`${indent}}`);
            } else {
                put('[\n');
                node.items.forEach((item, k) => {
                    put(inner);
                    fit(item, inner, inner.length);
                    put(k + 1 < node.items.length ? ',\n' : '\n');
                });
                put(`${indent}]`);
            }
        };
        const entries = ordered(doc.entries, TOP_ORDER);
        put('{\n');
        entries.forEach(([key, value], k) => {
            const head = `  ${keyText(key)}: `;
            put(head);
            if (SECTIONS.includes(key.value) && value.t === 'object' && value.entries.length) {
                put('{\n');
                value.entries.forEach(([name, entry], e) => {
                    put('    ');
                    const start = bytes, lead = `    ${keyText(name)}: `;
                    put(lead.slice(4));
                    fit(entry, '    ', lead.length);
                    spans.push({ path: [key.value, name.value], start, end: bytes });
                    put(e + 1 < value.entries.length ? ',\n' : '\n');
                });
                put('  }');
            } else {
                fit(value, '  ', head.length);
            }
            put(k + 1 < entries.length ? ',\n' : '\n');
        });
        put('}\n');
        return { text: parts.join(''), spans };
    }

    // the entry whose text holds a byte of a text written: [section, name], or null
    function entryAtByte(spans, at) {
        return spans.find((span) => span.start <= at && at < span.end)?.path ?? null;
    }

    return { nodeAt, namesOf, setAt, removeAt, insertAt, moveAt, renameKey, colorUsers, renameColor, colorOf, namesOfColor, colorAtDepth,
             write, entryAtByte, TOP_ORDER, SECTIONS, PAINTERS, C_NAME, HEX };
})();

if (typeof module !== 'undefined') module.exports = DgxUiAppearance;
else window.DgxUiAppearance = DgxUiAppearance;
