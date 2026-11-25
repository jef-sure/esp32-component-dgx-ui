// The names file as the builder holds it: its lines, each kept as it is, its words split as dgx_ui_c's reader splits
// them (tools/dgx_ui_names.c): white space between them, '#' a comment to the end of the line. A line edited is written
// again with each word at the column it stood at, where the word before it leaves room, else one space after it, and
// its comment at its column, else two spaces after the words; a word it had not, and a new line's, takes the column of
// the line before it in its table, or of the nearest line that has a word there. Lines added to a file of none are laid
// in columns of their own. Every other line stays as it was, byte for byte. Lines are never
// moved: a new one goes after the last of its table, or at the end of the file. It judges nothing of the format: the
// reader of the module does, and says the line it stops at.
'use strict';

const DgxUiNames = (() => {
    const WHITE = /[ \t\r]/;

    // the lines of a text: [{number, start, text, end, words: [{text, col}], comment: {text, col} | null}], number from
    // 1, start the line's first unit in the text, end its newline ('' for the last with none)
    function lines(text) {
        const out = [];
        let at = 0, number = 0;
        while (at < text.length || (at === text.length && number === 0 && text.length)) {
            const newline = text.indexOf('\n', at), stop = newline < 0 ? text.length : newline;
            const line = text.slice(at, stop), words = [];
            const hash = line.indexOf('#'), body = hash < 0 ? line : line.slice(0, hash);
            for (let k = 0; k < body.length;) {
                if (WHITE.test(body[k])) {
                    ++k;
                    continue;
                }
                const from = k;
                while (k < body.length && !WHITE.test(body[k])) ++k;
                words.push({ text: body.slice(from, k), col: from });
            }
            out.push({ number: ++number, start: at, text: line, end: newline < 0 ? '' : '\n', words,
                       comment: hash < 0 ? null : { text: line.slice(hash).replace(/\r$/, ''), col: hash } });
            if (newline < 0) break;
            at = newline + 1;
        }
        return out;
    }

    // the lines that name something, by their table: [{number, table, words: [text], comment, known}]; known, a table
    // the format has; a line of words and no known table is the reader's to refuse, and is edited as text
    function rows(text, format) {
        const tables = [...(format?.tables ?? []), ...(format?.appearance ?? [])];
        return lines(text).filter((line) => line.words.length).map((line) => ({
            number: line.number, table: line.words[0].text, words: line.words.map((word) => word.text), comment: line.comment?.text ?? null,
            known: tables.includes(line.words[0].text),
        }));
    }

    // a line's text of words at the columns given, after a lead, and a comment at its column
    function laid(lead, words, columns, comment, commentColumn) {
        let out = lead;
        words.forEach((word, k) => {
            if (k > 0) {
                const column = columns[k];
                out += column !== undefined && out.length < column ? ' '.repeat(column - out.length) : ' ';
            }
            out += word;
        });
        if (comment) out += (commentColumn !== undefined && out.length < commentColumn ? ' '.repeat(commentColumn - out.length) : '  ') + comment;
        return out;
    }

    // the columns of words at a place: those of a line of its own where it has them, the rest of the nearest line of the
    // same table that has a word there, else of the nearest line that has; near, the line number the distance is from
    function columnsAt(all, own, table, count, near) {
        const columns = own ? own.words.map((word) => word.col) : [];
        const named = all.filter((line) => line.words.length && line !== own);
        const byDistance = (a, b) => Math.abs(a.number - near) - Math.abs(b.number - near) || a.number - b.number;
        const ofTable = named.filter((line) => line.words[0].text === table).sort(byDistance), others = named.slice().sort(byDistance);
        for (let k = columns.length; k < count; ++k) {
            const like = ofTable.find((line) => line.words.length > k) ?? others.find((line) => line.words.length > k);
            columns.push(like ? like.words[k].col : undefined);
        }
        return columns;
    }

    // the line of a number written again with these words, its columns and its comment kept
    function setLine(text, number, words) {
        const all = lines(text), line = all.find((one) => one.number === number);
        if (!line) return null;
        const lead = line.words.length ? line.text.slice(0, line.words[0].col) : line.text.match(/^[ \t]*/)[0];
        const carriage = line.text.endsWith('\r') ? '\r' : '';
        const columns = columnsAt(all, line, words[0], words.length, number);
        const written = laid(lead, words, columns, line.comment?.text, line.comment?.col) + carriage;
        return text.slice(0, line.start) + written + text.slice(line.start + line.text.length);
    }

    function removeLine(text, number) {
        const line = lines(text).find((one) => one.number === number);
        if (!line) return null;
        return text.slice(0, line.start) + text.slice(line.start + line.text.length + line.end.length);
    }

    // a new line of words after the last line of its table, or at the end; its columns those of the line before it in
    // its table, or of the nearest line that names something. {text, number}
    function addLine(text, words) {
        const all = lines(text), named = all.filter((line) => line.words.length);
        const ofTable = named.filter((line) => line.words[0].text === words[0]);
        const after = ofTable.length ? ofTable[ofTable.length - 1] : null;
        const like = after ?? named[named.length - 1] ?? null;
        const lead = like ? like.text.slice(0, like.words[0].col) : '';
        const carriage = like && like.text.endsWith('\r') ? '\r' : '';
        const near = after ? after.number : all.length;
        const written = laid(lead, words, like ? [like.words[0].col, ...columnsAt(all, null, words[0], words.length, near).slice(1)] : []) + carriage;
        if (after) {
            const at = after.start + after.text.length;
            const newline = after.end || '\n';
            return { text: text.slice(0, at) + newline + written + text.slice(at), number: after.number + 1 };
        }
        const ends = text === '' || text.endsWith('\n');
        return { text: text + (ends ? '' : '\n') + written + '\n', number: all.length + 1 };
    }

    // lines added: to a file that names nothing, laid in columns of their own, each two spaces past the longest word
    // before it; else each by addLine(). The text
    function addLines(text, many) {
        if (lines(text).some((line) => line.words.length)) {
            for (const words of many) text = addLine(text, words).text;
            return text;
        }
        const widths = [];
        for (const words of many) words.forEach((word, k) => (widths[k] = Math.max(widths[k] ?? 0, word.length)));
        const columns = widths.map((_, k) => widths.slice(0, k).reduce((sum, width) => sum + width + 2, 0));
        const ends = text === '' || text.endsWith('\n');
        return text + (ends ? '' : '\n') + many.map((words) => laid('', words, columns)).join('\n') + '\n';
    }

    // the line a verdict of the reader stops at: "<path>:<line>: ..."; null for none
    function lineOfVerdict(said) {
        const found = /^[^:\n]*:(\d+): /.exec(said ?? '');
        return found ? +found[1] : null;
    }

    return { lines, rows, setLine, removeLine, addLine, addLines, lineOfVerdict };
})();

if (typeof module !== 'undefined') module.exports = DgxUiNames;
else window.DgxUiNames = DgxUiNames;
