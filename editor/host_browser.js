// The files of the builder in a browser. A host gives the store a project's files by paths relative to its directory:
// list(directory) the entries of a directory, [{name, directory}], or null when there is none; read(path) the bytes of a
// file, or null; write(path, text) true when written in place, false when given otherwise, as a download.
//  - dgxUiFolderHost(): a directory opened by the File System Access API, read and written in place;
//  - dgxUiMemoryHost(): files held in memory, the files a directory input chose where the browser has no such API, a save
//    being a download; and the host the tests give the store.
'use strict';

// a missing file or directory: null, not an error
async function dgxUiInFolder(folder, name, get) {
    try {
        return await folder[get](name);
    } catch (error) {
        if (error.name === 'NotFoundError' || error.name === 'TypeMismatchError') return null;
        throw error;
    }
}

async function dgxUiFolderAt(root, parts, create = false) {
    let at = root;
    for (const part of parts) {
        at = create ? await at.getDirectoryHandle(part, { create: true }) : await dgxUiInFolder(at, part, 'getDirectoryHandle');
        if (!at) return null;
    }
    return at;
}

const dgxUiParts = (path) => path.split('/').filter((part) => part !== '');

function dgxUiFolderHost(folder) {
    return {
        name: folder.name,
        inPlace: true,
        async list(directory) {
            const at = await dgxUiFolderAt(folder, dgxUiParts(directory));
            if (!at) return null;
            const entries = [];
            for await (const [name, handle] of at.entries()) entries.push({ name, directory: handle.kind === 'directory' });
            return entries.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
        },
        async read(path) {
            const parts = dgxUiParts(path), name = parts.pop();
            const at = await dgxUiFolderAt(folder, parts);
            const file = at && (await dgxUiInFolder(at, name, 'getFileHandle'));
            return file ? new Uint8Array(await (await file.getFile()).arrayBuffer()) : null;
        },
        async write(path, text) {
            const parts = dgxUiParts(path), name = parts.pop();
            const at = await dgxUiFolderAt(folder, parts, true);
            const writable = await (await at.getFileHandle(name, { create: true })).createWritable();
            await writable.write(text);
            await writable.close();
            return true;
        },
    };
}

// files: {path: Uint8Array, or a Blob read when asked, as a File of an input}; save(path, text): what a write does beside
// keeping the text, false when it is not written in place
function dgxUiMemoryHost(name, files, save = () => false) {
    const encoder = new TextEncoder();
    return {
        name,
        inPlace: false,
        files,
        async list(directory) {
            const prefix = dgxUiParts(directory).map((part) => part + '/').join('');
            const entries = new Map();
            for (const path of Object.keys(files)) {
                if (!path.startsWith(prefix)) continue;
                const rest = path.slice(prefix.length), slash = rest.indexOf('/');
                entries.set(slash < 0 ? rest : rest.slice(0, slash), slash >= 0);
            }
            if (!entries.size && prefix) return null;
            return [...entries].map(([entry, directory]) => ({ name: entry, directory })).sort((a, b) => (a.name < b.name ? -1 : 1));
        },
        async read(path) {
            const value = files[dgxUiParts(path).join('/')];
            if (value === undefined || value === null) return null;
            return ArrayBuffer.isView(value) ? value : new Uint8Array(await value.arrayBuffer());
        },
        async write(path, text) {
            files[dgxUiParts(path).join('/')] = encoder.encode(text);
            return save(path, text);
        },
    };
}

// a file saved as a download, where the page cannot write it in place
function dgxUiDownload(path, text) {
    const link = document.createElement('a');
    link.href = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    link.download = path.split('/').pop();
    link.click();
    URL.revokeObjectURL(link.href);
    return false;
}

const dgxUiBrowserHost = {
    get canOpenFolder() {
        return typeof window.showDirectoryPicker === 'function';
    },

    async openFolder() {
        return dgxUiFolderHost(await window.showDirectoryPicker({ mode: 'readwrite' }));
    },

    // what an <input webkitdirectory> gives: the paths start at the directory picked
    async fromPicked(list) {
        const picked = [...list];
        if (!picked.length) return null;
        const name = picked[0].webkitRelativePath.split('/')[0];
        const files = {};
        for (const file of picked) files[file.webkitRelativePath.slice(name.length + 1)] = file;
        return dgxUiMemoryHost(name, files, dgxUiDownload);
    },
};

if (typeof module !== 'undefined') module.exports = { dgxUiFolderHost, dgxUiMemoryHost };
