// The page of the preview alone (index.html): a description, its names file and appearance typed, drawn as the board
// draws them. A file of its own, so that the page runs under a Content-Security-Policy that takes no script written in
// a page.
'use strict';
(async () => {
    const element = id => document.getElementById(id);
    const panel = element('panel'), context = panel.getContext('2d');
    /* the loader's codes, dgx_ui_load.h */
    const loadErrors = ['OK', 'SYNTAX', 'VERSION', 'KEY', 'KIND', 'FIELD', 'RANGE', 'NAME_BAD', 'UNKNOWN', 'NO_LOOK', 'NO_STYLE', 'TOO_DEEP',
                        'MEMORY', 'NODES', 'ID', 'MISSING'].map(word => 'DGX_UI_LOAD_' + word);
    let pages = {};
    const preview = await DgxUiPreview.create({pageText: name => pages[name] === undefined ? null : JSON.stringify(pages[name])});
    const built = preview.version();
    element('version').textContent = `dgx-ui ${built.dgx_ui}, DGX ${built.dgx}; ${built.toolchain}; ${built.sources}`;

    const show = (id, verdict) => {
        const said = element(id);
        said.className = 'said' + (verdict.code ? ' refused' : '');
        said.textContent = verdict.code === 0 ? 'taken'
            : verdict.code === -1 ? verdict.said
            : `${loadErrors[verdict.code] || 'refused ' + verdict.code} at byte ${verdict.offset}${verdict.name ? ' "' + verdict.name + '"' : ''}`;
    };
    const textOf = id => element(id).value.trim() ? element(id).value : null;
    let wakeAt = 0;
    const apply = () => {
        try {
            pages = element('pages').value.trim() ? JSON.parse(element('pages').value) : {};
            element('pages-said').textContent = '';
        } catch (error) {
            element('pages-said').textContent = error.message;
        }
        const width = +element('width').value, height = +element('height').value, zoom = +element('zoom').value;
        if (preview.screen(width, height, +element('depth').value, +element('unit').value) !== 0) return;
        panel.width = width, panel.height = height, panel.style.width = width * zoom + 'px', panel.style.height = height * zoom + 'px';
        show('names-said', preview.names(textOf('names')));
        show('appearance-said', preview.appearance(textOf('appearance')));
        show('page-said', preview.page(element('page').value));
        element('notes').textContent = preview.notes().join('\n');
        wakeAt = 0;
    };
    const events = [];
    const frame = now => {
        if (now >= wakeAt) {
            if (preview.paint(now)) context.putImageData(preview.image(), 0, 0);
            wakeAt = now + Math.min(preview.wake(now), 1000);
            for (const line of preview.events()) events.push(line);
            element('events').textContent = events.slice(-12).join('\n');
        }
        requestAnimationFrame(frame);
    };
    const at = event => {
        const zoom = +element('zoom').value, box = panel.getBoundingClientRect();
        return [Math.floor((event.clientX - box.left) / zoom), Math.floor((event.clientY - box.top) / zoom)];
    };
    let down = false;
    panel.addEventListener('pointerdown', event => {
        down = true, panel.setPointerCapture(event.pointerId), preview.pointer(...at(event), true, performance.now()), wakeAt = 0;
    });
    panel.addEventListener('pointermove', event => {
        if (down) preview.pointer(...at(event), true, performance.now()), wakeAt = 0;
        const i = preview.widgetAt(...at(event)), widget = i >= 0 ? preview.widget(i) : null, shown = preview.shown();
        element('widget').textContent = widget ? `${shown ? 'page ' + shown + ', ' : ''}${i}: kind ${widget.kind}, ${widget.w}x${widget.h} ` +
                                                 `at ${widget.x},${widget.y}` + (widget.text !== null ? ` "${widget.text}"` : '') : '';
    });
    panel.addEventListener('pointerup', event => { down = false, preview.pointer(...at(event), false, performance.now()), wakeAt = 0; });
    for (const button of document.querySelectorAll('[data-key]'))
        button.addEventListener('click', () => { preview.key(+button.dataset.key), wakeAt = 0; });
    element('set-value').addEventListener('click', () => { preview.value(element('value-name').value, +element('value').value), wakeAt = 0; });
    element('apply').addEventListener('click', apply);
    /* the fonts of a project: the index found among the files chosen, each file read from the index's directory */
    element('fonts').addEventListener('change', async () => {
        const chosen = [...element('fonts').files], index = chosen.find(file => file.name === 'dgx_fonts-files.txt');
        if (!index) {
            element('fonts-said').textContent = 'no dgx_fonts-files.txt among the files chosen';
            return;
        }
        const directory = index.webkitRelativePath.slice(0, -index.name.length);
        const byPath    = new Map(chosen.map(file => [file.webkitRelativePath.slice(directory.length), file]));
        const read      = async path => {
            const file = byPath.get(path);
            if (!file) throw new Error('not among the files chosen');
            return new Uint8Array(await file.arrayBuffer());
        };
        const given   = await preview.fonts(await index.text(), read);
        const refused = given.filter(one => one.verdict.code !== 0);
        element('fonts-said').className   = 'said' + (refused.length ? ' refused' : '');
        element('fonts-said').textContent = `${given.length - refused.length} of ${given.length} taken` +
                                            refused.map(one => '\n' + one.verdict.said).join('');
        apply();
    });
    for (const id of ['page', 'appearance', 'names', 'pages']) element(id).addEventListener('input', apply);
    for (const id of ['width', 'height', 'depth', 'unit', 'zoom']) element(id).addEventListener('change', apply);
    apply();
    requestAnimationFrame(frame);
})().catch(error => {
    document.body.insertAdjacentHTML('beforeend', '<pre class="refused"></pre>');
    document.body.lastElementChild.textContent = String(error);
});
