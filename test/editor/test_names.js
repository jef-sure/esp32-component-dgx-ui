/*
 * node test_names.js <dgx_ui_preview.js> <directory of descriptions>: the names file as the builder edits it (editor/names.js), held to the reader
 * of the preview's module, the one dgx_ui_c reads names files with.
 *  - every names file of the tree (the hook project's, the documents'): its lines read and joined back byte for byte; a
 *    line written again with its own words the same; each line edited, added and removed leaves a file the reader takes;
 *  - the columns: an edited line of an aligned file keeps its words and its comment at their columns, a new line takes
 *    the columns of the line before it in its table; a word too long for its column moves the next one a space on;
 *  - the names a page uses and its file has not (preview_names_missing()): every page of the tree and of the directory
 *    (test_generate's) that loads by the rule loads on a names file of nothing but them, and then none is missing; their
 *    C the rule's;
 *  - what a names file may say (preview_names_format()), against the tables of the documents.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const ROOT = path.join(__dirname, '../..');
const DgxUiPreview = require(path.resolve(process.argv[2]));
const names = require(path.join(ROOT, 'editor/names.js'));
const description = require(path.join(ROOT, 'editor/description.js'));
let failures = 0, checks = 0;
const check = (held, what) => {
    ++checks;
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const TABLE = /^(look|value|picture|style|on_event|take|ctx|id|draw|font|hand)[ \t]/;

(async () => {
    const preview = await DgxUiPreview.create({ log: () => {} });
    preview.screen(320, 240, 16, 1);
    const format = preview.namesFormat();
    const taken = (text) => preview.names(text).code === 0;

    /* what a names file may say */
    check(same(format.tables, ['look', 'value', 'picture', 'style', 'on_event', 'take', 'ctx', 'id']) && same(format.keys, format.tables) &&
          same(format.appearance, ['draw', 'font', 'hand']), 'the tables: ' + JSON.stringify(format.tables));
    check(same(format.bare.on_event, ['back', 'page', 'command']) && same(format.bare.hand, ['handwriting', 'handwriting_canvas']) &&
          format.bare.look.includes('slider') && format.bare.look.includes('soft_picture') && format.bare.draw.length === 22, 'the names that go with no expression');
    const api = fs.readFileSync(path.join(ROOT, 'docs/en/api.md'), 'utf8');
    check(format.tables.every((table) => api.includes(`\`${table}\``)), 'every table in the documents');

    /* the names files of the tree */
    const files = [];
    for (const file of ['test/hook/project/main/vocab.txt', 'test/hook/project/main/my-vocab.txt', 'test/hook/project/components/second/words/names.txt'])
        files.push([file, fs.readFileSync(path.join(ROOT, file), 'utf8')]);
    for (const file of ['docs/en/cookbook.md', 'docs/ru/cookbook.md', 'docs/en/api.md', 'docs/ru/api.md']) {
        for (const block of fs.readFileSync(path.join(ROOT, file), 'utf8').matchAll(/```text\n([^]*?)```/g)) {
            const named = block[1].split('\n').filter((line) => line.trim() && !line.trim().startsWith('#'));
            if (named.length && named.every((line) => TABLE.test(line))) files.push([file, block[1]]);
        }
    }
    check(files.length >= 8, 'the names files of the tree: ' + files.length);
    let edited = 0;
    for (const [file, text] of files) {
        const lines = names.lines(text);
        check(lines.map((line) => line.text + line.end).join('') === text, `${file}: its lines joined back byte for byte`);
        check(taken(text), `${file}: taken as it is: ${preview.names(text).said}`);
        const rows = names.rows(text, format);
        for (const row of rows) {
            const line = lines.find((one) => one.number === row.number);
            if (!/[\t]|[ \t]+$/.test(line.text)) check(names.setLine(text, row.number, row.words) === text, `${file}:${row.number}: written again the same`);
            // its expression made longer by a few letters, or a new one where it had none of the library's
            const words = [...row.words];
            if (words.length > 2 && row.table !== 'font') words[2] = words[2] + '_x';
            const changed = names.setLine(text, row.number, words);
            check(taken(changed), `${file}:${row.number}: edited, taken: ${preview.names(changed).said}`);
            const others = names.lines(changed).filter((one) => one.number !== row.number).map((one) => one.text);
            check(same(others, lines.filter((one) => one.number !== row.number).map((one) => one.text)), `${file}:${row.number}: the other lines kept`);
            const removed = names.removeLine(text, row.number);
            check(taken(removed) && names.lines(removed).length === lines.length - 1, `${file}:${row.number}: removed, taken`);
            ++edited;
        }
        for (const table of format.tables.filter((one) => one !== 'look' && one !== 'id')) {
            const added = names.addLine(text, [table, `added_${table}`, table === 'picture' || table === 'take' || table === 'on_event' ? 'added' : '&added']);
            check(taken(added.text) && names.lines(added.text)[added.number - 1].words[1].text === `added_${table}`,
                  `${file}: a ${table} added, taken, at line ${added.number}`);
        }
    }
    check(edited >= 40, 'lines edited: ' + edited);

    /* the columns of an aligned file */
    const aligned = files.find(([, text]) => text.includes('value     volume    &room_volume'))?.[1] ?? '';
    check(aligned, 'the aligned names file of the cookbook');
    const volume = names.rows(aligned, format).find((row) => row.words[1] === 'volume');
    const longer = names.setLine(aligned, volume.number, ['value', 'volume', '&room_volume_of_the_hall']);
    const columns = (text, number) => names.lines(text)[number - 1].words.map((word) => word.col);
    check(same(columns(longer, volume.number), columns(aligned, volume.number)), 'an edited line keeps its columns: ' + columns(longer, volume.number));
    const wide = names.setLine(aligned, volume.number, ['value', 'the_volume_of_it', '&room_volume']);
    check(names.lines(wide)[volume.number - 1].text === 'value     the_volume_of_it &room_volume', 'a word too long: the next one space on');
    const added = names.addLine(aligned, ['value', 'bass', '&room_bass']);
    check(added.number === volume.number + 2 && same(columns(added.text, added.number), columns(aligned, volume.number + 1)),
          'a new line after the last of its table, at its columns: ' + names.lines(added.text)[added.number - 1].text);
    const commented = 'look      slider            # the library\'s\nvalue     volume    &v     # the room\n';
    const recommented = names.setLine(commented, 2, ['value', 'volume', '&room_volume']);
    check(recommented.split('\n')[1] === 'value     volume    &room_volume  # the room' &&
          names.setLine(commented, 2, ['value', 'v', '&w']).split('\n')[1] === 'value     v         &w     # the room', 'a comment at its column, else two spaces on');
    const fresh = names.addLine('', ['value', 'volume', '&volume']);
    check(fresh.text === 'value volume &volume\n' && fresh.number === 1 && taken(fresh.text), 'a line in an empty file');
    const crlf = 'value     a    &a\r\nvalue     b    &b\r\n';
    check(names.setLine(crlf, 1, ['value', 'a', '&aa']) === 'value     a    &aa\r\nvalue     b    &b\r\n' &&
          names.addLine(crlf, ['value', 'c', '&c']).text === crlf + 'value     c    &c\r\n', 'a file of \\r\\n keeps them');
    check(names.lineOfVerdict(preview.names('value a &a\nvalue a &b\n').said) === 2, 'the line of a refusal');

    /* the names a page uses and its file has not: a file of them alone loads every page of the tree that the rule loads */
    const pages = [];
    for (const file of ['docs/en/cookbook.md', 'docs/en/api.md', 'docs/en/getting-started.md']) {
        for (const block of fs.readFileSync(path.join(ROOT, file), 'utf8').matchAll(/```json\n([^]*?)```/g)) {
            const said = description.read(block[1]);
            if (said.ok && description.valueOf(said.doc, 'tree')) pages.push([file, block[1]]);
        }
    }
    for (const file of ['test/hook/project/main/framed.json', 'test/hook/project/components/second/c.json', 'test/hook/project/main/b.json'])
        pages.push([file, fs.readFileSync(path.join(ROOT, file), 'utf8')]);
    const generated = process.argv[3];
    for (const file of generated ? fs.readdirSync(generated).filter((name) => name.endsWith('.json')).sort() : [])
        pages.push([file, fs.readFileSync(path.join(generated, file), 'utf8')]);
    let made = 0;
    preview.appearance(null);
    for (const [file, page] of pages) {
        preview.names(null);
        if (preview.page(page).code !== 0) continue;
        preview.names('');
        const missing = preview.namesMissing('');
        const lines = missing.filter((entry) => entry.bare || entry.c).map((entry) => {
            const words = [entry.table, entry.name];
            if (!entry.bare) words.push(entry.c, ...(entry.count ? [String(entry.count)] : []), ...(entry.number ? ['number'] : []), ...(entry.page ? ['page'] : []));
            return words.join(' ');
        });
        if (lines.length !== missing.length) continue; // a name the rule makes no C of: the program's to give
        const text = lines.join('\n') + '\n';
        const filed = preview.names(text), loaded = preview.page(page);
        check(filed.code === 0 && loaded.code === 0, `${file}: its page on the names missing: ${filed.said} ${loaded.code} "${loaded.name}"`);
        check(!preview.namesMissing(text).length, `${file}: none missing after`);
        for (const entry of missing.filter((one) => !one.bare && !one.offset)) {
            const rule = { value: `&${entry.name}`, style: `&${entry.name}`, look: `&${entry.name}`, picture: entry.name, take: entry.name, on_event: entry.name,
                           ctx: `&${entry.name}`, id: entry.name }[entry.table];
            check(entry.c === rule, `${file}: ${entry.table} ${entry.name} by the rule ${rule}, not ${entry.c}`);
        }
        ++made;
    }
    check(made >= (generated ? 40 : 3), `pages made on the names they miss: ${made} of ${pages.length}`);

    if (failures) {
        console.error(`test_names.js: ${failures} of ${checks} checks failed`);
        process.exit(1);
    }
    console.log(`test_names.js: ${checks} checks: ${files.length} names files, ${edited} lines edited, the columns kept, ${made} pages loaded on the names they miss`);
})().catch((error) => {
    console.error('test_names.js:', error);
    process.exit(1);
});
